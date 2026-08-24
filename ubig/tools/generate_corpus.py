#!/usr/bin/env python3
import argparse, hashlib, json, math
from pathlib import Path
import numpy as np

SR=48000

def write_case(root,name,x,description):
    x=np.asarray(x,dtype='<f4')
    assert x.ndim==2 and x.shape[1]==2
    p=root/(name+'.f32le')
    x.tofile(p)
    return {
        'name':name,'file':p.name,'frames':int(x.shape[0]),'sample_rate':SR,
        'channels':2,'format':'float32le-interleaved','description':description,
        'sha256':hashlib.sha256(p.read_bytes()).hexdigest()
    }

def main():
    ap=argparse.ArgumentParser();ap.add_argument('out',nargs='?',default='build/corpus');a=ap.parse_args()
    root=Path(a.out);root.mkdir(parents=True,exist_ok=True)
    cases=[]

    n=4096;x=np.zeros((n,2),np.float32);x[0]=[0.5,-0.5];x[257]=[0.25,0.125]
    cases.append(write_case(root,'impulse',x,'startup latency and ringing/state probe'))

    n=SR*3;t=np.arange(n,dtype=np.float64)/SR
    x=np.column_stack((0.12*np.sin(2*np.pi*997*t)+0.035*np.sin(2*np.pi*113*t),
                       0.09*np.sin(2*np.pi*1553*t+0.31)+0.025*np.sin(2*np.pi*181*t)))
    cases.append(write_case(root,'dual_sine',x,'independent stereo deterministic tone pair'))

    n=SR*8;t=np.arange(n,dtype=np.float64)/SR
    levels=np.array([0.003,0.01,0.03,0.08,0.16,0.30,0.50,0.75],dtype=np.float64)
    amp=levels[np.minimum((np.arange(n)//SR),len(levels)-1)]
    x=np.column_stack((amp*np.sin(2*np.pi*75*t),0.91*amp*np.sin(2*np.pi*75*t+0.27)))
    cases.append(write_case(root,'level_stair_75hz',x,'low-frequency amplitude staircase for nonlinear/limiter boundaries'))

    n=SR*5;t=np.arange(n,dtype=np.float64)/SR
    freqs=[47,141,328,656,1031,2250,4688,9000,13875]
    l=sum((0.018/(1+i*.12))*np.sin(2*np.pi*f*t+0.11*i) for i,f in enumerate(freqs))
    r=sum((0.016/(1+i*.10))*np.sin(2*np.pi*(f*1.013)*t+0.23*i) for i,f in enumerate(freqs))
    x=np.column_stack((l,r))
    cases.append(write_case(root,'multiband_multitone',x,'band-grid and intermodulation probe'))

    rng=np.random.default_rng(0x55424947)
    n=SR*5
    noise=rng.standard_normal((n,2),dtype=np.float32)*np.float32(0.035)
    # deterministic sparse transients
    for i in range(0,n,65537):
        noise[i,0]+=np.float32(0.55);noise[i,1]-=np.float32(0.42)
    cases.append(write_case(root,'det_noise_transients',noise,'deterministic wideband state/excitation probe'))

    n=SR*12;t=np.arange(n,dtype=np.float64)/SR
    env=np.ones(n);env[(np.arange(n)//(SR*2))%3==1]=0.08;env[(np.arange(n)//(SR*2))%3==2]=0.35
    x=np.column_stack((env*(0.08*np.sin(2*np.pi*181*t)+0.025*np.sin(2*np.pi*997*t)),
                       env*(0.075*np.sin(2*np.pi*239*t+0.4)+0.02*np.sin(2*np.pi*1553*t))))
    cases.append(write_case(root,'adaptive_history',x,'12-second leveler/regulator attack-release history probe'))

    manifest={'schema':'ubig-generated-corpus-v1','sample_rate':SR,'channels':2,'cases':cases,
              'chunk_schedules':{'host480':[480],'tiny':[1],'mixed':[1,64,127,353,480,1024,7,511,31]}}
    mp=root/'manifest.json';mp.write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'wrote {len(cases)} cases to {root}')
    for c in cases: print(c['name'],c['frames'],c['sha256'][:16])

if __name__=='__main__':main()

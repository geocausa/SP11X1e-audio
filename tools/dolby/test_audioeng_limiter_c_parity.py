#!/usr/bin/env python3
from __future__ import annotations
import hashlib
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path
import numpy as np

ROOT=Path(__file__).resolve().parents[2]
ORACLE=ROOT/'dolby-port'/'sp11_audioeng_limiter_oracle.py'
CLI=ROOT/'dolby-port'/'sp11_audioeng_limiter_cli'

def load_oracle():
    spec=importlib.util.spec_from_file_location('sp11_audioeng_limiter_oracle',ORACLE)
    mod=importlib.util.module_from_spec(spec);assert spec.loader;spec.loader.exec_module(mod);return mod

def main()->int:
    oracle=load_oracle(); n=48000*3; t=np.arange(n,dtype=np.float64)/48000.0
    x=np.zeros((n,2),np.float32)
    x[:,0]=(.7*np.sin(2*np.pi*75*t)+.35*np.sin(2*np.pi*997*t)).astype(np.float32)
    x[:,1]=(.62*np.sin(2*np.pi*113*t)-.31*np.sin(2*np.pi*701*t)).astype(np.float32)
    x[12000:16000]*=np.float32(1.65);x[60000:68000]*=np.float32(2.1)
    rng=np.random.default_rng(0x511);x+=(rng.standard_normal(x.shape)*.004).astype(np.float32)
    ref,stats=oracle.process(x,48000.0,True);refb=ref.astype('<f4').tobytes()
    patterns=['1','64','480','1024','127,353','31,257,509,17,1024,3,480,65']
    with tempfile.TemporaryDirectory(prefix='sp11-audioeng-limiter-') as td:
        td=Path(td);inp=td/'in.f32';inp.write_bytes(x.astype('<f4').tobytes())
        for pat in patterns:
            out=td/'out.f32';subprocess.run([str(CLI),str(inp),str(out),pat],check=True)
            got=out.read_bytes()
            if got!=refb:
                a=np.frombuffer(got,dtype='<f4');b=np.frombuffer(refb,dtype='<f4');idx=np.flatnonzero(a!=b)
                raise SystemExit(f'FAIL pattern={pat} differing={idx.size} first={int(idx[0]) if idx.size else None}')
            print(f'pattern={pat:32s} PASS sha256={hashlib.sha256(got).hexdigest()}')
    print('limited_frames',stats['limited_frames'],'min_gain',stats['min_gain'])
    print('AUDIOENG_LIMITER_C_PARITY PASS')
    return 0
if __name__=='__main__': raise SystemExit(main())

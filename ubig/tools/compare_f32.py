#!/usr/bin/env python3
import argparse, json
import numpy as np

def main():
    ap=argparse.ArgumentParser();ap.add_argument('reference');ap.add_argument('candidate');ap.add_argument('--skip',type=int,default=0,help='frames to skip');a=ap.parse_args()
    r=np.fromfile(a.reference,dtype='<f4');c=np.fromfile(a.candidate,dtype='<f4')
    if r.size!=c.size or r.size%2: raise SystemExit(f'size mismatch ref={r.size} cand={c.size}')
    r=r.reshape(-1,2)[a.skip:];c=c.reshape(-1,2)[a.skip:]
    d=c.astype(np.float64)-r.astype(np.float64)
    exact=np.array_equal(r.view(np.uint32),c.view(np.uint32))
    neq=np.flatnonzero(np.any(r.view(np.uint32)!=c.view(np.uint32),axis=1))
    rms=lambda x: float(np.sqrt(np.mean(np.square(x,dtype=np.float64)))) if x.size else 0.0
    rr=rms(r.astype(np.float64));er=rms(d)
    corr=[]
    for ch in range(2):
        rv=r[:,ch].astype(np.float64);cv=c[:,ch].astype(np.float64)
        if rv.std()==0 or cv.std()==0:corr.append(None)
        else:corr.append(float(np.corrcoef(rv,cv)[0,1]))
    snr=None if er==0 else float(20*np.log10(rr/er)) if rr else float('-inf')
    out={'frames':int(r.shape[0]),'exact_float32':bool(exact),'first_diff_frame':None if not neq.size else int(neq[0]+a.skip),
         'reference_rms':rr,'error_rms':er,'max_abs_error':float(np.max(np.abs(d))) if d.size else 0.0,
         'snr_db':snr,'correlation':corr}
    print(json.dumps(out,indent=2))
if __name__=='__main__':main()

import argparse, math, wave
from pathlib import Path
import numpy as np
from sp11_known_input_alignment import robust_loopback_lag

parser=argparse.ArgumentParser()
parser.add_argument('--input', type=Path, default=Path('sp11-known-input-stimulus-48k.wav'))
parser.add_argument('--windows', type=Path, default=Path('windows-loopback-known-input.wav'))
parser.add_argument('--native', type=Path, default=Path('native_known_output.f32'))
args=parser.parse_args()
IN=args.input
WIN=args.windows
NAT=args.native

def rw(p):
    with wave.open(str(p),'rb') as w:
        sr=w.getframerate(); ch=w.getnchannels(); x=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(np.float64).reshape(-1,ch)/32768.0
    return sr,x

def align(inp,out,sr):
    lag, _corr = robust_loopback_lag(inp, out, sr)
    return lag

def rms(x): return math.sqrt(float(np.mean(x*x)))
def db(x): return 20*math.log10(max(float(x),1e-15))

sr,x=rw(IN); _,w=rw(WIN); n=np.fromfile(NAT,dtype='<f4').astype(np.float64).reshape(-1,2)
xm=x.mean(1); wm=w.mean(1); nm=n.mean(1)
wlag=align(xm,wm,sr)
# native lag previously robustly measured 689; refine around onset using correlation on first 4 seconds
# brute-force normalized correlation around 0..1500 with 4x decimation
ref=xm[:4*sr:4]
best=(-1,None)
for lag in range(0,1501):
    z=nm[lag:lag+4*sr:4]
    if len(z)!=len(ref): break
    den=np.linalg.norm(ref)*np.linalg.norm(z)
    sc=float(np.dot(ref,z)/den) if den else -1
    if sc>best[0]: best=(sc,lag)
nlag=best[1]
print(f'windows_lag={wlag} ({wlag/sr:.6f}s) native_lag={nlag} ({nlag/sr:.6f}s) corr={best[0]:.6f}')

# sweep starts input t=3.5, 8s long, 35 -> 18000 logarithmically
T=8.0; f0=35.; f1=18000.; k=math.log(f1/f0)/T
# use 0.25 sec windows, avoid sweep fade edges 0.15 sec
win=0.25
rows=[]
for t0 in np.arange(3.65,11.35,win):
    t1=t0+win
    c=(t0+t1)/2
    tau=c-3.5
    fc=f0*math.exp(k*tau)
    a=xm[int(t0*sr):int(t1*sr)]
    wb=wm[int(t0*sr)+wlag:int(t1*sr)+wlag]
    nb=nm[int(t0*sr)+nlag:int(t1*sr)+nlag]
    if min(len(a),len(wb),len(nb))<100: continue
    ga=db(rms(wb)/rms(a)); gn=db(rms(nb)/rms(a)); d=ga-gn
    rows.append((fc,ga,gn,d))
print('freq_hz windows_gain native_gain win_minus_native')
for r in rows: print(f'{r[0]:9.1f} {r[1]:+9.3f} {r[2]:+9.3f} {r[3]:+9.3f}')
# octave-ish summarized at selected centers using nearest samples and median within +/-12% freq
print('\nSUMMARY median +/-12%')
for target in [40,55,75,90,140,200,300,500,750,1000,1500,2500,4000,6000,9000,13000,17000]:
    rr=[r for r in rows if abs(math.log(r[0]/target)) <= math.log(1.12)]
    if not rr: rr=[min(rows,key=lambda r:abs(math.log(r[0]/target)))]
    arr=np.array(rr)
    print(f'{target:6.0f} Hz  Win {np.median(arr[:,1]):+7.2f}  Nat {np.median(arr[:,2]):+7.2f}  W-N {np.median(arr[:,3]):+7.2f}')

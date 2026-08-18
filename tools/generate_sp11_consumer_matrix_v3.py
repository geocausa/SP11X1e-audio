#!/usr/bin/env python3
"""Generate the SP11 sequential single-tone consumer transfer matrix v3."""
from __future__ import annotations
import argparse, hashlib, json, wave
from pathlib import Path
import numpy as np

FS=48_000
FREQS=(100,250,315,500,630,1000,1250,1600,2000,2500,4000,6300)
LEVELS=(0.0125,0.05,0.2)
LEAD_S=2.0
TONE_S=0.75
GAP_S=0.25
TAIL_S=2.0
RAMP_S=0.05
SYNC_FREQ=1000
SYNC_PEAK=0.1
SYNC_TONE_S=0.75
SYNC_GAP_S=1.25

def build():
    schedule=[]
    chunks=[np.zeros((round(LEAD_S*FS),2),dtype=np.float64)]
    cursor=LEAD_S
    nt=round(TONE_S*FS); nr=round(RAMP_S*FS)
    n=np.arange(nt,dtype=np.float64)
    ramp=np.ones(nt,dtype=np.float64)
    # Linear ramp makes exact PCM reproduction independent of libm cosine details.
    ramp[:nr]=np.arange(nr,dtype=np.float64)/nr
    ramp[-nr:]=np.arange(nr-1,-1,-1,dtype=np.float64)/nr
    # Strong stereo marker used only for acoustic time alignment.
    sync_n=round(SYNC_TONE_S*FS); sn=np.arange(sync_n,dtype=np.float64)
    sr=round(RAMP_S*FS); sync_ramp=np.ones(sync_n,dtype=np.float64)
    sync_ramp[:sr]=np.arange(sr,dtype=np.float64)/sr
    sync_ramp[-sr:]=np.arange(sr-1,-1,-1,dtype=np.float64)/sr
    sync=SYNC_PEAK*np.sin(2.0*np.pi*SYNC_FREQ*sn/FS)*sync_ramp
    chunks.append(np.column_stack((sync,sync)))
    sync_marker={'start_s':cursor,'duration_s':SYNC_TONE_S,'frequency_hz':SYNC_FREQ,'source_peak':SYNC_PEAK,'channel':'stereo'}
    cursor+=SYNC_TONE_S
    chunks.append(np.zeros((round(SYNC_GAP_S*FS),2),dtype=np.float64)); cursor+=SYNC_GAP_S
    for f in FREQS:
        base=np.sin(2.0*np.pi*f*n/FS)*ramp
        for level in LEVELS:
            for ch_name,ch in (('left',0),('right',1)):
                x=np.zeros((nt,2),dtype=np.float64)
                x[:,ch]=level*base
                chunks.append(x)
                schedule.append({'index':len(schedule)+1,'start_s':cursor,'duration_s':TONE_S,'frequency_hz':f,'source_peak':level,'channel':ch_name})
                cursor+=TONE_S
                chunks.append(np.zeros((round(GAP_S*FS),2),dtype=np.float64))
                cursor+=GAP_S
    chunks.append(np.zeros((round(TAIL_S*FS),2),dtype=np.float64)); cursor+=TAIL_S
    return np.concatenate(chunks),schedule,cursor,sync_marker

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('output',type=Path); ap.add_argument('--schedule',type=Path); a=ap.parse_args()
    x,schedule,duration,sync_marker=build(); pcm=np.clip(np.rint(x*32767.0),-32768,32767).astype('<i2')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with wave.open(str(a.output),'wb') as w:
        w.setnchannels(2);w.setsampwidth(2);w.setframerate(FS);w.writeframes(pcm.tobytes())
    schedule_path=a.schedule or a.output.with_suffix('.schedule.json')
    meta={'schema':'sp11-consumer-matrix-v3','fs':FS,'frequencies_hz':list(FREQS),'levels':list(LEVELS),'lead_s':LEAD_S,'tone_s':TONE_S,'gap_s':GAP_S,'tail_s':TAIL_S,'duration_s':duration,'sync_marker':sync_marker,'analysis_window':{'offset_s':0.125,'duration_s':0.5},'conditions':schedule}
    schedule_path.write_text(json.dumps(meta,indent=2)+'\n')
    print(hashlib.sha256(a.output.read_bytes()).hexdigest(),a.output)
    print(hashlib.sha256(schedule_path.read_bytes()).hexdigest(),schedule_path)
    print(f'duration={duration:.3f}s conditions={len(schedule)} peak={float(np.max(np.abs(x))):.6f}')
if __name__=='__main__':main()

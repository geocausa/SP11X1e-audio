#!/usr/bin/env python3
import math,struct

def f32(x): return struct.unpack('<f',struct.pack('<f',x))[0]
def hx(x):
    x=f32(x)
    # C99 hex literal representing the exact float32 value
    return x.hex()+'f'
with open('src/core/stage_a_fft320_tables.h','w') as f:
    f.write('/* Generated mathematical twiddles; no vendor binary data. */\n#ifndef UBIG_STAGE_A_FFT320_TABLES_H\n#define UBIG_STAGE_A_FFT320_TABLES_H\n')
    f.write('static const float ubig_w64_re[32]={\n')
    f.write(','.join(hx(math.cos(-2*math.pi*k/64)) for k in range(32)))
    f.write('};\nstatic const float ubig_w64_im[32]={\n')
    f.write(','.join(hx(math.sin(-2*math.pi*k/64)) for k in range(32)))
    f.write('};\nstatic const float ubig_w320_re[1280]={\n')
    f.write(','.join(hx(math.cos(-2*math.pi*r*k/320)) for r in range(1,5) for k in range(320)))
    f.write('};\nstatic const float ubig_w320_im[1280]={\n')
    f.write(','.join(hx(math.sin(-2*math.pi*r*k/320)) for r in range(1,5) for k in range(320)))
    f.write('};\n#endif\n')

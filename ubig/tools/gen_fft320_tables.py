#!/usr/bin/env python3
"""Generate the SP11 320-point mixed-radix FFT roots from mathematics.

The staged root tables are the ordinary forward roots of unity, quantized to
six decimal places before float32 conversion. The radix-5 butterfly constants
are emitted from their full-precision trigonometric definitions.
"""
import math
import struct


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def q6(x):
    return f32(round(x, 6))


def full_root(x):
    if abs(x) < 1.0e-15:
        x = math.copysign(0.0, x)
    return f32(x)


def hx(x):
    return f32(x).hex() + 'f'


def stage_table(stride):
    out = []
    denom = 4 * stride
    for power in range(1, 4):
        for base in range(0, stride, 4):
            out.extend(q6(math.cos(-2 * math.pi * power * j / denom))
                       for j in range(base, base + 4))
            out.extend(q6(math.sin(-2 * math.pi * power * j / denom))
                       for j in range(base, base + 4))
    return out


def final_table():
    out = []
    for power in range(1, 5):
        for base in range(0, 64, 4):
            out.extend(q6(math.cos(-2 * math.pi * power * j / 320))
                       for j in range(base, base + 4))
            out.extend(q6(math.sin(-2 * math.pi * power * j / 320))
                       for j in range(base, base + 4))
    return out


def sp11_mid_table():
    out = []
    for power in range(1, 5):
        for base in range(0, 8, 4):
            out.extend(full_root(math.cos(-2 * math.pi * power * r / 40))
                       for r in range(base, base + 4))
            out.extend(full_root(math.sin(-2 * math.pi * power * r / 40))
                       for r in range(base, base + 4))
    return out


def sp11_final_table():
    out = []
    for power in range(1, 8):
        for base in range(0, 40, 4):
            out.extend(full_root(math.cos(-2 * math.pi * power * q / 320))
                       for q in range(base, base + 4))
            out.extend(full_root(math.sin(-2 * math.pi * power * q / 320))
                       for q in range(base, base + 4))
    return out


def emit_array(f, name, values):
    f.write(f'static const float {name}[{len(values)}]={{\n')
    for i in range(0, len(values), 8):
        f.write('    ' + ','.join(hx(v) for v in values[i:i+8]) + ',\n')
    f.write('};\n')


with open('src/core/stage_a_fft320_tables.h', 'w') as f:
    f.write('/* Generated from roots of unity; no vendor binary data. */\n')
    f.write('#ifndef UBIG_STAGE_A_FFT320_TABLES_H\n#define UBIG_STAGE_A_FFT320_TABLES_H\n')
    emit_array(f, 'ubig_fft320_stage4_twiddle', stage_table(4))
    emit_array(f, 'ubig_fft320_stage16_twiddle', stage_table(16))
    emit_array(f, 'ubig_fft320_final_twiddle', final_table())
    emit_array(f, 'ubig_fft320_sp11_mid_twiddle', sp11_mid_table())
    emit_array(f, 'ubig_fft320_sp11_final_twiddle', sp11_final_table())
    f.write('static const float ubig_fft320_radix5_c0=' + hx(math.sin(math.pi/5)) + ';\n')
    f.write('static const float ubig_fft320_radix5_c1=' + hx(math.cos(math.pi/10)) + ';\n')
    f.write('static const float ubig_fft320_radix5_c2=' + hx(math.cos(2*math.pi/5)) + ';\n')
    f.write('static const float ubig_fft320_radix5_c3=' + hx(-math.cos(math.pi/5)) + ';\n')
    f.write('#endif\n')

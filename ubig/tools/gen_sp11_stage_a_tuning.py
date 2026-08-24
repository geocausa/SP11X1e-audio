#!/usr/bin/env python3
import json, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=json.loads((ROOT/'specs/sp11-stage-a-dynamic-tuning-v1.json').read_text())
def f32bits(s): return int(s,16)
def fhex(bits): return struct.unpack('<f',struct.pack('<I',bits))[0].hex()+'f'
def unit(v): return struct.unpack('<f',struct.pack('<f',v/2080.0))[0].hex()+'f'
def arr(name,vals,conv=str,ctype='float'):
    out=[f'static const {ctype} {name}[{len(vals)}]={{']
    for i in range(0,len(vals),8): out.append('    '+','.join(conv(x) for x in vals[i:i+8])+',')
    out.append('};');return '\n'.join(out)
c=spec['compressor']; lines=['/* Generated from specs/sp11-stage-a-dynamic-tuning-v1.json. */','#ifndef UBIG_STAGE_A_SP11_TUNING_TABLES_H','#define UBIG_STAGE_A_SP11_TUNING_TABLES_H']
for key,name in [('scalar_cfg_f32_bits','ubig_sp11_comp_scalar_cfg'),('dual_cfg_f32_bits','ubig_sp11_comp_dual_cfg'),('flag_cfg_f32_bits','ubig_sp11_comp_flag_cfg'),('band_update_cfg_f32_bits','ubig_sp11_comp_band_update_cfg'),('transition_cfg_f32_bits','ubig_sp11_comp_transition_cfg'),('direction_cfg_f32_bits','ubig_sp11_comp_direction_cfg'),('severity_coeff_f32_bits','ubig_sp11_comp_severity_coeff')]:
    lines.append(arr(name,c[key],lambda x:fhex(f32bits(x))))
lines.append(arr('ubig_sp11_comp_distribution',c['distribution'],str,'int32_t'))
flat=sum(spec['base_rows_units_2080'],[]);lines.append(arr('ubig_sp11_stage_a_base_rows_flat',flat,unit))
lines.append(arr('ubig_sp11_stage_a_side_a',spec['side_a_units_2080'],unit));lines.append(arr('ubig_sp11_stage_a_side_b',spec['side_b_units_2080'],unit))
lines.append(arr('ubig_sp11_stage_a_mask',spec['mask'],str,'int32_t'))
lines.append(arr('ubig_sp11_stage_a_runtime',spec['runtime_f32_bits'],lambda x:fhex(f32bits(x))))
lines.append(arr('ubig_sp11_stage_a_channel_mix',spec['channel_mix_units_2080'],unit))
fam=spec['profile_family_state']
for key,prefix in [('common','ubig_sp11_family_common'),('movie_music','ubig_sp11_family_movie_music')]:
    flat_groups=sum(fam[key]['groups'],[])
    lines.append(arr(prefix+'_groups',flat_groups,str,'int32_t'))
    lines.append(f'#define {prefix.upper()}_GROUP_COUNT {len(fam[key]["groups"])}u')
    lines.append(f'#define {prefix.upper()}_CHANNEL_DEVIATION {int(fam[key]["channel_deviation"])}')
    lines.append(f'#define {prefix.upper()}_SLOW_GAIN_ENABLE {int(fam[key]["slow_gain_enable"])}u')
    lines.append(f'#define {prefix.upper()}_SLOW_GAIN_MIX {int(fam[key]["slow_gain_mix"])}')
lines.append(f'#define UBIG_SP11_STAGE_A_INPUT_SCALE {fhex(f32bits(spec["input_scale_f32_bits"]))}')
lines.append(f'#define UBIG_SP11_STAGE_A_LIMITER_CEILING {fhex(f32bits(spec["limiter_ceiling_f32_bits"]))}')
lines.append(f'#define UBIG_SP11_STAGE_A_DRIVE_STATE {fhex(f32bits(spec["drive_state_f32_bits"]))}')
lines.append(f'#define UBIG_SP11_STAGE_A_CONTROLLER_DRIVE {unit(spec["controller_drive_units_2080"])}')
lines.append(f'#define UBIG_SP11_COMP_HOLD_SAMPLES {int(c["hold_samples"])}u')
lines.append(f'#define UBIG_SP11_COMP_RESERVED {int(c["reserved_u32"])}u')
lines.append('#endif')
(ROOT/'src/core/stage_a_sp11_tuning_tables.h').write_text('\n'.join(lines)+'\n')

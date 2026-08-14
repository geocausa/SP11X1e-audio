#!/usr/bin/env python3
"""Build the evidence-backed SP11 Windows endpoint-volume transaction oracle.

This tool does not touch ALSA or the DSP.  It combines the recovered Windows
endpoint taper, final render VOL_CTRL Q28 actuator, qcadcm GainStep selector,
and ACDB prior/new-CKV runtime delta into one deterministic transaction plan.

Recovered Windows lifecycle:
  * DAX/VLLDP postgain is derived from endpoint master dB when the Dolby APO
    generation is configured; it is not rewritten by an ordinary live slider.
  * live SetVolume sends final render VOL_CTRL iid 0x4a63 / pid 0x08001038;
  * qcadcm then selects GainStep 1..30 and sends the selected 0x489e four-frame
    non-persistent ACDB delta as one OOB APM_CMD_SET_CFG transaction.
  * a stereo master gesture is delivered as left-channel state first, then the
    matching right-channel state, as proved by fresh SP11 KDNET.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.acdb_gainstep_delta_inventory import extract_gainstep_delta
from tools.acdb_setcfg_inventory import parse_chunks
from tools.sp11_final_volume_q28 import multichannel_payload

def _load(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


volume = _load(ROOT / "deploy/dolby/sp11_dolby_volume_sync.py", "sp11_volume_sync")
msiir = _load(ROOT / "deploy/dolby/sp11_msiir_volume_sync.py", "sp11_msiir_sync")

FINAL_VOL_IID = 0x4A63
FINAL_VOL_PARAM = 0x08001038
GAINSTEP_IID = 0x489E
GAINSTEP_PARAMS = (0x08001020, 0x08001021, 0x08001022, 0x08001026)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def qcadcm_q28_from_db(db: float) -> tuple[int, int, float]:
    """Match qcadcm GetQ28Gain for the endpoint's non-positive dB range.

    The hash-locked ARM64 routine treats input as signed Q16.16 dB, takes the
    negative-gain magnitude, shifts it right by 14 and indexes a 0.25-dB Q28
    table.  All 301 table rows from 0 through -75 dB equal the rounded analytic
    Q28 value at that quarter-dB point.
    """
    db = max(-75.0, min(0.0, float(db)))
    q16 = int(round(db * 65536.0))
    quarter_index = ((-q16) >> 14) if q16 < 0 else 0
    effective_db = -(quarter_index / 4.0)
    q28 = int(round((10.0 ** (effective_db / 20.0)) * msiir.Q28_ONE))
    return max(0, min(msiir.Q28_ONE, q28)), quarter_index, effective_db


def plan_for_ui_scalar(chunks, scalar: float, *, muted: bool = False) -> dict:
    scalar = max(0.0, min(1.0, float(scalar)))
    endpoint_db = volume.windows_endpoint_db_from_ui_scalar(scalar)
    postgain = volume.postgain_from_ui_scalar(scalar, muted)
    q28, quarter_db_index, effective_q28_db = qcadcm_q28_from_db(endpoint_db)
    step = msiir.select_ckv_step_q28(q28)
    delta, delta_meta = extract_gainstep_delta(chunks, step)
    final_payload = multichannel_payload(q28)
    coeff = msiir.COEFF_PAYLOADS[step - 1]

    coeff_meta = next(
        item for item in delta_meta["parameters"]
        if int(item["param_id"], 16) == msiir.PARAM_COEFFS
    )
    if coeff_meta["payload_sha256"] != sha256(coeff):
        raise ValueError(f"GainStep {step} coefficient table disagrees with reviewed ACDB")

    return {
        "ui_scalar": scalar,
        "endpoint_db": endpoint_db,
        "postgain_1_16_db": postgain,
        "final_vol_ctrl": {
            "iid": f"0x{FINAL_VOL_IID:08x}",
            "param_id": f"0x{FINAL_VOL_PARAM:08x}",
            "q28": q28,
            "q28_hex": f"0x{q28:08x}",
            "qcadcm_quarter_db_index": quarter_db_index,
            "qcadcm_effective_db": effective_q28_db,
            "payload_size": len(final_payload),
            "payload_sha256": sha256(final_payload),
        },
        "gainstep": {
            "step": step,
            "iid": f"0x{GAINSTEP_IID:08x}",
            "param_ids": [f"0x{x:08x}" for x in GAINSTEP_PARAMS],
            "delta_size": len(delta),
            "delta_sha256": sha256(delta),
            "coefficient_size": len(coeff),
            "coefficient_sha256": sha256(coeff),
        },
        "generation_configuration": ["dolby_postgain"],
        "ordered_operations": [
            "final_vol_ctrl_ramped_gain",
            "gainstep_oob_nonpersistent_delta",
        ],
        "stereo_master_sequence": [
            "left_new_right_old_then_mixed_gainstep",
            "left_new_right_new_then_final_gainstep",
        ],
    }


def inventory(acdb: Path) -> dict:
    raw = acdb.read_bytes()
    chunks = parse_chunks(raw)
    rows = [plan_for_ui_scalar(chunks, percent / 100.0) for percent in range(101)]
    return {
        "format": "SP11 Windows endpoint-volume transaction oracle",
        "format_version": 1,
        "source_acdb": str(acdb),
        "source_acdb_sha256": sha256(raw),
        "windows_generation_configuration": [
            "DAX/VLLDP postgain from endpoint master dB once per Dolby APO generation",
        ],
        "windows_live_order": [
            "final VOL_CTRL 0x4a63/0x08001038 Q28 gain (topology ramp 10 ms / 1000 us / curve 3)",
            "GainStep selection by qcadcm nearest-Q28 rule",
            "one OOB non-persistent 0x489e four-frame ACDB delta",
        ],
        "windows_stereo_master_sequence": [
            "left=new/right=old with mixed-state GainStep",
            "left=new/right=new with final GainStep",
        ],
        "rows_integer_percent": rows,
    }


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("acdb", type=Path)
    p.add_argument("--ui-scalar", type=float)
    p.add_argument("--output", type=Path)
    p.add_argument("--show-payloads", action="store_true")
    args = p.parse_args()

    raw = args.acdb.read_bytes()
    chunks = parse_chunks(raw)
    if args.ui_scalar is None:
        result = inventory(args.acdb)
    else:
        result = plan_for_ui_scalar(chunks, args.ui_scalar)
        if args.show_payloads:
            q28 = result["final_vol_ctrl"]["q28"]
            step = result["gainstep"]["step"]
            delta, _ = extract_gainstep_delta(chunks, step)
            result["final_vol_ctrl"]["payload_hex"] = multichannel_payload(q28).hex()
            result["gainstep"]["delta_hex"] = delta.hex()

    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

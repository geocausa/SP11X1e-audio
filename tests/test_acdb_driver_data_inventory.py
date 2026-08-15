from pathlib import Path
import importlib.util

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "acdb_driver_data_inventory.py"
ACDB = Path(r"C:\Users\SurfacePro7\Documents\KDNET\Gemini\DUMP\acdb_cal_0D.acdb")

spec = importlib.util.spec_from_file_location("acdb_driver_data_inventory", TOOL)
mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(mod)


def _record(doc, module_id, schema_keys):
    for r in doc["records"]:
        if r["module_id"] == module_id and r["key_ids"] == schema_keys:
            return r
    raise AssertionError((module_id, schema_keys))


def _row(record, **keys):
    for row in record["rows"]:
        actual = {k["name"]: k["value"] for k in row["keys"]}
        if actual == keys:
            return row
    raise AssertionError(keys)


def _param(row, param_id):
    for p in row["params"]:
        if p["param_id"] == param_id:
            return p
    raise AssertionError(param_id)


def test_rev0d_driver_data_inventory_is_structurally_stable():
    if not ACDB.exists():
        return
    doc = mod.decode(ACDB)
    assert doc["sha256"] == "a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde"
    assert doc["driver_module_count"] == 11
    assert doc["gclu_record_count"] == 15
    assert doc["driver_module_ids"] == [
        "0x08000020", "0x08000023", "0x08000027", "0x0800002a",
        "0x08000030", "0x08000040", "0x08000042", "0x08000050",
        "0x08000060", "0x08000080", "0x08000090",
    ]


def test_rev0d_sp11_speaker_wsa_profile_and_interface_row():
    if not ACDB.exists():
        return
    doc = mod.decode(ACDB)

    wsa = _record(doc, "0x08000090", ["0x01000006"])
    wsa_row = _row(wsa, render_endpoint=1)
    p91 = _param(wsa_row, "0x08000091")
    assert p91["size"] == 24
    assert p91["payload_hex"] == (
        "010000000300000000000000040000000000000001000000"
    )

    hwif = _record(doc, "0x08000020", ["0x01000006", "0x01000010"])
    spk = _row(hwif, render_endpoint=1, channel_count=2)
    p21 = _param(spk, "0x08000021")
    p22 = _param(spk, "0x08000022")
    assert p21["payload_hex"] == (
        "0000000002000000000008000500010300000000010008000600010300000000"
    )
    assert p22["payload_hex"] == (
        "040000001500031005000000130010100000000006001010240000000e000c1001000000"
    )


def test_rev0d_speaker_gain_module_is_count_30_table():
    if not ACDB.exists():
        return
    doc = mod.decode(ACDB)
    gain = _record(doc, "0x0800002a", ["0x01000006"])
    row = _row(gain, render_endpoint=1)
    p2b = _param(row, "0x0800002b")
    assert p2b["size"] == 124
    raw = bytes.fromhex(p2b["payload_hex"])
    assert int.from_bytes(raw[:4], "little") == 30
    assert int.from_bytes(raw[-4:], "little") == 0x10000000


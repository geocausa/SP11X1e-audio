import hashlib
import importlib.util
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "deploy" / "dolby" / "sp11_msiir_volume_sync.py"
spec = importlib.util.spec_from_file_location("sp11_msiir_volume_sync", SCRIPT)
mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(mod)


def test_reviewed_internal_speaker_gain_table_shape():
    assert len(mod.Q28_GAIN_STEPS) == 30
    assert mod.Q28_GAIN_STEPS[0] == 0
    assert mod.Q28_GAIN_STEPS[1] == 0x016D0E6F  # -21 dB
    assert mod.Q28_GAIN_STEPS[-1] == 0x10000000  # 0 dB
    assert all(a <= b for a, b in zip(mod.Q28_GAIN_STEPS, mod.Q28_GAIN_STEPS[1:]))


def test_qcadcm_step_mapping_key_points():
    assert mod.select_ckv_step_q28(0) == 1
    assert mod.select_ckv_step_q28(0x016D0E6F) == 2
    assert mod.select_ckv_step_q28(0x10000000) == 30
    # Current Linux PipeWire 0.25 raw gain is 0.015625 / about -36.124 dB:
    # nearest Windows Q28 anchor is the zero/floor row.
    assert mod.select_ckv_step_postgain(-578) == 1
    # Fresh Windows 25% endpoint scalar measured about -20.747 dB.
    assert mod.select_ckv_step_postgain(round(-20.74741 * 16)) == 2
    assert mod.select_ckv_step_postgain(0) == 30


def test_exact_reviewed_payload_hashes_and_variable_lengths():
    assert len(mod.COEFF_PAYLOADS) == 30
    assert hashlib.sha256(mod.COEFF_PAYLOADS[1]).hexdigest() == "e1720e6dc7566b79eff3b593cb64a003ed97eba126e226ba57e892f974c6afe4"
    assert hashlib.sha256(mod.COEFF_PAYLOADS[29]).hexdigest() == "fd08adf0b49d4eb14fcba678b539f09267f40657db6c19002ef3bd96c8028f80"
    assert {i + 1 for i, p in enumerate(mod.COEFF_PAYLOADS) if len(p) == 152} == {3, 9, 24}
    assert all(len(p) in {96, 152} for p in mod.COEFF_PAYLOADS)


def test_tlv_blob_is_allowlisted_target_and_exact_payload():
    blob = mod.build_tlv_blob(2)
    iid, param, size = struct.unpack_from("<III", blob, 0)
    assert iid == 0x489E
    assert param == 0x08001022
    assert size == len(mod.COEFF_PAYLOADS[1])
    assert blob[12:] == mod.COEFF_PAYLOADS[1]


def test_postgain_q28_conversion_is_linear_gain_not_db_distance():
    q = mod.postgain_to_q28(-336)  # -21.0 dB
    expected = round((10 ** (-21.0 / 20.0)) * (1 << 28))
    assert abs(q - expected) <= 1
    assert mod.select_ckv_step_q28(q) == 2


def test_read_postgain_control_page(tmp_path):
    p = tmp_path / "control"
    b = bytearray(12)
    struct.pack_into("<i", b, 4, -578)
    p.write_bytes(b)
    assert mod.read_postgain(p) == -578


def test_read_postgain_ubig_v2_control_page(tmp_path):
    p = tmp_path / "ubig-control-v2"
    b = bytearray(mod.UBIG_CONTROL_BYTES)
    struct.pack_into("<III", b, 0, mod.UBIG_CONTROL_MAGIC, mod.UBIG_CONTROL_ABI, mod.UBIG_CONTROL_BYTES)
    struct.pack_into("<i", b, mod.UBIG_DESIRED_POSTGAIN_OFF, -332)
    p.write_bytes(b)
    assert mod.detect_control_format(p) == mod.CONTROL_FORMAT_UBIG_V2
    assert mod.read_postgain(p) == -332
    assert mod.read_postgain(p, mod.CONTROL_FORMAT_UBIG_V2) == -332

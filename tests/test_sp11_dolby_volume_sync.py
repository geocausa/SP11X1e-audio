import importlib.util
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "deploy" / "dolby" / "sp11_dolby_volume_sync.py"
spec = importlib.util.spec_from_file_location("sp11_dolby_volume_sync", SCRIPT)
mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(mod)


def test_postgain_conversion_matches_recovered_windows_relation():
    assert mod.postgain_from_linear_gain(1.0) == 0
    assert mod.postgain_from_linear_gain(0.0) == -1200
    assert mod.postgain_from_linear_gain(1.0, muted=True) == -1200
    gain = 10 ** ((-423 / 16.0) / 20.0)
    assert mod.postgain_from_linear_gain(gain) == -423


def test_current_sp11_pipewire_example_uses_raw_linear_gain():
    value = mod.postgain_from_linear_gain(0.003908)
    assert -775 <= value <= -765
    # Demonstrate why the displayed cubic UI value must not be treated as
    # linear amplitude: cube root is about the observed wpctl 0.16.
    assert math.isclose(0.003908 ** (1.0 / 3.0), 0.1575, rel_tol=0.01)


def test_extract_node_channel_volume_and_mute():
    snap = [{"info": {"props": {"node.name": mod.DEFAULT_NODE}, "params": {"Props": [
        {"volume": 1.0, "mute": False, "channelVolumes": [0.003908, 0.003908], "softMute": False},
        {"params": ["dolby:Bypass", False]},
    ]}}}]
    gain, muted = mod.extract_node_volume(snap)
    assert gain == 0.003908
    assert muted is False


def test_balance_does_not_reduce_master_proxy():
    snap = [{"info": {"props": {"node.name": mod.DEFAULT_NODE}, "params": {"Props": [
        {"mute": False, "channelVolumes": [0.25, 0.0625]},
    ]}}}]
    gain, _ = mod.extract_node_volume(snap)
    assert gain == 0.25


def test_control_write_preserves_existing_profile_slots(tmp_path):
    path = tmp_path / "control"
    path.write_bytes(bytes((3, 2)))
    mod.write_postgain_request(path, -423)
    data = path.read_bytes()
    assert len(data) == mod.CONTROL_BYTES
    assert data[:2] == bytes((3, 2))
    assert struct.unpack_from("<i", data, mod.POSTGAIN_REQUEST_OFF)[0] == -423
    assert struct.unpack_from("<i", data, mod.POSTGAIN_ACK_OFF)[0] == mod.POSTGAIN_NONE


def test_control_update_preserves_plugin_ack(tmp_path):
    path = tmp_path / "control"
    page = bytearray(mod.CONTROL_BYTES)
    page[0:2] = bytes((5, 5))
    struct.pack_into("<i", page, mod.POSTGAIN_REQUEST_OFF, -423)
    struct.pack_into("<i", page, mod.POSTGAIN_ACK_OFF, -423)
    path.write_bytes(page)
    mod.write_postgain_request(path, -385)
    data = path.read_bytes()
    assert data[:2] == bytes((5, 5))
    assert struct.unpack_from("<i", data, mod.POSTGAIN_REQUEST_OFF)[0] == -385
    assert struct.unpack_from("<i", data, mod.POSTGAIN_ACK_OFF)[0] == -423

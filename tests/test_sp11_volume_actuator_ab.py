import math
import struct

from tools import sp11_volume_actuator_ab as ab


def test_q28_reference_values_track_windows_endpoint_db():
    assert ab.endpoint_q28_from_db(0.0) == 0x10000000
    for db in (-46.5, -30.25, -20.7474098205566):
        q = ab.endpoint_q28_from_db(db)
        got = 20 * math.log10(q / ab.Q28_ONE)
        assert abs(got - db) < 1e-5


def test_dsp_apply_safe_order_and_no_double_attenuation(tmp_path, monkeypatch):
    calls = []
    monkeypatch.setattr(ab.base, "write_postgain_request", lambda p, v: calls.append(("postgain", v)))
    monkeypatch.setattr(ab, "write_final_q28", lambda q: calls.append(("dsp", q)))
    monkeypatch.setattr(ab.base, "set_hardware_volume", lambda node, scalar, wpctl="wpctl": calls.append(("host", scalar)))
    sig = ab.dsp_apply_state((0.25 ** 3, False), 69, tmp_path / "ctl", False, None)
    assert sig is not None
    assert calls[0] == ("postgain", -332)
    assert calls[1][0] == "dsp"
    assert calls[2] == ("host", 1.0)
    assert calls[1][1] == ab.endpoint_q28_from_db(-20.7474098205566)


def test_dsp_apply_repeated_state_suppressed(tmp_path, monkeypatch):
    calls = []
    monkeypatch.setattr(ab.base, "write_postgain_request", lambda *a: calls.append("pg"))
    monkeypatch.setattr(ab, "write_final_q28", lambda *a: calls.append("dsp"))
    monkeypatch.setattr(ab.base, "set_hardware_volume", lambda *a: calls.append("host"))
    sig = ab.dsp_apply_state((0.25 ** 3, False), 69, tmp_path / "ctl", False, None)
    sig2 = ab.dsp_apply_state((0.25 ** 3, False), 69, tmp_path / "ctl", False, sig)
    assert sig2 == sig
    assert calls == ["pg", "dsp", "host"]


def test_restore_host_actuator_safe_order(monkeypatch, tmp_path):
    from types import SimpleNamespace
    calls = []
    snap = [
        {"id": 38, "info": {"props": {"node.name": ab.base.DEFAULT_NODE}, "params": {"Props": [
            {"channelVolumes": [0.25 ** 3, 0.25 ** 3], "mute": False}
        ]}}},
        {"id": 69, "info": {"props": {"node.name": ab.base.DEFAULT_HARDWARE_NODE}, "params": {"Props": [
            {"channelVolumes": [1.0, 1.0], "mute": False}
        ]}}},
    ]
    monkeypatch.setattr(ab.base, "snapshot", lambda _p: snap)
    monkeypatch.setattr(ab.base, "write_postgain_request", lambda p, v: calls.append(("postgain", v)))
    monkeypatch.setattr(ab.base, "set_hardware_volume", lambda node, scalar, wpctl="wpctl": calls.append(("host", scalar)))
    monkeypatch.setattr(ab, "write_final_q28", lambda q: calls.append(("dsp", q)))
    args = SimpleNamespace(
        pw_dump="pw-dump", node=ab.base.DEFAULT_NODE,
        hardware_node=ab.base.DEFAULT_HARDWARE_NODE,
        control=tmp_path / "ctl", dry_run=False, wpctl="wpctl",
    )
    assert ab.restore_host_actuator(args) == 0
    assert calls[0] == ("postgain", -332)
    assert calls[1][0] == "host"
    assert math.isclose(calls[1][1], 0.451034576472, abs_tol=1e-10)
    assert calls[2] == ("dsp", ab.Q28_ONE)


def test_write_final_q28_tlv_bytes(monkeypatch, tmp_path):
    helper = tmp_path / "tlv_write"; helper.write_text("x")
    seen = []
    class CP:
        returncode = 0; stdout = "ok"; stderr = ""
    monkeypatch.setattr(ab, "find_control_numid", lambda card, amixer: 321)
    monkeypatch.setattr(ab.subprocess, "run", lambda argv, **kw: seen.append(argv) or CP())
    ab.write_final_q28(0x007dda19, helper=helper)
    assert seen == [[str(helper), "hw:0", "321", struct.pack("<I", 0x007dda19).hex()]]

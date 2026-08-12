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


def test_pinned_windows_taper_shape_and_key_points():
    assert len(mod.WINDOWS_ENDPOINT_DB) == 201
    assert mod.WINDOWS_TAPER_SPACING == 0.005
    assert mod.windows_endpoint_db_from_ui_scalar(0.0) == -75.0
    assert math.isclose(mod.windows_endpoint_db_from_ui_scalar(0.10), -34.0460205078125, abs_tol=1e-9)
    assert math.isclose(mod.windows_endpoint_db_from_ui_scalar(0.25), -20.7474098205566, abs_tol=1e-9)
    assert math.isclose(mod.windows_endpoint_db_from_ui_scalar(0.50), -10.4270467758179, abs_tol=1e-9)
    assert math.isclose(mod.windows_endpoint_db_from_ui_scalar(1.0), 0.0, abs_tol=1e-12)
    assert all(a <= b for a, b in zip(mod.WINDOWS_ENDPOINT_DB, mod.WINDOWS_ENDPOINT_DB[1:]))


def test_windows_taper_interpolates_between_measured_rows():
    a = mod.WINDOWS_ENDPOINT_DB[50]  # 0.250
    b = mod.WINDOWS_ENDPOINT_DB[51]  # 0.255
    assert math.isclose(mod.windows_endpoint_db_from_ui_scalar(0.2525), (a + b) / 2.0, abs_tol=1e-9)


def test_pipewire_cubic_gain_recovers_visible_scalar():
    assert math.isclose(mod.pipewire_ui_scalar_from_linear_gain(0.015625), 0.25, abs_tol=1e-12)
    assert math.isclose(mod.pipewire_ui_scalar_from_linear_gain(0.125), 0.5, abs_tol=1e-12)
    assert mod.pipewire_ui_scalar_from_linear_gain(0.0) == 0.0


def test_25_percent_full_windows_state():
    ui, db, postgain, hw = mod.derive_windows_state(0.25 ** 3, False)
    assert math.isclose(ui, 0.25, abs_tol=1e-12)
    assert math.isclose(db, -20.7474098205566, abs_tol=1e-9)
    assert postgain == -332
    assert math.isclose(hw, 0.451034576472, rel_tol=0, abs_tol=1e-10)
    assert math.isclose(hw ** 3, 10 ** (db / 20.0), rel_tol=0, abs_tol=2e-7)


def test_other_reference_positions():
    refs = [
        (0.10, -34.0460205078125, -545),
        (0.50, -10.4270467758179, -167),
        (1.00, 0.0, 0),
    ]
    for ui0, db0, pg0 in refs:
        ui, db, pg, hw = mod.derive_windows_state(ui0 ** 3, False)
        assert math.isclose(ui, ui0, abs_tol=2e-12)
        assert math.isclose(db, db0, abs_tol=1e-9)
        assert pg == pg0
        assert math.isclose(hw ** 3, 10 ** (db / 20.0), rel_tol=0, abs_tol=2e-7)


def test_muted_state_retains_taper_gain_but_clamps_dolby_postgain():
    ui, db, pg, hw = mod.derive_windows_state(0.25 ** 3, True)
    assert math.isclose(ui, 0.25, abs_tol=1e-12)
    assert math.isclose(db, -20.7474098205566, abs_tol=1e-9)
    assert pg == -1200
    assert math.isclose(hw ** 3, 10 ** (db / 20.0), abs_tol=2e-7)


def test_recovered_direct_postgain_relation_still_available():
    assert mod.postgain_from_linear_gain(1.0) == 0
    assert mod.postgain_from_linear_gain(0.0) == -1200
    gain = 10 ** ((-423 / 16.0) / 20.0)
    assert mod.postgain_from_linear_gain(gain) == -423


def test_extract_node_volume_and_id():
    snap = [{"id": 38, "info": {"props": {"node.name": mod.DEFAULT_NODE}, "params": {"Props": [
        {"volume": 1.0, "mute": False, "channelVolumes": [0.015625, 0.015625], "softMute": False},
    ]}}}]
    gain, muted = mod.extract_node_volume(snap)
    assert gain == 0.015625
    assert muted is False
    assert mod.extract_node_id(snap, mod.DEFAULT_NODE) == 38


def test_balance_does_not_reduce_master_proxy():
    snap = [{"info": {"props": {"node.name": mod.DEFAULT_NODE}, "params": {"Props": [
        {"mute": False, "channelVolumes": [0.125, 0.015625]},
    ]}}}]
    gain, _ = mod.extract_node_volume(snap)
    assert gain == 0.125


def test_control_write_preserves_existing_profile_slots(tmp_path):
    path = tmp_path / "control"
    path.write_bytes(bytes((3, 2)))
    mod.write_postgain_request(path, -332)
    data = path.read_bytes()
    assert len(data) == mod.CONTROL_BYTES
    assert data[:2] == bytes((3, 2))
    assert struct.unpack_from("<i", data, mod.POSTGAIN_REQUEST_OFF)[0] == -332
    assert struct.unpack_from("<i", data, mod.POSTGAIN_ACK_OFF)[0] == mod.POSTGAIN_NONE


def test_control_update_preserves_plugin_ack(tmp_path):
    path = tmp_path / "control"
    page = bytearray(mod.CONTROL_BYTES)
    page[0:2] = bytes((5, 5))
    struct.pack_into("<i", page, mod.POSTGAIN_REQUEST_OFF, -423)
    struct.pack_into("<i", page, mod.POSTGAIN_ACK_OFF, -423)
    path.write_bytes(page)
    mod.write_postgain_request(path, -332)
    data = path.read_bytes()
    assert data[:2] == bytes((5, 5))
    assert struct.unpack_from("<i", data, mod.POSTGAIN_REQUEST_OFF)[0] == -332
    assert struct.unpack_from("<i", data, mod.POSTGAIN_ACK_OFF)[0] == -423


def test_apply_state_coordinates_postgain_and_hidden_hardware(tmp_path, monkeypatch):
    writes = []
    volumes = []
    monkeypatch.setattr(mod, "write_postgain_request", lambda path, value: writes.append((path, value)))
    monkeypatch.setattr(mod, "set_hardware_volume", lambda node_id, scalar, wpctl="wpctl": volumes.append((node_id, scalar, wpctl)))
    control = tmp_path / "control"
    sig = mod.apply_state((0.25 ** 3, False), 69, control, False, None, "wpctl")
    assert sig is not None
    assert writes == [(control, -332)]
    assert len(volumes) == 1 and volumes[0][0] == 69
    assert math.isclose(volumes[0][1], 0.451034576472, abs_tol=1e-10)
    # Echo/repeated state should not write either target again.
    sig2 = mod.apply_state((0.25 ** 3, False), 69, control, False, sig, "wpctl")
    assert sig2 == sig
    assert len(writes) == 1
    assert len(volumes) == 1


def test_dry_run_never_requires_hardware_node(tmp_path):
    sig = mod.apply_state((0.25 ** 3, False), None, tmp_path / "control", True, None)
    assert sig is not None


def test_monitor_seeds_initial_snapshot_before_monitor_events(tmp_path, monkeypatch):
    from types import SimpleNamespace

    class FakeStdout:
        def fileno(self):
            return 9

    class FakeProc:
        stdout = FakeStdout()
        returncode = 0
        def poll(self):
            return 0
        def terminate(self):
            raise AssertionError("completed fake monitor must not be terminated")
        def wait(self):
            return 0

    args = SimpleNamespace(
        pw_dump="pw-dump",
        node=mod.DEFAULT_NODE,
        hardware_node=mod.DEFAULT_HARDWARE_NODE,
        settle_ms=0,
        bootstrap_ms=5000,
        bootstrap_guard_ms=1000,
        control=tmp_path / "control",
        dry_run=False,
        wpctl="wpctl",
    )
    applied = []
    monkeypatch.setattr(mod.subprocess, "Popen", lambda *a, **k: FakeProc())
    monkeypatch.setattr(mod, "settled_state", lambda *a, **k: ((0.25 ** 3, False), 69))
    monkeypatch.setattr(mod, "iter_json_stream", lambda fd: iter(()))
    monkeypatch.setattr(
        mod,
        "apply_state",
        lambda state, hw, control, dry, last, wpctl="wpctl": applied.append((state, hw, control, dry, last, wpctl)) or (-332, 451034576),
    )
    assert mod.run_monitor(args) == 0
    assert applied == [((0.25 ** 3, False), 69, args.control, False, None, "wpctl")]


def test_monitor_bootstrap_retries_until_nodes_exist(tmp_path, monkeypatch):
    from types import SimpleNamespace

    class FakeStdout:
        def fileno(self): return 9
    class FakeProc:
        stdout = FakeStdout(); returncode = 0
        def poll(self): return 0
        def terminate(self): raise AssertionError
        def wait(self): return 0

    args = SimpleNamespace(
        pw_dump="pw-dump", node=mod.DEFAULT_NODE, hardware_node=mod.DEFAULT_HARDWARE_NODE,
        settle_ms=0, bootstrap_ms=5000, bootstrap_guard_ms=1000, control=tmp_path / "control",
        dry_run=False, wpctl="wpctl",
    )
    seq = iter([(None, None), (None, 69), ((0.25 ** 3, False), 69)])
    calls = []
    monkeypatch.setattr(mod.subprocess, "Popen", lambda *a, **k: FakeProc())
    monkeypatch.setattr(mod, "settled_state", lambda *a, **k: next(seq))
    monkeypatch.setattr(mod, "iter_json_stream", lambda fd: iter(()))
    monkeypatch.setattr(mod.time, "sleep", lambda _s: None)
    monkeypatch.setattr(mod, "apply_state", lambda state, hw, *a, **k: calls.append((state, hw)) or (-332, 451034576))
    assert mod.run_monitor(args) == 0
    assert calls == [((0.25 ** 3, False), 69)]


def test_bootstrap_guard_ignores_stale_unity_monitor_replay(tmp_path, monkeypatch):
    from types import SimpleNamespace

    class FakeStdout:
        def fileno(self): return 9
    class FakeProc:
        stdout = FakeStdout(); returncode = 0
        def poll(self): return 0
        def terminate(self): raise AssertionError
        def wait(self): return 0

    args = SimpleNamespace(
        pw_dump="pw-dump", node=mod.DEFAULT_NODE, hardware_node=mod.DEFAULT_HARDWARE_NODE,
        settle_ms=0, bootstrap_ms=5000, bootstrap_guard_ms=1000,
        control=tmp_path / "control", dry_run=False, wpctl="wpctl",
    )
    stale_unity = [{"id": 41, "info": {"props": {"node.name": mod.DEFAULT_NODE}, "params": {"Props": [
        {"mute": False, "channelVolumes": [1.0, 1.0]}
    ]}}}]
    snapshots = iter([((0.25 ** 3, False), 69), ((0.25 ** 3, False), 69)])
    applied = []
    monkeypatch.setattr(mod.subprocess, "Popen", lambda *a, **k: FakeProc())
    monkeypatch.setattr(mod, "settled_state", lambda *a, **k: next(snapshots))
    monkeypatch.setattr(mod, "iter_json_stream", lambda fd: iter((stale_unity,)))
    monkeypatch.setattr(mod.time, "monotonic", lambda: 0.0)
    monkeypatch.setattr(mod, "apply_state", lambda state, hw, *a, **k: applied.append((state, hw)) or (-332, 451034576))
    assert mod.run_monitor(args) == 0
    assert applied == [((0.25 ** 3, False), 69), ((0.25 ** 3, False), 69)]
    assert all(state != (1.0, False) for state, _hw in applied)

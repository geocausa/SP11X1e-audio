import importlib.util
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "ubig" / "app" / "ubig_control.py"
spec = importlib.util.spec_from_file_location("ubig_control_app", MODULE)
mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)


def test_db_raw_domain_roundtrip():
    assert mod.db_to_raw(-12.0) == -192
    assert mod.db_to_raw(0.0) == 0
    assert mod.db_to_raw(12.0) == 192
    assert mod.raw_to_db(-192) == -12.0
    assert mod.raw_to_db(48) == 3.0


def test_control_page_profile_and_eq_request_ack_layout(tmp_path):
    path = tmp_path / "ubig-control-v2"
    with mod.ControlPage(path) as control:
        initial = control.snapshot()
        assert initial.request_generation == 0
        assert initial.desired_profile == 0
        generation = control.request_profile("Movie")
        assert generation == 1
        curve = tuple(range(-10, 10))
        generation = control.request_custom_eq(curve)
        assert generation == 2
        snapshot = control.snapshot()
        assert snapshot.desired_profile == mod.PROFILE_CUSTOM
        assert snapshot.custom_eq == curve
        assert snapshot.desired_flags & mod.CUSTOM_EQ_VALID
        assert snapshot.request_pending
    assert path.stat().st_size == mod.CONTROL_BYTES
    assert path.stat().st_mode & 0o777 == 0o600


def test_saved_state_roundtrip(tmp_path):
    path = tmp_path / "control.json"
    curve = tuple(index - 10 for index in range(20))
    assert mod.save_state("Custom", curve, path) == path
    profile, loaded = mod.load_saved_state(path)
    assert profile == mod.PROFILE_CUSTOM
    assert loaded == curve
    payload = json.loads(path.read_text())
    assert payload["profile"] == "Custom"
    assert path.stat().st_mode & 0o777 == 0o600


def test_debian_package_tracks_only_userspace_controller():
    build = (ROOT / "packaging" / "debian" / "build-control-deb.sh").read_text()
    control = (ROOT / "packaging" / "debian" / "control.in").read_text()
    assert "ubig-sp11-candidate.so" not in build
    assert "gir1.2-gtk-4.0" in control
    assert "dpkg-deb --build --root-owner-group" in build


def test_engine_live_flag(tmp_path):
    path = tmp_path / "ubig-control-v2"
    with mod.ControlPage(path) as control:
        assert not control.snapshot().engine_live
        control.mapping[mod.ENGINE_FLAGS_OFF:mod.ENGINE_FLAGS_OFF + 4] = (mod.ENGINE_LIVE).to_bytes(4, "little")
        assert control.snapshot().engine_live


def test_profile_dropdown_is_immediate_apply():
    gui = (ROOT / "ubig" / "app" / "ubig_geq.py").read_text()
    assert 'notify::selected' in gui
    assert 'on_profile_selected' in gui
    assert 'Apply profile' not in gui
    assert 'Music and Game are equivalent in the SP11 Windows stereo policy' in gui


def test_restore_without_saved_state_is_noop(tmp_path, monkeypatch, capsys):
    state = tmp_path / "missing.json"
    monkeypatch.setattr(mod, "default_state_path", lambda: state)
    gui_spec = importlib.util.spec_from_file_location("ubig_geq_restore_test", ROOT / "ubig" / "app" / "ubig_geq.py")
    # The GUI module requires GI, so validate the source policy without importing GTK.
    text = (ROOT / "ubig" / "app" / "ubig_geq.py").read_text()
    assert 'if not state_path.exists()' in text
    assert 'keeping engine startup profile' in text

def test_debian_package_installs_restore_autostart():
    build = (ROOT / "packaging" / "debian" / "build-control-deb.sh").read_text()
    restore = ROOT / "packaging" / "debian" / "io.github.geocausa.UbiG.Restore.desktop"
    assert restore.exists()
    assert '/etc/xdg/autostart' in build
    assert '--restore' in restore.read_text()

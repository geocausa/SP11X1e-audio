from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / "deploy" / "wireplumber" / "98-sp11-production-endpoint-policy.conf"
PULSE_POLICY = ROOT / "deploy" / "wireplumber" / "99-sp11-pulse-hide-hardware.conf"
PULSE_SCRIPT = ROOT / "deploy" / "wireplumber" / "sp11-pulse-hide-hardware.lua"
INSTALLER = ROOT / "deploy" / "ubig" / "install-production.sh"
BYPASS = ROOT / "deploy" / "pipewire" / "98-sp11-ubig-bypass.conf"


def test_production_policy_hides_physical_speaker_backend():
    text = POLICY.read_text()
    assert 'node.name = "alsa_output.platform-sound.HiFi__Speaker__sink"' in text
    assert "node.hidden = true" in text
    assert "priority.session = 0" in text


def test_pulse_bridge_policy_hides_only_the_physical_speaker():
    conf = PULSE_POLICY.read_text()
    script = PULSE_SCRIPT.read_text()
    assert 'name = "sp11-pulse-hide-hardware.lua"' in conf
    assert "custom.sp11-pulse-hide-hardware = required" in conf
    assert 'p["config.name"] == "pipewire-pulse.conf"' in script
    assert 'p["application.process.binary"] == "pipewire"' in script
    assert 'p["client.api"] == nil' in script
    assert 'alsa_output.platform-sound.HiFi__Speaker__sink' in script
    assert 'client:update_permissions { [id] = "-" }' in script


def test_production_installer_retires_autoloaded_bypass_and_installs_policy():
    text = INSTALLER.read_text()
    assert "98-sp11-production-endpoint-policy.conf" in text
    assert "99-sp11-pulse-hide-hardware.conf" in text
    assert "sp11-pulse-hide-hardware.lua" in text
    assert 'rm -f "$BYPASS_ACTIVE"' in text
    assert "diagnostic bypass is active in production" in text
    assert "physical speaker backend is not hidden" in text
    assert "physical speaker is visible through pipewire-pulse" in text


def test_bypass_file_is_explicitly_diagnostic_only():
    text = BYPASS.read_text()
    assert "ON-DEMAND DIAGNOSTIC/HISTORICAL UTILITY ONLY" in text

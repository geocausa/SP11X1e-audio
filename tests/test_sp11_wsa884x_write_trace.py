from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TRACE = ROOT / "tools/diagnostics/wsa884x-write-trace/sp11-wsa884x-write-trace.sh"
SERVICE = ROOT / "deploy/diagnostics/wsa884x-write-trace/sp11-wsa884x-write-trace.service"


def test_wsa884x_write_trace_is_observation_only():
    text = TRACE.read_text()
    forbidden = (
        "amixer",
        "wpctl",
        "pw-play",
        "aplay",
        "paplay",
        "speaker-test",
        "systemctl start",
        "systemctl stop",
        "modprobe",
        "devmem",
        "/sys/kernel/debug/regmap",
        "regmap_read",
        "regmap_write(",
        "DRE_CTL_1",
        "CSR_GAIN_EN",
    )
    for token in forbidden:
        assert token not in text


def test_wsa884x_write_trace_filters_only_wsa884x_aperture_and_cleans_up():
    text = TRACE.read_text()
    assert "p:sp11_wsa884x_write _regmap_write" in text
    assert "reg >= 12288 && reg <= 13823" in text
    assert "options/stacktrace" in text
    assert "trap cleanup EXIT" in text
    assert "-:sp11_wsa884x_write" in text
    assert "sleep 40" in text


def test_wsa884x_write_trace_uses_boot_unique_output_names():
    text = TRACE.read_text()
    assert 'BOOT_ID="$(cat /proc/sys/kernel/random/boot_id)"' in text
    assert 'wsa884x-write-${BOOT_ID}.trace' in text
    assert 'wsa884x-write-${BOOT_ID}.meta' in text
    assert "wsa884x-write-boot.trace" not in text


def test_wsa884x_write_trace_service_runs_before_graphical_target():
    text = SERVICE.read_text()
    assert "Before=graphical.target" in text
    assert "After=local-fs.target systemd-modules-load.service" in text
    assert "ExecStart=/usr/local/libexec/sp11-wsa884x-write-trace.sh" in text
    assert "pipewire" not in text.lower()
    assert "wireplumber" not in text.lower()

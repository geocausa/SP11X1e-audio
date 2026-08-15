from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TRACE = ROOT / "tools/diagnostics/wsa-runtime-write-trace/sp11-wsa-runtime-write-trace.sh"
SERVICE = ROOT / "deploy/diagnostics/wsa-runtime-write-trace/sp11-wsa-runtime-write-trace.service"


def test_runtime_write_trace_has_no_audio_control_or_playback_actions():
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
        "regmap",
        "/sys/kernel/debug/regmap",
        "DRE_CTL_1",
        "CSR_GAIN_EN",
    )
    for token in forbidden:
        assert token not in text


def test_runtime_write_trace_only_observes_existing_asoc_write_boundaries():
    text = TRACE.read_text()
    assert "p:sp11_wsa_cupd snd_soc_component_update_bits" in text
    assert "p:sp11_wsa_cwrite snd_soc_component_write" in text
    assert "options/stacktrace" in text
    assert "sleep 35" in text
    assert 'cat "$TRACE/trace" > "$OUT"' in text


def test_runtime_write_trace_cleans_up_kprobes_and_stacktrace_option():
    text = TRACE.read_text()
    assert "-:sp11_wsa_cupd" in text
    assert "-:sp11_wsa_cwrite" in text
    assert 'echo 0 > "$TRACE/options/stacktrace"' in text
    assert "trap cleanup EXIT" in text


def test_runtime_write_trace_service_starts_before_graphical_session():
    text = SERVICE.read_text()
    assert "Before=graphical.target" in text
    assert "After=local-fs.target systemd-modules-load.service" in text
    assert "ExecStart=/usr/local/libexec/sp11-wsa-runtime-write-trace.sh" in text
    assert "pipewire" not in text.lower()
    assert "wireplumber" not in text.lower()

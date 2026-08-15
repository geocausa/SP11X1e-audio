from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ARM = ROOT / "deploy/diagnostics/sp11-wsa-boot-lifecycle-trace.sh"
COLLECT = ROOT / "deploy/diagnostics/sp11-wsa-boot-lifecycle-collect.sh"
SERVICE = ROOT / "deploy/diagnostics/sp11-wsa-boot-lifecycle-trace.service"


def test_trace_harness_is_read_only_with_respect_to_audio_stack():
    text = ARM.read_text()
    forbidden = (
        "regmap",
        "amixer",
        "wpctl",
        "pw-play",
        "aplay",
        "paplay",
        "systemctl start",
        "systemctl stop",
        "modprobe",
        "snd_soc_component_write",
    )
    for token in forbidden:
        assert token not in text


def test_trace_harness_covers_producer_consumer_and_soundwire_ordering():
    text = ARM.read_text()
    required = (
        "wsa884x_hw_params",
        "wsa884x_mute_stream",
        "wsa884x_spkr_event",
        "wsa_macro_enable_interpolator",
        "wsa_macro_config_compander",
        "qcom_swrm_hw_params",
        "swrm_runtime_suspend",
        "swrm_runtime_resume",
        "sdw_prepare_stream",
        "sdw_enable_stream",
        "sdw_disable_stream",
        "sdw_deprepare_stream",
    )
    for symbol in required:
        assert symbol in text


def test_boot_service_arms_before_graphical_session_without_audio_dependencies():
    text = SERVICE.read_text()
    assert "Before=display-manager.service graphical.target" in text
    assert "Type=oneshot" in text
    assert "ExecStart=/usr/local/sbin/sp11-wsa-boot-lifecycle-trace" in text
    assert "pipewire" not in text.lower()
    assert "wireplumber" not in text.lower()


def test_collector_only_stops_tracing_and_copies_trace_buffer():
    text = COLLECT.read_text()
    assert 'echo 0 > "$TRACE/tracing_on"' in text
    assert 'cat "$TRACE/trace" > "$OUT"' in text
    for token in ("regmap", "amixer", "wpctl", "pw-play", "aplay", "systemctl"):
        assert token not in text

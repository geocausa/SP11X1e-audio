from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "deploy" / "ubig" / "sp11-ubig"


def test_helper_uses_production_v2_control_page():
    text = HELPER.read_text()
    assert "ubig-control-v2" in text
    assert 'UBIGCTL="${UBIGCTL:-/usr/bin/ubigctl}"' in text
    assert "sp11-ubig-profile.control" not in text
    assert 'UBIG_CONTROL_PATH="$CONTROL_FILE" "$UBIGCTL" profile' in text


def test_helper_resolves_exact_ubig_sink_not_prefix_siblings():
    text = HELPER.read_text()
    assert 'grep -F " $1 "' in text
    assert "effect_input.sp11_ubig_bypass" not in text
    assert "D=$(node_id 'effect_input.sp11_ubig')" in text


def test_helper_maps_legacy_names_to_v2_profiles():
    text = HELPER.read_text()
    for legacy, canonical in (
        ("dynamic", "Dynamic"),
        ("movie", "Movie"),
        ("music", "Music"),
        ("game", "Game"),
        ("voice", "Voice"),
        ("onlinecourse", "Course"),
        ("personalize", "Custom"),
    ):
        assert f"{legacy}) echo {canonical}" in text

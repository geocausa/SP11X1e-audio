from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEPLOY = ROOT / "deploy" / "ubig-candidate"


def test_candidate_pipewire_template_preserves_golden_node_contract():
    text = (DEPLOY / "98-sp11-ubig-candidate.conf.in").read_text()
    assert 'plugin = "@PLUGIN@"' in text
    assert "label  = ubig_sp11_candidate" in text
    for name in (
        "effect_input.sp11_windows_dolby",
        "effect_input.sp11_windows_dolby_engine",
        "effect_output.sp11_windows_dolby",
    ):
        assert name in text
    assert "sp11_dolby_windows_chain" not in text


def test_candidate_prepare_and_switch_keep_deblob_and_rollback_gates():
    prepare = (DEPLOY / "prepare.sh").read_text()
    switch = (DEPLOY / "switch.sh").read_text()
    assert "candidate-control-check" in prepare
    assert "c993c123f2cb3b92776754da2383217e00b5f290664571f12cfb62b9afb3a175" in prepare
    assert "DolbyAPOVR\\.dll|DolbyAPOvlldp150\\.dll|sp11_pe_load" in prepare
    assert "MemoryDenyWriteExecute=yes" in switch
    assert "Environment=UBIG_CONTROL_FORMAT=ubig-v2" in switch
    assert "98-sp11-windows-dolby.conf.rollback" in switch
    assert 'install -m 0644 "$ROLLBACK_CONF" "$ACTIVE_CONF"' in switch


def test_candidate_shell_scripts_parse():
    subprocess.run(
        ["bash", "-n", str(DEPLOY / "prepare.sh"), str(DEPLOY / "switch.sh")],
        check=True,
    )

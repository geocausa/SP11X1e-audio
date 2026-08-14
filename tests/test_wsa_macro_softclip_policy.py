from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / "patches" / "0053-ASoC-lpass-wsa-macro-fix-v2.5-softclip-address.patch"
UCM = ROOT / "deploy" / "ucm2" / "Qualcomm" / "x1e80100" / "SP11-HiFi.conf"


def test_softclip_control_uses_versioned_register_base():
    text = PATCH.read_text()
    assert "wsa->reg_layout->softclip0_reg_base +" in text
    assert "WSA_MACRO_SOFTCLIP_CTRL_OFFSET" in text
    assert "softclip_path * wsa->reg_layout->softclip1_reg_offset" in text


def test_sp11_ucm_does_not_enable_unproven_softclip_blocks():
    text = UCM.read_text()
    assert "name='WSA WSA_Softclip0 Enable' 0" in text
    assert "name='WSA WSA_Softclip1 Enable' 0" in text
    assert "name='WSA WSA_Softclip0 Enable' 1" not in text
    assert "name='WSA WSA_Softclip1 Enable' 1" not in text

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / "patches/0055-ASoC-wsa884x-match-Windows-DRE-CTL1-lifecycle.patch"
UCM = ROOT / "deploy/ucm2/Qualcomm/x1e80100/SP11-HiFi.conf"


def test_sp11_ucm_pins_inverted_pa_control_to_raw_zero():
    text = UCM.read_text()
    assert text.count("SpkrLeft PA Volume' 31") == 2
    assert text.count("SpkrRight PA Volume' 31") == 2
    assert "user value 31 maps the CSR gain field to raw zero" in text
    assert "patch 0055 keeps CSR_GAIN_EN clear" in text
    assert "DRE_CTL_1" in text
    assert "final VOL_CTRL" in text


def test_dre_patch_pins_windows_init_value_and_disables_sp11_csr_gain():
    text = PATCH.read_text()
    assert "regmap_write(regmap, WSA884X_DRE_CTL_1, 0x00);" in text
    assert "wsa884x->supply_config == WSA884X_SUPPLY_2S ?" in text
    assert "0x0 : 0x1" in text
    assert "Windows leaves DRE_CTL_1 at 0x00" in text

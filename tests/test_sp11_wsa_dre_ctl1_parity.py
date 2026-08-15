from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / "patches/0055-ASoC-wsa884x-match-Windows-DRE-CTL1-lifecycle.patch"
UCM = ROOT / "deploy/ucm2/Qualcomm/x1e80100/SP11-HiFi.conf"


def test_sp11_ucm_does_not_program_pa_volume():
    text = UCM.read_text()
    assert "SpkrLeft PA Volume'" not in text
    assert "SpkrRight PA Volume'" not in text
    assert "DRE_CTL_1" in text
    assert "final VOL_CTRL" in text


def test_dre_patch_pins_windows_init_value_and_disables_sp11_csr_gain():
    text = PATCH.read_text()
    assert "regmap_write(regmap, WSA884X_DRE_CTL_1, 0x00);" in text
    assert "wsa884x->supply_config == WSA884X_SUPPLY_2S ?" in text
    assert "0x0 : 0x1" in text
    assert "Windows leaves DRE_CTL_1 at 0x00" in text

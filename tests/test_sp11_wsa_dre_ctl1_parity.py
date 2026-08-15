from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / "patches/0055-ASoC-wsa884x-match-Windows-DRE-CTL1-lifecycle.patch"
UCM = ROOT / "deploy/ucm2/Qualcomm/x1e80100/SP11-HiFi.conf"
README = ROOT / "deploy/wsa-dre-ctl1/README.md"


def test_production_ucm_stays_on_known_good_pre_dre_pa_state():
    text = UCM.read_text()
    assert text.count("SpkrLeft PA Volume' 24") == 2
    assert text.count("SpkrRight PA Volume' 24") == 2
    assert "SpkrLeft PA Volume' 31" not in text
    assert "SpkrRight PA Volume' 31" not in text
    assert "validated operating point 24" in text


def test_rejected_dre_patch_is_preserved_but_not_production_policy():
    text = PATCH.read_text()
    assert "regmap_write(regmap, WSA884X_DRE_CTL_1, 0x00);" in text
    assert "wsa884x->supply_config == WSA884X_SUPPLY_2S ?" in text
    assert "0x0 : 0x1" in text
    assert "Windows leaves DRE_CTL_1 at 0x00" in text

    readme = README.read_text()
    assert "REJECTED — cold-boot acoustic failure" in readme
    assert "Do not arm `sp11-audio-wsa-dre-ctl1` again" in readme

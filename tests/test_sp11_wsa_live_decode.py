import pytest

from tools.sp11_wsa_live_decode import decode_journal, samples_from_journal


JOURNAL = """\
[  540.684150] host kernel: wsa884x-codec sdw:1:0:0217:0204:00:0: SP11 WSA live sample=0 failed=0x0 pa=01 sta=2f/00 err=00/00 intr=00/00 adc=c6e0 temp=72c0 vbat=c580 wavg=00 cps=00 ilim=44
[  540.685086] host kernel: wsa884x-codec sdw:1:0:0217:0204:00:1: SP11 WSA live sample=0 failed=0x0 pa=01 sta=2f/00 err=00/00 intr=00/00 adc=70e0 temp=7080 vbat=c5c0 wavg=00 cps=00 ilim=44
[  540.804232] host kernel: wsa884x-codec sdw:1:0:0217:0204:00:1: SP11 WSA live sample=1 failed=0x0 pa=01 sta=2f/00 err=00/00 intr=00/00 adc=c6e0 temp=70c0 vbat=c5c0 wavg=00 cps=00 ilim=44
[  540.804922] host kernel: wsa884x-codec sdw:1:0:0217:0204:00:0: SP11 WSA live sample=1 failed=0x0 pa=01 sta=2f/00 err=00/00 intr=00/00 adc=c660 temp=7300 vbat=c580 wavg=00 cps=00 ilim=44
"""


def test_samples_from_journal_parses_both_amplifiers():
    samples = samples_from_journal(JOURNAL)
    assert len(samples) == 4
    assert samples[0]["amplifier"] == 0
    assert samples[0]["adc_raw"] == "0xc6e0"
    assert samples[1]["amplifier"] == 1
    assert samples[1]["temperature_raw"] == "0x7080"


def test_decode_journal_summarizes_raw_values_and_current_limit():
    decoded = decode_journal(JOURNAL)
    summary = decoded["summary"]
    assert summary["sample_count"] == 4
    assert summary["amplifier_count"] == 2
    amp0 = summary["amplifiers"]["0"]
    assert amp0["sequences"] == [0, 1]
    assert amp0["failed_masks"] == ["0x0"]
    assert amp0["adc_raw"]["unique_count"] == 2
    assert amp0["temperature_raw"]["minimum"] == "0x72c0"
    assert amp0["current_limit"] == [{
        "register": "0x44",
        "override_enabled": False,
        "current_limit_code": 17,
    }]


def test_decode_journal_rejects_empty_input():
    with pytest.raises(ValueError, match="no SP11 WSA"):
        decode_journal("unrelated kernel line\n")

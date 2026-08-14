from tools.decode_qcaucd_command_fifo import decode_word, parse_log, summarize


def test_decode_command_packing_and_port_name():
    record = decode_word(bytes.fromhex("30 06 21 03"), 0, 12)

    assert record["register"] == "0x0630"
    assert record["command_id"] == 1
    assert record["logical_device"] == 2
    assert record["data"] == "0x03"
    assert record["data_port"] == 6
    assert record["function"] == "CPS"
    assert record["register_name"] == "ChannelEnable_B1"


def test_parse_requires_marker_and_pairs_one_db_row_per_marker():
    text = """
ffff`0000  30 06 21 03
CODEX_DP6BRIDGE datap=ffff0001
ffff`0001  30 05 22 03  0... ignored ASCII
ffff`0002  30 06 21 03
CODEX_DP6BRIDGE datap=ffff0003
not a db row
ffff`0003  30 06 11 03
"""

    records = parse_log(text)

    assert len(records) == 2
    assert records[0]["function"] == "VISENSE"
    assert records[0]["logical_device"] == 2
    assert records[1]["function"] == "CPS"
    assert records[1]["logical_device"] == 1


def test_summary_recognizes_repeated_cycles_and_omits_final_tail():
    records = []
    for index, raw in enumerate(
        (
            "f0 00 17 01",
            "30 05 20 03",
            "f0 00 11 01",
            "30 05 22 03",
            "44 00 f3 02",
        )
    ):
        records.append(decode_word(bytes.fromhex(raw), index, index + 1))

    summary = summarize(records)

    assert summary["record_count"] == 5
    assert summary["cycle_lengths"] == [2, 2]
    assert summary["cycles_identical_ignoring_command_id"] is True
    assert summary["positive_channel_enable_counts"] == {
        "dp5_dev2": 2,
    }

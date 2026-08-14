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


def test_parse_accepts_v2_direct_fifo_and_rejects_adjacent_controller_fifos():
    text = """
CODEX_QCAUCD_V2CMD addr=0000000006b15020 datap=ffff0001 sel=1
ffff`0001  b1 34 10 00
CODEX_QCAUCD_V2CMD addr=0000000006b15024 datap=ffff0002 sel=1
ffff`0002  01 34 11 01
CODEX_QCAUCD_V2CMD addr=0000000006b15050 datap=ffff0003 sel=0
ffff`0003  00 00 00 00
CODEX_QCAUCD_V2CMD_READ_RESULT addr=6b15050 datap=ffff0003
ffff`0003  21 00 01 40
CODEX_QCAUCD_V2CMD addr=0000000006b15020 datap=ffff0004 sel=1
ffff`0004  b1 34 20 00
"""

    records = parse_log(text)

    assert len(records) == 2
    assert [record["register"] for record in records] == ["0x34b1", "0x34b1"]
    assert [record["logical_device"] for record in records] == [1, 2]
    assert all(record["marker"] == "CODEX_QCAUCD_V2CMD" for record in records)
    assert all(record["fifo_address"] == "0x06b15020" for record in records)


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

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "protected_topology", ROOT / "tools/build_sp11_protected_topology.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


def test_graph_calibration_variant_defaults_legacy_manifest_to_windows_full():
    assert MODULE.validate_graph_calibration_variant({}, "windows-full") == "windows-full"


def test_graph_calibration_variant_requires_explicit_settable_opt_in():
    record = {"variant": "settable-v1"}
    try:
        MODULE.validate_graph_calibration_variant(record, "windows-full")
    except ValueError as exc:
        assert "requires explicit --graph-calibration-variant settable-v1" in str(exc)
    else:
        raise AssertionError("settable-v1 stage compiled without explicit opt-in")
    assert MODULE.validate_graph_calibration_variant(record, "settable-v1") == "settable-v1"


def test_private_data_header():
    payload = bytes.fromhex("0100000002000000")
    block = MODULE.private_data(0x53503101, payload)
    assert block.hex() == "080000000131505300000000000000000100000002000000"


def test_source_link_token_slots():
    module = {
        "iid": "0x4002",
        "module_id": "0x07001011",
        "module_name": "SPLITTER",
        "subgraph_id": "0xb0000001",
        "container_id": "0xe0000001",
        "properties": {"max_input_ports": 1, "max_output_ports": 7},
    }
    edges = [
        {
            "source_port": 1,
            "destination_iid": "0x4003",
            "destination_port": 2,
        }
    ]
    rendered = MODULE.module_tuple(module, edges)
    assert "token206 1" in rendered
    assert "token207 2" in rendered
    assert "token209 16387" in rendered


def test_windows_pull_endpoint_keeps_its_canonical_module_id():
    module = {
        "iid": "0x4660",
        "module_id": "0x07001006",
        "module_name": "SH_MEM_PULL_MODE",
        "subgraph_id": "0xb000007e",
        "container_id": "0xe000004c",
        "properties": {"max_input_ports": 0, "max_output_ports": 1},
    }
    rendered = MODULE.module_tuple(module, [])
    assert "token200 117444614" in rendered
    assert "token200 117444608" not in rendered


def test_root_pcm_converter_uses_windows_internal_layout():
    module = {
        "iid": "0x465f",
        "module_id": "0x07001003",
        "module_name": "PCM_CNV",
        "subgraph_id": "0xb000007e",
        "container_id": "0xe000004c",
        "properties": {"max_input_ports": 1, "max_output_ports": 1},
    }
    rendered = MODULE.module_tuple(module, [])
    assert "token252 3" in rendered


def test_vi_codec_dma_source_maps_to_wsa_tx0_backend():
    module = {
        "iid": "0x4026",
        "module_id": "0x07001024",
        "module_name": "CODEC_DMA_SOURCE",
        "subgraph_id": "0xb0000001",
        "container_id": "0xe0000007",
        "properties": {"max_input_ports": 0, "max_output_ports": 1},
    }
    rendered = MODULE.module_tuple(module, [])
    assert "token263 106" in rendered


def test_cps_codec_dma_source_maps_to_wsa_tx1_backend():
    module = {
        "iid": "0x402b",
        "module_id": "0x07001024",
        "module_name": "CODEC_DMA_SOURCE",
        "subgraph_id": "0xb0000001",
        "container_id": "0xe0000005",
        "properties": {"max_input_ports": 0, "max_output_ports": 1},
    }
    rendered = MODULE.module_tuple(module, [])
    assert "token263 108" in rendered


def test_backend_and_frontend_tuple_directions_are_distinct():
    backend = MODULE.simple_tuple("WSA", 0xB0000001, 0, 1)
    frontend = MODULE.simple_tuple("MultiMedia1", 0xB0000001, 0, 2)
    assert "token4 1" in backend
    assert "token4 2" in frontend


def _control_fixture(include_headroom: bool = True):
    import struct

    links = [
        # Compact synthetic records; only the fourth identity/intent is a hard gate.
        struct.pack("<IIIII", 0x4024, 0x80000000, 0x4027, 0x80000000, 0),
        struct.pack("<IIIII", 0x4028, 0x80000000, 0x4027, 0x80000001, 0),
        struct.pack("<IIIII", 0x4157, 0x80000007, 0x40DF, 0xC0000001, 0),
    ]
    intent = 0x08001118 if include_headroom else 0xDEADBEEF
    links.append(
        struct.pack("<IIIII", 0x4664, 0x80000000, 0x4663, 0x80000000, 2)
        + struct.pack("<III", 0x08001062, 4, intent)
        + struct.pack("<III", 0x0800136F, 4, 1)
    )
    aggregate = struct.pack("<I", 4) + b"".join(links)
    return {
        "link_count": 4,
        "linux_aggregate_payload_size": len(aggregate),
        "linux_aggregate_payload_hex": aggregate.hex(),
    }


def test_windows_default_control_payload_keeps_popless_headroom_link():
    import struct

    wrapped = MODULE.windows_default_control_payload(_control_fixture())
    size, param_id, reserved0, reserved1 = struct.unpack_from("<IIII", wrapped, 0)
    payload = wrapped[16:]
    assert size == len(payload)
    assert param_id == 0x08001061
    assert reserved0 == reserved1 == 0
    assert struct.unpack_from("<I", payload, 0)[0] == 4
    assert struct.pack("<IIIII", 0x4664, 0x80000000, 0x4663, 0x80000000, 2) in payload
    assert struct.pack("<I", 0x08001118) in payload


def test_windows_default_control_payload_rejects_three_link_regression():
    control = _control_fixture()
    control["link_count"] = 3
    try:
        MODULE.windows_default_control_payload(control)
    except ValueError as exc:
        assert "four control links" in str(exc)
    else:
        raise AssertionError("three-link DEFAULT control payload was accepted")


def test_windows_default_control_payload_rejects_missing_headroom_intent():
    try:
        MODULE.windows_default_control_payload(_control_fixture(False))
    except ValueError as exc:
        assert "headroom control link" in str(exc)
    else:
        raise AssertionError("DEFAULT control payload without POPLESS headroom was accepted")

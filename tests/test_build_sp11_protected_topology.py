import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "protected_topology", ROOT / "tools/build_sp11_protected_topology.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


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


def test_notification_pcm_converter_uses_windows_internal_layout():
    module = {
        "iid": "0x469d",
        "module_id": "0x07001003",
        "module_name": "PCM_CNV",
        "subgraph_id": "0xb0000082",
        "container_id": "0xe0000120",
        "properties": {"max_input_ports": 1, "max_output_ports": 1},
    }
    rendered = MODULE.module_tuple(module, [])
    assert "token252 3" in rendered


def test_notification_container_properties_are_evidence_locked():
    assert MODULE.CONTAINERS[0xE0000046] == (0x0B001001, 4096, 0xFFFFFFFF, 0xFFFFFFFF, 1)
    assert MODULE.CONTAINERS[0xE0000120] == (0x0B001001, 4096, 1, 0xFFFFFFFF, 1)
    assert MODULE.CONTAINERS[0xE0000071] == (0x0B001001, 1024, 0xFFFFFFFF, 0xFFFFFFFF, 1)


def test_family_frontend_and_volume_iids_are_distinct():
    assert MODULE.FAMILY_CONFIG["DEFAULT"]["frontend_iid"] == 0x4660
    assert MODULE.FAMILY_CONFIG["DEFAULT"]["volume_iid"] == 0x4A63
    assert MODULE.FAMILY_CONFIG["NOTIFICATION"]["frontend_iid"] == 0x469E
    assert MODULE.FAMILY_CONFIG["NOTIFICATION"]["volume_iid"] == 0x4A5F
    assert MODULE.FAMILY_CONFIG["NOTIFICATION"]["root_sal_port"] == 18

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

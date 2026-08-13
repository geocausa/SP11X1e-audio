import hashlib
import json
import tempfile
import unittest
from argparse import Namespace
from pathlib import Path
from unittest.mock import patch

from deploy.dolby import sp11_volume_sync_dispatch as dispatch
from deploy.dolby import sp11_windows_volume_transaction_sync as sync


class WindowsVolumeTransactionSyncTests(unittest.TestCase):
    def test_qcadcm_q28_quantizes_to_quarter_db_table(self):
        self.assertEqual(
            sync.qcadcm_q28_from_db(-20.7474098205566),
            round((10.0 ** (-20.5 / 20.0)) * sync.Q28_ONE),
        )

    def test_delta_table_requires_exact_thirty_size_classes(self):
        rows = []
        for step in range(1, 31):
            size = 272 if step in (3, 9, 24) else 216
            blob = b"x" * size
            rows.append({
                "gain_step": step,
                "serialized_hex": blob.hex(),
                "serialized_sha256": hashlib.sha256(blob).hexdigest(),
            })
        with tempfile.TemporaryDirectory() as tmpdir:
            table = Path(tmpdir) / "deltas.json"
            table.write_text(json.dumps({
                "source_sha256": sync.EXPECTED_ACDB_SHA256,
                "steps": rows,
            }))
            deltas = sync.load_deltas(table)
        self.assertEqual(len(deltas), 30)
        self.assertEqual(len(deltas[1]), 216)
        self.assertEqual(len(deltas[2]), 272)

    def test_apply_order_is_postgain_transaction_then_host_unity(self):
        calls = []
        deltas = tuple(b"d" * (272 if step in (3, 9, 24) else 216)
                       for step in range(1, 31))
        with patch.object(sync.base, "write_postgain_request",
                          side_effect=lambda path, value:
                          calls.append(("postgain", value))), \
             patch.object(sync, "write_transaction",
                          side_effect=lambda q28, delta, **kwargs:
                          calls.append(("transaction", q28, len(delta)))), \
             patch.object(sync.base, "set_hardware_volume",
                          side_effect=lambda node, scalar, wpctl:
                          calls.append(("host", scalar))):
            signature = sync.apply_transaction(
                (0.25 ** 3, False), 69, Path("control"), deltas,
                Path("tlv_write"), "hw:0", 321, "wpctl"
            )
        self.assertEqual(calls[0], ("postgain", -332))
        self.assertEqual(calls[1][0], "transaction")
        self.assertEqual(calls[1][2], 272)
        self.assertEqual(calls[2], ("host", 1.0))
        self.assertEqual(signature[0], -332)

    def test_dispatch_selects_candidate_only_when_control_exists(self):
        class CP:
            returncode = 0
            stdout = "numid=321,name='SP11 Windows Volume Transaction'"

        with patch.object(dispatch.subprocess, "run", return_value=CP()):
            self.assertTrue(dispatch.has_control())

    def test_idle_start_establishes_host_attenuation(self):
        args = Namespace(
            delta_table=Path("deltas.json"), card="hw:0", amixer="amixer",
            tlv_write=Path(__file__), pw_dump="pw-dump", node="virtual",
            hardware_node="hardware", pcm_status=Path("status"),
            wpctl="wpctl", control=Path("control"), interval_ms=100,
            once=True,
        )
        with patch.object(sync, "load_deltas", return_value=(b"",) * 30), \
             patch.object(sync, "find_control_numid", return_value=321), \
             patch.object(sync.base, "snapshot", return_value=[]), \
             patch.object(sync.base, "extract_node_volume",
                          return_value=(0.25 ** 3, False)), \
             patch.object(sync.base, "extract_node_id", return_value=69), \
             patch.object(sync.msiir, "graph_running", return_value=False), \
             patch.object(sync, "restore_host_attenuation",
                          return_value=(-332, 451035000)) as restore:
            self.assertEqual(sync.run(args), 0)
        restore.assert_called_once_with((0.25 ** 3, False), 69, "wpctl")

    def test_load_module_accepts_extensionless_installed_helper(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            helper = Path(tmpdir) / ".local/bin/test-helper"
            helper.parent.mkdir(parents=True)
            helper.write_text("answer = 42\n")
            with patch.object(sync, "ROOT", None), \
                 patch.object(sync.Path, "home", return_value=Path(tmpdir)):
                module = sync.load_module(
                    "unused.py", "test-helper", "test_extensionless_helper"
                )
        self.assertEqual(module.answer, 42)


if __name__ == "__main__":
    unittest.main()

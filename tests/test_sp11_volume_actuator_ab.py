import math
import struct
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from tools import sp11_volume_actuator_ab as ab


class VolumeActuatorABTests(unittest.TestCase):
    def test_q28_reference_values_track_windows_endpoint_db(self):
        self.assertEqual(ab.endpoint_q28_from_db(0.0), 0x10000000)
        for db in (-46.5, -30.25, -20.7474098205566):
            with self.subTest(db=db):
                q = ab.endpoint_q28_from_db(db)
                got = 20 * math.log10(q / ab.Q28_ONE)
                self.assertLess(abs(got - db), 1e-5)

    def test_dsp_apply_safe_order_and_no_double_attenuation(self):
        calls = []
        with patch.object(ab.base, "write_postgain_request",
                          side_effect=lambda p, v: calls.append(("postgain", v))), \
             patch.object(ab, "write_final_q28",
                          side_effect=lambda q: calls.append(("dsp", q))), \
             patch.object(ab.base, "set_hardware_volume",
                          side_effect=lambda node, scalar, wpctl="wpctl":
                          calls.append(("host", scalar))):
            sig = ab.dsp_apply_state(
                (0.25 ** 3, False), 69, Path("ctl"), False, None
            )
        self.assertIsNotNone(sig)
        self.assertEqual(calls[0], ("postgain", -332))
        self.assertEqual(calls[1][0], "dsp")
        self.assertEqual(calls[2], ("host", 1.0))
        self.assertEqual(
            calls[1][1], ab.endpoint_q28_from_db(-20.7474098205566)
        )

    def test_dsp_apply_repeated_state_suppressed(self):
        calls = []
        with patch.object(ab.base, "write_postgain_request",
                          side_effect=lambda *a: calls.append("pg")), \
             patch.object(ab, "write_final_q28",
                          side_effect=lambda *a: calls.append("dsp")), \
             patch.object(ab.base, "set_hardware_volume",
                          side_effect=lambda *a: calls.append("host")):
            sig = ab.dsp_apply_state(
                (0.25 ** 3, False), 69, Path("ctl"), False, None
            )
            sig2 = ab.dsp_apply_state(
                (0.25 ** 3, False), 69, Path("ctl"), False, sig
            )
        self.assertEqual(sig2, sig)
        self.assertEqual(calls, ["pg", "dsp", "host"])

    def test_restore_host_actuator_safe_order(self):
        calls = []
        snap = [
            {"id": 38, "info": {
                "props": {"node.name": ab.base.DEFAULT_NODE},
                "params": {"Props": [{
                    "channelVolumes": [0.25 ** 3, 0.25 ** 3], "mute": False
                }]},
            }},
            {"id": 69, "info": {
                "props": {"node.name": ab.base.DEFAULT_HARDWARE_NODE},
                "params": {"Props": [{
                    "channelVolumes": [1.0, 1.0], "mute": False
                }]},
            }},
        ]
        args = SimpleNamespace(
            pw_dump="pw-dump", node=ab.base.DEFAULT_NODE,
            hardware_node=ab.base.DEFAULT_HARDWARE_NODE,
            control=Path("ctl"), dry_run=False, wpctl="wpctl",
        )
        with patch.object(ab.base, "snapshot", return_value=snap), \
             patch.object(ab.base, "write_postgain_request",
                          side_effect=lambda p, v: calls.append(("postgain", v))), \
             patch.object(ab.base, "set_hardware_volume",
                          side_effect=lambda node, scalar, wpctl="wpctl":
                          calls.append(("host", scalar))), \
             patch.object(ab, "write_final_q28",
                          side_effect=lambda q: calls.append(("dsp", q))):
            self.assertEqual(ab.restore_host_actuator(args), 0)
        self.assertEqual(calls[0], ("postgain", -332))
        self.assertEqual(calls[1][0], "host")
        self.assertTrue(math.isclose(
            calls[1][1], 0.451034576472, abs_tol=1e-10
        ))
        self.assertEqual(calls[2], ("dsp", ab.Q28_ONE))

    def test_write_final_q28_tlv_bytes(self):
        class CP:
            returncode = 0
            stdout = "ok"
            stderr = ""

        seen = []
        with tempfile.TemporaryDirectory() as tmpdir:
            helper = Path(tmpdir) / "tlv_write"
            helper.write_text("x")
            with patch.object(ab, "find_control_numid", return_value=321), \
                 patch.object(
                     ab.subprocess, "run",
                     side_effect=lambda argv, **kw: seen.append(argv) or CP()
                 ):
                ab.write_final_q28(0x007DDA19, helper=helper)
        self.assertEqual(seen, [[
            str(helper), "hw:0", "321", struct.pack("<I", 0x007DDA19).hex()
        ]])

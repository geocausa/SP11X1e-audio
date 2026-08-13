import struct
import unittest

from tools.acdb_gainstep_delta_inventory import EXPECTED_PARAMS, extract_gainstep_delta
from tools.acdb_protection_stage_builder import (
    CHANNEL_COUNT_KEY_ID,
    DEVICE_CHANNEL_COUNT_KEY_ID,
    RX_DEVICE_KEY_ID,
    SAMPLE_RATE_KEY_ID,
    SPEAKER_SUBGRAPH_ID,
    SPEAKER_VOLUME_STEP_KEY_ID,
)


class GainStepDeltaInventoryTests(unittest.TestCase):
    def test_expected_delta_is_complete_msiir_group(self):
        self.assertEqual(
            EXPECTED_PARAMS,
            (
                (0x489E, 0x08001020),
                (0x489E, 0x08001021),
                (0x489E, 0x08001022),
                (0x489E, 0x08001026),
            ),
        )

    def test_extract_selects_only_group_containing_gainstep_key(self):
        # Minimal synthetic resolver output is injected by monkey-patching the
        # imported resolver so the grouping/delta-key logic is tested without
        # requiring the proprietary ACDB fixture in the repository.
        import tools.acdb_gainstep_delta_inventory as mod

        old = mod.resolve_subgraph_calibration
        try:
            def frame(iid, pid, payload):
                body = struct.pack("<IIII", iid, pid, len(payload), 0) + payload
                return body.ljust((len(body) + 7) & ~7, b"\0")

            prefix = [frame(0x48A1, 0x08001020 + i, bytes([i]) * 4) for i in range(4)]
            target = [
                frame(0x489E, 0x08001020, b"A" * 28),
                frame(0x489E, 0x08001021, b"B" * 16),
                frame(0x489E, 0x08001022, b"C" * 96),
                frame(0x489E, 0x08001026, b"D" * 4),
            ]
            suffix = [frame(0x5000, 0x08000001, b"Z" * 4)]
            params = []
            for iid, pid, payload_size in [
                *[(0x48A1, 0x08001020 + i, 4) for i in range(4)],
                (0x489E, 0x08001020, 28),
                (0x489E, 0x08001021, 16),
                (0x489E, 0x08001022, 96),
                (0x489E, 0x08001026, 4),
                (0x5000, 0x08000001, 4),
            ]:
                params.append({
                    "iid": f"0x{iid:08x}",
                    "param_id": f"0x{pid:08x}",
                    "payload_size": payload_size,
                    "payload_sha256": "00",
                    "pool_offset": "0x00000000",
                })
            def fake(_chunks, sg, ckv):
                self.assertEqual(sg, SPEAKER_SUBGRAPH_ID)
                self.assertEqual(ckv[SPEAKER_VOLUME_STEP_KEY_ID], 2)
                return b"".join(prefix + target + suffix), {
                    "parameters": params,
                    "groups": [
                        {"selection": "runtime-ckv", "parameter_count": 4,
                         "keys": [{"key_id": f"0x{SAMPLE_RATE_KEY_ID:08x}", "value": 48000}]},
                        {"selection": "runtime-ckv", "parameter_count": 4,
                         "keys": [{"key_id": f"0x{SPEAKER_VOLUME_STEP_KEY_ID:08x}", "value": 2}]},
                        {"selection": "default-remainder", "parameter_count": 1},
                    ],
                }
            mod.resolve_subgraph_calibration = fake
            blob, meta = extract_gainstep_delta({}, 2)
            self.assertEqual(blob, b"".join(target))
            self.assertEqual(meta["serialized_size"], 216)
            self.assertEqual(meta["parameter_count"], 4)
        finally:
            mod.resolve_subgraph_calibration = old


if __name__ == "__main__":
    unittest.main()

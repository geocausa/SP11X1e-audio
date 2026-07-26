import unittest
from datetime import timezone
from pathlib import Path
from uuid import UUID

from tools.windows_audio_etl_inventory import (
    canonical_provider_id,
    parse_probe,
)


ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / "artifacts" / "raw" / "windows-target-20260726"


class WindowsAudioEtlInventoryTests(unittest.TestCase):
    def test_event_header_guid_byte_order_is_corrected(self):
        EventHeader = type("EventHeader", (), {})
        header = EventHeader()
        header.provider_id = UUID(
            "bed34bae-6ff3-b645-8d21-bdd6fb832853"
        )
        self.assertEqual(
            canonical_provider_id(header),
            "ae4bd3be-f36f-45b6-8d21-bdd6fb832853",
        )

    def test_rejected_44100_probe_is_not_reported_as_started(self):
        probe = parse_probe(
            RAW
            / "extra-capture"
            / "scenario15_44100hz_resampling.txt"
        )
        self.assertEqual(probe["timestamp"].utcoffset().total_seconds(), 3600)
        self.assertEqual(
            probe["timestamp"].astimezone(timezone.utc).hour, 10
        )
        self.assertFalse(probe["format_supported"])
        self.assertFalse(probe["initialized"])
        self.assertFalse(probe["started"])
        self.assertFalse(probe["completed"])
        self.assertEqual(probe["frames_written"], 0)

    def test_shared_raw_probe_is_complete(self):
        probe = parse_probe(
            RAW
            / "SP11-PARITY-OUTPUT"
            / "scenario20_shared_raw.txt"
        )
        self.assertTrue(probe["raw"])
        self.assertTrue(probe["format_supported"])
        self.assertTrue(probe["completed"])
        self.assertEqual(probe["frames_written"], 1_440_000)


if __name__ == "__main__":
    unittest.main()

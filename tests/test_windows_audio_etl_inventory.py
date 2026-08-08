import tempfile
import unittest
from datetime import timezone
from pathlib import Path
from uuid import UUID

from tools.windows_audio_etl_inventory import (
    canonical_provider_id,
    parse_probe,
)


class WindowsAudioEtlInventoryTests(unittest.TestCase):
    def _probe(self, body: str):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "probe.txt"
            path.write_text(body, encoding="utf-8")
            return parse_probe(path)

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
        probe = self._probe(
            """Timestamp=2026-07-26T11:00:00+01:00
Seconds=30
Mode=shared
Rate=44100
Channels=2
Format=S16_LE
Raw=False
IsFormatSupportedHr=0x88890008
InitializeHr=0x88890008
Started=0
Stopped=0
LoopTimedOut=False
FramesWritten=0
"""
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
        probe = self._probe(
            """Timestamp=2026-07-26T12:00:00+01:00
Seconds=30
Mode=shared
Rate=48000
Channels=2
Format=FLOAT32
Raw=True
IsFormatSupportedHr=0x00000000
SetClientPropertiesRawHr=0x00000000
InitializeHr=0x00000000
Started=1
Stopped=1
LoopTimedOut=False
FramesWritten=1440000
"""
        )
        self.assertTrue(probe["raw"])
        self.assertTrue(probe["format_supported"])
        self.assertTrue(probe["completed"])
        self.assertEqual(probe["frames_written"], 1_440_000)


if __name__ == "__main__":
    unittest.main()

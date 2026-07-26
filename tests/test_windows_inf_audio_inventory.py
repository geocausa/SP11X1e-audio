import unittest

from tools.windows_inf_audio_inventory import (
    registry_entries,
    speaker_inventory,
)


class WindowsInfAudioInventoryTests(unittest.TestCase):
    def test_speaker_modes_and_formats_are_decoded(self):
        entries = registry_entries(
            "\n".join(
                [
                    'HKR,QCAUD\\WaveSpeaker\\FormatsAndModes1,type,,"host"',
                    "HKR,QCAUD\\WaveSpeaker\\FormatsAndModes1"
                    "\\ModeAndDefaultFormat0,mode,,\"DEFAULT\"",
                    "HKR,QCAUD\\WaveSpeaker\\FormatsAndModes1"
                    "\\ModeAndDefaultFormat0\\WaveFormat0,nChannels,"
                    "0x00010001,0x00000002",
                    "HKR,QCAUD\\WaveSpeaker\\FormatsAndModes1"
                    "\\ModeAndDefaultFormat0\\WaveFormat0,nBitsPerSample,"
                    "0x00010001,0x00000010",
                    "HKR,QCAUD\\WaveSpeaker\\FormatsAndModes1"
                    "\\ModeAndDefaultFormat0\\WaveFormat0,dwChannelMask,"
                    "0x00010001,0x00000003",
                ]
            )
        )

        groups = speaker_inventory(entries)

        self.assertEqual(groups[0]["type"], "host")
        self.assertEqual(groups[0]["modes"][0]["name"], "DEFAULT")
        audio_format = groups[0]["modes"][0]["formats"][0]
        self.assertEqual(audio_format["channels"], 2)
        self.assertEqual(audio_format["bits_per_sample"], 16)
        self.assertEqual(audio_format["channel_mask"], "0x00000003")

    def test_inline_inf_comment_is_removed_from_numeric_value(self):
        entries = registry_entries(
            "HKR,ADCM\\SpkrProtVIInfo,\"SamplesPerSecond\","
            "%REG_DWORD%,0x1F40 ; 8 kHz"
        )

        self.assertEqual(entries[0]["value"], 8000)


if __name__ == "__main__":
    unittest.main()

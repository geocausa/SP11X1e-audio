import struct
import unittest

from tools.sp11_protection_readback import (
    decode_cps_stats,
    decode_speaker_condition,
    decode_thermal_stats,
    decode_tmax_xmax,
)


class ProtectionReadbackTests(unittest.TestCase):
    def test_decodes_two_speaker_tmax_xmax(self):
        body = struct.pack("<IiIiIiIiI", 2, 1 << 26, 3, 42 << 22, 4,
                           1 << 25, 5, 37 << 22, 6)
        result = decode_tmax_xmax(body)
        self.assertEqual(result["speakers"][0]["max_excursion_mm"], 0.5)
        self.assertEqual(result["speakers"][1]["max_temperature_c"], 37.0)

    def test_decodes_public_spvi_conditions(self):
        result = decode_speaker_condition(struct.pack("<Iii", 2, 0, 2))
        self.assertEqual(result["speakers"][0]["condition_name"], "ok")
        self.assertEqual(result["speakers"][1]["condition_name"], "open")

    def test_decodes_cps_q_formats_per_speaker(self):
        channel = [4 << 24] * 10 + [35 << 20] * 10 + [-(1 << 24)] * 10
        result = decode_cps_stats(struct.pack("<III60i", 10, 7, 2,
                                               *(channel + channel)))
        self.assertEqual(result["speakers"][0]["battery_v"][0], 4.0)
        self.assertEqual(result["speakers"][1]["die_temperature_c"][0], 35.0)
        self.assertEqual(result["speakers"][1]["cps_gain_db"][0], -1.0)

    def test_decodes_thermal_q_formats_per_speaker(self):
        channel = [4 << 24] * 10 + [40 << 22] * 10 + [-(2 << 23)] * 10 + [80 << 22]
        result = decode_thermal_stats(struct.pack("<IIiIiI62i", 10, 8, 1, 9,
                                                   1, 2, *(channel + channel)))
        self.assertTrue(result["valid"])
        self.assertEqual(result["speakers"][0]["coil_resistance_ohm"][0], 4.0)
        self.assertEqual(result["speakers"][1]["target_temperature_c"], 80.0)


if __name__ == "__main__":
    unittest.main()

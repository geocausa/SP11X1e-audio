import unittest

from tools.sp11_cps_transport_model import build_model

class Sp11CpsTransportModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model = build_model()

    def test_one_shared_master_port_13(self):
        self.assertEqual(
            self.model["deduped_master_runtime_ports"],
            [{"port": 13, "channel_mask": 0x03}],
        )

    def test_exact_left_right_windows_register_bytes(self):
        expected = self.model["windows_expected"]
        for slave in self.model["slaves"]:
            got = slave["derived_registers"]
            for name, value in expected[slave["name"]].items():
                self.assertEqual(got[name], value, (slave["name"], name))
            self.assertEqual(got["sample_interval_clocks"], 800)

    def test_no_unobserved_extended_registers(self):
        self.assertEqual(
            self.model["registers_intentionally_not_programmed"],
            ["OffsetCtrl2", "BlockCtrl2"],
        )

if __name__ == "__main__":
    unittest.main()

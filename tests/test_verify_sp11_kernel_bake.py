import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.verify_sp11_kernel_bake import verify_dtb, verify_source


class VerifySp11KernelBakeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.source = Path(self.temporary.name)
        driver = self.source / "drivers/net/wireless/ath/ath12k/core.c"
        dtsi = self.source / "arch/arm64/boot/dts/qcom/x1-microsoft-denali.dtsi"
        driver.parent.mkdir(parents=True)
        dtsi.parent.mkdir(parents=True)
        driver.write_text(
            'if (of_property_read_bool(ab->dev->of_node, "disable-rfkill"))\n'
            "\treturn 0;\n",
            encoding="utf-8",
        )
        dtsi.write_text(
            "&pcie4_port0 {\n\twifi@0 {\n\t\tdisable-rfkill;\n\t};\n};\n",
            encoding="utf-8",
        )

    def tearDown(self):
        self.temporary.cleanup()

    def test_accepts_both_required_source_halves(self):
        self.assertEqual(verify_source(self.source), {
            "ath12k_dt_disable_rfkill_hook": True,
            "sp11_wcn7850_disable_rfkill_property": True,
        })

    def test_rejects_missing_driver_hook(self):
        driver = self.source / "drivers/net/wireless/ath/ath12k/core.c"
        driver.write_text("static int ath12k_core_rfkill_config(void) {}\n",
                          encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "ath12k does not honor"):
            verify_source(self.source)

    def test_checks_compiled_dtb_property_with_fdtget(self):
        dtb = self.source / "final.dtb"
        dtb.write_bytes(b"fixture")
        completed = subprocess.CompletedProcess([], 0, "", "")
        with patch("tools.verify_sp11_kernel_bake.subprocess.run",
                   return_value=completed) as run:
            self.assertEqual(verify_dtb(dtb), {
                "compiled_dtb_disable_rfkill_property": True,
            })
        self.assertEqual(run.call_args.args[0][-2:], [
            "/soc@0/pci@1c08000/pcie@0/wifi@0", "disable-rfkill",
        ])

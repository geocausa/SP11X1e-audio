import hashlib
import unittest
from unittest.mock import patch

from tools import sp11_windows_volume_transaction as tx


class WindowsVolumeTransactionTests(unittest.TestCase):
    def fake_delta(self, _chunks, step):
        coeff = tx.msiir.COEFF_PAYLOADS[step - 1]
        # Preserve the real Windows delta size class for transaction tests.
        size = 272 if len(coeff) == 152 else 216
        blob = bytes([step]) * size
        return blob, {
            "parameters": [{
                "param_id": "0x08001022",
                "payload_sha256": hashlib.sha256(coeff).hexdigest(),
            }]
        }

    def test_25_percent_matches_pinned_windows_state(self):
        with patch.object(tx, "extract_gainstep_delta", side_effect=self.fake_delta):
            row = tx.plan_for_ui_scalar({}, 0.25)
        self.assertAlmostEqual(row["endpoint_db"], -20.7474098205566, places=9)
        self.assertEqual(row["postgain_1_16_db"], -332)
        self.assertEqual(row["gainstep"]["step"], 3)
        self.assertEqual(row["gainstep"]["delta_size"], 272)
        self.assertEqual(row["final_vol_ctrl"]["payload_size"], 104)
        self.assertEqual(row["final_vol_ctrl"]["qcadcm_quarter_db_index"], 82)
        self.assertEqual(row["final_vol_ctrl"]["qcadcm_effective_db"], -20.5)

    def test_windows_operation_order_is_explicit(self):
        with patch.object(tx, "extract_gainstep_delta", side_effect=self.fake_delta):
            row = tx.plan_for_ui_scalar({}, 0.40)
        self.assertEqual(row["generation_configuration"], ["dolby_postgain"])
        self.assertEqual(row["ordered_operations"], [
            "final_vol_ctrl_ramped_gain",
            "gainstep_oob_nonpersistent_delta",
        ])
        self.assertEqual(row["stereo_master_sequence"], [
            "left_new_right_old_then_mixed_gainstep",
            "left_new_right_new_then_final_gainstep",
        ])

    def test_large_gainstep_delta_classes(self):
        # Use exact ACDB anchor Q28s via their endpoint dB values indirectly by
        # checking the selector/table against the 30 recovered coefficient rows.
        self.assertEqual(
            {i + 1 for i, p in enumerate(tx.msiir.COEFF_PAYLOADS) if len(p) == 152},
            {3, 9, 24},
        )


if __name__ == "__main__":
    unittest.main()

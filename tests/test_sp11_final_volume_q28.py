import math
import struct
import unittest

from tools.sp11_final_volume_q28 import (
    Q28_ONE,
    multichannel_payload,
    q28_from_db,
)


class FinalVolumeQ28Tests(unittest.TestCase):
    def test_q28_reference_values_track_windows_endpoint_db(self):
        self.assertEqual(q28_from_db(0.0), Q28_ONE)
        for db in (-46.5, -30.25, -20.7474098205566):
            with self.subTest(db=db):
                q28 = q28_from_db(db)
                got = 20 * math.log10(q28 / Q28_ONE)
                self.assertLess(abs(got - db), 1e-5)

    def test_multichannel_payload_matches_windows_shape(self):
        gain = 0x007DDA19
        payload = multichannel_payload(gain)
        self.assertEqual(len(payload), 104)
        self.assertEqual(struct.unpack_from("<I", payload, 0), (8,))
        self.assertEqual(struct.unpack_from("<III", payload, 4), (2, 0, gain))
        self.assertEqual(struct.unpack_from("<III", payload, 16), (4, 0, gain))
        self.assertEqual(payload[28:], bytes(76))

    def test_multichannel_payload_rejects_above_unity(self):
        with self.assertRaisesRegex(ValueError, "outside"):
            multichannel_payload(Q28_ONE + 1)

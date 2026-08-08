import math
import tempfile
import unittest
from pathlib import Path

from tools.dolby.generate_stereo_matrix_probe import write_probe
from tools.dolby.analyze_state_pinned_oracle import read_pcm16_stereo


class DolbyMatrixProbeTests(unittest.TestCase):
    def test_probe_has_expected_three_stereo_cases(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "probe.wav"
            write_probe(path)
            rate, samples, frames = read_pcm16_stereo(path)
            self.assertEqual(rate, 48000)
            self.assertEqual(frames, 17 * rate)

            def channel_stats(start_seconds):
                start = int(start_seconds * rate)
                count = int(0.5 * rate)
                left = [samples[2 * i] / 32768.0 for i in range(start, start + count)]
                right = [samples[2 * i + 1] / 32768.0 for i in range(start, start + count)]
                l_rms = math.sqrt(sum(x * x for x in left) / count)
                r_rms = math.sqrt(sum(x * x for x in right) / count)
                dot = sum(x * y for x, y in zip(left, right))
                denom = math.sqrt(sum(x * x for x in left) * sum(y * y for y in right))
                corr = dot / denom if denom else 0.0
                return l_rms, r_rms, corr

            in_phase = channel_stats(5.25)
            left_only = channel_stats(9.25)
            anti_phase = channel_stats(13.25)

            self.assertGreater(in_phase[0], 0.17)
            self.assertAlmostEqual(in_phase[0], in_phase[1], places=6)
            self.assertGreater(in_phase[2], 0.999999)

            self.assertGreater(left_only[0], 0.17)
            self.assertEqual(left_only[1], 0.0)
            self.assertEqual(left_only[2], 0.0)

            self.assertGreater(anti_phase[0], 0.17)
            self.assertAlmostEqual(anti_phase[0], anti_phase[1], places=6)
            self.assertLess(anti_phase[2], -0.999999)


if __name__ == "__main__":
    unittest.main()

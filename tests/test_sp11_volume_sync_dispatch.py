import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

PATH = Path(__file__).resolve().parents[1]/'deploy/ubig/sp11_volume_sync_dispatch.py'
spec = importlib.util.spec_from_file_location('dispatch', PATH)
dispatch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dispatch)


class DispatcherStartup(unittest.TestCase):
    def clock(self):
        self.now = 0.0
        def sleep(seconds):
            self.now += seconds
        return patch.multiple(dispatch.time, monotonic=lambda: self.now, sleep=sleep)

    def test_late_control_does_not_latch_rollback(self):
        with self.clock(), patch.object(dispatch, 'has_control', side_effect=[False, False, True]):
            self.assertTrue(dispatch.wait_for_transaction_control(timeout=1, interval=.25))
            self.assertEqual(self.now, .5)

    def test_absent_control_has_bounded_fallback(self):
        with self.clock(), patch.object(dispatch, 'has_control', return_value=False):
            self.assertFalse(dispatch.wait_for_transaction_control(timeout=.6, interval=.25))
            self.assertEqual(self.now, .6)

    def test_ready_control_needs_no_sleep(self):
        with self.clock(), patch.object(dispatch, 'has_control', return_value=True):
            self.assertTrue(dispatch.wait_for_transaction_control())
            self.assertEqual(self.now, 0)

    def test_missing_amixer_does_not_crash_dispatch(self):
        with patch.object(dispatch.subprocess, 'run', side_effect=FileNotFoundError):
            self.assertFalse(dispatch.has_control())


if __name__ == '__main__':
    unittest.main()

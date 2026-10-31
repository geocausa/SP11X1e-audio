"""Actual event-loop regressions for backend removal/recreation and wpctl races."""
import subprocess
import unittest
from argparse import Namespace
from contextlib import ExitStack
from pathlib import Path
from unittest.mock import patch
from deploy.dolby import sp11_windows_volume_transaction_sync as dolby
from deploy.ubig import sp11_windows_volume_transaction_sync as ubig


class Monitor:
    returncode = 0
    class Output:
        def fileno(self): return 9
    stdout = Output()
    def poll(self): return 0
    def wait(self): return 0
    def terminate(self): raise AssertionError("completed monitor")


def visible(gain=.25 ** 3, node=41):
    return {'id': node, 'info': {'props': {'node.name': 'virtual'},
            'params': {'Props': [{'channelVolumes': [gain], 'mute': False}]}}}


def hardware(node=69):
    return {'id': node, 'info': {'props': {'node.name': 'hardware'}}}


class HardwareRecoveryTests(unittest.TestCase):
    def exercise(self, sync, events, snapshots, fail_apply=None):
        calls = []
        args = Namespace(delta_table=Path('unused'), card='hw:0', amixer='amixer',
                         tlv_write=Path(__file__), pw_dump='pw-dump', node='virtual',
                         hardware_node='hardware', pcm_status=Path('status'),
                         wpctl='wpctl', control=Path('control'), interval_ms=100,
                         settle_ms=0, bootstrap_ms=0, bootstrap_guard_ms=0,
                         node_settle_ms=0, once=False)
        count = 0
        def apply(state, target, *a, **kw):
            nonlocal count
            count += 1
            calls.append(('apply', target, state, kw.get('previous')))
            if count == fail_apply:
                raise subprocess.CalledProcessError(1, ['wpctl', 'set-volume', str(target)])
            _, db, _, _ = sync.base.derive_windows_state(*state)
            q28 = sync.qcadcm_q28_from_db(db)
            return q28, sync.msiir.select_ckv_step_q28(q28)
        def attenuate(state, target, _wpctl):
            calls.append(('attenuate', target, state))
            return (-332, 451035000, False)
        with ExitStack() as stack:
            for obj, attr, kwargs in [
                (sync, 'load_deltas', {'return_value': (b'',) * 30}),
                (sync, 'find_control_numid', {'return_value': 321}),
                (sync, 'find_mute_control_numid', {'return_value': None}),
                (sync, 'find_volume_only_control_numid', {'return_value': None}),
                (sync, 'find_control_values', {'return_value': 284}),
                (sync.subprocess, 'Popen', {'return_value': Monitor()}),
                (sync.base, 'snapshot', {'side_effect': snapshots}),
                (sync.base, 'iter_json_stream', {'return_value': iter(events)}),
                (sync.msiir, 'graph_running', {'return_value': True}),
                (sync, 'apply_transaction', {'side_effect': apply}),
                (sync, 'restore_host_attenuation', {'side_effect': attenuate}),
                (sync, 'restore_visible_control_state', {'side_effect': AssertionError('visible node unchanged')}),
                (sync.base, 'set_hardware_mute', {}),
                (sync, 'set_wsa_rx_volume', {}),
            ]:
                stack.enter_context(patch.object(obj, attr, **kwargs))
            queue = 'queue_ubig_postgain_for_generation' if sync is ubig else 'queue_dolby_postgain_for_generation'
            stack.enter_context(patch.object(sync, queue, return_value=-332))
            self.assertEqual(sync.run(args), 0)
        return calls

    def test_removed_backend_is_never_used_for_queued_slider_event(self):
        for sync in [dolby, ubig]:
            with self.subTest(sync=sync.__name__):
                changed = .15 ** 3
                events = [[{'id': 69, 'info': None}], [visible(changed)], [hardware(84)]]
                calls = self.exercise(sync, events,
                                      [[visible(), hardware()], [visible(changed)],
                                       [visible(changed), hardware(84)]])
                applies = [c for c in calls if c[0] == 'apply']
                self.assertEqual([c[1] for c in applies], [69, 84])
                self.assertEqual(applies[-1][2], (changed, False))
                self.assertIsNone(applies[-1][3])
                self.assertLess(calls.index(('attenuate', 84, (changed, False))), calls.index(applies[-1]))

    def test_hardware_only_replacement_resets_same_volume_transaction(self):
        for sync in [dolby, ubig]:
            with self.subTest(sync=sync.__name__):
                calls = self.exercise(sync, [[hardware(84)]],
                                      [[visible(), hardware()], [visible(), hardware(84)]])
                applies = [c for c in calls if c[0] == 'apply']
                self.assertEqual([c[1] for c in applies], [69, 84])
                self.assertIsNone(applies[-1][3])
                self.assertTrue(any(c[:2] == ('attenuate', 84) for c in calls))

    def test_backend_disappears_between_lookup_and_actuation(self):
        for sync in [dolby, ubig]:
            with self.subTest(sync=sync.__name__):
                changed = .3 ** 3
                calls = self.exercise(sync, [[visible(changed)], [hardware(84)]],
                                      [[visible(), hardware()], [visible(changed), hardware(84)]], fail_apply=2)
                applies = [c for c in calls if c[0] == 'apply']
                self.assertEqual([c[1] for c in applies], [69, 69, 84])
                self.assertIsNone(applies[-1][3])

    def test_initial_actuator_race_retries_fresh_snapshot(self):
        for sync in [dolby, ubig]:
            with self.subTest(sync=sync.__name__):
                calls = self.exercise(sync, [],
                                      [[visible(), hardware()], [visible(), hardware(84)]], fail_apply=1)
                self.assertEqual([c[1] for c in calls if c[0] == 'apply'], [69, 84])

    def test_removal_without_replacement_does_not_command_old_backend_on_exit(self):
        for sync in [dolby, ubig]:
            with self.subTest(sync=sync.__name__):
                calls = self.exercise(sync, [[{'id': 69, 'info': None}]], [[visible(), hardware()]])
                self.assertEqual([c[1] for c in calls if c[0] == 'apply'], [69])
                self.assertFalse(any(c[0] == 'attenuate' for c in calls))

    def test_unrelated_removal_is_not_a_backend_reset(self):
        for sync in [dolby, ubig]:
            with self.subTest(sync=sync.__name__):
                calls = self.exercise(sync, [[{'id': 999, 'info': None}]], [[visible(), hardware()]])
                self.assertEqual(len([c for c in calls if c[0] == 'apply']), 1)

    def test_partial_update_is_not_a_removal(self):
        for sync in [dolby, ubig]:
            with self.subTest(sync=sync.__name__):
                self.assertFalse(sync.event_removes_node([{'id': 69}], 69))
                self.assertFalse(sync.event_removes_node([{'id': 69, 'info': {}}], 69))
                self.assertTrue(sync.event_removes_node([{'id': 69, 'info': None}], 69))


if __name__ == '__main__':
    unittest.main()

import hashlib
import json
import tempfile
import unittest
from argparse import Namespace
from pathlib import Path
from unittest.mock import patch

from deploy.dolby import sp11_volume_sync_dispatch as dispatch
from deploy.dolby import sp11_windows_volume_transaction_sync as sync


class WindowsVolumeTransactionSyncTests(unittest.TestCase):
    def test_qcadcm_q28_quantizes_to_quarter_db_table(self):
        self.assertEqual(
            sync.qcadcm_q28_from_db(-20.7474098205566),
            round((10.0 ** (-20.5 / 20.0)) * sync.Q28_ONE),
        )

    def test_delta_table_requires_exact_thirty_size_classes(self):
        rows = []
        for step in range(1, 31):
            size = 272 if step in (3, 9, 24) else 216
            blob = b"x" * size
            rows.append({
                "gain_step": step,
                "serialized_hex": blob.hex(),
                "serialized_sha256": hashlib.sha256(blob).hexdigest(),
            })
        with tempfile.TemporaryDirectory() as tmpdir:
            table = Path(tmpdir) / "deltas.json"
            table.write_text(json.dumps({
                "source_sha256": sync.EXPECTED_ACDB_SHA256,
                "steps": rows,
            }))
            deltas = sync.load_deltas(table)
        self.assertEqual(len(deltas), 30)
        self.assertEqual(len(deltas[1]), 216)
        self.assertEqual(len(deltas[2]), 272)

    def test_apply_order_is_transaction_then_host_unity_without_live_postgain(self):
        calls = []
        deltas = tuple(b"d" * (272 if step in (3, 9, 24) else 216)
                       for step in range(1, 31))
        with patch.object(sync.base, "write_postgain_request") as postgain, \
             patch.object(sync, "write_transaction",
                          side_effect=lambda q28, delta, **kwargs:
                          calls.append(("transaction", q28, len(delta)))), \
             patch.object(sync.base, "set_hardware_volume",
                          side_effect=lambda node, scalar, wpctl:
                          calls.append(("host", scalar))):
            signature = sync.apply_transaction(
                (0.25 ** 3, False), 69, deltas,
                Path("tlv_write"), "hw:0", 321, "wpctl"
            )
        postgain.assert_not_called()
        self.assertEqual(calls[0][0], "transaction")
        self.assertEqual(calls[0][2], 272)
        self.assertEqual(calls[1], ("host", 1.0))
        expected_q28 = sync.qcadcm_q28_from_db(-20.7474098205566)
        self.assertEqual(signature, (expected_q28, 3))

    def test_live_followup_does_not_rewrite_hidden_sink_unity(self):
        deltas = tuple(b"d" * (272 if step in (3, 9, 24) else 216)
                       for step in range(1, 31))
        q8 = sync.qcadcm_q28_from_db(-37.183)
        with patch.object(sync, "write_transaction"), \
             patch.object(sync.base, "set_hardware_volume") as host:
            result = sync.apply_transaction(
                (0.17 ** 3, False), 69, deltas, Path("tlv"), "hw:0", 33,
                "wpctl", previous=(q8, 1), stereo_sequence=True,
            )
        host.assert_not_called()
        self.assertEqual(result[1], 2)

    def test_exact_volume_only_payload_is_two_q28_words(self):
        calls = []

        class CP:
            returncode = 0
            stdout = ""
            stderr = ""

        with patch.object(
            sync.subprocess, "run",
            side_effect=lambda argv, **kwargs: calls.append(argv) or CP(),
        ):
            sync.write_volume_only(
                0x0039DB88, 0x00C7763F, helper=Path("tlv_write"),
                card="hw:0", numid=35,
            )

        self.assertEqual(calls[0][-1], "88db39003f76c700")

    def test_wsa_rx_windows_gain_sets_both_named_controls(self):
        calls = []

        class CP:
            returncode = 0
            stdout = ""
            stderr = ""

        with patch.object(
            sync.subprocess, "run",
            side_effect=lambda argv, **kwargs: calls.append(argv) or CP(),
        ):
            sync.set_wsa_rx_volume(84, card="hw:0", amixer="amixer")

        self.assertEqual(len(calls), 2)
        self.assertEqual(calls[0][-2:], ["name='WSA WSA_RX0 Digital Volume'", "84"])
        self.assertEqual(calls[1][-2:], ["name='WSA WSA_RX1 Digital Volume'", "84"])

    def test_wsa_rx_gain_rejects_values_above_windows_control_max(self):
        with self.assertRaises(ValueError):
            sync.set_wsa_rx_volume(85)

    def test_exact_endpoint_mute_selector_is_one_u32(self):
        calls = []

        class CP:
            returncode = 0
            stdout = ""
            stderr = ""

        with patch.object(
            sync.subprocess, "run",
            side_effect=lambda argv, **kwargs: calls.append(argv) or CP(),
        ):
            sync.write_endpoint_mute(
                True, helper=Path("tlv_write"), card="hw:0", numid=77
            )
            sync.write_endpoint_mute(
                False, helper=Path("tlv_write"), card="hw:0", numid=77
            )

        self.assertEqual(calls[0][-1], "01000000")
        self.assertEqual(calls[1][-1], "00000000")

    def test_named_mute_control_discovery(self):
        class CP:
            returncode = 0
            stdout = (
                "numid=33,iface=MIXER,name='SP11 Windows Volume Transaction'\n"
                "numid=34,iface=MIXER,name='SP11 Windows Endpoint Mute'\n"
                "numid=35,iface=MIXER,name='SP11 Windows Volume Only'\n"
            )

        with patch.object(sync.subprocess, "run", return_value=CP()):
            self.assertEqual(sync.find_control_numid(), 33)
            self.assertEqual(sync.find_mute_control_numid(), 34)
            self.assertEqual(sync.find_volume_only_control_numid(), 35)

    def test_postgain_is_queued_once_per_dolby_generation(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            generation = Path(tmpdir) / "generation"
            control = Path(tmpdir) / "control"
            with patch.object(sync.base, "write_postgain_request") as write:
                first = sync.queue_dolby_postgain_for_generation(
                    (0.25 ** 3, False), 41, control, generation
                )
                same = sync.queue_dolby_postgain_for_generation(
                    (0.50 ** 3, False), 41, control, generation
                )
                replacement = sync.queue_dolby_postgain_for_generation(
                    (0.50 ** 3, False), 42, control, generation
                )
            self.assertEqual(first, -332)
            self.assertIsNone(same)
            self.assertNotEqual(replacement, -332)
            self.assertEqual(write.call_count, 2)
            self.assertEqual(generation.read_text().strip(), "42")


    def test_postgain_queue_forwards_ubig_control_format(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            generation = Path(tmpdir) / "generation"
            control = Path(tmpdir) / "control"
            with patch.object(sync.base, "write_postgain_request") as write:
                sync.queue_dolby_postgain_for_generation(
                    (0.25 ** 3, False), 41, control, generation, sync.base.CONTROL_FORMAT_UBIG_V2
                )
            write.assert_called_once_with(control, -332, sync.base.CONTROL_FORMAT_UBIG_V2)


    def test_control_capacity_detects_stereo_transaction_extension(self):
        class CP:
            returncode = 0
            stdout = "numid=33,iface=MIXER,name='SP11 Windows Volume Transaction'\n  ; type=BYTES,access=-----RW-,values=288\n"
        with patch.object(sync.subprocess, "run", return_value=CP()):
            self.assertEqual(sync.find_control_values("hw:0", 33), 288)

    def test_windows_lr_sequence_matches_kdnet_upward_transition(self):
        q8 = sync.qcadcm_q28_from_db(-37.183)
        q17 = sync.qcadcm_q28_from_db(-26.409)
        deltas = tuple(bytes([step]) * (272 if step in (3, 9, 24) else 216)
                       for step in range(1, 31))
        calls = []
        with patch.object(sync, "write_transaction",
                          side_effect=lambda left, delta, **kw:
                          calls.append((left, kw.get("right_q28"), delta[0]))), \
             patch.object(sync.base, "set_hardware_volume"), \
             patch.object(sync.base, "set_hardware_mute"):
            result = sync.apply_transaction(
                (0.17 ** 3, False), 69, deltas, Path("tlv"), "hw:0", 33,
                "wpctl", previous=(q8, 1), stereo_sequence=True,
            )
        self.assertEqual(result, (q17, 2))
        self.assertEqual(calls, [(q17, q8, 2), (q17, q17, 2)])

    def test_windows_lr_sequence_matches_kdnet_downward_transition(self):
        q8 = sync.qcadcm_q28_from_db(-37.183)
        q17 = sync.qcadcm_q28_from_db(-26.409)
        deltas = tuple(bytes([step]) * (272 if step in (3, 9, 24) else 216)
                       for step in range(1, 31))
        calls = []
        with patch.object(sync, "write_transaction",
                          side_effect=lambda left, delta, **kw:
                          calls.append((left, kw.get("right_q28"), delta[0]))), \
             patch.object(sync.base, "set_hardware_volume"), \
             patch.object(sync.base, "set_hardware_mute"):
            result = sync.apply_transaction(
                (0.08 ** 3, False), 69, deltas, Path("tlv"), "hw:0", 33,
                "wpctl", previous=(q17, 2), stereo_sequence=True,
            )
        self.assertEqual(result, (q8, 1))
        self.assertEqual(calls, [(q8, q17, 2), (q8, q8, 1)])

    def test_prior_new_ckv_same_row_uses_volume_only_twice(self):
        q8 = sync.qcadcm_q28_from_db(-37.183)
        deltas = tuple(bytes([step]) * (272 if step in (3, 9, 24) else 216)
                       for step in range(1, 31))
        calls = []
        with patch.object(sync, "write_transaction") as combined, \
             patch.object(sync, "write_volume_only",
                          side_effect=lambda left, right, **kw:
                          calls.append((left, right, kw["numid"]))), \
             patch.object(sync.base, "set_hardware_volume") as host:
            result = sync.apply_transaction(
                (0.10 ** 3, False), 69, deltas, Path("tlv"), "hw:0", 33,
                "wpctl", previous=(q8, 1), stereo_sequence=True,
                volume_only_numid=35,
            )
        q10 = result[0]
        self.assertEqual(result[1], 1)
        self.assertEqual(calls, [(q10, q8, 35), (q10, q10, 35)])
        combined.assert_not_called()
        host.assert_not_called()

    def test_prior_new_ckv_up_boundary_calibrates_first_call_only(self):
        q8 = sync.qcadcm_q28_from_db(-37.183)
        deltas = tuple(bytes([step]) * (272 if step in (3, 9, 24) else 216)
                       for step in range(1, 31))
        calls = []
        with patch.object(sync, "write_transaction",
                          side_effect=lambda left, delta, **kw:
                          calls.append(("cal", left, kw.get("right_q28"), delta[0]))), \
             patch.object(sync, "write_volume_only",
                          side_effect=lambda left, right, **kw:
                          calls.append(("vol", left, right, None))), \
             patch.object(sync.base, "set_hardware_volume"):
            result = sync.apply_transaction(
                (0.17 ** 3, False), 69, deltas, Path("tlv"), "hw:0", 33,
                "wpctl", previous=(q8, 1), stereo_sequence=True,
                volume_only_numid=35,
            )
        q17 = result[0]
        self.assertEqual(result[1], 2)
        self.assertEqual(calls, [
            ("cal", q17, q8, 2),
            ("vol", q17, q17, None),
        ])

    def test_prior_new_ckv_down_boundary_calibrates_second_call_only(self):
        q17 = sync.qcadcm_q28_from_db(-26.409)
        deltas = tuple(bytes([step]) * (272 if step in (3, 9, 24) else 216)
                       for step in range(1, 31))
        calls = []
        with patch.object(sync, "write_transaction",
                          side_effect=lambda left, delta, **kw:
                          calls.append(("cal", left, kw.get("right_q28"), delta[0]))), \
             patch.object(sync, "write_volume_only",
                          side_effect=lambda left, right, **kw:
                          calls.append(("vol", left, right, None))), \
             patch.object(sync.base, "set_hardware_volume"):
            result = sync.apply_transaction(
                (0.08 ** 3, False), 69, deltas, Path("tlv"), "hw:0", 33,
                "wpctl", previous=(q17, 2), stereo_sequence=True,
                volume_only_numid=35,
            )
        q8 = result[0]
        self.assertEqual(result[1], 1)
        self.assertEqual(calls, [
            ("vol", q8, q17, None),
            ("cal", q8, q8, 1),
        ])

    def test_prior_new_ckv_initial_handover_establishes_one_combined_row(self):
        deltas = tuple(bytes([step]) * (272 if step in (3, 9, 24) else 216)
                       for step in range(1, 31))
        calls = []
        with patch.object(sync, "write_transaction",
                          side_effect=lambda left, delta, **kw:
                          calls.append((left, kw.get("right_q28"), delta[0]))), \
             patch.object(sync, "write_volume_only") as volume_only, \
             patch.object(sync.base, "set_hardware_volume") as host:
            result = sync.apply_transaction(
                (0.17 ** 3, False), 69, deltas, Path("tlv"), "hw:0", 33,
                "wpctl", previous=None, stereo_sequence=True,
                volume_only_numid=35,
            )
        self.assertEqual(calls, [(result[0], result[0], 2)])
        volume_only.assert_not_called()
        host.assert_called_once_with(69, 1.0, "wpctl")

    def test_v31_applies_rx84_once_after_first_active_handover(self):
        class FakeStdout:
            def fileno(self):
                return 9

        class FakeProc:
            stdout = FakeStdout()
            returncode = 0

            def poll(self):
                return 0

            def terminate(self):
                raise AssertionError("completed fake monitor must not be terminated")

            def wait(self):
                return 0

        initial = [{
            "id": 41,
            "info": {
                "props": {"node.name": "virtual"},
                "params": {"Props": [{"channelVolumes": [0.25 ** 3]}]},
            },
        }, {
            "id": 69,
            "info": {"props": {"node.name": "hardware"}},
        }]
        changed = [{
            "id": 41,
            "info": {
                "props": {"node.name": "virtual"},
                "params": {"Props": [{"channelVolumes": [0.50 ** 3]}]},
            },
        }]
        args = Namespace(
            delta_table=Path("deltas.json"), card="hw:0", amixer="amixer",
            tlv_write=Path(__file__), pw_dump="pw-dump", node="virtual",
            hardware_node="hardware", pcm_status=Path("status"),
            wpctl="wpctl", control=Path("control"), interval_ms=100,
            settle_ms=0, bootstrap_ms=0, bootstrap_guard_ms=0,
            node_settle_ms=0, once=False,
        )
        rx_calls = []
        applied = []

        def control_values(_card, numid, _amixer="amixer"):
            return 288 if numid == 321 else 16

        with patch.object(sync, "load_deltas", return_value=(b"",) * 30), \
             patch.object(sync, "find_control_numid", return_value=321), \
             patch.object(sync, "find_mute_control_numid", return_value=None), \
             patch.object(sync, "find_volume_only_control_numid", return_value=323), \
             patch.object(sync, "find_control_values", side_effect=control_values), \
             patch.object(sync.subprocess, "Popen", return_value=FakeProc()), \
             patch.object(sync.base, "snapshot", return_value=initial), \
             patch.object(sync.base, "iter_json_stream", return_value=iter((changed,))), \
             patch.object(sync, "queue_dolby_postgain_for_generation", return_value=-332), \
             patch.object(sync.msiir, "graph_running", return_value=True), \
             patch.object(sync, "apply_transaction",
                          side_effect=lambda state, *a, **k:
                          applied.append(state) or (123, 3)), \
             patch.object(sync, "set_wsa_rx_volume",
                          side_effect=lambda value, **kw: rx_calls.append(value)), \
             patch.object(sync, "restore_host_attenuation"), \
             patch.object(sync.base, "set_hardware_mute"):
            self.assertEqual(sync.run(args), 0)

        self.assertEqual(applied, [(0.25 ** 3, False), (0.50 ** 3, False)])
        # One 84 after first active handover, no reapply on ordinary volume,
        # then 81 when the candidate service exits.
        self.assertEqual(rx_calls, [84, 81])

    def test_legacy_kernel_never_touches_wsa_rx_gain_policy(self):
        args = Namespace(
            delta_table=Path("deltas.json"), card="hw:0", amixer="amixer",
            tlv_write=Path(__file__), pw_dump="pw-dump", node="virtual",
            hardware_node="hardware", pcm_status=Path("status"),
            wpctl="wpctl", control=Path("control"), interval_ms=100,
            once=True,
        )
        with patch.object(sync, "load_deltas", return_value=(b"",) * 30), \
             patch.object(sync, "find_control_numid", return_value=321), \
             patch.object(sync, "find_mute_control_numid", return_value=None), \
             patch.object(sync, "find_volume_only_control_numid", return_value=None), \
             patch.object(sync, "find_control_values", return_value=288), \
             patch.object(sync.base, "snapshot", return_value=[]), \
             patch.object(sync.base, "extract_node_volume", return_value=(0.25 ** 3, False)), \
             patch.object(sync.base, "extract_node_id", return_value=69), \
             patch.object(sync, "queue_dolby_postgain_for_generation", return_value=-332), \
             patch.object(sync.msiir, "graph_running", return_value=True), \
             patch.object(sync, "apply_transaction", return_value=(123, 3)), \
             patch.object(sync, "set_wsa_rx_volume") as set_rx, \
             patch.object(sync.base, "set_hardware_mute"):
            self.assertEqual(sync.run(args), 0)
        set_rx.assert_not_called()

    def test_dispatch_selects_candidate_only_when_control_exists(self):
        class CP:
            returncode = 0
            stdout = "numid=321,name='SP11 Windows Volume Transaction'"

        with patch.object(dispatch.subprocess, "run", return_value=CP()):
            self.assertTrue(dispatch.has_control())

    def test_idle_start_establishes_host_attenuation(self):
        args = Namespace(
            delta_table=Path("deltas.json"), card="hw:0", amixer="amixer",
            tlv_write=Path(__file__), pw_dump="pw-dump", node="virtual",
            hardware_node="hardware", pcm_status=Path("status"),
            wpctl="wpctl", control=Path("control"), interval_ms=100,
            once=True,
        )
        with patch.object(sync, "load_deltas", return_value=(b"",) * 30), \
             patch.object(sync, "find_control_numid", return_value=321), \
             patch.object(sync, "find_mute_control_numid", return_value=None), \
             patch.object(sync, "find_volume_only_control_numid", return_value=None), \
             patch.object(sync.base, "snapshot", return_value=[]), \
             patch.object(sync.base, "extract_node_volume",
                          return_value=(0.25 ** 3, False)), \
             patch.object(sync.base, "extract_node_id", return_value=69), \
             patch.object(sync.msiir, "graph_running", return_value=False), \
             patch.object(sync, "queue_dolby_postgain_for_generation",
                          return_value=-332) as queue, \
             patch.object(sync, "restore_host_attenuation",
                          return_value=(-332, 451035000)) as restore:
            self.assertEqual(sync.run(args), 0)
        queue.assert_called_once()
        restore.assert_called_once_with((0.25 ** 3, False), 69, "wpctl")

    def test_monitor_applies_complete_volume_event_without_another_snapshot(self):
        class FakeStdout:
            def fileno(self):
                return 9

        class FakeProc:
            stdout = FakeStdout()
            returncode = 0

            def poll(self):
                return 0

            def terminate(self):
                raise AssertionError("completed fake monitor must not be terminated")

            def wait(self):
                return 0

        initial = [{
            "id": 41,
            "info": {
                "props": {"node.name": "virtual"},
                "params": {"Props": [{"channelVolumes": [0.25 ** 3] }]},
            },
        }, {
            "id": 69,
            "info": {"props": {"node.name": "hardware"}},
        }]
        changed = [{
            "id": 41,
            "info": {
                "props": {"node.name": "virtual"},
                "params": {"Props": [{"channelVolumes": [0.5 ** 3] }]},
            },
        }]
        args = Namespace(
            delta_table=Path("deltas.json"), card="hw:0", amixer="amixer",
            tlv_write=Path(__file__), pw_dump="pw-dump", node="virtual",
            hardware_node="hardware", pcm_status=Path("status"),
            wpctl="wpctl", control=Path("control"), interval_ms=100,
            settle_ms=0, bootstrap_ms=0, bootstrap_guard_ms=0, once=False,
        )
        snapshots = []
        applied = []
        with patch.object(sync, "load_deltas", return_value=(b"",) * 30), \
             patch.object(sync, "find_control_numid", return_value=321), \
             patch.object(sync, "find_mute_control_numid", return_value=None), \
             patch.object(sync, "find_volume_only_control_numid", return_value=None), \
             patch.object(sync, "find_control_values", return_value=284), \
             patch.object(sync.subprocess, "Popen", return_value=FakeProc()), \
             patch.object(sync.base, "snapshot",
                          side_effect=lambda _pw: snapshots.append(True) or initial), \
             patch.object(sync.base, "iter_json_stream",
                          return_value=iter((changed,))), \
             patch.object(sync, "queue_dolby_postgain_for_generation",
                          return_value=-332) as queue, \
             patch.object(sync.msiir, "graph_running", return_value=True), \
             patch.object(sync, "apply_transaction",
                          side_effect=lambda state, *a, **k:
                          applied.append(state) or (123, 2)), \
             patch.object(sync, "restore_host_attenuation"), \
             patch.object(sync.base, "set_hardware_mute"):
            self.assertEqual(sync.run(args), 0)

        self.assertEqual(len(snapshots), 1)
        self.assertEqual(applied, [(0.25 ** 3, False), (0.5 ** 3, False)])
        queue.assert_called_once()

    def test_recreated_visible_sink_inherits_previous_state_before_transaction(self):
        class FakeStdout:
            def fileno(self):
                return 9

        class FakeProc:
            stdout = FakeStdout()
            returncode = 0

            def poll(self):
                return 0

            def terminate(self):
                raise AssertionError("completed fake monitor must not be terminated")

            def wait(self):
                return 0

        old_gain = 0.25 ** 3
        initial = [{
            "id": 41,
            "info": {
                "props": {"node.name": "virtual"},
                "params": {"Props": [{"channelVolumes": [old_gain]}]},
            },
        }, {
            "id": 69,
            "info": {"props": {"node.name": "hardware"}},
        }]
        recreated_unity = [{
            "id": 42,
            "info": {
                "props": {"node.name": "virtual"},
                "params": {"Props": [{"channelVolumes": [1.0]}]},
            },
        }]
        args = Namespace(
            delta_table=Path("deltas.json"), card="hw:0", amixer="amixer",
            tlv_write=Path(__file__), pw_dump="pw-dump", node="virtual",
            hardware_node="hardware", pcm_status=Path("status"),
            wpctl="wpctl", control=Path("control"), interval_ms=100,
            settle_ms=0, bootstrap_ms=0, bootstrap_guard_ms=0,
            node_settle_ms=0, once=False,
        )
        applied = []
        restored = []

        def fake_apply(state, *args, **kwargs):
            applied.append(state)
            _ui, db, postgain, _host = sync.base.derive_windows_state(*state)
            q28 = sync.qcadcm_q28_from_db(db)
            return q28, sync.msiir.select_ckv_step_q28(q28)

        with patch.object(sync, "load_deltas", return_value=(b"",) * 30), \
             patch.object(sync, "find_control_numid", return_value=321), \
             patch.object(sync, "find_mute_control_numid", return_value=None), \
             patch.object(sync, "find_volume_only_control_numid", return_value=None), \
             patch.object(sync, "find_control_values", return_value=284), \
             patch.object(sync.subprocess, "Popen", return_value=FakeProc()), \
             patch.object(sync.base, "snapshot", return_value=initial), \
             patch.object(sync.base, "iter_json_stream",
                          return_value=iter((recreated_unity,))), \
             patch.object(sync.msiir, "graph_running", return_value=True), \
             patch.object(sync, "apply_transaction", side_effect=fake_apply), \
             patch.object(sync, "queue_dolby_postgain_for_generation",
                          return_value=-332) as queue, \
             patch.object(sync, "restore_visible_control_state",
                          side_effect=lambda state, node_id, wpctl:
                          restored.append((state, node_id)) or 0.25), \
             patch.object(sync, "restore_host_attenuation"), \
             patch.object(sync.base, "set_hardware_mute"):
            self.assertEqual(sync.run(args), 0)

        self.assertEqual(restored, [((old_gain, False), 42)])
        self.assertEqual(queue.call_count, 2)
        self.assertTrue(applied)
        self.assertTrue(all(state == (old_gain, False) for state in applied))

    def test_exact_dsp_mute_change_does_not_resend_volume_transaction(self):
        args = Namespace(
            delta_table=Path("deltas.json"), card="hw:0", amixer="amixer",
            tlv_write=Path(__file__), pw_dump="pw-dump", node="virtual",
            hardware_node="hardware", pcm_status=Path("status"),
            wpctl="wpctl", control=Path("control"), interval_ms=100,
            once=False, settle_ms=0, bootstrap_ms=0, bootstrap_guard_ms=0,
            node_settle_ms=0,
        )

        class FakeStdout:
            def fileno(self): return 9
        class FakeProc:
            stdout = FakeStdout(); returncode = 0
            def poll(self): return 0
            def terminate(self): raise AssertionError
            def wait(self): return 0

        initial = [{
            "id": 41,
            "info": {"props": {"node.name": "virtual"},
                     "params": {"Props": [{"channelVolumes": [0.25 ** 3], "mute": False}]}},
        }, {"id": 69, "info": {"props": {"node.name": "hardware"}}}]
        muted_event = [{
            "id": 41,
            "info": {"props": {"node.name": "virtual"},
                     "params": {"Props": [{"channelVolumes": [0.25 ** 3], "mute": True}]}},
        }]
        applied = []
        dsp_mutes = []
        hardware_mutes = []
        with patch.object(sync, "load_deltas", return_value=(b"",) * 30), \
             patch.object(sync, "find_control_numid", return_value=321), \
             patch.object(sync, "find_mute_control_numid", return_value=322), \
             patch.object(sync, "find_volume_only_control_numid", return_value=None), \
             patch.object(sync, "find_control_values", return_value=288), \
             patch.object(sync.subprocess, "Popen", return_value=FakeProc()), \
             patch.object(sync.base, "snapshot", return_value=initial), \
             patch.object(sync.base, "iter_json_stream", return_value=iter((muted_event,))), \
             patch.object(sync, "queue_dolby_postgain_for_generation", return_value=-332), \
             patch.object(sync.msiir, "graph_running", return_value=True), \
             patch.object(sync, "write_endpoint_mute",
                          side_effect=lambda muted, **kw: dsp_mutes.append(muted)), \
             patch.object(sync, "apply_transaction",
                          side_effect=lambda state, *a, **k: (
                              applied.append(state) or (
                                  sync.qcadcm_q28_from_db(
                                      sync.base.derive_windows_state(*state)[1]
                                  ),
                                  sync.msiir.select_ckv_step_q28(
                                      sync.qcadcm_q28_from_db(
                                          sync.base.derive_windows_state(*state)[1]
                                      )
                                  ),
                              )
                          )), \
             patch.object(sync.base, "set_hardware_mute",
                          side_effect=lambda _id, muted, _wp: hardware_mutes.append(muted)), \
             patch.object(sync, "restore_host_attenuation"):
            self.assertEqual(sync.run(args), 0)

        self.assertEqual(applied, [(0.25 ** 3, False)])
        self.assertEqual(dsp_mutes, [False, True])
        # Successful exact DSP mute/unmute must not add a second physical-sink
        # mute edge. Hardware mute is reserved for rollback/fail-closed paths.
        self.assertEqual(hardware_mutes, [])

    def test_failed_exact_dsp_unmute_keeps_hardware_muted(self):
        args = Namespace(
            delta_table=Path("deltas.json"), card="hw:0", amixer="amixer",
            tlv_write=Path(__file__), pw_dump="pw-dump", node="virtual",
            hardware_node="hardware", pcm_status=Path("status"),
            wpctl="wpctl", control=Path("control"), interval_ms=100, once=True,
        )
        hardware_mutes = []
        hardware_volumes = []
        with patch.object(sync, "load_deltas", return_value=(b"",) * 30), \
             patch.object(sync, "find_control_numid", return_value=321), \
             patch.object(sync, "find_mute_control_numid", return_value=322), \
             patch.object(sync, "find_volume_only_control_numid", return_value=None), \
             patch.object(sync, "find_control_values", return_value=288), \
             patch.object(sync.base, "snapshot", return_value=[]), \
             patch.object(sync.base, "extract_node_volume", return_value=(0.25 ** 3, False)), \
             patch.object(sync.base, "extract_node_id", return_value=69), \
             patch.object(sync, "queue_dolby_postgain_for_generation", return_value=-332), \
             patch.object(sync.msiir, "graph_running", return_value=True), \
             patch.object(sync, "write_endpoint_mute", side_effect=RuntimeError("DSP reject")), \
             patch.object(sync.base, "set_hardware_volume",
                          side_effect=lambda _id, scalar, _wp: hardware_volumes.append(scalar)), \
             patch.object(sync.base, "set_hardware_mute",
                          side_effect=lambda _id, muted, _wp: hardware_mutes.append(muted)), \
             patch.object(sync, "apply_transaction") as apply_tx:
            self.assertEqual(sync.run(args), 0)
        apply_tx.assert_not_called()
        self.assertEqual(hardware_mutes[-1], True)
        self.assertTrue(hardware_volumes)

    def test_dispatch_prefers_explicit_candidate_helper_dir(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            helper_dir = Path(tmpdir) / "candidate-bin"
            helper_dir.mkdir()
            helper = helper_dir / "sp11-windows-volume-transaction-sync"
            helper.write_text("#!/bin/sh\n")
            with patch.dict(sync.os.environ, {"UBIG_VOLUME_HELPER_DIR": str(helper_dir)}, clear=False):
                self.assertEqual(dispatch.resolve_program("sp11-windows-volume-transaction-sync"), helper)

    def test_load_module_prefers_explicit_candidate_helper_dir(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            helper_dir = Path(tmpdir) / "candidate-bin"
            helper_dir.mkdir()
            helper = helper_dir / "test-helper"
            helper.write_text("answer = 84\n")
            installed = Path(tmpdir) / ".local/bin/test-helper"
            installed.parent.mkdir(parents=True)
            installed.write_text("answer = 42\n")
            with patch.dict(sync.os.environ, {"UBIG_VOLUME_HELPER_DIR": str(helper_dir)}, clear=False), \
                 patch.object(sync, "ROOT", None), \
                 patch.object(sync.Path, "home", return_value=Path(tmpdir)):
                module = sync.load_module("unused.py", "test-helper", "test_candidate_helper")
        self.assertEqual(module.answer, 84)

    def test_load_module_accepts_extensionless_installed_helper(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            helper = Path(tmpdir) / ".local/bin/test-helper"
            helper.parent.mkdir(parents=True)
            helper.write_text("answer = 42\n")
            with patch.object(sync, "ROOT", None), \
                 patch.object(sync.Path, "home", return_value=Path(tmpdir)):
                module = sync.load_module(
                    "unused.py", "test-helper", "test_extensionless_helper"
                )
        self.assertEqual(module.answer, 42)


if __name__ == "__main__":
    unittest.main()

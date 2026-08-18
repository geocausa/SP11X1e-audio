import unittest
from pathlib import Path
from unittest.mock import patch

from deploy.dolby import sp11_dolby_monitor_link as linker


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "deploy/dolby/98-sp11-windows-dolby.conf"


class DolbyControlSplitTests(unittest.TestCase):
    def test_visible_sink_is_copy_only_and_hidden_engine_hosts_dolby(self):
        text = CONFIG.read_text()
        visible = text.index('node.name        = "effect_input.sp11_windows_dolby"')
        engine = text.index('node.name          = "effect_input.sp11_windows_dolby_engine"')
        plugin = text.index("label  = sp11_dolby_windows_chain")
        self.assertLess(visible, engine)
        self.assertGreater(plugin, visible)
        self.assertLess(plugin, engine)
        self.assertIn("copy_l", text[:plugin])
        self.assertIn("copy_r", text[:plugin])
        self.assertIn(
            'plugin = "/home/geoca/.local/lib/sp11-dolby/sp11_dolby_windows_chain.so"',
            text,
        )
        self.assertIn('media.class      = Audio/Sink', text[:engine])
        # The hidden capture side deliberately omits Audio/Sink so it is a
        # Stream/Input/Audio and does not create a second desktop speaker.
        engine_capture = text[engine:text.index("playback.props", engine)]
        self.assertNotIn("media.class", engine_capture)
        # The hidden input must be passive.  Otherwise the persistent unity
        # monitor links pin the complete Dolby->ALSA speaker graph RUNNING at
        # desktop idle, unlike Windows which disables the PA after playback.
        self.assertIn("node.passive       = true", engine_capture)

    def test_control_copy_cannot_autoconnect(self):
        text = CONFIG.read_text()
        unused = text.index('effect_output.sp11_windows_dolby_control_unused')
        engine = text.index('node.name          = "effect_input.sp11_windows_dolby_engine"', unused)
        section = text[unused:engine]
        self.assertIn("node.autoconnect   = false", section)
        self.assertIn("node.dont-reconnect = true", section)

    def test_linker_has_only_exact_unity_monitor_links(self):
        self.assertEqual(
            linker.LINKS,
            (
                (
                    "effect_input.sp11_windows_dolby:monitor_FL",
                    "effect_input.sp11_windows_dolby_engine:input_FL",
                ),
                (
                    "effect_input.sp11_windows_dolby:monitor_FR",
                    "effect_input.sp11_windows_dolby_engine:input_FR",
                ),
            ),
        )

    def test_node_id_finds_hidden_engine(self):
        class CP:
            returncode = 0
            stderr = ""
            stdout = (
                '[{"id": 91, "info": {"props": {'
                '"node.name": "effect_input.sp11_windows_dolby_engine"}}}]'
            )

        with patch.object(linker, "_run", return_value=CP()):
            self.assertEqual(linker.node_id("pw-dump", linker.ENGINE_NODE), 91)

    def test_engine_unity_guard_repairs_only_hidden_engine_props(self):
        calls = []

        class CP:
            def __init__(self, returncode=0, stdout=""):
                self.returncode = returncode
                self.stdout = stdout
                self.stderr = ""

        def fake_run(args):
            calls.append(args)
            if args[1:3] == ["get-volume", "91"]:
                return CP(stdout="Volume: 0.06 [MUTED]\n")
            if args[1:3] == ["set-volume", "91"]:
                return CP()
            if args[1:3] == ["set-mute", "91"]:
                return CP()
            raise AssertionError(args)

        with patch.object(linker, "_run", side_effect=fake_run):
            self.assertTrue(linker.ensure_engine_unity(91, "wpctl"))
        self.assertIn(["wpctl", "set-volume", "91", "1.0"], calls)
        self.assertIn(["wpctl", "set-mute", "91", "0"], calls)

    def test_engine_unity_guard_is_noop_when_already_unity(self):
        class CP:
            returncode = 0
            stderr = ""
            stdout = "Volume: 1.00\n"

        with patch.object(linker, "_run", return_value=CP()) as run:
            self.assertTrue(linker.ensure_engine_unity(91, "wpctl"))
        run.assert_called_once_with(["wpctl", "get-volume", "91"])

    def test_reconcile_creates_missing_exact_links(self):
        calls = []

        class CP:
            def __init__(self, returncode=0, stdout=""):
                self.returncode = returncode
                self.stdout = stdout
                self.stderr = ""

        def fake_run(args):
            calls.append(args)
            if args[1] == "-o":
                return CP(stdout="\n".join(source for source, _ in linker.LINKS))
            if args[1] == "-i":
                return CP(stdout="\n".join(sink for _, sink in linker.LINKS))
            if args[1] == "-l":
                if any(args[1:2] == ["-L"] for args in calls):
                    return CP(stdout="\n".join(
                        f"{source}\n  |-> {sink}" for source, sink in linker.LINKS
                    ))
                return CP(stdout="")
            if args[1] == "-L":
                return CP()
            raise AssertionError(args)

        with patch.object(linker, "_run", side_effect=fake_run):
            self.assertTrue(linker.reconcile("pw-link"))
        creates = [call for call in calls if call[1] == "-L"]
        self.assertEqual(len(creates), 2)


if __name__ == "__main__":
    unittest.main()

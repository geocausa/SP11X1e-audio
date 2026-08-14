from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
PATCH = (
    ROOT
    / "patches"
    / "0051-ASoC-q6apm-quiesce-pull-watermarks-before-soft-pause.patch"
)


class Sp11SoftPauseOrderingTests(unittest.TestCase):
    def setUp(self):
        self.patch = PATCH.read_text(encoding="utf-8")
        self.additions = "\n".join(
            line[1:]
            for line in self.patch.splitlines()
            if line.startswith("+") and not line.startswith("+++")
        )

    def test_pull_stream_stops_before_pause_command(self):
        hunk = self.patch.split("@@", 2)[2]
        stopped = hunk.index(
            "+\t\tprtd->state = Q6APM_STREAM_STOPPED;"
        )
        command = hunk.index(
            " \tret = q6apm_graph_sp11_soft_pause(prtd->graph, pause);"
        )
        self.assertLess(stopped, command)

    def test_rejected_pause_restores_running_state(self):
        self.assertIn("if (ret) {", self.additions)
        self.assertIn("if (pause)", self.additions)
        self.assertIn(
            "prtd->state = Q6APM_STREAM_RUNNING;", self.additions
        )

    def test_change_is_limited_to_pcm_frontend(self):
        changed_files = [
            line.removeprefix("diff --git a/").split(" b/", 1)[0]
            for line in self.patch.splitlines()
            if line.startswith("diff --git a/")
        ]
        self.assertEqual(
            changed_files, ["sound/soc/qcom/qdsp6/q6apm-dai.c"]
        )

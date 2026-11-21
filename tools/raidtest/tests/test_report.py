"""The markdown report and the batch.py and boss reader calls it makes.

    python -m unittest discover -s tools/raidtest/tests
"""
from __future__ import annotations

import datetime
import io
import pathlib
import subprocess
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import dc  # noqa: E402
from pulls import TraceFacts, Veto  # noqa: E402
from report import PullRow, State, boss_reader, render, verdict  # noqa: E402

UTC = datetime.timezone.utc
HEAD = datetime.datetime(2026, 11, 21, 12, 0, tzinfo=UTC)
START = int(HEAD.timestamp() * 1000) + 3_600_000


def facts(name: str, built: datetime.datetime, **extra) -> TraceFacts:
    base = dict(path=pathlib.Path("/logs") / name, encounter="northrend-beasts", built=built, diff=1,
                warnings=[], hardmode=None, start_ms=START, end_ms=START + 125_000, outcome="wipe", closed=True)
    base.update(extra)
    return TraceFacts(**base)


class FakeRun:
    def __init__(self):
        self.calls = []

    def __call__(self, command, **kwargs):
        self.calls.append(command)
        return subprocess.CompletedProcess(command, 0, "tool output\n", "")


class ReportTest(unittest.TestCase):
    def render(self, state: State) -> tuple[str, FakeRun]:
        out, run = io.StringIO(), FakeRun()
        render(state, run, out)
        return out.getvalue(), run

    def state(self) -> State:
        after = HEAD + datetime.timedelta(minutes=30)
        good = facts("649_1_northrend-beasts_1.ndjson", after,
                     deaths=["Mendy (heal) at 1:02.000: Icehowl, last hit Trample for 40000 from Icehowl"],
                     vetoes=[Veto(START + 10_000, "Tanky", "dungeon clear", "follow"),
                             Veto(START + 500_000, "Tanky", "dungeon clear", "follow")])
        bad = facts("649_1_northrend-beasts_2.ndjson", after, warnings=[("raid-dead", "only 2 of 25 alive")])
        old = facts("649_1_northrend-beasts_0.ndjson", HEAD - datetime.timedelta(hours=3))
        run = dc.Run("tr-1", "success", "", START, START + 300_000,
                     [dc.Pull(START, START + 120_000, "wipe"), dc.Pull(START + 130_000, START + 250_000, "kill")])
        return State(boss="northrend-beasts", diff="25n", head_sha="abc1234", head_time=HEAD, wanted=3, cap=6,
                     runs=[("test", run)],
                     rows=[PullRow("test", "tr-1", 1, (START, START + 120_000), "wipe", good),
                           PullRow("test", "tr-1", 2, (START + 130_000, START + 250_000), "kill", bad)],
                     counted=[good], baseline=[old])

    def test_sections(self):
        text, run = self.render(self.state())
        self.assertIn("# Raid test: northrend-beasts 25n", text)
        self.assertIn("**1/3 valid pulls** after `abc1234`, 2 pulled this cycle (cap 6). The cap ran out first.",
                      text)
        self.assertIn("| tr-1 | success | 2 | - |", text)
        self.assertIn("| 1 | test | tr-1 | wipe | wipe | 2:00 | - | 1 | counts |", text)
        self.assertIn("| no: raid-dead |", text)
        self.assertIn("- pull 2: only 2 of 25 alive", text)
        self.assertIn("Icehowl, last hit Trample", text)
        self.assertIn("pull 1: 1 row(s): dungeon clear zeroed follow x1", text)

    def test_tool_calls(self):
        _, run = self.render(self.state())
        tails = [call[1:] for call in run.calls]
        batch = tails[0][0]
        self.assertTrue(batch.endswith("batch.py"))
        self.assertEqual(tails[0][1:], ["--strict", "--since", "abc1234", "--boss", "northrend-beasts"])
        self.assertEqual(tails[1][1:5], ["--boss", "northrend-beasts", "--split-at", "abc1234"])
        self.assertTrue(tails[1][5].endswith("_0.ndjson") and tails[1][6].endswith("_1.ndjson"))
        self.assertEqual(tails[2][1:4], ["--boss", "northrend-beasts", "--probes"])
        self.assertTrue(tails[3][0].endswith("northrend_beasts.py"))

    def test_no_baseline_skips_the_split(self):
        state = self.state()
        state.baseline = []
        text, run = self.render(state)
        self.assertIn("Skipped: needs valid pulls on both sides of HEAD (0 before, 1 after).", text)
        self.assertFalse(any("--split-at" in call for call in run.calls))

    def test_stopped(self):
        state = self.state()
        state.stopped = "the worldserver build failed"
        text, _ = self.render(state)
        self.assertIn("**Stopped:** the worldserver build failed", text)

    def test_no_contamination(self):
        state = self.state()
        state.rows[0].trace.vetoes = []
        text, _ = self.render(state)
        self.assertIn("None: no `dungeon clear` or `dungeon clear combat` veto row falls inside a pull.", text)

    def test_boss_reader_filed_under_an_alias(self):
        self.assertEqual(boss_reader("vezax").name, "general_vezax.py")
        self.assertEqual(boss_reader("northrend-beasts").name, "northrend_beasts.py")
        self.assertIsNone(boss_reader("iron-assembly"))

    def test_verdicts(self):
        before = facts("x", HEAD - datetime.timedelta(hours=1))
        self.assertEqual(verdict(PullRow("t", "", 1, None, "", before), "25n", HEAD), "baseline")
        self.assertEqual(verdict(PullRow("t", "", 1, None, "", None), "25n", HEAD), "no trace")
        ten = facts("x", HEAD + datetime.timedelta(hours=1), diff=0)
        self.assertEqual(verdict(PullRow("t", "", 1, None, "", ten), "25n", HEAD), "no: difficulty 0")


if __name__ == "__main__":
    unittest.main()

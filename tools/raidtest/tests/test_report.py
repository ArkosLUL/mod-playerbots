"""The markdown report, split per pool, and the batch.py and boss reader calls it makes.

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
from pools import PullRow  # noqa: E402
from pulls import TraceFacts, Veto  # noqa: E402
from report import State, boss_reader, render, verdict  # noqa: E402

UTC = datetime.timezone.utc
HEAD = datetime.datetime(2026, 11, 21, 12, 0, tzinfo=UTC)
START = int(HEAD.timestamp() * 1000) + 3_600_000


def facts(name: str, built: datetime.datetime, **extra) -> TraceFacts:
    base = dict(path=pathlib.Path("/logs") / name, encounter="northrend-beasts", built=built, diff=3,
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
        old = HEAD - datetime.timedelta(hours=3)
        good1 = facts("649_11_northrend-beasts_1.ndjson", after,
                      deaths=["Mendy (heal) at 1:02.000: Icehowl, last hit Trample for 40000 from Icehowl"],
                      vetoes=[Veto(START + 10_000, "Tanky", "dungeon clear", "follow"),
                              Veto(START + 500_000, "Tanky", "dungeon clear", "follow")])
        bad2 = facts("649_12_northrend-beasts_2.ndjson", after, warnings=[("raid-dead", "only 2 of 25 alive")])
        good2 = facts("649_12_northrend-beasts_3.ndjson", after)
        base1 = facts("649_5_northrend-beasts_0.ndjson", old)
        run1 = dc.Run("tr-1", "success", "", START, START + 300_000, [dc.Pull(START, START + 120_000, "kill")],
                      pool="toc1", instance_id=11)
        run2 = dc.Run("tr-2", "success", "", START, START + 300_000,
                      [dc.Pull(START, START + 120_000, "wipe"), dc.Pull(START + 130_000, START + 250_000, "kill")],
                      pool="toc2", instance_id=12)
        return State(boss="northrend-beasts", diff="25h", head_sha="abc1234", head_time=HEAD, wanted=1,
                     pools=["toc1", "toc2", "toc3"], cap=2, runs=[("test", run1), ("test", run2)],
                     rows=[PullRow("test", "toc1", "tr-1", 1, (START, START + 120_000), "kill", good1),
                           PullRow("test", "toc2", "tr-2", 1, (START, START + 120_000), "wipe", bad2),
                           PullRow("test", "toc2", "tr-2", 2, (START + 130_000, START + 250_000), "kill", good2)],
                     counted={"toc1": [good1], "toc2": [good2], "toc3": []},
                     baseline={"toc1": [base1], "toc2": [], "toc3": []})

    def section(self, text: str, pool: str) -> str:
        return text.split(f"\n## {pool}\n", 1)[1].split("\n## ", 1)[0]

    def test_headline_counts_every_pool(self):
        text, _ = self.render(self.state())
        self.assertIn("# Raid test: northrend-beasts 25h", text)
        self.assertIn("**2/3 valid pulls** after `abc1234` (toc1 1/1, toc2 1/1, toc3 0/1), 3 pulled this cycle "
                      "(cap 2 per pool). The cap ran out first.", text)
        self.assertIn("| test | toc2 | tr-2 | 12 | success | 2 | - |", text)

    def test_pool_sections(self):
        text, _ = self.render(self.state())
        toc1, toc2, toc3 = (self.section(text, pool) for pool in ("toc1", "toc2", "toc3"))
        self.assertIn("1/1 valid after HEAD, 1 baseline.", toc1)
        self.assertIn("| 1 | test | tr-1 | kill | wipe | 2:00 | - | 1 | no: DC vetoed actions in the fight |", toc1)
        self.assertIn("Icehowl, last hit Trample", toc1)
        self.assertIn("pull 1: 1 row(s): dungeon clear zeroed follow x1", toc1)
        self.assertIn("| 2 | test | tr-2 | kill |", toc2)
        self.assertIn("| no: raid-dead |", toc2)
        self.assertIn("- pull 1: only 2 of 25 alive", toc2)
        self.assertNotIn("Icehowl", toc2)
        self.assertIn("No pulls.", toc3)
        self.assertLess(text.index("\n## toc3\n"), text.index("\n## All pools\n"))

    def test_split_at_reads_only_the_pools_own_traces(self):
        text, run = self.render(self.state())
        splits = [call[1:] for call in run.calls if "--split-at" in call]
        self.assertEqual(len(splits), 1)
        self.assertEqual(splits[0][1:5], ["--boss", "northrend-beasts", "--split-at", "abc1234"])
        self.assertEqual([pathlib.Path(p).name for p in splits[0][5:]],
                         ["649_5_northrend-beasts_0.ndjson", "649_11_northrend-beasts_1.ndjson"])
        self.assertIn("Skipped: needs valid toc2 pulls on both sides of HEAD (0 before, 1 after).",
                      self.section(text, "toc2"))

    def test_overall_tools(self):
        text, run = self.render(self.state())
        tails = [call[1:] for call in run.calls]
        strict = [t for t in tails if "--strict" in t]
        self.assertEqual(strict[0][1:], ["--strict", "--since", "abc1234", "--boss", "northrend-beasts"])
        probes = [t for t in tails if "--probes" in t][0]
        self.assertEqual([pathlib.Path(p).name for p in probes[4:]],
                         ["649_11_northrend-beasts_1.ndjson", "649_12_northrend-beasts_3.ndjson"])
        readers = [t for t in tails if t[0].endswith("northrend_beasts.py")]
        self.assertEqual(len(readers), 2)
        overall = text.split("\n## All pools\n", 1)[1]
        self.assertIn("**2/3 valid pulls**", overall)

    def test_stopped(self):
        state = self.state()
        state.stopped = "the worldserver build failed"
        text, _ = self.render(state)
        self.assertIn("**Stopped:** the worldserver build failed", text)

    def test_no_contamination(self):
        state = self.state()
        state.rows[0].trace.vetoes = []
        text, _ = self.render(state)
        self.assertIn("None: no `dungeon clear` or `dungeon clear combat` veto row falls inside a pull.",
                      self.section(text, "toc1"))

    def test_boss_reader_filed_under_an_alias(self):
        self.assertEqual(boss_reader("vezax").name, "general_vezax.py")
        self.assertEqual(boss_reader("northrend-beasts").name, "northrend_beasts.py")
        self.assertIsNone(boss_reader("iron-assembly"))

    def test_verdicts(self):
        before = facts("x", HEAD - datetime.timedelta(hours=1))
        self.assertEqual(verdict(PullRow("t", "toc1", "", 1, None, "", before), "25h", HEAD), "baseline")
        self.assertEqual(verdict(PullRow("t", "toc1", "", 1, None, "", None), "25h", HEAD), "no trace")
        ten = facts("x", HEAD + datetime.timedelta(hours=1), diff=0)
        self.assertEqual(verdict(PullRow("t", "toc1", "", 1, None, "", ten), "25h", HEAD), "no: difficulty 0")


if __name__ == "__main__":
    unittest.main()

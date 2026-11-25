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
from pulls import DeathRow, TraceFacts, Veto  # noqa: E402
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
        render(state, run, out, full=True)
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
        self.assertIsNone(boss_reader("no-such-boss"))

    def test_the_digest_is_the_default(self):
        out, run = io.StringIO(), FakeRun()
        render(self.state(), run, out)
        text = out.getvalue()
        self.assertIn("\n## Done rule\n", text)
        self.assertNotIn("\n## toc1\n", text)
        self.assertFalse(any("--split-at" in call for call in run.calls))

    def test_verdicts(self):
        before = facts("x", HEAD - datetime.timedelta(hours=1))
        self.assertEqual(verdict(PullRow("t", "toc1", "", 1, None, "", before), "25h", HEAD), "baseline")
        self.assertEqual(verdict(PullRow("t", "toc1", "", 1, None, "", None), "25h", HEAD), "no trace")
        ten = facts("x", HEAD + datetime.timedelta(hours=1), diff=0)
        self.assertEqual(verdict(PullRow("t", "toc1", "", 1, None, "", ten), "25h", HEAD), "no: difficulty 0")
        after = facts("x", HEAD + datetime.timedelta(hours=1))
        self.assertEqual(verdict(PullRow("t", "toc1", "", 1, None, "", after), "25h", HEAD), "counts")
        self.assertEqual(verdict(PullRow("t", "toc1", "", 1, None, "", after, engaged_first=True), "25h", HEAD),
                         "no: the boss engaged before the start step, no refresh")


def died(t_ms: int, mechanic: str, name: str = "Bot") -> DeathRow:
    return DeathRow(t_ms, name, "melee", mechanic)


def pull(name: str, built: datetime.datetime, outcome: str, rows: list[DeathRow]) -> TraceFacts:
    return facts(name, built, outcome=outcome, deaths=[f"{row.name} at {row.t_ms}" for row in rows],
                 death_rows=rows)


class DigestTest(unittest.TestCase):
    POOLS = ["uld1", "uld2", "uld3", "uld4", "uld5", "uld6"]

    def state(self, counted: dict[str, list[TraceFacts]], history=()) -> State:
        rows = [PullRow("test", pool, f"tr-{pool}", 1, None, "", found[0])
                for pool, found in counted.items() if found]
        return State(boss="vezax", diff="25n", head_sha="abc1234", head_time=HEAD, wanted=1, pools=self.POOLS,
                     cap=2, rows=rows, counted=counted, history=list(history))

    def render(self, state: State) -> tuple[str, FakeRun]:
        out, run = io.StringIO(), FakeRun()
        render(state, run, out)
        return out.getvalue(), run

    def section(self, text: str, title: str) -> str:
        return text.split(f"\n## {title}\n", 1)[1].split("\n## ", 1)[0]

    def test_done_rule_needs_eight_of_ten_clean_pulls_per_composition(self):
        after = HEAD + datetime.timedelta(minutes=30)
        five = [died(1000 * n, "Shadow Crash 62659") for n in range(5)]
        six = [died(1000 * n, "Shadow Crash 62659") for n in range(6)]
        pools = [f"uld{number}" for number in range(1, 31)]
        counted = {pool: [pull(pool, after, "kill", five)] for pool in pools}
        counted["uld1"] = [pull("uld1", after, "kill", six)]
        counted["uld4"] = [pull("uld4", after, "wipe", five)]
        for pool in ("uld2", "uld5", "uld8"):
            counted[pool] = [pull(pool, after, "wipe", six)]
        counted["uld3"] = [facts("uld3", after, outcome="kill", hardmode=False)]

        def done_rule() -> str:
            state = self.state(counted)
            state.pools = pools
            return self.section(self.render(state)[0], "Done rule")

        text = done_rule()
        self.assertIn("- A (10 pools): 8 of 10 clean, needs 8; not clean: uld1 kill 6, uld4 wipe 5", text)
        self.assertIn("- B (10 pools): 7 of 10 clean, needs 8; not clean: uld2 wipe 6, uld5 wipe 6, uld8 wipe 6",
                      text)
        self.assertIn("- C (10 pools): 9 of 10 clean, needs 8; not clean: uld3 kill 0 hard mode OFF", text)
        self.assertIn("This build not yet: B.", text)
        counted["uld8"] = [pull("uld8", after, "kill", [])]
        self.assertIn("This build passes, if the boss reader shows hard mode triggered in each clean kill.",
                      done_rule())
        for found in counted.values():
            if found[0].hardmode is None:
                found[0].hardmode = True
        self.assertIn("This build passes.", done_rule())

    def test_done_rule_needs_every_pool_pulled_and_a_two_copy_roster_both_clean(self):
        after = HEAD + datetime.timedelta(minutes=30)
        counted = {"uld1": [pull("a1", after, "kill", [])], "uld4": [pull("a4", after, "kill", [])],
                   "uld2": [pull("b2", after, "kill", [])], "uld5": [pull("b5", after, "wipe", [])],
                   "uld3": [pull("c3", after, "kill", [])]}
        text = self.section(self.render(self.state(counted))[0], "Done rule")
        self.assertIn("- A (2 pools): 2 of 2 clean, needs 2", text)
        self.assertIn("- B (2 pools): 1 of 2 clean, needs 2; not clean: uld5 wipe 0", text)
        self.assertIn("- C (2 pools): 1 of 1 clean, needs 1; no valid pull on this build from uld6", text)
        self.assertIn("This build not yet: B, C.", text)

    def test_done_rule_reads_only_the_newest_build(self):
        older, newer = HEAD + datetime.timedelta(minutes=30), HEAD + datetime.timedelta(hours=2)
        counted = {"uld1": [pull("a1", newer, "kill", [])], "uld2": [pull("b2", newer, "kill", [])],
                   "uld3": [pull("c3", newer, "wipe", [])], "uld6": [pull("c6", older, "kill", [])]}
        text = self.section(self.render(self.state(counted))[0], "Done rule")
        self.assertIn("- C (2 pools): 0 of 1 clean, needs 1; not clean: uld3 wipe 0; no valid pull on this build "
                      "from uld6", text)
        self.assertIn("This build not yet: A, B, C.", text)

    def test_done_rule_needs_every_composition_in_the_report(self):
        after = HEAD + datetime.timedelta(minutes=30)
        state = self.state({"uld1": [pull("a1", after, "kill", [])]})
        state.pools = ["uld1"]
        text = self.section(self.render(state)[0], "Done rule")
        self.assertIn("- B: no pool of it in this report", text)
        self.assertIn("This build can't be judged here: no pool of B, C.", text)

    def test_deaths_by_mechanic_and_per_pull(self):
        after = HEAD + datetime.timedelta(minutes=30)
        cascade = [died(1000, "Shadow Crash 62659", "First")] + \
            [died(60_000 + n * 1000, "melee from General Vezax") for n in range(10)] + \
            [died(80_000, "Shadow Crash 62659")]
        counted = {"uld1": [pull("a1", after, "wipe", cascade)], "uld2": [pull("b2", after, "kill", [])]}
        text, _ = self.render(self.state(counted))
        mechanics = self.section(text, "Deaths by mechanic")
        self.assertIn("| melee from General Vezax | 10 | 1/2 | 1:00 |", mechanics)
        self.assertIn("| Shadow Crash 62659 | 2 | 1/2 | 0:01 |", mechanics)
        per_pull = self.section(text, "Deaths per pull")
        self.assertIn("**Pull 1** uld1 (A), wipe at 2:05: 12 deaths", per_pull)
        self.assertIn("- 0:01 First (melee): Shadow Crash 62659", per_pull)
        self.assertEqual(per_pull.count("\n- "), 9)
        self.assertIn("- then 4 more from 1:07 to 1:20: melee from General Vezax x3, Shadow Crash 62659 x1",
                      per_pull)
        self.assertIn("**Pull 2** uld2 (B), kill at 2:05: 0 deaths", per_pull)
        self.assertNotIn("Each pull's first", per_pull)

    def test_thirty_pulls_show_two_deaths_each_in_full(self):
        after = HEAD + datetime.timedelta(minutes=30)
        deaths = [died(1000 * (n + 1), "Shadow Crash 62659") for n in range(5)]
        counted = {f"uld{number}": [pull(f"p{number}", after, "wipe", deaths)] for number in range(1, 31)}
        per_pull = self.section(self.render(self.state(counted))[0], "Deaths per pull")
        self.assertTrue(per_pull.lstrip().startswith("Each pull's first 2 deaths, then the rest by mechanic."))
        self.assertEqual(per_pull.count("\n- 0:0"), 60)
        self.assertEqual(per_pull.count("\n- then 3 more from 0:03 to 0:05: Shadow Crash 62659 x3"), 30)

    def test_trend_over_the_last_four_builds(self):
        builds = [HEAD + datetime.timedelta(hours=hours) for hours in (-4, -3, -2, -1, 1)]
        crash = [died(1000, "Shadow Crash 62659"), died(2000, "Shadow Crash 62659")]
        history = []
        for index, built in enumerate(builds):
            history += [("uld1", pull(f"a{index}", built, "kill", [])),
                        ("uld4", pull(f"d{index}", built, "wipe", crash)),
                        ("uld2", pull(f"b{index}", built, "wipe", crash[:1]))]
        text = self.section(self.render(self.state({}, history))[0], "Trend over the last 4 builds")
        rows = [line for line in text.splitlines() if line.startswith("| ") and "build" not in line]
        self.assertEqual(len(rows), 4)
        stamp = builds[-1].astimezone().strftime("%m-%d %H:%M")
        self.assertEqual(rows[0], f"| {stamp} (after HEAD) | 3 | A 1/1/2, B 0/0/1 | 1 | 2:05 "
                                  "| Shadow Crash 62659 1.0 |")
        self.assertNotIn(builds[0].astimezone().strftime("%m-%d %H:%M"), text)

    def test_reader_summaries_and_no_full_dumps(self):
        after = HEAD + datetime.timedelta(minutes=30)
        counted = {"uld1": [pull("603_1_general-vezax_1.ndjson", after, "kill", [])]}
        text, run = self.render(self.state(counted))
        (call,) = run.calls
        self.assertEqual((pathlib.Path(call[1]).name, call[-1]), ("general_vezax.py", "--summary"))
        self.assertIn("### uld1 (A): 603_1_general-vezax_1.ndjson", text)
        self.assertIn("Each valid pull's `--summary`", text)
        self.assertNotIn("DC multiplier vetoes", text)

    def test_reader_summaries_of_each_compositions_fewest_and_most_deaths(self):
        after = HEAD + datetime.timedelta(minutes=30)
        crash = [died(1000 * n, "Shadow Crash 62659") for n in range(9)]
        counted = {"uld1": [pull("a1", after, "wipe", crash[:3])], "uld4": [pull("a4", after, "kill", [])],
                   "uld7": [pull("a7", after, "wipe", crash)], "uld10": [pull("a10", after, "wipe", crash[:3])],
                   "uld2": [pull("b2", after, "kill", crash[:1])]}
        state = self.state(counted)
        state.pools = ["uld1", "uld2", "uld4", "uld7", "uld10"]
        text, run = self.render(state)
        self.assertEqual([pathlib.Path(call[2]).name for call in run.calls], ["b2", "a4", "a7"])
        self.assertIn("each composition's pulls with the fewest and the most deaths, 3 of 5", text)


if __name__ == "__main__":
    unittest.main()

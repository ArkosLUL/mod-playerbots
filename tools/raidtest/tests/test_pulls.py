"""Pull counting over synthetic RaidObs traces: validity, build against HEAD, difficulty, the baseline,
matching DC pull windows to traces, and the DC veto window.

    python -m unittest discover -s tools/raidtest/tests
"""
from __future__ import annotations

import datetime
import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import pulls  # noqa: E402
from pulls import Corpus, match_traces, vetoes_inside  # noqa: E402

UTC = datetime.timezone.utc
HEAD = datetime.datetime(2026, 11, 21, 12, 0, tzinfo=UTC)
TANK, HEALER = 101, 102


def ms(when: datetime.datetime) -> int:
    return int(when.timestamp() * 1000)


def write_trace(folder: pathlib.Path, pulled: datetime.datetime, built: datetime.datetime | None, diff: int = 1,
                human: bool = False, dead: bool = False, records: list[dict] | None = None,
                closed: bool = True, length_ms: int = 60000) -> pathlib.Path:
    header = {"e": "hdr", "v": 14, "ts": ms(pulled), "map": 603, "inst": 7, "diff": diff,
              "boss": "xt-002-deconstructor", "cfg": {},
              "roster": [{"g": TANK, "n": "Tanky", "c": 1, "r": "tank"},
                         {"g": HEALER, "n": "Mendy", "c": 2, "r": "heal"}]}
    if human:
        header["roster"].append({"g": 103, "n": "Someone", "c": 4, "r": "melee", "h": 1})
    if built is not None:
        header["bin"] = ms(built)
    hp = 0 if dead else 100
    lines = [header,
             {"e": "snap", "t": 0, "u": [[TANK, 0, 0, 0, 0, hp, 100, 0, 0, 0, 0, 0],
                                         [HEALER, 1, 1, 0, 0, hp, 100, 0, 0, 0, 0, 0]]}]
    lines += records or []
    if closed:
        lines.append({"e": "end", "t": length_ms, "out": "kill"})
    path = folder / f"603_7_xt-002-deconstructor_{int(pulled.timestamp())}.ndjson"
    path.write_text("\n".join(json.dumps(line) for line in lines) + "\n", encoding="utf-8")
    return path


class CorpusTest(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.dir = pathlib.Path(folder.name)

    def corpus(self) -> Corpus:
        return Corpus("xt-002", [self.dir])

    def at(self, minutes: int) -> datetime.datetime:
        return HEAD + datetime.timedelta(minutes=minutes)

    def test_counts_only_valid_pulls_built_after_head_at_the_difficulty(self):
        good = write_trace(self.dir, self.at(30), self.at(20))
        write_trace(self.dir, self.at(40), self.at(-20))                # binary predates HEAD
        write_trace(self.dir, self.at(50), self.at(20), diff=0)         # 10-man
        write_trace(self.dir, self.at(60), self.at(20), human=True)     # a human in the raid
        write_trace(self.dir, self.at(70), self.at(20), dead=True)      # raid dead at the open
        write_trace(self.dir, self.at(80), None)                        # no build stamp
        newer = write_trace(self.dir, self.at(90), self.at(20))
        self.assertEqual([f.path for f in self.corpus().counted("25n", HEAD)], [newer, good])

    def test_strict_drops_a_human_who_only_did_damage(self):
        path = write_trace(self.dir, self.at(30), self.at(20), human=True)
        facts = pulls.read_facts(path)
        self.assertEqual([kind for kind, _ in facts.warnings], ["human-in-raid"])
        self.assertFalse(facts.valid(1))

    def test_baseline_is_the_newest_valid_before_head(self):
        write_trace(self.dir, self.at(-300), self.at(-400))
        middle = write_trace(self.dir, self.at(-200), self.at(-400))
        newest = write_trace(self.dir, self.at(-100), self.at(-400))
        write_trace(self.dir, self.at(-50), self.at(-400), human=True)
        write_trace(self.dir, self.at(30), self.at(20))
        self.assertEqual([f.path for f in self.corpus().baseline("25n", HEAD, 2)], [newest, middle])

    def test_difficulty_ids(self):
        path = write_trace(self.dir, self.at(30), self.at(20), diff=3)
        self.assertEqual([f.path for f in self.corpus().counted("25h", HEAD)], [path])
        self.assertEqual(self.corpus().counted("25n", HEAD), [])

    def test_open_trace_is_read_again(self):
        path = write_trace(self.dir, self.at(30), self.at(20), closed=False)
        corpus = self.corpus()
        self.assertFalse(corpus.facts(path).closed)
        write_trace(self.dir, self.at(30), self.at(20))
        self.assertTrue(corpus.facts(path).closed)

    def test_between(self):
        inside = write_trace(self.dir, self.at(30), self.at(20))
        write_trace(self.dir, self.at(90), self.at(20))
        found = self.corpus().between(ms(self.at(29)), ms(self.at(31)))
        self.assertEqual([f.path for f in found], [inside])

    def test_deaths_and_vetoes(self):
        records = [
            {"e": "death", "t": 4000, "g": HEALER, "killer": 4294967296 + 5, "cause": None,
             "blow": [4294967296 + 5, 9000], "rewind": [[3000, 4294967296 + 5, 62016, 9000]]},
            {"e": "death", "t": 5000, "g": TANK, "killer": TANK, "cause": "reset"},
            {"e": "veto", "t": 2000, "g": TANK, "m": "dungeon clear", "a": "follow"},
            {"e": "veto", "t": 2500, "g": TANK, "m": "dungeon clear combat", "a": "attack anything"},
            {"e": "veto", "t": 2600, "g": TANK, "m": "xt002", "a": "melee"},
        ]
        facts = pulls.read_facts(write_trace(self.dir, self.at(30), self.at(20), records=records))
        self.assertEqual(len(facts.deaths), 1)
        self.assertIn("Mendy (heal)", facts.deaths[0])
        self.assertIn("last hit spell 62016 for 9000", facts.deaths[0])
        self.assertEqual(facts.wipe_commands, 1)
        self.assertEqual([(v.multiplier, v.action) for v in facts.vetoes],
                         [("dungeon clear", "follow"), ("dungeon clear combat", "attack anything")])
        start = ms(self.at(30))
        self.assertEqual(len(vetoes_inside(facts, start + 2200, start + 9000)), 1)


class MatchTest(unittest.TestCase):
    def facts(self, name: str, start: int, end: int) -> pulls.TraceFacts:
        return pulls.TraceFacts(path=pathlib.Path(name), encounter="xt-002", built=None, diff=1, warnings=[],
                                hardmode=None, start_ms=start, end_ms=end, outcome="wipe", closed=True)

    def test_each_window_gets_its_own_trace(self):
        first = self.facts("a", 10_000, 70_000)
        second = self.facts("b", 100_000, 160_000)
        far = self.facts("c", 900_000, 960_000)
        matched = match_traces([(5_000, 72_000), (98_000, 161_000), (400_000, 450_000)], [far, second, first])
        self.assertEqual(matched, [first, second, None])

    def test_trace_used_once(self):
        only = self.facts("a", 10_000, 70_000)
        self.assertEqual(match_traces([(5_000, 72_000), (6_000, 71_000)], [only]), [only, None])


class CauseTest(unittest.TestCase):
    def test_causes(self):
        class FakeTrace:
            def name(self, guid):
                return {1: "Bot", 2: "Boss"}.get(guid, "-")

            def spell(self, spell):
                return f"spell {spell}"

        trace = FakeTrace()
        self.assertEqual(pulls.death_cause(trace, {"g": 1, "cause": "reset"}), "the wipe command")
        self.assertIn("script", pulls.death_cause(trace, {"g": 1, "cause": "self"}))
        self.assertIn("own blow", pulls.death_cause(trace, {"g": 1, "blow": [1, 500]}))
        self.assertEqual(pulls.death_cause(trace, {"g": 1, "killer": 2, "blow": [2, 500]}), "Boss")


if __name__ == "__main__":
    unittest.main()

"""Trace facts over synthetic RaidObs traces: validity, difficulty, the instance a trace was recorded in,
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
from unittest import mock

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
                closed: bool = True, length_ms: int = 60000, inst: int = 7, map_id: int = 603) -> pathlib.Path:
    header = {"e": "hdr", "v": 14, "ts": ms(pulled), "map": map_id, "inst": inst, "diff": diff,
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
    path = folder / f"{map_id}_{inst}_xt-002-deconstructor_{int(pulled.timestamp())}.ndjson"
    path.write_text("\n".join(json.dumps(line) for line in lines) + "\n", encoding="utf-8")
    return path


def isolate_cache(test: unittest.TestCase) -> pathlib.Path:
    folder = tempfile.TemporaryDirectory()
    test.addCleanup(folder.cleanup)
    patch = mock.patch.object(pulls, "CACHE_ROOT", pathlib.Path(folder.name))
    patch.start()
    test.addCleanup(patch.stop)
    return pathlib.Path(folder.name)


class CorpusTest(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.dir = pathlib.Path(folder.name)
        self.cache = isolate_cache(self)

    def corpus(self) -> Corpus:
        return Corpus("xt-002", [self.dir])

    def at(self, minutes: int) -> datetime.datetime:
        return HEAD + datetime.timedelta(minutes=minutes)

    def test_valid_only_without_warnings_and_with_a_build(self):
        def valid(path):
            return pulls.read_facts(path).valid(1)

        self.assertTrue(valid(write_trace(self.dir, self.at(30), self.at(20))))
        self.assertFalse(valid(write_trace(self.dir, self.at(50), self.at(20), diff=0)))
        self.assertFalse(valid(write_trace(self.dir, self.at(60), self.at(20), human=True)))
        self.assertFalse(valid(write_trace(self.dir, self.at(70), self.at(20), dead=True)))
        self.assertFalse(valid(write_trace(self.dir, self.at(80), None)))

    def test_strict_drops_a_human_who_only_did_damage(self):
        path = write_trace(self.dir, self.at(30), self.at(20), human=True)
        facts = pulls.read_facts(path)
        self.assertEqual([kind for kind, _ in facts.warnings], ["human-in-raid"])
        self.assertFalse(facts.valid(1))

    def test_difficulty_ids(self):
        facts = pulls.read_facts(write_trace(self.dir, self.at(30), self.at(20), diff=3))
        self.assertTrue(facts.valid(pulls.DIFF_IDS["25h"]))
        self.assertFalse(facts.valid(pulls.DIFF_IDS["25n"]))

    def test_open_trace_is_read_again(self):
        path = write_trace(self.dir, self.at(30), self.at(20), closed=False)
        corpus = self.corpus()
        self.assertFalse(corpus.facts(path).closed)
        write_trace(self.dir, self.at(30), self.at(20))
        self.assertTrue(corpus.facts(path).closed)

    def test_instance_from_the_header(self):
        facts = pulls.read_facts(write_trace(self.dir, self.at(30), self.at(20), inst=41, map_id=649))
        self.assertEqual((facts.map_id, facts.instance), (649, 41))

    def test_name_parts(self):
        self.assertEqual(pulls.name_parts(pathlib.Path("649_2_gormok-the-impaler_1794917863.ndjson")),
                         (649, 2, 1794917863))
        self.assertEqual(pulls.name_parts(pathlib.Path("603_7_flame_leviathan_1794917863.ndjson"))[:2], (603, 7))
        self.assertIsNone(pulls.name_parts(pathlib.Path("notes_1794917863.ndjson")))

    def test_in_instance_keeps_overlapping_instances_apart(self):
        mine = write_trace(self.dir, self.at(30), self.at(20), inst=8)
        write_trace(self.dir, self.at(30) + datetime.timedelta(seconds=5), self.at(20), inst=7)
        write_trace(self.dir, self.at(31), self.at(20), inst=9)
        second = write_trace(self.dir, self.at(33), self.at(20), inst=8)
        found = self.corpus().in_instance(603, 8, ms(self.at(29)), ms(self.at(40)))
        self.assertEqual([f.path for f in found], [mine, second])

    def test_in_instance_window_and_map(self):
        write_trace(self.dir, self.at(-600), self.at(-700), inst=8)    # instance id reused a day before
        inside = write_trace(self.dir, self.at(30), self.at(20), inst=8)
        write_trace(self.dir, self.at(90), self.at(20), inst=8)        # a later run in the same instance
        corpus = self.corpus()
        self.assertEqual([f.path for f in corpus.in_instance(603, 8, ms(self.at(29)), ms(self.at(31)))], [inside])
        self.assertEqual(corpus.in_instance(649, 8, ms(self.at(29)), ms(self.at(31))), [])

    def test_in_instance_opens_only_files_named_for_it(self):
        write_trace(self.dir, self.at(30), self.at(20), inst=8)
        write_trace(self.dir, self.at(30), self.at(20), inst=9)
        write_trace(self.dir, self.at(90), self.at(20), inst=8)
        corpus = self.corpus()
        with mock.patch.object(pulls, "read_facts", wraps=pulls.read_facts) as read:
            corpus.in_instance(603, 8, ms(self.at(29)), ms(self.at(31)))
        self.assertEqual([call.args[0].name.split("_")[1] for call in read.call_args_list], ["8"])

    def test_rescan_finds_new_files(self):
        corpus = self.corpus()
        self.assertEqual(corpus.in_instance(603, 8, ms(self.at(29)), ms(self.at(31))), [])
        path = write_trace(self.dir, self.at(30), self.at(20), inst=8)
        self.assertEqual(corpus.in_instance(603, 8, ms(self.at(29)), ms(self.at(31))), [])
        corpus.rescan()
        self.assertEqual([f.path for f in corpus.in_instance(603, 8, ms(self.at(29)), ms(self.at(31)))], [path])

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
        self.assertEqual(facts.death_rows, [pulls.DeathRow(4000, "Mendy", "heal", "spell 62016")])

    def test_a_closed_trace_is_read_once_across_corpora(self):
        path = write_trace(self.dir, self.at(30), self.at(20), records=[
            {"e": "death", "t": 4000, "g": HEALER, "killer": 4294967296 + 5, "cause": None,
             "blow": [4294967296 + 5, 9000], "rewind": [[3000, 4294967296 + 5, 0, 9000]]}])
        first = self.corpus()
        fresh = first.facts(path)
        first.flush()
        with mock.patch.object(pulls, "read_facts", wraps=pulls.read_facts) as read:
            cached = self.corpus().facts(path)
        read.assert_not_called()
        self.assertEqual(cached, fresh)
        self.assertEqual(cached.death_rows[0].mechanic, "melee from #1:5")

    def test_a_rewritten_trace_or_changed_code_is_read_again(self):
        path = write_trace(self.dir, self.at(30), self.at(20))
        corpus = self.corpus()
        corpus.facts(path)
        corpus.flush()
        write_trace(self.dir, self.at(30), self.at(20), length_ms=900000)
        with mock.patch.object(pulls, "read_facts", wraps=pulls.read_facts) as read:
            self.assertEqual(self.corpus().facts(path).end_ms - ms(self.at(30)), 900000)
        self.assertEqual(read.call_count, 1)
        with mock.patch.object(pulls, "code_stamp", return_value="other"), \
                mock.patch.object(pulls, "read_facts", wraps=pulls.read_facts) as read:
            self.corpus().facts(path)
        self.assertEqual(read.call_count, 1)

    def test_an_open_trace_is_not_cached(self):
        path = write_trace(self.dir, self.at(30), self.at(20), closed=False)
        corpus = self.corpus()
        corpus.facts(path)
        corpus.flush()
        self.assertEqual(list(self.cache.rglob("*.json")), [])


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


class DcInterferenceTest(unittest.TestCase):
    def facts(self, veto_at: int | None) -> pulls.TraceFacts:
        vetoes = [] if veto_at is None else [pulls.Veto(veto_at, "Uldxay", "dungeon clear", "dps assist")]
        return pulls.TraceFacts(path=pathlib.Path("a"), encounter="xt-002",
                                built=datetime.datetime(2026, 11, 21, 18), diff=1, warnings=[], hardmode=True,
                                start_ms=100_000, end_ms=300_000, outcome="wipe", closed=True, vetoes=vetoes)

    def test_a_veto_in_the_fight_voids_the_pull(self):
        self.assertTrue(self.facts(110_000).dc_interfered)
        self.assertFalse(self.facts(110_000).valid(1))

    def test_a_veto_before_the_pull_does_not(self):
        self.assertFalse(self.facts(90_000).dc_interfered)
        self.assertTrue(self.facts(90_000).valid(1))
        self.assertTrue(self.facts(None).valid(1))


class FreezeTest(unittest.TestCase):
    def facts(self, gaps: list[int]) -> pulls.TraceFacts:
        return pulls.TraceFacts(path=pathlib.Path("a"), encounter="northrend-beasts",
                                built=datetime.datetime(2026, 11, 21, 18), diff=3, warnings=[], hardmode=None,
                                start_ms=100_000, end_ms=300_000, outcome="kill", closed=True, snap_gaps=gaps)

    def test_a_freeze_in_the_fight_voids_the_pull(self):
        facts = self.facts([250, 6857, 240])
        self.assertEqual(facts.froze, 6857)
        self.assertFalse(facts.valid(3))

    def test_load_gaps_do_not(self):
        facts = self.facts([250, 909, 450, pulls.FREEZE_MS])
        self.assertEqual(facts.froze, 0)
        self.assertTrue(facts.valid(3))
        self.assertTrue(self.facts([]).valid(3))


class CauseTest(unittest.TestCase):
    def test_causes(self):
        class FakeTrace:
            def name(self, guid):
                return {1: "Bot", 2: "Boss"}.get(guid, "-")

            def spell(self, spell):
                return {62188: "Biting Cold 62188"}.get(spell, f"spell {spell}")

        trace = FakeTrace()
        self.assertEqual(pulls.death_cause(trace, {"g": 1, "cause": "reset"}), "the wipe command")
        self.assertIn("script", pulls.death_cause(trace, {"g": 1, "cause": "self"}))
        self.assertEqual(pulls.death_cause(trace, {"g": 1, "blow": [1, 500]}), "its own blow: a fall, lava or .die")
        self.assertEqual(pulls.death_cause(trace, {"g": 1, "killer": 2, "blow": [2, 500]}), "Boss")
        biting_cold = {"g": 1, "killer": 1, "blow": [1, 1087],
                       "rewind": [[180935, 2, 64545, 2716], [181924, 1, 62188, 1087]]}
        self.assertEqual(pulls.death_cause(trace, biting_cold), "its own Biting Cold 62188")
        fall = {"g": 1, "killer": 1, "blow": [1, 30000], "rewind": [[181924, 1, 62188, 1087]]}
        self.assertEqual(pulls.death_cause(trace, fall), "its own blow: a fall, lava or .die")

    def test_mechanics(self):
        class FakeTrace:
            def name(self, guid):
                return {1: "Bot", 2: "Boss"}.get(guid, "-")

            def spell(self, spell):
                return "melee" if not spell else {62188: "Biting Cold 62188"}.get(spell, f"spell {spell}")

        trace = FakeTrace()
        self.assertEqual(pulls.death_mechanic(trace, {"g": 1, "cause": "reset"}), "the wipe command")
        self.assertEqual(pulls.death_mechanic(trace, {"g": 1, "cause": "self"}), "a script kill")
        self.assertEqual(pulls.death_mechanic(trace, {"g": 1, "blow": [1, 500]}), "its own blow")
        self.assertEqual(pulls.death_mechanic(trace, {"g": 1, "killer": 2, "blow": [2, 500]}), "killed by Boss")
        biting_cold = {"g": 1, "killer": 1, "blow": [1, 1087], "rewind": [[181924, 1, 62188, 1087]]}
        self.assertEqual(pulls.death_mechanic(trace, biting_cold), "its own Biting Cold 62188")
        swing = {"g": 1, "killer": 2, "blow": [2, 9000], "rewind": [[1, 2, 62188, 10], [2, 2, 0, 9000]]}
        self.assertEqual(pulls.death_mechanic(trace, swing), "melee from Boss")
        spell = {"g": 1, "killer": 2, "blow": [2, 9000], "rewind": [[2, 2, 62999, 9000]]}
        self.assertEqual(pulls.death_mechanic(trace, spell), "spell 62999")


if __name__ == "__main__":
    unittest.main()

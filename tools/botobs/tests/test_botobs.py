"""Tests for the pure parts of the trace toolchain.

    python -m unittest discover -s tools/botobs/tests

stdlib unittest rather than pytest: nothing else in this repo declares a Python dependency, and a
test suite that needs an install is one nobody runs before a commit.

What is worth pinning here is the arithmetic that a reader has no way to sanity-check by eye - churn
counting, which shape a probe key is, whether a boss slug reaches its encounter - plus the invariant
checks, which have to be shown failing on bad input or they only prove the fixture is quiet.
"""
from __future__ import annotations

import json
import pathlib
import sys
import tempfile
import unittest

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from coverage import bucket, coverage_metrics  # noqa: E402
from metrics import Side, compare  # noqa: E402
from obstrace import Trace, boss_key, canonical_boss, pull_time  # noqa: E402
from probes import (  # noqa: E402
    HOLDER, LATCH, Series, latch_windows, prefix_matches_boss, resolve_guids,
)
from views import verify_checks  # noqa: E402

FIXTURE = BOTOBS / "fixtures" / "coverage-v12.ndjson"


class BossIdentity(unittest.TestCase):
    def test_council_members_share_an_encounter(self):
        for slug in ("stormcaller-brundir", "runemaster-molgeim", "steelbreaker", "assembly-of-iron"):
            self.assertEqual(canonical_boss(slug), "iron-assembly", slug)

    def test_elders_are_freya(self):
        self.assertEqual(canonical_boss("elder-stonebark"), "freya")

    def test_both_spellings_of_xt_agree(self):
        self.assertEqual(canonical_boss("xt002"), canonical_boss("xt-002-deconstructor"))

    def test_an_unmapped_slug_is_its_own_encounter(self):
        self.assertEqual(canonical_boss("thorim"), "thorim")

    def test_boss_key_reads_the_filename(self):
        self.assertEqual(boss_key(pathlib.Path("603_1_stormcaller-brundir_1788810873.ndjson")),
                         "iron-assembly")

    def test_pull_time_reads_the_epoch(self):
        when = pull_time(pathlib.Path("603_1_mimiron_1789144166.ndjson"))
        self.assertIsNotNone(when)
        self.assertEqual((when.year, when.month, when.day), (2026, 9, 11))

    def test_pull_time_declines_a_name_it_cannot_read(self):
        self.assertIsNone(pull_time(pathlib.Path("notatrace.ndjson")))


class ProbePrefixes(unittest.TestCase):
    def test_initials_reach_the_slug(self):
        self.assertTrue(prefix_matches_boss("fl", "flame-leviathan"))

    def test_punctuation_stripped_slug_matches(self):
        self.assertTrue(prefix_matches_boss("ironassembly", "iron-assembly"))
        self.assertTrue(prefix_matches_boss("xt002", "xt-002"))

    def test_a_different_encounter_does_not_match(self):
        self.assertFalse(prefix_matches_boss("thorim", "mimiron"))
        self.assertFalse(prefix_matches_boss("fl", "freya"))


class GuidSubstitution(unittest.TestCase):
    class FakeTrace:
        names = {4294968722: "Leviathan MK II", 12: "Shortcounter"}

        def name(self, guid):
            return self.names.get(guid, str(guid))

    def test_a_guid_inside_a_payload_is_named(self):
        trace = self.FakeTrace()
        self.assertEqual(resolve_guids(trace, "slot 3 orb 4294968722"), "slot 3 orb Leviathan MK II")

    def test_a_short_counter_is_left_alone(self):
        # A slot index and a player's guid key are both bare small integers, so anything under four
        # digits stays as written even when it happens to collide with a known guid.
        trace = self.FakeTrace()
        self.assertEqual(resolve_guids(trace, "slot 12 bearing 239"), "slot 12 bearing 239")


class Churn(unittest.TestCase):
    def rows(self, *values, guid=1, step=1000):
        return [(index * step, guid, value) for index, value in enumerate(values)]

    def test_repeats_are_not_changes(self):
        series = Series("k", HOLDER, self.rows("a", "a", "a"))
        self.assertEqual(series.changes, 0)
        self.assertEqual(series.flips, 0)

    def test_a_flip_is_a_value_coming_back(self):
        series = Series("k", HOLDER, self.rows("a", "b", "a"))
        self.assertEqual(series.changes, 2)
        self.assertEqual(series.flips, 1)
        self.assertEqual(series.flip_pairs[("a", "b")], 1)

    def test_a_one_way_walk_never_flips(self):
        series = Series("k", HOLDER, self.rows("a", "b", "c", "d"))
        self.assertEqual(series.changes, 3)
        self.assertEqual(series.flips, 0)

    def test_holders_keep_their_own_trajectories(self):
        # Two bots each holding one value are not one bot changing its mind, which is the whole
        # difference between a per-guid key and a raid-wide latch.
        rows = self.rows("a", guid=1) + self.rows("b", guid=2)
        self.assertEqual(Series("k", HOLDER, rows).changes, 0)

    def test_a_latch_reads_the_global_order(self):
        rows = [(0, 1, "a"), (1000, 2, "b"), (2000, 3, "a")]
        series = Series("k", LATCH, rows)
        self.assertEqual(series.changes, 2)
        self.assertEqual(series.flips, 1)

    def test_mean_hold_is_the_time_between_changes(self):
        series = Series("k", HOLDER, self.rows("a", "b", "c", step=2000))
        self.assertEqual(series.mean_hold_ms, 2000)


class LatchWindows(unittest.TestCase):
    class FakeTrace:
        def __init__(self, marks):
            self.marks = marks

        def of(self, *events):
            return [{"e": "note", "k": "p.phase", "t": t, "txt": v} for t, v in self.marks]

    def test_a_window_runs_to_the_next_mark(self):
        trace = self.FakeTrace([(0, "1"), (500, "2"), (900, "1")])
        spans = latch_windows(trace, "p.phase", "1")
        self.assertEqual(spans[0], (0, 500))
        self.assertEqual(spans[1][0], 900)

    def test_a_value_never_held_has_no_window(self):
        trace = self.FakeTrace([(0, "1")])
        self.assertEqual(latch_windows(trace, "p.phase", "4"), [])


class Buckets(unittest.TestCase):
    def entry(self, fired_by=(), **counts):
        base = {c: 0 for c in ("checks", "fires", "pushes", "won", "shared", "throttled",
                               "minimal", "dead")}
        base.update(counts)
        base["fired_by"] = set(fired_by)
        return base

    def test_a_node_only_a_couple_of_bots_fire_is_thin(self):
        self.assertEqual(bucket(self.entry(fired_by=(1,), checks=9, fires=9, won=9), 10), "THIN")

    def test_a_node_the_whole_raid_fires_is_ok(self):
        self.assertEqual(bucket(self.entry(fired_by=range(10), checks=9, fires=9, won=9), 10), "ok")

    def test_a_dead_name_outranks_everything(self):
        self.assertEqual(bucket(self.entry(dead=1, won=99, checks=99), 10), "DEAD")

    def test_fired_but_never_ran_is_lost(self):
        self.assertEqual(bucket(self.entry(checks=9, fires=9, pushes=9), 10), "LOST")

    def test_asked_and_never_true_is_never(self):
        self.assertEqual(bucket(self.entry(checks=9), 10), "NEVER")

    def test_never_asked_is_throttled(self):
        self.assertEqual(bucket(self.entry(throttled=9), 10), "THROT")


class Comparison(unittest.TestCase):
    def side(self, label, *values):
        return Side(label, [{"metrics": {"m": v}} for v in values])

    def test_disjoint_ranges_have_moved(self):
        finding = compare(self.side("a", 1.0, 2.0), self.side("b", 8.0, 9.0))[0]
        self.assertTrue(finding["moved"])

    def test_overlapping_ranges_have_not(self):
        finding = compare(self.side("a", 1.0, 9.0), self.side("b", 2.0, 8.0))[0]
        self.assertFalse(finding["moved"])

    def test_a_metric_on_one_side_only_is_reported_as_such(self):
        before = Side("a", [{"metrics": {"gone": 3.0}}])
        after = Side("b", [{"metrics": {"new": 3.0}}])
        only = {f["key"]: f["only"] for f in compare(before, after)}
        self.assertEqual(only, {"gone": "before", "new": "after"})

    def test_zero_on_both_sides_is_not_a_finding(self):
        self.assertEqual(compare(self.side("a", 0.0, 0.0), self.side("b", 0.0, 0.0)), [])


class Fixture(unittest.TestCase):
    def setUp(self):
        self.trace = Trace(FIXTURE)

    def test_every_invariant_passes(self):
        failures = [(label, count) for label, count, _ in verify_checks(self.trace) if count]
        self.assertEqual(failures, [])

    def test_the_fixture_exercises_every_bucket(self):
        counts = coverage_metrics(self.trace)
        for tag in ("dead", "never", "lost", "thin", "throt", "ok"):
            self.assertIn(f"cov.{tag}", counts, tag)

    def corrupt(self, mangle) -> Trace:
        lines = FIXTURE.read_text(encoding="utf-8").splitlines()
        out = [json.dumps(mangle(json.loads(line))) for line in lines if line.strip()]
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / "broken.ndjson"
            path.write_text("\n".join(out) + "\n", encoding="utf-8")
            return Trace(path)

    def failing(self, trace) -> list[str]:
        return [label for label, count, _ in verify_checks(trace) if count]

    def test_an_undefined_coverage_id_is_caught(self):
        # Proves the check bites rather than that the fixture is quiet: without this, a check that
        # never fails and a check that cannot fail look the same.
        def mangle(rec):
            if rec.get("e") == "cov":
                rec["r"] = [[999] + row[1:] for row in rec["r"]]
            return rec

        self.assertTrue(self.failing(self.corrupt(mangle)))

    def test_more_fires_than_checks_is_caught(self):
        def mangle(rec):
            if rec.get("e") == "cov" and rec["r"]:
                row = rec["r"][0]
                while len(row) < 3:
                    row.append(0)
                row[1], row[2] = 1, 9999
            return rec

        self.assertTrue(self.failing(self.corrupt(mangle)))


if __name__ == "__main__":
    unittest.main()

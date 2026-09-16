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

import argparse  # noqa: E402
import contextlib  # noqa: E402
import datetime  # noqa: E402
import io  # noqa: E402
import math  # noqa: E402
from unittest import mock  # noqa: E402

import postmortem  # noqa: E402
from bosses import flame_leviathan, yogg_saron  # noqa: E402
from raidobs import (  # noqa: E402
    corpus, coverage, deathreport, encounter, geometry, probes, space, stuck, timeline, validity, verify,
)
from raidobs.corpus import pull_time  # noqa: E402
from raidobs.coverage import bucket, coverage_metrics  # noqa: E402
from raidobs.encounter import boss_key, canonical_boss, prefix_matches_boss, recover_boss  # noqa: E402
from raidobs.metrics import Side, compare  # noqa: E402
from raidobs.probes import (  # noqa: E402
    HOLDER, LATCH, Series, declared_keys, latch_spans, latch_windows, resolve_guids,
)
from raidobs.stuck import idle_windows  # noqa: E402
from raidobs.trace import Trace, combat_deaths, death_records  # noqa: E402
from raidobs.verify import verify_checks  # noqa: E402

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

    def test_a_restated_value_does_not_split_its_window(self):
        # A traced container writes its state again when a trace opens, which is not a change.
        trace = self.FakeTrace([(0, "1"), (300, "1"), (500, "2")])
        self.assertEqual(latch_windows(trace, "p.phase", "1"), [(0, 500)])


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


FULL = BOTOBS / "fixtures" / "full-v12.ndjson"


def rich() -> Trace:
    return Trace(FULL)


class Geometry(unittest.TestCase):
    def test_dist2_ignores_height(self):
        self.assertAlmostEqual(geometry.dist2((0, 0, 100), (3, 4, -50)), 5.0)

    def test_edge_is_signed(self):
        self.assertAlmostEqual(geometry.edge((0, 0), [(0, 0, 10)]), -10.0)
        self.assertAlmostEqual(geometry.edge((20, 0), [(0, 0, 10)]), 10.0)

    def test_edge_takes_the_nearest_rim_not_the_nearest_centre(self):
        # A far circle with a wide radius can have the closer edge, which is the whole point.
        self.assertAlmostEqual(geometry.edge((0, 0), [(8, 0, 1), (30, 0, 29)]), 1.0)

    def test_edge_of_nothing(self):
        self.assertEqual(geometry.edge((0, 0), []), math.inf)

    def test_nearest_picks_the_closest(self):
        self.assertEqual(geometry.nearest((0, 0), {"far": (9, 0), "near": (2, 0)}), ("near", 2.0))

    def test_nearest_of_nothing(self):
        self.assertIsNone(geometry.nearest((0, 0), {}))

    def test_at_before_never_looks_forward(self):
        trace = rich()
        # The boss sits still, so what is being pinned is the policy, not the coordinates.
        self.assertIsNotNone(geometry.at(trace, 5001, 500))
        self.assertIsNone(geometry.at(trace, 5001, -9999))

    def test_at_nearest_respects_tolerance(self):
        trace = rich()
        self.assertIsNone(geometry.at(trace, 5001, 500000, geometry.NEAREST, tol=10))
        self.assertIsNotNone(geometry.at(trace, 5001, 500000, geometry.NEAREST))

    def test_at_unknown_guid(self):
        self.assertIsNone(geometry.at(rich(), 987654321, 0))

    def test_track_skips_rows_too_short_for_the_column(self):
        trace = rich()
        for snap in trace.of("snap"):
            snap["u"] = [row[:6] for row in snap["u"]]
        trace.__dict__.pop("_geom_frames", None)
        trace.__dict__.pop("_geom_index", None)
        self.assertEqual(geometry.track(trace, {5001}, ("t", "target")), {})
        self.assertTrue(geometry.track(trace, {5001}, ("t", "x", "y")))

    def test_anchors_and_radii_come_out_of_the_raid_tree(self):
        self.assertEqual(geometry.anchor("ULDUAR_YOGG_SARON_MIDDLE")[:2], (1980.28, -25.5868))
        self.assertAlmostEqual(geometry.radius("ULDUAR_YOGG_SARON_P1_LEASH"), 6.5)

    def test_a_unique_suffix_resolves(self):
        self.assertEqual(geometry.anchor("YOGG_SARON_MIDDLE"),
                         geometry.anchor("ULDUAR_YOGG_SARON_MIDDLE"))

    def test_an_ambiguous_suffix_names_the_candidates(self):
        with self.assertRaises(geometry.Unknown) as caught:
            geometry.anchor("MIDDLE")
        self.assertIn("ULDUAR_YOGG_SARON_MIDDLE", str(caught.exception))

    def test_an_unknown_name_is_not_silently_zero(self):
        with self.assertRaises(geometry.Unknown):
            geometry.anchor("NO_SUCH_ANCHOR_ANYWHERE")

    def declared_in(self, folder: str, **files: str) -> pathlib.Path:
        root = pathlib.Path(folder)
        for name, text in files.items():
            (root / f"{name}.h").write_text(text, encoding="utf-8")
        return root

    def test_every_way_the_tree_writes_a_position_is_read(self):
        with tempfile.TemporaryDirectory() as folder:
            root = self.declared_in(folder, spots=(
                "const Position CALL_SPOT = Position(1.0f, 2.0f, 3.0f);\n"
                "const Position BRACE_SPOT =\n    { 1.5f, -2.0f, 3.0f };\n"
                "inline Position const FLIPPED_SPOT = { 4.0f, 5.0f, 6.0f, 0.5f };\n"
                "static const Position DIRECT_SPOT{ 7, 8, 9 };\n"
                "const Position ROW_SPOTS[2] = { { 1.0f, 2.0f, 3.0f }, { 4.0f, 5.0f, 6.0f } };\n"))
            self.assertEqual(geometry.anchor("CALL_SPOT", root), (1.0, 2.0, 3.0))
            self.assertEqual(geometry.anchor("BRACE_SPOT", root), (1.5, -2.0, 3.0))
            self.assertEqual(geometry.anchor("FLIPPED_SPOT", root), (4.0, 5.0, 6.0))
            self.assertEqual(geometry.anchor("DIRECT_SPOT", root), (7.0, 8.0, 9.0))
            with self.assertRaises(geometry.Unknown):
                geometry.anchor("ROW_SPOTS", root)

    def test_a_name_two_files_disagree_on_is_refused(self):
        # Neither file is more right than the other, so neither value gets to win.
        with tempfile.TemporaryDirectory() as folder:
            root = self.declared_in(folder, lich="constexpr float LEASH = 40.0f;\n",
                                    valithria="constexpr float LEASH = 35.0f;\n",
                                    twins="constexpr float SAME = 5.0f;\n",
                                    twins_copy="constexpr float SAME = 5.0f;\n")
            with self.assertRaises(geometry.Unknown) as caught:
                geometry.radius("LEASH", root)
            self.assertIn("40.0", str(caught.exception))
            self.assertIn("35.0", str(caught.exception))
            self.assertAlmostEqual(geometry.radius("SAME", root), 5.0)

    def test_an_entry_several_creatures_share_names_no_point(self):
        trace = rich()
        self.assertEqual(geometry.reference("entry:33999", trace)[:2], (0.0, 0.0))
        trace.entries[5001] = 33999
        with self.assertRaises(geometry.Unknown):
            geometry.reference("entry:33999", trace)


class Spatial(unittest.TestCase):
    def test_event_spots_reads_a_cast_as_its_casters_position(self):
        found = space.event_spots(rich(), "cast:100")
        self.assertEqual([guid for _, guid, _ in found], [5003])

    def test_event_spots_reads_deaths(self):
        self.assertEqual(len(space.event_spots(rich(), "death")), 1)

    def test_event_spots_reads_notes(self):
        self.assertEqual(len(space.event_spots(rich(), "note:fixture.role")), 2)

    def test_event_spots_rejects_an_unknown_stream(self):
        with self.assertRaises(geometry.Unknown):
            space.event_spots(rich(), "nonsense")

    def test_band_takes_a_bare_number(self):
        self.assertAlmostEqual(space.band_of("15"), 15.0)
        self.assertAlmostEqual(space.band_of("ULDUAR_YOGG_SARON_P1_LEASH"), 6.5)
        self.assertIsNone(space.band_of(None))

    def test_scope_narrows_to_a_latch_value(self):
        trace = rich()
        inside = space.scope(trace, "fixture.phase=2")
        self.assertFalse(inside(0))
        self.assertTrue(inside(2200))

    def test_scope_of_a_value_nothing_held(self):
        self.assertIsNone(space.scope(rich(), "fixture.phase=9"))

    def test_clump_counts_only_the_frames_in_scope(self):
        trace = rich()
        self.assertEqual(dict(space.clump_histogram(trace, 10.0)), {2: 4})
        inside = space.scope(trace, "fixture.phase=2")
        self.assertEqual(dict(space.clump_histogram(trace, 10.0, inside)), {2: 1})

    def test_scope_without_during_drops_the_pre_roll(self):
        inside = space.scope(rich(), None)
        self.assertFalse(inside(-1))
        self.assertTrue(inside(0))

    def test_move_rows_join_an_origin_onto_a_destination(self):
        trace = rich()
        rows = space.move_rows(trace, (0.0, 0.0, 0.0), lambda when: when >= 0)
        movers = {row[0] for row in rows}
        self.assertEqual(movers, {"flee", "reach melee"})
        # A move record carries only where it was going; the start radius has to come from a snapshot.
        for row in rows:
            self.assertGreater(row[2], 0.0)

    def test_role_held_separates_a_pet_from_an_unknown(self):
        trace = rich()
        self.assertEqual(space.role_held(trace, 5001), "tank")
        self.assertEqual(space.role_held(trace, 5005), "pet")
        self.assertEqual(space.role_held(trace, 0), "nobody")
        self.assertEqual(space.role_held(trace, 111222333), "other")

    def test_the_raids_own_pets_are_not_hostiles(self):
        trace = rich()
        for snap in trace.of("snap"):
            for row in snap["u"]:
                if row[0] == 5005:
                    row[7] = 4294967400
        _, per_unit = space.threat_share(trace, None, lambda when: when >= 0)
        self.assertNotIn(5005, per_unit)
        self.assertIn(4294967400, per_unit)

    def test_a_frame_counts_until_the_next_snapshot_not_the_next_one_in_scope(self):
        # Snapshots at 0, 1000, 2000 and 2500. Scoped to 0 and 2000 only, the frame at 0 holds for
        # 1000 ms and not for the 2000 ms up to the next frame that passed the scope.
        held, _ = space.threat_share(rich(), None, lambda when: when in (0, 2000))
        self.assertEqual(dict(held), {"tank": 1500, "ranged": 1000})


class Decidability(unittest.TestCase):
    def test_a_human_role_always_decides(self):
        self.assertIn("human-role", validity.decidable_kinds())

    def test_the_build_only_decides_when_a_ref_is_named(self):
        self.assertNotIn("stale-build", validity.decidable_kinds())
        self.assertIn("stale-build", validity.decidable_kinds(since="HEAD"))

    def test_hard_mode_only_decides_when_asked_for(self):
        self.assertNotIn("hardmode-off", validity.decidable_kinds())
        self.assertIn("hardmode-off", validity.decidable_kinds(hardmode=True))

    def test_a_since_that_does_not_resolve_disqualifies(self):
        with contextlib.redirect_stdout(io.StringIO()) as banner:
            counted = validity.show_validity(rich(), "HEAD~no-such-ref")
        self.assertEqual(counted, 1)
        self.assertIn("cannot resolve", banner.getvalue())

    def test_a_time_without_an_offset_is_read_as_local(self):
        # build and pull stamps are aware, so a naive ref can't be compared with either
        _, when = validity.resolve_since(validity.REPO, "2026-09-10T00:00:00")
        self.assertEqual(when, datetime.datetime(2026, 9, 10).astimezone())
        with contextlib.redirect_stdout(io.StringIO()) as banner:
            validity.show_validity(rich(), "2026-09-10T00:00:00")
        self.assertIn("after given", banner.getvalue())

    def dead_at_open(self, dead):
        trace = rich()
        for snap in trace.of("snap"):
            for row in snap["u"]:
                if row[0] in dead:
                    row[5] = 0.0
        return [kind for kind, _ in validity.inspect(trace, None)[1]]

    def test_a_raid_dead_when_the_trace_opened_always_decides(self):
        self.assertIn("raid-dead", validity.decidable_kinds())
        self.assertIn("raid-dead", self.dead_at_open({5002, 5003, 5004}))

    def test_half_the_raid_dead_is_not_yet_nobody_pulling(self):
        # The recorder's wipe line is more than half, so the two agree on what counts.
        self.assertNotIn("raid-dead", self.dead_at_open(set()))
        self.assertNotIn("raid-dead", self.dead_at_open({5003, 5004}))

    def test_a_time_with_an_offset_keeps_it(self):
        _, when = validity.resolve_since(validity.REPO, "2026-09-10T00:00:00+00:00")
        self.assertEqual(when, datetime.datetime(2026, 9, 10, tzinfo=datetime.timezone.utc))


class Recovery(unittest.TestCase):
    """A pull nothing renamed is filed under the map, and has to be found by its units instead."""

    def test_the_engaged_boss_is_the_one_that_traded_damage(self):
        trace = rich()
        trace.header["boss"] = "ulduar"
        self.assertEqual(encounter.engaged_of(trace), "fixture-boss")

    def test_a_rename_outranks_the_units(self):
        trace = rich()
        trace.records.append({"t": 5, "e": "pull", "boss": "named-by-rename", "src": "rename"})
        self.assertEqual(encounter.encounter_of(trace), "named-by-rename")

    def test_a_pull_filed_under_the_map_is_named_by_its_units(self):
        trace = rich()
        trace.header["boss"] = "ulduar"
        self.assertEqual(encounter.encounter_of(trace), "fixture-boss")

    def test_a_pull_filed_under_its_encounter_keeps_that_name(self):
        # Mimiron's pull is filed `mimiron` and never renamed, while the first boss-flagged unit to
        # trade damage is Leviathan Mk II. The units must not overrule a name that was never the map's.
        trace = rich()
        self.assertEqual(trace.header["boss"], "fixtureboss")
        self.assertEqual(encounter.encounter_of(trace), "fixtureboss")

    def test_find_traces_opens_only_a_file_filed_under_the_map(self):
        with tempfile.TemporaryDirectory() as folder:
            unnamed = pathlib.Path(folder) / "603_4_ulduar_1789500000.ndjson"
            other = pathlib.Path(folder) / "603_4_thorim_1789500001.ndjson"
            for path in (unnamed, other):
                path.write_bytes(FULL.read_bytes())
            with mock.patch.object(corpus, "recover_boss", wraps=encounter.recover_boss) as opened:
                found = corpus.find_traces([folder], "fixture-boss")
            self.assertEqual([path.name for path in found], [unnamed.name])
            self.assertEqual([call.args[0].name for call in opened.call_args_list], [unnamed.name])

    def test_the_yogg_probe_check_does_not_depend_on_the_filed_name(self):
        trace = rich()
        trace.header["boss"] = "ulduar"
        self.assertIn("yogg.phase", yogg_saron.missing_probes(trace))

    def test_recover_boss_reads_it_off_disk(self):
        self.assertEqual(recover_boss(FULL), "fixture-boss")

    def test_recover_boss_on_a_file_that_is_not_one(self):
        with tempfile.TemporaryDirectory() as folder:
            junk = pathlib.Path(folder) / "junk.ndjson"
            junk.write_text("not json at all\n", encoding="utf-8")
            self.assertEqual(recover_boss(junk), "")


class Prefixes(unittest.TestCase):
    def test_the_first_word_of_a_slug_counts(self):
        # Otherwise no yogg.* key matches its own boss.
        self.assertTrue(prefix_matches_boss("yogg", "yogg-saron"))

    def test_the_other_two_forms_still_count(self):
        self.assertTrue(prefix_matches_boss("ironassembly", "iron-assembly"))
        self.assertTrue(prefix_matches_boss("fl", "flame-leviathan"))

    def test_an_unrelated_prefix_does_not(self):
        self.assertFalse(prefix_matches_boss("thorim", "yogg-saron"))


class Declarations(unittest.TestCase):
    def test_a_key_on_the_line_after_its_call_is_still_declared(self):
        self.assertIn("yogg.deathray", declared_keys())

    def test_a_key_nothing_declares_is_absent(self):
        self.assertNotIn("yogg.nothing", declared_keys())


class LatchAllValues(unittest.TestCase):
    def test_latch_spans_keeps_every_value_in_order(self):
        spans = latch_spans(rich(), "fixture.phase", end=9999)
        self.assertEqual([value for value, _, _ in spans], ["1", "2"])
        self.assertEqual(spans[0][1], -1000)
        self.assertEqual(spans[-1][2], 9999)

    def test_latch_windows_is_the_filtered_form(self):
        self.assertEqual(latch_windows(rich(), "fixture.phase", "1")[0][0], -1000)


class Ranking(unittest.TestCase):
    """What `--split-at` is allowed to call a move."""

    @staticmethod
    def sides(before_rows, after_rows):
        return (Side("before", [{"metrics": m} for m in before_rows]),
                Side("after", [{"metrics": m} for m in after_rows]))

    def find(self, findings, key):
        return next(f for f in findings if f["key"] == key)

    def test_a_change_under_the_printed_precision_is_not_a_move(self):
        before, after = self.sides([{"k": 0.144}, {"k": 0.144}], [{"k": 0.132}, {"k": 0.132}])
        self.assertFalse(self.find(compare(before, after), "k")["moved"])

    def test_a_real_shift_still_is(self):
        before, after = self.sides([{"k": 10.0}, {"k": 11.0}], [{"k": 1.0}, {"k": 2.0}])
        self.assertTrue(self.find(compare(before, after), "k")["moved"])

    def test_a_stream_in_a_minority_of_pulls_is_not_a_finding(self):
        before, after = self.sides([{"k": 5.0}, {}, {}, {}], [{}])
        self.assertEqual([f for f in compare(before, after) if f["key"] == "k"], [])

    def test_a_stream_in_most_pulls_is(self):
        before, after = self.sides([{"k": 5.0}, {"k": 6.0}, {"k": 5.5}], [{}])
        self.assertEqual(self.find(compare(before, after), "k")["only"], "before")

    def test_a_one_sided_zero_is_not_a_finding(self):
        before, after = self.sides([{"k": 0.0}, {"k": 0.0}], [{}])
        self.assertEqual([f for f in compare(before, after) if f["key"] == "k"], [])

    def test_a_value_only_one_of_two_pulls_carries_is_not_a_move(self):
        # Only one of the two pulls reached the phase that writes the key, so its side is one value.
        before, after = self.sides([{"k": 1.0}, {"k": 2.0}, {"k": 1.5}], [{"k": 9.0}, {}])
        self.assertFalse(self.find(compare(before, after), "k")["moved"])

    def test_the_same_shift_in_every_pull_still_is(self):
        before, after = self.sides([{"k": 1.0}, {"k": 2.0}, {"k": 1.5}], [{"k": 9.0}, {"k": 8.0}])
        self.assertTrue(self.find(compare(before, after), "k")["moved"])


class YoggPhases(unittest.TestCase):
    def test_stun_window_ends_at_induce_madness_or_phase_3(self):
        # 1789568759: wave 1 ran its full stun, wave 6 lost it to phase 3 at +49.8 s.
        self.assertEqual(yogg_saron.stun_window(192654, 224354, 644834), (224354, 252654))
        self.assertEqual(yogg_saron.stun_window(595034, 623820, 644834), (623820, 644834))
        self.assertEqual(yogg_saron.stun_window(595034, 623820, None), (623820, 655034))
        self.assertIsNone(yogg_saron.stun_window(595034, 623820, 620000))

    def test_hp_removed_counts_drops_inside_the_window_only(self):
        samples = [(0, 100.0), (1000, 90.0), (2000, 95.0), (3000, 80.0), (4000, 50.0)]
        # The drop across the start counts from the sample before it, a rise counts for nothing, and
        # anything after the end is left out.
        self.assertAlmostEqual(yogg_saron.hp_removed(samples, 1000, 500, 3000), 250.0)
        self.assertAlmostEqual(yogg_saron.hp_removed(samples, 1000, 0, 4000), 550.0)
        self.assertEqual(yogg_saron.hp_removed(samples, 1000, 5000, 6000), 0.0)

    def test_closest_approach_clamps_to_the_walk(self):
        # 1789581096's phase 3 opening: surfaced west of Yogg, walked to the melee spot east of him.
        self.assertLess(yogg_saron.closest_approach((1924.7, -25.4), (1998.54, -22.9), yogg_saron.BODY), 2.1)
        # A walk that stops short is judged from its end, not from the line carried on.
        self.assertAlmostEqual(yogg_saron.closest_approach((0.0, 0.0), (10.0, 0.0), (20.0, 0.0)), 10.0)
        self.assertAlmostEqual(yogg_saron.closest_approach((5.0, 5.0), (5.0, 5.0), (8.0, 9.0)), 5.0)

    def test_a_body_launch_is_the_switch_to_effect_motion_near_the_body(self):
        body_x, body_y = yogg_saron.BODY

        def row(t, distance, z, movegen):
            return [t, 1, body_x + distance, body_y, z, 0.0, 100.0, 100.0, 0, 1, movegen, 0, 0]

        rows = [row(0, 10.0, 324.9, 8), row(223, 12.8, 327.3, 16), row(446, 15.7, 328.8, 16),
                row(669, 30.0, 324.9, 8), row(892, 30.0, 324.9, 16), row(1115, 9.0, 237.5, 8),
                row(1338, 9.0, 237.5, 16)]
        # Only the first sample of a flight, only near the body, only on the platform: the brain room
        # sits under it at z 237.
        self.assertEqual([hit[0] for hit in yogg_saron.body_launches(rows)], [223])

    def test_first_off_tank_skips_tank_victims_and_no_victim(self):
        rows = [[0, 9, 0, 0, 0, 0, 100.0, 0, 0], [100, 9, 0, 0, 0, 0, 100.0, 0, 5105],
                [200, 9, 0, 0, 0, 0, 100.0, 0, 5131], [300, 9, 0, 0, 0, 0, 100.0, 0, 5105]]
        self.assertEqual(yogg_saron.first_off_tank(rows, {5105, 256463})[0], 200)
        self.assertIsNone(yogg_saron.first_off_tank(rows[:2], {5105}))

    def test_a_taunt_is_down_for_eight_seconds_after_its_last_cast(self):
        casts = [1000, 20000]
        self.assertFalse(yogg_saron.on_cooldown(casts, 500, 8000))
        self.assertTrue(yogg_saron.on_cooldown(casts, 8999, 8000))
        self.assertFalse(yogg_saron.on_cooldown(casts, 9000, 8000))
        self.assertTrue(yogg_saron.on_cooldown(casts, 20000, 8000))

    def test_phase_one_ends_where_phase_two_starts(self):
        spans = [(1, 0, 157629), (2, 157629, 644303), (1, 644303, 648355)]
        self.assertEqual(yogg_saron.phase1_end(spans), 157629)

    def test_a_lost_opening_mark_does_not_make_the_wipe_tail_phase_one(self):
        # Sara respawns after a wipe and the latch goes back to 1, which is all that is left when the
        # trace never recorded the phase 1 it opened on.
        spans = [(2, 131198, 181581), (1, 181581, 181753)]
        self.assertEqual(yogg_saron.phase1_end(spans), 131198)

    def test_a_pull_that_never_left_phase_one(self):
        self.assertEqual(yogg_saron.phase1_end([(1, 0, 90000)]), 90000)
        self.assertIsNone(yogg_saron.phase1_end([]))

    def test_back_to_back_pairs_only_neighbours_inside_the_window(self):
        # The 2026-09-16 wipe: a same-tick pair on the melee, a 2.5 s pair on the station, then a gap.
        deaths = [(70800, 17.9), (44556, 6.2), (44573, 2.3), (68300, 16.1), (78300, 3.3)]
        self.assertEqual(yogg_saron.back_to_back(deaths),
                         [((44556, 6.2), (44573, 2.3)), ((68300, 16.1), (70800, 17.9))])
        self.assertEqual(yogg_saron.back_to_back([(0, 1.0)]), [])

    def test_kill_kind_reads_how_many_bots_were_on_the_guardian(self):
        # The 2026-09-16 17:25 pull: a focus kill, a Guardian nobody was on, and the earlier split.
        self.assertEqual(yogg_saron.kill_kind(22, 23), "focus")
        self.assertEqual(yogg_saron.kill_kind(1, 23), "splash")
        self.assertEqual(yogg_saron.kill_kind(0, 23), "splash")
        self.assertEqual(yogg_saron.kill_kind(10, 23), "split")

    def test_interpolate_puts_a_running_bot_between_its_snapshots(self):
        self.assertEqual(yogg_saron.interpolate((0, 0.0, 0.0), (200, 4.0, 2.0), 50), (1.0, 0.5))
        self.assertEqual(yogg_saron.interpolate((0, 3.0, 4.0), None, 50), (3.0, 4.0))

    def test_merge_spans_folds_one_channel_seen_on_many_raiders(self):
        # One Diminish Power channel lands on every raider a few ms apart, so the per-raider windows
        # overlap. A break and re-cast is a real gap and stays two spans.
        spans = [(1200, 5000), (1000, 5000), (1100, 4990), (6500, 9000), (9000, 9500)]
        self.assertEqual(yogg_saron.merge_spans(spans), [(1000, 5000), (6500, 9500)])
        self.assertEqual(yogg_saron.merge_spans([]), [])

    def test_group_runs_splits_on_a_missed_snapshot_gap_and_drops_blips(self):
        # Snapshots every ~250 ms; a tentacle sampled once in between splits the run, and a lone empty
        # snapshot is not a window.
        stamps = [1000, 1250, 1500, 1750, 2000, 2250, 2500, 2750, 4000, 9000]
        self.assertEqual(yogg_saron.group_runs(stamps, 600, 1500), [(1000, 2750)])
        self.assertEqual(yogg_saron.group_runs(stamps, 600, 0), [(1000, 2750), (4000, 4000), (9000, 9000)])
        self.assertEqual(yogg_saron.group_runs([], 600, 0), [])

    def test_target_split_reads_a_pack_and_a_spread(self):
        self.assertEqual(yogg_saron.target_split([7, 7, 7, 7]), (1, 1.0))
        self.assertEqual(yogg_saron.target_split([7, 7, 8, 9]), (3, 0.5))

    def test_separation_is_the_smaller_angle_either_way_round(self):
        self.assertAlmostEqual(yogg_saron.separation((0, 0), (1, 0), (0, 1)), 90.0)
        self.assertAlmostEqual(yogg_saron.separation((0, 0), (1, 0), (-1, 0)), 180.0)
        # -170 and 170 degrees are 20 apart, not 340.
        self.assertAlmostEqual(yogg_saron.separation((0, 0), (-1, -0.176327), (-1, 0.176327)), 20.0, places=3)

    def test_facing_away_puts_the_source_outside_the_front_half(self):
        # Yogg due east of a bot at the origin.
        self.assertFalse(yogg_saron.facing_away(0.0, (0, 0), (10, 0)))
        self.assertFalse(yogg_saron.facing_away(math.radians(80), (0, 0), (10, 0)))
        self.assertTrue(yogg_saron.facing_away(math.radians(100), (0, 0), (10, 0)))
        self.assertTrue(yogg_saron.facing_away(math.pi, (0, 0), (10, 0)))

    def test_target_kind_sorts_the_phase_3_targets(self):
        self.assertEqual(yogg_saron.target_kind(0, None), "none")
        self.assertEqual(yogg_saron.target_kind(5, yogg_saron.NPC_IMMORTAL_GUARDIAN), "guardian")
        self.assertEqual(yogg_saron.target_kind(5, yogg_saron.NPC_CORRUPTOR_TENTACLE), "tentacle")
        self.assertEqual(yogg_saron.target_kind(5, yogg_saron.NPC_YOGG_SARON), "yogg")
        self.assertEqual(yogg_saron.target_kind(5, 1234), "other")


class DuplicateDeaths(unittest.TestCase):
    """Yogg's Insane kills its owner when it comes off, and dying takes it off."""

    class FakeTrace:
        def __init__(self, deaths):
            self.deaths = deaths

        def of(self, *events):
            return list(self.deaths)

    def test_a_blow_and_the_real_killer_become_one_record(self):
        trace = self.FakeTrace([
            {"t": 322890, "g": 7, "killer": 7, "blow": [99, 457]},
            {"t": 322891, "g": 7, "killer": 99},
        ])
        folded = death_records(trace)
        self.assertEqual(len(folded), 1)
        self.assertEqual(folded[0]["killer"], 99)
        self.assertEqual(folded[0]["blow"], [99, 457])

    def test_a_wipe_keeps_its_cause(self):
        trace = self.FakeTrace([
            {"t": 614012, "g": 7, "killer": 7, "cause": "reset"},
            {"t": 614013, "g": 7, "killer": 7, "cause": "self"},
        ])
        self.assertEqual([d["cause"] for d in death_records(trace)], ["reset"])
        self.assertEqual(combat_deaths(trace), [])

    def test_a_real_second_death_is_kept(self):
        trace = self.FakeTrace([
            {"t": 475069, "g": 7, "killer": 99, "blow": [99, 30984]},
            {"t": 478045, "g": 7, "killer": 99, "blow": [99, 30663]},
        ])
        self.assertEqual(len(death_records(trace)), 2)

    def test_the_fold_leaves_the_record_it_read_alone(self):
        first = {"t": 10, "g": 7, "killer": 7, "blow": [99, 1]}
        death_records(self.FakeTrace([first, {"t": 11, "g": 7, "killer": 99}]))
        self.assertEqual(first["killer"], 7)


class ShortRows(unittest.TestCase):
    """Columns 8 to 11 arrived in v8, so an older row raises if one is read without checking."""

    def test_the_flame_leviathan_frame_survives_a_pre_v8_row(self):
        snap = {"t": 0, "u": [[1, 1.0, 2.0, 3.0, 0.4, 100.0]]}
        frame = flame_leviathan.Frame(snap, rich(), {1: flame_leviathan.BOSS_ENTRY},
                                      set(), {}, {})
        self.assertEqual(frame.boss, (1.0, 2.0, 0.4, 0))

    def test_idle_windows_ignore_a_row_with_no_target_column(self):
        trace = rich()
        for snap in trace.of("snap"):
            snap["u"] = [r[:6] for r in snap["u"]]
        self.assertEqual(idle_windows(trace, 1), [])


class Idle(unittest.TestCase):
    def test_the_window_starts_where_the_silence_did(self):
        # Target held from 0 to 30 s, casts at 2 s and 5 s: quiet for 25 s from 5 s, not from 0.
        rows = [{"e": "hdr", "v": 12, "boss": "idle", "roster": [{"g": 1, "n": "Still", "r": "ranged"}]}]
        rows += [{"t": when, "e": "snap", "u": [[1, 0.0, 0.0, 0.0, 0.0, 100.0, 100.0, 99, 0, 0, 0, 0]]}
                 for when in range(0, 30001, 1000)]
        rows += [{"t": when, "e": "cast", "s": 1, "sp": 100, "tgt": 99} for when in (2000, 5000)]
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / "603_4_idle_1789500000.ndjson"
            path.write_text("\n".join(json.dumps(row) for row in rows) + "\n", encoding="utf-8")
            found = idle_windows(Trace(path), 10000)
        self.assertEqual(found, [{"guid": 1, "start": 5000, "quiet": 25000}])


class Renderers(unittest.TestCase):
    """Every printing entry point, against a fixture carrying one of every record.

    These do not check what is printed - they check that it prints. An undefined name inside a
    renderer is invisible to every test of the pure functions under it, and this is what catches it.
    """

    def run_quiet(self, call):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(io.StringIO()):
            call()
        return buffer.getvalue()

    def test_every_renderer_runs(self):
        trace = rich()
        calls = {
            "show_bot": lambda: timeline.show_bot(trace, "Bulwark"),
            "show_track": lambda: timeline.show_track(trace, "Bulwark"),
            "show_notes": lambda: timeline.show_notes(trace),
            "show_notes_prefix": lambda: timeline.show_notes(trace, "fixture."),
            "show_stalls": lambda: stuck.show_stalls(trace, 1000),
            "show_idle": lambda: stuck.show_idle(trace, 1),
            "show_vetoes": lambda: stuck.show_vetoes(trace),
            "show_clump": lambda: space.show_clump(trace, 10.0),
            "show_clump_during": lambda: space.show_clump(trace, 10.0, "fixture.phase=2"),
            "show_clump_never_held": lambda: space.show_clump(trace, 10.0, "fixture.phase=9"),
            "show_verify": lambda: verify.show_verify(trace),
            "show_coverage": lambda: coverage.show_coverage(trace),
            "show_coverage_by_bot": lambda: coverage.show_coverage(trace, None, True),
            "show_probes": lambda: probes.show_probes(trace),
            "show_probes_key": lambda: probes.show_probes(trace, "fixture.phase"),
            "show_probes_during": lambda: probes.show_probes(trace, None, "fixture.phase=1"),
            "summarise": lambda: deathreport.summarise(trace),
            "show_death": lambda: deathreport.show_death(trace, 0),
            "show_threat": lambda: space.show_threat(trace),
            "show_threat_entry": lambda: space.show_threat(trace, 33999),
            "show_moves": lambda: space.show_moves(trace, None, "entry:33999"),
            "show_moves_action": lambda: space.show_moves(trace, "flee", "entry:33999", "15"),
            "show_moves_unknown_anchor": lambda: space.show_moves(trace, None, "NO_SUCH_ANCHOR"),
            "show_where_death": lambda: space.show_where(trace, "death", "entry:33999"),
            "show_where_cast": lambda: space.show_where(trace, "cast:100", "entry:33999", "15"),
            "show_where_bad_spec": lambda: space.show_where(trace, "nonsense", "entry:33999"),
            "show_validity": lambda: validity.show_validity(trace),
            "show_validity_hardmode": lambda: validity.show_validity(trace, None, True),
        }
        for name, call in calls.items():
            with self.subTest(renderer=name):
                self.assertTrue(self.run_quiet(call), f"{name} printed nothing")

    def test_the_ones_that_answer_on_stderr_still_return(self):
        trace = rich()
        self.assertEqual(self.run_quiet(lambda: timeline.show_bot(trace, "Nobody")), "")
        self.assertEqual(self.run_quiet(lambda: timeline.show_track(trace, "Nobody")), "")
        self.run_quiet(lambda: deathreport.show_death(trace, 99))

    def test_the_smoke_test_catches_a_broken_renderer(self):
        def broken():
            print(undefined_name)  # noqa: F821

        with self.assertRaises(NameError):
            self.run_quiet(broken)

    def test_a_renderer_survives_a_trace_with_nothing_in_it(self):
        with tempfile.TemporaryDirectory() as folder:
            bare = pathlib.Path(folder) / "603_4_bare_1789500000.ndjson"
            bare.write_text(json.dumps({"e": "hdr", "v": 12, "boss": "bare", "roster": []}) + "\n",
                            encoding="utf-8")
            trace = Trace(bare)
            for call in (lambda: timeline.show_notes(trace),
                         lambda: stuck.show_vetoes(trace),
                         lambda: stuck.show_idle(trace, 1000),
                         lambda: space.show_threat(trace),
                         lambda: deathreport.summarise(trace)):
                self.run_quiet(call)


class DuringScope(unittest.TestCase):
    @staticmethod
    def args(**given):
        views = {view: None for view in postmortem.DURING_VIEWS}
        return argparse.Namespace(during="fixture.phase=2", **{**views, **given})

    def test_a_view_that_would_read_the_whole_pull_refuses_it(self):
        self.assertTrue(postmortem.ignores_during(self.args(stalls=6000)))

    def test_every_view_that_scopes_takes_it_even_given_bare(self):
        # --probes and --moves arrive as "" and --threat as 0 when named without a value
        for view, bare in (("probes", ""), ("where", "death"), ("moves", ""), ("threat", 0), ("clump", 10.0)):
            with self.subTest(view=view):
                self.assertFalse(postmortem.ignores_during(self.args(**{view: bare})))


class RichFixture(unittest.TestCase):
    """The thin coverage fixture leaves most invariants iterating an empty list."""

    def test_every_check_passes_on_a_trace_that_exercises_them(self):
        failed = [name for name, count, _ in verify_checks(rich()) if count]
        self.assertEqual(failed, [])

    def test_the_combat_checks_are_actually_reached(self):
        trace = rich()
        self.assertTrue(trace.of("dmg"))
        self.assertTrue(trace.of("death"))
        self.assertTrue(trace.of("move"))
        self.assertTrue(trace.of("veto"))
        self.assertTrue(trace.of("aura"))
        self.assertTrue(trace.of("haz"))

    def test_a_blow_with_no_damage_row_is_caught(self):
        trace = rich()
        victims = {death["g"] for death in trace.of("death")}
        trace.records = [rec for rec in trace.records
                         if not (rec.get("e") == "dmg" and rec.get("d") in victims)]
        failed = [name for name, count, _ in verify_checks(trace) if count]
        self.assertIn("every blow has a damage row behind it", failed)

    def test_a_blow_from_the_victim_needs_no_damage_row(self):
        # .die on yourself, falls and lava never reach the combat log
        trace = rich()
        death = trace.of("death")[0]
        death["blow"] = [death["g"], 9000]
        trace.records = [rec for rec in trace.records
                         if not (rec.get("e") == "dmg" and rec.get("d") == death["g"])]
        failed = [name for name, count, _ in verify_checks(trace) if count]
        self.assertNotIn("every blow has a damage row behind it", failed)

        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer):
            deathreport.death_block(trace, death, 0, True)
        self.assertIn("killed by its own blow", buffer.getvalue())

    def test_a_death_written_twice_is_caught(self):
        trace = rich()
        death = trace.of("death")[0]
        at = trace.records.index(death)
        trace.records.insert(at + 1, {**death, "t": death["t"] + 1})
        failed = [name for name, count, _ in verify_checks(trace) if count]
        self.assertIn("no death is recorded twice", failed)

    def test_a_dealt_column_going_backwards_is_caught(self):
        trace = rich()
        snaps = trace.of("snap")
        for row in snaps[-1]["u"]:
            row[11] = 0
        failed = [name for name, count, _ in verify_checks(trace) if count]
        self.assertIn("cumulative `dealt` never decreases", failed)


if __name__ == "__main__":
    unittest.main()

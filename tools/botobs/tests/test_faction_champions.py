"""Tests for the Faction Champions reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins the arithmetic behind each section, and the shared fixture shows every section
reads empty on a trace that is not Faction Champions rather than raising.
"""
from __future__ import annotations

import collections
import contextlib
import io
import json
import pathlib
import re
import sys
import tempfile
import unittest
from unittest import mock

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from bosses import faction_champions as fc  # noqa: E402
from raidobs import probes  # noqa: E402
from raidobs.paths import RAID_ROOT  # noqa: E402
from raidobs.probes import EVENT, HOLDER, LATCH  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v13.ndjson"
CHAMPIONS_HEADER = RAID_ROOT / "ToC" / "Util" / "ToCHelpers_FactionChampions.h"

TANK = 5001
ROGUE = 5002
MAGE = 5003
LOCK = 5004
PRIEST = 5006  # heal
SHAMAN = 5007  # heal
LATE = 5008  # joined after the header
FELHUNTER = (2 << 32) | 5

CREATURE = 1 << 32
PALADIN = CREATURE | 11
DISC = CREATURE | 12
DRUID = CREATURE | 13
ROGUE_C = CREATURE | 14
WARLOCK_C = CREATURE | 15
MAGE_C = CREATURE | 16
PET_C = CREATURE | 17

GUARD = "faction champions target guard multiplier"
REDIRECT = "faction champions threat redirect veto multiplier"

# The latch: first on the paladin, off him while he's shielded, back, then the priest once he dies.
KILL_ORDER = ((1000, 5000, PALADIN), (5000, 9000, DISC), (9000, 12250, PALADIN), (12250, 20000, DISC))


def kill_at(when: int) -> int:
    return next((guid for start, stop, guid in KILL_ORDER if start <= when < stop), 0)


def row(guid: int, hp: float, target: int) -> list:
    return [guid, 0.0, 0.0, 0.0, 0.0, hp, 80.0, target, 0, 0, 0, 0]


def snap(when: int) -> dict:
    units = [
        row(TANK, 100.0, ROGUE_C),
        row(ROGUE, 100.0, kill_at(when)),
        row(MAGE, 100.0, kill_at(when)),
        row(LOCK, 100.0, kill_at(when)),
        row(PRIEST, 100.0, 0),
        row(SHAMAN, 100.0, 0),
        row(FELHUNTER, 100.0, kill_at(when)),
        row(DISC, 100.0 - when / 1000.0, TANK),
        row(DRUID, 100.0, TANK),
        row(ROGUE_C, 100.0, TANK),
        row(WARLOCK_C, 100.0, TANK),
        row(MAGE_C, 100.0, TANK),
    ]
    # dead champions drop out of the sweep
    if when <= 12000:
        units.append(row(PALADIN, 100.0 - when / 200.0, TANK))
    return {"t": when, "e": "snap", "u": units, "hz": []}


def note(when: int, guid: int, key: str, txt) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": str(txt)}


def cast(when: int, caster: int, spell: int, target: int, ct: int = 0, triggered: bool = False) -> dict:
    rec = {"t": when, "e": "cast", "s": caster, "sp": spell, "tgt": target, "ct": ct}
    if triggered:
        rec["tr"] = 1
    return rec


def champions_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 3, "boss": "velanaa",
         "roster": [
             {"g": TANK, "n": "Bulwark", "r": "tank", "c": 1, "h": 0},
             {"g": ROGUE, "n": "Shiv", "r": "melee", "c": 4, "h": 0},
             {"g": MAGE, "n": "Frost", "r": "ranged", "c": 8, "h": 0},
             {"g": LOCK, "n": "Agony", "r": "ranged", "c": 9, "h": 0},
             {"g": PRIEST, "n": "Mercy", "r": "heal", "c": 5, "h": 0},
             {"g": SHAMAN, "n": "Totem", "r": "heal", "c": 7, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "velanaa", "src": "engage"},
        {"t": 1, "e": "unit", "g": LATE, "n": "Moonkin", "c": 11, "r": "ranged", "h": 0},
        {"t": 1, "e": "unit", "g": FELHUNTER, "en": 417, "n": "Zzarg", "own": LOCK},
        {"t": 1, "e": "unit", "g": PALADIN, "en": 34465, "n": "Velanaa", "b": 1},
        {"t": 1, "e": "unit", "g": DISC, "en": 34466, "n": "Anthar Forgemender", "b": 1},
        {"t": 1, "e": "unit", "g": DRUID, "en": 34469, "n": "Melador Valestrider", "b": 1},
        {"t": 1, "e": "unit", "g": ROGUE_C, "en": 34472, "n": "Irieth Shadowstep", "b": 1},
        {"t": 1, "e": "unit", "g": WARLOCK_C, "en": 34474, "n": "Serissa Grimdabbler", "b": 1},
        {"t": 1, "e": "unit", "g": MAGE_C, "en": 34468, "n": "Noozle Whizzlestick", "b": 1},
        {"t": 1, "e": "unit", "g": PET_C, "en": 35465, "n": "Zhaagrym", "own": WARLOCK_C},

        # A cleared latch restated at the trace's open, then a restate of a held value: neither is a hold.
        note(-500, TANK, "fc.kill", 0),
        note(1000, ROGUE, "fc.switch", "first"),
        note(1000, ROGUE, "fc.kill", PALADIN),
        note(1100, MAGE, "fc.kill", PALADIN),
        cast(4990, PALADIN, 66010, PALADIN),
        note(5000, MAGE, "fc.kill", DISC),
        note(5000, MAGE, "fc.switch", "immune"),
        note(9000, LOCK, "fc.switch", "back"),
        note(9000, LOCK, "fc.kill", PALADIN),
        note(12250, ROGUE, "fc.switch", "dead"),
        note(12250, ROGUE, "fc.kill", DISC),
        note(20000, TANK, "fc.switch", "reset"),
        note(20000, TANK, "fc.kill", 0),

        # CC: Frost holds the druid until 15 s, Agony the mage for the whole pull.
        note(1000, MAGE, "fc.cc", DRUID),
        note(1000, LOCK, "fc.cc", MAGE_C),
        note(15000, MAGE, "fc.cc", 0),
        cast(2000, MAGE, 12826, DRUID, 1500),
        cast(2100, MAGE, 12826, DRUID, 0, triggered=True),
        cast(6000, MAGE, 12826, DISC, 1500),
        cast(16000, MAGE, 12826, DRUID, 1500),
        cast(3000, LOCK, 6215, MAGE_C, 1500),
        cast(4000, LOCK, 6215, PET_C, 1500),

        # Heals: Flash of Light kicked, Nourish countered too late, Flash Heal spell-locked by the
        # felhunter, Tranquility countered 6 s into its channel.
        cast(3000, PALADIN, 66113, PALADIN, 1300),
        cast(3500, ROGUE, 1766, PALADIN),
        cast(7000, DRUID, 67965, DISC, 1500),
        cast(9000, MAGE, 2139, DRUID),
        cast(13000, DISC, 66104, DISC, 1500),
        cast(13400, FELHUNTER, 19647, DISC),
        cast(14000, DRUID, 66086, DRUID),
        cast(20000, MAGE, 2139, DRUID),
        {"t": 13300, "e": "act", "g": MAGE, "a": fc.COUNTERSPELL_ACTION, "rel": 100.0, "vd": "OK"},

        # Burst: a trinket before the pull does not count, a failed Icy Veins neither.
        cast(1500, SHAMAN, 32182, 0),
        {"t": -2000, "e": "act", "g": TANK, "a": "use trinket", "rel": 1.0, "vd": "OK"},
        {"t": 1600, "e": "act", "g": ROGUE, "a": "adrenaline rush", "rel": 1.0, "vd": "OK"},
        {"t": 2400, "e": "act", "g": MAGE, "a": "icy veins", "rel": 1.0, "vd": "FAILED"},
        {"t": 2500, "e": "act", "g": MAGE, "a": "icy veins", "rel": 1.0, "vd": "OK"},
        {"t": 4000, "e": "act", "g": ROGUE, "a": "killing spree", "rel": 1.0, "vd": "OK"},
        {"t": 6000, "e": "act", "g": TANK, "a": "recklessness", "rel": 1.0, "vd": "OK"},
        {"t": 1200, "e": "veto", "g": ROGUE, "m": fc.HOLD_BURST, "a": "adrenaline rush"},
        {"t": 11200, "e": "veto", "g": ROGUE, "m": fc.HOLD_BURST, "a": "adrenaline rush"},
        {"t": 2200, "e": "veto", "g": MAGE, "m": fc.TOC_BURST, "a": "icy veins"},

        # Fears: one Fear on Frost for 2 s, the ward and the totem answering.
        {"t": 6000, "e": "aura", "d": MAGE, "s": WARLOCK_C, "sp": 65809, "r": 0},
        {"t": 8000, "e": "aura", "d": MAGE, "s": WARLOCK_C, "sp": 65809, "r": 1},
        cast(500, PRIEST, fc.SPELL_FEAR_WARD, TANK),
        cast(5500, SHAMAN, fc.SPELL_TREMOR_TOTEM, 0),
        {"t": 500, "e": "act", "g": PRIEST, "a": fc.ANTI_FEAR_ACTION, "rel": 100.0, "vd": "OK"},

        # Two of the four multipliers, and one from another boss that must not count.
        {"t": 2000, "e": "veto", "g": TANK, "m": GUARD, "a": "tank assist"},
        {"t": 2000, "e": "veto", "g": MAGE, "m": GUARD, "a": "dps assist"},
        {"t": 2100, "e": "veto", "g": ROGUE, "m": REDIRECT, "a": "tricks of the trade"},
        {"t": 2100, "e": "veto", "g": ROGUE, "m": "vezax target guard multiplier", "a": "dps assist"},
        {"t": 21000, "e": "end", "out": "wipe"},
    ]
    records += [snap(when) for when in range(0, 21000, 1000)]
    return records


DECLARED = {
    "fc.kill": (LATCH, "fixture"),
    "fc.switch": (EVENT, "fixture"),
    "fc.cc": (HOLDER, "fixture"),
    "fc.interrupter": (LATCH, "fixture"),
    "toc.progress": (LATCH, "fixture"),
    "nb.snobold": (LATCH, "fixture"),
}


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_1_velanaa_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in champions_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_lineup_is_in_kill_order_and_leaves_the_pet_out(self):
        self.assertEqual(fc.lineup(self.trace), [
            (PALADIN, "Holy Paladin", True), (DISC, "Disc Priest", True), (DRUID, "Resto Druid", True),
            (ROGUE_C, "Rogue", False), (WARLOCK_C, "Warlock", False), (MAGE_C, "Mage", False)])

    def test_classes_are_named_for_the_roster_and_late_joiners(self):
        klass = fc.classes(self.trace)
        self.assertEqual((klass[TANK], klass[LOCK], klass[LATE]), ("warrior", "warlock", "druid"))

    def test_the_banner_names_only_fc_keys_this_pull_never_wrote(self):
        with mock.patch.object(probes, "declared_keys", return_value=DECLARED):
            self.assertEqual(fc.missing_probes(self.trace), ["fc.interrupter"])

    def test_each_hold_carries_the_switch_that_opened_it(self):
        holds = fc.kill_holds(self.trace)
        self.assertEqual([(hold["guid"], hold["start"], hold["stop"], hold["reason"]) for hold in holds], [
            (PALADIN, 1000, 5000, "first"), (DISC, 5000, 9000, "immune"),
            (PALADIN, 9000, 12250, "back"), (DISC, 12250, 20000, "dead")])

    def test_health_reads_the_samples_inside_the_hold(self):
        holds = fc.kill_holds(self.trace)
        self.assertEqual([(hold["hp_start"], hold["hp_end"]) for hold in holds],
                         [(95.0, 80.0), (95.0, 92.0), (55.0, 40.0), (87.0, 81.0)])

    def test_died_is_leaving_the_sweep_at_the_end_of_that_hold(self):
        self.assertEqual([hold["died"] for hold in fc.kill_holds(self.trace)], [False, False, True, False])

    def test_the_focus_share_counts_non_healer_bots_only(self):
        # the tank stays on the rogue, the three dps follow the latch, healers and the pet don't count
        for hold in fc.kill_holds(self.trace):
            self.assertAlmostEqual(hold["share"], 0.75)

    def test_cc_holds_end_on_a_release(self):
        self.assertEqual(fc.cc_holds(self.trace), {MAGE: [(DRUID, 1000, 15000)], LOCK: [(MAGE_C, 1000, 21000)]})

    def test_cc_casts_split_assigned_and_kill_target(self):
        rows = fc.cc_casts(self.trace)
        self.assertEqual({bot: (row["casts"], row["assigned"], row["kill"]) for bot, row in rows.items()},
                         {MAGE: (3, 1, 1), LOCK: (1, 1, 0)})
        self.assertEqual(rows[MAGE]["spells"], collections.Counter({"Polymorph": 3}))

    def test_a_heal_is_followed_only_inside_its_cast(self):
        rows = fc.heal_rows(self.trace)
        self.assertEqual([(row["spell"], row["window"], row["kill"], row["by"]) for row in rows], [
            (66113, 1300, True, ROGUE), (67965, 1500, False, None),
            (66104, 1500, True, LOCK), (66086, 10000, False, MAGE)])

    def test_lust_and_first_burst_count_from_the_pull(self):
        lust, first, vetoes = fc.burst_rows(self.trace)
        self.assertEqual((lust["s"], lust["sp"], lust["t"]), (SHAMAN, 32182, 1500))
        self.assertEqual(first, {ROGUE: (1600, "adrenaline rush"), MAGE: (2500, "icy veins"),
                                 TANK: (6000, "recklessness")})
        self.assertEqual(vetoes, collections.Counter({(fc.HOLD_BURST, "adrenaline rush"): 2,
                                                      (fc.TOC_BURST, "icy veins"): 1}))

    def test_a_fear_pair_is_one_fear_held_for_its_span(self):
        fears, wards = fc.fear_rows(self.trace)
        self.assertEqual(fears[65809], {"count": 1, "held": 2000})
        self.assertEqual(fears[65543], {"count": 0, "held": 0})
        self.assertEqual(wards, {fc.SPELL_FEAR_WARD: collections.Counter({PRIEST: 1}),
                                 fc.SPELL_TREMOR_TOTEM: collections.Counter({SHAMAN: 1})})

    def test_vetoes_keep_the_four_multipliers_only(self):
        self.assertEqual(fc.fc_vetoes(self.trace), collections.Counter({
            (GUARD, "tank assist"): 1, (GUARD, "dps assist"): 1, (REDIRECT, "tricks of the trade"): 1}))

    def test_every_section_renders(self):
        out = io.StringIO()
        with mock.patch.object(probes, "declared_keys", return_value=DECLARED), \
                contextlib.redirect_stdout(out):
            fc.show_banner(self.trace)
            for _, _, section in fc.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertIn("25-man heroic", text)
        self.assertIn("probes absent from this trace: fc.interrupter", text)
        self.assertIn("fc.switch: first 1, dead 1, immune 1, back 1, reset 1", text)
        self.assertIn("first interrupter by class: rogue 1, warlock 1, mage 1", text)


class KillEnding(unittest.TestCase):
    def test_the_hold_open_at_a_kill_died(self):
        # a kill writes no reset, and the priest is still in the last snapshot
        records = [rec for rec in champions_pull() if not (rec.get("e") == "note" and rec.get("t") == 20000)]
        records = [dict(rec, out="kill") if rec.get("e") == "end" else rec for rec in records]
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / "649_1_velanaa_1789500000.ndjson"
            path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
            holds = fc.kill_holds(Trace(path))
        self.assertEqual([(hold["guid"], hold["stop"], hold["died"]) for hold in holds][-1], (DISC, 21000, True))
        self.assertEqual([hold["died"] for hold in holds], [False, False, True, True])


class Source(unittest.TestCase):
    def test_the_kill_order_covers_the_champion_enum(self):
        text = CHAMPIONS_HEADER.read_text(encoding="utf-8")
        body = re.search(r"enum class ToCFactionChampions\b[^{]*\{(.*?)\};", text, re.S)
        self.assertIsNotNone(body, f"no ToCFactionChampions enum in {CHAMPIONS_HEADER}")
        entries = {int(value) for value in re.findall(r"^\s*\w+\s*=\s*(\d+)", body.group(1), re.M)}
        self.assertEqual(entries, set(fc.SPEC_OF))

    def test_burst_names_come_from_the_source(self):
        self.assertTrue({"bloodlust", "heroism", "offensive potion"} <= fc.burst_actions())


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            fc.show_banner(trace)
            for _, _, section in fc.SECTIONS:
                section(trace)
        self.assertIn("not a Faction Champions pull", out.getvalue())
        self.assertEqual(fc.kill_holds(trace), [])
        self.assertEqual(fc.cc_holds(trace), {})
        self.assertEqual(fc.cc_casts(trace), {})
        self.assertEqual(fc.heal_rows(trace), [])
        lust, first, vetoes = fc.burst_rows(trace)
        self.assertEqual((lust, first, vetoes), (None, {}, collections.Counter()))
        fears, wards = fc.fear_rows(trace)
        self.assertFalse(any(row["count"] for row in fears.values()))
        self.assertFalse(any(wards.values()))
        self.assertEqual(fc.fc_vetoes(trace), collections.Counter())


if __name__ == "__main__":
    unittest.main()

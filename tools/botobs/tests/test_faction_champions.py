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
WARRIOR_C = CREATURE | 18

GUARD = "faction champions target guard multiplier"
REDIRECT = "faction champions threat redirect veto multiplier"
PHYSICAL_GUARD = "faction champions physical switch guard multiplier"
AOE_GUARD = "faction champions aoe guard multiplier"
CC_GUARD = "faction champions cc guard multiplier"
TOTEM_GUARD = "faction champions totem guard multiplier"

# The latch: first on the paladin, off him while he's shielded, back, then the priest once he dies.
KILL_ORDER = ((1000, 5000, PALADIN), (5000, 9000, DISC), (9000, 12250, PALADIN), (12250, 20000, DISC))


def kill_at(when: int) -> int:
    return next((guid for start, stop, guid in KILL_ORDER if start <= when < stop), 0)


def row(guid: int, hp: float, target: int, movegen: int = 0) -> list:
    return [guid, 0.0, 0.0, 0.0, 0.0, hp, 80.0, target, 0, movegen, 0, 0]


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


def dmg(when: int, source: int, victim: int, spell: int) -> dict:
    return {"t": when, "e": "dmg", "s": source, "d": victim, "sp": spell, "a": 1000, "ok": 0, "sc": 1, "ab": 0,
            "rs": 0, "hp": 90.0}


def aura(when: int, victim: int, source: int, spell: int, removed: bool, left: int) -> dict:
    return {"t": when, "e": "aura", "d": victim, "s": source, "sp": spell, "r": int(removed), "dur": left}


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
        {"t": 1, "e": "unit", "g": WARRIOR_C, "en": 34475, "n": "Shocuul", "b": 1},

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

        # Dispels: Bulwark's Polymorph dispelled a second in (the removal shares the cast's stamp),
        # Agony's left to expire, Frost's Fear from above with no `dur` on its removal. A dispel before
        # the Polymorph, one on a raider under no CC and a Silence on a non-healer don't count. The
        # backlash is the 25H id.
        aura(2000, TANK, MAGE_C, 65801, False, 10000),
        cast(1900, PRIEST, 988, TANK),
        cast(2500, SHAMAN, 51886, MAGE),
        cast(3000, PRIEST, 988, TANK),
        aura(3000, TANK, MAGE_C, 65801, True, 9000),
        aura(10000, LOCK, MAGE_C, 65801, False, 10000),
        aura(20000, LOCK, MAGE_C, 65801, True, 0),
        aura(14000, ROGUE, DISC, 65542, False, 5000),
        aura(19000, ROGUE, DISC, 65542, True, 0),
        aura(3000, PRIEST, WARLOCK_C, 68159, False, 5000),
        aura(8000, PRIEST, WARLOCK_C, 68159, True, 0),
        # A Hex on Bulwark: Dispel Magic can't take a curse, the Cleanse Spirit after it can.
        aura(12000, TANK, DISC, 66054, False, 8000),
        cast(13000, PRIEST, 988, TANK),
        cast(14000, SHAMAN, 51886, TANK),
        aura(20000, TANK, DISC, 66054, True, 0),
        {"t": 3000, "e": "act", "g": PRIEST, "a": fc.DISPEL_CC_ACTION, "rel": 100.0, "vd": "OK"},
        {"t": 3100, "e": "act", "g": PRIEST, "a": fc.DISPEL_CC_ACTION, "rel": 100.0, "vd": "FAILED"},

        # Pack buffs: Velanaa's Avenging Wrath taken by the felhunter's Devour Magic below, the druid's
        # Bloodlust left. And a raid Cleave on the pack.
        cast(9000, PALADIN, 66011, 0),
        cast(15500, DRUID, 65980, 0),
        cast(5000, TANK, 47520, ROGUE_C),

        # Purges: on the kill target, a Spellsteal off it, the felhunter's Devour Magic for Agony.
        cast(4000, SHAMAN, 8012, PALADIN),
        cast(4000, SHAMAN, 8012, PALADIN, triggered=True),
        cast(6500, MAGE, 30449, DRUID),
        cast(10000, FELHUNTER, 48011, PALADIN),
        {"t": 4000, "e": "act", "g": SHAMAN, "a": fc.PURGE_ACTION, "rel": 100.0, "vd": "OK"},

        # Hand of Protection on the priest while she's the kill target, the physical switch to the
        # rogue until 8.5 s, and one Mass Dispel after both shields.
        cast(6000, PALADIN, 66009, DISC),
        note(-500, TANK, "fc.physical", 0),
        note(6000, ROGUE, "fc.physical", ROGUE_C),
        note(8500, MAGE, "fc.physical", 0),
        cast(7000, PRIEST, fc.SPELL_MASS_DISPEL, 0, 1500),

        # Crowd AoE: a Fan of Knives on Bulwark and one on nobody, a Shout fearing two, a Bladestorm
        # hitting two bots twice plus a tick past its window, a 25H Hellfire ticking Frost three times.
        cast(5000, ROGUE_C, 68099, 0),
        dmg(5000, ROGUE_C, TANK, 68099),
        cast(8000, ROGUE_C, 68099, 0),
        cast(9500, WARRIOR_C, 65930, TANK),
        aura(9500, MAGE, WARRIOR_C, 65930, False, 8000),
        aura(9500, LOCK, WARRIOR_C, 65930, False, 8000),
        aura(11000, MAGE, WARRIOR_C, 65930, True, 6500),
        aura(11000, LOCK, WARRIOR_C, 65930, True, 6500),
        cast(11000, WARRIOR_C, 65947, 0),
        dmg(12000, WARRIOR_C, TANK, 65946),
        dmg(12000, WARRIOR_C, ROGUE, 65946),
        dmg(13000, WARRIOR_C, TANK, 65946),
        dmg(13000, WARRIOR_C, ROGUE, 65946),
        dmg(20500, WARRIOR_C, TANK, 65946),
        cast(15000, WARLOCK_C, 68147, 0),
        dmg(16000, WARLOCK_C, MAGE, 68144),
        dmg(17000, WARLOCK_C, MAGE, 68144),
        dmg(18000, WARLOCK_C, MAGE, 68144),
        {"t": 12000, "e": "act", "g": ROGUE, "a": fc.AVOID_AOE_ACTION, "rel": 100.0, "vd": "OK"},

        # Six of the ten multipliers, and one from another boss that must not count.
        {"t": 2000, "e": "veto", "g": TANK, "m": GUARD, "a": "tank assist"},
        {"t": 2000, "e": "veto", "g": MAGE, "m": GUARD, "a": "dps assist"},
        {"t": 2100, "e": "veto", "g": ROGUE, "m": REDIRECT, "a": "tricks of the trade"},
        {"t": 2100, "e": "veto", "g": ROGUE, "m": "vezax target guard multiplier", "a": "dps assist"},
        {"t": 6000, "e": "veto", "g": ROGUE, "m": PHYSICAL_GUARD, "a": "faction champions focus priority"},
        {"t": 12000, "e": "veto", "g": ROGUE, "m": AOE_GUARD, "a": "reach melee"},
        {"t": 2200, "e": "veto", "g": LOCK, "m": CC_GUARD, "a": "fear on cc"},
        {"t": 7000, "e": "veto", "g": ROGUE, "m": TOTEM_GUARD, "a": "faction champions focus priority"},
        {"t": 21000, "e": "end", "out": "wipe"},
    ]
    records += [snap(when) for when in range(0, 21000, 1000)]
    return records


DECLARED = {
    "fc.kill": (LATCH, "fixture"),
    "fc.switch": (EVENT, "fixture"),
    "fc.cc": (HOLDER, "fixture"),
    "fc.physical": (LATCH, "fixture"),
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
            (ROGUE_C, "Rogue", False), (WARRIOR_C, "Warrior", False), (WARLOCK_C, "Warlock", False),
            (MAGE_C, "Mage", False)])

    def test_classes_are_named_for_the_roster_and_late_joiners(self):
        klass = fc.classes(self.trace)
        self.assertEqual((klass[TANK], klass[LOCK], klass[LATE]), ("warrior", "warlock", "druid"))

    def test_the_banner_names_only_fc_keys_this_pull_never_wrote(self):
        with mock.patch.object(probes, "declared_keys", return_value=DECLARED):
            self.assertEqual(fc.missing_probes(self.trace), ["fc.interrupter"])

    def test_the_banner_wants_fc_hold_but_not_fc_nitro(self):
        declared = {**DECLARED, "fc.hold": (HOLDER, "fixture"), "fc.nitro": (EVENT, "fixture")}
        with mock.patch.object(probes, "declared_keys", return_value=declared):
            self.assertEqual(fc.missing_probes(self.trace), ["fc.hold", "fc.interrupter"])

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

    def test_a_cc_duty_counts_the_holders_own_cast_on_its_champion(self):
        self.assertEqual(fc.cc_duties(self.trace), {
            MAGE: {"duties": 1, "cast": 1, "spells": collections.Counter({"Polymorph": 1}), "median": 14000},
            LOCK: {"duties": 1, "cast": 1, "spells": collections.Counter({"Fear": 1}), "median": 20000}})

    def test_cc_casts_split_assigned_and_kill_target(self):
        rows = fc.cc_casts(self.trace)
        self.assertEqual({bot: (row["casts"], row["assigned"], row["kill"]) for bot, row in rows.items()},
                         {MAGE: (3, 1, 1), LOCK: (1, 1, 0)})
        self.assertEqual(rows[MAGE]["spells"], collections.Counter({"Polymorph": 3}))

    def test_without_fc_ccaura_only_a_fear_can_read_as_landed(self):
        rows = fc.cc_casts(self.trace)
        self.assertEqual({bot: row["landed"] for bot, row in rows.items()},
                         {MAGE: {"Polymorph": None}, LOCK: {"Fear": 0}})
        self.assertEqual(fc.cc_aura_held(self.trace), {})

    def test_a_heal_is_followed_only_inside_its_cast(self):
        rows = fc.heal_rows(self.trace)
        self.assertEqual([(row["spell"], row["window"], row["kill"], row["by"], row["at"]) for row in rows], [
            (66113, 1300, True, ROGUE, 3500), (67965, 1500, False, None, None),
            (66104, 1500, True, LOCK, 13400), (66086, 10000, False, MAGE, 20000)])

    def test_channels_read_the_first_interrupt_inside_ten_seconds(self):
        self.assertEqual([(row["t"], row["caster"], row["name"], row["by"], row["after"])
                          for row in fc.channel_rows(self.trace)], [
            (14000, DRUID, "Tranquility", MAGE, 6000), (15000, WARLOCK_C, "Hellfire", None, None)])

    def test_pack_buffs_read_the_purge_after_them(self):
        self.assertEqual([(row["t"], row["target"], row["name"], row["purged"])
                          for row in fc.pack_buff_rows(self.trace)], [
            (9000, PALADIN, "Avenging Wrath", 10000), (15500, DRUID, "Bloodlust", None)])

    def test_own_splash_counts_the_raids_area_casts(self):
        self.assertEqual(fc.own_splash(self.trace), collections.Counter({"Cleave": 1}))

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

    def test_a_cc_aura_draws_the_first_dispel_inside_it(self):
        self.assertEqual([(aura["member"], aura["spell"], aura["start"], aura["stop"], aura["left"], aura["at"],
                           aura["by"]) for aura in fc.cc_auras(self.trace)], [
            (TANK, 65801, 2000, 3000, 9000, 3000, PRIEST),
            (MAGE, 65809, 6000, 8000, None, None, None),
            (LOCK, 65801, 10000, 20000, 0, None, None),
            (TANK, 66054, 12000, 20000, 0, 14000, SHAMAN)])

    def test_mass_dispel_counts_apart_while_cc_holds_and_splits_off_shield_windows(self):
        # the one cast at 7 s falls under both the Fear and the Divine Shield
        self.assertEqual(fc.mass_dispels_under_cc(self.trace), (1, 1))

    def test_backlash_reads_every_id_of_its_row(self):
        self.assertEqual(fc.backlashes(self.trace), [(3000, PRIEST)])

    def test_purges_on_champions_split_kill_target_and_credit_the_pet_owner(self):
        self.assertEqual([(row["t"], row["by"], row["spell"], row["kill"]) for row in fc.purge_rows(self.trace)], [
            (4000, SHAMAN, 8012, True), (6500, MAGE, 30449, False), (10000, LOCK, 48011, True)])

    def test_a_shield_pairs_with_the_next_mass_dispel_and_back(self):
        self.assertEqual([(row["t"], row["spell"], row["target"], row["kill"], row["mass"], row["masses"],
                           row["back"], row["early"]) for row in fc.shield_rows(self.trace)], [
            (4990, fc.SPELL_DIVINE_SHIELD, PALADIN, True, 7000, 1, 9000, True),
            (6000, fc.SPELL_HAND_OF_PROTECTION, DISC, True, 7000, 1, None, False)])

    def test_a_physical_hold_shares_over_tanks_melee_and_hunters(self):
        # Bulwark stays on the rogue, Shiv follows the kill target
        self.assertEqual(fc.physical_holds(self.trace), [
            {"guid": ROGUE_C, "start": 6000, "stop": 8500, "share": 0.5}])
        self.assertTrue(fc.physical("ranged", "hunter"))
        self.assertFalse(fc.physical("heal", "paladin"))

    def test_aoe_victims_go_to_the_cast_whose_window_holds_them(self):
        rows = fc.aoe_casts(self.trace)
        self.assertEqual([(row["t"], row["caster"], row["name"], dict(row["hits"])) for row in rows], [
            (5000, ROGUE_C, "Fan of Knives", {TANK: 1}),
            (8000, ROGUE_C, "Fan of Knives", {}),
            (9500, WARRIOR_C, "Intimidating Shout", {MAGE: 1, LOCK: 1}),
            (11000, WARRIOR_C, "Bladestorm", {TANK: 2, ROGUE: 2}),
            (15000, WARLOCK_C, "Hellfire", {MAGE: 3})])

    def test_vetoes_keep_the_faction_champions_multipliers_and_rules_only(self):
        self.assertEqual(len(set(fc.FC_MULTIPLIERS)), 18)
        self.assertEqual(fc.fc_vetoes(self.trace), collections.Counter({
            (GUARD, "tank assist"): 1, (GUARD, "dps assist"): 1, (REDIRECT, "tricks of the trade"): 1,
            (PHYSICAL_GUARD, "faction champions focus priority"): 1, (AOE_GUARD, "reach melee"): 1,
            (CC_GUARD, "fear on cc"): 1, (TOTEM_GUARD, "faction champions focus priority"): 1}))

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
        self.assertIn("first dispel cast inside each, by class: priest 1 (median 1.0 s, max 1.0 s),"
                      " shaman 1 (median 2.0 s, max 2.0 s)", text)
        self.assertIn("Mass Dispel: 1 cast(s) while counted CC held a raider, 1 of them inside a champion"
                      " shield window", text)
        self.assertIn("Unstable Affliction backlash on the raid: 1 (Mercy 1)", text)
        self.assertIn("3 offensive dispel(s) on champions, 2 on the kill target", text)
        self.assertIn(f"{fc.PURGE_ACTION}: 1 OK row(s)", text)
        self.assertRegex(text, r"Divine Shield .* yes +\+2\.0s x1 +\+4\.0s early")
        self.assertRegex(text, r"Bladestorm +1 +2\.0, max 2 +2\.0, max 2 +2 x1")
        self.assertIn(f"{fc.AVOID_AOE_ACTION}: 1 OK row(s)", text)
        self.assertIn("cast-time heals kicked 33% into the cast (median), 0 of 2 in the last 600 ms", text)
        self.assertIn("channels: Tranquility 0:14.000 Melador Valestrider +6.0s by Frost,"
                      " Hellfire 0:15.000 Serissa Grimdabbler no interrupt", text)
        self.assertIn("champion buffs: Avenging Wrath 0:09.000 Velanaa purged +1.0s,"
                      " Bloodlust 0:15.500 Melador Valestrider not purged", text)
        self.assertIn("raid splash: Cleave 1", text)


RET_C = CREATURE | 19


def shield_pull() -> list[dict]:
    """Each shield lands 10 ms before the refresh that moves `fc.kill` off it."""
    return [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 3, "boss": "velanaa",
         "roster": [{"g": PRIEST, "n": "Mercy", "r": "heal", "c": 5, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "velanaa", "src": "engage"},
        {"t": 1, "e": "unit", "g": PALADIN, "en": 34465, "n": "Velanaa", "b": 1},
        {"t": 1, "e": "unit", "g": DISC, "en": 34466, "n": "Anthar Forgemender", "b": 1},
        {"t": 1, "e": "unit", "g": MAGE_C, "en": 34468, "n": "Noozle Whizzlestick", "b": 1},
        {"t": 1, "e": "unit", "g": RET_C, "en": 34471, "n": "Malithas Brightblade", "b": 1},
        note(1000, PRIEST, "fc.switch", "first"),
        note(1000, PRIEST, "fc.kill", MAGE_C),
        # Ice Block on the kill target, and the paladin's early return lands inside it
        cast(1490, MAGE_C, fc.SPELL_ICE_BLOCK, MAGE_C),
        note(1500, PRIEST, "fc.switch", "immune"),
        note(1500, PRIEST, "fc.kill", PALADIN),
        # Divine Shield Mass Dispelled, back 4 s in
        cast(1990, PALADIN, fc.SPELL_DIVINE_SHIELD, PALADIN),
        note(2000, PRIEST, "fc.switch", "immune"),
        note(2000, PRIEST, "fc.kill", DISC),
        # off the kill target, so no back to read
        cast(2990, RET_C, fc.SPELL_DIVINE_SHIELD, RET_C),
        cast(3500, PRIEST, fc.SPELL_MASS_DISPEL, 0, 1500),
        note(6000, PRIEST, "fc.switch", "back"),
        note(6000, PRIEST, "fc.kill", PALADIN),
        note(8000, PRIEST, "fc.switch", "dead"),
        note(8000, PRIEST, "fc.kill", RET_C),
        # Divine Shield run to its end, back 12.5 s in
        cast(8990, RET_C, fc.SPELL_DIVINE_SHIELD, RET_C),
        note(9000, PRIEST, "fc.switch", "immune"),
        note(9000, PRIEST, "fc.kill", DISC),
        note(21500, PRIEST, "fc.switch", "back"),
        note(21500, PRIEST, "fc.kill", RET_C),
        {"t": 22000, "e": "end", "out": "wipe"},
    ]


class ShieldReturns(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_1_velanaa_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in shield_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_back_counts_only_when_the_latch_returns_to_the_shielded_kill_target(self):
        self.assertEqual([(row["t"], row["target"], row["kill"], row["mass"], row["back"], row["early"])
                          for row in fc.shield_rows(self.trace)], [
            (1490, MAGE_C, True, 3500, None, False),
            (1990, PALADIN, True, 3500, 6000, True),
            (2990, RET_C, False, 3500, None, False),
            (8990, RET_C, True, None, 21500, False)])

    def test_the_back_column_marks_early_full_and_not_on_the_kill_target(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            fc.show_hop(self.trace)
        lines = [line for line in out.getvalue().splitlines() if "Divine Shield" in line or "Ice Block" in line]
        self.assertEqual([line.split()[-1] for line in lines], ["-", "early", "n/a", "+12.5s"])
        self.assertIn("+4.0s early", lines[1])


class SkullSwitch(unittest.TestCase):
    def test_a_players_skull_opens_its_own_hold_and_counts_in_order(self):
        records = [
            {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 1, "boss": "velanaa",
             "roster": [{"g": ROGUE, "n": "Shiv", "r": "melee", "c": 4, "h": 0}]},
            {"t": 0, "e": "pull", "boss": "velanaa", "src": "engage"},
            {"t": 1, "e": "unit", "g": PALADIN, "en": 34465, "n": "Velanaa", "b": 1},
            {"t": 1, "e": "unit", "g": WARLOCK_C, "en": 34474, "n": "Serissa Grimdabbler", "b": 1},
            note(1000, ROGUE, "fc.switch", "first"),
            note(1000, ROGUE, "fc.kill", PALADIN),
            note(4000, ROGUE, "fc.switch", "skull"),
            note(4000, ROGUE, "fc.kill", WARLOCK_C),
            note(9000, ROGUE, "fc.switch", "reset"),
            note(9000, ROGUE, "fc.kill", 0),
            {"t": 9000, "e": "end", "out": "wipe"},
        ]
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / "649_1_velanaa_1789500000.ndjson"
            path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
            trace = Trace(path)
            holds = fc.kill_holds(trace)
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                fc.show_kill(trace)
        self.assertEqual([(hold["guid"], hold["reason"]) for hold in holds],
                         [(PALADIN, "first"), (WARLOCK_C, "skull")])
        self.assertIn("fc.switch: first 1, skull 1, reset 1", out.getvalue())


class PulledGrip(unittest.TestCase):
    def test_a_pulled_hold_names_the_grip_before_it_or_none(self):
        records = [
            {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 3, "boss": "velanaa",
             "roster": [{"g": GRIPPER, "n": "Obliteration", "r": "melee", "c": 6, "h": 0}]},
            {"t": 0, "e": "pull", "boss": "velanaa", "src": "engage"},
            {"t": 1, "e": "unit", "g": PALADIN, "en": 34465, "n": "Velanaa", "b": 1},
            {"t": 1, "e": "unit", "g": DISC, "en": 34466, "n": "Caiphus", "b": 1},
            note(1000, GRIPPER, "fc.switch", "first"),
            note(1000, GRIPPER, "fc.kill", PALADIN),
            cast(2000, GRIPPER, fc.SPELL_DEATH_GRIP, DISC),
            note(4000, GRIPPER, "fc.switch", "pulled"),
            note(4000, GRIPPER, "fc.kill", DISC),
            # a grip on the paladin long before its pulled switch doesn't count
            cast(4500, GRIPPER, fc.SPELL_DEATH_GRIP, PALADIN),
            note(9000, GRIPPER, "fc.switch", "pulled"),
            note(9000, GRIPPER, "fc.kill", PALADIN),
            {"t": 12000, "e": "end", "out": "wipe"},
        ]
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / "649_2_velanaa_1789500000.ndjson"
            path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
            trace = Trace(path)
            holds = fc.kill_holds(trace)
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                fc.show_kill(trace)
        self.assertEqual([hold["grip"] for hold in holds], [None, (GRIPPER, 2000), None])
        self.assertIn("grip by Obliteration at 0:02.000", out.getvalue())
        self.assertEqual(out.getvalue().count("no grip"), 1)


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


BOOMKIN = 5009
BALANCE_C = CREATURE | 20
ENH_C = CREATURE | 21
TREANT = CREATURE | 22
GROUNDING = CREATURE | 23
RAID_TOTEM = CREATURE | 24


def adds_snap(when: int) -> dict:
    units = [row(guid, 100.0, 0) for guid in (TANK, ROGUE, LOCK, SHAMAN, BOOMKIN, RAID_TOTEM)]
    units += [row(WARRIOR_C, 100.0, ROGUE), row(BALANCE_C, 100.0, TANK), row(ENH_C, 100.0, TANK),
              # feared on the pre-roll sample too, which the uptime leaves out
              row(MAGE_C, 100.0, TANK, fc.FLEEING if when in (-1000, 6000, 7000) else 0)]
    if 3000 <= when <= 8000:
        units.append(row(TREANT, 100.0, TANK))
    if 7000 <= when <= 11000:
        units.append(row(GROUNDING, 100.0, 0))
    return {"t": when, "e": "snap", "u": units, "hz": []}


def adds_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 1, "boss": "shocuul",
         "roster": [
             {"g": TANK, "n": "Bulwark", "r": "tank", "c": 1, "h": 0},
             {"g": ROGUE, "n": "Shiv", "r": "melee", "c": 4, "h": 0},
             {"g": LOCK, "n": "Agony", "r": "ranged", "c": 9, "h": 0},
             {"g": SHAMAN, "n": "Totem", "r": "heal", "c": 7, "h": 0},
             {"g": BOOMKIN, "n": "Druidica", "r": "ranged", "c": 11, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "shocuul", "src": "engage"},
        {"t": 1, "e": "unit", "g": WARRIOR_C, "en": 34475, "n": "Shocuul", "b": 1},
        {"t": 1, "e": "unit", "g": MAGE_C, "en": 34468, "n": "Noozle Whizzlestick", "b": 1},
        {"t": 1, "e": "unit", "g": BALANCE_C, "en": 34460, "n": "Kavina Grovesong", "b": 1},
        {"t": 1, "e": "unit", "g": ENH_C, "en": 34463, "n": "Shaabad", "b": 1},
        {"t": 1, "e": "unit", "g": RAID_TOTEM, "en": 5925, "n": "Grounding Totem", "own": SHAMAN},
        {"t": 2500, "e": "unit", "g": TREANT, "en": fc.NPC_TREANT, "n": "Treant", "own": BALANCE_C},
        {"t": 6500, "e": "unit", "g": GROUNDING, "en": 5925, "n": "Grounding Totem", "own": ENH_C},

        # A Cyclone held 3.1-9.1 s. The recast right after the landing finds its turn already taken, and
        # a later one never lands.
        cast(2000, BOOMKIN, 33786, WARRIOR_C, 1000),
        note(3100, WARRIOR_C, fc.CC_AURA, 33786),
        cast(3050, BOOMKIN, 33786, WARRIOR_C, 1000),
        note(9100, WARRIOR_C, fc.CC_AURA, 0),
        cast(10000, BOOMKIN, 33786, WARRIOR_C, 1000),
        # fc.ccaura never records a root, so this one can't read as landed or not.
        cast(14000, BOOMKIN, 53308, WARRIOR_C, 1500),
        # A Fear turning the mage to fleeing at 6 s, and one that never does.
        cast(4000, LOCK, 6215, MAGE_C, 1500),
        cast(12000, LOCK, 6215, MAGE_C, 1500),

        # Exhaustion: Shiv's cleared by his death, Bulwark's run out, Druidica's Sated gone before the
        # pull, and Totem's lust at 12 s writing Agony's as a removal at full length then his own apply.
        aura(-3000, BOOMKIN, BOOMKIN, 57724, True, 0),
        aura(8000, ROGUE, ROGUE, 57723, True, 92000),
        {"t": 8000, "e": "death", "g": ROGUE, "killer": WARRIOR_C},
        cast(12000, SHAMAN, 2825, 0),
        aura(12000, LOCK, SHAMAN, 57723, True, 600000),
        aura(12001, LOCK, LOCK, 57723, False, 599999),
        aura(12005, SHAMAN, SHAMAN, 57723, False, 599995),
        aura(15000, TANK, TANK, 57723, True, 0),

        # Arcane Explosion hits land 3 ms before their cast row.
        dmg(997, MAGE_C, TANK, 68000),
        cast(1000, MAGE_C, 68000, 0),
        dmg(4997, MAGE_C, TANK, 68000),
        dmg(4997, MAGE_C, ROGUE, 68000),
        cast(5000, MAGE_C, 68000, 0),

        # Force of Nature at 2 s; the treant hits two raiders.
        cast(2000, BALANCE_C, 65861, 0),
        dmg(4000, TREANT, TANK, 0),
        dmg(5000, TREANT, ROGUE, 0),

        # Grounding at 6.5 s: Shiv holds it until it's gone, Bulwark walks off it while it stands.
        cast(6500, ENH_C, 65989, 0),
        note(7500, ROGUE, fc.TOTEM, GROUNDING),
        note(8000, TANK, fc.TOTEM, GROUNDING),
        note(9000, TANK, fc.TOTEM, 0),
        note(12000, ROGUE, fc.TOTEM, 0),
        {"t": 7600, "e": "act", "g": ROGUE, "a": fc.KILL_TOTEM_ACTION, "rel": 100.0, "vd": "OK"},
        {"t": 7700, "e": "act", "g": ROGUE, "a": fc.KILL_TOTEM_ACTION, "rel": 100.0, "vd": "FAILED"},
        {"t": 20000, "e": "end", "out": "wipe"},
    ]
    records += [adds_snap(when) for when in range(-1000, 20000, 1000)]
    return records


class CcAndAdds(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_1_shocuul_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in adds_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_fc_ccaura_spans_add_up_per_spell(self):
        self.assertEqual(fc.cc_aura_held(self.trace), {WARRIOR_C: collections.Counter({33786: 6000})})

    def test_fear_uptime_counts_live_samples_from_the_pull(self):
        uptime = fc.fear_uptime(self.trace)
        self.assertEqual(uptime[MAGE_C], (2, 20))
        self.assertEqual(uptime[WARRIOR_C], (0, 20))

    def test_a_turn_lands_only_the_first_cast_whose_window_holds_it(self):
        rows = fc.cc_casts(self.trace)
        self.assertEqual({bot: (row["casts"], row["landed"]) for bot, row in rows.items()},
                         {BOOMKIN: (4, {"Cyclone": 1, "Entangling Roots": None}), LOCK: (2, {"Fear": 1})})

    def test_the_pace_counts_the_landings_it_can_read(self):
        self.assertEqual(fc.cc_pace(self.trace), (2, 20.0))

    def test_exhaustion_carried_in_is_a_first_row_removal(self):
        self.assertEqual([(row["member"], row["spell"], row["t"], row["left"]) for row in fc.lust_carriers(self.trace)],
                         [(ROGUE, 57723, 8000, 92000), (TANK, 57723, 15000, 0)])

    def test_instant_aoe_rows_just_before_the_cast_belong_to_it(self):
        self.assertEqual([(row["t"], row["name"], dict(row["hits"])) for row in fc.aoe_casts(self.trace)], [
            (1000, "Arcane Explosion", {TANK: 1}), (5000, "Arcane Explosion", {TANK: 1, ROGUE: 1})])

    def test_summons_and_their_units_leave_the_raid_totem_out(self):
        self.assertEqual([(row["t"], row["caster"], row["spell"]) for row in fc.summon_casts(self.trace)],
                         [(2000, BALANCE_C, 65861), (6500, ENH_C, 65989)])
        self.assertEqual([(row["guid"], row["first"], row["last"], row["damage"], row["victims"])
                          for row in fc.add_units(self.trace)],
                         [(TREANT, 3000, 8000, 2000, 2), (GROUNDING, 7000, 11000, 0, 0)])

    def test_a_totem_hold_is_gone_only_when_the_totem_left_inside_it(self):
        self.assertEqual([(row["bot"], row["totem"], row["start"], row["stop"], row["gone"])
                          for row in fc.totem_holds(self.trace)],
                         [(ROGUE, GROUNDING, 7500, 12000, True), (TANK, GROUNDING, 8000, 9000, False)])

    def test_the_new_sections_render(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            fc.show_cc(self.trace)
            fc.show_burst(self.trace)
            fc.show_aoe(self.trace)
            fc.show_adds(self.trace)
        text = out.getvalue()
        self.assertRegex(text, r"Noozle Whizzlestick +Mage +10\.0% of 20 +0% +-")
        self.assertRegex(text, r"Shocuul +Warrior +0\.0% of 20 +0% +Cyclone 33786 6\.0s")
        self.assertIn("Cyclone 3 (1 landed), Entangling Roots 1 (landed n/a)", text)
        self.assertIn("Fear 2 (1 landed)", text)
        self.assertIn("landed per 10 s: 1.00 (2 in 20 s)", text)
        self.assertIn("Exhaustion or Sated carried into the pull: 2 of 5, the last expired at 0:15.000", text)
        self.assertRegex(text, r"Shiv +Exhaustion +0:08\.000  cleared, 92 s left")
        self.assertIn("Force of Nature 0:02.000 (Kavina Grovesong)", text)
        self.assertRegex(text, r"Treant \(Kavina Grovesong\) +0:03\.000 +0:08\.000 +5\.0s  2000 to 2 raider\(s\)")
        self.assertIn("treants: 1, 2000 damage to the raid", text)
        self.assertIn(f"{fc.KILL_TOTEM_ACTION}: 1 OK row(s)", text)


GRIPPER = 5010  # death knight
HUNTER = 5011
WARRIOR = 5012
PALADIN_BOT = 5013
DRUID_BOT = 5014
HUMAN = 5015
HUMAN_DK = 5016
HUNTER_PET = (2 << 32) | 6
FILLERS = tuple(range(6001, 6010))


def placed(guid: int, x: float, y: float, hp: float, target: int) -> list:
    return [guid, x, y, 0.0, 0.0, hp, 80.0, target, 0, 0, 0, 0]


def pack_snap(when: int) -> dict:
    # the grip lands him 10 yd off, clear of the pack and the priest
    paladin = (6.0, 8.0) if when >= 11000 else (0.0, 0.0)
    units = [placed(guid, 1.0, 0.0, 100.0, PALADIN) for guid in FILLERS + (GRIPPER,)]
    units += [placed(ROGUE, 1.0, 0.0, 0.0 if when >= 20000 else 100.0, PALADIN),
              placed(PRIEST, 20.0, 0.0, 100.0, 0), placed(TANK, 1.0, 1.0, 100.0, ROGUE_C),
              placed(HUNTER, 25.0, 0.0, 100.0, ROGUE_C), placed(WARRIOR, 2.0, 1.0, 80.0, WARRIOR_C),
              placed(ROGUE_C, 2.0, 0.0, 100.0, PRIEST if when < 15000 else ROGUE),
              placed(WARRIOR_C, 0.0, 2.0, 100.0, WARRIOR), placed(MAGE_C, 30.0, 0.0, 100.0, PRIEST)]
    # flat at half health from 4 s, gone after the 24 s sample
    if when <= 24000:
        units.append(placed(PALADIN, *paladin, 100.0 if when < 4000 else 50.0, ROGUE))
    return {"t": when, "e": "snap", "u": units, "hz": []}


def pack_pull() -> list[dict]:
    roster = [{"g": guid, "n": f"Filler{guid}", "r": "melee", "c": 4, "h": 0} for guid in FILLERS]
    roster += [
        {"g": TANK, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
        {"g": ROGUE, "n": "Shiv", "r": "melee", "c": 4, "h": 0},
        {"g": PRIEST, "n": "Mercy", "r": "heal", "c": 5, "h": 0},
        {"g": GRIPPER, "n": "Obliteration", "r": "melee", "c": 6, "h": 0},
        {"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": 3, "h": 0},
        {"g": WARRIOR, "n": "Mighty", "r": "melee", "c": 1, "h": 0},
        {"g": PALADIN_BOT, "n": "Justice", "r": "melee", "c": 2, "h": 0},
        {"g": DRUID_BOT, "n": "Tree", "r": "heal", "c": 11, "h": 0},
        {"g": HUMAN, "n": "Dragon", "r": "tank", "c": 2, "h": 1},
        {"g": HUMAN_DK, "n": "Deathsong", "r": "melee", "c": 6, "h": 1}]
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 3, "boss": "velanaa",
         "roster": roster},
        {"t": 0, "e": "pull", "boss": "velanaa", "src": "engage"},
        {"t": 1, "e": "unit", "g": PALADIN, "en": 34465, "n": "Velanaa", "b": 1},
        {"t": 1, "e": "unit", "g": ROGUE_C, "en": 34472, "n": "Irieth Shadowstep", "b": 1},
        {"t": 1, "e": "unit", "g": WARRIOR_C, "en": 34475, "n": "Shocuul", "b": 1},
        {"t": 1, "e": "unit", "g": MAGE_C, "en": 34468, "n": "Noozle Whizzlestick", "b": 1},
        {"t": 1, "e": "unit", "g": HUNTER_PET, "en": 1, "n": "Cat", "own": HUNTER},
        note(1000, ROGUE, "fc.switch", "first"),
        note(1000, ROGUE, "fc.kill", PALADIN),
        note(25000, ROGUE, "fc.switch", "dead"),
        note(25000, ROGUE, "fc.kill", MAGE_C),

        # The human's grip at 8 s goes nowhere; the bot's duty at 9 s, cast at 10 s, released once it's out.
        cast(8000, HUMAN_DK, fc.SPELL_DEATH_GRIP, PALADIN),
        note(9000, GRIPPER, fc.GRIP, PALADIN),
        cast(10000, GRIPPER, fc.SPELL_DEATH_GRIP, PALADIN),
        note(10050, GRIPPER, fc.GRIP, 0),

        # Concussive Shot on the rogue champion while it's on the priest; Hamstring on the warrior champion
        # on a warrior bot at 80%.
        cast(12000, HUNTER, 5116, ROGUE_C),
        cast(13000, WARRIOR, 1715, WARRIOR_C),

        # Peels: Mighty's Disarm on duty before the warrior champion's Bladestorm, a Dismantle off duty, and
        # a Frost Nova outside any hold that doesn't count.
        note(11500, WARRIOR, fc.PEEL, WARRIOR_C),
        cast(11600, WARRIOR, 676, WARRIOR_C),
        note(11700, WARRIOR, fc.PEEL, 0),
        cast(12500, WARRIOR_C, fc.SPELL_BLADESTORM, 0),
        cast(14500, FILLERS[0], 51722, ROGUE_C),
        cast(15500, FILLERS[1], 42917, 0),
        # Trueshot's Freezing Arrow names no target, so it counts inside its hold only; Justice's Repentance
        # off duty
        note(18500, HUNTER, fc.PEEL, ROGUE_C),
        cast(18600, HUNTER, 60192, 0),
        note(18700, HUNTER, fc.PEEL, 0),
        cast(19000, PALADIN_BOT, 20066, ROGUE_C),
        cast(26000, HUNTER, 60192, 0),

        # Hand of Protection on the priest, Pain Suppression on Shiv, who dies at 20 s anyway.
        cast(14000, PALADIN_BOT, 10278, PRIEST),
        cast(15000, PRIEST, 33206, ROGUE),
        {"t": 20000, "e": "death", "g": ROGUE, "killer": ROGUE_C, "x": 1.0, "y": 0.0, "z": 0.0},

        # Rebirth on Shiv, two champions within 8 yd of where he fell.
        cast(22000, DRUID_BOT, 48477, ROGUE, 2000),

        # Taunts: a bot's Hand of Reckoning, the pet's Growl, the human's Taunt and Righteous Defense on
        # a friend. A taunt on a raider doesn't count.
        cast(16000, TANK, 62124, ROGUE_C),
        cast(16500, HUNTER_PET, 61676, WARRIOR_C),
        cast(17000, HUMAN, 355, ROGUE_C),
        cast(17500, TANK, 31789, PRIEST),
        cast(18000, TANK, 62124, PRIEST),
        {"t": 30000, "e": "end", "out": "wipe"},
    ]
    records += [pack_snap(when) for when in range(0, 30001, 1000)]
    return records


class PackAndPeel(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_1_velanaa_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in pack_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_champions_round_each_role_count_centre_to_centre(self):
        by_role = fc.pack_by_role(self.trace)
        # a melee bot has the paladin, the rogue and the warrior champion within 8 yd until the grip
        self.assertEqual(by_role["heal"], (0.0, 31))
        self.assertGreater(by_role["melee"][0], 2.0)
        self.assertLess(by_role["melee"][0], 3.0)

    def test_early_melee_deaths_and_the_healer_gap(self):
        self.assertEqual([death["g"] for death in fc.early_melee_deaths(self.trace)], [ROGUE])
        self.assertAlmostEqual(fc.melee_to_healer(self.trace), 19.0)

    def test_the_first_kill_is_the_hold_that_died(self):
        self.assertEqual(fc.first_kill(self.trace), 25000)

    def test_a_flat_span_with_ten_bots_on_the_kill_target_is_the_stall(self):
        stall = fc.longest_stall(self.trace)
        self.assertEqual((stall["guid"], stall["start"], stall["stop"], stall["low"], stall["high"]),
                         (PALADIN, 4000, 24000, 50.0, 50.0))

    def test_a_grip_reads_the_pack_before_and_after_how_far_it_moved_and_the_gap(self):
        self.assertEqual(fc.grips(self.trace), [
            {"t": 8000, "by": HUMAN_DK, "target": PALADIN, "kill": True, "before": 2, "after": 2, "moved": 0.0,
             "since": None},
            {"t": 10000, "by": GRIPPER, "target": PALADIN, "kill": True, "before": 2, "after": 0, "moved": 10.0,
             "since": 2000}])
        self.assertEqual(fc.grip_holds(self.trace), [
            {"bot": GRIPPER, "target": PALADIN, "start": 9000, "stop": 10050, "cast": True}])

    def test_a_snare_is_needed_when_its_champion_is_on_a_healer_or_a_hurt_raider(self):
        self.assertEqual([(row["spell"], row["needed"]) for row in fc.snare_casts(self.trace)],
                         [("Concussive Shot", True), ("Hamstring", False)])

    def test_a_peel_counts_its_duty_and_a_nova_needs_a_hold(self):
        self.assertEqual([(row["t"], row["by"], row["spell"], row["target"], row["duty"])
                          for row in fc.peel_casts(self.trace)],
                         [(11600, WARRIOR, "Disarm", WARRIOR_C, True),
                          (14500, FILLERS[0], "Dismantle", ROGUE_C, False),
                          (18600, HUNTER, "Freezing Arrow", ROGUE_C, True),
                          (19000, PALADIN_BOT, "Repentance", ROGUE_C, False)])

    def test_the_warrior_champion_reads_disarms_against_bladestorms(self):
        self.assertEqual(fc.warrior_disarms(self.trace),
                         [{"guid": WARRIOR_C, "bladestorms": [12500], "disarms": [11600]}])

    def test_externals_read_whether_the_target_lived(self):
        self.assertEqual([(row["spell"], row["target"], row["died"]) for row in fc.external_casts(self.trace)],
                         [("Hand of Protection", PRIEST, False), ("Pain Suppression", ROGUE, True)])

    def test_a_rebirth_counts_champions_where_its_target_fell(self):
        self.assertEqual(fc.battle_res_casts(self.trace),
                         [{"t": 22000, "by": DRUID_BOT, "target": ROGUE, "near": 2, "again": None}])

    def test_taunts_split_bots_and_humans_and_skip_a_raider(self):
        self.assertEqual(fc.taunt_casts(self.trace), collections.Counter({
            ("Hand of Reckoning", "bot"): 1, ("Growl (pet)", "bot"): 1, ("Taunt", "human"): 1,
            ("Righteous Defense", "bot"): 1}))

    def test_both_sections_render(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            fc.show_pack(self.trace)
            fc.show_peel(self.trace)
        text = out.getvalue()
        self.assertIn("melee deaths in the first 60 s: 1 (Shiv)", text)
        self.assertIn("first kill: 25.0 s from the pull", text)
        self.assertRegex(text, r"longest flat span: Velanaa 0:04\.000 for 20\.0 s, 50-50% STALL")
        self.assertIn(f"{fc.GRIP} holds: 1, 1 ending in the bot's grip", text)
        self.assertRegex(text, r"Deathsong\* +Velanaa .* +-\n")
        self.assertRegex(text, r"Obliteration +Velanaa .* 10\.0 +2\.0s\n")
        self.assertRegex(text, r"Disarm +warrior +1  1 / 0")
        self.assertIn("Shocuul (Warrior): Bladestorm 1 (first 0:12.500), disarms 1 (first 0:11.600); disarmed before"
                      " the first Bladestorm: yes", text)
        self.assertRegex(text, r"Concussive Shot +hunter +1  1 / 0 / 0")
        self.assertRegex(text, r"Pain Suppression +Mercy +Shiv +melee +1\+1 +yes")
        self.assertIn("taunts on champions: Growl (pet) bot 1, Hand of Reckoning bot 1, Righteous Defense bot 1,"
                      " Taunt human 1", text)


def ball_snap(when: int) -> dict:
    # pulled 50 yd off the pack at 5 s; the mage champion is loose on the priest all along
    paladin = (50.0, 0.0) if when >= 5000 else (2.0, 0.0)
    units = [placed(TANK, 0.0, 0.0, 100.0, ROGUE_C), placed(PRIEST, 60.0, 0.0, 100.0, 0),
             placed(HUNTER, 55.0, 0.0, 100.0, PALADIN),
             placed(PALADIN, *paladin, 80.0, HUNTER), placed(DISC, 0.0, 2.0, 100.0, TANK),
             placed(ROGUE_C, 1.0, 0.0, 100.0, TANK), placed(WARRIOR_C, 0.0, -1.0, 100.0, TANK),
             placed(MAGE_C, 40.0, 0.0, 100.0, PRIEST)]
    return {"t": when, "e": "snap", "u": units, "hz": []}


def ball_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 3, "boss": "velanaa",
         "roster": [{"g": TANK, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": PRIEST, "n": "Mercy", "r": "heal", "c": 5, "h": 0},
                    {"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": 3, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "velanaa", "src": "engage"},
        {"t": 1, "e": "unit", "g": PALADIN, "en": 34465, "n": "Velanaa", "b": 1},
        {"t": 1, "e": "unit", "g": DISC, "en": 34466, "n": "Anthar Forgemender", "b": 1},
        {"t": 1, "e": "unit", "g": ROGUE_C, "en": 34472, "n": "Irieth Shadowstep", "b": 1},
        {"t": 1, "e": "unit", "g": WARRIOR_C, "en": 34475, "n": "Shocuul", "b": 1},
        {"t": 1, "e": "unit", "g": MAGE_C, "en": 34468, "n": "Noozle Whizzlestick", "b": 1},
        note(0, TANK, "fc.switch", "first"),
        note(0, TANK, "fc.kill", PALADIN),
        # Trueshot opens; a champion's own cast on the raid and a later raid cast don't count
        cast(40, PALADIN, 66113, PALADIN),
        cast(50, HUNTER, 3044, PALADIN),
        cast(900, TANK, 62124, ROGUE_C),
        {"t": 20000, "e": "end", "out": "wipe"},
    ]
    records += [ball_snap(when) for when in range(0, 20001, 1000)]
    return records


class DeathBall(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_1_velanaa_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in ball_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_the_anchor_is_where_the_champions_landed(self):
        x, y = fc.pack_anchor(self.trace)
        self.assertAlmostEqual(x, 8.6)
        self.assertAlmostEqual(y, 0.2)

    def test_windows_split_champion_targets_by_role_and_measure_the_raid_off_the_pack(self):
        first = fc.ball_windows(self.trace)[0]
        self.assertEqual(first["targets"], collections.Counter({"tank": 30, "ranged": 10, "heal": 10}))
        # the pack centre is the three champions on the tank, the loose mage left out
        self.assertAlmostEqual(first["ranged"], ((55 - 1 / 3) ** 2 + (1 / 3) ** 2) ** 0.5)

    def test_the_pulled_kill_target_reads_off_the_pack_and_clear_of_the_healers(self):
        (row,) = fc.kill_isolation(self.trace)
        self.assertEqual(row["guid"], PALADIN)
        self.assertAlmostEqual(row["clear"], 0.75)
        self.assertGreater(row["pack"], 49.0)

    def test_escapees_count_seconds_off_the_landing_spot_not_on_a_tank(self):
        self.assertEqual(fc.escapee_seconds(self.trace), {MAGE_C: 20.0, PALADIN: 16.0})

    def test_the_start_is_the_first_raid_cast_and_each_champions_first_pick(self):
        self.assertEqual(fc.first_engage(self.trace), {"t": 50, "by": HUNTER, "spell": 3044, "target": PALADIN})
        self.assertEqual(fc.first_picks(self.trace), [
            (0, PALADIN, HUNTER), (0, DISC, TANK), (0, ROGUE_C, TANK), (0, MAGE_C, PRIEST), (0, WARRIOR_C, TANK)])

    def test_the_section_renders(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            fc.show_ball(self.trace)
        text = out.getvalue()
        self.assertRegex(text, r"0:00\.000 +60/  0/ 20/ 20/  0")
        self.assertRegex(text, r"Velanaa +0:00\.000 +20\.0s +49\.\d +75%")
        self.assertIn("first raid cast on a champion: 0:00.050 Trueshot (bot)", text)
        self.assertIn("first picks: tank 3 of 5 (60%), melee 0, ranged 1, heal 1", text)
        self.assertIn("escapees, s off the landing spot not on a tank or a holder: Noozle Whizzlestick 20,"
                      " Velanaa 16", text)


def trinket_pull() -> list[dict]:
    roster = [{"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": 3, "h": 0},
              {"g": PRIEST, "n": "Mercy", "r": "heal", "c": 5, "h": 0},
              {"g": MAGE, "n": "Frost", "r": "ranged", "c": 8, "h": 0}]
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 3, "boss": "velanaa",
         "roster": roster},
        {"t": 0, "e": "pull", "boss": "velanaa", "src": "engage"},
        {"t": 1, "e": "unit", "g": HUNTER_PET, "en": 1, "n": "Cat", "own": HUNTER},
        {"t": 1, "e": "unit", "g": DISC, "en": 34466, "n": "Anthar Forgemender", "b": 1},
        {"t": 1, "e": "unit", "g": ROGUE_C, "en": 34472, "n": "Irieth Shadowstep", "b": 1},
        {"t": 1, "e": "unit", "g": WARRIOR_C, "en": 34475, "n": "Shocuul", "b": 1},
        {"t": 1, "e": "unit", "g": MAGE_C, "en": 34468, "n": "Noozle Whizzlestick", "b": 1},
        {"t": 1, "e": "spell", "sp": 24394, "n": "Intimidation"},
        # Mercy's Psychic Scream under fc.opener; she steps in to 3 yd of the rogue as she casts, which only
        # the snapshot after the cast shows. The Arcane Shot is too early to count.
        cast(1000, HUNTER, 3044, ROGUE_C),
        note(2500, PRIEST, fc.OPENER, 10890),
        cast(2900, PRIEST, 10890, 0),
        note(3000, PRIEST, fc.OPENER, 0),
        cast(3000, ROGUE_C, fc.SPELL_PVP_TRINKET, ROGUE_C),
        # the pet's Intimidation on the warrior pops it; Frost's later Frostbolt on it pops nothing
        cast(5800, HUNTER_PET, 24394, WARRIOR_C),
        cast(5900, MAGE, 116, WARRIOR_C),
        cast(6000, WARRIOR_C, fc.SPELL_PVP_TRINKET, WARRIOR_C),
        # Frost held the opener without casting it; the later Nova is out of the mage champion's reach, an
        # Arcane Shot pops nothing, and a champion's heal on it is no raid cast
        note(7000, MAGE, fc.OPENER, 42917),
        note(7500, MAGE, fc.OPENER, 0),
        cast(8500, MAGE, 42917, 0),
        cast(8700, HUNTER, 3044, MAGE_C),
        cast(8800, DISC, 66104, MAGE_C, 1500),
        cast(9000, MAGE_C, fc.SPELL_PVP_TRINKET, MAGE_C),
        {"t": 10000, "e": "end", "out": "wipe"},
    ]
    for when in range(0, 10001, 1000):
        records.append({"t": when, "e": "snap", "hz": [], "u": [
            placed(PRIEST, 30.0 if when < 3000 else 2.0, 0.0, 100.0, 0), placed(MAGE, 20.0, 0.0, 100.0, 0),
            placed(HUNTER, 50.0, 50.0, 100.0, 0), placed(ROGUE_C, 5.0, 0.0, 100.0, PRIEST),
            placed(WARRIOR_C, 25.0, 0.0, 100.0, MAGE), placed(MAGE_C, 60.0, 0.0, 100.0, HUNTER),
            placed(DISC, 60.0, 5.0, 100.0, HUNTER)]})
    return records


class Trinkets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_1_velanaa_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in trinket_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_trinket_names_an_opener_aoe_in_reach_else_a_cc_cast_on_it_else_none(self):
        self.assertEqual(fc.trinket_triggers(self.trace), [
            {"t": 3000, "champion": ROGUE_C, "spell": 10890, "by": PRIEST, "at": 2900, "aoe": True, "opener": True},
            {"t": 6000, "champion": WARRIOR_C, "spell": 24394, "by": HUNTER_PET, "at": 5800, "aoe": False,
             "opener": False},
            {"t": 9000, "champion": MAGE_C, "spell": None, "by": None, "at": None, "aoe": False, "opener": False}])

    def test_an_opener_hold_reads_whether_its_holder_cast(self):
        self.assertEqual([(hold["bot"], hold["spell"], hold["start"], hold["stop"], hold["cast"])
                          for hold in fc.opener_holds(self.trace)],
                         [(PRIEST, 10890, 2500, 3000, True), (MAGE, 42917, 7000, 7500, False)])

    def test_the_section_renders(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            fc.show_trinket(self.trace)
        text = out.getvalue()
        self.assertIn("fc.opener holds: 2, 1 with the holder's cast", text)
        self.assertRegex(text, r"0:02\.500-0:03\.000 +Mercy +Psychic Scream +cast")
        self.assertRegex(text, r"0:07\.000-0:07\.500 +Frost +Frost Nova +-")
        self.assertRegex(text, r"0:03\.000 Irieth Shadowstep +Rogue +Psychic Scream +Mercy +0\.1s +yes +yes")
        self.assertRegex(text, r"0:06\.000 Shocuul +Warrior +Intimidation +Cat \(Trueshot\) +0\.2s +- +-")
        self.assertRegex(text, r"0:09\.000 Noozle Whizzlestick +Mage +none seen +- +- +- +-")
        self.assertIn("by trigger: Intimidation 1, Psychic Scream 1, none seen 1", text)
        self.assertIn("burned by opener AoE: 1 of 3 (33%), 1 by an fc.opener holder", text)


class Source(unittest.TestCase):
    def test_the_kill_order_covers_the_champion_enum(self):
        text = CHAMPIONS_HEADER.read_text(encoding="utf-8")
        body = re.search(r"enum class ToCFactionChampions\b[^{]*\{(.*?)\};", text, re.S)
        self.assertIsNotNone(body, f"no ToCFactionChampions enum in {CHAMPIONS_HEADER}")
        entries = {int(value) for value in re.findall(r"^\s*\w+\s*=\s*(\d+)", body.group(1), re.M)}
        self.assertEqual(entries, set(fc.SPEC_OF))

    def test_burst_names_come_from_the_source(self):
        self.assertTrue({"bloodlust", "heroism", "offensive potion"} <= fc.burst_actions())


CHAIN_TANK = 5017  # a DK tank


def chain_snap(when: int) -> dict:
    # Felesta's grip at 2 s lands her 25 yd out, Obliteration's at 3.5 s 50, Deathsong's at 6 s 75
    x = 0.0 if when < 3000 else 25.0 if when < 4000 else 50.0 if when < 7000 else 75.0
    return {"t": when, "e": "snap", "hz": [],
            "u": [placed(PALADIN, x, 0.0, 100.0, CHAIN_TANK), placed(ROGUE_C, 0.0, 0.0, 100.0, CHAIN_TANK)]}


def chain_pull() -> list[dict]:
    roster = [{"g": CHAIN_TANK, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
              {"g": GRIPPER, "n": "Obliteration", "r": "melee", "c": 6, "h": 0},
              {"g": HUMAN_DK, "n": "Deathsong", "r": "melee", "c": 6, "h": 1}]
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 3, "boss": "velanaa",
         "roster": roster},
        {"t": 0, "e": "pull", "boss": "velanaa", "src": "engage"},
        {"t": 1, "e": "unit", "g": PALADIN, "en": 34465, "n": "Velanaa", "b": 1},
        {"t": 1, "e": "unit", "g": ROGUE_C, "en": 34472, "n": "Irieth Shadowstep", "b": 1},
        {"t": 1, "e": "unit", "g": HUNTER_PET, "en": 3825, "n": "Wolf", "own": GRIPPER},
        note(1000, CHAIN_TANK, "fc.chain", PALADIN),
        cast(2000, CHAIN_TANK, fc.SPELL_DEATH_GRIP, PALADIN),
        cast(2000, PALADIN, fc.SPELL_GRIP_PULL, 0, triggered=True),
        cast(3500, GRIPPER, fc.SPELL_DEATH_GRIP, PALADIN),
        note(3600, GRIPPER, "fc.chain", 0),
        cast(6000, HUMAN_DK, fc.SPELL_DEATH_GRIP, PALADIN),
        cast(6000, PALADIN, fc.SPELL_GRIP_PULL, 0, triggered=True),
        cast(6011, HUNTER_PET, 53148, PALADIN, triggered=True),
        # past the pull's tail, so a bot grip outside it
        cast(30000, GRIPPER, fc.SPELL_DEATH_GRIP, ROGUE_C),
        {"t": 32000, "e": "end", "out": "wipe"},
    ]
    return records + [chain_snap(when) for when in range(0, 32001, 1000)]


class GripChain(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_1_velanaa_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in chain_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_pull_reads_its_links_with_carry_and_distance_off_the_pack(self):
        chains, outside = fc.chain_links(self.trace)
        self.assertEqual(len(chains), 1)
        self.assertEqual((chains[0]["target"], chains[0]["start"], chains[0]["stop"]), (PALADIN, 1000, 3600))
        self.assertEqual([(link["t"], link["by"], link["moved"], link["off"]) for link in chains[0]["links"]],
                         [(2000, CHAIN_TANK, 25.0, 25.0), (3500, GRIPPER, 25.0, 50.0), (6000, HUMAN_DK, 25.0, 75.0)])
        self.assertEqual([(row["t"], row["by"]) for row in outside], [(30000, GRIPPER)])

    def test_each_link_reads_its_pull_and_what_cut_it(self):
        chains, _ = fc.chain_links(self.trace)
        self.assertEqual([(link["pulled"], link["cut"]) for link in chains[0]["links"]],
                         [(True, []), (False, []), (True, [(11, HUNTER_PET, "pet Charge")])])
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            fc.show_chain(self.trace)
        self.assertRegex(out.getvalue(), r"Obliteration +bot DK .* no pull\n")
        self.assertRegex(out.getvalue(), r"Deathsong .* cut by Wolf \(Obliteration\) pet Charge \+11ms")

    def test_the_order_is_tank_bot_dk_then_the_player(self):
        self.assertEqual([fc.link_role(self.trace, guid) for guid in (CHAIN_TANK, GRIPPER, HUMAN_DK)],
                         fc.CHAIN_ORDER)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            fc.show_chain(self.trace)
        self.assertIn("pull on Velanaa at 0:01.000, open 2.6 s: in order", out.getvalue())
        self.assertIn("bot grips outside a pull: 1 (Obliteration 0:30.000 on Irieth Shadowstep)", out.getvalue())


MIGHTY = 5020  # a warrior bot holding a champion
DK_C = CREATURE | 19


def hold_snap(when: int) -> dict:
    # the rogue champion turns to the priest at 8 s; the warrior champion is gone after the 14 s sample
    rogue_on = MIGHTY if when < 8000 or when >= 10000 else PRIEST
    units = [
        placed(PRIEST, 0.0, 0.0, 100.0, 0),
        placed(MIGHTY, 41.0, 0.0, 100.0, ROGUE_C),
        placed(ROGUE, 0.0, 36.0, 0.0 if when >= 8000 else 100.0, WARRIOR_C),
        placed(TANK, 0.0, 36.0, 100.0, WARRIOR_C),
        placed(ROGUE_C, 40.0, 0.0, 100.0, rogue_on),
        placed(PALADIN, -30.0, 0.0, 100.0, TANK),
        placed(DISC, -30.0, 0.0, 100.0, TANK),
        placed(MAGE_C, -30.0, 0.0, 100.0, TANK),
        placed(DK_C, -30.0, 0.0, 100.0, TANK),
    ]
    if when <= 14000:
        units.append(placed(WARRIOR_C, 0.0, 35.0, 100.0, ROGUE if when < 8000 else TANK))
    return {"t": when, "e": "snap", "hz": [], "u": units}


def hold_pull() -> list[dict]:
    roster = [{"g": TANK, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
              {"g": MIGHTY, "n": "Mighty", "r": "melee", "c": 1, "h": 0},
              {"g": ROGUE, "n": "Shadow", "r": "melee", "c": 4, "h": 0},
              {"g": PRIEST, "n": "Prayer", "r": "heal", "c": 5, "h": 0}]
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 3, "boss": "velanaa",
         "roster": roster},
        {"t": 0, "e": "pull", "boss": "velanaa", "src": "engage"},
        {"t": 1, "e": "unit", "g": PALADIN, "en": 34465, "n": "Velanaa", "b": 1},
        {"t": 1, "e": "unit", "g": DISC, "en": 34466, "n": "Anthar Forgemender", "b": 1},
        {"t": 1, "e": "unit", "g": ROGUE_C, "en": 34472, "n": "Irieth Shadowstep", "b": 1},
        {"t": 1, "e": "unit", "g": WARRIOR_C, "en": 34475, "n": "Shocuul", "b": 1},
        {"t": 1, "e": "unit", "g": MAGE_C, "en": 34468, "n": "Noozle Whizzlestick", "b": 1},
        {"t": 1, "e": "unit", "g": DK_C, "en": 34461, "n": "Gorgrim Shadowcleave", "b": 1},
        note(-500, TANK, fc.HOLD, 0),
        note(0, TANK, "fc.kill", PALADIN),
        # Mighty holds the rogue until it turns kill target
        note(0, MIGHTY, fc.HOLD, ROGUE_C),
        note(10000, TANK, "fc.kill", ROGUE_C),
        note(10000, MIGHTY, fc.HOLD, 0),
        # Shadow holds the warrior and dies on it, the tank takes it over until it dies
        note(2000, ROGUE, fc.HOLD, WARRIOR_C),
        {"t": 7500, "e": "death", "g": ROGUE, "killer": WARRIOR_C, "x": 0.0, "y": 36.0, "z": 0.0},
        note(8000, ROGUE, fc.HOLD, 0),
        note(8000, TANK, fc.HOLD, WARRIOR_C),
        note(15000, TANK, fc.HOLD, 0),
        {"t": 20000, "e": "end", "out": "wipe"},
    ]
    return records + [hold_snap(when) for when in range(0, 20000, 1000)]


class Holds(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_1_velanaa_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in hold_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_each_hold_reads_the_share_on_its_holder_the_healer_gap_and_the_deaths(self):
        self.assertEqual(fc.hold_rows(self.trace), [
            {"bot": MIGHTY, "champion": ROGUE_C, "start": 0, "stop": 10000, "samples": 10, "on": 0.8,
             "healer": 40.0, "others": collections.Counter({PRIEST: 2.0}), "holder_died": False, "died": False},
            {"bot": ROGUE, "champion": WARRIOR_C, "start": 2000, "stop": 8000, "samples": 6, "on": 1.0,
             "healer": 35.0, "others": collections.Counter(), "holder_died": True, "died": False},
            {"bot": TANK, "champion": WARRIOR_C, "start": 8000, "stop": 15000, "samples": 7, "on": 1.0,
             "healer": 35.0, "others": collections.Counter(), "holder_died": False, "died": True}])

    def test_the_section_renders(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            fc.show_hold(self.trace)
        text = out.getvalue()
        self.assertIn("fc.hold: Bulwark (paladin), Mighty (warrior), Shadow (rogue)", text)
        self.assertRegex(text, r"Mighty +Irieth Shadowstep +Rogue +0:00\.000 +10\.0s +80% +40\.0 +- +-  Prayer 2\.0s")
        self.assertRegex(text, r"Shadow +Shocuul +Warrior +0:02\.000 +6\.0s +100% +35\.0 +yes +-  -")
        self.assertRegex(text, r"Bulwark +Shocuul +Warrior +0:08\.000 +7\.0s +100% +35\.0 +- +yes  -")
        self.assertIn("melee champions never held: Gorgrim Shadowcleave", text)
        self.assertIn("on holder, median over the holds: 100%", text)

    def test_a_champion_on_a_holder_is_no_escapee_only_while_the_hold_lasts(self):
        # the rogue is loose on the priest at 8-9 s and on Mighty once the hold is over; the warrior is
        # on Shadow a second before his hold opens
        self.assertEqual(fc.escapee_seconds(self.trace), {ROGUE_C: 12.0, WARRIOR_C: 1.0})


def stands_pull() -> list[dict]:
    roster = [{"g": MAGE, "n": "Frost", "r": "ranged", "c": 8, "h": 0},
              {"g": PRIEST, "n": "Prayer", "r": "heal", "c": 5, "h": 0}]
    stand = fc.STAND_ACTIONS[0]

    def move(when: int, guid: int, x: float, by: str = stand) -> dict:
        return {"t": when, "e": "move", "g": guid, "k": "point", "x": x, "y": 0.0, "z": 0.0, "by": by, "ok": 1}

    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 3, "boss": "velanaa",
         "roster": roster},
        {"t": 0, "e": "pull", "boss": "velanaa", "src": "engage"},
        {"t": 1, "e": "unit", "g": PALADIN, "en": 34465, "n": "Velanaa", "b": 1},
        # Frost's slot jumps 10 yd at 2 s, its 3 s target is the same slot restated
        move(1000, MAGE, 0.0), move(2000, MAGE, 10.0), move(3000, MAGE, 10.2),
        # Prayer's moving samples at 1 and 2 s are its step off, the one at 0 s comes before any move row
        move(500, PRIEST, 0.0, "faction champions step off"),
        # the raid takes 2k and Prayer heals 2k of a 3k heal
        dmg(5000, PALADIN, MAGE, 0), dmg(5500, PALADIN, MAGE, 0),
        {"t": 5600, "e": "heal", "s": PRIEST, "d": MAGE, "sp": 48071, "a": 3000, "oh": 1000, "hp": 100.0},
        # Prayer runs from three champions, Frost from the focus the bots expect next, then Prayer boosts a
        # walk at 60% with no note
        note(3990, PRIEST, fc.NITRO, "focus 3"),
        cast(4000, PRIEST, fc.SPELL_NITRO, 0),
        move(4010, PRIEST, 25.0, "faction champions chased"),
        note(6000, MAGE, fc.NITRO, "next"),
        cast(6000, MAGE, fc.SPELL_NITRO, 0),
        cast(8000, PRIEST, fc.SPELL_NITRO, 0),
        move(8010, PRIEST, 10.0, "reach spell"),
        {"t": 10000, "e": "end", "out": "wipe"},
    ]
    for when in range(0, 10000, 1000):
        frost = 0.0 if when < 2000 else 5.0 if when < 3000 else 10.0
        # Prayer moves the first 3 s and casts from 6 s
        prayer = placed(PRIEST, 0.0, 0.0, 60.0 if when >= 7000 else 100.0, 0)
        prayer[8] = int(when < 3000)
        prayer[10] = 48071 if when >= 6000 else 0
        records.append({"t": when, "e": "snap", "hz": [], "u": [placed(MAGE, frost, 0.0, 100.0, 0), prayer,
                                                                 placed(PALADIN, 40.0, 0.0, 100.0, PRIEST)]})
    return records


class StandsAndBoosts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_1_velanaa_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in stands_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_stand_counts_its_moves_its_slot_jumps_and_path_against_net(self):
        self.assertEqual(fc.stand_rows(self.trace),
                         [{"bot": MAGE, "moves": 3, "jumps": 1, "path": 10.0, "net": 10.0},
                          {"bot": PRIEST, "moves": 1, "jumps": 0, "path": 0.0, "net": 0.0}])

    def test_a_moving_sample_goes_to_the_last_mover(self):
        self.assertEqual(fc.mover_rows(self.trace), [
            {"bot": MAGE, "samples": 10, "moving": 0.0, "by": collections.Counter()},
            {"bot": PRIEST, "samples": 10, "moving": 0.3,
             "by": collections.Counter({"faction champions step off": 2, "none": 1})}])

    def test_healing_reads_effective_against_damage_taken(self):
        self.assertEqual(fc.heal_output(self.trace),
                         {"secs": 10.0, "taken": 2000, "healed": 2000, "healers": [(PRIEST, 2000)]})

    def test_a_boost_reads_its_reason_health_and_the_move_it_served(self):
        rows = fc.nitro_rows(self.trace)
        self.assertEqual(rows, [
            {"t": 4000, "bot": PRIEST, "reason": "focus 3", "hp": 100.0, "by": "faction champions chased",
             "dist": 25.0},
            {"t": 6000, "bot": MAGE, "reason": "next", "hp": 100.0, "by": None, "dist": None},
            {"t": 8000, "bot": PRIEST, "reason": "walk", "hp": 60.0, "by": "reach spell", "dist": 10.0}])
        self.assertEqual(fc.nitro_tally(rows), [("focus", 1), ("next", 1), ("walk", 1)])

    def test_a_healer_reads_its_moving_and_casting_share(self):
        self.assertEqual(fc.healer_activity(self.trace),
                         [{"bot": PRIEST, "samples": 10, "moving": 0.3, "casting": 0.4}])

    def test_the_section_renders(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            fc.show_stands(self.trace)
        self.assertRegex(out.getvalue(), r"Frost +ranged +3 +1 +10 +10")
        self.assertRegex(out.getvalue(), r"healer Prayer +moving +30% +casting +40%")
        self.assertRegex(out.getvalue(), r"Prayer +heal +30%: faction champions step off 20%, none 10%")
        self.assertIn("raid healed 0.2k/s effective, took 0.2k/s; per healer: Prayer 2k", out.getvalue())
        self.assertIn("Nitro Boosts: 3 use(s): focus 1, next 1, walk 1", out.getvalue())
        self.assertRegex(out.getvalue(), r"0:04\.000 Prayer +focus 3 +100%  faction champions chased 25 yd")
        self.assertRegex(out.getvalue(), r"0:06\.000 Frost +next +100%  - -")
        self.assertRegex(out.getvalue(), r"0:08\.000 Prayer +walk +60%  reach spell 10 yd")


MOON = 5021  # a balance druid bot


def control_pull() -> list[dict]:
    roster = [{"g": TANK, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
              {"g": ROGUE, "n": "Shiv", "r": "melee", "c": 4, "h": 0},
              {"g": MOON, "n": "Moon", "r": "ranged", "c": 11, "h": 0},
              {"g": PALADIN_BOT, "n": "Holylight", "r": "heal", "c": 2, "h": 0}]
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 3, "boss": "velanaa",
         "roster": roster},
        {"t": 0, "e": "pull", "boss": "velanaa", "src": "engage"},
        {"t": 1, "e": "unit", "g": PALADIN, "en": 34465, "n": "Velanaa", "b": 1},
        {"t": 1, "e": "unit", "g": DISC, "en": 34466, "n": "Caiphus", "b": 1},
        {"t": 1, "e": "unit", "g": ROGUE_C, "en": 34472, "n": "Irieth", "b": 1},
        {"t": 1, "e": "unit", "g": WARRIOR_C, "en": 34475, "n": "Narrhok", "b": 1},
        {"t": 1, "e": "unit", "g": PET_C, "en": 35610, "n": "Cat", "own": WARRIOR_C},
        {"t": 1, "e": "spell", "sp": 498, "n": "Divine Protection"},
        note(0, MOON, fc.KILL, PALADIN),
        note(0, MOON, fc.DANGER, ROGUE_C),
        # Moon's Cyclone on the rogue ends as its next one lands on the warrior
        cast(1000, MOON, fc.SPELL_CYCLONE, ROGUE_C, ct=500),
        note(1500, ROGUE_C, fc.CC_AURA, fc.SPELL_CYCLONE),
        note(1000, MOON, fc.CYCLONE, DISC),
        cast(2000, MOON, fc.SPELL_CYCLONE, WARRIOR_C, ct=500),
        note(2500, ROGUE_C, fc.CC_AURA, 0),
        note(2500, WARRIOR_C, fc.CC_AURA, fc.SPELL_CYCLONE),
        note(8500, WARRIOR_C, fc.CC_AURA, 0),
        note(9000, MOON, fc.CYCLONE, 0),
        cast(3000, MOON, 48461, PALADIN, ct=1500),
        cast(4500, MOON, 48461, PALADIN, ct=1500),
        # the rogue disarmed and stunned
        cast(2000, ROGUE, 51722, ROGUE_C),
        cast(3000, PALADIN_BOT, 10308, ROGUE_C),
        # the tank's defensive, Lay on Hands, death, Rebirth and second death
        dmg(1000, ROGUE_C, TANK, 0), dmg(2000, ROGUE_C, TANK, 0), dmg(3000, ROGUE_C, TANK, 0),
        dmg(3500, PET_C, TANK, 0),
        cast(5000, TANK, 498, 0),
        cast(5500, PALADIN_BOT, 48788, TANK),
        {"t": 6000, "e": "death", "g": TANK, "killer": ROGUE_C, "x": 0.0, "y": 0.0, "z": 0.0},
        cast(7000, MOON, 48477, TANK, ct=2000),
        {"t": 9500, "e": "death", "g": TANK, "killer": ROGUE_C, "x": 0.0, "y": 0.0, "z": 0.0},
        # Divine Sacrifice: Divine Guardian lands on three with it, a later one doesn't count
        cast(4000, PALADIN_BOT, fc.SPELL_DIVINE_SACRIFICE, 0),
        aura(4005, TANK, PALADIN_BOT, fc.SPELL_DIVINE_GUARDIAN, False, 6000),
        aura(4005, MOON, PALADIN_BOT, fc.SPELL_DIVINE_GUARDIAN, False, 6000),
        aura(4006, PALADIN_BOT, PALADIN_BOT, fc.SPELL_DIVINE_GUARDIAN, False, 6000),
        aura(5000, ROGUE, PALADIN_BOT, fc.SPELL_DIVINE_GUARDIAN, False, 5000),
        cast(4200, PALADIN_BOT, 31821, 0),
        # the kill target died and the bots chained the priest a second later
        note(6000, MOON, fc.SWITCH, "dead"),
        note(7000, TANK, fc.CHAIN, DISC),
        note(8000, TANK, fc.CHAIN, 0),
        {"t": 10000, "e": "end", "out": "wipe"},
    ]
    for when in range(0, 10000, 1000):
        records.append({"t": when, "e": "snap", "hz": [], "u": [
            placed(MOON, 20.0, 0.0, 100.0, PALADIN), placed(TANK, -5.0, 0.0, 40.0, ROGUE_C),
            placed(PALADIN, 0.0, 0.0, 100.0, MOON), placed(DISC, 40.0, 0.0, 100.0, TANK),
            placed(ROGUE_C, -5.0, 1.0, 100.0, TANK), placed(WARRIOR_C, -5.0, -1.0, 100.0, TANK)]})
    return records


class ControlAndDefence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_1_velanaa_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in control_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def render(self, section) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            section(self.trace)
        return out.getvalue()

    def test_a_cyclone_ended_by_its_casters_next_one_is_broken(self):
        self.assertEqual(fc.own_breaks(self.trace), {MOON: collections.Counter({"holds": 2, "broken": 1})})
        self.assertIn("ended by its caster's next one elsewhere: Moon 1 of 2", self.render(fc.show_cc))

    def test_the_cyclone_stand_reads_both_reaches(self):
        self.assertEqual(fc.cyclone_rows(self.trace), [
            {"bot": MOON, "cyclones": 2, "damage": 2, "secs": 8.0, "kill": 20.0, "healer": 20.0, "both": 1.0}])
        self.assertIn("Moon: Cyclone 2, casts on the kill target 2; fc.cyclone 8 s", self.render(fc.show_stands))

    def test_lay_on_hands_is_an_external_and_a_rebirth_reads_the_next_death(self):
        self.assertIn({"t": 5500, "by": PALADIN_BOT, "spell": "Lay on Hands", "target": TANK, "died": True,
                       "on": (2, 1), "reach": None},
                      fc.external_casts(self.trace))
        self.assertEqual([(row["target"], row["again"]) for row in fc.battle_res_casts(self.trace)], [(TANK, 2500)])
        self.assertRegex(self.render(fc.show_peel), r"Bulwark +tank +3 +\+2s")

    def test_damage_counts_a_pet_for_its_owner_and_the_danger_champions_control(self):
        rows = fc.champion_damage(self.trace)
        self.assertEqual([(row["guid"], row["damage"], row["kills"]) for row in rows],
                         [(ROGUE_C, 3000, 2), (WARRIOR_C, 1000, 0)])
        self.assertEqual(fc.melee_control(self.trace), [
            {"guid": ROGUE_C, "held": 1000, "disarms": 1, "stuns": 1, "cc": 1, "danger": True, "danger_ms": 10000},
            {"guid": WARRIOR_C, "held": 6000, "disarms": 0, "stuns": 0, "cc": 1, "danger": False,
             "danger_ms": 0}])
        text = self.render(fc.show_damage)
        self.assertIn("*Irieth", text)
        self.assertIn("danger 10.0s", text)
        self.assertRegex(text, r"Irieth +Rogue +3000 +75% +2")

    def test_divine_sacrifice_reads_who_divine_guardian_reached_and_raid_wide_casts_list(self):
        rows = [row for row in fc.external_casts(self.trace) if row["spell"] == "Divine Sacrifice"]
        self.assertEqual([(row["t"], row["target"], row["reach"]) for row in rows], [(4000, PALADIN_BOT, 3)])
        self.assertEqual(fc.raid_wide_casts(self.trace), [(4200, PALADIN_BOT, "Aura Mastery")])
        text = self.render(fc.show_peel)
        self.assertIn("Divine Guardian on 3", text)
        self.assertIn("raid-wide: Aura Mastery Holylight 0:04.200", text)

    def test_targeted_counts_samples_with_a_raider_on_the_champion(self):
        self.assertEqual(fc.targeted_share(self.trace),
                         {PALADIN: (10, 10), DISC: (0, 10), ROGUE_C: (10, 10), WARRIOR_C: (0, 10)})
        self.assertRegex(self.render(fc.show_cc), r"Irieth +Rogue +\S+% of 10 +100%")

    def test_a_chain_opened_after_a_dead_switch_reads_as_after_a_kill(self):
        chains, _ = fc.chain_links(self.trace)
        self.assertEqual([(chain["target"], chain["after_kill"]) for chain in chains], [(DISC, 1000)])
        self.assertIn("chain 1.0 s after a kill on Caiphus", self.render(fc.show_chain))

    def test_a_personal_reads_its_health_and_the_deaths_after_it(self):
        self.assertEqual(fc.personal_rows(self.trace),
                         [{"t": 5000, "bot": TANK, "spell": "Divine Protection", "hp": 40.0}])
        self.assertEqual([(row["t"], row["first"]["t"]) for row in fc.personal_deaths(self.trace)],
                         [(6000, 5000), (9500, 5000)])
        self.assertIn("Bulwark        tank   0:05.000 Divine Protection at 40%", self.render(fc.show_personal))


def focus_snap(when: int) -> dict:
    # all four on the mage to 1.5 s, on the priest from 2 s until he dies, then three on the tank
    if when < 2000:
        victim = MAGE
    elif when < 5000:
        victim = PRIEST
    else:
        victim = TANK
    picks = [victim] * 4 if victim != TANK else [TANK, TANK, TANK, 0]
    priest = 0.0 if when >= 5000 else (100.0 if when < 2000 else 100.0 - (when - 2000) / 50.0)
    mage = max(100.0 - when / 30.0, 50.0) if when < 2000 else 60.0
    units = [placed(TANK, 0.0, 0.0, 90.0, ROGUE_C), placed(MAGE, 20.0, 0.0, mage, PALADIN),
             placed(PRIEST, 25.0, 0.0, priest, 0), placed(MOON, 30.0, 0.0, 100.0, 0)]
    units += [placed(guid, 0.0, 1.0, 100.0, pick) for guid, pick in zip((PALADIN, DISC, ROGUE_C, WARRIOR_C), picks)]
    return {"t": when, "e": "snap", "hz": [], "u": units}


def focus_pull() -> list[dict]:
    roster = [{"g": TANK, "n": "Bulwark", "r": "tank", "c": 1, "h": 0},
              {"g": MAGE, "n": "Frost", "r": "ranged", "c": 8, "h": 0},
              {"g": PRIEST, "n": "Mercy", "r": "heal", "c": 5, "h": 0},
              {"g": MOON, "n": "Moon", "r": "ranged", "c": 11, "h": 0}]
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 3, "boss": "velanaa",
         "roster": roster},
        {"t": 0, "e": "pull", "boss": "velanaa", "src": "engage"},
        {"t": 1, "e": "unit", "g": PALADIN, "en": 34465, "n": "Velanaa", "b": 1},
        {"t": 1, "e": "unit", "g": DISC, "en": 34466, "n": "Caiphus", "b": 1},
        {"t": 1, "e": "unit", "g": ROGUE_C, "en": 34472, "n": "Irieth", "b": 1},
        {"t": 1, "e": "unit", "g": WARRIOR_C, "en": 34475, "n": "Narrhok", "b": 1},
        {"t": 1, "e": "spell", "sp": 47877, "n": "Master Healthstone"},
        # the mage's Ice Block inside its focus, the priest's Pain Suppression half a second before his
        cast(1200, MAGE, 45438, 0),
        cast(1500, PRIEST, 33206, PRIEST),
        {"t": 4800, "e": "death", "g": PRIEST, "killer": ROGUE_C, "x": 0.0, "y": 0.0, "z": 0.0},
        cast(3000, TANK, 47877, 0),
        # the bots named the priest before the re-pick at 1.9 s, and nobody before the one at 5.2 s
        note(1000, TANK, fc.NEXT_FOCUS, PRIEST),
        note(1900, TANK, fc.NEXT_FOCUS, 0),
        note(1900, TANK, fc.REPICK, 4),
        note(5200, TANK, fc.REPICK, 3),
        # a Cyclone that runs its whole 6 s out as the next one lands isn't broken
        cast(0, MOON, fc.SPELL_CYCLONE, ROGUE_C),
        note(0, ROGUE_C, fc.CC_AURA, fc.SPELL_CYCLONE),
        cast(5900, MOON, fc.SPELL_CYCLONE, WARRIOR_C),
        note(6000, ROGUE_C, fc.CC_AURA, 0),
        note(6000, WARRIOR_C, fc.CC_AURA, fc.SPELL_CYCLONE),
        note(6500, WARRIOR_C, fc.CC_AURA, 0),
        {"t": 7000, "e": "end", "out": "wipe"},
    ]
    return records + [focus_snap(when) for when in range(0, 7001, 500)]


class FocusAndRepick(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_1_velanaa_1789500000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in focus_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def render(self, section) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            section(self.trace)
        return out.getvalue()

    def test_peaks_count_the_most_champions_on_one_member(self):
        self.assertEqual(fc.focus_peaks(self.trace), collections.Counter({4: 10, 3: 5}))

    def test_episodes_carry_their_cover_and_death(self):
        self.assertEqual(fc.focus_episodes(self.trace), [
            {"g": MAGE, "start": 0, "stop": 1500, "peak": 4, "hp": 100.0, "low": 50.0, "died": None,
             "cover": [(1200, "Ice Block", MAGE)]},
            {"g": PRIEST, "start": 2000, "stop": 4500, "peak": 4, "hp": 100.0, "low": 50.0, "died": 4800,
             "cover": [(-500, "Pain Suppression", PRIEST)]}])

    def test_a_repick_scores_the_last_prediction(self):
        self.assertEqual(fc.repick_checks(self.trace), [
            {"t": 1900, "predicted": PRIEST, "actual": PRIEST, "count": 4},
            {"t": 5200, "predicted": 0, "actual": TANK, "count": 3}])

    def test_the_section_renders(self):
        text = self.render(fc.show_focus)
        self.assertIn("share of 15 samples: 3: 33%, 4: 67%", text)
        self.assertRegex(text, r"0:02\.000 +2\.5s Mercy +heal +4 +100>50 +0:04\.800  Pain Suppression -0\.5s")
        self.assertIn("named the member with the most champions 1.5 s later after 1 of them", text)

    def test_a_cyclone_that_ran_out_is_not_broken(self):
        self.assertEqual(fc.own_breaks(self.trace), {MOON: collections.Counter({"holds": 2, "broken": 0})})

    def test_a_healthstone_over_70_is_flagged(self):
        self.assertIn("healthstones over 70%: 1 of 1, Bulwark 90%", self.render(fc.show_personal))


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
        self.assertEqual(fc.cc_duties(trace), {})
        self.assertEqual(fc.heal_rows(trace), [])
        lust, first, vetoes = fc.burst_rows(trace)
        self.assertEqual((lust, first, vetoes), (None, {}, collections.Counter()))
        fears, wards = fc.fear_rows(trace)
        self.assertFalse(any(row["count"] for row in fears.values()))
        self.assertFalse(any(wards.values()))
        self.assertEqual(fc.cc_auras(trace), [])
        self.assertEqual(fc.mass_dispels_under_cc(trace), (0, 0))
        self.assertEqual(fc.backlashes(trace), [])
        self.assertEqual(fc.purge_rows(trace), [])
        self.assertEqual(fc.shield_rows(trace), [])
        self.assertEqual(fc.physical_holds(trace), [])
        self.assertEqual(fc.aoe_casts(trace), [])
        self.assertEqual(fc.fc_vetoes(trace), collections.Counter())
        self.assertEqual(fc.cc_aura_held(trace), {})
        self.assertEqual(fc.fear_uptime(trace), {})
        self.assertEqual(fc.lust_carriers(trace), [])
        self.assertEqual(fc.summon_casts(trace), [])
        self.assertEqual(fc.add_units(trace), [])
        self.assertEqual(fc.totem_holds(trace), [])
        self.assertEqual(fc.first_kill(trace), None)
        self.assertEqual(fc.longest_stall(trace), None)
        self.assertEqual(fc.grips(trace), [])
        self.assertEqual(fc.grip_holds(trace), [])
        self.assertEqual(fc.chain_links(trace), ([], []))
        self.assertEqual(fc.snare_casts(trace), [])
        self.assertEqual(fc.peel_casts(trace), [])
        self.assertEqual(fc.warrior_disarms(trace), [])
        self.assertEqual(fc.external_casts(trace), [])
        self.assertEqual(fc.battle_res_casts(trace), [])
        self.assertEqual(fc.taunt_casts(trace), collections.Counter())
        self.assertEqual(fc.ball_windows(trace), [])
        self.assertEqual(fc.kill_isolation(trace), [])
        self.assertEqual(fc.escapee_seconds(trace), {})
        self.assertEqual(fc.champion_trinkets(trace), [])
        self.assertEqual(fc.trinket_triggers(trace), [])
        self.assertEqual(fc.opener_holds(trace), [])
        self.assertEqual(fc.hold_rows(trace), [])
        self.assertEqual(fc.first_engage(trace), None)
        self.assertEqual(fc.first_picks(trace), [])
        self.assertEqual(fc.channel_rows(trace), [])
        self.assertEqual(fc.pack_buff_rows(trace), [])
        self.assertEqual(fc.own_splash(trace), collections.Counter())
        self.assertEqual(fc.stand_rows(trace), [])
        self.assertEqual(fc.nitro_rows(trace), [])
        self.assertEqual(fc.healer_activity(trace), [])
        self.assertEqual(fc.mover_rows(trace), [])
        self.assertEqual(fc.heal_output(trace), {})
        self.assertEqual(fc.targeted_share(trace), {})
        self.assertEqual(fc.raid_wide_casts(trace), [])
        self.assertEqual(fc.own_breaks(trace), {})
        self.assertEqual(fc.cyclone_rows(trace), [])
        self.assertEqual(fc.champion_damage(trace), [])
        self.assertEqual(fc.melee_control(trace), [])
        self.assertEqual(fc.personal_rows(trace), [])
        self.assertEqual(fc.personal_deaths(trace), [])
        self.assertEqual(fc.focus_peaks(trace), collections.Counter())
        self.assertEqual(fc.focus_episodes(trace), [])
        self.assertEqual(fc.repick_checks(trace), [])


if __name__ == "__main__":
    unittest.main()

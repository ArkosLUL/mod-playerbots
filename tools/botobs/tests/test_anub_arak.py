"""Tests for the Anub'arak reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 10H pull pins the arithmetic behind each section, a 25H one the side tank, grip, kite
stage and allowance probes, a second 25H one the gather and the burrower window, a third the
Permafrost time, the focus picks and the tank patches spent, a fourth the tank spots, a dropped grip,
a burrower's submerge, the speeds at a cover, the healer and tank views, a fifth the kite flip-flop, the
ready paladin, a dropped Feign Death and the pursuit replay, a sixth the scarabs, the redirects and the
burrower openers, a seventh the DPS split, the side groups, the focus per side, the kite plans and the
stretch held off the anchor, an eighth each patch kite's turn, the bridge's Hand, the spike's first pass
by a patch, Impale on a side tank, a burrower pulled off its side tank and the main tank's Expose
Weakness, a ninth a Hand against its predicted catch, a turn with no Hand, the first tank patch per
submerge, a relayed far grip, Bladestorm near a burrower, the burrower threat cap, the drag's pre-steps
and the submerge spot search, a tenth the escort, claim, Hand, second patch and per submerge
kite reads and the Hand's cooldown off anub.hands, an eleventh three burrowers on one side tank and the
two sides' burrowers within Spider Frenzy's reach, a twelfth the bystanders' gather and dodge swaps and
dodge walks, a thirteenth the escort at the boost, at the end and in Permafrost, an own kite dead to a scarab,
kite tools spent off the mark and the main tank out of the healers' reach, a fourteenth a Demonic Circle
teleport, a kite sphere spent early and a Feign Death back before the submerge, a fifteenth a boost after the
Hand, a patch kite turning on its boost, the escort waiting by the patch and both kite patches down as he went
under, a sixteenth the side tanks' area casts and anub.saoe, the seconds a Penetrating Cold carrier went with no
HoT or shield, the healers' walk behind the tank patches and burrowers reaching a healer past no patch, a
seventeenth the phase 3 Penetrating Cold waves with the healer duties, Aura Mastery and a herd, the healing table
per phase and a pulled burrower's way back to a tank, with and without the new keys, an eighteenth the kite
replay's starts, each kite's anub.kitepred and anub.kitepath and its first hazard, a nineteenth each wave's focus
pick per side, the side tanks' Expose Weakness and anub.p3wave, a twentieth each speed boost against the spike's
speed and its pick's +boost@ second, the standby escort at the claim, the spike's next leg past a patch, the
healers' phase 3 mana and mana cooldowns and the melee on a burrower no tank holds, a twenty-first the kiter
closing on a spike no faster than itself and a tank's form or stance lost while he is up, a twenty-second the push
per phase 1, a ranged bot's capped rows, the kite patches and Hands per submerge, the warlock and mage top ups, a
scarab victim's stacks and kite seconds and phase 3's heals by kind, Light's uptime and water totems, the first
one again phase 3's race against the swarm and the bot tanks at the engage, a twenty-third the move swings, the
opening walk, the submerge drag after the push and phase 3's melee ring reach, a twenty-fourth a grip's landing
against the other side's patch, the spike's walk after a patch, the turn's Hands and circle, a planned teleport, a
ground area emptying and the paladins' resistance auras, and the shared fixture shows every
section reads empty on a trace that is not Anub'arak rather than raising. The kite planner's port is checked against the native
test's cases that need no Hand.
"""
from __future__ import annotations

import contextlib
import io
import json
import math
import pathlib
import statistics
import sys
import tempfile
import unittest
import unittest.mock

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from bosses import anub_arak as ab  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v12.ndjson"

TANK = 5001
HEAL = 5002
KITE = 5003   # ranged, marked twice
MELEE = 5004  # human, marked once, impaled
LATE = 5005   # west of the Web Door at the pull
FLOAT = 5006  # still up in the coliseum at the pull, lands at 5 s, dies to the spike
PALADIN = 5007

CREATURE = 1 << 32
SPIKE = CREATURE | 800  # lower guid than the boss, same name, also boss-flagged
BOSS = CREATURE | 900
SPHERES = [CREATURE | n for n in range(911, 917)]
S1, S2, S3 = SPHERES[:3]
B1 = CREATURE | 921
B2 = CREATURE | 922

X, Y, Z = ab.ROOM_CENTER
PATCH_A = (X + 10.0, Y + 12.0)
PATCH_B = (X + 10.0, Y - 12.0)
KITE_PATCH = (X - 20.0, Y + 14.0)
FLYING_Z = 155.67
PATCH_Z = 142.7
FALLING_Z = 143.0
PERMAFROST_10H = 67856
IMPALE_10H = 67859
COLD_10H = 68509
SWARM_10H = 68646


def phase(when: int) -> int:
    if when < 0:
        return 0
    if when < 20000:
        return 1
    if when < 40000:
        return 2
    return 1 if when < 50000 else 3


def boss_target(when: int) -> int:
    if when < 0 or phase(when) == 2:
        return 0
    if when < 2000:
        return KITE
    if 40000 <= when < 43000:
        return HEAL
    return TANK


def spheres_at(when: int) -> list[tuple[int, tuple[float, float], float]]:
    """`(guid, xy, z)` per sphere alive at `when`. S1 and S2 land at 8 s; S3 falls at 22 s, lands at 23 s,
    and the spike takes it, last sampled at 31 s."""
    if when < 0:
        return []
    out = [(S1, PATCH_A, PATCH_Z if when >= 8000 else FLYING_Z),
           (S2, PATCH_B, PATCH_Z if when >= 8000 else FLYING_Z)]
    if when <= 31000:
        out.append((S3, KITE_PATCH, PATCH_Z if when >= 23000 else FALLING_Z if when >= 22000 else FLYING_Z))
    out += [(guid, (X + 5.0 * index, Y + 30.0), FLYING_Z) for index, guid in enumerate(SPHERES[3:])]
    return out


# phase 3 x offset and moving flag: 6 yd east, a 1 yd shuffle, 4 yd west, then 4 yd east with a
# 1 yd step back before he stops
P3_WALK = {50000: (0.0, 1), 51000: (3.0, 1), 52000: (6.0, 0), 53000: (6.0, 1), 54000: (7.0, 0),
           55000: (3.0, 1), 56000: (-1.0, 0), 57000: (-1.0, 1), 58000: (4.0, 1), 59000: (3.0, 0)}


def boss_spot(when: int) -> tuple[float, float]:
    """He walks 10 yd north at 12 s, while the tank is on a submerge leg, and stays there to the
    submerge."""
    if 12000 <= when < 20000:
        return X + 10.0, Y + 15.0
    return X + 10.0 + P3_WALK.get(when, (0.0, 0))[0], Y + 5.0


def boss_moving(when: int) -> int:
    return 1 if when == 11000 else P3_WALK.get(when, (0.0, 0))[1]


def burrowers_at(when: int) -> list[list]:
    rows = []
    if 2000 <= when <= 19000:
        spot = (PATCH_A[0] + 2.0, PATCH_A[1]) if when >= 8000 else (X + 10.0, Y + 20.0)
        rows.append([B1, spot[0], spot[1], Z, 0.0, 100.0 - (when - 2000) / 200.0, 0.0, TANK, 0, 0, 0, 0])
    # B2: gone 6-11 s and back full, then a jump from 60 to 100 at 17 s.
    b2 = {2000: 100.0, 3000: 90.0, 4000: 80.0, 5000: 70.0, 12000: 100.0, 13000: 90.0, 14000: 80.0,
          15000: 70.0, 16000: 60.0, 17000: 100.0, 18000: 95.0, 19000: 90.0}
    if when in b2:
        rows.append([B2, X - 20.0, Y, Z, 0.0, b2[when], 0.0, KITE, 0, 0, 0, 0])
    return rows


def player(guid: int, when: int) -> list:
    spots = {TANK: (X + 10.0, Y + 3.0, Z), HEAL: (X - 10.0, Y, Z), KITE: (X - 20.0, Y + 10.0, Z),
             MELEE: (X + 10.0, Y + 6.0, Z), LATE: (640.0, 144.7, 144.0), PALADIN: (X + 5.0, Y - 5.0, Z)}
    if guid == FLOAT:
        spot = (745.0, 135.0, 395.0) if when < 5000 else (X, Y - 10.0, Z)
    else:
        spot = spots[guid]
    hp = 100.0
    if guid == FLOAT and when >= 39000:
        hp = 0.0
    if guid == MELEE and when == 54000:
        hp = 40.0
    return [guid, spot[0], spot[1], spot[2], 0.0, hp, 100.0, 0, 0, 0, 0, 0]


def snap(when: int) -> dict:
    units = [player(guid, when) for guid in (TANK, HEAL, KITE, MELEE, LATE, FLOAT, PALADIN)]
    units.append([BOSS, *boss_spot(when), Z, 0.0, 100.0 - max(when, 0) / 1000.0, 0.0,
                  boss_target(when), boss_moving(when), 0, 0, 0])
    if phase(when) == 2:
        units.append([SPIKE, X - 30.0, Y + 14.0, Z, 0.0, 100.0, 0.0, KITE, 0, 0, 0, 0])
    hz = []
    for guid, (x, y), z in spheres_at(when):
        units.append([guid, x, y, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_10H, x, y, PATCH_Z, 6.0, 1])
    units += burrowers_at(when)
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def note(when: int, guid: int, key: str, txt) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": str(txt)}


def aura(when: int, guid: int, spell: int, removed: bool, caster: int = BOSS) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": caster, "sp": spell, "r": 1 if removed else 0}


def dmg(when: int, source: int, victim: int, spell: int, amount: int) -> dict:
    return {"t": when, "e": "dmg", "s": source, "d": victim, "sp": spell, "a": amount}


def cast(when: int, source: int, spell: int, target: int) -> dict:
    return {"t": when, "e": "cast", "s": source, "sp": spell, "tgt": target, "ct": 0}


def grip(when: int, source: int, target: int, pulls: bool = True) -> list[dict]:
    # A Death Grip that hits casts its triggered pull the same ms; one that misses casts only itself
    rows = [cast(when, source, ab.SPELL_DEATH_GRIP, target)]
    if pulls:
        rows.append({**cast(when, source, ab.SPELL_DEATH_GRIP_PULL, target), "tr": 1})
    return rows


def veto(when: int, guid: int, multiplier: str, action: str) -> dict:
    return {"t": when, "e": "veto", "g": guid, "m": multiplier, "a": action}


def drop(when: int, guid: int) -> dict:
    return {"t": when, "e": "act", "g": guid, "a": "drop target", "rel": 99.0, "vd": "OK"}


def anub_pull(boss: str = "anub-arak", map_id: int = ab.MAP_TOC) -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": map_id, "inst": 4, "diff": 2, "boss": boss,
         "roster": [
             {"g": TANK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
             {"g": HEAL, "n": "Mercy", "r": "heal", "c": "priest", "h": 0},
             {"g": KITE, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0},
             {"g": MELEE, "n": "Arkos", "r": "melee", "c": "druid", "h": 1},
             {"g": LATE, "n": "Straggler", "r": "ranged", "c": "mage", "h": 0},
             {"g": FLOAT, "n": "Drifter", "r": "melee", "c": "warrior", "h": 0},
             {"g": PALADIN, "n": "Justice", "r": "heal", "c": "paladin", "h": 0}]},
        {"t": 0, "e": "pull", "boss": boss, "src": "engage"},
        {"t": -2000, "e": "unit", "g": SPIKE, "en": 34660, "n": "Anub'arak", "b": 1},
        {"t": -2000, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 2000, "e": "unit", "g": B1, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        {"t": 2000, "e": "unit", "g": B2, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"}
          for guid in SPHERES),

        note(-30000, TANK, "toc.progress", 8),
        note(-20000, TANK, "toc.progress", 9),
        note(0, TANK, "anub.phase", 1),
        note(20000, TANK, "anub.phase", 2),
        note(40000, TANK, "anub.phase", 1),
        note(50000, TANK, "anub.phase", 3),
        note(10000, TANK, "anub.patch0", S1),
        note(10000, TANK, "anub.patch1", S2),
        # side 1 loses its latch through the later marks, so S2 counts as a patch nobody latched
        note(30500, TANK, "anub.patch1", 0),
        note(39000, TANK, "anub.patch1", S2),

        # Kite shoots both tank patches down.
        note(7000, KITE, "anub.sphere", 0),
        note(7500, KITE, "anub.sphere", 1),
        note(8100, KITE, "anub.sphere", "none"),
        cast(7000, KITE, 75, S1),
        cast(7600, KITE, 75, S2),
        # Late stocks S3 just before the first mark, then Kite shoots one while Melee kites
        note(21000, LATE, "anub.sphere", "stock"),
        drop(21200, LATE),
        cast(21500, LATE, 133, S3),
        note(26000, LATE, "anub.sphere", "none"),
        note(34000, KITE, "anub.sphere", "kite"),
        drop(34200, KITE),
        cast(34500, KITE, 75, SPHERES[3]),
        note(35500, KITE, "anub.sphere", "none"),
        drop(50000, KITE),

        # B2 lands one Shadow Strike on Kite and has the second kicked.
        {"t": 3000, "e": "cast", "s": B2, "sp": ab.SPELL_SHADOW_STRIKE, "tgt": KITE, "ct": 8000},
        dmg(11000, B2, KITE, ab.SPELL_SHADOW_STRIKE, 40000),
        {"t": 13000, "e": "cast", "s": B2, "sp": ab.SPELL_SHADOW_STRIKE, "tgt": KITE, "ct": 8000},
        note(12500, MELEE, "anub.interrupter", 1),
        note(12500, TANK, "anub.interrupter", 0),
        note(14000, MELEE, "anub.interrupter", 0),

        # Four marks: a patch, an Impale, a death and one the emerge ends.
        aura(22000, KITE, ab.SPELL_MARK, False, SPIKE),
        note(22100, KITE, "anub.kite", "patch"),
        note(27000, KITE, "anub.kite", "hold"),
        aura(30000, KITE, ab.SPELL_MARK, True, SPIKE),
        note(30000, KITE, "anub.kite", "none"),
        aura(33000, MELEE, ab.SPELL_MARK, False, SPIKE),
        dmg(35000, SPIKE, MELEE, IMPALE_10H, 18000),
        aura(36000, MELEE, ab.SPELL_MARK, True, SPIKE),
        aura(36500, FLOAT, ab.SPELL_MARK, False, SPIKE),
        aura(38500, FLOAT, ab.SPELL_MARK, True, SPIKE),
        {"t": 38500, "e": "death", "g": FLOAT, "killer": SPIKE, "x": X, "y": Y - 10.0, "z": Z,
         "blow": [SPIKE, 20000]},
        aura(38800, KITE, ab.SPELL_MARK, False, SPIKE),
        note(39000, KITE, "anub.kite", "ring"),
        aura(39600, KITE, ab.SPELL_MARK, True, SPIKE),
        # The tank stood in the lane while Kite ran.
        dmg(29000, SPIKE, TANK, IMPALE_10H, 15000),
        note(28000, MELEE, "anub.dodge", "spike"),
        note(28500, TANK, "anub.dodge", "lane"),
        note(29500, MELEE, "anub.dodge", "none"),

        # Paladin covers Melee too late, Kite blocks its own last kite, and a Hand of Protection on
        # somebody unmarked counts for nobody
        note(34000, PALADIN, "anub.protect", "hop"),
        cast(35500, PALADIN, 10278, MELEE),
        note(36000, PALADIN, "anub.protect", "none"),
        cast(37000, PALADIN, 10278, HEAL),
        note(39000, KITE, "anub.protect", "self"),
        cast(39100, KITE, 45438, KITE),
        note(39600, KITE, "anub.protect", "none"),

        note(0, TANK, "anub.pickup", "attack"),
        note(11500, TANK, "anub.pickup", "submerge"),
        note(40500, TANK, "anub.pickup", "taunt"),
        cast(41000, PALADIN, ab.SPELL_RIGHTEOUS_DEFENSE, HEAL),
        cast(42500, TANK, 355, BOSS),
        note(43000, TANK, "anub.pickup", "hold"),
        # he is back on the tank, so this one never reaches him
        cast(44000, PALADIN, ab.SPELL_RIGHTEOUS_DEFENSE, HEAL),
        cast(45000, MELEE, 6795, BOSS),

        veto(30000, KITE, ab.THREAT_CAP, "auto shot"),
        veto(45000, KITE, ab.THREAT_CAP, "auto shot"),
        veto(45500, KITE, "anubarak sphere guard", "polymorph"),
        veto(46000, KITE, ab.THREAT_CAP, "multi-shot"),
        veto(46500, LATE, ab.THREAT_CAP, "frostbolt"),

        aura(50200, TANK, SWARM_10H, False),
        aura(50200, MELEE, SWARM_10H, False),
        note(50500, TANK, "anub.defensive", "shield wall"),
        note(51000, TANK, "anub.defensive", "covered"),
        dmg(51000, BOSS, TANK, ab.SPELL_LEECHING_SWARM_DMG, 1000),
        dmg(51000, BOSS, MELEE, ab.SPELL_LEECHING_SWARM_DMG, 2000),
        dmg(52000, BOSS, TANK, ab.SPELL_LEECHING_SWARM_DMG, 1000),
        dmg(53000, BOSS, TANK, ab.SPELL_LEECHING_SWARM_DMG, 1000),
        {"t": 51500, "e": "cast", "s": KITE, "sp": 32182, "tgt": 0, "ct": 0},

        aura(52000, MELEE, COLD_10H, False),
        {"t": 53000, "e": "heal", "s": HEAL, "d": MELEE, "sp": 48071, "a": 5000, "oh": 0},
        {"t": 54000, "e": "heal", "s": HEAL, "d": TANK, "sp": 48071, "a": 3000, "oh": 0},
        {"t": 55000, "e": "heal", "s": HEAL, "d": MELEE, "sp": 48071, "a": 4000, "oh": 1000},
        aura(58000, MELEE, COLD_10H, True),
        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    records += [snap(when) for when in range(-2000, 60000, 1000)]
    return records


def write_trace(folder: str, name: str, records: list[dict]) -> Trace:
    path = pathlib.Path(folder) / name
    path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
    return Trace(path)


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_4_anub-arak_1789500000.ndjson", anub_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_the_boss_is_his_entry_not_the_spike_that_shares_his_name(self):
        self.assertEqual(ab.boss_guid(self.trace), BOSS)

    def test_phases_letter_the_repeats_and_read_his_health_at_each_edge(self):
        rows = ab.phase_rows(self.trace)
        self.assertEqual([row["label"] for row in rows], ["P1", "P2", "P1b", "P3"])
        self.assertEqual([row["hp"] for row in rows], [100.0, 80.0, 60.0, 50.0])
        self.assertEqual(sum(1 for row in rows if row["phase"] == 2), 1)

    def test_a_death_lands_in_the_phase_it_happened_in(self):
        dead = {row["label"]: [rec["g"] for rec in row["deaths"]] for row in ab.phase_rows(self.trace)}
        self.assertEqual(dead["P2"], [FLOAT])
        self.assertEqual(dead["P1b"], [])

    def test_stage_nine_after_an_earlier_stage_is_an_edge(self):
        self.assertEqual(ab.stage_nine(self.trace), (-20000, True))

    def test_pull_flags_the_locked_out_and_the_late_lander(self):
        rows = {row["guid"]: row for row in ab.pull_rows(self.trace)}
        self.assertEqual(set(rows), {LATE, FLOAT})
        self.assertEqual(rows[LATE]["why"], "locked out")
        self.assertIsNone(rows[LATE]["arrived"])
        self.assertEqual(rows[FLOAT]["why"], "not landed")
        self.assertEqual(rows[FLOAT]["arrived"], 5000)

    def test_each_mark_window_gets_its_outcome(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual([(w["guid"], w["outcome"]) for w in windows],
                         [(KITE, "patch"), (MELEE, "impale"), (FLOAT, "death"), (KITE, "emerge")])
        self.assertTrue(windows[0]["consumed"])
        self.assertEqual((windows[1]["impales"], windows[1]["damage"]), (1, 18000))

    def test_a_mark_starting_inside_a_patchs_slow_is_flagged(self):
        # Arkos stands 6 yd off S1 at its mark; S3 under Trueshot's first mark is still falling, no centre yet
        windows = ab.mark_windows(self.trace)
        self.assertEqual([window["in_frost"] for window in windows], [False, True, False, False])
        self.assertAlmostEqual(windows[1]["frost_yd"], 6.0)

    def test_a_window_never_opens_on_the_branch_an_earlier_one_closed_with(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual(windows[0]["branches"], ["patch", "hold"])
        self.assertEqual(windows[3]["branches"], ["ring"])

    def test_a_window_opens_on_a_branch_written_before_its_aura(self):
        self.assertEqual(ab.branches_in([(900, "patch"), (1500, "hold")], 1000, 2000), ["patch", "hold"])

    def test_each_window_reads_protection_on_the_kiter(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual([window["near"] for window in windows], [0, None, None, 200])
        self.assertEqual([[(rec["t"], rec["s"]) for rec in window["hop"]] for window in windows],
                         [[], [(35500, PALADIN)], [], []])
        self.assertEqual([[rec["sp"] for rec in window["own"]] for window in windows], [[], [], [], [45438]])
        self.assertEqual([[(rec["g"], rec["txt"]) for rec in window["protect"]] for window in windows],
                         [[], [(PALADIN, "hop"), (PALADIN, "none")], [], [(KITE, "self"), (KITE, "none")]])

    def test_impale_on_anyone_but_the_kiter_is_stray(self):
        stray = ab.stray_impales(self.trace, ab.mark_windows(self.trace))
        self.assertEqual([(rec["t"], rec["d"]) for rec in stray], [(29000, TANK)])

    def test_spheres_split_flying_from_patches_by_height(self):
        self.assertEqual(ab.sphere_timeline(ab.Samples(self.trace)),
                         [(-2000, 0, 0, 0), (0, 6, 0, 0), (8000, 4, 2, 2), (22000, 3, 3, 2),
                          (23000, 3, 3, 3), (32000, 3, 2, 2)])

    def test_a_low_sphere_with_no_permafrost_on_it_is_falling(self):
        samples = ab.Samples(self.trace)
        kinds = samples.sphere_kinds(22000)
        self.assertEqual((kinds[S1], kinds[S3], kinds[SPHERES[3]]), ("patch", "falling", "flying"))
        self.assertEqual(samples.sphere_kinds(23000)[S3], "patch")

    def test_only_a_patch_gone_just_after_a_mark_is_consumed(self):
        samples = ab.Samples(self.trace)
        self.assertEqual(ab.patch_lives(self.trace, samples),
                         {S1: (8000, 59000), S2: (8000, 59000), S3: (22000, 31000)})
        self.assertEqual(ab.consumed_patches(self.trace, samples), {S3: (31000, 30000)})

    def test_heroic_spheres_left_at_each_submerge_and_phase_3(self):
        left = ab.spheres_left(self.trace, ab.Samples(self.trace))
        self.assertEqual(left, [("P2", 20000, 4), ("P3", 50000, 3), ("end", 59000, 3)])

    def test_sphere_shots_count_casts_aimed_at_one(self):
        self.assertEqual(ab.sphere_shots(self.trace), {KITE: 3, LATE: 1})

    def test_patch_supply_at_each_mark(self):
        rows = ab.supply_rows(self.trace)
        self.assertEqual([(row["tank"], row["other"], row["flying"], row["falling"]) for row in rows],
                         [(2, 0, 3, 1), (1, 1, 3, 0), (1, 1, 3, 0), (1, 1, 3, 0)])
        self.assertEqual([row["holders"] for row in rows], [[(LATE, "stock")], [(KITE, "kite")], [], []])

    def test_each_duty_span_counts_its_holders_shots_and_drops(self):
        self.assertEqual([(row["guid"], row["label"], row["start"], row["stop"], row["casts"], row["drops"])
                          for row in ab.duty_spans(self.trace)],
                         [(KITE, "0", 7000, 7500, 1, 0), (KITE, "1", 7500, 8100, 1, 0),
                          (LATE, "stock", 21000, 26000, 1, 1), (KITE, "kite", 34000, 35500, 1, 1)])

    def test_burrower_submerges_read_from_a_gap_and_a_jump(self):
        rows = {row["guid"]: row for row in ab.burrower_rows(self.trace)}
        self.assertEqual(rows[B1]["submerges"], 0)
        self.assertEqual(rows[B2]["submerges"], 2)
        self.assertEqual((rows[B2]["low"], rows[B2]["end"]), (60.0, 90.0))
        self.assertEqual((rows[B1]["first"], rows[B1]["last"]), (2000, 19000))

    def test_burrower_time_on_permafrost(self):
        rows = {row["guid"]: row for row in ab.burrower_rows(self.trace)}
        self.assertAlmostEqual(rows[B1]["frost"], 12 / 18)
        self.assertEqual(rows[B2]["frost"], 0.0)

    def test_a_burrower_is_on_frost_only_within_its_own_reach_not_a_players(self):
        samples = ab.Samples(self.trace)
        self.assertTrue(ab.on_permafrost(samples, [B1, PATCH_A[0] + 6.0, PATCH_A[1], Z], 10000))
        self.assertFalse(ab.on_permafrost(samples, [B1, PATCH_A[0] + 7.0, PATCH_A[1], Z], 10000))

    def test_shadow_strike_lands_only_when_its_damage_follows(self):
        rows = {row["guid"]: row for row in ab.burrower_rows(self.trace)}
        self.assertEqual((rows[B2]["strikes"], rows[B2]["landed"]), (2, 1))

    def test_threat_times_each_surfaced_window_to_a_tank(self):
        rows = ab.threat_rows(self.trace)
        self.assertEqual([(row["label"], row["kind"], row["to_tank"]) for row in rows],
                         [("P1", "pull", 2000), ("P1b", "emerge", 3000), ("P3", "swarm", 0)])
        emerge = rows[1]
        self.assertEqual(dict(emerge["held"]), {"heal": 3000, "tank": 7000})

    def test_righteous_defense_counts_only_on_the_friend_he_is_hitting(self):
        emerge = ab.threat_rows(self.trace)[1]
        self.assertEqual([(rec["t"], rec["s"], rec["sp"]) for rec in emerge["taunts"]],
                         [(41000, PALADIN, ab.SPELL_RIGHTEOUS_DEFENSE), (42500, TANK, 355), (45000, MELEE, 6795)])

    def test_the_drag_anchor_is_the_patch_midpoint_once_both_latch(self):
        rows = ab.threat_rows(self.trace)
        self.assertAlmostEqual(rows[1]["off_anchor"], 5.0)

    def test_the_drag_anchor_is_none_with_a_side_unlatched(self):
        samples = ab.Samples(self.trace)
        latches = [ab.latch_spans(self.trace, key, 60000) for key in ("anub.patch0", "anub.patch1")]
        self.assertIsNone(ab.drag_anchor(latches, samples, 9000))
        self.assertIsNone(ab.drag_anchor(latches, samples, 33000))
        point = ab.drag_anchor(latches, samples, 10000)
        self.assertAlmostEqual(point[0], X + 10.0)
        self.assertAlmostEqual(point[1], Y)

    def test_off_anchor_leaves_out_samples_with_no_anchor_or_on_a_submerge_leg(self):
        # 10 and 11 s only: no anchor before 10 s, a submerge leg from 11.5 s
        self.assertAlmostEqual(ab.threat_rows(self.trace)[0]["off_anchor"], 5.0)

    def test_his_legs_and_reversals_per_window(self):
        rows = ab.threat_rows(self.trace)
        self.assertEqual([(row["label"], row["legs"], round(row["yards"], 1), row["reversals"]) for row in rows],
                         [("P1", 1, 10.0, 0), ("P1b", 0, 0, 0), ("P3", 3, 14.0, 2)])

    def test_a_leg_runs_while_he_is_flagged_moving_and_drops_under_2_yd(self):
        legs = ab.boss_legs(ab.Samples(self.trace), BOSS, 50000, 60000)
        self.assertEqual([(leg["start"], leg["stop"], round(leg["yards"], 1)) for leg in legs],
                         [(50000, 52000, 6.0), (55000, 56000, 4.0), (57000, 59000, 4.0)])

    def test_a_leg_the_window_cuts_ends_on_its_last_moving_sample(self):
        legs = ab.boss_legs(ab.Samples(self.trace), BOSS, 50000, 51500)
        self.assertEqual([(leg["start"], leg["stop"], round(leg["yards"], 1)) for leg in legs],
                         [(50000, 51000, 3.0)])

    def test_pickup_rows_and_threat_cap_vetoes_per_window(self):
        rows = ab.threat_rows(self.trace)
        self.assertEqual(dict(rows[0]["pickup"]), {(TANK, "attack"): 1, (TANK, "submerge"): 1})
        self.assertEqual(dict(rows[1]["pickup"]), {(TANK, "taunt"): 1, (TANK, "hold"): 1})
        self.assertEqual([dict(row["capped"]) for row in rows], [{}, {KITE: 2, LATE: 1}, {}])

    def test_no_side_spots_means_no_stretch_held_off(self):
        self.assertEqual([row["held_off"] for row in ab.threat_rows(self.trace)], [None, None, None])

    def test_the_submerge_spot_is_his_last_surfaced_sample_against_each_tank_patch(self):
        gaps = ab.threat_rows(self.trace)[0]["submerge"]
        self.assertEqual(gaps["spot"], (X + 10.0, Y + 15.0))
        self.assertEqual({side: round(yards, 1) for side, yards in gaps["patches"].items()}, {0: 3.0, 1: 27.0})

    def test_waves_fall_back_to_burrower_spawns_without_anub_wave(self):
        self.assertEqual(ab.wave_starts(self.trace), [(1, 2000)])

    def test_a_key_the_reader_reads_is_absent_whatever_the_source_declares(self):
        gone = ab.missing_probes(self.trace)
        self.assertIn("anub.wave", gone)
        self.assertIn("anub.spot0", gone)
        self.assertIn("anub.hstand", gone)
        self.assertNotIn("anub.phase", gone)

    def test_swarm_reads_the_probe_and_falls_back_to_the_aura(self):
        self.assertEqual(ab.swarm_span(self.trace), (50000, 60000, "anub.phase"))
        self.assertEqual(ab.swarm_span(self.trace, []), (50200, 60000, "Leeching Swarm"))

    def test_swarm_damage_per_bot(self):
        self.assertEqual(ab.swarm_damage(self.trace), {TANK: (3, 3000), MELEE: (1, 2000)})

    def test_lust_is_timed_from_phase_3(self):
        self.assertEqual([row["rel"] for row in ab.lust_casts(self.trace, 50000)], [1500])

    def test_a_cold_window_counts_only_heals_onto_the_carrier(self):
        rows = ab.cold_windows(self.trace)
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual((row["guid"], row["phase"]), (MELEE, "P3"))
        self.assertEqual((row["heals"], row["amount"], row["overheal"]), (2, 9000, 1000))
        self.assertEqual(row["healers"], {HEAL})
        self.assertEqual(row["low"], 40.0)

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(self.trace)
            for _, _, section in ab.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertNotIn("not a Trial of the Crusader", text)
        self.assertIn(f"patch #{S3 & 0xFFFFFFFF} last seen 0:31.000", text)
        self.assertIn("3 taunt(s): bot 2, human 1", text)
        self.assertIn("anubarak threat cap vetoes: Trueshot 2, Straggler 1", text)
        self.assertIn("Hand of Protection by Justice +2.5 s; anub.protect Justice hop +1.0 s", text)
        self.assertIn("Ice Block +0.3 s; anub.protect Trueshot self +0.2 s", text)
        self.assertIn("spans with a cast: side 0 1 of 1, side 1 1 of 1, kite 1 of 1, stock 1 of 1", text)


def focus_pull() -> list[dict]:
    """B1 holds the focus from 1 to 5 s, nothing to 6 s, then B2 to the end at 10 s. Trueshot leaves B1
    for the boss at 4 s, Drifter stays on the boss through B2 and is dead at 9 s, and the human and the
    healer never count. B1 is last sampled at 2 s, the focus due drops at 2 and 4.5 s, and Trueshot puts
    Corruption on B2 twice around B2's Shadow Strike, which Drifter kicks."""
    def target(guid: int, when: int) -> int:
        if guid in (MELEE, FLOAT) and 1000 <= when < 5000:
            return B1
        if guid == KITE:
            return B2 if when >= 6000 else (BOSS if when == 4000 else B1)
        return BOSS if guid == FLOAT else 0

    def focus_snap(when: int) -> dict:
        units = [[guid, X, Y, Z, 0.0, 0.0 if guid == FLOAT and when == 9000 else 100.0, 100.0,
                  target(guid, when), 0, 0, 0, 0] for guid in (KITE, FLOAT, MELEE, HEAL)]
        if when <= 2000:
            units.append([B1, X + 5.0, Y, Z, 0.0, 100.0 - when / 100.0, 0.0, 0, 0, 0, 0, 0])
        units.append([B2, X - 5.0, Y, Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        return {"t": when, "e": "snap", "u": units, "hz": []}

    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 4, "diff": 1, "boss": "anub-arak",
         "roster": [
             {"g": KITE, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0},
             {"g": FLOAT, "n": "Drifter", "r": "melee", "c": "warrior", "h": 0},
             {"g": MELEE, "n": "Arkos", "r": "melee", "c": "rogue", "h": 1},
             {"g": HEAL, "n": "Mercy", "r": "heal", "c": "priest", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": 0, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 0, "e": "unit", "g": B1, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        {"t": 0, "e": "unit", "g": B2, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        # a cleared latch restated at the trace's open is no hold
        note(-500, KITE, "anub.focus", 0),
        note(1000, KITE, "anub.focus", B1),
        note(5000, FLOAT, "anub.focus", 0),
        note(6000, KITE, "anub.focus", B2),
        note(1000, KITE, "anub.dps", "burrower"),
        note(4000, KITE, "anub.dps", "boss"),
        note(4500, KITE, "anub.dps", "burrower"),
        note(1000, FLOAT, "anub.dps", "burrower"),
        *(note(when, KITE, "anub.focusdue", due) for when, due in (
            (0, 0), (1000, 1), (2000, 0), (3000, 1), (4500, 0), (6500, 1))),
        {"e": "spell", "sp": 47813, "n": "Corruption"},
        cast(1500, KITE, 47813, B2),
        cast(1500, MELEE, 1766, B2),
        cast(2500, KITE, 49050, B1),
        {"t": 3000, "e": "cast", "s": B2, "sp": ab.SPELL_SHADOW_STRIKE, "tgt": KITE, "ct": 1000},
        cast(3500, FLOAT, 6552, B2),
        {**cast(3600, KITE, 47813, B2), "tr": 1},
        cast(4500, KITE, 47813, B2),
        {"t": 10000, "e": "end", "out": "wipe"},
    ]
    records += [focus_snap(when) for when in range(0, 10000, 1000)]
    return records


class BurrowerFocus(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_4_anub-arak_1789500001.ndjson", focus_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_each_focus_holds_its_time_and_the_living_dps_bots_share(self):
        # B1: Trueshot 3 of 4, Drifter 4 of 4. B2: Trueshot 4 of 4, Drifter 0 of 3 alive.
        rows = {row["guid"]: row for row in ab.focus_rows(self.trace)}
        self.assertEqual(set(rows), {B1, B2})
        self.assertEqual((rows[B1]["held"], rows[B2]["held"]), (4000, 4000))
        self.assertAlmostEqual(rows[B1]["share"], 7 / 8)
        self.assertAlmostEqual(rows[B2]["share"], 4 / 7)

    def test_the_section_prints_the_focus_and_the_dps_picks(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_burrowers(self.trace)
        text = out.getvalue()
        self.assertRegex(text, rf"#{B1 & 0xFFFFFFFF} +4\.0s +88%")
        self.assertRegex(text, rf"#{B2 & 0xFFFFFFFF} +4\.0s +57%")
        self.assertIn("anub.dps: Drifter burrower 1; Trueshot burrower 2, boss 1", text)
        self.assertIn("DPS casts at a burrower other than the focus: 2, by Trueshot 2; most: Corruption 47813 2",
                      text)
        self.assertIn(f"anub.focusdue dropped with the focus alive: 0:02.000 #{B1 & 0xFFFFFFFF} at 80%", text)

    def test_casts_off_the_focus_leave_out_humans_triggers_and_a_kick_on_a_cast(self):
        self.assertEqual([(rec["t"], rec["s"], rec["tgt"]) for rec in ab.stray_burrower_casts(self.trace)],
                         [(1500, KITE, B2), (4500, KITE, B2)])

    def test_focus_due_drops_only_count_a_focus_still_sampled(self):
        rows = ab.focus_due_drops(self.trace, ab.Samples(self.trace))
        self.assertEqual([(row["t"], row["guid"], row["hp"]) for row in rows], [(2000, B1, 80.0)])

    def test_an_older_trace_reads_its_one_focus_with_no_side(self):
        self.assertEqual([side for side, _ in ab.focus_latches(self.trace)], [None])
        self.assertEqual([(row["side"], row["guid"]) for row in ab.focus_rows(self.trace)], [(None, B1), (None, B2)])
        self.assertEqual([(row["side"], row["guid"]) for row in ab.focus_picks(self.trace)], [(None, B1), (None, B2)])
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_burrowers(self.trace)
        self.assertRegex(out.getvalue(), rf"#{B1 & 0xFFFFFFFF} +4\.0s +88% +-\n")


MAIN, SIDE0, SIDE1, DK, MAGE, LOCK, ROGUE, DRUID, HUNTER, DRAGON = range(6001, 6011)
IMP = (2 << 32) | 7001
G0, G1, K1, K2, IDLE = (CREATURE | n for n in range(911, 916))
BN1, BN2, BS1, BS2 = (CREATURE | n for n in range(921, 925))
WN, WS = CREATURE | 931, CREATURE | 932
SPK = CREATURE | 941
PERMAFROST_25H = 67857

O = ab.TANK_PATCH_OFFSET
GAP = ab.SIDE_SPOT_GAP
EXIT_N1, EXIT_N2, EXIT_S1, EXIT_S2 = (740.2, 193.4), (694.5, 185.4), (732.0, 83.4), (694.9, 102.5)
WAVE1 = {BN1: EXIT_N1, BN2: EXIT_N2, BS1: EXIT_S1, BS2: EXIT_S2}
WAVE2 = {WN: EXIT_N1, WS: EXIT_S1}
PATCH0 = (X + 3.0, Y + O)            # G0, 3 yd off side 0's spot
G1_LAND = (X + 3.0, Y + O - GAP - 2.0)  # 2 yd off side 1's spot once G0 latched
G1_FLY = (X + 20.0, Y - 20.0)
SUBMERGE_AT = (X - 9.0, Y + O)       # 12 yd off G0
SPIKE_ROGUE = (PATCH0[0] + 9.0, PATCH0[1])  # in the fail test's reach of G0
SPIKE_HUNTER = (X - 25.0, Y - 25.0)
SPOTS = {SIDE0: (X + 3.0, Y + O + 4.0), SIDE1: (X, Y - O - 2.0), DK: (X + 3.0, Y + O + 0.5),
         MAGE: (X - 10.0, Y - 3.0), LOCK: (X - 12.0, Y + 3.0), ROGUE: (X + 15.0, Y), DRUID: (X - 14.0, Y - 6.0),
         HUNTER: (X + 15.0, Y + 6.0), IMP: (X - 13.0, Y + 4.0)}


def refine_sphere(guid: int, when: int):
    """`(xy, z)` or None. The DK grips G0 onto itself at 4 s and it dies at 5 s; the grip on G1 at 8 s
    misses, and the imp kills it at 12.5 s; K1 and K2 die while he is under."""
    if when < 0:
        return None
    if guid == G0:
        if when < 4500:
            return (X + 10.0, Y + 20.0), FLYING_Z
        return PATCH0, PATCH_Z if when >= 6500 else Z + 0.5
    if guid == G1:
        if when < 12500:
            return (G1_FLY if when < 11500 else G1_LAND), FLYING_Z
        return G1_LAND, PATCH_Z if when >= 14000 else FALLING_Z
    for sphere, spot, killed in ((K1, (X - 20.0, Y + 20.0), 32500), (K2, (X - 20.0, Y - 20.0), 36500)):
        if guid == sphere:
            if when < killed:
                return spot, FLYING_Z
            return spot, PATCH_Z if when >= killed + 1500 else FALLING_Z
    return (X, Y + 30.0), FLYING_Z


def refine_burrower_target(guid: int, when: int) -> int:
    if guid == BN1:
        return MAGE if when < 7000 else SIDE0
    if guid == BN2:
        return 0 if when < 6000 else (MAIN if when < 9000 else SIDE1)
    if guid == BS1:
        return SIDE1
    if guid == BS2:
        return DRUID
    return 0 if when < 50500 else (SIDE0 if guid == WN else SIDE1)


def refine_snap(when: int) -> dict:
    main = (X + 3.0, Y + O - 3.0) if 20000 <= when < 30000 else (X - 5.0, Y)
    units = [[MAIN, *main, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0]]
    units += [[guid, *spot, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0] for guid, spot in SPOTS.items()]
    boss = SUBMERGE_AT if 28000 <= when < 50000 else (X - 5.0, Y + 2.0)
    target = MAIN if 1000 <= when < 30000 or when >= 51000 else 0
    units.append([BOSS, *boss, Z, 0.0, 100.0 - max(when, 0) / 1000.0, 0.0, target, 0, 0, 0, 0])
    if 31000 <= when < 48000:
        spike = SPIKE_ROGUE if when < 40000 else SPIKE_HUNTER
        units.append([SPK, *spike, Z, 0.0, 100.0, 0.0, ROGUE if when < 40000 else HUNTER, 0, 0, 0, 0])
    hz = []
    for guid in (G0, G1, K1, K2, IDLE):
        place = refine_sphere(guid, when)
        if place is None:
            continue
        (x, y), z = place
        units.append([guid, x, y, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_25H, x, y, PATCH_Z, 6.0, 1])
    for guid, (x, y) in {**WAVE1, **WAVE2}.items():
        if (guid in WAVE1 and 5000 <= when <= 44500) or (guid in WAVE2 and when >= 50000):
            units.append([guid, x, y, Z, 0.0, 100.0, 0.0, refine_burrower_target(guid, when), 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def triggered(when: int, source: int, spell: int, target: int) -> dict:
    return {**cast(when, source, spell, target), "tr": 1}


def refine_pull() -> list[dict]:
    """A 25H pull on the new probes: P1 to 30 s, P2 to 50 s, then up again, burrower waves at 5 and 50 s."""
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 5, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": MAIN, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
             {"g": SIDE0, "n": "Ecoterrorist", "r": "tank", "c": "druid", "h": 0},
             {"g": SIDE1, "n": "Aegis", "r": "tank", "c": "paladin", "h": 0},
             {"g": DK, "n": "Deathsong", "r": "melee", "c": "deathknight", "h": 0},
             {"g": MAGE, "n": "Frostfire", "r": "ranged", "c": "mage", "h": 0},
             {"g": LOCK, "n": "Hellflame", "r": "ranged", "c": "warlock", "h": 0},
             {"g": ROGUE, "n": "Shadowstep", "r": "melee", "c": "rogue", "h": 0},
             {"g": DRUID, "n": "Druidica", "r": "ranged", "c": "druid", "h": 0},
             {"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -1000, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": -1000, "e": "unit", "g": IMP, "en": 416, "n": "Ruirin", "own": LOCK},
        {"t": 30000, "e": "unit", "g": SPK, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"}
          for guid in (G0, G1, K1, K2, IDLE)),
        *({"t": 5000, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"} for guid in WAVE1),
        *({"t": 50000, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"} for guid in WAVE2),
        *({"e": "spell", "sp": spell, "n": name} for spell, name in (
            (42842, "Frostbolt"), (47964, "Firebolt"), (47813, "Corruption"), (53190, "Starfall"))),

        note(0, MAIN, "anub.phase", 1),
        note(30000, MAIN, "anub.phase", 2),
        note(50000, MAIN, "anub.phase", 1),
        # a cleared latch restated at the trace's open
        note(-500, MAIN, "anub.side0", 0),
        note(0, MAIN, "anub.side0", SIDE0),
        note(0, SIDE1, "anub.side1", SIDE1),
        note(0, MAIN, "anub.patch0", 0),
        note(7000, DK, "anub.patch0", G0),
        note(0, MAIN, "anub.patch1", 0),
        note(5000, SIDE0, "anub.wave", 1),
        note(50000, SIDE1, "anub.wave", 2),
        note(0, MAIN, "anub.focusdue", 0),
        note(20000, MAGE, "anub.focusdue", 1),
        note(45000, MAGE, "anub.focusdue", 0),
        note(55000, MAGE, "anub.focusdue", 1),
        note(0, DK, "anub.grip", 1),
        note(11000, DK, "anub.grip", 2),
        note(30000, MAIN, "anub.allow", 1),

        # BN2 moves from side 0 to side 1, BS2 never gets a row so its exit decides
        note(5000, BN1, "anub.bside", 0),
        note(5000, BN2, "anub.bside", 0),
        note(8000, BN2, "anub.bside", 1),
        note(5000, BS1, "anub.bside", 1),
        note(50000, WN, "anub.bside", 0),
        note(50000, WS, "anub.bside", 1),

        note(3000, DK, "anub.sphere", "grip0"),
        note(7500, DK, "anub.sphere", "grip1"),
        note(11000, DK, "anub.sphere", "none"),
        note(11000, LOCK, "anub.sphere", 1),
        note(13000, LOCK, "anub.sphere", "none"),
        note(30500, MAGE, "anub.sphere", "kite"),
        note(33000, MAGE, "anub.sphere", "none"),
        *(note(when, DK, "anub.gripstep", step) for when, step in (
            (3000, "walk"), (3900, "cast"), (4500, "kill"), (6500, "none"), (7500, "walk"), (7900, "cast"),
            (11000, "none"))),
        note(0, SIDE0, "anub.sidetank", "ready"),
        note(7000, SIDE0, "anub.sidetank", "pickup"),
        note(8000, SIDE0, "anub.sidetank", "hold"),
        note(0, SIDE1, "anub.sidetank", "ready"),
        note(5000, SIDE1, "anub.sidetank", "pickup"),
        note(1000, MAIN, "anub.pickup", "attack"),
        note(25000, MAIN, "anub.pickup", "stepout"),
        note(26000, MAIN, "anub.pickup", "hold"),
        note(30500, MAGE, "anub.gather", "walk"),
        note(32000, MAGE, "anub.gather", "hold"),
        note(50000, MAGE, "anub.gather", "none"),

        # G0 takes a hit from the mage, then a later shadow bolt cast: the hit is the killer
        *grip(4000, DK, G0),
        cast(4500, MAGE, 42842, G0),
        dmg(4700, MAGE, G0, 42842, 9000),
        cast(4800, LOCK, 47809, G0),
        # G1: the grip misses, then a proc and a Hunter's Mark after the imp's bolt are no killer
        *grip(8000, DK, G1),
        cast(12000, IMP, 47964, G1),
        triggered(12200, MAGE, 12579, G1),
        cast(12300, HUNTER, 53338, G1),
        cast(31000, LOCK, 47813, K1),
        cast(32000, MAGE, 42842, K1),
        triggered(36000, DRUID, 53190, K2),
        triggered(36100, MAGE, 12579, K2),

        aura(31000, ROGUE, ab.SPELL_MARK, False),
        *(note(when, ROGUE, "anub.kitestage", stage) for when, stage in (
            (31000, "run"), (33000, "boost:11305"), (35000, "immune"), (39000, "patch"), (40000, "none"))),
        cast(33000, ROGUE, 11305, ROGUE),
        cast(35000, SIDE1, 10278, ROGUE),
        aura(40000, ROGUE, ab.SPELL_MARK, True),
        # one Nitro Boosts read three times: the use, its triggered speed and the aura
        aura(41000, HUNTER, ab.SPELL_MARK, False),
        *(note(when, HUNTER, "anub.kitestage", stage) for when, stage in (
            (41000, "run"), (42000, "boost:55004"), (46000, "patch"), (48000, "none"))),
        cast(42000, HUNTER, 55004, HUNTER),
        triggered(42010, HUNTER, 54861, HUNTER),
        aura(42020, HUNTER, 54861, False, HUNTER),
        cast(44000, HUNTER, 5384, HUNTER),
        aura(48000, HUNTER, ab.SPELL_MARK, True),
        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    records += [refine_snap(when) for when in range(-1000, 60000, 500)]
    return records


class RefinedPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_5_anub-arak_1789500002.ndjson", refine_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_each_burrower_reads_its_exit_and_side_at_the_spawn(self):
        rows = {row["guid"]: row for row in ab.pickup_rows(self.trace)}
        self.assertEqual({guid: (row["wave"], row["exit"], row["side"], row["probed"]) for guid, row in rows.items()},
                         {BN1: (1, EXIT_N1, 0, True), BN2: (1, EXIT_N2, 0, True), BS1: (1, EXIT_S1, 1, True),
                          BS2: (1, EXIT_S2, 1, False), WN: (2, EXIT_N1, 0, True), WS: (2, EXIT_S1, 1, True)})

    def test_every_owner_in_turn(self):
        rows = {row["guid"]: row for row in ab.pickup_rows(self.trace)}
        self.assertEqual(rows[BN2]["owners"], [0, 1])
        self.assertEqual(rows[BS2]["owners"], [1])

    def test_pickup_times_any_tank_apart_from_its_owning_side_tank(self):
        rows = {row["guid"]: row for row in ab.pickup_rows(self.trace)}
        # BN2's side 1 owns it from 8 s, when its side tank picks it up at 9 s
        self.assertEqual({guid: (row["to_tank"], row["to_side"], row["tank"]) for guid, row in rows.items()},
                         {BN1: (2000, 2000, SIDE0), BN2: (1000, 4000, SIDE0), BS1: (0, 0, SIDE1),
                          BS2: (None, None, SIDE1), WN: (500, 500, SIDE0), WS: (500, 500, SIDE1)})

    def test_the_side_tank_is_measured_from_its_patch_else_its_spot(self):
        rows = {row["guid"]: row for row in ab.pickup_rows(self.trace)}
        self.assertEqual((rows[BN1]["from"], round(rows[BN1]["gap"], 2)), ("spot", 5.0))
        self.assertEqual((rows[BS1]["from"], round(rows[BS1]["gap"], 2)), ("spot", 2.0))
        self.assertEqual((rows[WN]["from"], round(rows[WN]["gap"], 2)), ("patch", 4.0))
        # side 1 has no patch, so its spot hangs off side 0's
        self.assertEqual(rows[WS]["from"], "spot")
        self.assertAlmostEqual(rows[WS]["gap"], math.dist(SPOTS[SIDE1], (PATCH0[0], PATCH0[1] - GAP)))

    def test_focus_due_per_wave_against_the_next_spawn(self):
        waves = ab.wave_starts(self.trace)
        self.assertEqual(waves, [(1, 5000), (2, 50000)])
        self.assertEqual(ab.focus_due_rows(self.trace, waves),
                         [{"wave": 1, "spawn": 5000, "due": 20000, "next": 50000},
                          {"wave": 2, "spawn": 50000, "due": 55000, "next": None}])

    def test_focus_due_only_after_the_next_wave_counts_as_never(self):
        rows = ab.focus_due_rows(self.trace, [(1, 5000), (2, 10000), (3, 50000)])
        self.assertEqual([row["due"] for row in rows], [None, 20000, 55000])

    def test_each_dead_sphere_names_its_killer(self):
        rows = ab.sphere_deaths(self.trace)
        self.assertEqual([(row["guid"], row["killed"], row["killer"], row["pet"], row["spell"], row["source"],
                           row["shape"]) for row in rows],
                         [(G0, 5000, MAGE, None, 42842, "dmg", ()),
                          (G1, 12500, LOCK, IMP, 47964, "cast", ()),
                          (K1, 32500, MAGE, None, 42842, "cast", ()),
                          (K2, 36500, DRUID, None, 53190, "cast", ("area",))])

    def test_a_sphere_lands_against_its_sides_spot(self):
        rows = {row["guid"]: row for row in ab.sphere_deaths(self.trace)}
        self.assertEqual({guid: (row["side"], row["latched"], row["duty"]) for guid, row in rows.items()},
                         {G0: (0, True, None), G1: (1, False, "1"), K1: (None, False, "kite"),
                          K2: (None, False, None)})
        self.assertAlmostEqual(rows[G0]["off_spot"], 3.0)
        # G0 had latched by then, so side 1's spot is GAP off it
        self.assertAlmostEqual(rows[G1]["off_spot"], 2.0)
        self.assertIsNone(rows[K1]["off_spot"])

    def test_a_grip_lands_only_within_reach_of_the_dk_in_time(self):
        rows = ab.grip_rows(self.trace)
        self.assertEqual([(row["t"], row["sphere"], row["duty"], row["landed"]) for row in rows],
                         [(4000, G0, "grip0", 500), (8000, G1, "grip1", None)])
        self.assertAlmostEqual(rows[1]["closest"], math.hypot(17.0, 20.0 + O + 0.5))

    def test_a_grip_reads_the_side_it_served_and_the_side_that_latched_it(self):
        self.assertEqual([(row["served"], row["latched"]) for row in ab.grip_rows(self.trace)],
                         [(0, 0), (1, None)])

    def test_a_side_without_a_tank_patch_while_burrowers_are_out(self):
        # side 0 latches 2 s after wave 1; side 1 never does, so all of wave 1's P1 and P1b count
        self.assertEqual(ab.unpatched_ms(self.trace, ab.Samples(self.trace)), {0: 2000, 1: 34500})

    def test_covered_counts_overlaps_once(self):
        self.assertEqual(ab.covered([(0, 10), (5, 20), (30, 40)], 2, 35), 23)
        self.assertEqual(ab.covered([(0, 10)], 20, 30), 0)

    def test_spheres_spent_per_submerge_against_the_allowance(self):
        # G0 outlives the trace, so no tank patch goes and the two kills under him are no spend
        self.assertEqual(ab.allowance_rows(self.trace, ab.sphere_deaths(self.trace), ab.spent_patches(self.trace)),
                         [{"label": "P2", "start": 30000, "allow": 1, "dropped": None, "tank": 0, "other": 2,
                           "spent": [], "over": False}])

    def test_kite_stages_and_boosts_per_window(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual([window["stages"] for window in windows],
                         [{"run": 2000, "boost:11305": 2000, "immune": 4000, "patch": 1000},
                          {"run": 1000, "boost:55004": 4000, "patch": 2000}])
        self.assertEqual([[(rec["t"], rec["sp"]) for rec in window["boosts"]] for window in windows],
                         [[(33000, 11305)], [(42000, 55004)]])
        self.assertEqual([[rec["sp"] for rec in window["own"]] for window in windows], [[], [5384]])

    def test_the_spikes_closest_approach_to_each_latched_tank_patch(self):
        # side 1 never latches, so only side 0 reads
        windows = ab.mark_windows(self.trace)
        self.assertEqual([{side: round(yards, 2) for side, yards in window["tank_near"].items()} for window in windows],
                         [{0: 9.0}, {0: round(math.dist(SPIKE_HUNTER, PATCH0), 2)}])

    def test_main_tank_share_in_a_patch_and_the_submerge_gaps(self):
        rows = ab.threat_rows(self.trace)
        self.assertEqual([(row["label"], row["main"], row["in_patch"], row["living"]) for row in rows],
                         [("P1", MAIN, 20, 60), ("P1b", MAIN, 0, 20)])
        self.assertEqual(rows[0]["submerge"]["spot"], SUBMERGE_AT)
        self.assertEqual({side: round(yards, 1) for side, yards in rows[0]["submerge"]["patches"].items()}, {0: 12.0})
        self.assertIsNone(rows[1]["submerge"])

    def test_both_bot_side_tanks_latched(self):
        self.assertEqual(ab.side_tanks(self.trace), (2, {0: [SIDE0], 1: [SIDE1]}))

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(self.trace)
            for _, _, section in ab.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertNotIn("side tanks latched", text)
        self.assertRegex(text, r"#911 .* Frostfire +Frostbolt 42842 +dmg +- +0 latched +3\.0 yd")
        self.assertRegex(text, r"#912 .* Hellflame's pet +Firebolt 47964 +cast +- +1 +2\.0 yd")
        self.assertIn("periodic or area spells aimed at a sphere: Hellflame Corruption 47813 1,"
                      " Druidica Starfall 53190 1", text)
        self.assertIn("P2     0:30.000  allowance  1  tank 0  other 2  spent 0\n", text)
        self.assertIn("anub.grip: gripping from 0:00.000, shooting from 0:11.000", text)
        self.assertRegex(text, r"Deathsong +grip1 +#912 +missed, closest \d+\.\d yd\n")
        self.assertRegex(text, r"Deathsong +grip0 +#911 +landed \+0\.5 s, latched side 0\n")
        self.assertIn("no tank patch with burrowers out while he was up: side 1 34.5 s", text)
        self.assertRegex(text, r"#922 +1 N 694\.5,185\.4 0>1 ")
        self.assertIn("kitestage run 2.0 s, boost Sprint 2.0 s, immune 4.0 s, patch 1.0 s; boosts Sprint +2.0 s",
                      text)
        self.assertIn("kitestage run 1.0 s, boost Nitro Boosts 4.0 s, patch 2.0 s; boosts Nitro Boosts +1.0 s", text)
        self.assertIn("Feign Death +3.0 s unasked", text)
        self.assertIn("own immunity on a kiter anub.protect never asked for: Feign Death 1", text)
        self.assertIn("wave 1 0:05.000: focus due 0:20.000, 30.0 s before wave 2 at 0:50.000", text)
        self.assertIn("wave 2 0:50.000: focus due 0:55.000, the last wave", text)
        self.assertIn("main tank Bulwark in a patch 33% of 60 samples", text)
        self.assertIn(f"submerged 0:30.000 at {SUBMERGE_AT[0]:.1f}, {SUBMERGE_AT[1]:.1f}: patch0 12.0 yd under",
                      text)
        self.assertIn("anub.pickup: Bulwark attack 1, stepout 1, hold 1", text)
        self.assertIn("spike to tank patch: side 0 9.0 yd in reach\n", text)
        self.assertIn("spike within 11.5 yd of a latched tank patch, its fail test's reach: 1 window(s)", text)
        self.assertIn("side 1  never latched one\n", text)


T0, R1, KA, KB = (CREATURE | n for n in range(951, 955))
BA, BB, BC, BD = (CREATURE | n for n in range(961, 965))
SPK2 = CREATURE | 942
TP0, TP1 = (X + 3.0, Y + O), (X + 3.0, Y - O)
R1_FLY = (X + 20.0, Y - 25.0)
KA_FLY = (X - 30.0, Y + 25.0)
DK_CAST = (X - 26.0, Y + 12.0)   # 20 yd off the first submerge's gather centre
DK_AFTER = (X - 20.0, Y + 12.0)  # 6 yd on from the cast
KB_SPOT = (X - 25.0, Y - 15.0)
SLOTS = {MAGE: (X - 13.0, Y), DRUID: (X - 7.0, Y), LOCK: (X - 10.0, Y + 3.0), HUNTER: (X - 10.0, Y - 3.0),
         ROGUE: (X - 10.0, Y + 3.0)}
# kiter, mark on, mark off, and where the spike sits meanwhile
KITES = ((ROGUE, 11000, 17000, (X + 2.0, Y)), (HUNTER, 18000, 25000, (X - 10.0, Y + 13.0)),
         (MAGE, 42000, 46000, (X + 17.0, Y - 20.0)), (LOCK, 46500, 52000, (X - 10.0, Y + 11.0)),
         (ROGUE, 52500, 53000, (X - 10.0, Y - 10.0)))
# guid, exit, first and last sample, emerge
BURROWERS = ((BA, EXIT_N1, 2000, 20000, None), (BB, EXIT_N2, 2000, 50000, 36000), (BC, EXIT_S1, 2000, 40000, None),
             (BD, EXIT_S2, 46000, 60000, None))


def kite_player(guid: int, when: int) -> list:
    """Mage, druid, warlock and hunter hold the gather in the first submerge, mage, druid and rogue in the
    second. The hunter dies on its kite at 25 s and the warlock stands off in the second submerge."""
    spot, hp = {MAIN: (X - 5.0, Y), SIDE0: (TP0[0], TP0[1] + 2.0), SIDE1: (TP1[0], TP1[1] - 2.0),
                DRUID: SLOTS[DRUID]}.get(guid, (X + 10.0, Y)), 100.0
    if guid == DK:
        spot = DK_AFTER if when >= 15500 else DK_CAST if when >= 13000 else (X - 5.0, Y + 5.0)
    elif guid == MAGE:
        spot = (X + 20.0, Y - 20.0) if 42000 < when <= 46000 else SLOTS[MAGE]
    elif guid == LOCK:
        spot = (X + 15.0, Y + 15.0) if when >= 30000 else SLOTS[LOCK]
    elif guid == HUNTER:
        spot = (X - 10.0, Y + 16.0) if when > 18000 else SLOTS[HUNTER]
        hp = 0.0 if when >= 25000 else 100.0
    elif guid == ROGUE and 40000 <= when < 52500:
        spot = SLOTS[ROGUE]
    return [guid, *spot, Z, 0.0, hp, 100.0, 0, 0, 0, 0, 0]


def kite_sphere(guid: int, when: int):
    """`(xy, z)` or None. T0 lies on side 0's spot from the pull. In the first submerge R1 lands on side 1's
    spot and latches, the DK grips KA at 15 s, which dies where the DK stood, and KB is shot down after."""
    if when < 0:
        return None
    if guid == T0:
        return TP0, PATCH_Z
    if guid == R1:
        return (R1_FLY, FLYING_Z) if when < 12500 else (TP1, PATCH_Z if when >= 14000 else FALLING_Z)
    if guid == KA:
        return (KA_FLY, FLYING_Z) if when <= 15000 else (DK_CAST, PATCH_Z if when >= 17500 else Z + 0.5)
    return KB_SPOT, FLYING_Z if when < 20000 else PATCH_Z if when >= 21500 else FALLING_Z


def kite_snap(when: int) -> dict:
    units = [kite_player(guid, when) for guid in (MAIN, SIDE0, SIDE1, DK, MAGE, LOCK, ROGUE, DRUID, HUNTER)]
    units.append([BOSS, X - 5.0, Y + 2.0, Z, 0.0, 100.0 - max(when, 0) / 1000.0, 0.0, MAIN, 0, 0, 0, 0])
    units += [[SPK2, *spot, Z, 0.0, 100.0, 0.0, kiter, 0, 0, 0, 0]
              for kiter, start, stop, spot in KITES if start <= when <= stop]
    hz = []
    for guid in (T0, R1, KA, KB):
        place = kite_sphere(guid, when)
        if place is None:
            continue
        (x, y), z = place
        units.append([guid, x, y, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_25H, x, y, PATCH_Z, 6.0, 1])
    for guid, spot, first, last, emerge in BURROWERS:
        if first <= when <= last:
            up = emerge if emerge and when >= emerge else first
            units.append([guid, *spot, Z, 0.0, 100.0 - (when - up) / 1000.0, 0.0, 0, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def kite_pull() -> list[dict]:
    """A 25H pull: P1 to 10 s, P2 to 30 s, P1b to 40 s, P2b to 55 s. The marks end on a Vanish and a death
    near the gather, an Ice Block far from it, a patch, and a Vanish as the emerge takes the spike."""
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 7, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": MAIN, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
             {"g": SIDE0, "n": "Ecoterrorist", "r": "tank", "c": "druid", "h": 0},
             {"g": SIDE1, "n": "Aegis", "r": "tank", "c": "paladin", "h": 0},
             {"g": DK, "n": "Deathsong", "r": "melee", "c": "deathknight", "h": 0},
             {"g": MAGE, "n": "Frostfire", "r": "ranged", "c": "mage", "h": 0},
             {"g": LOCK, "n": "Hellflame", "r": "ranged", "c": "warlock", "h": 0},
             {"g": ROGUE, "n": "Shadowstep", "r": "melee", "c": "rogue", "h": 0},
             {"g": DRUID, "n": "Druidica", "r": "ranged", "c": "druid", "h": 0},
             {"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 11000, "e": "unit", "g": SPK2, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"}
          for guid in (T0, R1, KA, KB)),
        *({"t": first, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"}
          for guid, _, first, _, _ in BURROWERS),
        *({"e": "spell", "sp": spell, "n": name} for spell, name in (
            (42842, "Frostbolt"), (47809, "Shadow Bolt"), (75, "Auto Shot"))),

        *(note(when, MAIN, "anub.phase", value) for when, value in ((0, 1), (10000, 2), (30000, 1), (40000, 2),
                                                                     (55000, 1))),
        note(0, MAIN, "anub.patch0", T0),
        note(0, MAIN, "anub.patch1", 0),
        note(13900, SIDE1, "anub.patch1", R1),
        note(13000, DK, "anub.grip", 1),
        note(16000, DK, "anub.grip", 0),
        note(13000, DK, "anub.sphere", "gripk"),
        note(16000, DK, "anub.sphere", "none"),
        note(15500, MAGE, "anub.sphere", "kite"),
        note(16500, MAGE, "anub.sphere", "none"),
        *(note(when, DK, "anub.gripstep", step) for when, step in (
            (13000, "walk"), (14900, "cast"), (15500, "kill"), (16000, "none"))),
        *(note(when, guid, "anub.gather", value) for guid in (MAGE, DRUID, LOCK, HUNTER)
          for when, value in ((10200, "walk"), (11000, "hold"))),
        *(note(when, guid, "anub.gather", value) for guid in (MAGE, DRUID, ROGUE)
          for when, value in ((40200, "walk"), (41000, "hold"))),

        cast(12500, HUNTER, 75, R1),
        *grip(15000, DK, KA),
        cast(16000, MAGE, 42842, KA),
        dmg(16000, MAGE, KA, 42842, 9000),
        cast(20000, LOCK, 47809, KB),

        *(aura(when, kiter, ab.SPELL_MARK, removed, SPK2) for kiter, start, stop, _ in KITES
          for when, removed in ((start, False), (stop, True))),
        {"t": 25000, "e": "death", "g": HUNTER, "killer": SPK2, "x": X - 10.0, "y": Y + 16.0, "z": Z,
         "blow": [SPK2, 20000]},
        cast(17000, ROGUE, 26889, ROGUE),
        cast(45900, MAGE, 45438, MAGE),
        cast(53100, ROGUE, 26889, ROGUE),
        {"t": 57000, "e": "end", "out": "wipe"},
    ]
    records += [kite_snap(when) for when in range(-500, 57000, 500)]
    return records


class KitePull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_7_anub-arak_1789500004.ndjson", kite_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def holds(self):
        return ab.holder_spans(self.trace, "anub.gather", ab.pull_end(self.trace))

    def centre(self, when: int, skip=frozenset()):
        return ab.gather_centre(self.holds(), ab.Samples(self.trace), ab.submerged_spans(self.trace), when, skip)

    def assertSpot(self, spot, expected):
        self.assertIsNotNone(spot)
        self.assertAlmostEqual(spot[0], expected[0])
        self.assertAlmostEqual(spot[1], expected[1])

    def test_a_grip_lands_against_the_dks_spot_at_the_cast_not_where_it_walked_after(self):
        # the DK is 6 yd on by the first sample after the cast, past the 3 yd test
        rows = ab.grip_rows(self.trace)
        self.assertEqual([(row["sphere"], row["duty"], row["cast_at"], row["landed"], row["served"]) for row in rows],
                         [(KA, "gripk", DK_CAST, 500, None)])
        self.assertAlmostEqual(rows[0]["closest"], 0.0)

    def test_the_gather_centre_is_the_mean_spot_of_the_bots_holding_it(self):
        self.assertSpot(self.centre(17000), (X - 10.0, Y))
        self.assertSpot(self.centre(17000, {MAGE}), (X - 9.0, Y))

    def test_the_gather_centre_leaves_out_the_dead_and_holds_from_an_earlier_submerge(self):
        # the dead hunter, then the warlock standing off on its first submerge's hold
        self.assertSpot(self.centre(28000), (X - 10.0, Y + 1.0))
        self.assertSpot(self.centre(41500), (X - 10.0, Y + 1.0))

    def test_the_gather_centre_is_none_with_nobody_holding_or_while_he_is_up(self):
        self.assertIsNone(self.centre(10500))
        self.assertIsNone(self.centre(35000))

    def test_each_mark_end_reads_the_spike_against_the_gather_and_the_kite_patches_alive(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual([window["guid"] for window in windows], [ROGUE, HUNTER, MAGE, LOCK, ROGUE])
        self.assertEqual([round(window["to_gather"], 2) for window in windows],
                         [12.0, 12.0, round(math.hypot(25.5, 21.5), 2), 10.0, 10.0])
        self.assertEqual([window["kite_patches"] for window in windows], [[], *[[KA, KB]] * 4])

    def test_a_kite_ended_by_a_death_or_an_own_immunity_near_the_gather_is_flagged(self):
        windows = ab.mark_windows(self.trace)[:4]
        self.assertEqual([(window["ended"], window["near_gather"]) for window in windows],
                         [("Vanish", True), ("death", True), ("Ice Block", False), (None, False)])

    def test_an_immunity_as_the_emerge_takes_the_spike_ends_nothing(self):
        window = ab.mark_windows(self.trace)[4]
        self.assertEqual((window["outcome"], window["ended"], window["near_gather"]), ("emerge", None, False))

    def test_a_burrower_dies_inside_30_s_of_its_first_sight_or_its_last_emerge(self):
        rows = {row["guid"]: row for row in ab.burrower_windows(self.trace)}
        self.assertEqual({guid: (row["wave"], row["up"], row["emerged"], row["died"], row["inside"])
                          for guid, row in rows.items()},
                         {BA: (1, 2000, False, 20000, True), BB: (1, 36000, True, 50000, True),
                          BC: (1, 2000, False, 40000, False), BD: (2, 46000, False, None, None)})

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(self.trace)
            for _, _, section in ab.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertRegex(text, r"Deathsong +gripk +#953 +landed \+0\.5 s\n")
        self.assertIn("at the end: spike 12.0 yd from the gather; no kite patch; ended by Vanish within 20 yd of"
                      " the gather\n", text)
        self.assertIn("at the end: spike 33.4 yd from the gather; kite patch #953, #954; ended by Ice Block\n", text)
        self.assertIn("kite ended by a death or an own immunity within 20 yd of the gather: Shadowstep 0:17.000"
                      " Vanish, Trueshot 0:25.000 death\n", text)
        self.assertRegex(text, r"#962 +1 +0:36\.000 emerge +0:50\.000 +14\.0 s yes\n")
        self.assertRegex(text, r"#964 +2 +0:46\.000 sight +alive +- -\n")
        self.assertIn("wave 1: 2 of 3 dead inside, 1 later, 0 alive\n", text)
        self.assertIn("wave 2: 0 of 1 dead inside, 0 later, 1 alive\n", text)


Q0, Q1, QR, QA, QB = (CREATURE | n for n in range(971, 976))
U1, U2, U3 = (CREATURE | n for n in range(981, 984))
QR_LAND = (X + 3.0, Y + O - GAP + 1.0)  # 1 yd off side 1's spot while side 0's patch is latched
RIM0 = (PATCH0[0] + 7.0, PATCH0[1])
RIM1 = (G1_LAND[0] + 7.0, G1_LAND[1])
RIM1B = (QR_LAND[0] + 7.0, QR_LAND[1])
U3_SPOT = (X - 3.0, Y)  # 2 yd off the main tank
IN_PATCH0 = (PATCH0[0] + 3.0, PATCH0[1])


def frost_phase(when: int) -> int:
    return 2 if 20000 <= when < 40000 or when >= 55000 else 1


def frost_sphere(guid: int, when: int):
    """`(xy, z)` or None. The DK grips Q0 at 3 s, the mage kills it at 4 s and it lies on side 0's spot until
    the hunter's kite spends it at 64 s; the hunter shoots Q1 down at 8 s and the rogue's kite spends it at
    31 s; the DK grips QR onto side 1's spot at 43 s and the mage kills it at 44 s."""
    if when < 0:
        return None
    if guid == Q0:
        if when > 64000:
            return None
        if when < 3500:
            return (X + 10.0, Y + 20.0), FLYING_Z
        return PATCH0, PATCH_Z if when >= 5500 else FALLING_Z if when >= 4000 else Z + 0.5
    if guid == Q1:
        if when > 31000:
            return None
        return (G1_FLY, FLYING_Z) if when < 8000 else (G1_LAND, PATCH_Z if when >= 9500 else FALLING_Z)
    if guid == QR:
        if when < 43500:
            return (X + 20.0, Y - 30.0), FLYING_Z
        return QR_LAND, PATCH_Z if when >= 45500 else FALLING_Z if when >= 44000 else Z + 0.5
    return (X + (5.0 if guid == QB else 0.0), Y + 30.0), FLYING_Z


def frost_player(guid: int, when: int) -> list:
    """Side 0's tank waits in the gather through the first submerge, side 1's once its patch is spent."""
    spot, target = (X - 12.0, Y), 0
    if guid == MAIN:
        spot = (X - 5.0, Y)
    elif guid == SIDE0:
        spot = (X - 10.0, Y + 1.0) if 20500 <= when < 40500 else RIM0
    elif guid == SIDE1:
        spot = RIM1 if when < 31000 else (X - 10.0, Y - 1.0) if when < 45500 else RIM1B
    elif guid == DK:
        spot = PATCH0 if 2500 <= when < 4500 else QR_LAND if 42500 <= when < 44500 else (X - 8.0, Y)
    elif guid == MAGE:
        target = BOSS
    elif guid == ROGUE:
        spot, target = ((PATCH0[0] + 2.0, PATCH0[1] + 3.0), U1) if 9000 <= when <= 14000 else ((X - 20.0, Y), BOSS)
    elif guid == HUNTER:
        spot = IN_PATCH0 if 56500 <= when <= 58000 else (X - 16.0, Y)
    elif guid == HEAL:
        spot = (X - 14.0, Y)
    elif guid == MELEE:
        spot = (X - 18.0, Y)
    return [guid, *spot, Z, 0.0, 100.0, 100.0, target, 0, 0, 0, 0]


def frost_burrowers(when: int) -> list[list]:
    """U1 walks from its exit onto side 0's tank at 9 s, 4 yd off the patch; U2 reaches side 1's tank at 10 s,
    5 yd off the patch, then 3 yd from 12.5 s; U3 sits on the main tank and comes back full at 49 s."""
    rows = []
    if 6000 <= when <= 14000:
        spot = EXIT_N1 if when < 9000 else (PATCH0[0] + 4.0, PATCH0[1])
        rows.append([U1, *spot, Z, 0.0, 100.0 - (when - 6000) / 100.0, 0.0, SIDE0, 0, 0, 0, 0])
    if 6000 <= when <= 15000:
        spot = EXIT_S1 if when < 10000 else (G1_LAND[0] + (5.0 if when <= 12000 else 3.0), G1_LAND[1])
        rows.append([U2, *spot, Z, 0.0, 100.0 - (when - 6000) / 200.0, 0.0, SIDE1, 0, 0, 0, 0])
    if 41000 <= when <= 44000 or 49000 <= when <= 53000:
        hp = 100.0 - (when - 41000) / 100.0 if when <= 44000 else 100.0 - (when - 49000) / 1000.0
        rows.append([U3, *U3_SPOT, Z, 0.0, hp, 0.0, MAIN, 0, 0, 0, 0])
    return rows


def frost_snap(when: int) -> dict:
    units = [frost_player(guid, when) for guid in (MAIN, SIDE0, SIDE1, DK, MAGE, ROGUE, HUNTER, HEAL, MELEE)]
    units.append([BOSS, X - 5.0, Y + 2.0, Z, 0.0, 100.0 - max(when, 0) / 1000.0, 0.0,
                  MAIN if when >= 0 and frost_phase(when) == 1 else 0, 0, 0, 0, 0])
    hz = []
    for guid in (Q0, Q1, QR, QA, QB):
        place = frost_sphere(guid, when)
        if place is None:
            continue
        (x, y), z = place
        units.append([guid, x, y, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_25H, x, y, PATCH_Z, 6.0, 1])
    units += frost_burrowers(when)
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def frost_pull() -> list[dict]:
    """A 25H pull: P1 to 20 s, P2 to 40 s, P1b to 55 s, P2b to the end at 70 s, burrower waves at 6 and 41 s.
    The rogue's kite spends side 1's patch and a grip brings a new one 5.5 s after the emerge; the hunter,
    marked inside side 0's patch with no allowance left, spends that one."""
    # guid, patch, on, off; None runs to the end
    stints = ((SIDE0, Q0, 5500, 20500), (SIDE0, Q0, 40500, 64000), (SIDE1, Q1, 9500, 31000), (SIDE1, QR, 45500, None),
              (MAIN, Q0, 12000, 14000), (ROGUE, Q0, 10000, 16000), (MAGE, Q0, 18000, 22000), (HEAL, Q0, 25000, 27000),
              (MELEE, Q0, 11000, 15000), (HUNTER, Q0, 56500, 58000), (IMP, Q0, 10000, 20000))
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 8, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": MAIN, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
             {"g": SIDE0, "n": "Ecoterrorist", "r": "tank", "c": "druid", "h": 0},
             {"g": SIDE1, "n": "Aegis", "r": "tank", "c": "paladin", "h": 0},
             {"g": DK, "n": "Deathsong", "r": "melee", "c": "deathknight", "h": 0},
             {"g": MAGE, "n": "Frostfire", "r": "ranged", "c": "mage", "h": 0},
             {"g": ROGUE, "n": "Shadowstep", "r": "melee", "c": "rogue", "h": 0},
             {"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0},
             {"g": HEAL, "n": "Mercy", "r": "heal", "c": "priest", "h": 0},
             {"g": MELEE, "n": "Arkos", "r": "melee", "c": "druid", "h": 1}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": -500, "e": "unit", "g": IMP, "en": 416, "n": "Ruirin", "own": MAGE},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"}
          for guid in (Q0, Q1, QR, QA, QB)),
        *({"t": first, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"}
          for guid, first in ((U1, 6000), (U2, 6000), (U3, 41000))),
        *({"e": "spell", "sp": spell, "n": name} for spell, name in ((42842, "Frostbolt"), (75, "Auto Shot"))),

        *(note(when, MAIN, "anub.phase", value) for when, value in ((0, 1), (20000, 2), (40000, 1), (55000, 2))),
        note(0, MAIN, "anub.side0", SIDE0),
        note(0, MAIN, "anub.side1", SIDE1),
        *(note(when, MAIN, "anub.patch0", value) for when, value in ((0, 0), (5500, Q0), (64100, 0))),
        *(note(when, MAIN, "anub.patch1", value) for when, value in ((0, 0), (9500, Q1), (31100, 0), (45500, QR))),
        # the spend drops the allowance, and the second submerge goes under with none
        *(note(when, MAIN, "anub.allow", value) for when, value in ((0, 0), (20000, 1), (31100, 0), (55000, 0))),
        note(6000, MAIN, "anub.wave", 1),
        note(41000, MAIN, "anub.wave", 2),
        *(note(when, MAIN, "anub.focus", value) for when, value in (
            (0, 0), (6500, U1), (7000, 0), (12000, U2), (15000, 0), (50000, U3), (52000, 0))),
        *(note(when, SIDE0, "anub.sidetank", value) for when, value in (
            (0, "ready"), (9000, "hold"), (21000, "gather"), (40500, "hold"))),
        note(10000, SIDE1, "anub.sidetank", "hold"),
        note(18500, MAGE, "anub.leave", "walk"),
        note(22500, MAGE, "anub.leave", "none"),
        note(12500, ROGUE, "anub.mstand", "walk"),
        note(14000, ROGUE, "anub.mstand", "none"),
        note(15000, MAGE, "anub.lead", "walk"),
        note(17000, MAGE, "anub.lead", "none"),

        *grip(3000, DK, Q0),
        dmg(4000, MAGE, Q0, 42842, 9000),
        cast(7500, HUNTER, 75, Q1),
        dmg(8000, HUNTER, Q1, 75, 9000),
        *grip(43000, DK, QR),
        dmg(44000, MAGE, QR, 42842, 9000),

        aura(22000, ROGUE, ab.SPELL_MARK, False),
        aura(30000, ROGUE, ab.SPELL_MARK, True),
        *(note(when, ROGUE, "anub.kitestage", stage) for when, stage in (
            (22000, "run"), (27000, "patch"), (30000, "none"))),
        aura(57000, HUNTER, ab.SPELL_MARK, False),
        aura(63000, HUNTER, ab.SPELL_MARK, True),
        *(note(when, HUNTER, "anub.kitestage", stage) for when, stage in (
            (57000, "run"), (60000, "immune"), (63000, "none"))),
        {"t": 70000, "e": "end", "out": "wipe"},
    ]
    for guid, patch, start, stop in stints:
        records.append(aura(start, guid, PERMAFROST_25H, False, patch))
        if stop is not None:
            records.append(aura(stop, guid, PERMAFROST_25H, True, patch))
    records += [frost_snap(when) for when in range(-500, 70000, 500)]
    return records


class FrostPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_8_anub-arak_1789500005.ndjson", frost_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_permafrost_time_splits_at_the_phase_edges_and_leaves_out_pets(self):
        totals = ab.frost_totals(ab.frost_stints(self.trace))
        self.assertEqual({guid: (bot["group"], bot["up"], bot["down"], bot["stints"]) for guid, bot in totals.items()},
                         {SIDE0: ("side tank", 29000, 9500, 2), SIDE1: ("side tank", 20000, 26000, 2),
                          MAIN: ("main tank", 2000, 0, 1), ROGUE: ("melee", 6000, 0, 1),
                          MAGE: ("ranged", 2000, 2000, 1), HEAL: ("heal", 0, 2000, 1), MELEE: ("melee", 4000, 0, 1),
                          HUNTER: ("ranged", 0, 1500, 1)})

    def test_each_long_stint_reads_its_phase_and_the_bots_target_at_its_start(self):
        rows = [row for row in ab.frost_stints(self.trace) if row["stop"] - row["start"] > ab.FROST_STINT_MS]
        self.assertEqual([(row["guid"], row["start"], row["phase"], row["target"], row["closed"]) for row in rows],
                         [(SIDE0, 5500, "P1", 0, True), (SIDE1, 9500, "P1", 0, True), (ROGUE, 10000, "P1", U1, True),
                          (MELEE, 11000, "P1", 0, True), (MAGE, 18000, "P1", BOSS, True),
                          (SIDE0, 40500, "P1b", 0, True), (SIDE1, 45500, "P1b", 0, False)])

    def test_a_focus_pick_reads_its_time_up_and_where_it_stood(self):
        picks = ab.focus_picks(self.trace)
        self.assertEqual([(row["t"], row["guid"], row["hp"], row["up"], row["target"], row["settled"])
                          for row in picks],
                         [(6500, U1, 95.0, 500, SIDE0, False), (12000, U2, 70.0, 6000, SIDE1, True),
                          (50000, U3, 99.0, 1000, MAIN, True)])
        self.assertAlmostEqual(picks[0]["side_tank"], min(math.dist(EXIT_N1, RIM0), math.dist(EXIT_N1, RIM1)))
        self.assertAlmostEqual(picks[0]["patch"], math.dist(EXIT_N1, PATCH0))
        self.assertAlmostEqual(picks[1]["side_tank"], 2.0)
        self.assertAlmostEqual(picks[1]["patch"], 5.0)
        self.assertAlmostEqual(picks[2]["side_tank"], min(math.dist(U3_SPOT, RIM0), math.dist(U3_SPOT, RIM1B)))
        self.assertAlmostEqual(picks[2]["patch"], min(math.dist(U3_SPOT, PATCH0), math.dist(U3_SPOT, QR_LAND)))

    def test_a_burrower_on_a_side_tank_reads_its_median_distance_from_that_sides_patch(self):
        # U3 sits on the main tank, no side tank, so it never counts
        self.assertEqual([(row["guid"], row["held"], len(row["gaps"]), row["median"])
                          for row in ab.held_rows(self.trace)],
                         [(U1, 11, 11, 4.0), (U2, 11, 11, 3.0)])

    def test_each_side_reads_its_first_tank_patch_from_a_grip_or_shots(self):
        rows = ab.first_patches(self.trace)
        self.assertEqual([(row["side"], row["t"], row["guid"], row["grip"] and row["grip"]["t"], row["death"]["killer"])
                          for row in rows], [(0, 5500, Q0, 3000, MAGE), (1, 9500, Q1, None, HUNTER)])
        self.assertAlmostEqual(rows[0]["death"]["off_spot"], 3.0)
        self.assertAlmostEqual(rows[1]["death"]["off_spot"], 2.0)

    def test_a_tank_patch_gone_under_him_names_the_kite_that_spent_it(self):
        self.assertEqual([(row["side"], row["guid"], row["gone"], row["label"], row["kiter"], row["mark"], row["into"],
                           row["stage"]) for row in ab.spent_patches(self.trace)],
                         [(1, Q1, 31000, "P2", ROGUE, 22000, 8000, "patch"),
                          (0, Q0, 64000, "P2b", HUNTER, 57000, 6000, "immune")])

    def test_a_spent_sides_next_patch_reads_its_grip_against_the_emerge(self):
        rows = ab.spent_patches(self.trace)
        back = rows[0]["back"]
        self.assertEqual((back["t"] - rows[0]["emerge"], back["guid"], back["grip"]["t"], back["grip"]["landed"]),
                         (5500, QR, 43000, 500))
        self.assertAlmostEqual(back["death"]["off_spot"], 1.0)
        self.assertIsNone(rows[1]["back"])

    def test_a_spend_past_the_heroic_allowance_is_over(self):
        rows = ab.allowance_rows(self.trace, ab.sphere_deaths(self.trace), ab.spent_patches(self.trace))
        self.assertEqual([(row["label"], row["allow"], row["dropped"], len(row["spent"]), row["over"]) for row in rows],
                         [("P2", 1, 31100, 1, False), ("P2b", 0, None, 1, True)])

    def test_a_mark_starting_inside_a_patch_is_flagged(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual([(window["guid"], window["in_frost"]) for window in windows], [(ROGUE, False), (HUNTER, True)])
        self.assertAlmostEqual(windows[1]["frost_yd"], 3.0)

    def test_side_tank_gather_time_per_submerge(self):
        self.assertEqual(ab.side_tank_gather(self.trace), [("P2", 20000, {SIDE0: 19000}), ("P2b", 55000, {})])

    def test_the_newer_probes_read_empty_on_a_pull_without_their_rows(self):
        self.assertEqual(ab.spot_changes(self.trace), [])
        self.assertEqual(ab.hstand_time(self.trace), {})
        self.assertEqual(ab.engine_passes(self.trace), {})
        self.assertEqual(ab.healer_engine_coverage(self.trace), {})
        self.assertEqual(ab.healer_follows(self.trace), {})
        self.assertEqual(ab.defensive_rows(self.trace), [])
        self.assertEqual([row["step"] for row in ab.grip_rows(self.trace)], [None, None])
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_healers(self.trace)
            ab.show_tanks(self.trace)
        self.assertIn("no cov records, so no engine passes and no raid healer engine coverage\n", out.getvalue())
        self.assertIn("anub.hstand: no rows\n", out.getvalue())
        self.assertIn("no tank defensive cast\n", out.getvalue())

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(self.trace)
            for _, _, section in ab.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertRegex(text, r"\n  main tank +1 +2\.0 +0\.0 +2\.0\n")
        self.assertRegex(text, r"\n  side tank +2 +49\.0 +35\.5 +84\.5\n")
        # Arkos is human, so melee totals only Shadowstep
        self.assertRegex(text, r"\n  melee +1 +6\.0 +0\.0 +6\.0\n")
        self.assertRegex(text, r"\n  Arkos +melee +4\.0 +0\.0 +4\.0 +1 +4\.0  human\n")
        self.assertRegex(text, r"0:10\.000 Shadowstep +melee +6\.0 s P1 +burrower #981\n")
        self.assertRegex(text, r"0:18\.000 Frostfire +ranged +4\.0 s P1 +Anub'arak\n")
        self.assertRegex(text, r"0:45\.500 Aegis +side tank +24\.5 s P1b +-  on at the end\n")
        self.assertNotIn("Ruirin", text)
        self.assertIn("anub.leave: Frostfire walk 1, none 1\n", text)
        self.assertIn("anub.mstand: Shadowstep walk 1, none 1\n", text)
        self.assertIn("anub.lead: Frostfire walk 1, none 1\n", text)
        self.assertIn("anub.sidetank gather while he was down: P2 Ecoterrorist 19.0 s; P2b none\n", text)
        self.assertRegex(text, r"0:06\.500 #981 +95% +0\.5 s Ecoterrorist .* before its tank\n")
        self.assertIn("1 of 3 pick(s) before its target was a tank within 5 yd", text)
        self.assertRegex(text, r"#982 +11 +11 +3\.0 yd\n")
        self.assertIn("side 0   0:05.500  #971 from a grip by Deathsong at 0:03.000, landed +0.5 s, killed by"
                      " Frostfire Frostbolt 42842, 3.0 yd off its spot\n", text)
        self.assertIn("side 1   0:09.500  #972 from shots, killed by Trueshot Auto Shot 75, 2.0 yd off its spot\n",
                      text)
        self.assertIn("P2     0:20.000  allowance  1  tank 0  other 0  spent 1, allowance 0 from 0:31.100\n", text)
        self.assertIn("side 1 #972 gone 0:31.000, Shadowstep's mark 8.0 s in, kitestage patch\n        back 0:45.500,"
                      " 5.5 s after the emerge: #973 from a grip by Deathsong at 0:43.000, landed +0.5 s, killed by"
                      " Frostfire Frostbolt 42842, 1.0 yd off its spot\n", text)
        self.assertIn("P2b    0:55.000  allowance  0  tank 0  other 0  spent 1  over\n", text)
        self.assertIn("side 0 #971 gone 1:04.000, Trueshot's mark 6.0 s in, kitestage immune\n"
                      "        not back by the end\n", text)
        self.assertRegex(text, r"0:22\.000 Shadowstep +melee +8\.0 +- +patch +0 +0 +-  tank patch 1 spent\n")
        self.assertIn("marked 3.0 yd from a Permafrost centre, inside its slow\n", text)
        self.assertIn("marked within 7.5 yd of a Permafrost centre: Trueshot 0:57.000 3.0 yd\n", text)


R0, RG, RR = (CREATURE | n for n in range(1001, 1004))
RX, RY = CREATURE | 1011, CREATURE | 1012
GHOUL = CREATURE | 1021
SCARAB = CREATURE | 1031
SPK3 = CREATURE | 1041
EXPOSE_25 = 67847
EXPOSE_10 = 67721

MAIN_AT = (X - 5.0, Y)
SIDE0_AT = (X + 3.0, Y + O + 4.0)
SIDE1_AT = (X, Y - O - 2.0)
H_NEAR, H_OUT, H_STAND = (X - 5.0, Y + 10.0), (X - 5.0, Y + 30.0), (X - 5.0, Y + 5.0)
PAL_AT = (X - 41.0, Y - 14.0)       # 41 yd from side 1's tank, 53 from side 0's
B_DOWN = (X - 20.0, Y)
K0 = (X - 22.0, Y + 3.0)
DK_GRIP0, DK_P2, DK_P1B = (X + 3.0, Y + O + 0.5), (X - 30.0, Y), (X - 10.0, Y + 5.0)
G_SPOT = (X + 3.0, Y - 4.0)         # 20 yd from side 0's tank, 16 from its patch
RG_FLY = (X - 10.0, Y + 25.0)
RR_FLY, RR_LAND = (X - 25.0, Y + 40.0), (X - 23.0, Y + 15.0)
SPOT0_NEW = (X - 25.0, Y + 15.0)    # 2 yd from where the refill lands


def refill_kiter(when: int) -> tuple[float, float]:
    """The hunter runs east at 7 yd/s from its mark at 22.5 s, stands still 26-28 s, then runs on to 32 s."""
    if when < 22500:
        return K0
    run = min(when, 26000) - 22500 + max(0, min(when, 32000) - 28000)
    return K0[0] + 7.0 * run / 1000.0, K0[1]


def refill_player(guid: int, when: int) -> list:
    hp, target = 100.0, 0
    if guid == MAIN:
        spot, target = MAIN_AT, BOSS
    elif guid == SIDE0:
        spot = SIDE0_AT
    elif guid == SIDE1:
        spot, hp = SIDE1_AT, 0.0 if when >= 15000 else 100.0
    elif guid == HEAL:
        spot = H_NEAR if when < 10000 else H_OUT if when < 40000 else H_STAND
    elif guid == PALADIN:
        spot = PAL_AT
    elif guid == HUNTER:
        spot = refill_kiter(when)
    elif guid == ROGUE:
        spot = (X - 18.0, Y - 6.0)
    elif guid == MELEE:
        spot = (X - 20.0, Y + 15.0)
    else:
        spot = DK_GRIP0 if when < 5000 else DK_P2 if when < 40000 else DK_P1B
    return [guid, *spot, Z, 0.0, hp, 100.0, target, 0, 0, 0, 0]


def refill_sphere(guid: int, when: int):
    """`(xy, z)` or None. The DK grips R0 at 2 s and it dies at 3 s onto side 0's spot, where the hunter's kite
    spends it at 32 s; the DK grips RG at 43 s, it lands, nobody kills it and it floats back up at 48.5 s; RR
    is shot down at 50 s 2 yd from side 0's new spot."""
    if when < 0:
        return None
    if guid == R0:
        if when > 32000:
            return None
        if when < 2500:
            return (X + 10.0, Y + 20.0), FLYING_Z
        return TP0, Z + 0.5 if when < 3000 else FALLING_Z if when < 4500 else PATCH_Z
    if guid == RG:
        return (DK_P1B, Z + 0.5) if 43500 <= when < 48500 else (RG_FLY, FLYING_Z)
    if when < 50000:
        return RR_FLY, FLYING_Z
    return RR_LAND, FALLING_Z if when < 51500 else PATCH_Z


def refill_burrowers(when: int) -> list[list]:
    """RX holds side 0's tank to 10 s, goes for the human's ghoul and goes under at 12 s, 20 yd from its tank,
    back up full at 22.5 s; RY sits on side 1's tank until it dies at 15 s."""
    rows = []
    spot = hp = None
    target = 0
    if 2000 <= when < 10000:
        spot, hp, target = (SIDE0_AT[0] + 1.0, SIDE0_AT[1]), 100.0 - (when - 2000) / 400.0, SIDE0
    elif when == 10000:
        spot, hp, target = (X + 3.0, Y + 6.0), 79.0, GHOUL
    elif when == 10500:
        spot, hp, target = (X + 3.0, Y + 1.0), 78.5, GHOUL
    elif 11000 <= when < 12000:
        spot, hp, target = G_SPOT, 78.0 if when == 11000 else 75.0, GHOUL
    elif 12000 <= when <= 22000:
        spot, hp = G_SPOT, 70.0
    elif 22500 <= when <= 30000:
        spot, hp = EXIT_N1, 100.0
    if spot is not None:
        rows.append([RX, *spot, Z, 0.0, hp, 0.0, target, 0, 0, 0, 0])
    if 2000 <= when <= 20000:
        rows.append([RY, SIDE1_AT[0] + 1.0, SIDE1_AT[1], Z, 0.0, 100.0 - (when - 2000) / 1000.0, 0.0,
                     SIDE1 if when < 15000 else 0, 0, 0, 0, 0])
    return rows


def refill_snap(when: int) -> dict:
    units = [refill_player(guid, when) for guid in (MAIN, SIDE0, SIDE1, HEAL, PALADIN, HUNTER, ROGUE, MELEE, DK)]
    up = 0 <= when < 20000 or when >= 40000
    units.append([BOSS, *(B_DOWN if 15000 <= when < 40000 else (X - 5.0, Y + 2.0)), Z, 0.0,
                  100.0 - max(when, 0) / 1000.0, 0.0, MAIN if up else 0, 0, 0, 0, 0])
    if 22500 <= when <= 32000:
        units.append([SPK3, K0[0] - 6.0 + 3.5 * (when - 22500) / 1000.0, K0[1], Z, 0.0, 100.0, 0.0, HUNTER,
                      0, 0, 0, 0])
    hz = []
    for guid in (R0, RG, RR):
        place = refill_sphere(guid, when)
        if place is None:
            continue
        (x, y), z = place
        units.append([guid, x, y, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_25H, x, y, PATCH_Z, 6.0, 1])
    units += refill_burrowers(when)
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def expose(when: int, guid: int, spell: int, stacks: int) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": RX, "sp": spell, "r": 1 if not stacks else 0, "st": stacks}


def act(when: int, guid: int, action: str, verdict: str = "OK") -> dict:
    return {"t": when, "e": "act", "g": guid, "a": action, "rel": 1.0, "vd": verdict}


def refill_pull() -> list[dict]:
    """A 25H pull: P1 to 20 s, P2 to 40 s, P1b to the end at 60 s, burrower waves at 2 and 47 s. Side 1's tank
    dies at 15 s; the hunter's kite takes a Hand of Protection on a spike half its speed and spends side 0's
    patch while running."""
    spot = "{:.2f},{:.2f},142.60".format
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 9, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": MAIN, "n": "Ecoterrorist", "r": "tank", "c": "druid", "h": 0},
             {"g": SIDE0, "n": "Bulwark", "r": "tank", "c": "paladin", "h": 0},
             {"g": SIDE1, "n": "Felesta", "r": "tank", "c": "deathknight", "h": 0},
             {"g": HEAL, "n": "Mercy", "r": "heal", "c": "priest", "h": 0},
             {"g": PALADIN, "n": "Justice", "r": "heal", "c": "paladin", "h": 0},
             {"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0},
             {"g": ROGUE, "n": "Shadowstep", "r": "melee", "c": "rogue", "h": 0},
             {"g": MELEE, "n": "Arkos", "r": "melee", "c": "deathknight", "h": 1},
             {"g": DK, "n": "Obliteration", "r": "melee", "c": "deathknight", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 22000, "e": "unit", "g": SPK3, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"} for guid in (R0, RG, RR)),
        *({"t": 2000, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"} for guid in (RX, RY)),
        {"t": 9000, "e": "unit", "g": GHOUL, "en": 24207, "n": "Army of the Dead Ghoul", "own": MELEE},
        {"t": 12000, "e": "unit", "g": SCARAB, "en": ab.NPC_SCARAB, "n": "Swarm Scarab"},
        {"e": "covdef", "d": [[1, "timer", "nc", "n"], [2, "party member low health", "holy", "c"],
                              [3, "party member medium health", "holy", "c"], [5, "often", "holy", "c"],
                              [6, "raid healer combat engine trigger", ab.HEALER_ENGINE, "c"],
                              [7, "raid healer combat engine trigger", ab.HEALER_ENGINE, "n"]]},

        *(note(when, MAIN, "anub.phase", value) for when, value in ((0, 1), (20000, 2), (40000, 1))),
        note(0, MAIN, "anub.side0", SIDE0),
        note(0, MAIN, "anub.side1", SIDE1),
        *(note(when, MAIN, "anub.patch0", value) for when, value in ((0, 0), (5000, R0), (32100, 0), (52000, RR))),
        note(0, MAIN, "anub.patch1", 0),
        # instance-level, x,y,z; side 1's written bare x,y
        note(0, MAIN, "anub.spot0", spot(X, Y + O)),
        note(0, MAIN, "anub.spot1", f"{X:.2f},{Y - O:.2f}"),
        note(33000, MAIN, "anub.spot0", spot(*SPOT0_NEW)),
        note(2000, MAIN, "anub.wave", 1),
        note(47000, MAIN, "anub.wave", 2),
        note(2000, RX, "anub.bside", 0),
        note(2000, RY, "anub.bside", 1),

        *(note(when, DK, "anub.gripstep", step) for when, step in (
            (1000, "walk"), (1900, "cast"), (2200, "wait"), (2500, "kill"), (4000, "none"), (41000, "walk"),
            (42900, "cast"), (43000, "none"))),
        *(note(when, DK, "anub.sphere", duty) for when, duty in (
            (1000, "grip0"), (2500, "0"), (4000, "none"), (41000, "grip1"), (43000, "none"))),
        *grip(2000, DK, R0),
        dmg(2800, HUNTER, R0, 75, 9000),
        *grip(43000, DK, RG),
        dmg(49800, HUNTER, RR, 75, 9000),

        aura(22500, HUNTER, ab.SPELL_MARK, False, SPK3),
        aura(32000, HUNTER, ab.SPELL_MARK, True, SPK3),
        note(22500, HUNTER, "anub.kitestage", "run"),
        note(32000, HUNTER, "anub.kitestage", "none"),
        cast(22700, PALADIN, 10278, HUNTER),

        *(expose(when, SIDE0, EXPOSE_25, stacks) for when, stacks in (
            (5000, 1), (6000, 2), (7000, 3), (8000, 4), (9000, 5), (10000, 6), (18000, 0))),
        *(expose(when, SIDE1, EXPOSE_10, stacks) for when, stacks in ((6000, 1), (8000, 2), (11000, 3), (15000, 0))),
        *(expose(when, HEAL, EXPOSE_25, stacks) for when, stacks in ((10500, 1), (11000, 2), (20000, 0))),
        note(1000, MAIN, "anub.defensive", "none"),
        note(9500, SIDE0, "anub.defensive", "divine protection"),
        note(11500, SIDE1, "anub.defensive", "anti magic shell"),
        note(12000, SIDE1, "anub.defensive", "covered"),
        # no note for the second Divine Protection, its verdict didn't change. The triggered copy and the
        # dps DK's Icebound Fortitude don't count
        cast(9500, SIDE0, 498, SIDE0),
        cast(11500, SIDE1, 48707, SIDE1),
        cast(13000, SIDE0, 498, SIDE0),
        triggered(13000, SIDE0, 498, SIDE0),
        cast(12500, DK, 48792, DK),

        # side 1's death: the hit at 2.5 s and the heal at 16 s fall outside its 12 s
        dmg(2500, RY, SIDE1, 0, 20000),
        *(dmg(when, RY, SIDE1, 0, 20000) for when in (5000, 10000, 14900)),
        dmg(12000, BOSS, SIDE1, 0, 10000),
        dmg(13000, SCARAB, SIDE1, 0, 5000),
        {"t": 8000, "e": "heal", "s": HEAL, "d": SIDE1, "sp": 48071, "a": 8000, "oh": 1000},
        {"t": 13000, "e": "heal", "s": HEAL, "d": SIDE1, "sp": 48071, "a": 6000, "oh": 0},
        {"t": 14000, "e": "heal", "s": PALADIN, "d": SIDE1, "sp": 48782, "a": 4000, "oh": 4000},
        {"t": 16000, "e": "heal", "s": HEAL, "d": SIDE1, "sp": 48071, "a": 5000, "oh": 0},
        {"t": 11000, "e": "abs", "d": SIDE1, "s": HEAL, "sp": 48066, "a": 3000},
        {"t": 15000, "e": "death", "g": SIDE1, "killer": RY, "x": SIDE1_AT[0], "y": SIDE1_AT[1], "z": Z,
         "blow": [RY, 20000]},

        act(-1000, HEAL, "follow"),
        act(30000, HEAL, "follow"),
        act(30500, HEAL, "follow", "FAILED"),
        act(31000, HEAL, "follow"),
        act(35000, PALADIN, "follow"),
        act(36000, HUNTER, "follow"),
        note(41000, HEAL, "anub.hstand", "walk"),
        note(44000, HEAL, "anub.hstand", "hold"),
        note(41000, PALADIN, "anub.hstand", "hold"),
        # Mercy's often node reads double; Justice never carries the non-combat raid healer node
        {"t": 60000, "e": "cov", "g": HEAL, "r": [[1, 50, 0, 0, 0, 0, 10], [2, 300, 20, 20, 15],
                                                  [3, 280, 0, 0, 0, 0, 20], [5, 600], [6, 300, 4, 4, 3],
                                                  [7, 60, 2, 2, 2]]},
        {"t": 60000, "e": "cov", "g": PALADIN, "r": [[1, 30], [2, 250], [6, 250]]},
        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    records += [refill_snap(when) for when in range(-500, 60000, 500)]
    return records


class RefillPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_9_anub-arak_1789500006.ndjson", refill_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_spot_parses_x_and_y_with_or_without_z(self):
        self.assertEqual(ab.parse_spot("721.04,129.01,142.6"), (721.04, 129.01))
        self.assertEqual(ab.parse_spot("721.0,129.0"), (721.0, 129.0))
        self.assertIsNone(ab.parse_spot("721.0"))
        self.assertIsNone(ab.parse_spot("a,b"))

    def test_each_spot_change_reads_its_side_and_how_far_it_moved(self):
        rows = ab.spot_changes(self.trace)
        self.assertEqual([(row["side"], row["t"]) for row in rows], [(0, 0), (1, 0), (0, 33000)])
        self.assertAlmostEqual(rows[2]["spot"][0], SPOT0_NEW[0])
        self.assertAlmostEqual(rows[2]["moved"], math.dist(SPOT0_NEW, (X, Y + O)), places=2)
        self.assertIsNone(rows[0]["moved"])

    def test_a_refill_lands_against_the_spot_its_side_wrote(self):
        rows = {row["guid"]: row for row in ab.sphere_deaths(self.trace)}
        self.assertEqual((rows[RR]["side"], rows[RR]["killed"]), (0, 50000))
        self.assertAlmostEqual(rows[RR]["off_spot"], 2.0, places=2)
        self.assertAlmostEqual(rows[R0]["off_spot"], 3.0, places=2)

    def test_a_grip_reads_whether_its_duty_held_to_kill_and_a_sphere_back_up(self):
        self.assertEqual([(row["t"], row["sphere"], row["landed"], row["step"], row["step_ms"], row["rose"])
                          for row in ab.grip_rows(self.trace)],
                         [(2000, R0, 500, "kill", 500, None), (43000, RG, 500, "none", 0, 5500)])

    def test_a_burrower_submerge_reads_where_it_went_under(self):
        rows = ab.burrower_submerges(self.trace)
        self.assertEqual([(row["guid"], row["t"], row["back"], row["spot"], row["hp"], row["patches"], row["side"],
                           row["tank"], row["alive"], row["victim"]) for row in rows],
                         [(RX, 12000, 22500, G_SPOT, 70.0, 1, 0, SIDE0, True, GHOUL)])
        self.assertAlmostEqual(rows[0]["frost"], 16.0)
        self.assertAlmostEqual(rows[0]["tank_yd"], 20.0)
        self.assertEqual(ab.submerge_reasons(rows[0]), ["off its side tank"])

    def test_a_ghoul_reads_by_name_and_owner(self):
        self.assertEqual(ab.unit_label(self.trace, GHOUL), "Army of the Dead Ghoul (Arkos)")

    def test_a_cover_on_a_spike_no_faster_than_the_kiter_is_flagged(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual(len(windows), 1)
        cover = windows[0]["cover"]
        self.assertEqual([(row["rec"]["t"], row["slow"]) for row in cover], [(22700, True)])
        self.assertAlmostEqual(cover[0]["spike"], 3.5)
        self.assertAlmostEqual(cover[0]["kiter"], 7.0)

    def test_a_kiter_standing_still_while_marked_reads_stalled(self):
        self.assertEqual(ab.mark_windows(self.trace)[0]["stalled"], (2000, 2000))

    def test_a_tank_patch_spent_while_running_is_a_run_spend(self):
        rows = ab.spent_patches(self.trace)
        self.assertEqual([(row["side"], row["guid"], row["kiter"], row["stage"]) for row in rows],
                         [(0, R0, HUNTER, "run")])
        self.assertTrue(ab.run_spend(rows[0]))
        self.assertFalse(ab.run_spend({"stage": "patch"}))
        self.assertTrue(ab.run_spend({"stage": "boost:55004"}))

    def test_the_crowd_where_he_went_down_and_the_first_kiters_gap(self):
        rows = ab.submerge_crowds(self.trace)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["spot"], B_DOWN)
        self.assertEqual({yards: dict(counts) for yards, counts in rows[0]["near"].items()},
                         {8.0: {"ranged": 1, "melee": 1}, 12.0: {"ranged": 1, "melee": 2},
                          20.0: {"tank": 1, "ranged": 1, "melee": 3}})
        self.assertEqual((rows[0]["kiter"], rows[0]["mark"]), (HUNTER, 22500))
        self.assertAlmostEqual(rows[0]["spike_yd"], 6.0)

    def test_healer_distances_per_phase(self):
        gaps = ab.healer_rows(self.trace)["gaps"]
        self.assertAlmostEqual(statistics.median(gaps[(HEAL, "P1b")]["main"]), 5.0)
        self.assertNotIn(1, gaps[(HEAL, "P1b")])
        self.assertAlmostEqual(statistics.median(gaps[(PALADIN, "P1")]["main"]), math.dist(PAL_AT, MAIN_AT))

    def test_seconds_far_from_a_side_tank_a_burrower_is_on(self):
        far = ab.healer_rows(self.trace)["far"]
        self.assertEqual({key: ms for key, ms in far.items() if ms},
                         {(PALADIN, 1, "P1"): 13000, (PALADIN, 0, "P1"): 8000, (HEAL, 1, "P1"): 5000})

    def test_a_side_tank_with_no_healer_in_reach(self):
        bare = ab.healer_rows(self.trace)["bare"]
        self.assertEqual({key: ms for key, ms in bare.items() if ms}, {(1, SIDE1, "P1"): 5000})

    def test_follow_acts_count_only_ok_rows_from_a_healer_in_the_pull(self):
        self.assertEqual(ab.healer_follows(self.trace), {HEAL: [30000, 31000], PALADIN: [35000]})

    def test_engine_passes_take_the_most_common_node_sum(self):
        self.assertEqual(ab.engine_passes(self.trace), {HEAL: {"n": 60, "c": 300}, PALADIN: {"n": 30, "c": 250}})

    def test_raid_healer_engine_coverage_per_engine(self):
        self.assertEqual(ab.healer_engine_coverage(self.trace),
                         {"c": {"bots": {HEAL, PALADIN}, "checks": 550, "fires": 4, "won": 3},
                          "n": {"bots": {HEAL}, "checks": 60, "fires": 2, "won": 2}})

    def test_hstand_seconds_per_value(self):
        self.assertEqual(ab.hstand_time(self.trace), {HEAL: {"walk": 3000, "hold": 16000}, PALADIN: {"hold": 19000}})

    def test_expose_weakness_peak_per_wave_and_side_tank(self):
        self.assertEqual([(row["wave"], row["tank"], row["peak"], row["ms"]) for row in ab.expose_waves(self.trace)],
                         [(1, SIDE0, 6, 9000), (1, SIDE1, 3, 0), (2, SIDE0, 0, 0), (2, SIDE1, 0, 0)])

    def test_each_defensive_reads_the_stack_at_the_cast(self):
        rows = ab.defensive_rows(self.trace)
        self.assertEqual([(row["t"], row["guid"], row["spell"], row["stacks"], row["side"], row["phase"])
                          for row in rows],
                         [(9500, SIDE0, "divine protection", 5, 0, "P1"),
                          (11500, SIDE1, "anti magic shell", 3, 1, "P1"),
                          (13000, SIDE0, "divine protection", 6, 0, "P1")])
        self.assertEqual([ab.magic_on_side_tank(row) for row in rows], [False, True, False])

    def test_long_defensives_outside_phase_3_and_the_last_before_a_death(self):
        rows = ab.defensive_rows(self.trace)
        self.assertEqual([(row["t"], row["spell"]) for row in ab.long_defensive_rows(rows)],
                         [(9500, "divine protection"), (13000, "divine protection")])
        self.assertEqual(ab.long_defensive_rows([dict(rows[0], phase="P3"), dict(rows[2], side=None)]), [])
        self.assertEqual(ab.long_defensives_before(rows, SIDE0, 13000), [("divine protection", 9500)])
        self.assertEqual(ab.long_defensives_before(rows, SIDE0, 20000), [("divine protection", 13000)])
        self.assertEqual(ab.long_defensives_before(rows, SIDE1, 20000), [])

    def test_a_tank_death_reads_the_12_s_before_it(self):
        rows = ab.tank_deaths(self.trace)
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual((row["guid"], row["side"], row["main"], dict(row["taken"])),
                         (SIDE1, 1, False, {"burrower": 60000, "Anub'arak": 10000, "scarab": 5000}))
        self.assertEqual((row["heals"], row["gross"], row["overheal"], row["absorbed"], row["expose"]),
                         (3, 18000, 5000, 3000, 3))

    def test_the_new_keys_count_as_present(self):
        gone = ab.missing_probes(self.trace)
        for key in ("anub.spot0", "anub.spot1", "anub.hstand"):
            self.assertNotIn(key, gone)

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(self.trace)
            for _, _, section in ab.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertIn("tank patch 0 spent in run\n", text)
        self.assertIn("speeds at the cover: Hand of Protection +0.2 s: spike 3.5 yd/s, kiter 7.0 yd/s, no faster\n",
                      text)
        self.assertIn("stood still 2.0 s while marked, longest 2.0 s\n", text)
        self.assertIn("on a spike no faster than the kiter: Trueshot 0:22.700 Hand of Protection (3.5 vs 7.0 yd/s)\n",
                      text)
        self.assertIn("tank patch spent while the kiter ran or boosted: Trueshot 0:32.000 side 0 run\n", text)
        self.assertIn(": 8 yd 2 (melee 1, ranged 1); 12 yd 3 (melee 2, ranged 1); 20 yd 5 (tank 1, melee 3,"
                      " ranged 1)\n          first mark Trueshot 0:22.500, 6.0 yd from the spike\n", text)
        self.assertIn("duty held to kill +0.5 s\n", text)
        self.assertIn("duty dropped to none +0.0 s, never at kill; sphere back above z 150 +5.5 s\n", text)
        self.assertIn("grip duty never at kill: 1 of 2; gripped sphere back above z 150: #1002 0:48.500\n", text)
        self.assertRegex(text, r"side 0 +0:33\.000 +\d+\.\d, \d+\.\d +25\.2 yd on\n")
        self.assertRegex(text, r"#1011 +0:12\.000 .* 70% +16\.0 yd +1 +0 Bulwark 20\.0 yd +Army of the Dead Ghoul"
                               r" \(Arkos\)  off its side tank\n")
        self.assertIn("1 of 1 went under with its side tank alive and a patch alive", text)
        self.assertIn("follow acts, OK and alive in the pull: Justice 1 (0:35.000-0:35.000), Mercy 2"
                      " (0:30.000-0:31.000)", text)
        self.assertIn("Justice combat 250, non-combat 30 (11%), dead 0; Mercy combat 300, non-combat 60 (17%),"
                      " dead 0\n", text)
        self.assertIn("raid healer engine in the coverage: combat 2 bot(s), 550 checks, 4 fires, 3 won; non-combat"
                      " 1 bot(s), 60 checks, 2 fires, 2 won\n", text)
        self.assertIn("anub.hstand: Justice hold 1; Mercy walk 1, hold 1\n", text)
        self.assertIn("anub.hstand seconds: Justice hold 19.0 s; Mercy walk 3.0 s, hold 16.0 s\n", text)
        self.assertRegex(text, r"\n    side 1 Felesta +P1 5\.0 s\n")
        self.assertIn("wave 1   0:02.000  Bulwark 6 (9.0 s), Felesta 3 (0.0 s)\n", text)
        self.assertIn("Expose Weakness on anyone else, peak over the pull: Mercy 2\n", text)
        self.assertRegex(text, r"anti magic shell +stack  3  hp 100%  magic only on a side tank\n")
        self.assertIn("from 5 stacks: 2 of 3 side tank cast(s); magic only on a side tank outside phase 3: 1\n", text)
        self.assertIn("casts per tank: Bulwark divine protection 2; Felesta anti magic shell 1\n", text)
        self.assertIn(" a side tank cast outside phase 3 at 50%+ health: 2 of 2\n"
                      "     0:09.500 Bulwark        divine protection 100%\n"
                      "     0:13.000 Bulwark        divine protection 100%\n", text)
        self.assertRegex(text, r"0:15\.000 Felesta +side 1 P1 +taken 75,000 \(burrower 60,000, Anub'arak 10,000,"
                               r" scarab 5,000\); healed 13,000 by 3 heal\(s\), 5,000 over; absorbed 3,000;"
                               r" Expose peak 3; anub.defensive covered; top 100%; long defensives: none\n")


RANGER, STALKER, TRAPPER, CASTER = range(7001, 7005)
PAL_NEAR, PAL_CD, PAL_FAR, PAL_DEAD, PAL_HUMAN = range(7011, 7016)
SPK4 = CREATURE | 1051
CHASE_PATCH = (X - 14.0, Y + 3.0)
FLIP_A, FLIP_B, FLIP_C = (X, Y - 30.0), (X, Y - 10.0), (X + 20.0, Y - 20.0)
PALADIN_SPOTS = {PAL_NEAR: (X - 6.0, Y - 10.0), PAL_CD: (X - 6.0, Y - 15.0), PAL_FAR: (X - 6.0, Y + 20.0),
                 PAL_DEAD: (X - 6.0, Y - 17.0), PAL_HUMAN: (X - 6.0, Y - 18.0)}


def stalker_x(when: int) -> float:
    """The rogue runs east 1 yd a snapshot through its mark, 20-30 s."""
    return X - 10.0 + (min(max(when, 20000), 30000) - 20000) / 500.0


def stalker_spike() -> dict[int, float]:
    """The spike's x per snapshot of the rogue's mark, in pure pursuit at 3 yd a snapshot from 20 yd behind."""
    out = {20000: X - 30.0}
    for when in range(20000, 30000, 500):
        gap = stalker_x(when) - out[when]
        out[when + 500] = out[when] + min(3.0, gap)
    return out


def chase_spike(when: int, path: dict[int, float]) -> list | None:
    """The ranger's spike walks north 1 yd a snapshot past it instead of at it; the others sit 3 yd off theirs."""
    if 12000 <= when <= 18000:
        return [SPK4, X - 20.0, Y + (when - 12000) / 500.0, Z, 0.0, 100.0, 0.0, RANGER, 1, 0, 0, 0]
    if 20000 <= when <= 30000:
        return [SPK4, path[when], Y - 20.0, Z, 0.0, 100.0, 0.0, STALKER, 1, 0, 0, 0]
    if 32000 <= when <= 36000:
        return [SPK4, X + 23.0, Y + 20.0, Z, 0.0, 100.0, 0.0, TRAPPER, 0, 0, 0, 0]
    if 42000 <= when <= 46000:
        return [SPK4, X + 13.0, Y, Z, 0.0, 100.0, 0.0, CASTER, 0, 0, 0, 0]
    return None


def chase_snap(when: int, path: dict[int, float]) -> dict:
    spots = {RANGER: (X, Y), STALKER: (stalker_x(when), Y - 20.0), TRAPPER: (X + 20.0, Y + 20.0),
             CASTER: (X + 10.0, Y), **PALADIN_SPOTS}
    units = [[guid, *spot, Z, 0.0, 0.0 if guid == PAL_DEAD else 100.0, 100.0, 0, 0, 0, 0, 0]
             for guid, spot in spots.items()]
    units.append([BOSS, X, Y + 30.0, Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
    spike = chase_spike(when, path)
    if spike:
        units.append(spike)
    hz = [[PERMAFROST_25H, *CHASE_PATCH, PATCH_Z, 6.0, 1]] if 12000 <= when <= 18000 else []
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def move(when: int, guid: int, spot: tuple[float, float]) -> dict:
    return {"t": when, "e": "move", "g": guid, "k": "point", "x": spot[0], "y": spot[1], "z": Z, "tgt": 0, "ok": 1,
            "r": "", "by": "anubarak kite spike to permafrost", "pr": "forced"}


def chase_pull() -> list[dict]:
    """A 25H pull under him from 10 s with four marks: the ranger's spike walks past it (12-18 s), the rogue's
    chases it in pure pursuit while its walk flip-flops (20-30 s), the trapper carries Forbearance and drops a
    Feign Death at once (32-36 s), and the caster's Hand of Protection lands the tick the spike is near
    (42-46 s). Five paladins: one ready, one on cooldown, one far, one dead, one human."""
    path = stalker_spike()
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 10, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": RANGER, "n": "Ranger", "r": "ranged", "c": 3, "h": 0},
             {"g": STALKER, "n": "Stalker", "r": "melee", "c": 4, "h": 0},
             {"g": TRAPPER, "n": "Trapper", "r": "ranged", "c": 3, "h": 0},
             {"g": CASTER, "n": "Caster", "r": "ranged", "c": 8, "h": 0},
             {"g": PAL_NEAR, "n": "Lightbringer", "r": "heal", "c": 2, "h": 0},
             {"g": PAL_CD, "n": "Ashbringer", "r": "heal", "c": 2, "h": 0},
             {"g": PAL_FAR, "n": "Faithful", "r": "melee", "c": 2, "h": 0},
             {"g": PAL_DEAD, "n": "Fallen", "r": "heal", "c": 2, "h": 0},
             {"g": PAL_HUMAN, "n": "Arthas", "r": "melee", "c": 2, "h": 1}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 11000, "e": "unit", "g": SPK4, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        note(0, RANGER, "anub.phase", 1),
        note(10000, RANGER, "anub.phase", 2),
        *(aura(when, kiter, ab.SPELL_MARK, removed, SPK4) for kiter, start, stop in (
            (RANGER, 12000, 18000), (STALKER, 20000, 30000), (TRAPPER, 32000, 36000), (CASTER, 42000, 46000))
          for when, removed in ((start, False), (stop, True))),

        # the ranger's Feign Death sticks
        cast(17900, RANGER, ab.SPELL_FEIGN_DEATH, 0),
        aura(17900, RANGER, ab.SPELL_FEIGN_DEATH, False, RANGER),
        aura(19000, RANGER, ab.SPELL_FEIGN_DEATH, True, RANGER),
        # the stalker walks 10 yd ahead of itself, then its target flips 20 yd and back four times in 0.6 s,
        # then twice more 0.9 s apart
        *(move(when, STALKER, (stalker_x(when) + 10.0, Y - 20.0)) for when in range(20000, 26000, 200)),
        *(move(when, STALKER, spot) for when, spot in (
            (26000, FLIP_A), (26200, FLIP_B), (26400, FLIP_A), (26600, FLIP_B), (26800, FLIP_A), (29000, FLIP_C),
            (29900, FLIP_A))),
        cast(5000, PAL_CD, 10278, PAL_NEAR),
        cast(23000, PAL_NEAR, 10278, STALKER),
        aura(30000, TRAPPER, ab.SPELL_FORBEARANCE, False, PAL_NEAR),
        aura(40000, TRAPPER, ab.SPELL_FORBEARANCE, True, PAL_NEAR),
        cast(34000, TRAPPER, ab.SPELL_FEIGN_DEATH, 0),
        aura(34000, TRAPPER, ab.SPELL_FEIGN_DEATH, False, TRAPPER),
        aura(34000, TRAPPER, ab.SPELL_FEIGN_DEATH, True, TRAPPER),
        cast(42000, PAL_FAR, 10278, CASTER),
        aura(42000, CASTER, ab.SPELL_FORBEARANCE, False, PAL_FAR),
        {"t": 50000, "e": "end", "out": "wipe"},
    ]
    records += [chase_snap(when, path) for when in range(-500, 50001, 500)]
    return records


class ChasePull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_10_anub-arak_1789500007.ndjson", chase_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_walk_reversing_three_times_in_2_s_flip_flops(self):
        # the first flip jumps off the walk with no jump before it, so it reverses nothing
        moves = sorted(self.trace.of("move"), key=lambda rec: rec["t"])
        self.assertEqual(ab.flip_flops(moves, 20000, 30000), [(26200, 26800, 4)])
        self.assertEqual(ab.flip_flops(moves, 26500, 30000), [])
        self.assertEqual([window["flips"] for window in ab.mark_windows(self.trace)],
                         [[], [(26200, 26800, 4)], [], []])

    def test_a_feign_death_off_in_the_same_tick_is_dropped(self):
        self.assertEqual([window["feign_drops"] for window in ab.mark_windows(self.trace)], [[], [], [34000], []])

    def test_the_nearest_ready_paladin_as_the_spike_closes_to_12_yd(self):
        escorts = [window["escort"] for window in ab.mark_windows(self.trace)]
        self.assertIsNone(escorts[0])
        # the one on cooldown, the dead one and the human stand nearer
        self.assertEqual((escorts[1]["t"], escorts[1]["guid"], escorts[1]["forbearance"]), (22000, PAL_NEAR, False))
        self.assertAlmostEqual(escorts[1]["yards"], 10.0)
        self.assertEqual((escorts[2]["guid"], escorts[2]["forbearance"]), (None, True))

    def test_a_hand_landing_that_tick_reads_who_was_ready_before_it(self):
        escort = ab.mark_windows(self.trace)[3]["escort"]
        self.assertEqual((escort["t"], escort["guid"], escort["forbearance"]), (41999, PAL_FAR, False))
        self.assertAlmostEqual(escort["yards"], math.hypot(16.0, 20.0))

    def test_a_marked_paladin_is_not_ready(self):
        samples = ab.Samples(self.trace)
        ready = ab.hop_readiness(self.trace)
        self.assertEqual(ready["paladins"], {PAL_NEAR, PAL_CD, PAL_FAR, PAL_DEAD})
        marked = {**ready, "marks": ready["marks"] + [(PAL_NEAR, 21000, 23000, True)]}
        escort = ab.ready_paladin(samples, marked, STALKER, 22000)
        self.assertEqual(escort["guid"], PAL_FAR)
        self.assertAlmostEqual(escort["yards"], 40.0)

    def test_pursuit_replays_each_kite_against_the_real_spike(self):
        rows = ab.pursuit_rows(self.trace)
        self.assertEqual([row["guid"] for row in rows], [RANGER, STALKER, TRAPPER, CASTER])
        ranger, stalker = rows[0], rows[1]
        # the model heads at the ranger while the spike walks north past it
        self.assertEqual([len(ranger["ends"][horizon]) for horizon in (3000, 6000)], [1, 1])
        self.assertAlmostEqual(ranger["ends"][3000][0], math.hypot(6.0, 6.0))
        self.assertAlmostEqual(ranger["ends"][6000][0], math.hypot(12.0, 12.0))
        self.assertEqual([[round(value, 6) for value in ranger["passes"][horizon]] for horizon in (3000, 6000)],
                         [[-3.0], [-3.0]])
        self.assertEqual([len(stalker["ends"][horizon]) for horizon in (3000, 6000)], [9, 9])
        self.assertLess(max(stalker["ends"][3000] + stalker["ends"][6000]), 1e-9)
        self.assertEqual(stalker["passes"], {3000: [], 6000: []})
        self.assertEqual((rows[2]["ends"], rows[3]["ends"]), ({3000: [], 6000: []},) * 2)

    def test_a_snapshot_gap_ends_a_chase(self):
        runs = ab.chase_runs(ab.Samples(self.trace), {SPK4}, STALKER, 20000, 30000)
        self.assertEqual([(run[0][0], run[-1][0]) for run in runs], [(20000, 30000)])
        # a 500 ms hole at 1.25 s and the spike off the kiter at 3.5 s keep one chase; a 1 s hole splits it
        snaps = [{"t": when, "e": "snap", "hz": [],
                  "u": [[RANGER, X, Y, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
                        [SPK4, X - 5.0, Y, Z, 0.0, 100.0, 0.0, 0 if when == 3500 else RANGER, 0, 0, 0, 0]]}
                 for when in range(0, 5001, 250) if when != 1250]
        with tempfile.TemporaryDirectory() as folder:
            trace = write_trace(folder, "649_12_anub-arak_1789500009.ndjson", [
                {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 12, "diff": 3,
                 "boss": "anub-arak", "roster": [{"g": RANGER, "n": "Ranger", "r": "ranged", "c": 3, "h": 0}]},
                {"t": 0, "e": "unit", "g": SPK4, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1}, *snaps])
            runs = ab.chase_runs(ab.Samples(trace), {SPK4}, RANGER, 0, 5000)
        self.assertEqual([(run[0][0], run[-1][0]) for run in runs], [(0, 5000)])
        self.assertEqual([len(run) for run in runs], [19])
        with tempfile.TemporaryDirectory() as folder:
            trace = write_trace(folder, "649_12_anub-arak_1789500009.ndjson", [
                {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 12, "diff": 3,
                 "boss": "anub-arak", "roster": [{"g": RANGER, "n": "Ranger", "r": "ranged", "c": 3, "h": 0}]},
                {"t": 0, "e": "unit", "g": SPK4, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
                *(snap for snap in snaps if not 3250 <= snap["t"] <= 3750)])
            runs = ab.chase_runs(ab.Samples(trace), {SPK4}, RANGER, 0, 5000)
        self.assertEqual([(run[0][0], run[-1][0]) for run in runs], [(0, 3000), (4000, 5000)])

    def test_a_p90_over_the_flag_line_flags_a_kite(self):
        # p90 is the value at index int(n * 0.9) once sorted, so of ten it is the largest
        self.assertTrue(ab.pursuit_flagged([0.1] * 9 + [-3.0]))
        self.assertFalse(ab.pursuit_flagged([0.1] * 10 + [-3.0]))
        self.assertFalse(ab.pursuit_flagged([]))

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(self.trace)
            for _, _, section in ab.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertIn("            spike within 12 yd +2.0 s, nearest Hand of Protection-ready paladin: Lightbringer"
                      " 10.0 yd\n", text)
        self.assertIn("nearest Hand of Protection-ready paladin: Faithful 25.6 yd\n", text)
        self.assertIn("            walk flip-flopped from +6.2 s for 0.6 s, 4 reversals\n", text)
        self.assertIn("            Feign Death dropped at once +2.0 s\n", text)
        self.assertIn("walk flip-flopped, move targets 5+ yd apart reversing 3+ times in 2 s: Stalker 0:20.000"
                      " from +6.2 s\n", text)
        self.assertIn("Feign Death dropped within 100 ms: Trapper 0:32.000 +2.0 s\n", text)
        self.assertIn("as the spike came within 12 yd: within 30 yd in 2 of 3; further or none: Trapper 0:32.000"
                      " (Forbearance on the kiter)\n", text)
        self.assertRegex(text, r"0:12\.000 Ranger +3 s n 1: end 8\.5 / 8\.5 yd, patch pass 3\.0 / 3\.0 yd bias -3\.0"
                               r" \(n 1\); 6 s n 1: end 17\.0 / 17\.0 yd, .*  patch pass p90 over 1\.5 yd\n")
        self.assertIn("all kites, 3 s, n 10: end 0.0 / 8.5 yd, patch pass 3.0 / 3.0 yd bias -3.0 (n 1)\n", text)
        self.assertIn("all kites, 6 s, n 10: end 0.0 / 17.0 yd, patch pass 3.0 / 3.0 yd bias -3.0 (n 1)\n", text)
        self.assertIn("p90 patch-pass error over 1.5 yd over all kites: 3 s, 6 s\n", text)


WB1, WB2 = CREATURE | 1061, CREATURE | 1062
SPH = CREATURE | 1071
SC1, SC2, SC3 = (CREATURE | n for n in range(1081, 1084))
MANDIBLES_25H = 67863
WAVE_SPOTS = {MAIN: (X - 5.0, Y), SIDE0: (X + 3.0, Y + 16.0), SIDE1: (EXIT_S1[0], EXIT_S1[1] + 10.0),
              HUNTER: (EXIT_N1[0], EXIT_N1[1] - 10.0), MAGE: (EXIT_N1[0], EXIT_N1[1] - 15.0),
              LOCK: (EXIT_N1[0], EXIT_N1[1] - 20.0), ROGUE: (EXIT_S1[0], EXIT_S1[1] + 18.0),
              HEAL: (EXIT_S1[0], EXIT_S1[1] + 25.0), DK: (EXIT_S1[0], EXIT_S1[1] + 3.0), PALADIN: (X + 10.0, Y),
              IMP: (EXIT_N1[0], EXIT_N1[1] - 3.4)}


def wave_target(guid: int, when: int) -> int:
    if guid == HUNTER:
        return WB1 if 3000 <= when < 15000 else BOSS
    if guid == ROGUE:
        return WB2 if 4000 <= when < 8000 else 0
    if guid == WB1:
        if when == 2000:
            return SPH
        if when <= 3000:
            return IMP
        return HUNTER if 7000 <= when <= 10000 else SIDE0
    if guid == WB2:
        return SIDE1
    return 0


def wave_scarabs(when: int) -> list[list]:
    """SC1 on the healer dies at 25 s and its corpse is sampled to 32 s; SC2 sits on the main tank, then on the
    mage from 35 s, and outlives the 30 s emerge to 45 s; SC3 comes up in phase 1 on the healer, 40-48 s."""
    rows = []
    for guid, first, last, target in ((SC1, 12000, 25000, HEAL), (SC2, 12000, 45000, MAIN if when < 35000 else MAGE),
                                      (SC3, 40000, 48000, HEAL)):
        if first <= when <= last:
            rows.append([guid, X - 5.0, Y - 2.0, Z, 0.0, 100.0, 0.0, target, 0, 0, 0, 0])
    if 25000 < when <= 32000:
        rows.append([SC1, X - 5.0, Y - 2.0, Z, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0])
    return rows


def wave_snap(when: int) -> dict:
    units = [[guid, *spot, Z, 0.0, 0.0 if guid == DK else 100.0, 100.0, wave_target(guid, when), 0, 0, 0, 0]
             for guid, spot in WAVE_SPOTS.items()]
    up = 0 <= when < 10000 or when >= 30000
    units.append([BOSS, X, Y + 2.0, Z, 0.0, 100.0, 0.0, MAIN if up else 0, 0, 0, 0, 0])
    units.append([SPH, X, Y + 30.0, FLYING_Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
    if 2000 <= when <= 15000:
        units += [[guid, *exit_spot, Z, 0.0, 100.0 - (when - 2000) / 200.0, 0.0, wave_target(guid, when), 0, 0, 0, 0]
                  for guid, exit_spot in ((WB1, EXIT_N1), (WB2, EXIT_S1))]
    units += wave_scarabs(when)
    return {"t": when, "e": "snap", "u": units, "hz": []}


def mandibles(when: int, guid: int, stacks: int, scarab: int) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": scarab, "sp": MANDIBLES_25H, "r": 1 if not stacks else 0,
            "st": stacks}


def wave_pull() -> list[dict]:
    """A 25H pull: P1 to 10 s, P2 to 30 s, then up again to the end at 60 s. Two burrowers spawn at 2 s, one
    aimed at a sphere for a tick, then the warlock's imp, then its side tank; the hunter misdirects at the pull
    and off that burrower, the rogue's Tricks go off the other burrower and to a side tank."""
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 11, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": MAIN, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 0},
             {"g": SIDE0, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
             {"g": SIDE1, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
             {"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": 3, "h": 0},
             {"g": ROGUE, "n": "Shadowstep", "r": "melee", "c": 4, "h": 0},
             {"g": DK, "n": "Deathsong", "r": "melee", "c": 6, "h": 0},
             {"g": MAGE, "n": "Frostfire", "r": "ranged", "c": 8, "h": 0},
             {"g": LOCK, "n": "Hellflame", "r": "ranged", "c": 9, "h": 0},
             {"g": HEAL, "n": "Mercy", "r": "heal", "c": 5, "h": 0},
             {"g": PALADIN, "n": "Justice", "r": "melee", "c": 2, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": -500, "e": "unit", "g": IMP, "en": 416, "n": "Ruirin", "own": LOCK},
        {"t": -500, "e": "unit", "g": SPH, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"},
        *({"t": 2000, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"} for guid in (WB1, WB2)),
        *({"t": 12000, "e": "unit", "g": guid, "en": ab.NPC_SCARAB, "n": "Swarm Scarab"} for guid in (SC1, SC2, SC3)),
        *(note(when, MAIN, "anub.phase", value) for when, value in ((0, 1), (10000, 2), (30000, 1))),
        note(0, MAIN, "anub.side0", SIDE0),
        note(0, MAIN, "anub.side1", SIDE1),
        note(2000, MAIN, "anub.wave", 1),

        cast(500, HUNTER, 34477, MAIN),
        triggered(600, HUNTER, 34477, MAIN),
        cast(4000, HUNTER, 34477, MAIN),
        cast(5000, ROGUE, 57934, PALADIN),
        cast(20000, ROGUE, 57934, SIDE0),

        *(mandibles(when, MAIN, stacks, SC2) for when, stacks in
          ((13000, 1), (16000, 2), (20000, 3), (28000, 4), (50000, 0))),
        *(mandibles(when, HEAL, stacks, SC1) for when, stacks in ((14000, 1), (18000, 2), (22000, 0))),
        *(mandibles(when, MAGE, stacks, SC2) for when, stacks in ((38000, 1), (45000, 0))),
        *(dmg(when, SC2, MAIN, MANDIBLES_25H, 1000) for when in (15000, 18000, 21000)),
        *(dmg(when, SC2, MAIN, MANDIBLES_25H, 4000) for when in (32000, 35000)),
        *(dmg(when, SC1, HEAL, MANDIBLES_25H, 500) for when in (17000, 20000)),
        dmg(41000, SC2, MAGE, MANDIBLES_25H, 700),
        dmg(41000, SC2, IMP, MANDIBLES_25H, 300),
        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    records += [wave_snap(when) for when in range(-500, 60001, 500)]
    return records


class WavePull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_11_anub-arak_1789500008.ndjson", wave_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_scarabs_alive_after_each_emerge_by_target(self):
        rows = ab.scarab_emerges(self.trace)
        self.assertEqual([(row["label"], row["start"], row["main"], row["stacks"]) for row in rows],
                         [("P1b", 30000, MAIN, 4)])
        # SC1's corpse is still sampled at the emerge
        self.assertEqual([(offset, dict(targets)) for offset, targets in rows[0]["alive"]],
                         [(0, {MAIN: 1}), (10000, {MAGE: 1, HEAL: 1}), (20000, {}), (30000, {})])

    def test_each_scarabs_life_and_how_long_it_outlived_its_phase(self):
        self.assertEqual([(row["guid"], row["first"], row["last"], row["label"], row["past"])
                          for row in ab.scarab_lives(self.trace)],
                         [(SC1, 12000, 25000, "P2", None), (SC2, 12000, 45000, "P2", 15000),
                          (SC3, 40000, 48000, "P1b", None)])

    def test_mandibles_peak_stack_and_damage_per_player_per_phase(self):
        # a stack carried over the emerge is its peak there; the imp's tick is off the roster
        self.assertEqual([(row["label"], row["players"]) for row in ab.mandible_windows(self.trace)],
                         [("P1", []), ("P2", [(MAIN, 4, 3000), (HEAL, 2, 1000)]),
                          ("P1b", [(MAIN, 4, 8000), (MAGE, 1, 700)])])

    def test_each_redirect_reads_the_casters_target_and_the_burrowers_after(self):
        rows = ab.redirect_rows(self.trace)
        self.assertEqual([(row["t"], row["spell"], row["caster"], row["to"], row["group"], row["target"],
                           row["on_burrower"]) for row in rows],
                         [(500, 34477, HUNTER, MAIN, "main tank", BOSS, False),
                          (4000, 34477, HUNTER, MAIN, "main tank", WB1, True),
                          (5000, 57934, ROGUE, PALADIN, "melee", WB2, True),
                          (20000, 57934, ROGUE, SIDE0, "side tank", 0, False)])
        self.assertEqual([row["after"] for row in rows],
                         [[], [(1000, SIDE0), (3000, HUNTER), (8000, SIDE0)], [(1000, SIDE1)], []])

    def test_each_burrower_reads_its_opener_and_the_players_nearest_its_exit(self):
        rows = {row["guid"]: row for row in ab.pickup_rows(self.trace)}
        # a tick on the sphere at the spawn is no opener; the dead DK 3 yd off the south exit and the imp
        # are no player
        self.assertEqual({guid: row["opener"] for guid, row in rows.items()}, {WB1: (500, IMP), WB2: (0, SIDE1)})
        self.assertEqual({guid: [(member, round(yards, 6)) for member, yards in row["nearest"]]
                          for guid, row in rows.items()},
                         {WB1: [(HUNTER, 10.0), (MAGE, 15.0), (LOCK, 20.0)],
                          WB2: [(SIDE1, 10.0), (ROGUE, 18.0), (HEAL, 25.0)]})

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(self.trace)
            for _, _, section in ab.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertIn("P1b    0:30.000  +0 s 1 (Ecoterrorist 1); +10 s 2 (Frostfire 1, Mercy 1); +20 s 0; +30 s 0\n"
                      "                     main tank Ecoterrorist 4\n", text)
        self.assertIn("P2     0:10.000  2 scarab(s), median 23.0 s, longest 33.0 s; 1 lived past its end, longest"
                      " 15.0 s on\n", text)
        self.assertIn("P1b    0:30.000  1 scarab(s), median 8.0 s, longest 8.0 s; 0 lived past its end\n", text)
        self.assertIn("P2     0:10.000  4,000: Ecoterrorist 4 3,000, Mercy 2 1,000\n", text)
        self.assertIn("P1b    0:30.000  8,700: Ecoterrorist 4 8,000, Frostfire 1 700\n", text)
        self.assertRegex(text, r"0:04\.000 Misdirection +Trueshot +> Ecoterrorist +main tank on burrower #1061:"
                               r" \+1 s Bulwark, \+3 s Trueshot, \+8 s Bulwark\n")
        self.assertIn("Misdirection 2: Ecoterrorist (main tank) 2; by group main tank 2; the caster on a burrower 1\n",
                      text)
        self.assertIn("Tricks of the Trade 2: Justice (melee) 1, Bulwark (side tank) 1; by group melee 1, side tank 1;"
                      " the caster on a burrower 1\n", text)
        self.assertRegex(text, r"#1061 +1 Ruirin \(Hellflame\) \+0\.5 s +nearest Trueshot ranged 10, Frostfire ranged"
                               r" 15, Hellflame ranged 20\n")
        self.assertRegex(text, r"#1062 +1 Felesta \(tank\) \+0\.0 s +nearest Felesta tank 10, Shadowstep melee 18,"
                               r" Mercy heal 25\n")
        self.assertIn("opened on a non-tank: 1 of 2; to a tank after it: median 1.5 s, longest 1.5 s\n", text)


SLICER, BASHER, BRAWLER, ARCHER, GUNNER, LIGHT = range(8001, 8007)
BU0, BU1 = CREATURE | 1101, CREATURE | 1102
SP1 = CREATURE | 1111
SPOT0, SPOT1 = (X, Y + 15.0), (X, Y - 15.0)
SP1_AT = (X + 5.0, Y - 15.0)
# damage dealt a snapshot while he is up
RATES = {SLICER: 1000, BASHER: 2000, BRAWLER: 5000, ARCHER: 1500, GUNNER: 1000}


def split_up(when: int) -> bool:
    return 0 <= when < 20000 or 40000 <= when < 50000


def split_dealt(guid: int, when: int) -> int:
    return RATES.get(guid, 0) * sum(1 for stamp in range(0, 50000, 500) if split_up(stamp) and stamp < when)


def split_player(guid: int, when: int) -> list:
    """Assasin holds BU0 on side 0's half, walks across the midline to BU1 at 5-7 s, turns to him at 15 s
    and jumps 31 yd at the submerge; Mighty stands on him on the midline and steps 2 yd at 12 s with no
    move row; Trueshot walks 4 yd at the pull; Hellflame dies at 39.5 s; Deathsong is human."""
    up = split_up(when)
    boss = BOSS if up else 0
    moving = 0
    if guid == SLICER:
        if when < 5000:
            spot, target = (X, Y + 14.0), BU0 if when >= 0 else 0
        elif when < 7000:
            spot, target, moving = (X, Y + 14.0 - 7.0 * (when - 5000) / 500), BU1, 1
        elif when < 20000:
            spot, target = (X, Y - 14.0), BU1 if when < 15000 else BOSS
        else:
            spot, target = (X, Y + 17.0), boss
    elif guid == BASHER:
        spot, target = (X + 2.0, Y if when < 12000 else Y + 2.0), boss
    elif guid == BRAWLER:
        spot, target = (X + 3.0, Y + 14.0), BU0 if 1000 <= when < 10000 else boss
    elif guid == ARCHER:
        spot = (X - 20.0, Y + 10.0)
        target = BU0 if 0 <= when < 10000 else BOSS if 10000 <= when < 20000 else 0
    elif guid == GUNNER:
        spot = (X - 24.0 + min(max(when, 0), 2000) / 500, Y - 10.0)
        target, moving = BU1 if 0 <= when < 20000 else boss, int(0 <= when < 2000)
    else:
        spot = {MAIN: (X + 1.0, Y + 1.0), SIDE0: SPOT0, SIDE1: SPOT1, LIGHT: (X - 15.0, Y)}[guid]
        target = boss if guid == MAIN else 0
    hp = 0.0 if guid == ARCHER and when >= 39500 else 100.0
    return [guid, *spot, Z, 0.0, hp, 100.0, target, moving, 0, 0, split_dealt(guid, when)]


def split_snap(when: int) -> dict:
    units = [split_player(guid, when) for guid in (MAIN, SIDE0, SIDE1, SLICER, BASHER, BRAWLER, ARCHER, GUNNER, LIGHT)]
    # held 12 yd off the anchor from 10 s to the submerge
    spot = (X + 12.0, Y) if 10000 <= when < 20000 else (X, Y)
    hp = 100.0 - min(max(when, 0), 15000) / 1000.0 if when < 40000 else 85.0 - 3.0 * (when - 40000) / 1000.0
    units.append([BOSS, *spot, Z, 0.0, hp, 0.0, MAIN if split_up(when) else 0, 0, 0, 0, 0])
    if 1000 <= when <= 10000:
        units.append([BU0, X, Y + 16.0, Z, 0.0, 100.0 - (when - 1000) / 100.0, 0.0, SIDE0, 0, 0, 0, 0])
    if 1000 <= when <= 20000:
        units.append([BU1, X, Y - 16.0, Z, 0.0, 100.0 - (when - 1000) / 200.0, 0.0, SIDE1, 0, 0, 0, 0])
    hz = []
    if 0 <= when <= 27000:
        units.append([SP1, *SP1_AT, PATCH_Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        hz.append([PERMAFROST_25H, *SP1_AT, PATCH_Z, 6.0, 1])
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def move_row(when: int, guid: int, spot: tuple[float, float], by: str) -> dict:
    return {**move(when, guid, spot), "by": by, "pr": "combat"}


def split_pull() -> list[dict]:
    """A 25H pull on the side split: P1 to 20 s, P2 to 40 s, then up again to the end at 50 s. Burrower
    waves at 1 and 45 s; side 0 focuses BU0 to 10 s, side 1 BU1 to 15 s. Four kites under him: a bridge that
    spends side 1's patch, a hand, an own immunity, and a patch kite carried over that dies unused."""
    spot = "{:.2f},{:.2f},{:.2f}"
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 13, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": MAIN, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 0},
             {"g": SIDE0, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
             {"g": SIDE1, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
             {"g": SLICER, "n": "Assasin", "r": "melee", "c": 4, "h": 0},
             {"g": BASHER, "n": "Mighty", "r": "melee", "c": 1, "h": 0},
             {"g": BRAWLER, "n": "Deathsong", "r": "melee", "c": 6, "h": 1},
             {"g": ARCHER, "n": "Hellflame", "r": "ranged", "c": 9, "h": 0},
             {"g": GUNNER, "n": "Trueshot", "r": "ranged", "c": 3, "h": 0},
             {"g": LIGHT, "n": "Holylight", "r": "heal", "c": 2, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 0, "e": "unit", "g": SP1, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"},
        *({"t": 1000, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"} for guid in (BU0, BU1)),
        {"e": "spell", "sp": 49052, "n": "Steady Shot"},
        {"e": "spell", "sp": 47486, "n": "Mortal Strike"},
        *(note(when, MAIN, "anub.phase", value) for when, value in ((0, 1), (20000, 2), (40000, 1))),
        note(0, MAIN, "anub.side0", SIDE0),
        note(0, MAIN, "anub.side1", SIDE1),
        note(0, MAIN, "anub.spot0", spot.format(*SPOT0, Z)),
        note(0, MAIN, "anub.spot1", spot.format(*SPOT1, Z)),
        note(5000, SIDE1, "anub.patch1", SP1),
        note(27500, SIDE1, "anub.patch1", 0),
        note(1000, MAIN, "anub.wave", 1),
        note(45000, MAIN, "anub.wave", 2),

        # the split at each wave writes only the bots whose side changed: Hellflame is dead by wave 2
        *(note(1000, guid, "anub.dside", side) for guid, side in ((SLICER, 0), (BASHER, 1), (ARCHER, 0), (GUNNER, 1))),
        note(45000, GUNNER, "anub.dside", 0),
        note(0, MAIN, "anub.focus0", 0),
        note(1000, SLICER, "anub.focus0", BU0),
        note(10000, ARCHER, "anub.focus0", 0),
        note(1000, GUNNER, "anub.focus1", BU1),
        note(15000, GUNNER, "anub.focus1", 0),
        *(note(when, guid, "anub.dps", branch) for when, guid, branch in (
            (0, SLICER, "burrower"), (5000, SLICER, "loose"), (15000, SLICER, "boss"), (0, ARCHER, "burrower"),
            (10000, ARCHER, "boss"), (0, GUNNER, "burrower"), (0, BRAWLER, "burrower"))),
        # side 0's focus is no stray for a side 1 bot; BU1 after side 1 let it go is
        cast(3000, BASHER, 47486, BU0),
        cast(16000, GUNNER, 49052, BU1),
        move_row(5000, SLICER, (X, Y - 14.0), "anubarak melee burrower stand"),
        move_row(0, GUNNER, (X - 20.0, Y - 10.0), "reach spell"),

        # a drag leg at 13 s splits the 12 yd hold in two, and the submerge leg from 18 s ends it
        *(note(when, MAIN, "anub.pickup", value) for when, value in (
            (0, "attack"), (9000, "drag"), (10000, "hold"), (13000, "drag"), (14000, "hold"), (18000, "submerge"),
            (40000, "attack"), (40500, "hold"))),

        *(aura(when, kiter, ab.SPELL_MARK, removed, BOSS) for kiter, start, stop in (
            (ARCHER, 21000, 27000), (GUNNER, 28000, 33000), (GUNNER, 34000, 37500), (ARCHER, 38000, 39500))
          for when, removed in ((start, False), (stop, True))),
        note(21100, ARCHER, "anub.kiteplan", "patch+hand"),
        note(23000, ARCHER, "anub.kiteplan", "patch"),
        note(28100, GUNNER, "anub.kiteplan", "hand"),
        note(34200, GUNNER, "anub.kiteplan", "own"),
        cast(30000, LIGHT, 10278, GUNNER),
        cast(37000, GUNNER, ab.SPELL_FEIGN_DEATH, 0),
        {"t": 39500, "e": "death", "g": ARCHER, "killer": BOSS, "x": X - 20.0, "y": Y + 10.0, "z": Z,
         "blow": [BOSS, 20000]},
        {"t": 50000, "e": "end", "out": "wipe"},
    ]
    records += [split_snap(when) for when in range(-500, 50000, 500)]
    return records


class SplitPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_13_anub-arak_1789500010.ndjson", split_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, *sections) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            for section in sections:
                section(self.trace)
        return out.getvalue()

    def test_each_phase_splits_samples_damage_and_walks_per_role(self):
        rows = ab.dps_rows(self.trace)
        self.assertEqual([(row["label"], row["phase"]) for row in rows], [("P1", 1), ("P2", 2), ("P1b", 1)])
        melee, ranged = rows[0]["roles"]["melee"], rows[0]["roles"]["ranged"]
        # Deathsong is human, and Assasin's 31 yd jump at the submerge is no walk
        self.assertEqual((melee["bots"], melee["samples"], dict(melee["kinds"]), melee["moving"]),
                         ({SLICER, BASHER}, 80, {"boss": 50, "burrower": 30}, 4))
        self.assertEqual({order: round(yards, 6) for order, yards in melee["walked"].items()},
                         {"anubarak melee burrower stand": 28.0, "no move row": 2.0})
        self.assertEqual(dict(melee["dealt"]), {"boss": 90000, "burrower": 30000})
        self.assertEqual((dict(ranged["kinds"]), ranged["moving"], round(ranged["yards"], 6), dict(ranged["dealt"])),
                         ({"burrower": 60, "boss": 20}, 4, 4.0, {"burrower": 70000, "boss": 30000}))
        self.assertEqual(dict(rows[1]["roles"]["melee"]["kinds"]), {"none": 80})

    def test_a_dead_bot_drops_out_of_the_split(self):
        rows = ab.dps_rows(self.trace)
        self.assertEqual(rows[1]["roles"]["ranged"]["samples"], 79)
        self.assertEqual(rows[2]["roles"]["ranged"]["bots"], {GUNNER})

    def test_his_health_lost_per_phase(self):
        self.assertEqual([round(ab.boss_lost(row), 6) for row in ab.dps_rows(self.trace)], [15.0, 0.0, 28.5])

    def test_each_wave_reads_its_side_groups_from_the_living(self):
        self.assertEqual([(row["wave"], row["t"], row["sides"]) for row in ab.side_groups(self.trace)],
                         [(1, 1000, {0: {"melee": [SLICER], "ranged": [ARCHER]},
                                     1: {"melee": [BASHER], "ranged": [GUNNER]}}),
                          (2, 45000, {0: {"melee": [SLICER], "ranged": [GUNNER]},
                                      1: {"melee": [BASHER], "ranged": []}})])

    def test_a_melee_walk_across_the_midline_reads_its_group(self):
        # Mighty never leaves the midline, and Assasin's P1b start on side 0 is a new window
        self.assertEqual(ab.side_crossings(self.trace),
                         [{"guid": SLICER, "t": 6500, "label": "P1", "frm": 0, "to": 1, "group": 0}])
        noted = ab.spot_spans(self.trace)
        self.assertEqual([ab.midline_half(noted, (X, Y + dy), 1000) for dy in (6.0, 4.0, -4.0, -6.0)],
                         [0, None, None, 1])

    def test_each_side_has_its_own_focus_latch(self):
        self.assertEqual([side for side, _ in ab.focus_latches(self.trace)], [0, 1])
        rows = ab.focus_rows(self.trace)
        self.assertEqual([(row["side"], row["guid"], row["held"]) for row in rows], [(0, BU0, 9000), (1, BU1, 14000)])
        # side 0: Assasin 8 of 18, Hellflame 18 of 18; side 1: Mighty on him, Trueshot 28 of 28
        self.assertAlmostEqual(rows[0]["share"], 26 / 36)
        self.assertAlmostEqual(rows[1]["share"], 0.5)

    def test_each_focus_pick_is_labelled_by_side(self):
        self.assertEqual([(row["t"], row["side"], row["guid"], row["target"], row["settled"])
                          for row in ab.focus_picks(self.trace)],
                         [(1000, 0, BU0, SIDE0, True), (1000, 1, BU1, SIDE1, True)])

    def test_a_cast_off_both_focuses_is_stray(self):
        self.assertEqual([(rec["t"], rec["s"], rec["tgt"]) for rec in ab.stray_burrower_casts(self.trace)],
                         [(16000, GUNNER, BU1)])

    def test_each_kite_reads_its_plan_terminals(self):
        self.assertEqual([(window["guid"], window["plan"]) for window in ab.mark_windows(self.trace)],
                         [(ARCHER, ["patch+hand", "patch"]), (GUNNER, ["hand"]), (GUNNER, ["own"]),
                          (ARCHER, ["patch"])])

    def test_a_plan_carried_in_counts_only_without_a_row_near_the_mark(self):
        self.assertEqual(ab.plan_terminals([(900, "patch"), (1500, "hand")], 1000, 5000), ["patch", "hand"])
        self.assertEqual(ab.plan_terminals([(100, "patch")], 5000, 9000), ["patch"])
        self.assertEqual(ab.plan_terminals([(100, "hand"), (5300, "own")], 5000, 9000), ["own"])
        self.assertEqual(ab.plan_terminals([(100, "hand"), (7000, "patch")], 5000, 9000), ["hand", "patch"])
        self.assertEqual(ab.plan_terminals([], 0, 1000), [])

    def test_a_terminal_is_used_only_with_everything_it_spends(self):
        hand = {"hop": [{"t": 1}], "own": []}
        self.assertEqual(ab.plan_missing("patch+hand", hand, None), ["no tank patch spent"])
        self.assertEqual(ab.plan_missing("patch+hand", hand, {"side": 1}), [])
        self.assertEqual(ab.plan_missing("own", hand, None), ["no own immunity"])
        self.assertIsNone(ab.plan_missing("none", hand, None))

    def test_the_longest_stretch_held_off_the_anchor_with_no_drag_leg(self):
        self.assertEqual([(row["label"], row["held_off"]) for row in ab.threat_rows(self.trace)],
                         [("P1", (4000, 14000)), ("P1b", (0, None))])

    def test_the_dps_section_prints(self):
        text = self.text(ab.show_dps)
        self.assertIn("20.0 s  boss 100.0% -> 85.0%, 15.0% lost  under 20%\n", text)
        self.assertIn("walked 30 yd: anubarak melee burrower stand 28, no move row 2\n", text)
        self.assertIn("dealt 6.0k/s: boss 75%, burrower 25%, scarab 0%, other 0%, none 0%\n", text)
        self.assertIn("    ranged  2 bot(s): target boss 25%, burrower 75%, scarab 0%, other 0%, none 0%; moving 5%;"
                      " 2 yd a bot\n", text)
        self.assertIn("dealt 5.0k/s: boss 30%, burrower 70%, scarab 0%, other 0%, none 0%\n", text)
        self.assertIn("boss health lost per phase 1: P1 15.0% under, P1b 28.5%\n", text)
        self.assertIn("anub.dps rows per role: melee burrower 1, loose 1, boss 1; ranged burrower 2, boss 1\n", text)
        self.assertIn("    wave 1  0:01.000  side 0: melee Assasin; ranged Hellflame\n"
                      "                      side 1: melee Mighty; ranged Trueshot\n", text)
        self.assertIn("    wave 2  0:45.000  side 0: melee Assasin; ranged Trueshot\n"
                      "                      side 1: melee Mighty\n", text)
        self.assertIn("    P1     0:00.000  1 walk(s): Assasin 1 (1 astray)\n", text)
        self.assertIn("    P1b    0:40.000  0 walk(s)\n", text)

    def test_the_burrower_section_prints_each_side_and_no_focus_due(self):
        text = self.text(ab.show_burrowers)
        self.assertRegex(text, rf"#{BU0 & 0xFFFFFFFF} +9\.0s +72% +0\n")
        self.assertRegex(text, rf"#{BU1 & 0xFFFFFFFF} +14\.0s +50% +1\n")
        self.assertRegex(text, rf"0:01\.000 #{BU1 & 0xFFFFFFFF} +100% +0\.0 s Felesta .* +1\n")
        self.assertIn("no anub.focusdue rows, so no focus due per wave: only older traces write it\n", text)
        self.assertIn("anub.focusdue dropped with the focus alive: no anub.focusdue rows\n", text)
        self.assertIn("DPS casts at a burrower other than the focus: 1, by Trueshot 1; most: Steady Shot 49052 1\n",
                      text)
        self.assertIn("anub.dps: Assasin burrower 1, loose 1, boss 1;", text)

    def test_the_spike_section_prints_each_plan_and_its_use(self):
        text = self.text(ab.show_spike)
        self.assertIn("  tank patch 1 spent\n            kiteplan patch+hand>patch: used\n", text)
        self.assertIn("            kiteplan hand: used\n", text)
        self.assertIn("            kiteplan own: used\n", text)
        self.assertIn("            kiteplan patch: unused, no tank patch spent\n", text)
        self.assertIn("anub.kiteplan terminal at the mark's end, used before it ended: patch 1 of 2, hand 1 of 1,"
                      " own 1 of 1\n", text)
        self.assertIn("kiter dead with its terminal unused: Hellflame 0:38.000 patch\n", text)

    def test_the_threat_section_prints_the_stretch_held_off(self):
        text = self.text(ab.show_threat)
        self.assertIn("          held off the anchor with no drag leg: longest 4.0 s from 0:14.000\n", text)
        self.assertIn("          held off the anchor with no drag leg: never\n", text)
        self.assertIn("first submerge leg;\n  10 s or more: none\n", text)

    def test_the_new_keys_count_as_present(self):
        gone = ab.missing_probes(self.trace)
        for key in ("anub.dside", "anub.focus0", "anub.focus1", "anub.kiteplan"):
            self.assertNotIn(key, gone)
        self.assertNotIn("anub.focus", gone)
        self.assertNotIn("anub.focusdue", gone)

    def test_every_section_prints(self):
        text = self.text(ab.show_banner, *(section for _, _, section in ab.SECTIONS))
        self.assertIn("DPS\n  P1 ", text)


BEAR, SHIELD, DEATH, ARMS, RUNNER, WALKER, SAVIOR, ARCHER2 = range(9001, 9009)
BOSS3 = CREATURE | 1200
SPIKE3 = CREATURE | 1201
PATCH_N, PATCH_S = CREATURE | 1211, CREATURE | 1212
PATCH_N_AT, PATCH_S_AT = (X, Y + 20.0), (X, Y - 20.0)
BUR1, BUR2 = CREATURE | 1221, CREATURE | 1222
WOLF = CREATURE | 1231
IMPALE_25H = 67860
SPELL_CHARGE, SPELL_CHARGE_STUN, SPELL_SUNDER, SPELL_CLAW = 11578, 7922, 7386, 52476
SPELL_SWIPE, SPELL_ROAR, SPELL_MAUL = 48562, 5209, 48480
# (start, stop, kiter) per mark under him
TURN_MARKS = ((12000, 20000, RUNNER), (25000, 32000, WALKER), (35000, 42000, ARCHER2), (44000, 49000, ARMS))


def lerp(path: list[tuple[int, float, float]], when: int) -> tuple[float, float]:
    """The spot on a `(t, dx, dy)` path off the room centre at `when`, held before its first point and after its
    last."""
    if when <= path[0][0]:
        return X + path[0][1], Y + path[0][2]
    for (low, x0, y0), (high, x1, y1) in zip(path, path[1:]):
        if when <= high:
            share = (when - low) / (high - low)
            return X + x0 + (x1 - x0) * share, Y + y0 + (y1 - y0) * share
    return X + path[-1][1], Y + path[-1][2]


RUNNER_PATH = [(12000, 30.0, 22.0), (15000, 15.0, 5.0), (19000, 1.0, -19.0)]
WALKER_PATH = [(25000, 30.0, 20.0), (27000, 20.0, 20.0), (32000, 12.0, 20.0)]
# spawned 10 yd from the north patch, out past its clearance, back in at 14 s, then down to the south patch
SPIKE_RUN = [(12000, 10.0, 20.0), (12500, 16.0, 20.0), (13500, 15.0, 10.0), (14000, 10.0, 12.0), (15000, 6.0, 0.0),
             (20000, 2.0, -14.0)]


def turn_kiter(when: int) -> int:
    return next((kiter for start, stop, kiter in TURN_MARKS if start <= when <= stop), 0)


def turn_spike(when: int) -> tuple[float, float]:
    if when <= 20000:
        return lerp(SPIKE_RUN, when)
    if when < 35000:
        return X + 30.0, Y - 10.0
    if 46000 <= when <= 49000:
        return X + 10.0, Y + 30.0
    return X + 40.0, Y + 30.0 if when >= 44000 else Y


def bur1_target(when: int) -> int:
    """On Felesta from 1.5 s, off to Mighty at 4 s, back, onto the main tank at 6.5 s, back, to Trueshot's wolf at
    7.5 s, back, and to Mighty again at 17 s, past the 15 s after it came up."""
    if when < 1500:
        return RUNNER
    targets = {4000: ARMS, 6500: BEAR, 7500: WOLF}
    if when in targets:
        return targets[when]
    return ARMS if when >= 17000 else DEATH


def bur2_target(when: int) -> int:
    return BEAR if 5000 <= when <= 8000 or 64500 <= when <= 65500 else SHIELD


def turn_player(guid: int, when: int) -> list:
    spot = {BEAR: (X, Y), SHIELD: (X - 5.0, Y + 25.0), DEATH: (X - 5.0, Y - 25.0), ARMS: (X + 5.0, Y - 30.0),
            SAVIOR: (X - 10.0, Y), ARCHER2: (X - 30.0, Y)}.get(guid)
    if guid == RUNNER:
        spot = lerp(RUNNER_PATH, when)
    elif guid == WALKER:
        spot = lerp(WALKER_PATH, when)
    hp = 0.0 if guid == WALKER and when >= 32000 else 100.0
    up = when < 10000 or when >= 50000
    return [guid, *spot, Z, 0.0, hp, 100.0, BOSS3 if up and guid == BEAR else 0, 0, 0, 0, 0]


def turn_snap(when: int) -> dict:
    units = [turn_player(guid, when) for guid in (BEAR, SHIELD, DEATH, ARMS, RUNNER, WALKER, SAVIOR, ARCHER2)]
    up = when < 10000 or when >= 50000
    units.append([BOSS3, X, Y + 1.0, Z, 0.0, 90.0, 0.0, BEAR if up else 0, 0, 0, 0, 0])
    units.append([WOLF, X - 28.0, Y, Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
    if 10000 <= when < 50000:
        units.append([SPIKE3, *turn_spike(when), Z, 0.0, 100.0, 0.0, turn_kiter(when), 1, 0, 0, 0])
    hz = []
    for guid, spot, last in ((PATCH_N, PATCH_N_AT, 70000), (PATCH_S, PATCH_S_AT, 20000)):
        if 0 <= when <= last:
            units.append([guid, *spot, PATCH_Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
            hz.append([PERMAFROST_25H, *spot, PATCH_Z, 6.0, 1])
    if 1000 <= when <= 18000:
        units.append([BUR1, X - 5.0, Y - 23.0, Z, 0.0, 100.0 - (when - 1000) / 500.0, 0.0, bur1_target(when),
                      0, 0, 0, 0])
    if when >= 1000:
        units.append([BUR2, X - 5.0, Y + 23.0, Z, 0.0, 100.0 - (when - 1000) / 2000.0, 0.0, bur2_target(when),
                      0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def turn_pull(with_turn_notes: bool = True) -> list[dict]:
    """A 25H pull: up to 10 s, under to 50 s, up again to 70 s. Four kites under him: a patch+hand bridge with
    anub.bridge and anub.kitemove rows that the spike fails on, a patch kite that dies short of its patch while
    the spike impales a side tank, a patch+hand kite whose Hand lands with its immune stage, and a patch+hand kite
    that never turns. While he is up a burrower is pulled off its side tank by a melee and by a pet, and the main
    tank takes Expose Weakness twice. `with_turn_notes` False drops the anub.bridge and anub.kitemove rows."""
    records = [
        {"e": "hdr", "v": 14, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 15, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": BEAR, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 0},
             {"g": SHIELD, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
             {"g": DEATH, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
             {"g": ARMS, "n": "Mighty", "r": "melee", "c": 1, "h": 0},
             {"g": RUNNER, "n": "Prayer", "r": "heal", "c": 5, "h": 0},
             {"g": WALKER, "n": "Druidica", "r": "ranged", "c": 11, "h": 0},
             {"g": SAVIOR, "n": "Justice", "r": "melee", "c": 2, "h": 0},
             {"g": ARCHER2, "n": "Trueshot", "r": "ranged", "c": 3, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": BOSS3, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 10000, "e": "unit", "g": SPIKE3, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"}
          for guid in (PATCH_N, PATCH_S)),
        *({"t": 1000, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"}
          for guid in (BUR1, BUR2)),
        {"t": -500, "e": "unit", "g": WOLF, "n": "Wolf", "own": ARCHER2},
        *({"e": "spell", "sp": spell, "n": name} for spell, name in (
            (SPELL_CHARGE, "Charge"), (SPELL_SUNDER, "Sunder Armor"), (SPELL_CLAW, "Claw"),
            (SPELL_SWIPE, "Swipe (Bear)"), (SPELL_ROAR, "Challenging Roar"), (SPELL_MAUL, "Maul"))),
        *(note(when, BEAR, "anub.phase", value) for when, value in ((0, 1), (10000, 2), (50000, 1))),
        note(0, BEAR, "anub.side0", SHIELD),
        note(0, BEAR, "anub.side1", DEATH),
        note(0, SHIELD, "anub.patch0", PATCH_N),
        note(0, DEATH, "anub.patch1", PATCH_S),
        note(20200, DEATH, "anub.patch1", 0),
        note(1000, BEAR, "anub.wave", 1),

        # Mighty's opener on BUR1 is older than 3 s by the pull at 4 s, and the stun is triggered
        cast(500, ARMS, SPELL_MAUL, BUR1),
        cast(1500, ARMS, SPELL_CHARGE, BUR1),
        triggered(2000, ARMS, SPELL_CHARGE_STUN, BUR1),
        cast(3000, ARMS, SPELL_SUNDER, BUR2),
        cast(3500, ARMS, SPELL_SUNDER, BUR1),
        cast(7200, WOLF, SPELL_CLAW, BUR1),

        # the main tank: a Swipe too early, one in, the Roar, a triggered Swipe and one past the peak
        cast(500, BEAR, SPELL_SWIPE, 0),
        cast(2000, BEAR, SPELL_SWIPE, 0),
        cast(6200, BEAR, SPELL_ROAR, 0),
        cast(6300, BEAR, SPELL_MAUL, BOSS3),
        triggered(6400, BEAR, SPELL_SWIPE, 0),
        cast(7000, BEAR, SPELL_SWIPE, 0),
        {"t": 6000, "e": "aura", "d": BEAR, "s": BUR2, "sp": 67847, "r": 0, "st": 1},
        {"t": 6500, "e": "aura", "d": BEAR, "s": BUR2, "sp": 67847, "r": 0, "st": 2},
        {"t": 9000, "e": "aura", "d": BEAR, "s": BUR2, "sp": 67847, "r": 1},
        {"t": 65000, "e": "aura", "d": BEAR, "s": BUR2, "sp": 67847, "r": 0, "st": 1},

        *(aura(when, kiter, ab.SPELL_MARK, removed, BOSS3) for start, stop, kiter in TURN_MARKS
          for when, removed in ((start, False), (stop, True))),
        note(12100, RUNNER, "anub.kiteplan", "patch+hand"),
        note(25100, WALKER, "anub.kiteplan", "patch"),
        note(35100, ARCHER2, "anub.kiteplan", "patch+hand"),
        note(44100, ARMS, "anub.kiteplan", "patch+hand"),
        *(note(when, guid, "anub.kitestage", stage) for when, guid, stage in (
            (12000, RUNNER, "run"), (15400, RUNNER, "immune"), (18000, RUNNER, "patch"), (20000, RUNNER, "none"),
            (25000, WALKER, "run"), (27000, WALKER, "patch"), (32000, WALKER, "none"),
            (35000, ARCHER2, "run"), (37100, ARCHER2, "immune"), (42000, ARCHER2, "none"),
            (44000, ARMS, "run"), (49000, ARMS, "none"))),
        cast(15300, SAVIOR, 10278, RUNNER),
        cast(37000, SAVIOR, 10278, ARCHER2),
        dmg(28000, SPIKE3, SHIELD, IMPALE_25H, 10000),
        dmg(28500, SPIKE3, SHIELD, IMPALE_25H, 12000),
        dmg(31900, SPIKE3, WALKER, IMPALE_25H, 30000),
        {"t": 32000, "e": "death", "g": WALKER, "killer": SPIKE3, "x": X + 12.0, "y": Y + 20.0, "z": Z,
         "blow": [SPIKE3, 30000]},
        {"t": 70000, "e": "end", "out": "wipe"},
    ]
    if with_turn_notes:
        records += [note(when, RUNNER, key, value) for when, key, value in (
            (12100, "anub.bridge", "none"), (15000, "anub.bridge", "ask"), (15400, "anub.bridge", "hand"),
            (20000, "anub.bridge", "none"), (12100, "anub.kitemove", "edge0"), (17000, "anub.kitemove", "in"))]
    records += [turn_snap(when) for when in range(-500, 70000, 500)]
    return records


class TurnPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_15_anub-arak_1789500020.ndjson", turn_pull())
        cls.bare = write_trace(cls.folder.name, "649_15_anub-arak_1789500021.ndjson", turn_pull(False))
        cls.windows = ab.mark_windows(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, trace, *sections) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            for section in sections:
                section(trace)
        return out.getvalue()

    def test_the_turn_is_the_first_bridge_row_else_the_first_walk_stage(self):
        stages = [("run", 1000, 3000), ("immune", 3000, 5000), ("patch", 5000, 9000)]
        self.assertEqual(ab.kite_turn([(2000, "none"), (2500, "ask")], stages, 1000, 9000), (2500, "anub.bridge"))
        self.assertEqual(ab.kite_turn([(500, "turn")], stages, 1000, 9000), (5000, "kitestage"))
        self.assertEqual(ab.kite_turn([], stages, 1000, 9000, ("patch", "immune")), (3000, "kitestage"))
        self.assertEqual(ab.kite_turn([], [("patch", 0, 9000)], 1000, 9000), (1000, "kitestage"))
        self.assertEqual(ab.kite_turn([], stages[:1], 1000, 9000), (None, None))

    def test_the_kitemove_at_the_turn_is_carried_in_then_each_change(self):
        rows = [(100, "edge1"), (5100, "in"), (6000, "in"), (7000, "edge0")]
        self.assertEqual(ab.moves_from(rows, 5000, 9000), ["in", "edge0"])
        self.assertEqual(ab.moves_from(rows, 4000, 6500), ["edge1", "in"])
        self.assertEqual(ab.moves_from([], 4000, 6500), [])

    def test_a_patch_hand_turn_reads_the_bridge_else_the_hand_against_the_turn(self):
        def window(bridge, turn, hands, plan=("patch+hand",)):
            return {"plan": list(plan), "bridge": bridge, "turn": turn, "hop": [{"t": when} for when in hands]}

        self.assertEqual(ab.bridge_verdict(window(["ask", "hand"], 1000, [])), "hand")
        self.assertEqual(ab.bridge_verdict(window(["ask", "turn"], 1000, [1200])), "turn")
        self.assertEqual(ab.bridge_verdict(window(["ask"], 1000, [])), "ask")
        self.assertEqual(ab.bridge_verdict(window([], None, [500])), "never")
        self.assertEqual(ab.bridge_verdict(window([], 5000, [3600])), "hand")
        self.assertEqual(ab.bridge_verdict(window([], 5000, [3000])), "early")
        self.assertEqual(ab.bridge_verdict(window([], 5000, [7000])), "late")
        self.assertEqual(ab.bridge_verdict(window([], 5000, [])), "turn")
        self.assertIsNone(ab.bridge_verdict(window([], 5000, [], ("hand",))))

    def test_each_patch_kite_reads_its_turn_its_move_and_the_patch_it_went_for(self):
        rows = [(window["guid"], window["patch_kite"], window["turn"], window["turn_by"], window["moves"],
                 window["bridge"]) for window in self.windows]
        self.assertEqual(rows, [(RUNNER, True, 15000, "anub.bridge", ["edge0", "in"], ["ask", "hand"]),
                                (WALKER, True, 27000, "kitestage", [], []),
                                (ARCHER2, True, 37100, "kitestage", [], []),
                                (ARMS, True, None, None, [], [])])
        bridge, walk, hand, _ = (window["turned_for"] for window in self.windows)
        self.assertEqual((bridge["guid"], bridge["side"], bridge["failed"]), (PATCH_S, 1, True))
        self.assertAlmostEqual(bridge["kiter"], math.sqrt(2.0))
        self.assertAlmostEqual(bridge["spike"], math.hypot(2.0, 6.0))
        # the south patch is gone by then, so the north one is the only candidate
        self.assertEqual((walk["guid"], walk["side"], walk["failed"]), (PATCH_N, 0, False))
        self.assertAlmostEqual(walk["kiter"], 12.0)
        self.assertAlmostEqual(hand["kiter"], math.hypot(30.0, 20.0))

    def test_the_first_pass_before_the_turn_leaves_out_a_patch_the_spike_spawned_inside(self):
        first, walk, hand, never = (window["first_pass"] for window in self.windows)
        self.assertEqual((first["t"], first["guid"], first["side"]), (14000, PATCH_N, 0))
        self.assertAlmostEqual(first["yards"], math.hypot(10.0, 8.0))
        self.assertIsNone(walk)
        self.assertIsNone(hand)
        self.assertEqual((never["t"], never["guid"]), (46000, PATCH_N))

    def test_impale_on_a_side_tank_lands_in_its_window_with_the_side(self):
        self.assertEqual([[(rec["t"], rec["d"], side) for rec, side in window["side_impales"]]
                          for window in self.windows], [[], [(28000, SHIELD, 0), (28500, SHIELD, 0)], [], []])

    def test_the_spike_section_prints_each_turn_and_its_summary(self):
        text = self.text(self.trace, ab.show_spike)
        south = f"side 1 patch #{PATCH_S & 0xFFFFFFFF} ({X:.1f},{Y - 20.0:.1f})"
        north = f"side 0 patch #{PATCH_N & 0xFFFFFFFF} ({X:.1f},{Y + 20.0:.1f})"
        self.assertIn(f"            turned +3.0 s (anub.bridge), kitemove edge0>in; {south}, closest after the turn:"
                      " kiter 1.4 yd, spike 6.3 yd; the spike failed on it\n"
                      "            patch+hand: Hand at the turn, read off anub.bridge; Hand of Protection by Justice"
                      " +0.3 s from the turn\n"
                      f"            spike within 14.5 yd of {north}: 12.8 yd +2.0 s, before the turn\n", text)
        self.assertIn(f"            turned +2.0 s (kitestage), kitemove -; {north}, closest after the turn: kiter"
                      " 12.0 yd, spike 42.4 yd; unspent, death\n"
                      "            Impale on side 0 tank Bulwark: 2 hit(s) for 22,000 from +3.0 s\n", text)
        self.assertIn("            patch+hand: Hand at the turn, read off Hand of Protection casts; Hand of Protection"
                      " by Justice -0.1 s from the turn\n", text)
        self.assertIn("            never turned for a patch\n"
                      f"            spike within 14.5 yd of {north}: 14.1 yd +2.0 s, never turned\n", text)
        self.assertIn("  patch kites turned for a patch: 3 of 4, the spike failed on it in 1, the kiter inside its 7.5"
                      " yd slow after the turn in 1; anub.kitemove at the turn: - 2, edge0 1\n", text)
        self.assertIn("  patch+hand kites at the turn: Hand at the turn 2; never turned 1: Mighty 0:44.000\n", text)
        self.assertIn("  spike within 14.5 yd of a patch, its route clearance, before the turn or in a kite with none:"
                      " Prayer 0:12.000 +2.0 s, Mighty 0:44.000 +2.0 s\n", text)
        self.assertIn("  Impale on a side tank: Bulwark (side 0) 2 in Druidica's kite 0:25.000\n", text)

    def test_without_the_turn_notes_the_spike_section_falls_back_on_the_stages_and_the_casts(self):
        bridge = ab.mark_windows(self.bare)[0]
        self.assertEqual((bridge["turn"], bridge["turn_by"], bridge["moves"], bridge["bridge"]),
                         (15400, "kitestage", [], []))
        self.assertEqual(ab.bridge_verdict(bridge), "hand")
        text = self.text(self.bare, ab.show_spike)
        self.assertIn("anub.kitemove at the turn: - 3, no anub.kitemove rows\n", text)
        self.assertIn("  patch+hand kites at the turn: Hand at the turn 2; never turned 1: Mighty 0:44.000; no"
                      " anub.bridge rows, so read off Hand of Protection casts\n", text)

    def test_kitemove_counts_as_present_and_absent_without_its_rows_and_the_retired_bridge_never_missing(self):
        self.assertNotIn("anub.kitemove", ab.missing_probes(self.trace))
        gone = ab.missing_probes(self.bare)
        self.assertIn("anub.kitemove", gone)
        self.assertNotIn("anub.bridge", gone)

    def test_a_burrower_pulled_off_its_side_tank_reads_the_puller_and_its_casts(self):
        rows = ab.burrower_pulls(self.trace)
        self.assertEqual([(row["guid"], row["t"], row["tank"], row["side"], row["on"], row["puller"], row["up"],
                           row["wave"], row["emerged"]) for row in rows],
                         [(BUR1, 4000, DEATH, 1, 1500, ARMS, 1000, 1, False),
                          (BUR1, 7500, DEATH, 1, 7000, WOLF, 1000, 1, False)])
        self.assertEqual([[(rec["t"], rec["sp"]) for rec in row["casts"]] for row in rows],
                         [[(1500, SPELL_CHARGE), (2000, SPELL_CHARGE_STUN), (3500, SPELL_SUNDER)],
                          [(7200, SPELL_CLAW)]])

    def test_the_burrower_section_prints_each_pull(self):
        text = self.text(self.trace, ab.show_burrowers)
        self.assertIn("triggered ones marked *:\n", text)
        self.assertIn(f"    #{BUR1 & 0xFFFFFFFF}      wave 1 sight 0:01.000: Felesta (side 1) > Mighty (melee) +3.0 s,"
                      " 2.5 s on the side tank; casts on it: Charge 11578 +0.5 s, spell 7922* +1.0 s, Sunder Armor 7386"
                      " +2.5 s\n", text)
        self.assertIn(f"    #{BUR1 & 0xFFFFFFFF}      wave 1 sight 0:01.000: Felesta (side 1) > Wolf (Trueshot)"
                      " (ranged) +6.5 s, 0.5 s on the side tank; casts on it: Claw 52476 +6.2 s\n", text)
        self.assertIn("  pulls per puller: Mighty 1, Wolf (Trueshot) 1\n", text)

    def test_the_main_tanks_expose_weakness_reads_its_burrowers_and_his_area_casts(self):
        rows = ab.main_tank_exposes(self.trace)
        self.assertEqual([(row["start"], row["peak"], row["at"], row["off"], row["by"]) for row in rows],
                         [(6000, 2, 6500, 9000, [BUR2, BUR1]), (65000, 1, 65000, None, [BUR2])])
        self.assertEqual([[(rec["t"], rec["sp"]) for rec in row["area"]] for row in rows],
                         [[(2000, SPELL_SWIPE), (6200, SPELL_ROAR)], []])

    def test_the_tank_section_prints_the_main_tanks_expose_weakness(self):
        text = self.text(self.trace, ab.show_tanks)
        self.assertIn("  the main tank Ecoterrorist's Expose Weakness per run of stacks: its peak, the burrowers on him"
                      " at a stack,\n  and his area casts from 5 s before the first stack to the peak, triggered ones"
                      " left out:\n"
                      f"     0:06.000  peak 2 at 0:06.500, off 0:09.000; by #{BUR2 & 0xFFFFFFFF}, #{BUR1 & 0xFFFFFFFF};"
                      " area casts: Swipe (Bear) 48562 -4.0 s, Challenging Roar 5209 +0.2 s\n"
                      f"     1:05.000  peak 1 at 1:05.000, up at the end; by #{BUR2 & 0xFFFFFFFF}; area casts: none\n",
                      text)

    def test_every_section_prints(self):
        text = self.text(self.trace, ab.show_banner, *(section for _, _, section in ab.SECTIONS))
        self.assertIn("TANKS\n", text)


RECO, RBUL, RFELESTA, ROBLIT, RPRAY, RAGONY, RJUSTICE, RMIGHTY, RLOCK = range(9101, 9110)
RBOSS = CREATURE | 1300
RSPIKE = CREATURE | 1301
RPATCH_N, RPATCH_S, RSPHERE = CREATURE | 1311, CREATURE | 1312, CREATURE | 1313
RBUR = CREATURE | 1321
RSPOT1 = (X, Y - 20.0)
RPATCH_S_AT = (X + 42.0, Y - 2.0)
# where the far grip's sphere flies, then where each link brings it down
RSPHERE_FLY, RLINK1_AT, RLINK2_AT = (X + 60.0, Y - 30.0), (X + 40.0, Y - 25.0), (X + 18.0, Y - 22.0)
SPIKE_RUN_YD_S = 3.5 * 3.25


def relay_up(when: int) -> bool:
    return when < 10000 or when >= 50000


def relay_spot(guid: int, when: int) -> tuple[float, float]:
    if guid == RECO:
        return lerp([(2000, 0.0, 0.0), (4000, 6.0, 8.0)], when)
    if guid == RPRAY:
        return lerp([(24000, 0.0, 0.0), (30000, 42.0, 0.0)], when)
    if guid == RMIGHTY:
        return (X - 5.0, Y + 10.0) if when < 30000 else (X + 30.0, Y + 23.0)
    if guid == ROBLIT:
        return RLINK1_AT if when >= 50000 else (X + 10.0, Y - 10.0)
    if guid == RFELESTA:
        return RLINK2_AT if when >= 50000 else (X - 5.0, Y - 25.0)
    return {RBUL: (X - 20.0, Y + 25.0), RAGONY: (X + 30.0, Y + 20.0), RJUSTICE: (X + 5.0, Y - 5.0),
            RLOCK: (X - 30.0, Y)}[guid]


def relay_spike(when: int) -> tuple[tuple[float, float], int]:
    """Still on Agony, then 12 yd behind Prayer at the Hand and closing at its last speed step, then idle after
    the fail."""
    if when < 20000:
        return (X + 30.0, Y - 10.0), RAGONY
    if when < 25000:
        return (X - 20.0, Y), RPRAY
    if when <= 27000:
        return (X + 2.0 + SPIKE_RUN_YD_S * (when - 26000) / 1000.0, Y), RPRAY
    if when <= 30000:
        return (relay_spot(RPRAY, when)[0] - 7.625, Y), RPRAY
    return (X + 40.0, Y + 30.0), 0


def relay_sphere(when: int) -> tuple[tuple[float, float], float] | None:
    if when < 52500:
        return RSPHERE_FLY, FLYING_Z
    if when < 54500:
        return RLINK1_AT, FALLING_Z
    return RLINK2_AT, PATCH_Z if when >= 56000 else FALLING_Z


def relay_snap(when: int) -> dict:
    units = []
    for guid in (RECO, RBUL, RFELESTA, ROBLIT, RPRAY, RAGONY, RJUSTICE, RMIGHTY, RLOCK):
        hp = 0.0 if guid == RAGONY and when >= 19900 else 100.0
        target = RBOSS if guid == RECO and relay_up(when) else 0
        units.append([guid, *relay_spot(guid, when), Z, 0.0, hp, 100.0, target, 0, 0, 0, 0])
    units.append([RBOSS, X + 5.0, Y + 1.0, Z, 0.0, 90.0, 0.0, RECO if relay_up(when) else 0, 0, 0, 0, 0])
    if 10000 <= when < 50000:
        spot, target = relay_spike(when)
        units.append([RSPIKE, *spot, Z, 0.0, 100.0, 0.0, target, 1, 0, 0, 0])
    hz = []
    for guid, spot, last in ((RPATCH_N, (X, Y + 20.0), 70000), (RPATCH_S, RPATCH_S_AT, 30800)):
        if 0 <= when <= last:
            units.append([guid, *spot, PATCH_Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
            hz.append([PERMAFROST_25H, *spot, PATCH_Z, 6.0, 1])
    if when >= 0:
        spot, z = relay_sphere(when)
        units.append([RSPHERE, *spot, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_25H, *spot, PATCH_Z, 6.0, 1])
    if 1000 <= when <= 69000:
        units.append([RBUR, X - 5.0, Y + 23.0, Z, 0.0, 100.0 - (when - 1000) / 2000.0, 0.0, RBUL, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def relay_pull(new_rows: bool = True) -> list[dict]:
    """A 25H pull: up to 10 s, under to 50 s, up again to 70 s. Under him a patch+hand kite turns without a Hand
    and dies, and the next one takes its Hand 12 yd off a spike at its last speed step and spends its patch under
    it. Back up, a far grip at a sphere is gripped again by a relay link nearer the spot. Before the submerge the
    main tank pre-steps twice and the submerge spot search writes three rows. Mighty Bladestorms near a burrower
    and far from it. `new_rows` False drops anub.relay, anub.subspot, the prestep rows and the burrower vetoes."""
    records = [
        {"e": "hdr", "v": 14, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 16, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": RECO, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 0},
             {"g": RBUL, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
             {"g": RFELESTA, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
             {"g": ROBLIT, "n": "Obliteration", "r": "melee", "c": 6, "h": 0},
             {"g": RPRAY, "n": "Prayer", "r": "heal", "c": 5, "h": 0},
             {"g": RAGONY, "n": "Agony", "r": "ranged", "c": 9, "h": 0},
             {"g": RJUSTICE, "n": "Justice", "r": "melee", "c": 2, "h": 0},
             {"g": RMIGHTY, "n": "Mighty", "r": "melee", "c": 1, "h": 0},
             {"g": RLOCK, "n": "Fel", "r": "ranged", "c": 9, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": RBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 10000, "e": "unit", "g": RSPIKE, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"}
          for guid in (RPATCH_N, RPATCH_S, RSPHERE)),
        {"t": 1000, "e": "unit", "g": RBUR, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        *(note(when, RECO, "anub.phase", value) for when, value in ((0, 1), (10000, 2), (50000, 1))),
        note(0, RECO, "anub.side0", RBUL),
        note(0, RECO, "anub.side1", RFELESTA),
        note(0, RBUL, "anub.patch0", RPATCH_N),
        note(0, RFELESTA, "anub.patch1", RPATCH_S),
        note(31500, RFELESTA, "anub.patch1", 0),
        note(56000, RFELESTA, "anub.patch1", RSPHERE),
        note(0, RECO, "anub.spot0", f"{X},{Y + 20.0},{Z}"),
        note(0, RECO, "anub.spot1", f"{RSPOT1[0]},{RSPOT1[1]},{Z}"),

        # Agony turns without a Hand and dies, handing the spike on at its speed; Prayer's Hand lands and its
        # patch walk comes under it
        *(aura(when, kiter, ab.SPELL_MARK, removed, RBOSS)
          for kiter, start, stop in ((RAGONY, 10500, 19900), (RPRAY, 20000, 30000))
          for when, removed in ((start, False), (stop, True))),
        {"t": 19900, "e": "death", "g": RAGONY, "killer": RSPIKE, "x": X + 30.0, "y": Y + 20.0, "z": Z,
         "blow": [RSPIKE, 30000]},
        note(10600, RAGONY, "anub.kiteplan", "patch+hand"),
        note(20100, RPRAY, "anub.kiteplan", "patch+hand"),
        *(note(when, guid, "anub.kitestage", stage) for when, guid, stage in (
            (10500, RAGONY, "run"), (15000, RAGONY, "patch"), (19900, RAGONY, "none"),
            (20000, RPRAY, "run"), (26000, RPRAY, "immune"), (28000, RPRAY, "patch"), (30000, RPRAY, "none"))),
        *(note(when, guid, "anub.bridge", value) for when, guid, value in (
            (10600, RAGONY, "none"), (15000, RAGONY, "turn"), (19900, RAGONY, "none"),
            (20100, RPRAY, "none"), (26000, RPRAY, "hand"), (30000, RPRAY, "none"))),
        cast(26000, RJUSTICE, 10278, RPRAY),
        aura(26000, RPRAY, 10278, False, RJUSTICE),
        aura(36000, RPRAY, 10278, True, RJUSTICE),

        # link 1 grips from far off, link 2 from its post nearer the spot once link 1's grip has landed
        *(note(when, guid, "anub.sphere", value) for when, guid, value in (
            (50000, ROBLIT, "grip1"), (56000, ROBLIT, "none"), (50000, RFELESTA, "grip1"), (56000, RFELESTA, "none"))),
        *(note(when, guid, "anub.gripstep", value) for when, guid, value in (
            (50000, ROBLIT, "walk"), (52000, ROBLIT, "cast"), (52600, ROBLIT, "wait"), (54100, ROBLIT, "none"),
            (50000, RFELESTA, "post"), (54000, RFELESTA, "cast"), (54600, RFELESTA, "kill"),
            (56000, RFELESTA, "none"))),
        *grip(52000, ROBLIT, RSPHERE),
        *grip(54000, RFELESTA, RSPHERE),

        *(note(when, RECO, "anub.pickup", value) for when, value in (
            (500, "hold"), (4000, "drag"), (6000, "hold"), (50500, "hold"))),
        cast(5000, RMIGHTY, ab.SPELL_BLADESTORM, 0),
        cast(60000, RMIGHTY, ab.SPELL_BLADESTORM, 0),
        veto(6500, RMIGHTY, ab.BURROWER_GUARD, "heroic strike"),
        {"t": 70000, "e": "end", "out": "wipe"},
    ]
    if new_rows:
        records += [
            note(2000, RECO, "anub.pickup", "prestep"),
            note(8000, RECO, "anub.pickup", "prestep"),
            *(note(when, RECO, "anub.subspot", value) for when, value in (
                (6000, "750,135,143 r25"), (7000, "none"), (9000, "750,139,143 r22"))),
            *(note(when, RFELESTA, "anub.relay", value) for when, value in ((51000, "wait"), (54000, "cast"),
                                                                            (56000, "none"), (58600, "miss"))),
            # a later grip that missed its hit roll: the cast with no triggered pull
            *grip(58000, RFELESTA, CREATURE | 1399, pulls=False),
            veto(5500, RMIGHTY, ab.BURROWER_GUARD, "bladestorm"),
            veto(7000, RLOCK, ab.BURROWER_THREAT_CAP, "shadow bolt"),
            veto(8000, RLOCK, ab.BURROWER_THREAT_CAP, "shadow bolt"),
            veto(9000, RMIGHTY, ab.BURROWER_THREAT_CAP, "cleave"),
        ]
    records += [relay_snap(when) for when in range(-500, 70000, 500)]
    return records


class RelayPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_16_anub-arak_1789500030.ndjson", relay_pull())
        cls.bare = write_trace(cls.folder.name, "649_16_anub-arak_1789500031.ndjson", relay_pull(False))
        cls.windows = ab.mark_windows(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, trace, *sections) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            for section in sections:
                section(trace)
        return out.getvalue()

    def test_the_catch_is_pure_pursuit_stepping_its_speed_after_a_kiter_running_on(self):
        self.assertEqual(ab.predicted_catch((X, Y), 15.5, (X + 5.0, Y), (1.0, 0.0), 7.0), 0.0)
        # 12 yd off at its last step, closing 4.375 yd/s, inside 7.5 yd on the eleventh tick
        self.assertAlmostEqual(ab.predicted_catch((X + 2.0, Y), 15.5, (X + 14.0, Y), (1.0, 0.0), 7.0), 1.1)
        # at its base speed it only steps up to the kiter's at 14 s, so it closes much later
        self.assertGreater(ab.predicted_catch((X + 2.0, Y), 0.0, (X + 14.0, Y), (1.0, 0.0), 7.0), 14.0)
        with unittest.mock.patch.object(ab, "KITE_RING_RADIUS", 1000.0):
            self.assertIsNone(ab.predicted_catch((X - 10.0, Y), 15.5, (X, Y), (1.0, 0.0), 12.0))

    def test_the_spike_restarts_at_a_submerges_first_mark_or_after_a_gap(self):
        marks = [(1, 1000, 5000, True), (2, 5100, 9000, True), (3, 13000, 15000, True)]
        self.assertEqual(ab.spike_restart(marks, [(0, 20000)], 6000), 1000)
        self.assertEqual(ab.spike_restart(marks, [(0, 20000)], 14000), 13000)
        self.assertIsNone(ab.spike_restart(marks, [(0, 20000)], 25000))

    def test_a_hand_row_with_no_ask_turns_at_the_patch_stage_after_it(self):
        stages = [("run", 1000, 2000), ("immune", 2000, 4000), ("patch", 4000, 9000)]
        self.assertEqual(ab.kite_turn([(2000, "hand")], stages, 1000, 9000, ("patch", "immune")), (4000, "kitestage"))
        self.assertEqual(ab.kite_turn([(2000, "hand")], stages[:2], 1000, 9000, ("patch", "immune")), (None, None))
        window = {"plan": ["patch+hand"], "bridge": ["hand"], "turn": 4000, "hop": []}
        self.assertEqual(ab.bridge_verdict(window), "catch")

    def test_each_hand_reads_against_its_predicted_catch_and_the_patch_under_it(self):
        agony, prayer = self.windows
        self.assertEqual((agony["catch"], prayer["turn"], prayer["turn_by"]), ([], 28000, "kitestage"))
        row, = prayer["catch"]
        self.assertAlmostEqual(row["before"], 1.1)
        self.assertAlmostEqual(row["yards"], 12.0)
        self.assertAlmostEqual(row["spike"], SPIKE_RUN_YD_S)
        self.assertEqual((row["end"], row["under"]), (36000, True))
        self.assertEqual((agony["turn_escort"]["guid"], prayer["turn_escort"]), (RJUSTICE, None))
        self.assertAlmostEqual(agony["turn_escort"]["yards"], math.hypot(25.0, 25.0))

    def test_the_spike_section_prints_the_hand_the_fallback_turn_and_the_first_patch_per_submerge(self):
        text = self.text(self.trace, ab.show_spike)
        self.assertIn("            patch+hand: turned without a Hand, read off anub.bridge\n", text)
        self.assertIn("            no Hand at the turn, nearest Hand of Protection-ready paladin then: Justice 35.4 yd,"
                      " past its 30 yd reach\n", text)
        self.assertIn("            patch+hand: Hand at the catch, the turn under it, read off anub.bridge; Hand of"
                      " Protection by Justice -2.0 s from the turn\n", text)
        self.assertIn("            Hand against the catch, by Justice +6.0 s: 1.1 s before the predicted catch, spike"
                      " 12.0 yd off at 11.4 yd/s; the patch followed under it, 6.0 s of it left\n", text)
        self.assertIn("  patch+hand kites' Hand against the catch predicted as it went out: Prayer 0:20.000 1.1 s"
                      " before, patch under it\n", text)
        # the latch clears at 31.5 s, after the patch's last sample at 30.8 s
        self.assertIn("  first tank patch gone per submerge, seconds into phase 2 at its anub.patchN clear, its side,"
                      " kiter and kiteplan:\n    P2 21.5 side 1 Prayer patch+hand\n", text)

    def test_each_far_grip_reads_every_link_against_its_sides_spot(self):
        chain, = ab.far_grips(self.trace)
        self.assertEqual((chain["sphere"], chain["side"]), (RSPHERE, 1))
        self.assertEqual([(link["dk"], link["t"], link["landed"]) for link in chain["links"]],
                         [(ROBLIT, 52000, 500), (RFELESTA, 54000, 500)])
        first, second = chain["links"]
        self.assertAlmostEqual(first["before"], math.hypot(60.0, 10.0))
        self.assertAlmostEqual(first["after"], second["before"])
        self.assertAlmostEqual(first["after"], math.hypot(40.0, 5.0))
        self.assertAlmostEqual(second["after"], math.hypot(18.0, 2.0))
        self.assertAlmostEqual(chain["death"]["off_spot"], math.hypot(18.0, 2.0))

    def test_the_sphere_section_prints_each_link_and_the_relay_rows(self):
        text = self.text(self.trace, ab.show_spheres)
        self.assertIn(f"    #{RSPHERE & 0xFFFFFFFF} side 1, died 0:54.500, 18.1 yd off its spot\n"
                      f"      link 1 {'0:52.000':>9} {'Obliteration':22} {'landed +0.5 s':24} 60.8 > 40.3 yd\n"
                      f"      link 2 {'0:54.000':>9} {'Felesta':22} {'landed +0.5 s':24} 40.3 > 18.1 yd\n", text)
        self.assertIn("  anub.relay: 0:51.000 Felesta wait, 0:54.000 Felesta cast, 0:58.600 Felesta miss\n", text)
        self.assertIn("Felesta post 1", text)
        self.assertIn("  anub.relay: no rows\n", self.text(self.bare, ab.show_spheres))

    def test_a_grip_triggering_no_pull_counts_as_a_miss_per_caster(self):
        self.assertEqual(ab.grip_misses(self.trace), {ROBLIT: (1, 0), RFELESTA: (2, 1)})
        self.assertEqual(ab.grip_misses(self.bare), {ROBLIT: (1, 0), RFELESTA: (1, 0)})
        self.assertTrue(all(row["pulled"] for row in ab.grip_rows(self.trace)))
        self.assertIn("with no pull, its 49576 triggering no 49560: Felesta 1 of 2, Obliteration 0 of 1\n",
                      self.text(self.trace, ab.show_spheres))

    def test_each_bladestorm_reads_the_nearest_burrower(self):
        rows = ab.bladestorm_casts(self.trace)
        self.assertEqual([(row["t"], row["guid"], row["burrower"]) for row in rows],
                         [(5000, RMIGHTY, RBUR), (60000, RMIGHTY, RBUR)])
        self.assertEqual([round(row["yards"], 1) for row in rows], [13.0, 35.0])
        capped, held = ab.burrower_vetoes(self.trace)
        self.assertEqual((dict(capped), dict(held)), ({RLOCK: 2, RMIGHTY: 1}, {RMIGHTY: 1}))

    def test_the_burrower_section_prints_bladestorm_and_the_threat_cap(self):
        text = self.text(self.trace, ab.show_burrowers)
        burrower = f"#{RBUR & 0xFFFFFFFF}"
        self.assertIn(f"  Bladestorm, the nearest living burrower at the cast, near within 25 yd: Mighty 0:05.000"
                      f" {burrower} 13.0 yd near, Mighty 1:00.000 {burrower} 35.0 yd; near in 1 of 2\n", text)
        self.assertIn("  anubarak burrower guard vetoes on bladestorm: Mighty 1\n", text)
        self.assertIn("  anubarak burrower threat cap vetoes per bot, one per verdict change: Fel 2, Mighty 1\n", text)
        bare = self.text(self.bare, ab.show_burrowers)
        self.assertIn("  anubarak burrower guard vetoes on bladestorm: no rows\n", bare)
        self.assertIn("  anubarak burrower threat cap vetoes per bot, one per verdict change: no rows\n", bare)

    def test_a_subspot_parses_its_spot_and_clearance(self):
        self.assertEqual(ab.parse_subspot("750,139,143 r22"), ((750.0, 139.0), 22))
        self.assertEqual(ab.parse_subspot("none"), (None, None))

    def test_the_drags_pre_steps_and_the_subspot_rows_per_window(self):
        first, later = ab.threat_rows(self.trace)
        self.assertEqual([(step["t"], step["next"], step["after"], round(step["walked"], 1))
                          for step in first["presteps"]], [(2000, "drag", 2000, 10.0), (8000, None, None, 0.0)])
        self.assertEqual(first["subspot"], [(6000, "750,135,143 r25"), (7000, "none"), (9000, "750,139,143 r22")])
        self.assertEqual((later["presteps"], later["subspot"]), ([], []))

    def test_the_threat_section_prints_the_pre_steps_and_the_subspot_rows(self):
        text = self.text(self.trace, ab.show_threat)
        self.assertIn("          drag pre-steps: Ecoterrorist 0:02.000 walked 10.0 yd, then drag +2.0 s; Ecoterrorist"
                      " 0:08.000 walked 0.0 yd, nothing after it\n", text)
        self.assertIn("          anub.subspot: 750,135,143 r25 0:06.000, none 0:07.000, 750,139,143 r22 0:09.000;"
                      " he went down 3.0 yd off its last spot\n", text)
        self.assertIn("  anub.subspot is each submerge-spot search, the spot and its clearance r in yards\n", text)
        self.assertIn("  no submerge leg is no anub.pickup submerge or prestep after the window's first search\n", text)
        bare = self.text(self.bare, ab.show_threat)
        self.assertIn("next pickup row; no prestep rows\n", bare)
        self.assertIn("clearance r in yards; no anub.subspot rows\n", bare)
        self.assertNotIn("anub.subspot: ", bare)

    def test_the_new_keys_count_as_present_and_absent_without_their_rows(self):
        for key in ("anub.relay", "anub.subspot"):
            self.assertNotIn(key, ab.missing_probes(self.trace))
            self.assertIn(key, ab.missing_probes(self.bare))

    def test_every_section_prints(self):
        text = self.text(self.trace, ab.show_banner, *(section for _, _, section in ab.SECTIONS))
        self.assertIn("PURSUIT\n", text)


def side_pull(diff: int, side1: int, later: list[dict] | None = None) -> list[dict]:
    return [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 6, "diff": diff, "boss": "anub-arak",
         "roster": [{"g": SIDE0, "n": "Ecoterrorist", "r": "tank", "c": "druid", "h": 0},
                    {"g": DRAGON, "n": "Dragon", "r": "tank", "c": "paladin", "h": 1}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        note(0, SIDE0, "anub.side0", SIDE0),
        note(0, SIDE0, "anub.side1", side1),
        *(later or []),
        {"t": 1000, "e": "end", "out": "wipe"},
    ]


class SideTanks(unittest.TestCase):
    def banner(self, diff: int, side1: int, later: list[dict] | None = None) -> tuple[tuple, str]:
        with tempfile.TemporaryDirectory() as folder:
            trace = write_trace(folder, "649_6_anub-arak_1789500003.ndjson", side_pull(diff, side1, later))
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                ab.show_banner(trace)
            return ab.side_tanks(trace), out.getvalue()

    def test_a_human_side_tank_is_named(self):
        latched, text = self.banner(1, DRAGON)
        self.assertEqual(latched, (2, {0: [SIDE0], 1: [DRAGON]}))
        self.assertIn("side tanks latched: 2 of 2 needed (side 0 Ecoterrorist, side 1 Dragon (human))", text)

    def test_a_swap_lists_each_side_in_order(self):
        latched, text = self.banner(1, DRAGON, [note(500, SIDE0, "anub.side0", DRAGON),
                                                note(500, SIDE0, "anub.side1", SIDE0)])
        self.assertEqual(latched, (2, {0: [SIDE0, DRAGON], 1: [DRAGON, SIDE0]}))
        self.assertIn("(side 0 Ecoterrorist > Dragon (human), side 1 Dragon (human) > Ecoterrorist)", text)

    def test_25_man_one_short(self):
        latched, text = self.banner(1, 0)
        self.assertEqual(latched, (2, {0: [SIDE0]}))
        self.assertIn("side tanks latched: 1 of 2 needed (side 0 Ecoterrorist, side 1 none)", text)

    def test_10_man_needs_one(self):
        latched, text = self.banner(2, 0)
        self.assertEqual(latched, (1, {0: [SIDE0]}))
        self.assertNotIn("side tanks latched", text)


WKITER, WPALADIN, WTANK = range(9201, 9204)
WBOSS = CREATURE | 1400
WSPIKE = CREATURE | 1401
WPATCH = CREATURE | 1411
WPATCH_AT = (X, Y + 20.0)
WPALADIN_AT = (X + 5.0, Y - 15.0)
# south, back north, past the tank patch under the Hand, then west
WKITER_PATH = [(12000, 30.0, -20.0), (20000, 30.0, 10.0), (24000, 8.0, 14.0), (30000, -20.0, 10.0)]
SPELL_HOP3, SPELL_ICE_BLOCK, SPELL_BLINK = 10278, 45438, 1953


def rework_snap(when: int) -> dict:
    kiter = lerp(WKITER_PATH, when)
    units = [[WTANK, X, Y + 1.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [WPALADIN, *WPALADIN_AT, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [WKITER, *kiter, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [WBOSS, X, Y, Z, 0.0, 80.0, 0.0, 0, 0, 0, 0, 0],
             [WPATCH, *WPATCH_AT, PATCH_Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0]]
    if 12000 <= when <= 30000:
        units.append([WSPIKE, kiter[0] - 6.0, kiter[1], Z, 0.0, 100.0, 0.0, WKITER, 1, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": [[PERMAFROST_25H, *WPATCH_AT, PATCH_Z, 6.0, 1]]}


def rework_pull(with_hands: bool = True) -> list[dict]:
    """A 25H submerge from 10 s to 72 s and one Hand kite in it, 12 s to 30 s: the escort walking and dodging,
    its claim 0.2 s before its Hand, the kiter past the tank patch under the Hand, a Blink in it, Ice Block
    after it and a second patch grant.
    The paladin's earlier Hand and an anub.hands row put its cooldown at 180 s; `with_hands` False drops the
    row."""
    records = [
        {"e": "hdr", "v": 14, "ts": 1789600000000, "map": ab.MAP_TOC, "inst": 16, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": WTANK, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 0},
                    {"g": WPALADIN, "n": "Holylight", "r": "heal", "c": 2, "h": 0},
                    {"g": WKITER, "n": "Smartface", "r": "ranged", "c": 8, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": WBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 10000, "e": "unit", "g": WSPIKE, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        {"t": 0, "e": "unit", "g": WPATCH, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"},
        *(note(when, WTANK, "anub.phase", value) for when, value in ((0, 1), (10000, 2), (72000, 1))),
        note(0, WTANK, "anub.patch0", WPATCH),
        aura(12000, WKITER, ab.SPELL_MARK, False, WSPIKE),
        aura(30000, WKITER, ab.SPELL_MARK, True, WSPIKE),
        note(12100, WKITER, "anub.kiteplan", "hand"),
        *(note(when, WPALADIN, "anub.escort", value) for when, value in (
            (12500, "walk"), (15000, "post"), (30000, "none"))),
        *(move_row(when, WPALADIN, WPALADIN_AT, by) for when, by in (
            (13000, ab.ESCORT_BY), (13400, ab.DODGE_BY), (13800, ab.ESCORT_BY), (16000, ab.DODGE_BY))),
        note(19800, WPALADIN, "anub.protect", "hop"),
        note(20100, WPALADIN, "anub.protect", "none"),
        cast(2000, WPALADIN, SPELL_HOP3, WTANK),
        cast(20000, WPALADIN, SPELL_HOP3, WKITER),
        aura(20000, WKITER, SPELL_HOP3, False, WPALADIN),
        aura(28000, WKITER, SPELL_HOP3, True, WPALADIN),
        cast(22000, WKITER, SPELL_BLINK, WKITER),
        cast(28500, WKITER, SPELL_ICE_BLOCK, WKITER),
        note(29000, WKITER, "anub.second", "grant"),
        {"t": 75000, "e": "end", "out": "wipe"},
    ]
    if with_hands:
        records.append(note(12100, WKITER, "anub.hands", f"{WPALADIN}:170:25"))
    records += [rework_snap(when) for when in range(-500, 75000, 500)]
    return records


class ReworkPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_16_anub-arak_1789600000.ndjson", rework_pull())
        cls.bare = write_trace(cls.folder.name, "649_16_anub-arak_1789600001.ndjson", rework_pull(False))
        cls.window = ab.mark_windows(cls.trace)[0]

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, trace) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_spike(trace)
        return out.getvalue()

    def test_the_hand_cooldown_reads_off_anub_hands_else_the_base_five_minutes(self):
        self.assertEqual(ab.hop_cooldown(self.trace), (180000, "anub.hands"))
        self.assertEqual(ab.hop_cooldown(self.bare), (ab.HOP_COOLDOWN_MS, "default"))
        self.assertEqual(ab.hand_reads(self.trace), [(12100, WPALADIN, 170000, 25)])

    def test_the_escort_reads_its_distance_at_the_claim_and_its_swaps(self):
        escort = self.window["escort_read"]
        self.assertEqual((escort["guid"], escort["t"], escort["swaps"]), (WPALADIN, 19800, 2))
        self.assertAlmostEqual(escort["yards"], math.hypot(25.0, 23.125))

    def test_each_hand_reads_its_claim_and_what_followed(self):
        window = self.window
        self.assertEqual([(rec["s"], gap) for rec, gap in window["claim_casts"]], [(WPALADIN, 200)])
        near = window["hand_patch"]
        self.assertEqual((near["t"], near["side"]), (25500, 0))
        self.assertAlmostEqual(near["yards"], math.sqrt(50.0))
        self.assertEqual([(rec["sp"], gap) for rec, gap in window["own_after_hand"]], [(SPELL_ICE_BLOCK, 500)])
        self.assertEqual([rec["sp"] for rec in window["hand_boosts"]], [SPELL_BLINK])
        self.assertEqual([(rec["t"], rec["sp"]) for rec in window["after_hand"]], [(22000, SPELL_BLINK)])
        self.assertEqual(window["second"], [29000])

    def test_per_submerge_the_kite_and_idle_seconds_and_the_terminals(self):
        rows = ab.submerge_kites(self.trace, ab.mark_windows(self.trace), ab.spent_patches(self.trace))
        self.assertEqual([(row["label"], row["kite"], row["idle"], dict(row["terminals"]), row["after"], row["deaths"])
                          for row in rows], [("P2", 18000, 40000, {"hand": 1}, None, 0)])

    def test_the_spike_section_prints_the_rework_lines(self):
        text = self.text(self.trace)
        self.assertIn("            at the end: nobody holding the gather; no kite patch\n"
                      "            escort Holylight 34.1 yd off at the claim +7.8 s, past its 30 yd reach; walk and dodge"
                      " swapped 2 time(s) within 1 s; 35.4 yd at the end\n"
                      "            claim to cast: Holylight 0.2 s\n", text)
        self.assertIn("            under the Hand 7.1 yd from the side 0 tank patch +13.5 s, inside 15.5 yd\n"
                      "            Ice Block 0.5 s after the Hand ran out\n"
                      "            boost in a Hand's first 8 s: Blink +10.0 s\n"
                      "            boost after the Hand: Blink +10.0 s\n"
                      "            anub.second grant +17.0 s\n", text)
        self.assertIn("  Hand of Protection with a boost the kiter cast later in its kite: Smartface 0:12.000 Blink"
                      " 2.0 s after the Hand\n", text)
        self.assertIn("  ready is a bot paladin alive and unmarked, its last Hand of Protection 180 s or more back, the\n"
                      "  cooldown read off anub.hands, with no Forbearance on the kiter\n", text)
        self.assertIn("  escort within 30 yd of the kiter at the claim: 0 of 1; further: Smartface 0:12.000 34.1 yd\n", text)
        self.assertIn("  claim to cast, Hand of Protection: median 0.2 s, over 0.3 s: none\n", text)
        self.assertIn("  kiter within 15.5 yd of a tank patch under a Hand: Smartface 0:12.000 7.1 yd\n", text)
        self.assertIn("  own immunity after a Hand ran out: Smartface 0:12.000 Ice Block +0.5 s\n", text)
        self.assertIn("  anub.second grants: Smartface 0:12.000 +17.0 s\n", text)
        self.assertIn("    P2     0:10.000  kite 18.0 s, idle 40.0 s; hand 1; no tank patch gone; 0 kiter death(s)\n",
                      text)

    def test_the_new_keys_count_as_present(self):
        gone = ab.missing_probes(self.trace)
        for key in ("anub.second", "anub.hands"):
            self.assertNotIn(key, gone)
        self.assertIn("anub.hands", ab.missing_probes(self.bare))


CTANK0, CTANK1, CHEAL = range(9301, 9304)
CBOSS = CREATURE | 1500
CB1, CB2, CB3, CB4 = (CREATURE | n for n in range(1501, 1505))


def crowd_snap(when: int) -> dict:
    """Each side tank on two burrowers; from 4 s a third on side 0's for 2 s, then side 1's two 8 yd from
    side 0's for 4 s, then side 0's tank dead and side 1's on three."""
    dead = when >= 10000
    units = [[CTANK0, 718.0, 120.0, Z, 0.0, 0.0 if dead else 100.0, 100.0, 0, 0, 0, 0, 0],
             [CTANK1, 753.0, 120.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [CHEAL, 735.0, 140.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0]]
    if when < 4000:
        burrowers = [(CB1, 720.0, 120.0, CTANK0), (CB2, 721.0, 122.0, CTANK0),
                     (CB3, 750.0, 120.0, CTANK1), (CB4, 751.0, 122.0, CTANK1)]
    elif when < 6000:
        burrowers = [(CB1, 720.0, 120.0, CTANK0), (CB2, 721.0, 122.0, CTANK0),
                     (CB3, 722.0, 121.0, CTANK0), (CB4, 751.0, 122.0, CTANK1)]
    elif when < 10000:
        burrowers = [(CB1, 720.0, 120.0, CTANK0), (CB2, 721.0, 122.0, CTANK0),
                     (CB3, 728.0, 121.0, CTANK1), (CB4, 729.0, 122.0, CTANK1)]
    else:
        burrowers = [(CB1, 750.0, 120.0, CTANK1), (CB2, 751.0, 122.0, CTANK1), (CB3, 752.0, 121.0, CTANK1)]
    units += [[guid, x, y, Z, 0.0, 90.0, 0.0, victim, 0, 0, 0, 0] for guid, x, y, victim in burrowers]
    return {"t": when, "e": "snap", "u": units, "hz": []}


def crowd_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1789700000000, "map": ab.MAP_TOC, "inst": 17, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": CTANK0, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": CTANK1, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
                    {"g": CHEAL, "n": "Elemena", "r": "heal", "c": 7, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": CBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"}
          for guid in (CB1, CB2, CB3, CB4)),
        note(0, CTANK0, "anub.phase", 1),
        note(0, CTANK0, "anub.side0", CTANK0),
        note(0, CTANK0, "anub.side1", CTANK1),
        cast(3800, CTANK0, ab.SPELL_RIGHTEOUS_DEFENSE, CHEAL),
        triggered(3800, CTANK0, ab.SPELL_RIGHTEOUS_DEFENSE_TAUNT, CB2),
        triggered(3800, CTANK0, ab.SPELL_RIGHTEOUS_DEFENSE_TAUNT, CB3),
        {"t": 14500, "e": "end", "out": "wipe"},
    ]
    records += [crowd_snap(when) for when in range(0, 14500, 500)]
    return records


class SideTankCrowds(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_17_anub-arak_1789700000.ndjson", crowd_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_third_burrower_names_the_taunt_and_a_lone_side_tank(self):
        crowds = ab.side_tank_crowds(self.trace)
        self.assertEqual([(run["tank"], run["start"], run["stop"], run["peak"], run["alone"]) for run in crowds],
                         [(CTANK0, 4000, 5500, 3, False), (CTANK1, 10000, 14000, 3, True)])
        self.assertEqual(crowds[0]["taunts"], {"Righteous Defense": 2})
        self.assertFalse(crowds[1]["taunts"])

    def test_frenzy_counts_the_sides_within_12_yd(self):
        self.assertEqual(ab.frenzy_samples(self.trace), [("P1", 20, 8)])

    def test_a_taunt_reads_kept_by_its_yards_at_the_cast(self):
        # Righteous Defense at 3.8 s: CB2 3.6 yd off stays on Bulwark, CB3 32 yd off is Felesta's by 8.3 s
        rows = ab.pickup_taunts(self.trace, ab.Samples(self.trace))
        self.assertEqual([(row["guid"], round(row["yd"], 1), row["to"]) for row in rows],
                         [(CB2, 3.6, CTANK0), (CB3, 32.0, CTANK1)])
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_pickup_taunts(self.trace, ab.Samples(self.trace))
        self.assertIn("    Bulwark        Righteous Defense within 40 yd kept 1 of 2 (lost to Felesta 1); beyond"
                      " none\n", out.getvalue())

    def test_the_burrower_section_prints_both(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_side_tank_crowds(self.trace, ab.Samples(self.trace))
        text = out.getvalue()
        self.assertIn("0:04.000-0:05.500 Bulwark 3; Righteous Defense 2", text)
        self.assertIn("0:10.000-0:14.000 Felesta 3, the only side tank alive; no taunt", text)
        self.assertIn("with both holding one: P1 8 of 20", text)

    def test_a_search_with_no_submerge_leg_says_so(self):
        row = {"subspot": [(1000, "706,138,142 r25")], "submerge": None, "subleg": False, "subhold": 2}
        self.assertEqual(ab.subspot_text(row), "706,138,142 r25 0:01.000; no submerge leg, 2 subhold row(s)")
        self.assertEqual(ab.subspot_text({**row, "subleg": True}), "706,138,142 r25 0:01.000")


ETANK, EPALADIN, EKITER, EOWN, EROGUE, EHEAL = range(9501, 9507)
EBOSS = CREATURE | 1600
ESPIKE = CREATURE | 1601
ESCARAB = CREATURE | 1602
EKITER_AT = (X + 30.0, Y)
EOWN_AT = (X, Y + 30.0)
SPELL_NITRO_USE, SPELL_SPRINT = 55004, 11305


def escort_snap(when: int) -> dict:
    """The escort 20 yd off its kiter until 25 s, then 45; the healer 50 yd off the main tank for the first 4 s."""
    paladin = (X + 10.0, Y) if when < 25000 else (X - 15.0, Y)
    heal = (X + 50.0, Y + 30.0) if when < 4000 else (X + 5.0, Y + 5.0)
    units = [[ETANK, X, Y + 1.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [EPALADIN, *paladin, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [EKITER, *EKITER_AT, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [EOWN, *EOWN_AT, Z, 0.0, 0.0 if when >= 48000 else 100.0, 100.0, 0, 0, 0, 0, 0],
             [EROGUE, X - 5.0, Y, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [EHEAL, *heal, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [EBOSS, X, Y, Z, 0.0, 80.0, 0.0, ETANK, 0, 0, 0, 0]]
    if 12000 <= when <= 30000:
        units.append([ESPIKE, EKITER_AT[0] - 6.0, EKITER_AT[1], Z, 0.0, 100.0, 0.0, EKITER, 1, 0, 0, 0])
    elif 40000 <= when <= 48000:
        units.append([ESPIKE, EOWN_AT[0] - 6.0, EOWN_AT[1], Z, 0.0, 100.0, 0.0, EOWN, 1, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": []}


def escort_pull() -> list[dict]:
    """A 25H submerge from 10 s to 72 s: a hand kite whose Nitro goes with its escort 20 yd off, the escort then
    in Permafrost and 45 yd off at the end; an own kite dead to a scarab with its Divine Shield unused; a rogue
    Sprint off the mark in phases 1 and 2; and the main tank past every healer's reach for 4 s."""
    records = [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": ab.MAP_TOC, "inst": 18, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": ETANK, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 0},
                    {"g": EPALADIN, "n": "Justice", "r": "melee", "c": 2, "h": 0},
                    {"g": EKITER, "n": "Agony", "r": "ranged", "c": 9, "h": 0},
                    {"g": EOWN, "n": "Holylight", "r": "melee", "c": 2, "h": 0},
                    {"g": EROGUE, "n": "Shadow", "r": "melee", "c": 4, "h": 0},
                    {"g": EHEAL, "n": "Elemena", "r": "heal", "c": 7, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": EBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 10000, "e": "unit", "g": ESPIKE, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        {"t": 40000, "e": "unit", "g": ESCARAB, "en": ab.NPC_SCARAB, "n": "Swarm Scarab"},
        *(note(when, ETANK, "anub.phase", value) for when, value in ((0, 1), (10000, 2), (72000, 1))),
        aura(12000, EKITER, ab.SPELL_MARK, False, ESPIKE),
        aura(30000, EKITER, ab.SPELL_MARK, True, ESPIKE),
        note(12100, EKITER, "anub.kiteplan", "hand"),
        *(note(when, EPALADIN, "anub.escort", value) for when, value in ((12500, "walk"), (30000, "none"))),
        cast(20000, EKITER, SPELL_NITRO_USE, EKITER),
        aura(22000, EPALADIN, PERMAFROST_25H, False),
        aura(25000, EPALADIN, PERMAFROST_25H, True),
        aura(40000, EOWN, ab.SPELL_MARK, False, ESPIKE),
        note(40100, EOWN, "anub.kiteplan", "own"),
        {"t": 48000, "e": "death", "g": EOWN, "killer": ESCARAB, "x": EOWN_AT[0], "y": EOWN_AT[1], "z": Z},
        aura(48000, EOWN, ab.SPELL_MARK, True, ESPIKE),
        cast(5000, EROGUE, SPELL_SPRINT, EROGUE),
        cast(50000, EROGUE, SPELL_SPRINT, EROGUE),
        {"t": 75000, "e": "end", "out": "wipe"},
    ]
    records += [escort_snap(when) for when in range(-500, 75000, 500)]
    return records


class EscortPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_18_anub-arak_1789800000.ndjson", escort_pull())
        cls.windows = ab.mark_windows(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_the_escort_reads_at_the_boost_at_the_end_and_in_permafrost(self):
        escort = self.windows[0]["escort_read"]
        self.assertEqual((escort["guid"], escort["boost"]), (EPALADIN, 20000))
        self.assertAlmostEqual(escort["boost_yd"], 20.0)
        self.assertAlmostEqual(escort["end_yd"], 45.0)
        self.assertAlmostEqual(escort["frost_s"], 3.0)
        # no Hand went out and the escort only walked
        self.assertEqual((self.windows[0]["after_hand"], self.windows[0]["patch_escort"]), ([], None))

    def test_a_kiter_dead_to_a_scarab_with_its_own_unused(self):
        own = self.windows[1]
        self.assertEqual((own["guid"], own["outcome"], own["killer"], own["killer_spike"]),
                         (EOWN, "death", ESCARAB, False))
        self.assertTrue(ab.own_unused_death(own))
        self.assertFalse(ab.own_unused_death(self.windows[0]))

    def test_kite_tools_spent_off_the_mark_per_phase(self):
        self.assertEqual([(label, rec["s"], rec["sp"], rec["t"]) for label, rec in ab.spent_kite_tools(self.trace, self.windows)],
                         [("P1", EROGUE, SPELL_SPRINT, 5000), ("P2", EROGUE, SPELL_SPRINT, 50000)])

    def test_the_main_tank_out_of_every_healers_reach(self):
        bare = ab.healer_rows(self.trace)["bare"]
        self.assertEqual({key: ms for key, ms in bare.items() if ms}, {("main", ETANK, "P1"): 4000})

    def test_the_sections_print_them(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_spike(self.trace)
            ab.show_healers(self.trace)
        text = out.getvalue()
        self.assertIn("; 20.0 yd at the boost +8.0 s; 45.0 yd at the end; carried Permafrost 3.0 s\n", text)
        self.assertIn("            died to Swarm Scarab with its own immunity unused\n", text)
        self.assertIn("  Hand of Protection with a boost the kiter cast later in its kite: none\n", text)
        self.assertIn("  patch kites turned within 3 s of a boost going out: none\n", text)
        self.assertIn("  escort waiting by the patch (anub.escort patch), at the Hand or the kite's end: none\n", text)
        self.assertIn("  under 1.5 yd in 1 s: none\n", text)
        self.assertIn("  burrowers within 5 yd of a healer in phase 3: 0; with no step from the exit or the emerge\n"
                      "  within 7.5 yd of a Permafrost centre: none\n", text)
        self.assertIn("  escort carrying Permafrost in the kite: Agony 0:12.000 3.0 s\n", text)
        self.assertIn("  kiter dead to anything but the spike with its own immunity unused: Holylight 0:40.000 by"
                      " Swarm Scarab\n", text)
        self.assertIn("  kite tools a bot spent off its own mark, per phase: P1 Shadow Sprint 0:05.000;"
                      " P2 Shadow Sprint 0:50.000\n", text)
        self.assertIn("  main tank Ecoterrorist alive with no living healer within 40 yd: P1 4.0 s\n", text)


DANGRY, DHOLY = 9401, 9402


def dodge_pull() -> list[dict]:
    """He is down from 10 to 40 s. Angry swaps between its gather and a dodge four times within 1.5 s and
    once 2 s apart, dodging to two spots, each picked again within a yard; its dodge before the submerge
    doesn't count. Holylight dodges once, 5 s before its gather walk."""
    gather, east, west = (X, Y), (X + 12.0, Y), (X - 12.0, Y)
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": ab.MAP_TOC, "inst": 18, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": DANGRY, "n": "Angry", "r": "melee", "c": 1, "h": 0},
                    {"g": DHOLY, "n": "Holylight", "r": "heal", "c": 2, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        note(0, DANGRY, "anub.phase", 1),
        note(10000, DANGRY, "anub.phase", 2),
        note(40000, DANGRY, "anub.phase", 1),
        move_row(5000, DANGRY, east, ab.DODGE_BY),
        move_row(11000, DANGRY, gather, ab.GATHER_BY),
        move_row(11500, DANGRY, east, ab.DODGE_BY),
        move_row(12000, DANGRY, gather, ab.GATHER_BY),
        move_row(12400, DANGRY, (east[0] + 0.5, east[1]), ab.DODGE_BY),
        move_row(14400, DANGRY, gather, ab.GATHER_BY),
        move_row(15000, DANGRY, west, ab.DODGE_BY),
        move_row(15200, DANGRY, (west[0], west[1] + 0.5), ab.DODGE_BY),
        move_row(20000, DHOLY, (X, Y + 12.0), ab.DODGE_BY),
        move_row(25000, DHOLY, gather, ab.GATHER_BY),
        {"t": 45000, "e": "end", "out": "wipe"},
    ]


class DodgeSwaps(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_18_anub-arak_1789800000.ndjson", dodge_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_swaps_and_walks_count_per_submerge(self):
        self.assertEqual([(row["label"], row["start"], dict(row["swaps"]), dict(row["walks"]))
                          for row in ab.dodge_swaps(self.trace)],
                         [("P2", 10000, {DANGRY: 4}, {DANGRY: 2, DHOLY: 1})])

    def test_the_spike_section_prints_them(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_dodges(self.trace)
        text = out.getvalue()
        self.assertIn("    P2     0:10.000 swaps 4: Angry 4\n          dodge walks 3: Angry 2, Holylight 1\n", text)


CTANK, CLOCK, CHUNTER, CROGUE, CKITER = range(9601, 9606)
CBOSS = CREATURE | 1700
CSPIKE = CREATURE | 1701
CSPHERE = CREATURE | 1702
CPRESHOT = CREATURE | 1703
CIRCLE = (X - 20.0, Y)
SPELL_FEIGN, SPELL_CIRCLE_SUMMON, SPELL_CIRCLE_TELEPORT = 5384, 48018, 48020


def circle_lock(when: int) -> tuple[float, float]:
    """On its circle, out 25 yd east of it from 45 s, back on it by the teleport, and off it 1.5 s on."""
    if 45000 <= when <= 51000:
        return X + 5.0, Y
    if 51500 <= when < 53000 or when < 43000:
        return CIRCLE
    return (X - 10.0, Y) if when < 45000 else (X - 25.0, Y)


def circle_snap(when: int, preshot: bool) -> dict:
    units = [[CTANK, X - 5.0, Y, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [CLOCK, *circle_lock(when), Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [CHUNTER, X + 10.0, Y + 10.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [CROGUE, X + 10.0, Y - 10.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [CKITER, X, Y + 20.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [CBOSS, X, Y + 5.0, Z, 0.0, 60.0, 0.0, CTANK, 0, 0, 0, 0]]
    hz = []
    if 43000 <= when <= 60000:
        units.append([CSPIKE, *((X + 1.0, Y) if when < 51500 else (X - 16.0, Y)), Z, 0.0, 100.0, 0.0, CLOCK, 1, 0, 0, 0])
    elif 66000 <= when <= 70000:
        units.append([CSPIKE, X, Y + 14.0, Z, 0.0, 100.0, 0.0, CKITER, 1, 0, 0, 0])
    if when < 61000:
        units.append([CSPHERE, X, Y - 20.0, FLYING_Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
    elif when <= 70000:
        z = FALLING_Z if when < 62500 else PATCH_Z
        units.append([CSPHERE, X, Y - 20.0, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_25H, X, Y - 20.0, PATCH_Z, 6.0, 1])
    if preshot:
        z = FLYING_Z if when < 25000 else FALLING_Z if when < 26500 else PATCH_Z
        units.append([CPRESHOT, X - 30.0, Y + 30.0, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_25H, X - 30.0, Y + 30.0, PATCH_Z, 6.0, 1])
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def circle_pull(preshot: bool = True, summon: bool = True) -> list[dict]:
    """A 25H pull, P1 to 40 s, P2 to 102 s. The warlock puts its circle down at 2 s, is marked at 43 s, runs 25 yd
    east of the circle, Nitros at 48 s and teleports at 51 s with the spike 4 yd off, then stands 1.5 s. A kite
    sphere shot 15 s before the submerge lies down as the spike comes up and is never spent; another goes down
    at 61 s and a kiter marked at 66 s spends it at 70 s. In phase 1 a hunter's Feign Death, back long before the
    submerge, and a rogue's Sprint, still out. Without `preshot` no patch is down at the spike, and without
    `summon` the circle went down before the trace."""
    records = [
        {"e": "hdr", "v": 14, "ts": 1789900000000, "map": ab.MAP_TOC, "inst": 19, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": CTANK, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": CLOCK, "n": "Hellflame", "r": "ranged", "c": 9, "h": 0},
                    {"g": CHUNTER, "n": "Trueshot", "r": "ranged", "c": 3, "h": 0},
                    {"g": CROGUE, "n": "Shadow", "r": "melee", "c": 4, "h": 0},
                    {"g": CKITER, "n": "Pyro", "r": "ranged", "c": 8, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": CBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 0, "e": "unit", "g": CSPHERE, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"},
        {"t": 0, "e": "unit", "g": CPRESHOT, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"},
        {"t": 43000, "e": "unit", "g": CSPIKE, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        *(note(when, CTANK, "anub.phase", value) for when, value in ((0, 1), (40000, 2), (102000, 1))),
        cast(5000, CHUNTER, SPELL_FEIGN, CHUNTER),
        cast(5000, CROGUE, SPELL_SPRINT, CROGUE),
        aura(43000, CLOCK, ab.SPELL_MARK, False, CSPIKE),
        cast(48000, CLOCK, SPELL_NITRO_USE, CLOCK),
        cast(51000, CLOCK, SPELL_CIRCLE_TELEPORT, CLOCK),
        aura(60000, CLOCK, ab.SPELL_MARK, True, CSPIKE),
        aura(66000, CKITER, ab.SPELL_MARK, False, CSPIKE),
        aura(70000, CKITER, ab.SPELL_MARK, True, CSPIKE),
        {"t": 110000, "e": "end", "out": "wipe"},
    ]
    if summon:
        records.append({"t": 2000, "e": "cast", "s": CLOCK, "sp": SPELL_CIRCLE_SUMMON, "tgt": 0, "ct": 500})
    records += [circle_snap(when, preshot) for when in range(-500, 110000, 500)]
    return records


class CirclePull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_19_anub-arak_1789900000.ndjson", circle_pull())
        cls.windows = ab.mark_windows(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_feign_death_back_before_the_submerge_is_no_spent_tool(self):
        self.assertEqual([(label, rec["s"], rec["sp"]) for label, rec in ab.spent_kite_tools(self.trace, self.windows)],
                         [("P1", CROGUE, SPELL_SPRINT)])

    def test_the_teleport_reads_its_circle_its_boost_and_the_stand(self):
        [row] = ab.circle_teleports(self.trace, self.windows)
        self.assertEqual((row["guid"], row["t"], row["boost"]["sp"], row["stood"]),
                         (CLOCK, 51000, SPELL_NITRO_USE, 1500))
        self.assertAlmostEqual(row["spike_yd"], 4.0)
        self.assertAlmostEqual(row["circle_yd"], 21.0)
        self.assertAlmostEqual(row["passed"], 25.0)

    def test_a_kite_sphere_spent_early_in_its_kite(self):
        [row] = ab.kite_sphere_rows(self.trace)
        self.assertEqual((row["label"], row["start"], row["ready"], row["down"]), ("P2", 40000, 1, [CPRESHOT]))
        self.assertEqual(row["spheres"], [{"guid": CPRESHOT, "killed": 25000, "spent": None, "kiter": None,
                                           "into": None, "tank_gap": None},
                                          {"guid": CSPHERE, "killed": 61000, "spent": 70000, "kiter": CKITER,
                                           "into": 4000, "tank_gap": None}])

    def test_no_preshot_and_no_summon_in_the_trace(self):
        # the circle went down before the trace, so it is where the teleport landed
        trace = write_trace(self.folder.name, "649_19_anub-arak_1789900001.ndjson", circle_pull(False, False))
        [row] = ab.circle_teleports(trace, ab.mark_windows(trace))
        self.assertEqual((row["circle"], row["stood"]), (CIRCLE, 1500))
        [spheres] = ab.kite_sphere_rows(trace)
        self.assertEqual((spheres["ready"], spheres["down"]), (0, []))
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_spheres(trace)
        self.assertIn("into its kite, early  no kite patch down as the spike came up\n", out.getvalue())
        self.assertIn("  kite patches on the floor as he went down, under 2 flagged: P2 0 under\n", out.getvalue())

    def test_a_hand_on_a_warlock_reads_whether_its_circle_was_in_reach(self):
        # out of reach from 44 s, back in at 45.5 s; the mage's Hand is no warlock's
        records = circle_pull() + [
            aura(44000, CLOCK, ab.SPELL_CIRCLE_IN_RANGE, True, CLOCK),
            aura(45500, CLOCK, ab.SPELL_CIRCLE_IN_RANGE, False, CLOCK),
            cast(44500, CTANK, ab.HAND_OF_PROTECTION[2], CLOCK),
            cast(46000, CTANK, ab.HAND_OF_PROTECTION[2], CLOCK),
            cast(67000, CTANK, ab.HAND_OF_PROTECTION[2], CKITER),
        ]
        trace = write_trace(self.folder.name, "649_19_anub-arak_1789900002.ndjson", records)
        rows = ab.warlock_hands(trace, ab.mark_windows(trace))
        self.assertEqual([(row["guid"], row["t"], row["reach"]) for row in rows],
                         [(CLOCK, 44500, False), (CLOCK, 46000, True)])
        # before its first row: in reach when that row is a removal
        self.assertTrue(ab.circle_in_reach(trace, CLOCK, 43000))
        self.assertFalse(ab.circle_in_reach(trace, CKITER, 67000))
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_spike(trace)
        self.assertIn("  Hand of Protection on a bot warlock kiter, its circle in reach (62388) flagged: Hellflame"
                      " 0:43.000 +1.5 s by Bulwark, Hellflame 0:43.000 +3.0 s by Bulwark, circle in reach\n",
                      out.getvalue())

    def test_the_sections_print_them(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_spike(self.trace)
            ab.show_spheres(self.trace)
        text = out.getvalue()
        self.assertIn("  Hand of Protection on a bot warlock kiter, its circle in reach (62388) flagged: none\n", text)
        self.assertIn("  Demonic Circle teleports: Hellflame 0:51.000 spike 4.0 yd off, the circle 21.0 yd from it;"
                      " Nitro Boosts 3.0 s before, passing the circle at 25.0 yd; stood 1.5 s on landing\n", text)
        self.assertIn("  kite tools a bot spent off its own mark, per phase: P1 Shadow Sprint 0:05.000\n", text)
        self.assertIn(" down -15.0 s, never spent; ", text)
        self.assertIn("down +21.0 s, spent +30.0 s by Pyro 4.0 s into its kite, early\n", text)
        self.assertIn("  kite patches on the floor as he went down, under 2 flagged: P2 1 under\n", text)


B3 = CREATURE | 923
B4 = CREATURE | 924


def swarm_pull() -> list[dict]:
    """The synthetic pull plus a phase 3 race: he sits at 39% from 45 s, B4 is up from 45 to 53 s and B3
    from 52 s; Trueshot hits him, Straggler B4 and then B3. Mercy casts Flash Heal onto Trueshot at full
    health and Justice's Judgement of Light heals it unasked. Trueshot spends Rapid Fire held, Straggler
    Icy Veins released as behind, and Arkos, human, Rapid Fire."""
    records = anub_pull()
    for rec in records:
        if rec.get("e") != "snap":
            continue
        when = rec["t"]
        for row in rec["u"]:
            if row[0] == BOSS and 45000 <= when < 50000:
                row[5] = 39.0
            elif row[0] == KITE and when >= 50000:
                row[7] = BOSS
            elif row[0] == LATE and when >= 50000:
                row[7] = B4 if when < 53000 else B3
        if 45000 <= when <= 53000:
            rec["u"].append([B4, X - 5.0, Y, Z, 0.0, 80.0, 0.0, TANK, 0, 0, 0, 0])
        if when >= 52000:
            rec["u"].append([B3, X - 6.0, Y, Z, 0.0, 100.0, 0.0, TANK, 0, 0, 0, 0])
    records += [
        {"t": 45000, "e": "unit", "g": B4, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        {"t": 52000, "e": "unit", "g": B3, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        cast(56000, HEAL, 48071, KITE),
        {"t": 56000, "e": "heal", "s": HEAL, "d": KITE, "sp": 48071, "a": 6000, "oh": 0},
        {"t": 57000, "e": "heal", "s": PALADIN, "d": KITE, "sp": 20267, "a": 2000, "oh": 0},
        note(46000, KITE, "anub.burst", "hold"),
        cast(47000, KITE, 3045, KITE),
        note(47500, LATE, "anub.burst", "behind"),
        cast(48000, LATE, 12472, LATE),
        cast(48500, MELEE, 3045, MELEE),
        note(50300, KITE, "anub.burst", "wait"),
        note(53500, KITE, "anub.burst", "open"),
    ]
    return records


class SwarmRace(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_4_anub-arak_1789500100.ndjson", swarm_pull())
        cls.samples = ab.Samples(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_the_steps_split_the_swarm_and_the_healing_by_who_takes_it(self):
        first, second = ab.swarm_bins(self.trace, 50000, 60000, self.samples)
        self.assertEqual((first["boss"], first["tank"], first["other"]), (46.0, 100.0, 100.0))
        self.assertEqual((first["swarm_tank"], first["swarm_other"]), (3000, 2000))
        self.assertEqual((first["heal_tank"], first["heal_cold"], first["heal_other"]), (3000, 5000, 0))
        self.assertEqual((second["boss"], second["heal_cold"], second["heal_other"]), (41.0, 3000, 8000))

    def test_only_a_bot_heal_it_cast_onto_a_non_carrier_is_a_breach(self):
        self.assertEqual(ab.swarm_heal_breaches(self.trace, 50000, 60000), {(HEAL, 48071): [1, 6000]})

    def test_burrowers_up_as_phase_3_began_part_from_newer_ones(self):
        self.assertEqual(ab.swarm_burrowers(self.trace, 50000, self.samples), ({B4}, {B3}, 53000))
        shares = ab.swarm_dps_targets(self.trace, 50000, 60000, self.samples)
        self.assertEqual({kind: round(share, 2) for kind, share in shares.items()},
                         {"boss": 0.5, "old": 0.15, "new": 0.35, "other": 0.0})

    def test_burst_from_the_hold_counts_bots_with_their_reading(self):
        crossed, casts = ab.burst_before_swarm(self.trace, 50000, self.samples)
        self.assertEqual(crossed, 45000)
        self.assertEqual([(row["guid"], row["spell"], row["reading"]) for row in casts],
                         [(KITE, 3045, "hold"), (LATE, 12472, "behind")])

    def test_the_race_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_swarm(self.trace)
        text = out.getvalue()
        self.assertIn("bot heals cast onto a non-tank with no Penetrating Cold, a healer itself included: Mercy", text)
        self.assertIn("1 burrower(s) up as it began, the last gone 0:53.000 (+3.0 s); lust +1.5 s;"
                      " anub.burst open +3.5 s", text)
        self.assertIn("burst cast from his 50% at 0:45.000 to phase 3: 2, 1 not released as behind:"
                      " Trueshot Rapid Fire 0:47.000", text)

    def test_the_engage_names_the_nearest_and_each_bot_tank(self):
        spread = ab.engage_spread(self.trace, self.samples)
        self.assertEqual(spread["nearest"], TANK)
        self.assertAlmostEqual(spread["yards"], 31.24, places=2)
        self.assertEqual([guid for guid, _ in spread["tanks"]], [TANK])

    def test_a_bot_tank_inside_his_aggro_at_the_engage_is_flagged(self):
        records = anub_pull()
        for rec in records:
            if rec.get("e") == "snap" and rec["t"] <= 0:
                for row in rec["u"]:
                    if row[0] == TANK:
                        row[1], row[2] = ab.ANUB_WAIT[0] - 20.0, ab.ANUB_WAIT[1]
        with tempfile.TemporaryDirectory() as folder:
            trace = write_trace(folder, "649_4_anub-arak_1789500200.ndjson", records)
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                ab.show_pull(trace)
        self.assertIn("nearest his wait point Bulwark (bot tank) 20.0 yd; bot tanks Bulwark 20.0 yd;"
                      " inside his 23 yd aggro: Bulwark", out.getvalue())


JTANK, JPALADIN, JPYRO, JSHADOW, JDRUID = range(9701, 9706)
JBOSS = CREATURE | 1801
JSPIKE = CREATURE | 1802
JTP0, JTP1, JKP1, JKP2 = (CREATURE | n for n in range(1811, 1815))
JPATCHES = {JTP0: (X + 5.0, Y + 20.0), JTP1: (X + 5.0, Y - 20.0), JKP1: (X - 30.0, Y + 10.0),
            JKP2: (X - 30.0, Y - 10.0)}
JKITERS = {JPYRO: (X - 45.0, Y + 40.0), JSHADOW: (X + 20.0, Y), JDRUID: (X - 30.0, Y - 48.0)}
# (start, stop, kiter) per mark under him
JMARKS = ((12000, 22000, JPYRO), (25000, 35000, JSHADOW), (40000, 50000, JDRUID))
SPELL_DASH, SPELL_CAT_FORM = 33357, 768


def boost_snap(when: int) -> dict:
    """The escort 15 yd west of the north kite patch until 38 s, then 18 yd south of the south one."""
    paladin = (X - 45.0, Y + 10.0) if when < 38000 else (X - 30.0, Y - 28.0)
    units = [[JTANK, X - 5.0, Y, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [JPALADIN, *paladin, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             *([guid, *spot, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0] for guid, spot in JKITERS.items()),
             [JBOSS, X, Y, Z, 0.0, 70.0, 0.0, JTANK, 0, 0, 0, 0]]
    kiter = next((guid for start, stop, guid in JMARKS if start <= when <= stop), None)
    if kiter:
        units.append([JSPIKE, JKITERS[kiter][0] + 6.0, JKITERS[kiter][1], Z, 0.0, 100.0, 0.0, kiter, 1, 0, 0, 0])
    hz = []
    for guid, spot in JPATCHES.items():
        units.append([guid, *spot, PATCH_Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        hz.append([PERMAFROST_25H, *spot, PATCH_Z, 6.0, 1])
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def boost_pull() -> list[dict]:
    """A 25H submerge from 10 s to 70 s with both kite patches down as he goes under. Pyro's Hand kite Blinks
    before its Hand and Nitros 3 s after it, its escort waiting by the patch; Shadow's patch kite turns 2 s after
    its Sprint; Druidica's turns 5 s after its Dash, Cat Form just before the turn counting for nothing, the escort
    by the patch to the end with no Hand."""
    records = [
        {"e": "hdr", "v": 14, "ts": 1790000000000, "map": ab.MAP_TOC, "inst": 20, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": JTANK, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 1},
                    {"g": JPALADIN, "n": "Justice", "r": "melee", "c": 2, "h": 0},
                    {"g": JPYRO, "n": "Pyro", "r": "ranged", "c": 8, "h": 0},
                    {"g": JSHADOW, "n": "Shadow", "r": "melee", "c": 4, "h": 0},
                    {"g": JDRUID, "n": "Druidica", "r": "ranged", "c": 11, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": JBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 10000, "e": "unit", "g": JSPIKE, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"} for guid in JPATCHES),
        *(note(when, JTANK, "anub.phase", value) for when, value in ((0, 1), (10000, 2), (70000, 1))),
        note(0, JTANK, "anub.patch0", JTP0),
        note(0, JTANK, "anub.patch1", JTP1),
        *(aura(when, kiter, ab.SPELL_MARK, removed, JSPIKE) for start, stop, kiter in JMARKS
          for when, removed in ((start, False), (stop, True))),
        note(12100, JPYRO, "anub.kiteplan", "hand"),
        note(25100, JSHADOW, "anub.kiteplan", "patch"),
        note(40100, JDRUID, "anub.kiteplan", "patch"),
        *(note(when, guid, "anub.kitestage", stage) for when, guid, stage in (
            (25000, JSHADOW, "run"), (28000, JSHADOW, "patch"), (35000, JSHADOW, "none"),
            (40000, JDRUID, "run"), (46000, JDRUID, "patch"), (50000, JDRUID, "none"))),
        *(note(when, JPALADIN, "anub.escort", value) for when, value in (
            (12500, "patch"), (22000, "none"), (40500, "patch"), (50000, "none"))),
        cast(13000, JPYRO, SPELL_BLINK, JPYRO),
        cast(15000, JPALADIN, SPELL_HOP3, JPYRO),
        cast(18000, JPYRO, SPELL_NITRO_USE, JPYRO),
        cast(26000, JSHADOW, SPELL_SPRINT, JSHADOW),
        cast(41000, JDRUID, SPELL_DASH, JDRUID),
        cast(45500, JDRUID, SPELL_CAT_FORM, JDRUID),
        {"t": 75000, "e": "end", "out": "wipe"},
    ]
    records += [boost_snap(when) for when in range(-500, 75000, 500)]
    return records


class BoostPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_20_anub-arak_1790000000.ndjson", boost_pull())
        cls.windows = ab.mark_windows(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, *sections) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            for section in sections:
                section(self.trace)
        return out.getvalue()

    def test_a_boost_cast_after_the_hand_counts_and_one_before_it_does_not(self):
        self.assertEqual([[(rec["t"], rec["sp"]) for rec in window["after_hand"]] for window in self.windows],
                         [[(18000, SPELL_NITRO_USE)], [], []])

    def test_a_patch_kite_turning_within_3_s_of_a_run_boost(self):
        shadow, druid = self.windows[1:]
        self.assertEqual((shadow["turn"], shadow["boost_turn"]["sp"]), (28000, SPELL_SPRINT))
        self.assertEqual((druid["turn"], druid["boost_turn"]), (46000, None))
        self.assertIsNone(self.windows[0]["boost_turn"])
        window = {"turn": 5000, "boost_turn": {"t": 5100, "sp": SPELL_DASH}}
        self.assertEqual(ab.boost_turn_text(window), "turned with Dash")
        self.assertEqual(ab.boost_turn_text({**window, "boost_turn": {"t": 7500, "sp": SPELL_DASH}}),
                         "turned 2.5 s before Dash")

    def test_the_escort_by_the_patch_reads_at_the_hand_else_at_the_end(self):
        pyro, shadow, druid = (window["patch_escort"] for window in self.windows)
        self.assertIsNone(shadow)
        self.assertEqual((pyro["guid"], pyro["t"], pyro["hand"], pyro["spot"]),
                         (JPALADIN, 15000, True, (X - 45.0, Y + 10.0)))
        self.assertAlmostEqual(pyro["kiter_yd"], 30.0)
        self.assertAlmostEqual(pyro["patch_yd"], 15.0)
        self.assertEqual((druid["t"], druid["hand"]), (50000, False))
        self.assertAlmostEqual(druid["kiter_yd"], 20.0)
        self.assertAlmostEqual(druid["patch_yd"], 18.0)

    def test_both_kite_patches_down_as_he_went_under(self):
        [row] = ab.kite_sphere_rows(self.trace)
        self.assertEqual(row["down"], [JKP1, JKP2])

    def test_the_sections_print_them(self):
        text = self.text(ab.show_spike, ab.show_spheres)
        self.assertIn("            boost after the Hand: Nitro Boosts +6.0 s\n"
                      "            escort by the patch: Justice at 700.0,145.0, 15.0 yd off the kite patch, 30.0 yd"
                      " from the kiter at the Hand +3.0 s\n", text)
        self.assertIn("            turned 2.0 s after Sprint\n", text)
        self.assertIn("  Hand of Protection with a boost the kiter cast later in its kite: Pyro 0:12.000 Nitro Boosts"
                      " 3.0 s after the Hand\n", text)
        self.assertIn("  patch kites turned within 3 s of a boost going out: Shadow 0:25.000 turned 2.0 s after"
                      " Sprint\n", text)
        self.assertIn("  escort waiting by the patch (anub.escort patch), at the Hand or the kite's end: Pyro 0:12.000"
                      " Justice at 700.0,145.0, 15.0 yd off the kite patch, 30.0 yd from the kiter at the Hand +3.0 s,"
                      " Druidica 0:40.000 Justice at 715.0,107.0, 18.0 yd off the kite patch, 20.0 yd from the kiter"
                      " at the end +10.0 s\n", text)
        self.assertIn("  kite patches on the floor as he went down, under 2 flagged: P2 2\n", text)

    def test_every_section_prints(self):
        self.assertIn("TANKS\n", self.text(ab.show_banner, *(section for _, _, section in ab.SECTIONS)))


VMAIN, VFELESTA, VBULWARK, VTREE, VPRAYER, VELEMENA = range(9801, 9807)
VBOSS = CREATURE | 1901
VTP0, VTP1 = CREATURE | 1911, CREATURE | 1912
VB1, VB2, VB3, VB4 = (CREATURE | n for n in range(1921, 1925))
VPATCHES = {VTP0: (X + 5.0, Y + 20.0), VTP1: (X + 5.0, Y - 20.0)}
COLD_25H = 68510
SPELL_DND, SPELL_DND_TICK, SPELL_PESTILENCE, SPELL_BLOOD_BOIL = 49938, 52212, 50842, 49941
SPELL_CONSECRATION, SPELL_JUDGEMENT = 48819, 20271
SPELL_RENEW, SPELL_PWS, SPELL_UNHOLY_STRENGTH = 48068, 48066, 53365
VSPELLS = {SPELL_DND: "Death and Decay", SPELL_DND_TICK: "Death and Decay", SPELL_PESTILENCE: "Pestilence",
           SPELL_BLOOD_BOIL: "Blood Boil", SPELL_CONSECRATION: "Consecration", SPELL_JUDGEMENT: "Judgement of Light",
           SPELL_SWIPE: "Swipe (Bear)", SPELL_RENEW: "Renew", SPELL_PWS: "Power Word: Shield",
           SPELL_UNHOLY_STRENGTH: "Unholy Strength"}


def off_centre(spot: tuple[float, float]) -> tuple[float, float]:
    return spot[0] - X, spot[1] - Y


# Per burrower: its path off the room centre, its sampled stretches, and its target. VB4 submerges from 34 s to
# 40 s and comes back up full where it went down.
VBURROWERS = {
    VB1: ([(30000, *off_centre(EXIT_S1)), (34000, -20.0, -14.0)], ((30000, 60000),), VELEMENA),
    VB2: ([(30000, *off_centre(EXIT_S1)), (35000, 17.0, 0.0)], ((30000, 60000),), VTREE),
    VB3: ([(40000, 5.0, -10.0), (42000, 17.0, 0.0)], ((40000, 60000),), VTREE),
    VB4: ([(10000, *off_centre(EXIT_N1)), (14000, 8.0, -5.0), (40000, 8.0, -5.0), (42000, 16.0, 10.0)],
          ((10000, 34000), (40000, 60000)), VPRAYER),
}


def behind_healer(guid: int, when: int) -> tuple[float, float]:
    """Tree walks behind the patches from 15 to 19 s, Prayer from 22 to 25 s; Elemena never moves."""
    if guid == VTREE:
        return lerp([(15000, -20.0, 0.0), (19000, 20.0, 0.0)], when)
    if guid == VPRAYER:
        return lerp([(22000, -20.0, 10.0), (25000, 20.0, 10.0)], when)
    return X - 20.0, Y - 10.0


def behind_snap(when: int) -> dict:
    units = [[VMAIN, X - 5.0, Y, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [VFELESTA, X + 5.0, Y + 25.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [VBULWARK, X + 5.0, Y - 25.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             *([guid, *behind_healer(guid, when), Z, 0.0, 0.0 if guid == VPRAYER and when >= 50000 else 100.0,
                100.0, 0, 0, 0, 0, 0] for guid in (VTREE, VPRAYER, VELEMENA)),
             [VBOSS, X, Y, Z, 0.0, 50.0 - max(when, 0) / 2000.0, 0.0, VMAIN, 0, 0, 0, 0]]
    for guid, (path, stretches, target) in VBURROWERS.items():
        if any(low <= when <= high for low, high in stretches):
            hp = 100.0 - (when - (40000 if guid == VB4 and when >= 40000 else path[0][0])) / 800.0
            units.append([guid, *lerp(path, when), Z, 0.0, hp, 0.0, target, 1, 0, 0, 0])
    hz = []
    for guid, spot in VPATCHES.items():
        units.append([guid, *spot, PATCH_Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        hz.append([PERMAFROST_25H, *spot, PATCH_Z, 6.0, 1])
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def behind_pull(new_rows: bool = True) -> list[dict]:
    """A 25H pull, phase 1 to 20 s and phase 3 after. Felesta and Bulwark cast area spells, a Death and Decay tick,
    one before the pull and the main tank's Swipe left out. Three healers set off behind the tank patches: Tree
    and Prayer reach their stands, Elemena never holds one. Felesta carries Penetrating Cold into phase 3 under
    Unholy Strength, which is no cover; Tree carries it with Renew then Power Word: Shield on it for 8 of 15 s;
    Prayer dies under it. Four burrowers reach a healer in phase 3: VB1 past no patch, VB2 through one, VB3 with
    only its walk from the exit through one, VB4 through one before it submerged and past none after it emerged.
    `new_rows` False drops anub.saoe, anub.hbehind, the area casts and Penetrating Cold."""
    records = [
        {"e": "hdr", "v": 14, "ts": 1790100000000, "map": ab.MAP_TOC, "inst": 21, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": VMAIN, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 1},
                    {"g": VFELESTA, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
                    {"g": VBULWARK, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": VTREE, "n": "Tree", "r": "heal", "c": 11, "h": 0},
                    {"g": VPRAYER, "n": "Prayer", "r": "heal", "c": 5, "h": 0},
                    {"g": VELEMENA, "n": "Elemena", "r": "heal", "c": 7, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": VBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"} for guid in VPATCHES),
        *({"t": 10000, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"}
          for guid in VBURROWERS),
        *({"e": "spell", "sp": spell, "n": name} for spell, name in VSPELLS.items()),
        *(note(when, VMAIN, "anub.phase", value) for when, value in ((0, 1), (20000, 3))),
        note(0, VMAIN, "anub.side0", VFELESTA),
        note(0, VMAIN, "anub.side1", VBULWARK),
        note(0, VFELESTA, "anub.patch0", VTP0),
        note(0, VBULWARK, "anub.patch1", VTP1),

        cast(-1000, VFELESTA, SPELL_DND, 0),
        cast(2000, VFELESTA, SPELL_DND, 0),
        triggered(3000, VFELESTA, SPELL_DND_TICK, VB4),
        cast(8000, VFELESTA, SPELL_DND, 0),
        cast(9000, VFELESTA, SPELL_PESTILENCE, VB4),
        cast(30000, VFELESTA, SPELL_BLOOD_BOIL, 0),
        cast(4000, VBULWARK, SPELL_CONSECRATION, 0),
        cast(5000, VBULWARK, SPELL_JUDGEMENT, VBOSS),
        cast(12000, VBULWARK, SPELL_CONSECRATION, 0),
        cast(6000, VMAIN, SPELL_SWIPE, 0),
        *(note(when, VFELESTA, "anub.saoe", value) for when, value in (
            (1000, "death and decay"), (3000, "none"), (5000, "cap"), (7000, "death and decay"))),
        *(note(when, VBULWARK, "anub.saoe", value) for when, value in ((2000, "consecration"), (4000, "other"))),

        *(note(when, guid, "anub.hbehind", value) for when, guid, value in (
            (12000, VTREE, "wait"), (15000, VTREE, "go"), (15000, VPRAYER, "wait"), (22000, VPRAYER, "go"),
            (15000, VELEMENA, "wait"), (26000, VELEMENA, "go"))),
        *(note(when, guid, "anub.hstand", value) for when, guid, value in (
            (0, VTREE, "hold"), (15000, VTREE, "walk"), (19000, VTREE, "hold"),
            (0, VPRAYER, "hold"), (22000, VPRAYER, "walk"), (25000, VPRAYER, "hold"),
            (0, VELEMENA, "hold"), (26000, VELEMENA, "walk"))),

        aura(10000, VBULWARK, COLD_25H, False),
        aura(15000, VBULWARK, COLD_25H, True),
        aura(18000, VFELESTA, COLD_25H, False),
        aura(19000, VFELESTA, SPELL_UNHOLY_STRENGTH, False, VFELESTA),
        {"t": 19000, "e": "heal", "s": VFELESTA, "d": VFELESTA, "sp": SPELL_UNHOLY_STRENGTH, "a": 500, "oh": 0},
        aura(23000, VFELESTA, COLD_25H, True),
        aura(25000, VTREE, COLD_25H, False),
        aura(30000, VTREE, SPELL_RENEW, False, VPRAYER),
        aura(33000, VTREE, SPELL_PWS, False, VPRAYER),
        {"t": 34000, "e": "abs", "d": VTREE, "s": VPRAYER, "sp": SPELL_PWS, "a": 2000},
        aura(35000, VTREE, SPELL_RENEW, True, VPRAYER),
        aura(38000, VTREE, SPELL_PWS, True, VPRAYER),
        aura(40000, VTREE, COLD_25H, True),
        aura(45000, VPRAYER, COLD_25H, False),
        aura(50000, VPRAYER, COLD_25H, True),
        {"t": 50000, "e": "death", "g": VPRAYER, "killer": VBOSS, "x": X + 20.0, "y": Y + 10.0, "z": Z},
        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    if not new_rows:
        dropped = {SPELL_DND, SPELL_DND_TICK, SPELL_PESTILENCE, SPELL_BLOOD_BOIL, SPELL_CONSECRATION, COLD_25H}
        records = [rec for rec in records if rec.get("k") not in ("anub.saoe", "anub.hbehind")
                   and not (rec.get("e") in ("cast", "aura") and rec.get("sp") in dropped)]
    records += [behind_snap(when) for when in range(-500, 60000, 500)]
    return records


class BehindPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_21_anub-arak_1790100000.ndjson", behind_pull())
        cls.bare = write_trace(cls.folder.name, "649_21_anub-arak_1790100001.ndjson", behind_pull(False))

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, trace, *sections) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            for section in sections:
                section(trace)
        return out.getvalue()

    def test_side_tank_area_casts_per_surfaced_phase(self):
        self.assertEqual({key: dict(counts) for key, counts in ab.side_tank_area_casts(self.trace).items()},
                         {(VFELESTA, "P1"): {"Death and Decay": 2, "Pestilence": 1},
                          (VFELESTA, "P3"): {"Blood Boil": 1}, (VBULWARK, "P1"): {"Consecration": 2}})
        self.assertEqual(ab.side_tank_area_casts(self.bare), {})

    def test_the_tank_section_prints_the_area_casts_and_anub_saoe(self):
        self.assertIn("  side tank area casts per surfaced phase, anub.saoe's spells, triggered ones left out:\n"
                      "    Felesta        P1    Death and Decay 2, Pestilence 1\n"
                      "    Felesta        P3    Blood Boil 1\n"
                      "    Bulwark        P1    Consecration 2\n"
                      "  anub.saoe: Bulwark consecration 1, other 1; Felesta death and decay 2, none 1, cap 1\n",
                      self.text(self.trace, ab.show_tanks))
        self.assertIn("  side tank area casts per surfaced phase, anub.saoe's spells, triggered ones left out: none\n"
                      "  anub.saoe: no rows\n", self.text(self.bare, ab.show_tanks))

    def test_a_carriers_bare_seconds_leave_out_a_hot_a_shield_and_what_is_neither(self):
        self.assertEqual(ab.cold_bare(self.trace, 20000, 60000),
                         [{"guid": VFELESTA, "start": 20000, "carried": 3000, "bare": 3000, "died": False},
                          {"guid": VTREE, "start": 25000, "carried": 15000, "bare": 7000, "died": False},
                          {"guid": VPRAYER, "start": 45000, "carried": 5000, "bare": 5000, "died": True}])
        self.assertEqual(ab.cover_spans(self.trace), {VTREE: [(30000, 38000)]})
        self.assertEqual(ab.cold_bare(self.bare, 20000, 60000), [])

    def test_the_swarm_section_prints_the_bare_seconds(self):
        line = "  Penetrating Cold carriers in phase 3, seconds with no HoT or shield on them of those carried: "
        self.assertIn(line + "Felesta 0:20.000 3.0 of 3.0 s, Tree 0:25.000 7.0 of 15.0 s, Prayer 0:45.000 5.0 of"
                      " 5.0 s died\n", self.text(self.trace, ab.show_swarm))
        self.assertIn(line + "none\n", self.text(self.bare, ab.show_swarm))

    def test_each_healers_walk_behind_the_patches_to_standing_still(self):
        self.assertEqual([(row["guid"], row["go"], row["arrived"]) for row in ab.behind_walks(self.trace)],
                         [(VELEMENA, 26000, None), (VPRAYER, 22000, 25000), (VTREE, 15000, 19000)])
        self.assertEqual(ab.behind_walks(self.bare), [])

    def test_a_burrower_reaching_a_healer_reads_whether_its_path_crossed_a_patch(self):
        self.assertEqual([(row["guid"], row["t"], row["healer"], row["emerged"], row["crossed"])
                          for row in ab.healer_reaches(self.trace)],
                         [(VB1, 34000, VELEMENA, False, False), (VB2, 35000, VTREE, False, True),
                          (VB3, 42000, VTREE, False, True), (VB4, 42000, VPRAYER, True, False)])

    def test_the_healer_section_prints_them(self):
        text = self.text(self.trace, ab.show_healers)
        self.assertIn("  anub.hbehind, each healer's first go and the seconds until it stood still at its stand,"
                      " anub.hstand hold and\n  under 1.5 yd in 1 s: Elemena 0:26.000 never, Prayer 0:22.000 3.0 s,"
                      " Tree 0:15.000 4.0 s\n", text)
        self.assertIn("  burrowers within 5 yd of a healer in phase 3: 4; with no step from the exit or the emerge\n"
                      "  within 7.5 yd of a Permafrost centre: #1921 Elemena 0:34.000 on it, #1924 Prayer 0:42.000"
                      " after an emerge on it\n", text)
        self.assertIn("  under 1.5 yd in 1 s: none\n", self.text(self.bare, ab.show_healers))

    def test_the_new_keys_count_as_present_and_absent_without_their_rows(self):
        for key in ("anub.saoe", "anub.hbehind"):
            self.assertNotIn(key, ab.missing_probes(self.trace))
            self.assertIn(key, ab.missing_probes(self.bare))

    def test_every_section_prints(self):
        self.assertIn("TANKS\n", self.text(self.trace, ab.show_banner, *(section for _, _, section in ab.SECTIONS)))


HMAIN, HSIDE, HHOLY, HPRAYER, HSHAMAN, HMAGE, HROGUE, HJUSTICE = range(9901, 9909)
HBOSS = CREATURE | 2000
HTOTEM = CREATURE | 2001
HSTRAY = CREATURE | 2002  # heals with no unit row, so no owner
HBUR = CREATURE | 2021
HPOOLS = {HMAIN: 54000, HSIDE: 51000, HHOLY: 25000, HPRAYER: 25000, HSHAMAN: 24000, HMAGE: 20000, HROGUE: 28000,
          HJUSTICE: 30000}
SPELL_HOLY_LIGHT, SPELL_HEALING_WAVE, SPELL_HEALING_STREAM, SPELL_RECKONING = 48782, 49273, 52042, 62124


def cold_hp(guid: int, when: int) -> float:
    if when < 20000:
        return 100.0
    if guid == HMAGE:
        return 0.0 if when > 28001 else 15.0
    if guid == HROGUE:
        return 0.0 if when > 27000 else 10.0
    return {HMAIN: 50.0, HSIDE: 100.0, HHOLY: 20.0, HPRAYER: 8.0, HSHAMAN: 20.0, HJUSTICE: 10.0}[guid]


def cold_snap(when: int) -> dict:
    """HBUR is on Bulwark from 2 s, on Power from 5 to 7 s, back on Bulwark, then on Assasin from 10 s to its last
    sample at 12 s."""
    units = [[guid, X + index, Y - index, Z, 0.0, cold_hp(guid, when), 100.0, 0, 0, 0, 0, 0]
             for index, guid in enumerate(HPOOLS)]
    units.append([HBOSS, X, Y, Z, 0.0, 80.0, 0.0, HMAIN, 0, 0, 0, 0])
    if 2000 <= when <= 12000:
        target = HMAGE if 5000 <= when < 7000 else HROGUE if when >= 10000 else HSIDE
        units.append([HBUR, X + 10.0, Y + 20.0, Z, 0.0, 100.0 - (when - 2000) / 1000.0, 0.0, target, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": []}


def heal_row(when: int, source: int, target: int, spell: int, amount: int, over: int = 0) -> dict:
    return {"t": when, "e": "heal", "s": source, "d": target, "sp": spell, "a": amount, "oh": over}


def absorb_row(when: int, source: int, target: int, amount: int) -> dict:
    return {"t": when, "e": "abs", "d": target, "s": source, "sp": SPELL_PWS, "a": amount}


def cold_tick(when: int, target: int, amount: int, resisted: int, hp: float, absorbed: int = 0,
              overkill: int = 0) -> dict:
    return {**dmg(when, HBOSS, target, COLD_25H, amount), "ab": absorbed, "rs": resisted, "ok": overkill, "hp": hp}


def cold_pull(new_rows: bool = True) -> list[dict]:
    """A 25H pull, phase 1 to 20 s and phase 3 to 50 s. In phase 1 a burrower comes off Bulwark to Power, is taunted
    back, and goes to Assasin to its last sample with a Righteous Defense that never took it; Holylight heals, the
    shaman's totem heals, Prayer shields and a source with no owner heals. Holylight takes frost from Justice as
    phase 3 starts. Penetrating Cold lands on Justice, Assasin and Power at 25 s: Holylight and Elemena both heal
    Justice, Prayer's carrier, and it lives; Assasin dies before the tick and Power at it with nothing from a healer;
    Holylight's Aura Mastery covers the first two ticks. At 43 s it lands on Prayer, kept up by itself under its own
    shield. `new_rows` False drops anub.healduty, anub.coldheal, anub.mastery and toc.aura."""
    roster = ((HMAIN, "Ecoterrorist", "tank", 11), (HSIDE, "Bulwark", "tank", 2), (HHOLY, "Holylight", "heal", 2),
              (HPRAYER, "Prayer", "heal", 5), (HSHAMAN, "Elemena", "heal", 7), (HMAGE, "Power", "ranged", 8),
              (HROGUE, "Assasin", "melee", 4), (HJUSTICE, "Justice", "melee", 2))
    records = [
        {"e": "hdr", "v": 14, "ts": 1790200000000, "map": ab.MAP_TOC, "inst": 22, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": guid, "n": name, "r": role, "c": klass, "h": 0} for guid, name, role, klass in roster]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": HBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 2000, "e": "unit", "g": HBUR, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        {"t": 0, "e": "unit", "g": HTOTEM, "en": 3527, "n": "Healing Stream Totem", "own": HSHAMAN},
        *({"t": -500, "e": "unit", "g": guid, "n": name, "r": role, "c": klass, "mhp": HPOOLS[guid]}
          for guid, name, role, klass in roster),
        *(note(when, HMAIN, "anub.phase", value) for when, value in ((0, 1), (20000, 3))),
        note(0, HMAIN, "anub.side0", HSIDE),
        cast(6800, HSIDE, SPELL_RECKONING, HBUR),
        triggered(11000, HSIDE, ab.SPELL_RIGHTEOUS_DEFENSE_TAUNT, HBUR),

        heal_row(10000, HHOLY, HMAIN, SPELL_HOLY_LIGHT, 10000, 2000),
        heal_row(11000, HTOTEM, HMAIN, SPELL_HEALING_STREAM, 1000),
        absorb_row(12000, HPRAYER, HMAIN, 3000),
        heal_row(13000, HSTRAY, HMAIN, SPELL_HOLY_LIGHT, 500),

        *(note(when, guid, "toc.aura", value) for when, guid, value in (
            (0, HJUSTICE, "frost"), (0, HHOLY, "none"), (20000, HHOLY, "frost"), (20000, HJUSTICE, "none"))),
        *(note(when, guid, "anub.healduty", value) for when, guid, value in (
            (20000, HHOLY, 2), (20000, HSHAMAN, 2), (20000, HPRAYER, 1), (40000, HSHAMAN, 1))),
        *(note(when, HPRAYER, "anub.coldheal", value) for when, value in (
            (25100, HJUSTICE), (27600, 0), (43000, HPRAYER), (44000, 0))),
        *(note(when, HHOLY, "anub.mastery", value) for when, value in (
            (25100, "hold 1"), (25600, "cast"), (26600, "none"))),
        note(25100, HPRAYER, "anub.swarmheal", "risk"),
        note(43000, HPRAYER, "anub.swarmheal", "self"),

        aura(25000, HJUSTICE, COLD_25H, False),
        aura(25001, HROGUE, COLD_25H, False),
        aura(25002, HMAGE, COLD_25H, False),
        {**dmg(25001, HBOSS, HMAGE, ab.SPELL_LEECHING_SWARM_DMG, 1000), "hp": 15.0},
        aura(25600, HHOLY, ab.SPELL_AURA_MASTERY, False, HHOLY),
        heal_row(26000, HHOLY, HJUSTICE, SPELL_HOLY_LIGHT, 16000),
        heal_row(26500, HSHAMAN, HJUSTICE, SPELL_HEALING_WAVE, 8000, 1000),
        {"t": 27000, "e": "death", "g": HROGUE, "killer": HBUR, "x": X, "y": Y, "z": Z},
        aura(27000, HROGUE, COLD_25H, True),
        absorb_row(27500, HPRAYER, HJUSTICE, 2000),
        cold_tick(28000, HJUSTICE, 4656, 1164, 60.0),
        cold_tick(28001, HMAGE, 4000, 1820, 12.5, overkill=1500),
        {"t": 28001, "e": "death", "g": HMAGE, "killer": HBOSS, "x": X, "y": Y, "z": Z},
        aura(28001, HMAGE, COLD_25H, True),
        cold_tick(31000, HJUSTICE, 2910, 2910, 70.0),
        aura(31600, HHOLY, ab.SPELL_AURA_MASTERY, True, HHOLY),
        cold_tick(34000, HJUSTICE, 4656, 1164, 80.0),
        aura(43000, HJUSTICE, COLD_25H, True),

        aura(43000, HPRAYER, COLD_25H, False),
        absorb_row(43500, HPRAYER, HPRAYER, 3000),
        cold_tick(46000, HPRAYER, 0, 1164, 8.0, absorbed=4656),
        {"t": 50000, "e": "end", "out": "wipe"},
    ]
    if not new_rows:
        records = [rec for rec in records
                   if rec.get("k") not in ("anub.healduty", "anub.coldheal", "anub.mastery", "toc.aura")]
    records += [cold_snap(when) for when in range(-500, 50000, 500)]
    return records


class ColdPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_22_anub-arak_1790200000.ndjson", cold_pull())
        cls.bare = write_trace(cls.folder.name, "649_22_anub-arak_1790200001.ndjson", cold_pull(False))

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, trace, *sections) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            for section in sections:
                section(trace)
        return out.getvalue()

    def test_each_wave_reads_its_carriers_at_the_landing_and_the_first_tick(self):
        first, second = ab.cold_waves(self.trace)
        self.assertEqual((first["t"], first["rel"], second["t"]), (25000, 5000, 43000))
        rows = {row["guid"]: row for row in first["carriers"]}
        self.assertEqual(list(rows), [HJUSTICE, HROGUE, HMAGE])
        justice, rogue, mage = rows[HJUSTICE], rows[HROGUE], rows[HMAGE]
        self.assertEqual((justice["pct"], justice["health"], justice["tick"], justice["amount"], justice["margin"],
                          justice["fate"], justice["risk"]), (10.0, 3000.0, 28000, 5820, 13344, "survived", True))
        self.assertEqual((dict(justice["heals"]), dict(justice["absorbs"]), justice["helped"], justice["coldheal"]),
                         ({HHOLY: 16000, HSHAMAN: 7000}, {HPRAYER: 2000}, True, [HPRAYER]))
        # Power's swarm tick lands after its last snapshot, so the tick sets its health at the landing
        self.assertEqual((mage["pct"], mage["margin"], mage["fate"], mage["risk"], mage["helped"]),
                         (10.0, -1500, "died", True, False))
        self.assertEqual((rogue["tick"], rogue["amount"], rogue["margin"], rogue["fate"], rogue["risk"]),
                         (None, None, None, "before", True))
        [prayer] = second["carriers"]
        self.assertEqual((prayer["margin"], prayer["fate"], prayer["coldheal"], dict(prayer["absorbs"])),
                         (2000, "survived", [HPRAYER], {HPRAYER: 3000}))

    def test_each_wave_reads_frost_aura_mastery_its_rows_and_the_resists_per_tick(self):
        first, second = ab.cold_waves(self.trace)
        self.assertEqual((first["frost"], first["mastery"]), ([HHOLY], {1: [HHOLY], 2: [HHOLY], 3: []}))
        self.assertEqual(first["resists"], {1: [2984, 11640], 2: [2910, 5820], 3: [1164, 5820]})
        self.assertEqual(first["rows"], [(25100, HHOLY, "hold 1"), (25600, HHOLY, "cast"), (26600, HHOLY, "none")])
        self.assertEqual((second["mastery"], second["rows"]), ({1: [], 2: []}, []))
        bare = ab.cold_waves(self.bare)[0]
        self.assertEqual((bare["frost"], bare["rows"], bare["carriers"][0]["coldheal"]), (None, [], []))

    def test_a_herd_is_two_healers_on_one_carrier_while_one_at_risk_got_nothing(self):
        first, second = ab.cold_waves(self.trace)
        self.assertEqual(first["herds"], [{"carrier": HJUSTICE, "start": 26000, "stop": 26500,
                                           "healers": [HSHAMAN, HHOLY], "starved": [HROGUE, HMAGE]}])
        self.assertEqual(second["herds"], [])
        carriers = [{**row, "risk": False} if row["guid"] != HJUSTICE else row for row in first["carriers"]]
        self.assertEqual(ab.wave_herds(self.trace, carriers, ab.rows_by_target(self.trace, "heal"),
                                       {HHOLY, HPRAYER, HSHAMAN}), [])

    def test_the_swarm_section_prints_the_waves_and_the_heal_duties(self):
        text = self.text(self.trace, ab.show_swarm)
        self.assertIn("  anub.swarmheal: Prayer risk 1, self 1\n", text)
        self.assertIn("  anub.healduty, each healer bot's duty from phase 3: Elemena tank 0:20.000 > cold 0:40.000;"
                      " Holylight tank 0:20.000; Prayer cold 0:20.000\n"
                      "  anub.coldheal, the carrier at risk each healer bot kept up, self for itself:\n"
                      "    Prayer         Justice 0:25.100-0:27.600, self 0:43.000-0:44.000\n", text)
        self.assertIn("  first ticks survived per phase 3 Penetrating Cold wave: 1 of 3, 1 of 1 (2 of 4)\n", text)
        self.assertIn("    0:25.000 (+5.0 s): 1 of 3 through the first tick; frost Holylight; Aura Mastery tick 1"
                      " Holylight, tick 2 Holylight\n"
                      "      anub.mastery Holylight hold 1 +0.1 s, Holylight cast +0.6 s, Holylight none +1.6 s;"
                      " resisted per tick 26%* 50%* 20%\n"
                      "      Justice        melee    3.0k  10.0% risk +13.3k survived       coldheal Prayer; Holylight"
                      " 16.0k, Elemena 7.0k, Prayer abs 2.0k\n"
                      "      Assasin        melee    2.8k  10.0% risk      - died before it coldheal none; none\n"
                      "      Power          ranged   2.0k  10.0% risk  -1.5k died           coldheal none; none\n"
                      "      herd: Elemena, Holylight on Justice 0:26.000-0:26.500; at risk with nothing from a healer:"
                      " Assasin, Power\n", text)
        self.assertIn("    0:43.000 (+23.0 s): 1 of 1 through the first tick; frost Holylight; Aura Mastery tick 1 no,"
                      " tick 2 no\n      anub.mastery none; resisted per tick 20%\n"
                      "      Prayer         heal     2.0k   8.0% risk  +2.0k survived       coldheal self; Prayer abs"
                      " 3.0k\n", text)

    def test_the_swarm_section_reads_the_externals_and_the_tank_dips(self):
        # Bulwark at 30% from 30 s, Lay on Hands at 31 s, full from 33 s; at 20% from 40 s and dead at 41 s.
        # Prayer dies 4 s after its Pain Suppression.
        records = cold_pull() + [cast(31000, HHOLY, 48788, HSIDE), cast(32000, HPRAYER, 33206, HJUSTICE),
                                 {"t": 36000, "e": "death", "g": HPRAYER, "killer": HBOSS, "x": 0.0, "y": 0.0,
                                  "z": 0.0}]
        for rec in records:
            if rec.get("e") == "snap":
                when = rec["t"]
                for row in rec["u"]:
                    if row[0] == HSIDE:
                        row[5] = (30.0 if 30000 <= when < 33000 else 20.0 if 40000 <= when < 41000
                                  else 0.0 if when >= 41000 else row[5])
        trace = write_trace(self.folder.name, "649_22_anub-arak_1790200002.ndjson", records)
        text = self.text(trace, ab.show_swarm)
        self.assertIn("  phase 3 externals, the target's health at the cast, the caster dead within 10 s flagged:"
                      " 0:31.000 Holylight Lay on Hands > Bulwark (tank) 30%, 0:32.000 Prayer Pain Suppression >"
                      " Justice (melee) 10% (caster dead +4.0 s)\n", text)
        self.assertIn("  tank dips under 35% in phase 3: 2; an external onto it in 1: Bulwark 0:30.000 Lay on Hands"
                      " +1.0 s; none in 1 (Bulwark 1); died with none: Bulwark 0:41.000\n", text)
        self.assertIn("  tank dips under 35% in phase 3: 0; an external onto it in 0; none in 0; died with none: none\n",
                      self.text(self.trace, ab.show_swarm))

    def test_the_swarm_section_reads_a_trace_without_the_new_keys(self):
        text = self.text(self.bare, ab.show_swarm)
        self.assertIn("  anub.healduty: no rows\n  anub.coldheal: no rows\n", text)
        self.assertIn("rows, the resisted share per tick (* under Aura Mastery) and herds; anub.coldheal: no rows\n"
                      "    0:25.000 (+5.0 s): 1 of 3 through the first tick; toc.aura: no rows; Aura Mastery tick 1"
                      " Holylight, tick 2 Holylight\n      anub.mastery no rows; resisted per tick 26%* 50%* 20%\n"
                      "      Justice        melee    3.0k  10.0% risk +13.3k survived       Holylight 16.0k, Elemena"
                      " 7.0k, Prayer abs 2.0k\n", text)

    def test_healing_done_per_phase_folds_totems_and_counts_absorbs_for_the_shields_caster(self):
        first, third = ab.healing_done(self.trace)
        self.assertEqual((first["label"], first["ms"], third["label"], third["ms"]), ("P1", 20000, "P3", 30000))
        self.assertEqual(dict(first["sources"]), {HHOLY: [8000, 0, 2000], HSHAMAN: [1000, 0, 0],
                                                  HPRAYER: [0, 3000, 0], HSTRAY: [500, 0, 0]})
        self.assertEqual(first["unowned"], {HSTRAY})
        self.assertEqual(dict(third["sources"]), {HHOLY: [16000, 0, 0], HSHAMAN: [7000, 0, 1000],
                                                  HPRAYER: [0, 5000, 0]})
        self.assertEqual(third["unowned"], set())

    def test_the_healer_section_prints_the_healing_table(self):
        text = self.text(self.trace, ab.show_healers)
        self.assertIn("    P1 20.0 s, raid 0.6k HPS\n"
                      "      source         role     healing  absorbs  over    HPS share\n"
                      "      Holylight      heal          8k       0k   20%   0.4k   64%\n"
                      "      Prayer         heal          3k       3k    0%   0.1k   24%\n"
                      "      Elemena        heal          1k       0k    0%   0.1k    8%\n"
                      "      #1:2002        ?             0k       0k    0%   0.0k    4%\n"
                      "      sources the trace names no owner for, kept as their own: #1:2002\n"
                      "    P3 30.0 s, raid 0.9k HPS\n", text)
        self.assertIn("      Elemena        heal          7k       0k   12%   0.2k   25%\n", text)

    def test_a_pulled_burrower_reads_how_long_until_a_tank_had_it_back(self):
        samples = ab.Samples(self.trace)
        first, second = ab.pull_returns(self.trace, ab.burrower_pulls(self.trace, samples), samples)
        self.assertEqual((first["t"], first["puller"], first["back"], first["on_tank"]), (5000, HMAGE, 7000, HSIDE))
        self.assertEqual([(rec["t"], rec["sp"]) for rec in first["taunts"]], [(6800, SPELL_RECKONING)])
        self.assertEqual((second["t"], second["puller"], second["back"], second["last"], second["under"]),
                         (10000, HROGUE, None, 12000, False))
        self.assertEqual([rec["t"] for rec in second["taunts"]], [11000])

    def test_the_burrower_section_prints_each_way_back(self):
        text = self.text(self.trace, ab.show_burrowers)
        self.assertIn("      back on Bulwark after 2.0 s, taunted: Hand of Reckoning by Bulwark +1.8 s\n", text)
        self.assertIn("      never back on a tank, on non-tanks 2.0 s to its last sample; taunts at it: Righteous"
                      " Defense by Bulwark +1.0 s\n", text)

    def test_the_new_keys_count_as_present_and_absent_without_their_rows(self):
        for key in ("anub.healduty", "anub.coldheal", "anub.mastery", "toc.aura"):
            self.assertNotIn(key, ab.missing_probes(self.trace))
            self.assertIn(key, ab.missing_probes(self.bare))

    def test_every_section_prints(self):
        for trace in (self.trace, self.bare):
            self.assertIn("TANKS\n", self.text(trace, ab.show_banner, *(section for _, _, section in ab.SECTIONS)))


def kite_patch(x: float, y: float, tank: bool) -> tuple:
    return (x, y, tank, not tank, False)


# anubarak_planner_test.cpp's FirstKitesEndLate starts, 2026-10-31 BH, BI and BJ at 1:22
FIRST_KITES = [
    ((774.2, 140.7), (763.5, 177.7), (775.1, 136.6),
     [kite_patch(787.5, 120.7, True), kite_patch(796.6, 140.6, True), kite_patch(748.1, 155.3, False),
      kite_patch(747.0, 113.2, False)]),
    ((764.4, 140.8), (736.9, 168.5), (767.8, 141.0),
     [kite_patch(775.8, 120.2, True), kite_patch(789.7, 142.5, True), kite_patch(752.8, 131.8, False),
      kite_patch(721.2, 115.7, False)]),
    ((775.5, 134.1), (762.6, 170.9), (776.2, 136.5),
     [kite_patch(785.6, 116.2, True), kite_patch(798.7, 136.5, True), kite_patch(761.5, 150.2, False),
      kite_patch(748.1, 114.0, False)]),
]


def fresh_world(kiter, spike, gather, patches, life: float = 59.5, own: bool = False) -> ab.KiteWorld:
    return ab.KiteWorld(kiter, (spike[0], spike[1], ab.SPIKE_SPEEDS[0], 0, ab.SPIKE_STEP_S), life, patches, gather,
                        own=own)


def top_speed(world: ab.KiteWorld) -> ab.KiteWorld:
    x, y, _, _, due = world.spike
    return ab.KiteWorld(world.kiter, (x, y, ab.SPIKE_SPEEDS[-1], ab.SPIKE_LAST_STEP, due), world.life, world.patches,
                        world.gather, own=world.own)


def straight(world: ab.KiteWorld, heading, rules: dict, own: bool = False) -> ab.PlanCandidate:
    """anubarak_planner_test.cpp's Straight: the kiter walking on along `heading` to the plan's end."""
    out = ab.PlanCandidate("arc")
    out.own, out.ending, out.track = own, "own" if own else "none", [world.kiter]
    ab.planner_walk(world, rules, out.track, ab.planner_ticks(world, rules), ab.arc_mover(heading, 0.0, 0.0, rules))
    return out


def into(world: ab.KiteWorld, patch: int, rules: dict) -> ab.PlanCandidate:
    """Into: the kiter walking straight onto the patch and standing there."""
    out = ab.PlanCandidate("arc")
    out.patch, out.turn_tick, out.ending, out.track = patch, 0, "patch", [world.kiter]
    ab.planner_walk(world, rules, out.track, ab.planner_ticks(world, rules),
                    ab.leg_mover([world.patches[patch][:2]]))
    return out


def outcome(**fields) -> ab.PlanOutcome:
    out = ab.PlanOutcome()
    base = {"ending": "none", "end_tick": 150, "tank": -1, "kite": -1, "raid": -1, "caught": False}
    for name, value in {**base, **fields}.items():
        setattr(out, name, value)
    return out


class PlannerPort(unittest.TestCase):
    """The kite planner's port against anubarak_planner_test.cpp's cases that need no Hand."""

    rules = ab.planner_rules()

    def test_the_rules_come_off_the_planner_header(self):
        self.assertTrue(set(ab.PLANNER_FIELDS) <= set(self.rules))
        self.assertEqual(len(self.rules["arcTurns"]), 7)
        self.assertEqual(len(self.rules["testS"]), ab.SPIKE_LAST_STEP + 1)
        self.assertEqual(len(self.rules["centre"]), 2)

    def test_a_header_missing_a_rule_is_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            header = pathlib.Path(folder) / "ToCAnubarakKitePlanner.h"
            header.write_text("struct Rules\n{\n    // float ring = 40.0f;\n    float rim = 45.0f;\n};\n",
                              encoding="utf-8")
            ab.planner_rules.cache_clear()
            try:
                with unittest.mock.patch.object(ab, "KITE_PLANNER_HEADER", header):
                    with self.assertRaisesRegex(LookupError, r"Rules lacks centre, floorRadius, ring, headings"):
                        ab.planner_rules()
            finally:
                ab.planner_rules.cache_clear()

    def test_a_first_kite_runs_to_a_late_patch(self):
        # with no Hand to plan, BI's and BJ's patch ends pass the raid on the way
        for (kiter, spike, gather, patches), raid_free in zip(FIRST_KITES, (True, False, False)):
            world = fresh_world(kiter, spike, gather, patches)
            candidate, out, kept = ab.plan_kite(world, self.rules)
            self.assertFalse(kept)
            self.assertEqual((out.tank, out.kite, out.caught, out.ending), (-1, -1, False, "patch"))
            self.assertTrue(out.raid < 0 or not raid_free)
            self.assertGreaterEqual(out.end_s, 12.0)
            self.assertFalse(patches[candidate.patch][2])
            played = ab.kitesim_play(world, self.rules)
            self.assertEqual((played["end"], played["planned"]), ("patch", True))
            self.assertTrue(played["raid"] is None or not raid_free)
            self.assertGreaterEqual(played["s"], 12.0)

    def test_a_spike_restarted_by_a_tank_patch_is_a_tank_hazard_unless_led_out(self):
        patches = [kite_patch(760.0, 135.0, True)]
        _, trapped, _ = ab.plan_kite(fresh_world((735.0, 135.0), (771.0, 135.0), None, patches), self.rules)
        self.assertGreaterEqual(trapped.tank, 0)
        _, clear, _ = ab.plan_kite(fresh_world((785.0, 135.0), (771.0, 135.0), None, patches), self.rules)
        self.assertEqual(clear.tank, -1)

    def test_off_the_rim_no_pick_turns_back_within_a_second(self):
        world = fresh_world((790.0, 135.0), (760.0, 135.0), None, [])
        held, age, last, since = None, 0.0, (0.0, 0.0), 10.0
        for step in range(16):
            candidate, _, kept = ab.plan_kite(world, self.rules, held, age, 0.3 if held else 0.0)
            heading = ab.track_heading(candidate.track)
            turn = ab.turn_degrees(last, heading)
            if step and turn > 90.0:
                self.assertGreaterEqual(since, 1.0 - 1e-3)
            since = 0.0 if turn > 90.0 else since + 0.3
            last, age, held = heading, age + 0.3 if kept else 0.3, candidate
            spike, kiter = world.spike, world.kiter
            for index in range(3):
                spike = ab.chase_tick(spike, kiter[0], kiter[1], index * ab.CATCH_TICK_S)
                kiter = candidate.track[index + 1]
            world = world.moved(kiter, spike, 0.3)

    def test_kitepred_names_the_worst_hazard_first_else_a_catch_else_the_ending(self):
        self.assertEqual(ab.kitepred_text(outcome(kite=42, raid=12, caught=True)), "kite:4.2")
        self.assertEqual(ab.kitepred_text(outcome(raid=30, caught=True)), "raid:3.0")
        self.assertEqual(ab.kitepred_text(outcome(caught=True, end_tick=153)), "catch:15.3")
        self.assertEqual(ab.kitepred_text(outcome(ending="patch", end_tick=141)), "patch:14.1")

    def test_the_spike_runs_its_speed_steps_out_over_the_life_left(self):
        base = (0.0, 0.0, ab.SPIKE_SPEEDS[0], 0, ab.SPIKE_STEP_S)
        self.assertAlmostEqual(ab.run_length(base, 0.0, 10.0), 3.5 * 7.0 + ab.SPIKE_SPEEDS[1] * 3.0)
        self.assertAlmostEqual(ab.run_length(base, 2.0, 5.0), 3.5 * 5.0)
        top = (0.0, 0.0, ab.SPIKE_SPEEDS[2], 2, 0.0)
        self.assertAlmostEqual(ab.run_length(top, 30.0, 4.0), ab.SPIKE_SPEEDS[2] * 4.0)

    def test_a_patch_end_whose_restart_passes_another_kite_patch_is_a_kite_hazard(self):
        def world(patches):
            return ab.KiteWorld((735.0, 135.0), (735.0, 165.0, ab.SPIKE_SPEEDS[2], 2, ab.SPIKE_STEP_S), 59.5,
                                patches, (790.0, 135.0))

        alone = world([kite_patch(720.0, 135.0, False)])
        walk = ab.PlanCandidate("arc")
        walk.patch, walk.turn_tick, walk.ending = 0, 0, "patch"
        walk.track = [(735.0 - min(0.7 * tick, 15.0), 135.0) for tick in range(ab.planner_ticks(alone, self.rules) + 1)]
        clean = ab.judge(walk, alone, self.rules)
        self.assertEqual((clean.ending, clean.tank, clean.kite, clean.raid), ("patch", -1, -1, -1))
        crossed = ab.judge(walk, world([kite_patch(720.0, 135.0, False), kite_patch(760.0, 138.0, False)]), self.rules)
        self.assertEqual((crossed.ending, crossed.tank, crossed.kite), ("patch", -1, crossed.end_tick))

    def test_an_own_immunity_costs_more_mid_life_passes_the_raid_at_once_and_its_leg_past_a_tank_patch_counts(self):
        spike = (765.0, 135.0, ab.SPIKE_SPEEDS[2], 2, ab.SPIKE_STEP_S)
        mid = ab.KiteWorld((760.0, 135.0), spike, 40.0, [kite_patch(790.0, 145.0, True)], (800.0, 135.0))
        out = ab.planner_finish(mid, self.rules, ab.planner_ticks(mid, self.rules), 20, (758.0, 135.0), 5.0, -1, spike,
                                own_used=True)
        self.assertEqual((out.ending, out.caught, out.cost, out.tank, out.kite, out.raid),
                         ("own", False, self.rules["costOwnMid"], 20, -1, 0))
        self.assertAlmostEqual(out.end_gather, 42.0)
        late = ab.KiteWorld((760.0, 135.0), spike, 6.0, [], (800.0, 135.0))
        out = ab.planner_finish(late, self.rules, ab.planner_ticks(late, self.rules), 20, (758.0, 135.0), 5.0, -1,
                                spike, own_used=True)
        self.assertEqual((out.ending, out.cost, out.tank, out.raid), ("own", self.rules["costOwn"], -1, 0))

    def test_a_catch_or_immunity_passes_the_raid_at_once_only_when_its_spike_reaches_the_gather_unspent(self):
        base = (765.0, 135.0, ab.SPIKE_SPEEDS[0], 0, ab.SPIKE_STEP_S)
        def finish(life: float, patches=(), **ending) -> ab.PlanOutcome:
            world = ab.KiteWorld((760.0, 135.0), base, life, patches, (800.0, 135.0))
            return ab.planner_finish(world, self.rules, ab.planner_ticks(world, self.rules), 20, (758.0, 135.0), 5.0,
                                     -1, base, **ending)

        self.assertEqual(finish(40.0, caught=True).raid, 0)
        # 2 s left at 3.5 yd/s leaves it 28 yd off the gather
        self.assertEqual(finish(4.0, own_used=True).raid, -1)
        # a patch spent after the catch keeps the old raid reading
        self.assertEqual(finish(40.0, [kite_patch(770.0, 140.0, False)], fail_tick=25, fail_patch=0,
                                caught=True).raid, -1)

    def test_no_immunity_on_a_slow_spike(self):
        # 2026-11-01 BO at 1:22: the mage at the gather blocked a base speed spike 26 yd off before it got there
        world = fresh_world((777.55, 134.34), (774.61, 108.63), (774.96, 134.17),
                            [kite_patch(795.73, 119.19, True), kite_patch(792.11, 148.68, True),
                             kite_patch(769.09, 166.66, False), kite_patch(730.44, 111.3, False)], 57.3, own=True)
        world.spike = (*world.spike[:4], 6.9)
        candidate, out, _ = ab.plan_kite(world, self.rules)
        self.assertEqual((out.ending, candidate.own), ("patch", False))
        self.assertGreaterEqual(out.end_s, 12.0)

    def test_only_a_spike_faster_than_the_kiter_takes_its_own_immunity(self):
        slow = fresh_world((745.0, 135.0), (745.0, 150.0), None, [], own=True)
        walk = straight(slow, (0.0, 1.0), self.rules, own=True)
        caught = ab.judge(walk, slow, self.rules)
        self.assertEqual((caught.ending, caught.caught), ("none", True))
        self.assertEqual(ab.judge(walk, top_speed(slow), self.rules).ending, "own")

    def test_no_walk_into_a_slow_spike(self):
        # 2026-11-01 BP at 2:04: a side tank walked into a base speed spike, the catch reading raid-free though the
        # spike walks on into the gather. Running off passes the raid later, so it wins.
        world = fresh_world((770.0, 130.0), (770.0, 157.0), (768.0, 130.0),
                            [kite_patch(782.32, 114.76, True), kite_patch(795.69, 140.62, True)], 19.0)
        _, out, _ = ab.plan_kite(world, self.rules)
        self.assertGreaterEqual(out.end_s, 7.0)
        self.assertGreater(out.raid, 0)
        self.assertGreaterEqual(ab.kitesim_play(world, self.rules)["s"], 7.0)

    def test_a_late_immunity_still_passes_the_raid(self):
        # it costs least in the spike's last seconds, so it beats a patch passing the raid as early and loses to one
        # that never does
        with unittest.mock.patch.object(ab, "KITESIM_SLOW", 1.0):
            world = top_speed(fresh_world((735.0, 135.0), (735.0, 165.0), (765.0, 135.0),
                                          [kite_patch(720.0, 135.0, False)], 7.0, own=True))
            own, patch = straight(world, (0.0, -1.0), self.rules, own=True), into(world, 0, self.rules)
            own_away, patch_away = ab.judge(own, world, self.rules), ab.judge(patch, world, self.rules)
            self.assertEqual((own_away.ending, own_away.raid, own_away.cost), ("own", 0, self.rules["costOwn"]))
            self.assertEqual((patch_away.ending, patch_away.tank, patch_away.kite, patch_away.raid),
                             ("patch", -1, -1, -1))
            self.assertLess(ab.planner_compare(patch_away, own_away, self.rules), 0)

            # the spike starts by the gather and passes it within its first second either way
            world.gather = (742.0, 150.0)
            own_near, patch_near = ab.judge(own, world, self.rules), ab.judge(patch, world, self.rules)
            band = ab.to_tick(self.rules["hazardBandS"])
            self.assertEqual(own_near.ending, "own")
            self.assertTrue(0 <= own_near.raid < band and 0 <= patch_near.raid < band)
            self.assertLess(ab.planner_compare(own_near, patch_near, self.rules), 0)

    def test_a_patch_ends_a_catch_only_before_the_second_impale(self):
        # a top speed spike Impales every 0.4 s: caught 5 yd off the patch it fails there 0.2 s later, caught
        # 10 yd off it's 0.6 s short and goes on from the catch into the gather
        with unittest.mock.patch.object(ab, "KITESIM_SLOW", 1.0):
            def judged(spike_x: float) -> ab.PlanOutcome:
                world = top_speed(fresh_world((735.0, 135.0), (spike_x, 135.0), (790.0, 135.0),
                                              [kite_patch(720.0, 135.0, False)], 20.0))
                return ab.judge(into(world, 0, self.rules), world, self.rules)

            near, far = judged(749.75), judged(746.6)
            self.assertEqual((near.caught, near.tank, near.kite, near.raid), (True, -1, -1, -1))
            self.assertEqual((far.caught, far.kite, far.raid), (True, -1, 0))
            self.assertLess(ab.planner_compare(near, far, self.rules), 0)

    def test_the_spike_step_reads_off_its_speed_and_times_the_next_off_the_restart(self):
        self.assertEqual(ab.spike_step(3.4, 0.5), (0, 6.5))
        self.assertEqual(ab.spike_step(6.0, 8.0), (1, 6.0))
        # the clock reads the base step, the speed the second: half a step left
        self.assertEqual(ab.spike_step(6.0, 0.0), (1, 3.5))
        self.assertEqual(ab.spike_step(None, 15.0), (2, 6.0))
        self.assertEqual(ab.spike_step(0.0, None), (0, 7.0))


PRUNNER, PSECOND, PHUMAN, PG1, PG2, PTANK = range(9601, 9607)
PBOSS = CREATURE | 1700
PSPIKE = CREATURE | 1701
PTP, PKP, PSP = CREATURE | 1711, CREATURE | 1712, CREATURE | 1713
PATCH_SPOTS = {PTP: (X + 20.0, Y + 30.0), PKP: (X - 25.0, Y - 25.0), PSP: (X + 20.0, Y + 15.0)}
# at its base speed for its first 1.5 s, then on into the kite patch's reach at 26 s, 13.5 s after its restart
PRUNNER_SPIKE = [(12500, 0.0, -38.0), (14000, 5.25, -38.0), (20000, 10.0, -10.0), (26000, -15.0, -20.0),
                 (27500, -20.0, -24.0)]
IMPALE_25H = 67860


def planner_spike(when: int) -> tuple[tuple[float, float], int] | None:
    """The runner's chase, then idle through the human's kite, then the second's at the second step's speed east
    along y + 30: past the gather at 46 s and within the tank patch's reach from 49 s."""
    if when < 12500:
        return None
    if when < 44000:
        target = PRUNNER if when <= 27500 else PHUMAN if 31500 <= when <= 40000 else 0
        return lerp(PRUNNER_SPIKE, when), target
    return (X - 20.0 + 6.125 * (min(when, 50000) - 44000) / 1000.0, Y + 30.0), PSECOND if when <= 50000 else 0


def planner_snap(when: int) -> dict:
    spots = {PRUNNER: lerp([(12500, 30.0, 0.0), (27500, -10.0, -40.0)], when), PSECOND: (X + 35.0, Y + 10.0),
             PHUMAN: (X - 35.0, Y), PG1: (X + 40.0, Y - 20.0) if when < 15000 else (X + 5.0, Y + 15.0),
             PG2: (X + 5.0, Y + 19.0), PTANK: (X + 20.0, Y + 33.0)}
    units = [[guid, *spot, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0] for guid, spot in spots.items()]
    units.append([PBOSS, X, Y, Z, 0.0, 80.0, 0.0, 0, 0, 0, 0, 0])
    hz = []
    for guid, spot in PATCH_SPOTS.items():
        if guid == PTP or (5000 <= when <= 28000 if guid == PKP else when >= 5000):
            units.append([guid, *spot, PATCH_Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
            hz.append([PERMAFROST_25H, *spot, PATCH_Z, 6.0, 1])
    spike = planner_spike(when)
    if spike:
        units.append([PSPIKE, *spike[0], Z, 0.0, 100.0, 0.0, spike[1], 1, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def planner_pull(with_probes: bool = True, diff: int = 3) -> list[dict]:
    """A 25H submerge from 10 s to 75 s, three kites in it: the runner's 12.5-27.5 s, ending on the kite patch
    its kitepath picked; a human's 31.5-40 s; and the second's 44-50 s on a spike at its second step, past the
    gather and a tank patch and impaled. The gatherers hold from 11 s, one still walking in until 15 s."""
    records = [
        {"e": "hdr", "v": 14, "ts": 1790300000000, "map": ab.MAP_TOC, "inst": 23, "diff": diff, "boss": "anub-arak",
         "roster": [{"g": PRUNNER, "n": "Runner", "r": "ranged", "c": 3, "h": 0},
                    {"g": PSECOND, "n": "Second", "r": "melee", "c": 4, "h": 0},
                    {"g": PHUMAN, "n": "Arthas", "r": "melee", "c": 1, "h": 1},
                    {"g": PG1, "n": "Gatherer", "r": "heal", "c": 5, "h": 0},
                    {"g": PG2, "n": "Waiter", "r": "ranged", "c": 8, "h": 0},
                    {"g": PTANK, "n": "Felesta", "r": "tank", "c": 6, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": PBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 10000, "e": "unit", "g": PSPIKE, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"} for guid in PATCH_SPOTS),
        *(note(when, PTANK, "anub.phase", value) for when, value in ((0, 1), (10000, 2), (75000, 1))),
        note(0, PTANK, "anub.patch0", PTP),
        note(11000, PG1, "anub.gather", "hold"),
        note(11000, PG2, "anub.gather", "hold"),
        *(aura(when, kiter, ab.SPELL_MARK, removed, PSPIKE) for kiter, start, stop in (
            (PRUNNER, 12500, 27500), (PHUMAN, 31500, 40000), (PSECOND, 44000, 50000))
          for when, removed in ((start, False), (stop, True))),
        dmg(50000, PSPIKE, PSECOND, IMPALE_25H, 12000),
        {"t": 80000, "e": "end", "out": "wipe"},
    ]
    if with_probes:
        records += [
            *(note(when, PRUNNER, "anub.kitepred", value) for when, value in ((12400, "catch:15.2"), (18000, "patch:9.0"))),
            *(note(when, PRUNNER, "anub.kitepath", value) for when, value in (
                (12400, "ring:90:ccw:40"), (13000, "arc:300:12"), (15000, "arc:300:12>patch:1712@9.5:in"),
                (22000, "ring:180:cw:45>patch:1712@4.0:edge1"))),
            # the second's first pick read the same as a pick before, so it wrote nothing at its mark
            note(30000, PSECOND, "anub.kitepred", "raid:1.5"),
            note(44100, PSECOND, "anub.kitepath", "arc:0:0"),
            note(31600, PHUMAN, "anub.kitepath", "motion"),
            note(31600, PHUMAN, "anub.kitepred", "catch:8.0"),
        ]
    records += [planner_snap(when) for when in range(-500, 80001, 500)]
    return records


class KitesimPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_23_anub-arak_1790300000.ndjson", planner_pull())
        cls.bare = write_trace(cls.folder.name, "649_23_anub-arak_1790300001.ndjson", planner_pull(False))
        cls.windows = ab.mark_windows(cls.trace)
        cls.rows = ab.kitesim_rows(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, show, trace=None) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            show(trace or self.trace)
        return out.getvalue()

    def test_each_bot_kite_starts_from_its_mark_the_human_left_out(self):
        starts = {start["window"]["guid"]: start for start in ab.kite_starts(self.trace)}
        self.assertEqual(sorted(starts), [PRUNNER, PSECOND])
        runner, second = starts[PRUNNER], starts[PSECOND]
        self.assertEqual((runner["at"], runner["kiter"], runner["life"], runner["catch_from"]),
                         (12500, (X + 30.0, Y), 57.5, 0.0))
        self.assertEqual(runner["spike"], (X, Y - 38.0, ab.SPIKE_SPEEDS[0], 0, 7.0))
        # the stray patch sits 15 yd off the tank patch, so no kite ends on it
        self.assertEqual(sorted(runner["patches"]), sorted([(*PATCH_SPOTS[PTP], True, False, False),
                                                             (*PATCH_SPOTS[PKP], False, True, False),
                                                             (*PATCH_SPOTS[PSP], False, False, False)]))
        # as formed: at the mark the gatherer is still walking in from the east
        self.assertEqual(runner["gather"], (X + 5.0, Y + 17.0))
        self.assertEqual((second["at"], second["life"], len(second["patches"])), (44000, 26.0, 2))
        # its speed reads the second step while the restart clock reads the first, so half a step is left
        self.assertEqual(second["spike"], (X - 20.0, Y + 30.0, ab.SPIKE_SPEEDS[1], 1, 3.5))

    def test_each_replay_reads_the_real_end_tank_pass_and_raid_pass(self):
        self.assertEqual([(row["window"]["guid"], row["real"], row["real_s"], row["real_tank"], row["real_raid"],
                           row["fresh"]) for row in self.rows],
                         [(PRUNNER, "patch", 15.0, False, None, True), (PSECOND, "catch", 6.0, True, 2.0, False)])
        for row in self.rows:
            self.assertIn(row["played"]["end"], ("patch", "tank", "catch", "emerge"))
            self.assertRegex(row["played"]["pred"], r"^(tank|kite|raid|catch|none|patch):\d+\.\d$")

    def test_the_section_prints_each_kite_and_the_fresh_summary(self):
        text = self.text(ab.show_kitesim)
        self.assertIn("KITE PLANNER REPLAY\n  each bot kite's start replayed through a port of the kite planner,"
                      " re-planned every 0.3 s on 0.1 s ticks;\n", text)
        self.assertRegex(text, r"\n   0:12\.500 Runner            0  57\.5  \S+ +\w+ [\d.]+ s[ \w]* +patch 15\.0 s\n")
        self.assertRegex(text, r"\n   0:44\.000 Second            1  26\.0  \S+ +\w+ [\d.]+ s[ \w]* +catch 6\.0 s;"
                               r" (replay raid pass \+[\d.]+ s, )?real raid pass \+2\.0 s, real tank pass\n")
        self.assertIn("  fresh-spike kites, the spike at its base step and over 2 s to the replay's first event:"
                      " 1 of 2\n", text)
        self.assertRegex(text, r"    patch ends under 10 s: real 0, replay [01]\n    tank patch passes: real 0,"
                               r" replay [01]\n    raid passes: real 0, replay [01]\n")

    def test_only_25h_replays(self):
        with tempfile.TemporaryDirectory() as folder:
            trace = write_trace(folder, "649_23_anub-arak_1790300002.ndjson", planner_pull(diff=2))
            self.assertEqual(ab.kitesim_rows(trace), [])
            self.assertEqual(self.text(ab.show_kitesim, trace), "KITE PLANNER REPLAY\n  25H only\n")

    def test_a_pick_carried_in_counts_only_with_no_row_near_the_mark(self):
        rows = [(1000, "a"), (9500, "b"), (12000, "b"), (13000, "c")]
        self.assertEqual(ab.noted_in(rows, 10000, 20000), [(9500, "b"), (13000, "c")])
        self.assertEqual(ab.noted_in(rows, 3000, 20000), [(3000, "a"), (9500, "b"), (13000, "c")])
        self.assertEqual(ab.plan_terminals(rows, 3000, 20000), ["a", "b", "c"])

    def test_kitepath_and_kitepred_values_parse(self):
        self.assertEqual(ab.kitepath_heading("ring:180:cw:45>patch:1712@4.0:edge1+hand"), 180.0)
        self.assertEqual(ab.kitepath_heading("arc:22:-24"), 22.0)
        self.assertIsNone(ab.kitepath_heading("motion"))
        self.assertEqual(ab.kitepath_patch("arc:300:12>patch:1712@9.5:in+own"), 1712)
        self.assertIsNone(ab.kitepath_patch("ring:90:ccw:40"))
        self.assertEqual(ab.parse_kitepred("patch+hand:12.4"), ("patch+hand", 12.4))
        self.assertIsNone(ab.parse_kitepred("none"))

    def test_each_kite_reads_its_picks_held_and_a_reversal_within_a_second(self):
        runner, human, second = self.windows
        self.assertEqual([(pick["value"].split(">")[0], pick["held"], pick["reversal"]) for pick in runner["kitepath"]],
                         [("ring:90:ccw:40", 500, False), ("arc:300:12", 2000, True), ("arc:300:12", 7000, False),
                          ("ring:180:cw:45", 5500, False)])
        self.assertEqual([(pick["value"], pick["held"]) for pick in human["kitepath"]], [("motion", 8400)])
        self.assertEqual(second["kitepred"], [(44000, "raid:1.5")])

    def test_the_first_hazard_foreseen_against_the_first_real_one(self):
        hazards = ab.kite_hazards(self.trace, ab.Samples(self.trace), self.windows)
        # the runner's spike reaches the kite patch its pick ends on 13.5 s after its restart: no hazard
        self.assertEqual(hazards[(PRUNNER, 12500)], (("catch", 27600), None))
        self.assertEqual(hazards[(PSECOND, 44000)], (("raid", 45500), ("raid", 46000)))
        unplanned = {**self.windows[0], "kitepath": [{**pick, "value": pick["value"].split(">")[0]}
                                                     for pick in self.windows[0]["kitepath"]]}
        samples = ab.Samples(self.trace)
        self.assertEqual(ab.real_hazard(samples, {PSPIKE}, ab.patch_latches(self.trace), unplanned,
                                        (X + 5.0, Y + 17.0), ab.mark_spans(self.trace),
                                        ab.submerged_spans(self.trace), ab.planner_rules()), ("kite", 26000))

    def test_the_spike_section_prints_the_planner_lines(self):
        text = self.text(ab.show_spike)
        self.assertIn("            kitepred at the mark catch:15.2; real end patch 15.0 s\n"
                      "            kitepath ring:90:ccw:40 0.5 s, arc:300:12 2.0 s reversal,"
                      " arc:300:12>patch:1712@9.5:in 7.0 s, ring:180:cw:45>patch:1712@4.0:edge1 5.5 s\n"
                      "            first hazard: predicted catch +15.1 s, real none\n", text)
        self.assertIn("            kitepred at the mark raid:1.5; real end catch 6.0 s\n"
                      "            kitepath arc:0:0 5.9 s\n"
                      "            first hazard: predicted raid +1.5 s, real raid +2.0 s\n", text)
        self.assertIn("  anub.kitepath picks turning over 90 degrees within 1 s of the pick before:"
                      " Runner 0:12.500 +0.5 s\n", text)
        self.assertNotIn("kitepred at the mark", self.text(ab.show_spike, self.bare))
        self.assertIn("within 1 s of the pick before: none, no anub.kitepath rows\n", self.text(ab.show_spike, self.bare))

    def test_the_new_keys_count_as_present_and_absent_without_their_rows(self):
        gone = ab.missing_probes(self.trace)
        self.assertNotIn("anub.kitepath", gone)
        self.assertNotIn("anub.kitepred", gone)
        self.assertTrue({"anub.kitepath", "anub.kitepred"} <= set(ab.missing_probes(self.bare)))


WMAIN, WSIDE0, WSIDE1, WDPS1, WDPS2 = range(9701, 9706)
WAVE_BOSS = CREATURE | 1800
WB = {guid: CREATURE | guid for guid in range(1801, 1806)}
# guid: (side, spawn, last sample, spot)
WAVE_BUGS = {WB[1801]: (0, 2000, 30000, EXIT_N1), WB[1802]: (0, 2000, 30000, EXIT_N2),
             WB[1803]: (1, 2000, 30000, EXIT_S1), WB[1804]: (0, 45000, 55000, EXIT_N1),
             WB[1805]: (1, 45000, 56000, EXIT_S1)}


def wave_reads_snap(when: int) -> dict:
    units = [[guid, X + 2.0 * index, Y, Z, 0.0, 0.0 if guid == WSIDE0 and when >= 52000 else 100.0, 100.0,
              0, 0, 0, 0, 0] for index, guid in enumerate((WMAIN, WSIDE0, WSIDE1, WDPS1, WDPS2))]
    units.append([WAVE_BOSS, X, Y, Z, 0.0, 50.0, 0.0, WMAIN, 0, 0, 0, 0])
    for guid, (side, spawn, last, spot) in WAVE_BUGS.items():
        if spawn <= when <= last:
            units.append([guid, *spot, Z, 0.0, 100.0 - (when - spawn) / 1000.0, 0.0, WSIDE0 if side == 0 else WSIDE1,
                          0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": []}


def wave_reads_pull(new_rows: bool = True) -> list[dict]:
    """A 25H pull, phase 1 from 0 and phase 3 from 40 s. Wave 1 comes up at 2 s, two burrowers on side 0 picked
    at 9 s and one on side 1 picked at 4.5 s; wave 2 at 45 s, side 0's picked at 47 s, side 1's never. Side 0's
    tank peaks at 6 Expose Weakness in wave 1 and 9 in wave 2, dying at 52 s before the wave dies at 56 s."""
    records = [
        {"e": "hdr", "v": 14, "ts": 1790400000000, "map": ab.MAP_TOC, "inst": 24, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": WMAIN, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 0},
                    {"g": WSIDE0, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
                    {"g": WSIDE1, "n": "Bulwark", "r": "tank", "c": 1, "h": 0},
                    {"g": WDPS1, "n": "Hellflame", "r": "ranged", "c": 9, "h": 0},
                    {"g": WDPS2, "n": "Trueshot", "r": "ranged", "c": 3, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": WAVE_BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        *({"t": spawn, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"}
          for guid, (_, spawn, _, _) in WAVE_BUGS.items()),
        *(note(when, WMAIN, "anub.phase", value) for when, value in ((0, 1), (40000, 3))),
        note(0, WMAIN, "anub.side0", WSIDE0),
        note(0, WMAIN, "anub.side1", WSIDE1),
        *(note(when, WMAIN, "anub.wave", value) for when, value in ((2000, 1), (45000, 2))),
        *(note(spawn, guid, "anub.bside", side) for guid, (side, spawn, _, _) in WAVE_BUGS.items()),
        *(note(when, WDPS1, "anub.focus0", value) for when, value in ((9000, WB[1801]), (15000, 0), (47000, WB[1804]))),
        note(4500, WDPS2, "anub.focus1", WB[1803]),
        *(expose(when, WSIDE0, 67847, stacks) for when, stacks in ((5000, 3), (8000, 6), (20000, 0), (50000, 9))),
        *(expose(when, WSIDE1, 67847, stacks) for when, stacks in ((6000, 2), (48000, 4))),
        {"t": 52000, "e": "death", "g": WSIDE0, "killer": WB[1804], "x": X, "y": Y, "z": Z},
        {"t": 70000, "e": "end", "out": "wipe"},
    ]
    if new_rows:
        # two bots read the same wave, one a tick after the other
        records += [note(45100, WDPS1, "anub.p3wave", "kill:44"), note(45200, WDPS2, "anub.p3wave", "kill:44"),
                    note(51000, WDPS1, "anub.p3wave", "leave:8")]
    records += [wave_reads_snap(when) for when in range(-500, 70001, 500)]
    return records


class WaveReads(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_24_anub-arak_1790400000.ndjson", wave_reads_pull())
        cls.bare = write_trace(cls.folder.name, "649_24_anub-arak_1790400001.ndjson", wave_reads_pull(False))

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, trace) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_burrowers(trace)
        return out.getvalue()

    def test_p3wave_rows_from_two_bots_count_once(self):
        self.assertEqual(ab.p3wave_rows(self.trace), [(45100, "kill:44"), (51000, "leave:8")])

    def test_each_wave_reads_its_focus_pick_per_side_and_the_side_tanks_peak(self):
        first, second = ab.wave_reads(self.trace)
        self.assertEqual((first["wave"], first["start"], first["phase"]), (1, 2000, 1))
        self.assertEqual([(side["side"], side["pick"], side["tank"], side["peak"]) for side in first["sides"]],
                         [(0, 7000, WSIDE0, 6), (1, 2500, WSIDE1, 2)])
        self.assertNotIn("p3wave", first)
        self.assertEqual([(side["side"], side["pick"], side["peak"], side["death"]) for side in second["sides"]],
                         [(0, 2000, 9, 52000), (1, None, 4, None)])
        self.assertEqual((second["phase"], second["died"], second["p3wave"]),
                         (3, 56000, [(45100, "kill:44"), (51000, "leave:8")]))

    def test_the_burrower_section_prints_each_wave(self):
        text = self.text(self.trace)
        self.assertIn("    wave 1    0:02.000  side 0 pick +7.0 s, Felesta peak 6; side 1 pick +2.5 s, Bulwark peak 2\n"
                      "    wave 2    0:45.000  side 0 pick +2.0 s, Felesta peak 9; side 1 no pick, Bulwark peak 4\n"
                      "          P3: anub.p3wave kill:44 +0.1 s, leave:8 +6.0 s; the wave died 0:56.000;"
                      " Felesta died +7.0 s, Bulwark lived\n", text)
        self.assertNotIn("no anub.p3wave rows", text)
        bare = self.text(self.bare)
        self.assertIn("          P3: anub.p3wave none; the wave died 0:56.000; Felesta died +7.0 s, Bulwark lived\n",
                      bare)
        self.assertIn("  no anub.p3wave rows\n", bare)

    def test_p3wave_counts_as_present_and_absent_without_its_rows(self):
        self.assertNotIn("anub.p3wave", ab.missing_probes(self.trace))
        self.assertIn("anub.p3wave", ab.missing_probes(self.bare))


NXTANK, NXASSASIN, NXANGRY, NXAGONY, NXHOLY, NXPRAYER, NXDRUID = range(9901, 9908)
NXBOSS = CREATURE | 2001
NXSPIKE = CREATURE | 2002
NXPATCH = CREATURE | 2011
NXBUG = CREATURE | 2021
NXPATCH_AT = (X + 10.0, Y - 8.0)
NXHOLY_AT = (X - 20.0, Y - 15.0)
NXBUG_AT = (X + 5.0, Y + 30.0)
SPELL_VANISH, SPELL_DIVINE_PLEA, SPELL_INNERVATE, SPELL_HYMN = 26889, 54428, 29166, 64901
# (start, stop, kiter) per mark under him, and each kiter's path while marked
NXMARKS = ((12000, 20000, NXASSASIN), (20200, 21000, NXANGRY), (26000, 34000, NXAGONY))
NXKITERS = {NXASSASIN: [(12000, -28.0, -20.0), (20000, 28.0, -20.0)], NXANGRY: [(0, 10.0, 10.0)],
           NXAGONY: [(28000, -20.0, 20.0), (34000, -20.0, -22.0)]}
# base speed into Assasin's Sprint, fast to the Vanish, on through the kite patch's reach at Angry, idle where it
# spent it, then fast at Agony
NXSPIKE_PATH = [(12000, -40.0, -20.0), (16000, -26.0, -20.0), (20000, 18.0, -20.0), (21000, 10.0, -9.0),
               (26000, 10.0, -9.0), (28000, -12.0, 12.0), (31000, -16.0, -21.0), (34000, -18.0, -23.0)]


def nextleg_mana(guid: int, when: int) -> float:
    """Holylight from 60 to 25 and Prayer from 90 to 70 over phase 3's first 30 s."""
    start, end = {NXHOLY: (60.0, 25.0), NXPRAYER: (90.0, 70.0)}.get(guid, (100.0, 100.0))
    return start + (end - start) * min(max(when - 40000, 0), 30000) / 30000


def nextleg_snap(when: int) -> dict:
    spots = {NXTANK: (X + 5.0, Y + 27.0) if when < 50000 else (X + 5.0, Y + 10.0), NXHOLY: NXHOLY_AT,
             NXPRAYER: (X - 10.0, Y + 30.0), NXDRUID: (X - 25.0, Y + 5.0),
             **{guid: lerp(path, when) for guid, path in NXKITERS.items()}}
    units = [[guid, *spot, Z, 0.0, 100.0, nextleg_mana(guid, when),
              NXBUG if guid == NXANGRY and 42000 <= when < 56000 else 0, 0, 0, 0, 0] for guid, spot in spots.items()]
    units.append([NXBOSS, X, Y, Z, 0.0, 70.0, 0.0, NXTANK, 0, 0, 0, 0])
    kiter = next((guid for start, stop, guid in NXMARKS if start <= when <= stop), 0)
    if 12000 <= when <= 34000:
        units.append([NXSPIKE, *lerp(NXSPIKE_PATH, when), Z, 0.0, 100.0, 0.0, kiter, 1, 0, 0, 0])
    hz = []
    if when <= 21000:
        units.append([NXPATCH, *NXPATCH_AT, PATCH_Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        hz.append([PERMAFROST_25H, *NXPATCH_AT, PATCH_Z, 6.0, 1])
    if 41000 <= when <= 56000:
        units.append([NXBUG, *NXBUG_AT, Z, 0.0, 100.0, 0.0, NXPRAYER if when < 46000 else NXTANK, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def nextleg_pull() -> list[dict]:
    """A 25H pull, phase 2 from 10 s and phase 3 from 40 s. Assasin's kite plans its boost 2 s from its pick and
    Sprints 0.7 s later on a base speed spike, then Vanishes; the spike runs on through a kite patch at Angry, who
    spends it; Agony's kite plans no boost, Nitros on a fast spike and has Holylight on standby. In phase 3
    Holylight goes from 60 to 25% mana and Prayer from 90 to 70%, and Angry targets a burrower on Prayer, then on
    Felesta 3 yd off, then on her 20 yd off."""
    records = [
        {"e": "hdr", "v": 14, "ts": 1790500000000, "map": ab.MAP_TOC, "inst": 25, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": NXTANK, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
                    {"g": NXASSASIN, "n": "Assasin", "r": "melee", "c": 4, "h": 0},
                    {"g": NXANGRY, "n": "Angry", "r": "melee", "c": 1, "h": 0},
                    {"g": NXAGONY, "n": "Agony", "r": "ranged", "c": 3, "h": 0},
                    {"g": NXHOLY, "n": "Holylight", "r": "heal", "c": 2, "h": 0},
                    {"g": NXPRAYER, "n": "Prayer", "r": "heal", "c": 5, "h": 0},
                    {"g": NXDRUID, "n": "Druidica", "r": "ranged", "c": 11, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": NXBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 10000, "e": "unit", "g": NXSPIKE, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        {"t": 0, "e": "unit", "g": NXPATCH, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"},
        {"t": 41000, "e": "unit", "g": NXBUG, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        *(note(when, NXTANK, "anub.phase", value) for when, value in ((0, 1), (10000, 2), (40000, 3))),
        *(aura(when, kiter, ab.SPELL_MARK, removed, NXSPIKE) for start, stop, kiter in NXMARKS
          for when, removed in ((start, False), (stop, True))),
        note(12100, NXASSASIN, "anub.kitepath", "ring:90:ccw:40+boost@2.0"),
        note(26100, NXAGONY, "anub.kitepath", "arc:180:12"),
        *(note(when, NXHOLY, "anub.escort", value) for when, value in ((26500, "standby"), (34000, "none"))),
        note(31000, NXHOLY, "anub.protect", "hop"),
        *(note(when, NXANGRY, "anub.dps", value) for when, value in ((41500, "loose"), (50000, "burrower"))),
        cast(14800, NXASSASIN, SPELL_SPRINT, NXASSASIN),
        cast(19900, NXASSASIN, SPELL_VANISH, NXASSASIN),
        cast(30000, NXAGONY, SPELL_NITRO_USE, NXAGONY),
        cast(15000, NXHOLY, SPELL_DIVINE_PLEA, 0),
        cast(45000, NXDRUID, SPELL_INNERVATE, NXHOLY),
        cast(50000, NXPRAYER, SPELL_HYMN, 0),
        {"t": 75000, "e": "end", "out": "wipe"},
    ]
    records += [nextleg_snap(when) for when in range(-500, 75001, 500)]
    return records


class NextLegPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_25_anub-arak_1790500000.ndjson", nextleg_pull())
        cls.windows = ab.mark_windows(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, show) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            show(self.trace)
        return out.getvalue()

    def test_each_speed_boost_reads_the_spikes_speed_against_the_kiters_run_before_it(self):
        assasin, angry, agony = self.windows
        self.assertEqual([(row["rec"]["sp"], round(row["spike"], 2), round(row["kiter"], 2), row["slow"])
                          for row in assasin["boost_speeds"]], [(SPELL_SPRINT, 3.5, 7.0, True)])
        self.assertEqual(angry["boost_speeds"], [])
        [nitro] = agony["boost_speeds"]
        self.assertEqual((nitro["rec"]["sp"], nitro["slow"]), (SPELL_NITRO_USE, False))
        self.assertAlmostEqual(nitro["spike"], math.hypot(4.0, 33.0) / 3.0)

    def test_the_first_boost_reads_against_its_picks_boost_second(self):
        assasin, angry, agony = self.windows
        self.assertEqual((assasin["boost_plan"]["cast"]["t"], assasin["boost_plan"]["planned"]), (14800, 14100))
        self.assertIsNone(angry["boost_plan"])
        self.assertEqual((agony["boost_plan"]["pick"]["at"], agony["boost_plan"]["planned"]), (26100, None))
        self.assertEqual(ab.kitepath_boost("ring:180:cw:45>patch:1712@4.0:edge1+hand+boost@3.5"), 3.5)
        self.assertIsNone(ab.kitepath_boost("arc:22:-24"))
        unused = {"start": 0, "stop": 5000, "boosts": [], "kitepath": [{"at": 1000, "value": "arc:0:0+boost@1.5"}]}
        unused["boost_plan"] = ab.boost_plan(unused)
        self.assertEqual(ab.boost_plan_text(unused), "its pick's boost +2.5 s never cast")
        self.assertTrue(ab.boost_plan_slip(unused))
        self.assertIsNone(ab.boost_plan({**unused, "stop": 2000}))
        cast = {**unused, "boosts": [{"t": 2900, "s": 1, "sp": SPELL_SPRINT}]}
        cast["boost_plan"] = ab.boost_plan(cast)
        self.assertEqual(ab.boost_plan_text(cast), "Sprint +2.9 s against its pick's +2.5 s, on time")
        self.assertFalse(ab.boost_plan_slip(cast))

    def test_the_standby_escort_reads_its_yards_at_the_claim(self):
        standby = self.windows[2]["standby"]
        self.assertEqual((standby["guid"], standby["t"], standby["claimer"]), (NXHOLY, 31000, NXHOLY))
        self.assertAlmostEqual(standby["yards"], 14.0)
        self.assertIsNone(self.windows[0]["standby"])

    def test_the_spikes_next_leg_after_an_immunity_and_a_patch_end(self):
        assasin, angry, agony = (window["next_leg"] for window in self.windows)
        self.assertEqual((assasin["after"], assasin["next"]["guid"]), ("Vanish", NXANGRY))
        self.assertEqual((assasin["pass"]["guid"], assasin["pass"]["t"], assasin["pass"]["side"],
                          assasin["pass"]["spent"]), (NXPATCH, 20500, None, True))
        self.assertAlmostEqual(assasin["pass"]["yards"], math.hypot(4.0, 6.5))
        self.assertEqual((angry["after"], angry["next"]["guid"], angry["pass"]), ("patch", NXAGONY, None))
        self.assertIsNone(agony)

    def test_the_spike_section_prints_them(self):
        text = self.text(ab.show_spike)
        self.assertIn("            speeds at the boosts: Sprint +2.8 s: spike 3.5 yd/s, kiter run 7.0 yd/s,"
                      " no faster\n", text)
        self.assertIn("            speeds at the boosts: Nitro Boosts +4.0 s: spike 11.1 yd/s, kiter run 7.0 yd/s\n",
                      text)
        self.assertIn("            boost against the plan: Sprint +2.8 s against its pick's +2.1 s, 0.7 s late\n", text)
        self.assertIn("            next leg after Vanish to Angry: kite patch #2011 (755.0,127.0) 7.6 yd +0.3 s into"
                      " its mark, spent\n", text)
        self.assertIn("            next leg after the patch to Agony: no patch within 11.5 yd\n", text)
        self.assertIn("            boost against the plan: Nitro Boosts +4.0 s, its pick +0.1 s firing none\n"
                      "            standby escort Holylight 14.0 yd off at the claim +5.0 s\n", text)
        self.assertIn("  Sprint, Dash or Nitro Boosts on a spike no faster than the kiter's run over the 1.5 s before"
                      " it: Assasin 0:14.800 Sprint (3.5 vs 7.0 yd/s)\n", text)
        self.assertIn("  first speed boost against its anub.kitepath pick's +boost@ second, on time within 0.5 s: 0 of"
                      " 2; else: Assasin 0:12.000 Sprint +2.8 s against its pick's +2.1 s, 0.7 s late, Agony 0:26.000"
                      " Nitro Boosts +4.0 s, its pick +0.1 s firing none\n", text)
        self.assertIn("  anub.escort standby within 30 yd of the kiter at the claim: 1 of 1\n", text)
        self.assertIn("  the spike's next leg after an own immunity or a patch end, from the next mark to the spike"
                      " within 12 yd\n  of its kiter or of its spot at the mark, past a patch: 1 of 2: Assasin 0:12.000"
                      " next leg after Vanish to Angry: kite patch #2011 (755.0,127.0) 7.6 yd +0.3 s into its mark,"
                      " spent\n", text)

    def test_a_trace_with_no_boost_picks_or_standby_rows_says_so(self):
        records = [rec for rec in nextleg_pull()
                   if "+boost@" not in str(rec.get("txt")) and rec.get("txt") != "standby"]
        with tempfile.TemporaryDirectory() as folder:
            trace = write_trace(folder, "649_25_anub-arak_1790500001.ndjson", records)
            self.assertEqual([window["boost_plan"] for window in ab.mark_windows(trace)], [None, None, None])
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                ab.show_spike(trace)
        self.assertIn("  first speed boost against its anub.kitepath pick's +boost@ second: no +boost@ picks\n",
                      out.getvalue())
        self.assertIn("  anub.escort standby within 30 yd of the kiter at the claim: no standby rows\n", out.getvalue())

    def test_each_healers_mana_in_phase_3_and_the_mana_cooldowns(self):
        read = ab.healer_mana(self.trace)
        self.assertEqual((read["start"], read["mana"]), (40000, {NXHOLY: (60.0, 25.0), NXPRAYER: (90.0, 70.0)}))
        self.assertEqual({guid: casts for guid, casts in read["casts"].items()},
                         {NXHOLY: [(15000, SPELL_DIVINE_PLEA, 0, "P2")],
                          NXDRUID: [(45000, SPELL_INNERVATE, NXHOLY, "P3")], NXPRAYER: [(50000, SPELL_HYMN, 0, "P3")]})

    def test_the_healer_section_prints_them(self):
        self.assertIn("  mana at phase 3's start 0:40.000 and 30 s in, - dead or unsampled, and the Divine Plea,"
                      " Innervate and Hymn of Hope\n  casts with their phase:\n"
                      "    Holylight      60% > 25%; Divine Plea P2 0:15.000\n"
                      "    Prayer         90% > 70%; Hymn of Hope P3 0:50.000\n"
                      "    Druidica       Innervate on Holylight P3 0:45.000  (ranged)\n", self.text(ab.show_healers))

    def test_melee_seconds_on_a_burrower_no_tank_holds_by_anub_dps(self):
        self.assertEqual(ab.loose_burrower_time(self.trace), {NXANGRY: {"loose": 4000, "burrower": 6000}})
        self.assertIn("  melee bot seconds on a burrower no tank within 5 yd held, by anub.dps then: Angry 10.0 s"
                      " (burrower 6.0, loose 4.0)\n", self.text(ab.show_burrowers))


CLBEAR, CLDK, CLPAL, CLWAR, CLFEL = range(9951, 9956)
CLBOSS = CREATURE | 2101
CLSPIKE = CREATURE | 2102
SPELL_DIRE_BEAR, SPELL_FROST_PRESENCE, SPELL_RIGHTEOUS_FURY, SPELL_DEFENSIVE_STANCE = 9634, 48263, 25780, 71
# Fel walks 2.5 s at a far spike and turns away 33.5 yd off it, walks back into it from 4.5 s for 3 s, and runs
# from 10 s while it closes at top speed
CLFEL_PATH = [(12000, 0.0, 0.0), (14500, -17.5, 0.0), (16500, -3.5, 0.0), (19500, -24.5, 0.0), (22000, -7.0, 0.0),
              (25000, 14.0, 0.0)]
CLSPIKE_PATH = [(12000, -59.75, 0.0), (22000, -24.75, 0.0), (25000, 9.375, 0.0)]


def closing_snap(when: int) -> dict:
    units = [[guid, X + 3.0 * index, Y + 20.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0]
             for index, guid in enumerate((CLBEAR, CLDK, CLPAL, CLWAR))]
    units.append([CLFEL, *lerp(CLFEL_PATH, when), Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0])
    units.append([CLBOSS, X, Y + 25.0, Z, 0.0, 60.0, 0.0, CLBEAR, 0, 0, 0, 0])
    if 12000 <= when <= 25000:
        units.append([CLSPIKE, *lerp(CLSPIKE_PATH, when), Z, 0.0, 100.0, 0.0, CLFEL, 1, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": []}


def closing_pull() -> list[dict]:
    """A 25H pull, phase 2 from 10 s and P1b from 40 s. Fel's mark from 12 s closes on a base speed spike twice,
    ending 33.5 yd off and then 9 yd off, and on the top speed spike. Bulwark's Righteous Fury drops at 3 s with no
    act row and at his death at 50 s, Felesta's Frost Presence at 5 s for unholy presence with a PREREQ row too
    early to count, a melee warrior's Defensive Stance at 6 s, and Ecoterrorist's Dire Bear Form while he is down
    and at 45 s for caster form, run for anubarak innervate healer, 1.2 s before his death."""
    def form(when: int, guid: int, spell: int, removed: bool) -> dict:
        return aura(when, guid, spell, removed, guid)

    records = [
        {"e": "hdr", "v": 14, "ts": 1790600000000, "map": ab.MAP_TOC, "inst": 26, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": CLBEAR, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 0},
                    {"g": CLDK, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
                    {"g": CLPAL, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": CLWAR, "n": "Mighty", "r": "melee", "c": 1, "h": 0},
                    {"g": CLFEL, "n": "Fel", "r": "ranged", "c": 9, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": CLBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 10000, "e": "unit", "g": CLSPIKE, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        *(note(when, CLBEAR, "anub.phase", value) for when, value in ((0, 1), (10000, 2), (40000, 1))),
        *(aura(when, CLFEL, ab.SPELL_MARK, removed, CLSPIKE) for when, removed in ((12000, False), (25000, True))),
        *(form(when, CLPAL, SPELL_RIGHTEOUS_FURY, removed) for when, removed in (
            (0, False), (3000, True), (3500, False), (50000, True))),
        {"t": 50002, "e": "death", "g": CLPAL, "killer": CLBOSS, "x": X, "y": Y, "z": Z},
        *(form(when, CLDK, SPELL_FROST_PRESENCE, removed) for when, removed in (
            (0, False), (5000, True), (8000, False))),
        act(4900, CLDK, "anubarak innervate healer", "PREREQ"),
        act(5003, CLDK, "unholy presence"),
        *(form(when, CLWAR, SPELL_DEFENSIVE_STANCE, removed) for when, removed in ((0, False), (6000, True))),
        *(form(when, CLBEAR, SPELL_DIRE_BEAR, removed) for when, removed in (
            (0, False), (12000, True), (14000, False), (45000, True))),
        act(12002, CLBEAR, "caster form"),
        act(44700, CLBEAR, "frenzied regeneration"),
        act(44998, CLBEAR, "frenzied regeneration", "USELESS"),
        act(44999, CLBEAR, "anubarak innervate healer", "PREREQ"),
        act(45002, CLBEAR, "caster form"),
        {"t": 46200, "e": "death", "g": CLBEAR, "killer": CLBOSS, "x": X, "y": Y, "z": Z},
        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    records += [closing_snap(when) for when in range(-500, 60001, 500)]
    return records


class ClosingPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_26_anub-arak_1790600000.ndjson", closing_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, show) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            show(self.trace)
        return out.getvalue()

    def test_only_the_kiters_close_on_a_slow_spike_ending_near_it_counts(self):
        [window] = ab.mark_windows(self.trace)
        [span] = window["closing"]
        self.assertEqual((span["start"], span["stop"], span["speeds"]), (16500, 19500, [ab.SPIKE_SPEEDS[0]]))
        for field, value in (("from", 40.5), ("to", 9.0), ("kiter", 7.0)):
            self.assertAlmostEqual(span[field], value)

    def test_the_spike_section_prints_it_per_kite_and_summed(self):
        text = self.text(ab.show_spike)
        self.assertIn("            closed on a spike no faster than it from +4.5 s: 3.0 s, gap 40.5 to 9.0 yd, spike"
                      " 3.5 yd/s, kiter 7.0 yd/s\n", text)
        self.assertIn("  kiter closing on a spike no faster than itself for 2+ s, ending within 12 yd: 1 stretch(es),"
                      " 3.0 s; Fel 0:16.500 3.0 s, gap 40.5 to 9.0 yd, spike 3.5 yd/s\n", text)

    def test_a_tank_form_lost_while_he_is_up_reads_the_act_row_that_dropped_it(self):
        self.assertEqual([(row["t"], row["guid"], row["spell"], row["phase"], (row["act"] or {}).get("a"),
                           row["parent"], row["back"], row["died"]) for row in ab.form_drops(self.trace)],
                         [(3000, CLPAL, SPELL_RIGHTEOUS_FURY, "P1", None, None, 3500, 50002),
                          (5000, CLDK, SPELL_FROST_PRESENCE, "P1", "unholy presence", None, 8000, None),
                          (45000, CLBEAR, SPELL_DIRE_BEAR, "P1b", "caster form", "anubarak innervate healer", None,
                           46200)])

    def test_the_tank_section_prints_each_one(self):
        self.assertIn("\n  a tank's form or stance lost while he was up, deaths left out, with the act row within 0.5 s"
                      " that dropped it:\n"
                      "     0:03.000 Bulwark        P1    Righteous Fury   no OK act row within 0.5 s;"
                      " back after 0.5 s\n"
                      "     0:05.000 Felesta        P1    Frost Presence   by unholy presence; back after 3.0 s\n"
                      "     0:45.000 Ecoterrorist   P1b   Dire Bear Form   by caster form, run for anubarak innervate"
                      " healer; not back before its death 1.2 s later\n", self.text(ab.show_tanks))


RFTANK, RFJUST, RFHOLY, RFSHAM, RFLOCK, RFMAGE, RFHUNT = range(9961, 9968)
RFBOSS = CREATURE | 2201
RFSCARAB = CREATURE | 2202
RFTOTEM = CREATURE | 2203
SPELL_LIFE_TAP, SPELL_EVOCATION, SPELL_JOL_HEAL, SPELL_HST_HEAL, SPELL_HOLY_LIGHT = 57946, 12051, 20267, 52042, 48782
SPELL_MANA_SPRING, SPELL_HEALING_STREAM, SPELL_SIPHON_LIFE = 58774, 58757, 63106
SPELL_JOL, SPELL_JOW = 20271, 53408
def sustain_hp(when: int) -> float:
    """100% to 60% by the submerge at 20 s, 40% at 43 s, 26% as phase 3 starts at 50 s, 10% by 90 s."""
    if when < 20000:
        return 100.0 - max(when, 0) / 500
    if when < 40000:
        return 60.0
    if when < 43000:
        return 60.0 - (when - 40000) / 150
    if when < 50000:
        return 40.0 - (when - 43000) / 500
    return 26.0 - (when - 50000) / 2500


def sustain_snap(when: int) -> dict:
    mana = {RFLOCK: 90.0 if when < 60000 else 35.0, RFMAGE: 55.0 if when < 60000 else 80.0}
    units = [[guid, X + 3.0 * index, Y - 20.0, Z, 0.0, 100.0, mana.get(guid, 100.0), 0, 0, 0, 0, 0]
             for index, guid in enumerate((RFTANK, RFJUST, RFHOLY, RFSHAM, RFLOCK, RFMAGE))]
    units.append([RFHUNT, X, Y + 10.0, Z, 0.0, 100.0, 100.0, 0, 1 if 21000 <= when < 24000 else 0, 0, 0, 0])
    units.append([RFBOSS, X, Y, Z, 0.0, sustain_hp(when), 0.0, RFTANK, 0, 0, 0, 0])
    if 20500 <= when <= 25500:
        units.append([RFSCARAB, X + 3.0, Y + 10.0, Z, 0.0, 100.0, 0.0, RFHUNT, 1, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": []}


def sustain_pull() -> list[dict]:
    """A 25H pull: P1 to 20 s, P2 to 40 s, P1b to 50 s and phase 3 to 90 s. anub.push latches at 45 s, he
    reaching 40% at 43 s. Two kite patches go in the first submerge, with Holylight's 180 s Hand. A scarab chases
    Trueshot from 20.5 s, who walks from 21 s to 24 s and holds after, a Mandibles stack each way. Agony taps twice
    and Power Evocates once from his 40%, a tap at 30 s left out. In phase 3 Justice's Light holds 52-60 s, cut
    by his Wisdom, and Holylight's 65-85 s; Elemena's Mana Spring gives way to Healing Stream at 55 s."""
    records = [
        {"e": "hdr", "v": 14, "ts": 1790700000000, "map": ab.MAP_TOC, "inst": 27, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": RFTANK, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 0},
                    {"g": RFJUST, "n": "Justice", "r": "melee", "c": 2, "h": 0},
                    {"g": RFHOLY, "n": "Holylight", "r": "heal", "c": 2, "h": 0},
                    {"g": RFSHAM, "n": "Elemena", "r": "heal", "c": 7, "h": 0},
                    {"g": RFLOCK, "n": "Agony", "r": "ranged", "c": 9, "h": 0},
                    {"g": RFMAGE, "n": "Power", "r": "ranged", "c": 8, "h": 0},
                    {"g": RFHUNT, "n": "Trueshot", "r": "ranged", "c": 3, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": RFBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 20000, "e": "unit", "g": RFSCARAB, "en": ab.NPC_SCARAB, "n": "Swarm Scarab"},
        {"t": 50000, "e": "unit", "g": RFTOTEM, "n": "Healing Stream Totem", "own": RFSHAM},
        *({"e": "spell", "sp": spell, "n": name} for spell, name in (
            (SPELL_LIFE_TAP, "Life Tap"), (SPELL_EVOCATION, "Evocation"), (SPELL_JOL_HEAL, "Judgement of Light"),
            (SPELL_HST_HEAL, "Healing Stream Totem"), (SPELL_HOLY_LIGHT, "Holy Light"),
            (SPELL_MANA_SPRING, "Mana Spring Totem"), (SPELL_HEALING_STREAM, "Healing Stream Totem"),
            (SPELL_SIPHON_LIFE, "Siphon Life"))),
        *(note(when, RFTANK, "anub.phase", value) for when, value in ((0, 1), (20000, 2), (40000, 1), (50000, 3))),
        *(note(when, RFHUNT, "anub.dps", value) for when, value in ((10000, "capped"), (12000, "burrower"),
                                                                     (14000, "capped"))),
        note(10000, RFJUST, "anub.dps", "capped"),
        note(45000, RFHUNT, "anub.push", "on:3:5"),
        note(25000, RFHUNT, "anub.kitespent", 1),
        note(30000, RFHUNT, "anub.kitespent", 2),
        cast(26000, RFHOLY, 10278, RFHUNT),
        note(27000, RFHOLY, "anub.hands", f"{RFHOLY}:179:10"),
        note(21000, RFHUNT, "anub.scarabkite", "walk"),
        note(24000, RFHUNT, "anub.scarabkite", "hold"),
        {**aura(22000, RFHUNT, 67863, False, RFSCARAB), "st": 1},
        {**aura(24500, RFHUNT, 67863, False, RFSCARAB), "st": 2},
        aura(60000, RFHUNT, 67863, True, RFSCARAB),
        *(cast(when, RFLOCK, SPELL_LIFE_TAP, RFLOCK) for when in (30000, 44000, 46000)),
        cast(47000, RFMAGE, SPELL_EVOCATION, RFMAGE),
        cast(10000, RFSHAM, SPELL_MANA_SPRING, RFSHAM),
        cast(55000, RFSHAM, SPELL_HEALING_STREAM, RFSHAM),
        cast(52000, RFJUST, SPELL_JOL, RFBOSS),
        cast(60000, RFJUST, SPELL_JOW, RFBOSS),
        cast(65000, RFHOLY, SPELL_JOL, RFBOSS),
        note(52000, RFJUST, "anub.judge", "Justice:90"),
        note(65000, RFHOLY, "anub.judge", "Holylight:70"),
        heal_row(55000, RFHOLY, RFHUNT, SPELL_HOLY_LIGHT, 5000, 1000),
        heal_row(56000, RFJUST, RFJUST, SPELL_JOL_HEAL, 2000),
        heal_row(57000, RFTOTEM, RFLOCK, SPELL_HST_HEAL, 2000),
        heal_row(58000, RFLOCK, RFLOCK, SPELL_SIPHON_LIFE, 1200),
        dmg(58000, RFBOSS, RFHUNT, ab.SPELL_LEECHING_SWARM_DMG, 8000),
        {"t": 90000, "e": "end", "out": "wipe"},
    ]
    records += [sustain_snap(when) for when in range(-500, 90001, 500)]
    return records


class SustainPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_27_anub-arak_1790700000.ndjson", sustain_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, show) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            show(self.trace)
        return out.getvalue()

    def test_the_push_reads_per_phase_1_with_his_health_and_what_ended_it(self):
        self.assertEqual([(row["label"], row["txt"], row["hp"], row["then"]) for row in ab.push_reads(self.trace)],
                         [("P1", None, None, "submerged"), ("P1b", "on:3:5", 36.0, "phase 3")])
        self.assertIn("  anub.push per phase 1 (on:<seconds needed>:<seconds left>), his health as it latched, the"
                      " submerge drag after it\n  (drag:<seconds needed>:<seconds left> as it left anyway), and what"
                      " ended the phase: P1 no push, submerged; P1b on:3:5 at 36.0%, 5 s to its end, no drag after"
                      " it, phase 3\n", self.text(ab.show_dps))

    def test_only_a_ranged_bots_capped_rows_count(self):
        self.assertEqual(ab.ranged_capped(self.trace), {RFHUNT: 2})
        self.assertIn("  anub.dps capped rows per ranged DPS bot, a pick moved onto him off a burrower it was capped"
                      " on: Trueshot 2\n", self.text(ab.show_burrowers))

    def test_kite_patches_and_hands_per_submerge(self):
        self.assertEqual(ab.submerge_spends(self.trace),
                         [{"index": 1, "start": 20000, "patches": 2, "hands": [(26000, RFHOLY, 180000)]}])
        self.assertIn("  per submerge the kite patches anub.kitespent counted, over 1 before submerge 3 flagged, and"
                      " the Hands of Protection\n  cast with the caster's cooldown:\n"
                      "    1  0:20.000  patches 2 over the reserve; Hands Holylight 0:26.000 (180 s)\n",
                      self.text(ab.show_spike))

    def test_warlock_and_mage_mana_with_their_top_ups_from_his_40_percent(self):
        read = ab.caster_mana(self.trace)
        self.assertEqual((read["from"], read["mana"]), (43000, {RFLOCK: (90.0, 35.0), RFMAGE: (55.0, 80.0)}))
        self.assertEqual(read["topups"], {RFLOCK: {"Life Tap": 2}, RFMAGE: {"Evocation": 1}})
        self.assertIn("  warlock and mage bots, mana the same way, and their Life Taps and Evocations from his 40%"
                      " (0:43.000) to phase 3:\n    Agony          90% > 35%; Life Tap 2\n"
                      "    Power          55% > 80%; Evocation 1\n", self.text(ab.show_healers))

    def test_a_scarab_victims_stacks_moving_or_standing_and_its_kite_seconds(self):
        chase = ab.scarab_chases(self.trace)[RFHUNT]
        self.assertEqual(chase["stacks"], [(22000, True, 3.0), (24500, False, 3.0)])
        self.assertEqual(dict(chase["chased"]), {"-": 500, "walk": 3000, "hold": 2000})
        self.assertIn("    Trueshot       stacks 1 moving, 1 standing, median gap 3.0 yd; chased 5.5 s (walk 3.0,"
                      " hold 2.0, - 0.5)\n", self.text(ab.show_scarabs))

    def test_phase_3_sustain_by_kind_light_uptime_and_water_totems(self):
        rows = {row["guid"]: row for row in ab.swarm_sustain(self.trace, 50000, 90000, ab.Samples(self.trace))}
        self.assertNotIn(RFTANK, rows)
        self.assertEqual((dict(rows[RFHUNT]["heals"]), rows[RFHUNT]["swarm"], rows[RFHUNT]["alive"]),
                         ({"healers": 4000}, 8000, 40000))
        self.assertEqual(dict(rows[RFJUST]["heals"]), {"JoL": 2000})
        self.assertEqual(dict(rows[RFLOCK]["heals"]), {"HST": 2000, "own": 1200})
        light = ab.light_reads(self.trace, 50000, 90000)
        self.assertEqual(light["uptime"], 28000)
        self.assertEqual(light["casts"], {RFJUST: {("Light", True): 1, ("Wisdom", True): 1},
                                          RFHOLY: {("Light", True): 1}})
        self.assertEqual(ab.water_totems(self.trace, 50000), {RFSHAM: ("Mana Spring Totem", "Healing Stream Totem")})
        text = self.text(ab.show_swarm)
        self.assertIn("    Trueshot       ranged    40     0.10    0.00    0.00    0.00    0.00    0.00    0.00"
                      "    0.10    0.20    0.00 short     -       no party heal\n", text)
        self.assertIn("  Judgement of Light on him in phase 3: 28.0 of 40.0 s; judgements per paladin: Holylight"
                      " Light 1 on him; Justice Light 1 on him, Wisdom 1 on him\n"
                      "  anub.judge hand-overs: 0:52.000 Justice:90, 1:05.000 Holylight:70\n"
                      "  water totem per shaman at phase 3's start and 10 s in: Elemena Mana Spring Totem > Healing"
                      " Stream Totem\n", text)


LPTANK, LPMELEE, LPSWING, LPRANGED, LPHEAL = range(9971, 9976)
LPBOSS = CREATURE | 2301
LPSHALLOW = CREATURE | 2302
LPDEEP = CREATURE | 2303
LP_PATCHES = ((X + 10.0, Y + 12.0), (X + 10.0, Y - 12.0))
LP_BOSS_SPOT = (X + 30.0, Y)


def loop_snap(when: int) -> dict:
    """The melee bot walks 7 yd/s at him from 30 yd, reaching 6 yd at 3.5 s; the ranged bot stands 40 yd off
    and the healer has no target. In phase 3 the melee bot is on the shallow burrower, 3.3 yd out of its
    patch, the other one is 2.6 yd into the other patch."""
    melee_x = X + min(max(when, 0) / 1000 * 7.0, 24.5)
    units = [[LPTANK, *LP_BOSS_SPOT, Z, 0.0, 100.0, 100.0, LPBOSS, 0, 0, 0, 0],
             [LPMELEE, melee_x, Y, Z, 0.0, 100.0, 100.0, LPSHALLOW if when >= 61000 else LPBOSS, 0, 0, 0, 0],
             [LPSWING, X - 20.0, Y, Z, 0.0, 100.0, 100.0, LPBOSS, 0, 0, 0, 0],
             [LPRANGED, X - 10.0, Y, Z, 0.0, 100.0, 100.0, LPBOSS, 0, 0, 0, 0],
             [LPHEAL, X, Y + 5.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [LPBOSS, *LP_BOSS_SPOT, Z, 0.0, 100.0 - max(when, 0) / 1000, 0.0, LPTANK, 0, 0, 0, 0]]
    if when >= 60000:
        units.append([LPSHALLOW, LP_PATCHES[0][0] + 3.3, LP_PATCHES[0][1], Z, 0.0, 90.0, 0.0, LPTANK, 0, 0, 0, 0])
        units.append([LPDEEP, LP_PATCHES[1][0] + 2.6, LP_PATCHES[1][1], Z, 0.0, 90.0, 0.0, LPTANK, 0, 0, 0, 0])
    hz = [[ab.PERMAFROST[3], x, y, PATCH_Z, 6.0, 1] for x, y in LP_PATCHES]
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def loop_pull() -> list[dict]:
    """A 25H pull: P1 to 20 s, P2 to 40 s, P1b to 60 s, phase 3 to 80 s. The P1 push holds with no drag after
    it; the P1b one drags anyway at 50 s. In P2 a melee bot swings between its gather and a burrower stand 20 yd
    off, a second gather row within 4 yd of the first being the same goal."""
    gather, stand = "anubarak gather", "anubarak melee burrower stand"
    records = [
        {"e": "hdr", "v": 14, "ts": 1790800000000, "map": ab.MAP_TOC, "inst": 28, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": LPTANK, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 0},
                    {"g": LPMELEE, "n": "Shadow", "r": "melee", "c": 4, "h": 0},
                    {"g": LPSWING, "n": "Angry", "r": "melee", "c": 1, "h": 0},
                    {"g": LPRANGED, "n": "Agony", "r": "ranged", "c": 9, "h": 0},
                    {"g": LPHEAL, "n": "Prayer", "r": "heal", "c": 5, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": LPBOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 60000, "e": "unit", "g": LPSHALLOW, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        {"t": 60000, "e": "unit", "g": LPDEEP, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        *(note(when, LPTANK, "anub.phase", value) for when, value in ((0, 1), (20000, 2), (40000, 1), (60000, 3))),
        note(14000, LPRANGED, "anub.push", "on:2:6"),
        note(45000, LPRANGED, "anub.push", "on:3:5"),
        note(50000, LPTANK, "anub.push", "drag:9:12"),
        note(50000, LPTANK, "anub.pickup", "submerge"),
        note(61000, LPMELEE, "anub.focus0", LPSHALLOW),
        note(61000, LPMELEE, "anub.focus1", LPDEEP),
        move_row(21000, LPSWING, (X - 20.0, Y), gather),
        move_row(21500, LPSWING, (X - 19.0, Y), gather),
        move_row(24000, LPSWING, (X, Y), stand),
        move_row(27000, LPSWING, (X - 20.0, Y + 1.0), gather),
        move_row(30000, LPSWING, (X, Y), stand),
        move_row(33000, LPSWING, (X - 20.0, Y), gather),
        {"t": 80000, "e": "end", "out": "wipe"},
    ]
    records += [loop_snap(when) for when in range(-500, 80001, 500)]
    return records


class LoopPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_28_anub-arak_1790800000.ndjson", loop_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, show) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            show(self.trace)
        return out.getvalue()

    def test_a_swing_is_a_goal_another_8_yd_off_and_the_first_again(self):
        rows = {row["pair"]: row for row in ab.swing_rows(self.trace)}
        self.assertEqual({pair: (row["phase"], row["role"], row["swings"], dict(row["bots"]))
                          for pair, row in rows.items()},
                         {("anubarak gather", "anubarak melee burrower stand"): ("P2", "melee", 2, {LPSWING: 2}),
                          ("anubarak melee burrower stand", "anubarak gather"): ("P2", "melee", 1, {LPSWING: 1})})
        # twice the 20 yd gap, then twice the 20.02 yd one from the gather a yard north
        self.assertAlmostEqual(rows[("anubarak gather", "anubarak melee burrower stand")]["yards"],
                               40.0 + 2.0 * math.hypot(20.0, 1.0), places=3)
        text = self.text(ab.show_swings)
        self.assertIn("    P2    melee     2     80 yd  anubarak gather | anubarak melee burrower stand: Angry 2\n", text)
        self.assertNotIn("anubarak melee burrower stand | anubarak gather", text)

    def test_the_opening_walk_per_role(self):
        walk = ab.opening_walk(self.trace)
        self.assertEqual({role: (row["bots"], round(row["yards"], 1), row["reach"], row["never"])
                          for role, row in walk.items()},
                         {"tank": (1, 0.0, 0, 0), "melee": (2, 40.0, 3500, 1), "ranged": (1, 40.0, None, 1),
                          "heal": (1, 30.4, None, 1)})
        self.assertIn("over those in reach within 40 s: tank 1 bot(s) 0.0 yd, 0.0 s; heal 1 bot(s) 30.4 yd, never;"
                      " melee 2 bot(s) 40.0 yd, 3.5 s, 1 never; ranged 1 bot(s) 40.0 yd, never\n",
                      self.text(ab.show_pull))

    def test_the_push_line_names_the_drag_after_it(self):
        self.assertEqual([(row["label"], row["txt"], row["drag"], row["dragtxt"]) for row in ab.push_reads(self.trace)],
                         [("P1", "on:2:6", None, None), ("P1b", "on:3:5", 50000, "drag:9:12")])
        self.assertIn("P1 on:2:6 at 86.0%, 6 s to its end, no drag after it, submerged; P1b on:3:5 at 55.0%, 15 s"
                      " to its end, drag left 10 s before its end (drag:9:12), phase 3\n", self.text(ab.show_dps))

    def test_phase_3_focus_depth_ring_reach_and_melee_on_it(self):
        rows = [(row["t"], row["side"], row["guid"], round(row["depth"], 1), row["reach"], row["peak"],
                 row["median"]) for row in ab.swarm_focus_reach(self.trace)]
        self.assertEqual(rows, [(61000, 0, LPSHALLOW, 3.3, True, 1, 1), (61000, 1, LPDEEP, 2.6, False, 0, 0)])
        text = self.text(ab.show_burrowers)
        self.assertIn(f"     1:01.000 #{LPSHALLOW & 0xFFFFFFFF}      side 0  3.3 yd  reach  melee 1 peak, 1 median\n",
                      text)
        self.assertIn(f"     1:01.000 #{LPDEEP & 0xFFFFFFFF}      side 1  2.6 yd  deep   melee 0 peak, 0 median\n", text)


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(trace)
            for _, _, section in ab.SECTIONS:
                section(trace)
        self.assertIn("not a Trial of the Crusader Anub'arak pull", out.getvalue())
        self.assertNotIn("locked out", out.getvalue())
        self.assertIn("HEALERS\n  not an Anub'arak pull\n", out.getvalue())
        self.assertIn("TANKS\n  not an Anub'arak pull\n", out.getvalue())
        for title in ("SCARABS", "REDIRECTS", "PURSUIT", "DPS"):
            self.assertIn(f"{title}\n  not an Anub'arak pull\n", out.getvalue())

    def test_the_help_builds(self):
        # argparse formats each help with %, so a bare "40%," kills the reader before it opens a trace.
        out = io.StringIO()
        with unittest.mock.patch.object(sys, "argv", ["anub_arak.py", "--help"]), \
                contextlib.redirect_stdout(out), self.assertRaises(SystemExit) as stop:
            ab.main()
        self.assertEqual(stop.exception.code, 0)
        self.assertIn("--swarm", out.getvalue())

    def test_azjol_nerubs_anubarak_is_not_this_one(self):
        with tempfile.TemporaryDirectory() as folder:
            trace = write_trace(folder, "601_4_anub-arak_1789500000.ndjson", anub_pull(map_id=601))
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                ab.show_banner(trace)
        self.assertIn("not a Trial of the Crusader Anub'arak pull", out.getvalue())


R28_DK0, R28_DK1, R28_BULWARK, R28_JUSTICE, R28_LOCK, R28_KITER = 6101, 6102, 6103, 6104, 6105, 6106
R28_SPIKE = CREATURE | 6200
R28_SA, R28_SB = CREATURE | 6211, CREATURE | 6212
R28_BW1, R28_BW2 = CREATURE | 6221, CREATURE | 6222
# side 1's grip stand and patch; side 0's grip lands 15 yd north of it
R28_PATCH = (X + 30.0, Y - 12.0)
R28_LANDING = (X + 30.0, Y + 3.0)


def r28_snap(when: int) -> dict:
    players = {R28_DK0: R28_LANDING, R28_DK1: R28_PATCH, R28_BULWARK: (X - 40.0, Y + 40.0),
               R28_JUSTICE: (X - 10.0, Y + 30.0), R28_LOCK: (X - 10.0, Y + 10.0), R28_KITER: (X, Y - 20.0)}
    units = [[guid, x, y, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0] for guid, (x, y) in players.items()]
    units.append([R28_SA, *R28_PATCH, PATCH_Z if when >= 1500 else FLYING_Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
    units.append([R28_SB, *R28_LANDING, Z + 1.0 if when >= 4000 else FLYING_Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
    # the spike fails by the kite patch at 9 s and the next mark starts 4 s on, 20.6 yd further
    spike = (X, Y) if when <= 9000 else (X + 20.0, Y - 5.0)
    units.append([R28_SPIKE, *spike, Z, 0.0, 100.0, 0.0, R28_KITER if when <= 9000 else R28_LOCK, 0, 0, 0, 0])
    # both burrowers sit inside the Death and Decay for its first 5 s, then 20 yd off it
    off = 2.0 if when <= 15000 else 20.0
    for guid, side in ((R28_BW1, 1.0), (R28_BW2, -1.0)):
        units.append([guid, R28_PATCH[0] + side * off, R28_PATCH[1], Z, 0.0, 100.0, 0.0, R28_DK1, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": []}


def r28_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1790800000000, "map": ab.MAP_TOC, "inst": 4, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": R28_DK0, "n": "Obliteration", "r": "melee", "c": 6, "h": 0},
             {"g": R28_DK1, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
             {"g": R28_BULWARK, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
             {"g": R28_JUSTICE, "n": "Justice", "r": "heal", "c": 2, "h": 0},
             {"g": R28_LOCK, "n": "Agony", "r": "ranged", "c": 9, "h": 0},
             {"g": R28_KITER, "n": "Trueshot", "r": "ranged", "c": 3, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": 0, "e": "unit", "g": R28_SPIKE, "en": ab.NPC_SPIKE, "n": "Anub'arak"},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"} for guid in (R28_SA, R28_SB)),
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"}
          for guid in (R28_BW1, R28_BW2)),
        {"e": "spell", "sp": 49938, "n": "Death and Decay"},
        note(0, R28_DK1, "anub.side1", R28_DK1),
        note(900, R28_DK1, "anub.sphere", "grip1"),
        *grip(1000, R28_DK1, R28_SA),
        note(3000, R28_DK1, "anub.patch1", R28_SA),
        note(3400, R28_DK0, "anub.sphere", "grip0"),
        *grip(3500, R28_DK0, R28_SB),
        aura(6000, R28_LOCK, ab.SPELL_CIRCLE_IN_RANGE, False, R28_LOCK),
        cast(8600, R28_LOCK, ab.SPELL_CIRCLE_TELEPORT, R28_LOCK),
        cast(10000, R28_DK1, 49938, R28_BW1),
        note(0, R28_BULWARK, "toc.aura", "frost"),
        note(15000, R28_BULWARK, "toc.aura", "none"),
        note(0, R28_JUSTICE, "toc.aura", "none"),
        {"t": 20000, "e": "end", "out": "wipe"},
    ]
    records += [r28_snap(when) for when in range(0, 20500, 500)]
    return records


class Refine28Lines(unittest.TestCase):
    """The grip's landing against the other side's patch, a kite patch's yards to a tank patch, the spike's walk
    after a patch, the Hands and the circle at a patch kite's turn, a planned teleport, a side tank's ground area
    emptying, the paladins' resistance auras, and the port's next leg walking on past the fail."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_4_anub-arak_1790800000.ndjson", r28_pull())
        cls.samples = ab.Samples(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_grip_landing_reads_its_yards_to_the_other_sides_patch(self):
        first, second = ab.grip_rows(self.trace, self.samples)
        self.assertIsNone(first["other_gap"])
        self.assertAlmostEqual(second["other_gap"], 15.0)

    def test_a_kite_patch_reads_its_yards_to_the_nearest_tank_patch(self):
        death = {"killed": 4000, "landed": (X - 20.0, Y)}
        gap = ab.kite_patch_tank_gap(death, ab.patch_latches(self.trace), self.samples)
        self.assertAlmostEqual(gap, math.hypot(50.0, 12.0))
        self.assertIsNone(ab.kite_patch_tank_gap({"killed": 0, "landed": (X, Y)}, ab.patch_latches(self.trace),
                                                 self.samples))

    def test_the_spikes_walk_after_a_patch_and_its_restart_by_a_tank_patch(self):
        windows = [{"guid": R28_KITER, "start": 5000, "stop": 9000, "outcome": "patch"},
                   {"guid": R28_LOCK, "start": 13000, "stop": 19000, "outcome": "impale"}]
        (row,) = ab.restart_rows(self.trace, windows, self.samples)
        self.assertAlmostEqual(row["walk"], math.hypot(20.0, 5.0))
        self.assertAlmostEqual(row["tank"], math.hypot(10.0, 7.0))
        self.assertEqual(ab.restart_rows(self.trace, [{**windows[0], "stop": 7000}, windows[1]], self.samples), [])

    def test_a_patch_kites_turn_reads_the_hands_in_reach_and_the_circle(self):
        window = {"guid": R28_LOCK, "start": 5000, "stop": 9000, "outcome": "patch", "turn": 7000}
        (row,) = ab.patch_turn_rows(self.trace, [window, {**window, "turn": None}], self.samples)
        self.assertEqual((row["hands"], row["circle"]), (1, True))

    def test_a_planned_teleport_reads_its_second_and_the_cast(self):
        self.assertEqual(ab.kitepath_second("ring:0:ccw:36+boost@1.0+tp@2.5", "boost"), 1.0)
        self.assertEqual(ab.kitepath_boost("ring:0:ccw:36+boost@1.0+tp@2.5"), 1.0)
        self.assertEqual(ab.kitepath_second("ring:0:ccw:36+boost@1.0+tp@2.5", "tp"), 2.5)
        window = {"guid": R28_LOCK, "start": 5000, "stop": 20000,
                  "kitepath": [{"at": 6000, "value": "ring:0:ccw:36+tp@2.5"}]}
        (row,) = ab.teleport_plans(self.trace, [window, {**window, "kitepath": [{"at": 6000, "value": "arc:0:0"}]}])
        self.assertEqual((row["planned"], row["cast"]), (8500, 8600))

    def test_a_ground_area_reads_the_burrowers_inside_each_second(self):
        (row,) = ab.ground_area_rows(self.trace, self.samples)
        self.assertEqual((row["tank"], row["name"], row["inside"], row["empty"]),
                         (R28_DK1, "death and decay", [2, 2, 2, 2, 2, 0, 0, 0, 0, 0], 5))

    def test_each_paladins_resistance_aura_and_a_tank_on_one(self):
        rows = {row["guid"]: row for row in ab.paladin_aura_rows(self.trace)}
        self.assertEqual(dict(rows[R28_BULWARK]["held"]), {"frost": 15000, "none": 5000})
        self.assertTrue(rows[R28_BULWARK]["tank"])
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_paladin_auras(self.trace)
        self.assertIn("Bulwark frost 15 s, none 5 s (tank on a resistance)", out.getvalue())
        self.assertIn("Justice none 20 s", out.getvalue())

    def test_the_ports_next_leg_walks_on_past_the_fail(self):
        rules = ab.planner_rules()
        # the spike fails on the kite patch heading east; a tank patch 10 yd past the walk's end lies behind its
        # leg west to the gather, so only the walk passes it
        past = rules["driftReach"] + 10.0
        patches = [(X - 5.0, Y, False, True, False), (X + past, Y, True, False, False)]
        world = ab.KiteWorld((X, Y), (X, Y, 3.5, 0, 7.0), 40.0, patches, gather=(X - 30.0, Y))
        spike = (X, Y, 11.4, 2, 7.0)
        still = ab.planner_finish(world, rules, 400, 50, (X, Y), 5.0, -1, spike, 50, 0, True)
        self.assertEqual(still.tank, -1)
        self.assertTrue(still.kite_patch)
        walked = ab.planner_finish(world, rules, 400, 50, (X, Y), 5.0, -1, spike, 50, 0, True, heading=(1.0, 0.0))
        self.assertEqual(walked.tank, 50)


R29_DK, R29_MIGHTY, R29_HEAL = 6301, 6302, 6303
R29_BW1, R29_BW2 = CREATURE | 6311, CREATURE | 6312
SPELL_DND, SPELL_BLOOD_BOIL, SPELL_DARK_COMMAND, SPELL_ICY_TOUCH = 49938, 49941, 56222, 49909


def r29_snap(when: int) -> dict:
    units = [[guid, x, y, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0]
             for guid, (x, y) in ((R29_DK, (X, Y)), (R29_MIGHTY, (X + 10.0, Y + 20.0)), (R29_HEAL, (X - 30.0, Y)))]
    # taunted from 20 yd at 2 s and never hit, on the healer once the taunt ran out
    first = R29_DK if 2000 <= when < 5000 else R29_HEAL if when >= 5000 else 0
    units.append([R29_BW1, X + 20.0, Y, Z, 0.0, 100.0, 0.0, first, 0, 0, 0, 0])
    # 30 yd off the Bladestorm at 6 s, walks in to 2 yd of it by 9 s; taunted at 10 s and hit, lost all the same
    walked = min(max(when - 6000, 0), 3000) / 3000
    second = R29_DK if 10000 <= when < 13000 else R29_MIGHTY
    units.append([R29_BW2, X + 40.0 - 28.0 * walked, Y + 20.0, Z, 0.0, 100.0, 0.0, second, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": []}


def r29_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1790900000000, "map": ab.MAP_TOC, "inst": 5, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": R29_DK, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
             {"g": R29_MIGHTY, "n": "Mighty", "r": "melee", "c": 1, "h": 0},
             {"g": R29_HEAL, "n": "Elemena", "r": "heal", "c": 7, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"}
          for guid in (R29_BW1, R29_BW2)),
        {"e": "spell", "sp": SPELL_DND, "n": "Death and Decay"},
        {"e": "spell", "sp": SPELL_BLOOD_BOIL, "n": "Blood Boil"},
        {"e": "spell", "sp": SPELL_DARK_COMMAND, "n": "Dark Command"},
        {"e": "spell", "sp": SPELL_ICY_TOUCH, "n": "Icy Touch"},
        note(0, R29_DK, "anub.side1", R29_DK),
        # two Death and Decay picks, one FAILED under each; the FAILED row under none isn't the pick's
        note(1000, R29_DK, "anub.saoe", "death and decay"),
        act(1000, R29_DK, ab.SIDE_AOE_ACTION, "FAILED"),
        note(1500, R29_DK, "anub.saoe", "none"),
        act(1600, R29_DK, ab.SIDE_AOE_ACTION, "FAILED"),
        cast(2000, R29_DK, SPELL_DARK_COMMAND, R29_BW1),
        note(3000, R29_DK, "anub.saoe", "death and decay"),
        act(3000, R29_DK, "death and decay", "FAILED"),
        note(3500, R29_DK, "anub.saoe", "blood boil"),
        cast(3500, R29_DK, SPELL_BLOOD_BOIL, 0),
        cast(4000, R29_DK, SPELL_DND, 0),
        cast(6000, R29_MIGHTY, ab.SPELL_BLADESTORM, 0),
        cast(10000, R29_DK, SPELL_DARK_COMMAND, R29_BW2),
        cast(11000, R29_DK, SPELL_ICY_TOUCH, R29_BW2),
        {"t": 16000, "e": "end", "out": "wipe"},
    ]
    records += [r29_snap(when) for when in range(0, 16500, 500)]
    return records


class Refine29Lines(unittest.TestCase):
    """A side tank's area picks against its casts and failures, the hits inside each taunt, and the burrower
    closest over a Bladestorm."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_5_anub-arak_1790900000.ndjson", r29_pull())
        cls.samples = ab.Samples(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, show, *args) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            show(self.trace, *args)
        return out.getvalue()

    def test_area_picks_read_against_casts_and_failures_under_them(self):
        self.assertEqual(ab.side_aoe_picks(self.trace),
                         {R29_DK: {"death and decay": [2, 1, 2], "blood boil": [1, 1, 0]}})
        self.assertIn("    Felesta        death and decay 2 picked, 1 cast, 2 failed; blood boil 1 picked, 1 cast,"
                      " 0 failed\n", self.text(ab.show_side_tank_aoe, self.samples))

    def test_a_taunt_reads_its_end_yards_and_the_hits_inside_it(self):
        rows = ab.pickup_taunts(self.trace, self.samples)
        self.assertEqual([(row["guid"], round(row["yd"], 1), round(row["yd_end"], 1), row["hits"], row["to"])
                          for row in rows],
                         [(R29_BW1, 20.0, 20.0, 0, R29_HEAL), (R29_BW2, 23.3, 23.3, 1, R29_MIGHTY)])
        self.assertIn(f"    Felesta        1 of 2 lost: 0:02.000 #{R29_BW1 & 0xFFFFFFFF} 20.0 > 20.0 yd\n",
                      self.text(ab.show_pickup_taunts, self.samples))

    def test_a_bladestorm_reads_the_burrower_that_walked_into_it(self):
        (row,) = ab.bladestorm_casts(self.trace, self.samples)
        self.assertEqual((row["burrower"], round(row["yards"], 1), row["passer"], round(row["pass_yd"], 1),
                          round(row["passer_cast_yd"], 1)), (R29_BW1, 22.4, R29_BW2, 2.0, 30.0))
        self.assertIn(f"Mighty 0:06.000 #{R29_BW2 & 0xFFFFFFFF} 30.0 > 2.0 yd in; in 1 of 1\n",
                      self.text(ab.show_burrower_threat, self.samples))


R30_DK, R30_HEAL1, R30_HEAL2, R30_ROGUE, R30_MAGE, R30_WARRIOR = 6401, 6402, 6403, 6404, 6405, 6406
SPELL_PENETRATING_COLD_25H, SPELL_CLOAK = 68510, 31224


def r30_snap(when: int) -> dict:
    # out to 15 yd west of its spot from 12 to 13 s and back by 16 s; the healers 30 yd east of the spot
    out = min(max(when - 12000, 0), 1000) / 1000 - min(max(when - 15000, 0), 1000) / 1000
    units = [[R30_DK, X - 15.0 * out, Y, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [R30_HEAL1, X + 30.0, Y, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
             [R30_HEAL2, X + 30.0, Y + 2.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0]]
    units += [[guid, X + 5.0, Y + 5.0, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0]
              for guid in (R30_ROGUE, R30_MAGE, R30_WARRIOR)]
    return {"t": when, "e": "snap", "u": units, "hz": []}


def r30_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1790910000000, "map": ab.MAP_TOC, "inst": 5, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": R30_DK, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
             {"g": R30_HEAL1, "n": "Elemena", "r": "heal", "c": 7, "h": 0},
             {"g": R30_HEAL2, "n": "Prayer", "r": "heal", "c": 5, "h": 0},
             {"g": R30_ROGUE, "n": "Shadow", "r": "melee", "c": 4, "h": 0},
             {"g": R30_MAGE, "n": "Power", "r": "ranged", "c": 8, "h": 0},
             {"g": R30_WARRIOR, "n": "Angry", "r": "melee", "c": 1, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"e": "spell", "sp": SPELL_CLOAK, "n": "Cloak of Shadows"},
        note(0, R30_DK, "anub.side1", R30_DK),
        note(0, R30_DK, "anub.phase", 1),
        note(10000, R30_DK, "anub.phase", 3),
        # the walk out before the stretch, and the one back inside it
        move_row(12000, R30_DK, (X - 15.0, Y), "reach melee"),
        move_row(14500, R30_DK, (X, Y), "anubarak assist tank hold burrower"),
        # before phase 3, so not counted
        aura(5000, R30_ROGUE, SPELL_PENETRATING_COLD_25H, False),
        cast(5200, R30_ROGUE, SPELL_CLOAK, R30_ROGUE),
        aura(5200, R30_ROGUE, SPELL_PENETRATING_COLD_25H, True),
        # Cloaked off half a second in; the mage carried its 6 s with nothing; a warrior has no save
        aura(11000, R30_ROGUE, SPELL_PENETRATING_COLD_25H, False),
        cast(11500, R30_ROGUE, SPELL_CLOAK, R30_ROGUE),
        aura(11500, R30_ROGUE, SPELL_PENETRATING_COLD_25H, True),
        aura(11000, R30_MAGE, SPELL_PENETRATING_COLD_25H, False),
        aura(17000, R30_MAGE, SPELL_PENETRATING_COLD_25H, True),
        aura(11000, R30_WARRIOR, SPELL_PENETRATING_COLD_25H, False),
        aura(17000, R30_WARRIOR, SPELL_PENETRATING_COLD_25H, True),
        {"t": 20000, "e": "end", "out": "wipe"},
    ]
    records += [r30_snap(when) for when in range(0, 20500, 500)]
    return records


class Refine30Lines(unittest.TestCase):
    """A phase 3 side tank with under two healers in reach and the walk before it, and the carriers' own
    Penetrating Cold saves per class."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_5_anub-arak_1790910000.ndjson", r30_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, show, *args) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            show(self.trace, *args)
        return out.getvalue()

    def test_a_side_tank_out_of_two_healers_reach_reads_its_stretch_and_walks(self):
        rows = ab.thin_cover_rows(self.trace)
        self.assertEqual([(row["side"], row["tank"], row["start"], row["stop"], row["ms"], dict(row["walks"]))
                          for row in rows],
                         [(1, R30_DK, 13000, 15000, 2500,
                           {"reach melee": 1, "anubarak assist tank hold burrower": 1})])
        self.assertIn("    side 1 Felesta        2.5 s: 0:13.000-0:15.000 reach melee 1, anubarak assist tank hold"
                      " burrower 1\n", self.text(ab.show_thin_cover))

    def test_a_carrier_save_counts_per_class_in_phase_3_only(self):
        rows = ab.cold_self_saves(self.trace, 10000, 20000)
        self.assertEqual(sorted((row["guid"], row["start"], row["class"], row["saves"]) for row in rows),
                         [(R30_ROGUE, 11000, "rogue", [(11500, SPELL_CLOAK)]), (R30_MAGE, 11000, "mage", [])])
        text = self.text(ab.show_cold_self_saves, 10000, 20000)
        self.assertIn("per class: mage 0/1, rogue 1/1\n", text)
        self.assertIn("    Shadow 0:11.000 Cloak of Shadows +0.5 s\n", text)


R31_TANK, R31_TREE, R31_ROGUE, R31_MAGE, R31_PRIEST, R31_SHAM = 6501, 6502, 6503, 6504, 6505, 6506
R31_BURROWER, R31_TOTEM = 0xF130008717006507, 0xF130009AB1006508
R31_ICE_BLOCK, R31_MANA_SHIELD, R31_HEALTHSTONE, R31_SHIELD = 45438, 43020, 47877, 48066
R31_REJUVENATION, R31_PLAGUE, R31_EMBRACE, R31_STREAM = 48441, 48300, 15290, 52042


def r31_snap(when: int) -> dict:
    # the burrower is on the mage from 18 s
    units = [[guid, X + 5.0, Y + 5.0, Z, 0.0, 30.0, 100.0, 0, 0, 0, 0, 0]
             for guid in (R31_TANK, R31_TREE, R31_ROGUE, R31_MAGE, R31_PRIEST, R31_SHAM)]
    units.append([R31_BURROWER, X - 5.0, Y, Z, 0.0, 100.0, 0.0, R31_MAGE if when >= 18000 else R31_TANK, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": []}


def r31_heal(when: int, source: int, target: int, spell: int, amount: int) -> dict:
    return {"t": when, "e": "heal", "s": source, "d": target, "sp": spell, "a": amount, "oh": 0}


def r31_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1790920000000, "map": ab.MAP_TOC, "inst": 6, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": R31_TANK, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
             {"g": R31_TREE, "n": "Tree", "r": "heal", "c": 11, "h": 0},
             {"g": R31_ROGUE, "n": "Shadow", "r": "melee", "c": 4, "h": 0},
             {"g": R31_MAGE, "n": "Power", "r": "ranged", "c": 8, "h": 0},
             {"g": R31_PRIEST, "n": "Malediction", "r": "ranged", "c": 5, "h": 0},
             {"g": R31_SHAM, "n": "Stormweaver", "r": "ranged", "c": 7, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": 0, "e": "unit", "g": R31_BURROWER, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        {"t": 0, "e": "unit", "g": R31_TOTEM, "en": 3527, "n": "Healing Stream Totem IX", "own": R31_SHAM},
        {"e": "spell", "sp": SPELL_CLOAK, "n": "Cloak of Shadows"},
        {"e": "spell", "sp": R31_ICE_BLOCK, "n": "Ice Block"},
        {"e": "spell", "sp": R31_MANA_SHIELD, "n": "Mana Shield"},
        {"e": "spell", "sp": R31_HEALTHSTONE, "n": "Master Healthstone"},
        {"e": "spell", "sp": R31_SHIELD, "n": "Power Word: Shield"},
        {"e": "spell", "sp": R31_REJUVENATION, "n": "Rejuvenation"},
        {"e": "spell", "sp": R31_PLAGUE, "n": "Devouring Plague"},
        {"e": "spell", "sp": R31_EMBRACE, "n": "Vampiric Embrace"},
        {"e": "spell", "sp": R31_STREAM, "n": "Healing Stream Totem"},
        note(0, R31_TANK, "anub.phase", 1),
        note(10000, R31_TANK, "anub.phase", 3),
        # the rogue's Cloak under the swarm alone, then one on Penetrating Cold
        cast(12000, R31_ROGUE, SPELL_CLOAK, R31_ROGUE),
        aura(15000, R31_ROGUE, SPELL_PENETRATING_COLD_25H, False),
        cast(15500, R31_ROGUE, SPELL_CLOAK, R31_ROGUE),
        aura(15500, R31_ROGUE, SPELL_PENETRATING_COLD_25H, True),
        # the mage's Mana Shield on nothing, its Ice Block with the burrower on it
        cast(13000, R31_MAGE, R31_MANA_SHIELD, R31_MAGE),
        cast(19000, R31_MAGE, R31_ICE_BLOCK, R31_MAGE),
        # Power Word: Shield onto the tank is a heal, onto the priest itself a save
        cast(14000, R31_PRIEST, R31_SHIELD, R31_TANK),
        cast(14500, R31_PRIEST, R31_SHIELD, R31_PRIEST),
        # Tree's Rejuvenation onto the rogue and onto itself, its Healthstone; the priest's own plague heal
        cast(13000, R31_TREE, R31_REJUVENATION, R31_ROGUE),
        r31_heal(13000, R31_TREE, R31_ROGUE, R31_REJUVENATION, 1500),
        cast(16000, R31_TREE, R31_REJUVENATION, R31_TREE),
        r31_heal(16000, R31_TREE, R31_TREE, R31_REJUVENATION, 2000),
        cast(17000, R31_TREE, R31_HEALTHSTONE, R31_TREE),
        r31_heal(17000, R31_TREE, R31_TREE, R31_HEALTHSTONE, 5000),
        cast(12000, R31_PRIEST, R31_PLAGUE, BOSS),
        r31_heal(12500, R31_PRIEST, R31_PRIEST, R31_PLAGUE, 800),
        # the party heals: the totem onto the mage and its shaman, Vampiric Embrace onto the mage and the priest
        r31_heal(14000, R31_TOTEM, R31_MAGE, R31_STREAM, 1000),
        r31_heal(14000, R31_TOTEM, R31_SHAM, R31_STREAM, 1000),
        r31_heal(15000, R31_PRIEST, R31_MAGE, R31_EMBRACE, 1000),
        r31_heal(15000, R31_PRIEST, R31_PRIEST, R31_EMBRACE, 3000),
        veto(20000, R31_ROGUE, ab.SWARM_SELF_SAVE_HOLD, "cloak of shadows"),
        veto(21000, R31_ROGUE, ab.SWARM_SELF_SAVE_HOLD, "evasion"),
        veto(22000, R31_MAGE, ab.SWARM_SELF_SAVE_HOLD, "mana shield"),
        {"t": 30000, "e": "end", "out": "wipe"},
    ]
    records += [r31_snap(when) for when in range(0, 30500, 500)]
    return records


class Refine31Lines(unittest.TestCase):
    """Phase 3 class saves and heal items split by what the caster was under, the self save hold's vetoes,
    bot heals onto non-carriers with a healer's own, and what held each non-tank up with its party heals."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_6_anub-arak_1790920000.ndjson", r31_pull())
        cls.samples = ab.Samples(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, show, *args) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            show(self.trace, *args)
        return out.getvalue()

    def test_saves_split_by_penetrating_cold_a_burrower_or_neither(self):
        saves, vetoes = ab.swarm_self_saves(self.trace, 10000, 30000, self.samples)
        self.assertEqual(saves, {"Cloak of Shadows": [1, 0, 1], "Ice Block": [0, 1, 0], "Mana Shield": [0, 0, 1],
                                 "Power Word: Shield": [0, 0, 1], "Master Healthstone": [0, 0, 1]})
        self.assertEqual(dict(vetoes), {R31_ROGUE: 2, R31_MAGE: 1})
        text = self.text(ab.show_swarm_self_saves, 10000, 30000, self.samples)
        self.assertIn("neither: Cloak of Shadows 1/0/1, Ice Block 0/1/0, Mana Shield 0/0/1, Master Healthstone 0/0/1,"
                      " Power Word: Shield 0/0/1\n", text)
        self.assertIn("  anubarak swarm self save hold vetoes per bot: Power 1, Shadow 2\n", text)

    def test_a_healers_heal_on_itself_is_a_breach_and_a_dps_bots_own_or_an_item_is_not(self):
        self.assertEqual(ab.swarm_heal_breaches(self.trace, 10000, 30000),
                         {(R31_TREE, R31_REJUVENATION): [2, 3500]})

    def test_bare_leaves_out_the_healers_the_items_and_a_healers_own_with_the_party_heals(self):
        rows = {row["guid"]: row for row in ab.swarm_sustain(self.trace, 10000, 30000, self.samples)}
        self.assertEqual(ab.sustain_bare(self.trace, rows[R31_TREE]), 0)
        self.assertEqual(ab.sustain_bare(self.trace, rows[R31_MAGE]), 2000)
        self.assertEqual(ab.sustain_bare(self.trace, rows[R31_PRIEST]), 3800)
        self.assertEqual(rows[R31_MAGE]["party"], {("HST", R31_SHAM), ("VE", R31_PRIEST)})
        self.assertEqual(rows[R31_PRIEST]["party"], set())
        self.assertEqual(rows[R31_SHAM]["party"], {("HST", R31_SHAM)})
        text = self.text(ab.show_swarm_sustain, 10000, 30000, self.samples)
        self.assertIn("    Power          ranged    20     0.00    0.00    0.00    0.05    0.05    0.00    0.00"
                      "    0.10    0.00    0.10 short  14.5 long  Malediction VE, Stormweaver HST\n", text)
        self.assertIn("    Tree           heal      20     0.00    0.10    0.00    0.00    0.00    0.00    0.25"
                      "    0.35    0.00    0.00 short     -       no party heal\n", text)


R32_TANK, R32_PRIEST, R32_HUNTER, R32_TREE, R32_SHAM, R32_LOCK, R32_FEL = 6601, 6602, 6603, 6604, 6605, 6606, 6607
R32_TOTEM = 0xF130009AB1006608
R32_RENEW, R32_STREAM = 48068, 52042


def r32_snap(when: int) -> dict:
    # the hunter at 3% of 28k under the swarm, Tree at 30% of 25k; Agony 5 yd off the room centre
    def unit(guid, dx, dy, hp):
        return [guid, X + dx, Y + dy, Z, 0.0, hp, 100.0, 0, 0, 0, 0, 0]

    units = [unit(R32_TANK, 5.0, 0.0, 80.0), unit(R32_PRIEST, 5.0, 5.0, 30.0), unit(R32_HUNTER, 5.0, 5.0, 3.0),
             unit(R32_TREE, 5.0, 5.0, 30.0), unit(R32_SHAM, 5.0, 5.0, 30.0), unit(R32_LOCK, 3.0, 4.0, 100.0),
             unit(R32_FEL, 20.0, 0.0, 100.0)]
    return {"t": when, "e": "snap", "u": units, "hz": []}


def r32_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1790930000000, "map": ab.MAP_TOC, "inst": 7, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": R32_TANK, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
             {"g": R32_PRIEST, "n": "Prayer", "r": "heal", "c": 5, "h": 0},
             {"g": R32_HUNTER, "n": "Trueshot", "r": "ranged", "c": 3, "h": 0},
             {"g": R32_TREE, "n": "Tree", "r": "heal", "c": 11, "h": 0},
             {"g": R32_SHAM, "n": "Elemena", "r": "heal", "c": 7, "h": 0},
             {"g": R32_LOCK, "n": "Agony", "r": "ranged", "c": ab.CLASS_WARLOCK, "h": 0},
             {"g": R32_FEL, "n": "Fel", "r": "ranged", "c": ab.CLASS_WARLOCK, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": 0, "e": "unit", "g": R32_HUNTER, "n": "Trueshot", "mhp": 28000},
        {"t": 0, "e": "unit", "g": R32_TREE, "n": "Tree", "mhp": 25000},
        {"t": 0, "e": "unit", "g": R32_TOTEM, "en": 3527, "n": "Healing Stream Totem IX", "own": R32_SHAM},
        {"e": "spell", "sp": ab.SPELL_CIRCLE_SUMMON, "n": "Demonic Circle: Summon"},
        {"e": "spell", "sp": R32_RENEW, "n": "Renew"},
        {"e": "spell", "sp": R32_STREAM, "n": "Healing Stream Totem"},
        note(0, R32_TANK, "anub.phase", 1),
        note(10000, R32_TANK, "anub.phase", 3),
        # Agony's circle right after the engage, Fel's once the window closed
        cast(200, R32_LOCK, ab.SPELL_CIRCLE_SUMMON, R32_LOCK),
        cast(12000, R32_FEL, ab.SPELL_CIRCLE_SUMMON, R32_FEL),
        # Renew onto the hunter at 840 health is the HoT; onto Tree at 7500 it only feeds the swarm
        cast(12000, R32_PRIEST, R32_RENEW, R32_HUNTER),
        r31_heal(15000, R32_PRIEST, R32_HUNTER, R32_RENEW, 600),
        r31_heal(18000, R32_PRIEST, R32_HUNTER, R32_RENEW, 600),
        cast(13000, R32_PRIEST, R32_RENEW, R32_TREE),
        r31_heal(16000, R32_PRIEST, R32_TREE, R32_RENEW, 700),
        {"t": 30000, "e": "end", "out": "wipe"},
    ]
    # Healing Stream onto Tree with a hole from 11 s to 19 s
    records += [r31_heal(when, R32_TOTEM, R32_TREE, R32_STREAM, 500)
                for when in (11000, 19000, 21000, 23000, 25000, 27000, 29000)]
    records += [r32_snap(when) for when in range(0, 30500, 500)]
    return records


class Refine32Lines(unittest.TestCase):
    """Warlock circles summoned right after the engage, HoTs onto a non-tank the swarm dragged low kept out
    of the breach line, and each non-tank's longest stretch with no party heal tick."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_7_anub-arak_1790930000.ndjson", r32_pull())
        cls.samples = ab.Samples(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, show, *args) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            show(self.trace, *args)
        return out.getvalue()

    def test_a_circle_summoned_in_the_first_10_s_with_its_yards_off_the_centre(self):
        self.assertEqual(ab.pull_circles(self.trace), {R32_LOCK: [(200, 5.0)], R32_FEL: []})
        self.assertIn("  Demonic Circle: Summon per warlock bot in the first 10 s, yards off the room centre: Agony"
                      " 0:00.200 5.0 yd; Fel none\n", self.text(ab.show_pull))

    def test_a_hot_onto_a_non_tank_under_the_line_leaves_the_breach_line(self):
        self.assertEqual(ab.swarm_hots(self.trace, 10000, 30000, self.samples),
                         [{"t": 12000, "caster": R32_PRIEST, "target": R32_HUNTER, "spell": R32_RENEW}])
        self.assertEqual(ab.swarm_heal_breaches(self.trace, 10000, 30000, self.samples),
                         {(R32_PRIEST, R32_RENEW): [1, 700]})
        self.assertIn("  HoTs cast by healer bots onto another non-tank with no Penetrating Cold under 1200 health,"
                      " their ticks left out above: Prayer Renew 48068 > Trueshot 1\n",
                      self.text(ab.show_swarm_race, 10000, 30000, self.samples))

    def test_the_longest_stretch_with_no_party_heal_tick(self):
        rows = {row["guid"]: row for row in ab.swarm_sustain(self.trace, 10000, 30000, self.samples)}
        self.assertEqual(rows[R32_TREE]["gap"], 7500)
        self.assertIsNone(rows[R32_HUNTER]["gap"])
        self.assertIn(" short   7.5 long  Elemena HST\n",
                      self.text(ab.show_swarm_sustain, 10000, 30000, self.samples))


R33_MT, R33_PALA, R33_DK, R33_SHAM = 6701, 6702, 6703, 6704
R33_BOSS = CREATURE | 6790
R33_B1, R33_B2, R33_B3, R33_B4 = (CREATURE | n for n in (6791, 6792, 6793, 6794))


def r33_snap(when: int) -> dict:
    # B1 on Bulwark until it dies at 30 s, B2 and B3 on Elemena, B4 on Bulwark from the emerge
    def unit(guid, hp, target=0):
        return [guid, X, Y, Z, 0.0, hp, 100.0, target, 0, 0, 0, 0]

    units = [unit(guid, 100.0) for guid in (R33_MT, R33_PALA, R33_DK, R33_SHAM)]
    units.append(unit(R33_BOSS, 80.0, R33_MT))
    units.append(unit(R33_B1, 60.0 if when < 30000 else 0.0, R33_PALA))
    units += [unit(guid, 90.0, R33_SHAM) for guid in (R33_B2, R33_B3)]
    if when >= 40000:
        units.append(unit(R33_B4, 95.0, R33_PALA))
    return {"t": when, "e": "snap", "u": units, "hz": []}


def r33_pull() -> list[dict]:
    defense = ab.SPELL_RIGHTEOUS_DEFENSE_TAUNT
    records = [
        {"e": "hdr", "v": 14, "ts": 1790940000000, "map": ab.MAP_TOC, "inst": 8, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": R33_MT, "n": "Ecoterrorist", "r": "tank", "c": 11, "h": 0},
             {"g": R33_PALA, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
             {"g": R33_DK, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
             {"g": R33_SHAM, "n": "Elemena", "r": "heal", "c": 7, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": 0, "e": "unit", "g": R33_BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"}
          for guid in (R33_B1, R33_B2, R33_B3, R33_B4)),
        {"e": "spell", "sp": ab.SPELL_FREEZING_SLASH, "n": "Freezing Slash"},
        note(0, R33_MT, "anub.phase", 1),
        note(20000, R33_MT, "anub.phase", 2),
        note(40000, R33_MT, "anub.phase", 1),
        note(0, R33_PALA, "anub.side0", R33_PALA),
        note(0, R33_DK, "anub.side1", R33_DK),
        note(0, R33_B1, "anub.bside", 0),
        note(0, R33_B2, "anub.bside", 1),
        note(0, R33_B3, "anub.bside", 1),
        note(40000, R33_B4, "anub.bside", 0),
        # the drag opens with a pre-step and a hold before its leg; Felesta's row in between is someone else's
        note(1000, R33_MT, "anub.pickup", "hold"),
        note(10000, R33_MT, "anub.pickup", "prestep"),
        note(12000, R33_DK, "anub.pickup", "hold"),
        note(13000, R33_MT, "anub.pickup", "subhold"),
        note(15000, R33_MT, "anub.pickup", "submerge"),
        # one Freezing Slash long before, one still stunning him 1.5 s before the drag set out
        cast(0, R33_BOSS, ab.SPELL_FREEZING_SLASH, R33_MT),
        cast(8500, R33_BOSS, ab.SPELL_FREEZING_SLASH, R33_MT),
        # Bulwark escorts with B1 on him: walk 22-26 s, post 27-33 s, B1 dead from 30 s
        note(22000, R33_PALA, "anub.escort", "walk"),
        note(26000, R33_PALA, "anub.escort", "none"),
        note(27000, R33_PALA, "anub.escort", "post"),
        note(33000, R33_PALA, "anub.escort", "none"),
        # Righteous Defense on Elemena takes his own B4 and side 1's B2 and B3; the second one only B4
        cast(45000, R33_PALA, ab.SPELL_RIGHTEOUS_DEFENSE, R33_SHAM),
        cast(45000, R33_PALA, defense, R33_B4),
        cast(45000, R33_PALA, defense, R33_B2),
        cast(45000, R33_PALA, defense, R33_B3),
        cast(50000, R33_PALA, ab.SPELL_RIGHTEOUS_DEFENSE, R33_SHAM),
        cast(50000, R33_PALA, defense, R33_B4),
        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    records += [r33_snap(when) for when in range(0, 60500, 500)]
    return records


class Refine33Lines(unittest.TestCase):
    """Freezing Slash on the drag tank from 3 s before the drag set out, a side tank on escort duty with a
    burrower on it, and a side tank's Righteous Defense that took the other side's burrowers."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_8_anub-arak_1790940000.ndjson", r33_pull())
        cls.samples = ab.Samples(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def text(self, show, *args) -> str:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            show(self.trace, *args)
        return out.getvalue()

    def test_freezing_slash_from_3_s_before_the_drag_set_out(self):
        drags = [row["drag"] for row in ab.threat_rows(self.trace, self.samples) if row["drag"]]
        self.assertEqual(drags, [{"t": 10000, "tank": R33_MT, "slashes": [8500]}])
        self.assertIn("          Freezing Slash on Ecoterrorist from 3 s before the drag set out at 0:10.000:"
                      " 0:08.500 -1.5 s\n", self.text(ab.show_threat))

    def test_escort_duty_counts_only_while_a_living_burrower_is_on_the_side_tank(self):
        self.assertEqual(ab.escort_under_burrowers(self.trace, self.samples),
                         [{"tank": R33_PALA, "phase": "P2", "ms": 7000, "first": 22000}])
        self.assertIn("  side tank on escort duty (anub.escort walk, hold, post, patch or standby) with a burrower on"
                      " it, per phase: Bulwark P2 7.0 s from 0:22.000\n",
                      self.text(ab.show_rework, [], [], self.samples))

    def test_righteous_defense_that_took_the_other_sides_burrowers(self):
        self.assertEqual(ab.defense_takes(self.trace, self.samples),
                         [{"t": 45000, "tank": R33_PALA, "member": R33_SHAM, "hp": 100.0, "own": [R33_B4],
                           "other": [R33_B2, R33_B3]}])
        self.assertIn("  side tank Righteous Defense that took the other side's burrowers, the member and its health"
                      " at the cast: Bulwark 0:45.000 on Elemena 100%: #6792, #6793 (own #6794)\n",
                      self.text(ab.show_side_tank_crowds, self.samples))


R34_KITER, R34_PALA, R34_DK = 6801, 6802, 6803
R34_B1, R34_B2 = (CREATURE | n for n in (6891, 6892))


def r34_kiter_x(when: int) -> float:
    # 7 yd/s to 2 s, then Nitro's 17.5
    return 7.0 * when / 1000 if when <= 2000 else 14.0 + 17.5 * (when - 2000) / 1000


def r34_snap(when: int) -> dict:
    def unit(guid, x, hp=100.0, target=0):
        return [guid, x, Y, Z, 0.0, hp, 100.0, target, 0, 0, 0, 0]

    return {"t": when, "e": "snap", "hz": [], "u": [
        unit(R34_KITER, r34_kiter_x(when)), unit(R34_PALA, 0.0), unit(R34_DK, 0.0, 0.0 if when >= 3000 else 100.0),
        unit(R34_B1, 760.0, 90.0, R34_PALA), unit(R34_B2, 760.0, 90.0, R34_DK)]}


def r34_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1790950000000, "map": ab.MAP_TOC, "inst": 9, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": R34_KITER, "n": "Stormweaver", "r": "ranged", "c": 7, "h": 0},
             {"g": R34_PALA, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
             {"g": R34_DK, "n": "Felesta", "r": "tank", "c": 6, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"}
          for guid in (R34_B1, R34_B2)),
        note(0, R34_KITER, "anub.phase", 1),
        note(0, R34_PALA, "anub.side0", R34_PALA),
        note(0, R34_DK, "anub.side1", R34_DK),
        note(0, R34_B1, "anub.bside", 0),
        note(0, R34_B2, "anub.bside", 1),
        # B1 handed over with both side tanks up; B2 only once Felesta is dead
        note(2000, R34_B1, "anub.bside", 1),
        note(3000, R34_B2, "anub.bside", 0),
        # the kite's move points: 9 yd ahead at a run, 20 yd ahead on Nitro; a gather row isn't a kite row
        {"t": 1000, "e": "move", "g": R34_KITER, "by": "anubarak kite spike to permafrost", "ok": 1,
         "x": r34_kiter_x(1000) + 9.0, "y": Y},
        {"t": 3000, "e": "move", "g": R34_KITER, "by": "anubarak kite spike to permafrost", "ok": 1,
         "x": r34_kiter_x(3000) + 20.0, "y": Y},
        {"t": 3500, "e": "move", "g": R34_KITER, "by": "anubarak gather", "ok": 1, "x": 0.0, "y": Y},
        {"t": 4000, "e": "end", "out": "wipe"},
    ]
    records += [r34_snap(when) for when in range(0, 4200, 200)]
    return records


class Refine34Lines(unittest.TestCase):
    """The kite move points' yards ahead of the kiter by its speed, and burrower side changes with both side
    tanks alive."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_9_anub-arak_1790950000.ndjson", r34_pull())
        cls.samples = ab.Samples(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_kite_move_points_split_by_the_kiters_speed(self):
        ahead = ab.kite_move_ahead(self.trace, self.samples)
        self.assertEqual([round(yards, 1) for yards in ahead["run"]], [9.0])
        self.assertEqual([round(yards, 1) for yards in ahead["fast"]], [20.0])
        self.assertEqual(ab.ahead_text(ahead["fast"]), "n 1, median 20.0, p90 20.0 yd")
        self.assertEqual(ab.ahead_text([]), "none")

    def test_side_changes_count_only_with_both_side_tanks_alive(self):
        rows = {row["guid"]: row for row in ab.pickup_rows(self.trace, self.samples)}
        self.assertEqual(rows[R34_B1]["swaps"], [(2000, 1)])
        self.assertEqual(rows[R34_B2]["swaps"], [(3000, 0)])
        swaps = ab.live_side_swaps(self.trace, self.samples, list(rows.values()))
        self.assertEqual([(row["guid"], when, owner) for row, when, owner in swaps], [(R34_B1, 2000, 1)])


R35_PRAYER, R35_TREE, R35_HOLY = 6901, 6902, 6903


def r35_hp(guid: int, when: int) -> float:
    if guid == R35_PRAYER:
        # under 5% from 2 s, dead to a swarm tick at 4 s
        return 0.0 if when >= 4000 else (3.0 if when >= 2000 else 40.0)
    if guid == R35_TREE:
        # dead to a Penetrating Cold tick at 3 s, never under 5% while alive
        return 0.0 if when >= 3000 else 20.0
    return 4.0 if 1000 <= when < 1500 else 60.0


def r35_pull() -> list[dict]:
    def unit(guid, when):
        return [guid, X, Y, Z, 0.0, r35_hp(guid, when), 100.0, 0, 0, 0, 0, 0]

    records = [
        {"e": "hdr", "v": 14, "ts": 1790960000000, "map": ab.MAP_TOC, "inst": 9, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": R35_PRAYER, "n": "Prayer", "r": "heal", "c": 5, "h": 0},
             {"g": R35_TREE, "n": "Tree", "r": "heal", "c": 11, "h": 0},
             {"g": R35_HOLY, "n": "Holylight", "r": "heal", "c": 2, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        note(0, R35_PRAYER, "anub.phase", 3),
        dmg(3000, BOSS, R35_TREE, 68510, 4500),
        {"t": 3000, "e": "death", "g": R35_TREE, "killer": BOSS, "x": X, "y": Y, "z": Z},
        dmg(3900, BOSS, R35_PRAYER, ab.SPELL_LEECHING_SWARM_DMG, 190),
        {"t": 4000, "e": "death", "g": R35_PRAYER, "killer": BOSS, "x": X, "y": Y, "z": Z},
        {"t": 5000, "e": "end", "out": "wipe"},
    ]
    records += [{"t": when, "e": "snap", "hz": [], "u": [unit(guid, when) for guid in (R35_PRAYER, R35_TREE, R35_HOLY)]}
                for when in range(0, 5200, 250)]
    return records


class Refine35Lines(unittest.TestCase):
    """Healers' phase 3 seconds under 5% health, their lowest, and a death to the swarm's minimum tick."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_9_anub-arak_1790960000.ndjson", r35_pull())
        cls.samples = ab.Samples(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_time_under_the_floor_and_a_swarm_tick_death(self):
        rows = {row["guid"]: row for row in ab.healer_floor(self.trace, 0, 5000, self.samples)}
        self.assertEqual((rows[R35_PRAYER]["under"], rows[R35_PRAYER]["lowest"]), (2000, 3.0))
        self.assertEqual(rows[R35_PRAYER]["floor_deaths"], [4000])
        self.assertEqual((rows[R35_TREE]["under"], rows[R35_TREE]["floor_deaths"]), (0, []))
        self.assertEqual((rows[R35_HOLY]["under"], rows[R35_HOLY]["lowest"]), (500, 4.0))
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_healer_floor(self.trace, 0, 5000, self.samples)
        self.assertIn("Holylight 0.5 s 4.00%, Prayer 2.0 s 3.00%, died to the swarm floor 0:04.000,"
                      " Tree 0.0 s 20.00%\n", out.getvalue())


R36_MAIN, R36_SIDE, R36_HEAL = 7001, 7002, 7003
R36_B = CREATURE | 1700


def r36_pull() -> list[dict]:
    def frame(when):
        units = [[R36_MAIN, X, Y, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0],
                 [R36_SIDE, X + 10.0, Y, Z, 0.0, 0.0 if when >= 9000 else 95.0, 100.0, 0, 0, 0, 0, 0],
                 [R36_HEAL, X - 10.0, Y, Z, 0.0, 100.0, 80.0, 0, 0, 0, 0, 0]]
        if when >= 1000:
            units.append([R36_B, X + (12.0 if when < 6000 else 20.0), Y, Z, 0.0, 100.0, 0.0, R36_SIDE, 0, 0, 0, 0])
        return {"t": when, "e": "snap", "hz": [], "u": units}

    def hit(when, amount, hp, absorbed=0):
        return {**dmg(when, R36_B, R36_SIDE, 0, amount), "hp": hp, "ab": absorbed}

    records = [
        {"e": "hdr", "v": 14, "ts": 1791000000000, "map": ab.MAP_TOC, "inst": 9, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": R36_MAIN, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": R36_SIDE, "n": "Felesta", "r": "tank", "c": 6, "h": 0},
                    {"g": R36_HEAL, "n": "Mercy", "r": "heal", "c": 5, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 1000, "e": "unit", "g": R36_B, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        note(0, R36_MAIN, "anub.phase", 1),
        note(0, R36_MAIN, "anub.side0", R36_SIDE),
        hit(2000, 10000, 90.0), hit(2200, 10000, 80.0), hit(2400, 10000, 70.0), hit(2600, 10000, 60.0),
        cast(2650, R36_SIDE, 48792, R36_SIDE),
        {"t": 2700, "e": "heal", "s": R36_HEAL, "d": R36_SIDE, "sp": 48071, "a": 20000, "oh": 0, "hp": 70.0},
        cast(2750, R36_SIDE, 55233, R36_SIDE),
        hit(3000, 0, 40.0, 5000),
        note(8000, R36_SIDE, "anub.defensive", "kept"),
        hit(8500, 10000, 30.0),
        {"t": 9000, "e": "death", "g": R36_SIDE, "killer": R36_B, "x": X + 10.0, "y": Y, "z": Z},
        hit(9500, 10000, 50.0),
        {"t": 10000, "e": "end", "out": "wipe"},
    ]
    records += [frame(when) for when in range(0, 10500, 500)]
    return records


class SideTankHealthLines(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_9_anub-arak_1791000000.ndjson", r36_pull())
        cls.samples = ab.Samples(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_max_health_reads_back_to_back_melee_hits(self):
        _, rows, top = ab.health_tracks(self.trace)[R36_SIDE]
        self.assertAlmostEqual(top, 100000.0)
        self.assertIsNone(ab.max_health(rows[:3]))

    def test_a_defensive_reads_health_after_the_last_hit_or_heal(self):
        rows = {row["spell"]: row["hp"] for row in ab.defensive_rows(self.trace, self.samples)}
        self.assertAlmostEqual(rows["icebound fortitude"], 50.0)
        self.assertAlmostEqual(rows["vampiric blood"], 70.0)

    def test_a_tank_death_reads_its_last_verdict_and_top_health(self):
        row = ab.tank_deaths(self.trace, self.samples)[0]
        self.assertEqual((row["verdict"], row["top"]), ("kept", 95.0))

    def test_burrower_contact_and_landed_hits_stop_at_the_death(self):
        self.assertEqual(ab.side_tank_contact(self.trace, self.samples), {R36_SIDE: {"ms": 5000, "landed": 6}})

    def test_healer_mana_at_the_engage(self):
        self.assertEqual(ab.engage_mana(self.trace, self.samples), [(R36_HEAL, 80.0)])

    def test_the_lines_print(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_tanks(self.trace)
            ab.show_pull(self.trace)
        text = out.getvalue()
        self.assertIn("Felesta 5.0 s, 6, 1.20/s\n", text)
        self.assertIn("anub.defensive kept; top 95%", text)
        self.assertIn("healer mana at the engage: Mercy 80%\n", text)


R37_SIDE = 7101
R37_B1, R37_B2 = CREATURE | 1801, CREATURE | 1802


def r37_pull() -> list[dict]:
    def frame(when):
        walking = 4000 <= when < 6000
        tank = [R37_SIDE, X, Y, Z, 0.0, 100.0, 100.0, R37_B1, 1 if walking else 0, 0, 0, 0]
        units = [tank, [R37_B1, X + 3.0, Y, Z, math.pi, 100.0, 0.0, R37_SIDE, 0, 0, 0, 0]]
        if when < 8000:
            units.append([R37_B2, X - 3.0 if walking else X + 2.0, Y + 1.0, Z, 0.0, 100.0, 0.0, R37_SIDE, 0, 0, 0, 0])
        return {"t": when, "e": "snap", "hz": [], "u": units}

    records = [
        {"e": "hdr", "v": 14, "ts": 1791100000000, "map": ab.MAP_TOC, "inst": 9, "diff": 3, "boss": "anub-arak",
         "roster": [{"g": R37_SIDE, "n": "Felesta", "r": "tank", "c": 6, "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 0, "e": "unit", "g": R37_B1, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        {"t": 0, "e": "unit", "g": R37_B2, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        note(0, R37_SIDE, "anub.phase", 1),
        note(0, R37_SIDE, "anub.side0", R37_SIDE),
        note(3900, R37_SIDE, "anub.backup", "back"),
        dmg(500, R37_B1, R37_SIDE, 0, 5000), dmg(1720, R37_B1, R37_SIDE, 0, 5000),
        dmg(3700, R37_B1, R37_SIDE, 0, 5000),
        dmg(4500, R37_B2, R37_SIDE, 0, 5000), dmg(5500, R37_B2, R37_SIDE, 0, 5000),
        dmg(9800, R37_B1, R37_SIDE, 0, 5000),
        note(10000, R37_SIDE, "anub.phase", 3),
        {"t": 10000, "e": "end", "out": "wipe"},
    ]
    records += [frame(when) for when in range(0, 10500, 500)]
    return records


class SideTankFacingLines(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_9_anub-arak_1791100000.ndjson", r37_pull())
        cls.samples = ab.Samples(cls.trace)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_contact_and_hits_split_by_walking_and_behind(self):
        row = ab.side_tank_facing(self.trace, self.samples)[R37_SIDE]
        self.assertEqual(row[(False, False)], {"ms": 13500, "landed": 4})
        self.assertEqual(row[(True, False)], {"ms": 2000, "landed": 0})
        self.assertEqual(row[(True, True)], {"ms": 2000, "landed": 2})
        self.assertEqual(row[(False, True)], {"ms": 0, "landed": 0})

    def test_landed_a_swing_counts_standing_front_gaps_only(self):
        row = ab.side_tank_facing(self.trace, self.samples)[R37_SIDE]
        self.assertEqual((row["gaps"], row["swings"]), (2, 3))

    def test_the_lines_print(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_tanks(self.trace)
        text = out.getvalue()
        self.assertIn("Felesta      standing front 13.5 s 4 0.30/s, behind 0.0 s 0; walking front 2.0 s 0,"
                      " behind 2.0 s 2 1.00/s; a swing 0.67 (2 gaps)\n", text)
        self.assertIn("anub.backup: Felesta back 1\n", text)


if __name__ == "__main__":
    unittest.main()

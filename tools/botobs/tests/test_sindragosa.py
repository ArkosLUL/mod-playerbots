"""Tests for the Sindragosa reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H boss pull and a 25N drakes pull pin each section. The banner reads a fixed declared
set, so the source can gain or lose keys under it, and the shared v14 fixture shows every section
reads empty on a trace that is not Sindragosa rather than raising.
"""
from __future__ import annotations

import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from bosses import sindragosa as sg  # noqa: E402
from raidobs import probes  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
SINDRAGOSA = CREATURE | 950
TOMB1 = CREATURE | 951
TOMB2 = CREATURE | 952
TOMB3 = CREATURE | 953
RIMEFANG = CREATURE | 960
SPINESTALKER = CREATURE | 961
BULWARK = 7701  # tank
AEGIS = 7702    # tank
MEND = 7703     # healer
SHIV = 7704     # melee
ARCANE = 7705   # ranged
ARKOS = 7706    # human melee

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (AEGIS, "Aegis", "tank", 0), (MEND, "Mend", "heal", 0),
          (SHIV, "Shiv", "melee", 0), (ARCANE, "Arcane", "ranged", 0), (ARKOS, "Arkos", "melee", 1))

BOSS_KEYS = ("sg.phase", "sg.tank", "sg.breath", "sg.swap", "sg.stand", "sg.cold", "sg.unchained", "sg.chilled",
             "sg.brake", "sg.beacon", "sg.splash", "sg.hide", "sg.bombs", "sg.tombhold", "sg.target",
             "sg.defensive", "sg.humancall")
DRAKE_KEYS = ("sg.drake.target", "sg.drake.tank", "sg.drake.stand", "sg.drake.air", "sg.drake.brake",
              "sg.drake.dodge", "sg.drake.defensive", "sg.drake.humancall")
NEITHER_KEYS = ("sg.gauntlet.target", "sg.gauntlet.taunt", "sg.gauntlet.dodge", "sg.group", "sg.tomb",
                "sg.freedfallback", "sg.flaredblue")
DECLARED = {key: (probes.HOLDER, "ICCHelpers_Sindragosa.cpp:1") for key in BOSS_KEYS + DRAKE_KEYS + NEITHER_KEYS}


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def counter(guid: int) -> int:
    """The guid counter a probe value names a unit by."""
    return guid & 0xFFFFFFFF


def aura(when: int, guid: int, spell: int, stacks: int = 1, removed: int = 0) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": SINDRAGOSA, "sp": spell, "r": removed, "st": stacks}


def dmg(when: int, guid: int, spell: int, source: int = SINDRAGOSA) -> dict:
    return {"t": when, "e": "dmg", "s": source, "d": guid, "sp": spell, "a": 10000}


def cast(when: int, source: int, spell: int, cast_ms: int = 0, target: int | None = None) -> dict:
    rec = {"t": when, "e": "cast", "s": source, "sp": spell, "tgt": target or source}
    if cast_ms:
        rec["ct"] = cast_ms
    return rec


def cheat(when: int, guid: int, key: str, uses: int, target: int | None = None) -> dict:
    rec = {"t": when, "e": "cheat", "g": guid, "k": key, "n": uses}
    if target:
        rec["tgt"] = target
    return rec


def opening(diff: int, src: str) -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": diff, "boss": "sindragosa",
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 1},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": human}
                    for guid, name, role, human in ROSTER]},
        {"t": 0, "e": "pull", "boss": "sindragosa", "src": src},
        {"t": 1, "e": "spell", "sp": 871, "n": "Shield Wall"},
    ]


def boss_pull() -> list[dict]:
    return [
        *opening(3, "bossstate"),
        {"t": 1, "e": "unit", "g": SINDRAGOSA, "en": 36853, "n": "Sindragosa", "lvl": 83, "b": 1},
        *({"t": 1, "e": "unit", "g": tomb, "en": 36980, "n": "Ice Tomb"} for tomb in (TOMB1, TOMB2, TOMB3)),
        note(100, BULWARK, "sg.phase", "ground"),
        note(110, MEND, "sg.phase", "ground"),
        note(50000, BULWARK, "sg.phase", "air"),
        note(50100, MEND, "sg.phase", "air"),
        note(92000, BULWARK, "sg.phase", "ground"),
        note(200000, BULWARK, "sg.phase", "last"),
        # Tanks: Aegis walks in 12 s after the third breath, taunts at 16 s, and the roles flip.
        note(50, BULWARK, "sg.tank", "1"),
        note(50, AEGIS, "sg.tank", "2"),
        note(216000, AEGIS, "sg.tank", "1"),
        note(216000, BULWARK, "sg.tank", "2"),
        note(10, BULWARK, "sg.breath", "0"),
        cast(8500, SINDRAGOSA, 71058, 1500, BULWARK),
        note(10000, BULWARK, "sg.breath", "1"),
        cast(30500, SINDRAGOSA, 71058, 1500, BULWARK),
        note(32000, SHIV, "sg.breath", "2"),
        cast(198500, SINDRAGOSA, 73064, 1500, BULWARK),
        note(200000, BULWARK, "sg.breath", "3"),
        note(212000, AEGIS, "sg.swap", "walk"),
        note(216000, AEGIS, "sg.swap", "taunt"),
        note(217000, AEGIS, "sg.swap", "none"),
        note(60, AEGIS, "sg.brake", "1"),
        note(15000, AEGIS, "sg.brake", "0"),
        note(40000, SHIV, "sg.brake", "1"),
        note(42000, SHIV, "sg.brake", "0"),
        note(45000, SHIV, "sg.brake", "1"),
        note(46500, SHIV, "sg.brake", "0"),
        note(198000, BULWARK, "sg.defensive", "shield wall"),
        note(198000, BULWARK, "sg.humancall", "Shield Wall"),
        cast(199000, BULWARK, 871),
        note(201000, BULWARK, "sg.defensive", "idle"),
        # Stands: Arcane leaves its stand for the air phase, the tanks trade theirs at the swap.
        note(120, BULWARK, "sg.stand", "T"),
        note(120, AEGIS, "sg.stand", "M"),
        note(120, SHIV, "sg.stand", "M"),
        note(120, ARCANE, "sg.stand", "S"),
        note(120, MEND, "sg.stand", "S"),
        note(50000, ARCANE, "sg.stand", "none"),
        note(92500, ARCANE, "sg.stand", "S"),
        note(216500, BULWARK, "sg.stand", "M"),
        note(216500, AEGIS, "sg.stand", "T"),
        # One Blistering Cold: three bots run, the human stays and is hit.
        cast(100000, SINDRAGOSA, 71049, 5000),
        note(100200, BULWARK, "sg.cold", "tout"),
        note(100200, SHIV, "sg.cold", "escape 2"),
        note(100200, MEND, "sg.cold", "escape 2"),
        note(104000, SHIV, "sg.cold", "none"),
        dmg(105000, ARKOS, 71049),
        note(105500, BULWARK, "sg.cold", "none"),
        note(105500, MEND, "sg.cold", "none"),
        # Unchained Magic on Mend and Arcane; Mend's Backlash splashes Shiv, who carries nothing.
        aura(12000, MEND, 69762),
        aura(12000, ARCANE, 69762),
        note(12500, ARCANE, "sg.unchained", "spot 4"),
        aura(13000, MEND, 69766, 1),
        aura(14000, MEND, 69766, 3),
        aura(14000, ARCANE, 69766, 2),
        note(14100, MEND, "sg.unchained", "spot 1 hold"),
        note(14600, MEND, "sg.unchained", "spot 1"),
        aura(15000, MEND, 69766, 4),
        aura(20000, MEND, 69766, removed=1),
        dmg(20000, MEND, 71046),
        dmg(20000, SHIV, 71046),
        note(21000, MEND, "sg.unchained", "none"),
        aura(42000, MEND, 69762, removed=1),
        aura(42000, ARCANE, 69762, removed=1),
        note(47500, ARCANE, "sg.unchained", "none"),
        # Chilled to the Bone: Shiv stops at 6, the human never does.
        aura(30000, SHIV, 70106, 3),
        aura(30000, ARKOS, 70106, 8),
        aura(33000, SHIV, 70106, 6),
        note(33100, SHIV, "sg.chilled", "stop"),
        aura(35000, SHIV, 70106, 7),
        note(43000, SHIV, "sg.chilled", "none"),
        # The air phase: two beacons, a splash dodge, two bombs, the tombs held from the first.
        note(50000, BULWARK, "sg.bombs", "0"),
        note(50000, BULWARK, "sg.tombhold", "0"),
        note(56000, SHIV, "sg.beacon", "1"),
        note(56000, ARCANE, "sg.beacon", "2"),
        note(56500, MEND, "sg.splash", "dodge"),
        note(58000, MEND, "sg.splash", "none"),
        # A sweep that found no clear spot is no dodge.
        note(56600, BULWARK, "sg.splash", "blocked"),
        note(57600, BULWARK, "sg.splash", "none"),
        note(63000, MEND, "sg.bombs", "1"),
        note(63000, BULWARK, "sg.tombhold", "1"),
        note(63500, MEND, "sg.hide", f"bomb {counter(TOMB1)}"),
        note(63600, BULWARK, "sg.hide", f"bomb {counter(TOMB1)}"),
        note(64000, SHIV, "sg.beacon", "0"),
        note(64000, ARCANE, "sg.beacon", "0"),
        note(68000, MEND, "sg.hide", "none"),
        note(68000, BULWARK, "sg.hide", "none"),
        note(69000, MEND, "sg.bombs", "2"),
        note(69500, MEND, "sg.hide", f"bomb {counter(TOMB2)}"),
        note(74000, MEND, "sg.hide", "none"),
        note(88000, BULWARK, "sg.tombhold", "0"),
        # The last phase: beacons on A then B, Shiv hides from Mystic Buffet once.
        aura(206000, SHIV, 72530, 3),
        aura(206000, ARCANE, 72530, 2),
        note(206500, SHIV, "sg.hide", f"buffet {counter(TOMB3)}"),
        note(207000, MEND, "sg.beacon", "7"),
        aura(212000, SHIV, 72530, 4),
        note(214000, MEND, "sg.beacon", "0"),
        note(214500, SHIV, "sg.hide", "none"),
        note(226000, ARCANE, "sg.beacon", "8"),
        note(233000, ARCANE, "sg.beacon", "0"),
        # Targets: Shiv is held off while chilled, Arcane breaks two tombs.
        note(60, BULWARK, "sg.target", "boss"),
        note(60, SHIV, "sg.target", "boss"),
        note(33100, SHIV, "sg.target", "chilled"),
        note(43000, SHIV, "sg.target", "boss"),
        note(64000, ARCANE, "sg.target", f"tomb {counter(TOMB1)}"),
        note(80000, ARCANE, "sg.target", f"tomb {counter(TOMB2)}"),
        cheat(61000, BULWARK, "sg.heroicbuff", 4, ARKOS),
        cheat(120000, BULWARK, "lk.hoj", 1, TOMB1),
        {"t": 300000, "e": "end", "out": "kill"},
    ]


def drakes_pull() -> list[dict]:
    return [
        *opening(1, "mark"),
        {"t": 1, "e": "unit", "g": RIMEFANG, "en": 37533, "n": "Rimefang", "lvl": 83},
        {"t": 1, "e": "unit", "g": SPINESTALKER, "en": 37534, "n": "Spinestalker", "lvl": 83},
        note(100, BULWARK, "sg.drake.tank", "1"),
        note(100, AEGIS, "sg.drake.tank", "2"),
        note(100, BULWARK, "sg.drake.target", "owned rimefang"),
        note(100, SHIV, "sg.drake.target", "rimefang"),
        note(100, BULWARK, "sg.drake.air", "0"),
        note(150, SHIV, "sg.drake.stand", "melee rimefang"),
        note(150, MEND, "sg.drake.stand", "ranged rimefang"),
        note(20000, BULWARK, "sg.drake.defensive", "shield wall"),
        cast(20500, BULWARK, 871),
        note(24000, BULWARK, "sg.drake.defensive", "idle"),
        note(25000, BULWARK, "sg.drake.humancall", "Shield Wall"),
        # Rimefang takes off: Shiv is braked and off its stand, then dodges a blast and a puddle.
        note(30000, BULWARK, "sg.drake.air", "1"),
        note(30500, SHIV, "sg.drake.stand", "none"),
        note(31000, SHIV, "sg.drake.brake", "1"),
        note(31000, SHIV, "sg.drake.target", "brake"),
        note(32000, SHIV, "sg.drake.dodge", "blast"),
        note(33500, SHIV, "sg.drake.dodge", "none"),
        dmg(34000, SHIV, 71377, RIMEFANG),
        note(36000, SHIV, "sg.drake.brake", "0"),
        note(36000, SHIV, "sg.drake.target", "rimefang"),
        note(38000, SHIV, "sg.drake.dodge", "puddle"),
        note(39000, SHIV, "sg.drake.dodge", "none"),
        note(40000, MEND, "sg.drake.dodge", "puddle"),
        note(41000, MEND, "sg.drake.dodge", "none"),
        dmg(42000, ARCANE, 71381, RIMEFANG),
        dmg(43000, ARCANE, 71381, RIMEFANG),
        note(45000, AEGIS, "sg.drake.air", "0"),
        note(45500, SHIV, "sg.drake.stand", "melee rimefang"),
        note(60000, SHIV, "sg.drake.stand", "melee spinestalker"),
        note(70000, SHIV, "sg.drake.target", "spinestalker"),
        {"t": 90000, "e": "end", "out": "kill"},
    ]


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / f"631_5_{records[0]['boss']}_1789800000.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


def banner(records: list[dict], declared: dict = DECLARED) -> str:
    with mock.patch.object(probes, "declared_keys", return_value=declared):
        return printed(sg.show_banner, load(records))


class Phases(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sg.show_phases, load(boss_pull())), (
            "PHASES\n"
            "  sg.phase, each step from the first bot to note it\n"
            "\n"
            "         from       held  phase\n"
            "     0:00.100   0:49.900  ground\n"
            "     0:50.000   0:42.000  air\n"
            "     1:32.000   1:48.000  ground\n"
            "     3:20.000   1:40.000  last\n"))


class Tanks(unittest.TestCase):
    def test_breaths_count_each_rise(self):
        self.assertEqual(sg.breaths(load(boss_pull())), 3)

    def test_section(self):
        self.assertEqual(printed(sg.show_tanks, load(boss_pull())), (
            "TANKS\n"
            "  sg.tank per tank: its role, from when\n"
            "    Aegis  free 0:00.050, holder 3:36.000\n"
            "    Bulwark  holder 0:00.050, free 3:36.000\n"
            "  sg.breath counted 3 breath(s), her Frost Breath casts 3\n"
            "  sg.swap per bot: each step, from when\n"
            "    Aegis  walk 3:32.000 -> taunt 3:36.000\n"
            "  sg.brake per bot: times braked, time held\n"
            "    Aegis  1, 0:14.940\n"
            "    Shiv  2, 0:03.500\n"
            "  sg.defensive, each window: the tank, how long, what it offered and cast\n"
            "    3:18.000  Bulwark     0:03.000  offered shield wall; cast Shield Wall +1.0s\n"
            "  calls to a human tank  Shield Wall 3:18.000\n"))


class Stands(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sg.show_stands, load(boss_pull())), (
            "STANDS\n"
            "  per bot: the stands sg.stand held in order (moves between two)\n"
            "\n"
            "    Aegis  M/T (moved 1)\n"
            "    Arcane  S (moved 0)\n"
            "    Bulwark  T/M (moved 1)\n"
            "    Mend  S (moved 0)\n"
            "    Shiv  M (moved 0)\n"))


class Cold(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sg.show_cold, load(boss_pull())), (
            "COLD\n"
            "  1 Blistering Cold cast(s): the runs sg.cold noted, the players it hit\n"
            "\n"
            "     1:40.000  Bulwark tout, Mend escape 2, Shiv escape 2  hit Arkos human 1\n"))


class Unchained(unittest.TestCase):
    def test_rows_read_rules_and_peak(self):
        rows = sg.unchained_rows(load(boss_pull()))
        self.assertEqual([(row["carrier"], row["rules"], row["peak"]) for row in rows],
                         [(MEND, ["spot 1 hold", "spot 1"], 4), (ARCANE, ["spot 4"], 2)])

    def test_section(self):
        self.assertEqual(printed(sg.show_unchained, load(boss_pull())), (
            "UNCHAINED\n"
            "  2 Unchained Magic window(s): the carrier, the rules sg.unchained noted, peak Instability\n"
            "\n"
            "     0:12.000   0:30.000  Mend  spot 1 hold -> spot 1  peak 4\n"
            "     0:12.000   0:30.000  Arcane  spot 4  peak 2\n"
            "  Backlash hits on non-carriers: Shiv 1\n"))


class Chilled(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sg.show_chilled, load(boss_pull())), (
            "CHILLED\n"
            "  per player: sg.chilled stops, peak Chilled to the Bone stacks\n"
            "\n"
            "    Arkos human  stops 0  peak 8\n"
            "    Shiv  stops 1  peak 7\n"))


class Air(unittest.TestCase):
    def test_hides_count_until_the_next_bomb(self):
        rows = sg.air_rows(load(boss_pull()))
        self.assertEqual([row["bombs"] for row in rows], [[(1, 63000, 2, 1), (2, 69000, 1, 1)]])

    def test_section(self):
        self.assertEqual(printed(sg.show_air, load(boss_pull())), (
            "AIR\n"
            "  1 air phase(s): beacon spots, beacon dodges, bombs and the hides behind tombs, tomb hold\n"
            "\n"
            "  0:50.000  held 0:42.000\n"
            "    beacons  Shiv 1, Arcane 2\n"
            "    beacon dodges  Mend 1\n"
            "    bomb 1  1:03.000  hides 2, tombs 1\n"
            "    bomb 2  1:09.000  hides 1, tombs 1\n"
            "    tombs held  1:03.000 for 0:25.000\n"))


class Last(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sg.show_last, load(boss_pull())), (
            "LAST\n"
            "  last phase from 3:20.000: beacon spots, Mystic Buffet hides, peak Buffet stacks\n"
            "\n"
            "    beacons  Mend A 3:27.000, Arcane B 3:46.000\n"
            "    Arcane  hides 0, 0:00.000 hidden  peak 2\n"
            "    Shiv  hides 1, 0:08.000 hidden  peak 4\n"))


class Targets(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sg.show_targets, load(boss_pull())), (
            "TARGETS\n"
            "  per bot: spans of each resolver rule\n"
            "\n"
            "    Arcane  tomb 2\n"
            "    Bulwark  boss 1\n"
            "    Shiv  boss 2, chilled 1\n"))


class Drakes(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(sg.show_drakes, load(drakes_pull())), (
            "DRAKES\n"
            "  sg.drake.tank per tank: the drake it holds, from when\n"
            "    Aegis  Spinestalker 0:00.100\n"
            "    Bulwark  Rimefang 0:00.100\n"
            "  Rimefang up 1 time(s): 0:30.000 for 0:15.000\n"
            "  sg.drake.brake per bot: times braked, time held\n"
            "    Shiv  1, 0:05.000\n"
            "  per player: sg.drake.dodge rules, then Icy Blast and puddle hits\n"
            "    Arcane  dodges -  hit puddle 2\n"
            "    Mend  dodges puddle 1  hit -\n"
            "    Shiv  dodges blast 1, puddle 1  hit blast 1\n"
            "  sg.drake.stand per bot: the stands held in order (moves between two)\n"
            "    Mend  ranged rimefang (moved 0)\n"
            "    Shiv  melee rimefang/melee spinestalker (moved 1)\n"
            "  sg.drake.target per bot: spans of each resolver rule\n"
            "    Bulwark  owned 1\n"
            "    Shiv  brake 1, rimefang 2, spinestalker 1\n"
            "  sg.drake.defensive, each window: the tank, how long, what it offered and cast\n"
            "    0:20.000  Bulwark     0:04.000  offered shield wall; cast Shield Wall +0.5s\n"
            "  calls to a human tank  Shield Wall 0:25.000\n"))

    def test_a_boss_pull_has_no_drakes(self):
        self.assertEqual(printed(sg.show_drakes, load(boss_pull())),
                         "DRAKES\n  no sg.drake.* key written: not a drakes pull\n")


class Cheats(unittest.TestCase):
    def test_own_keys(self):
        self.assertEqual(printed(sg.show_cheats, load(boss_pull())), (
            "CHEATS\n"
            "  sg.heroicbuff  4 use(s) from 1:01.000\n"
            "    by  Bulwark 4\n"
            "    on  Arkos human\n"
            "  1 use(s) under other encounters' keys: icecrown_citadel.py --cheats\n"))


class Banner(unittest.TestCase):
    def test_a_boss_pull_spares_the_drakes_the_gauntlet_and_the_unused_keys(self):
        self.assertIn("boss pull, all its sg.* probes present\n", banner(boss_pull()))

    def test_a_missing_boss_probe_is_named(self):
        records = [rec for rec in boss_pull() if rec.get("k") != "sg.hide"]
        self.assertIn("boss pull, probes absent from this trace: sg.hide\n", banner(records))

    def test_a_drakes_pull_spares_the_boss_keys(self):
        self.assertIn("drakes pull, all its sg.* probes present\n", banner(drakes_pull()))

    def test_a_missing_drake_probe_is_named(self):
        records = [rec for rec in drakes_pull() if rec.get("k") != "sg.drake.dodge"]
        self.assertIn("drakes pull, probes absent from this trace: sg.drake.dodge\n", banner(records))

    def test_a_raid_wide_key_belongs_to_either_kind(self):
        declared = {**DECLARED, "icc.state": (probes.LATCH, "ICCScripts.h:1")}
        self.assertIn("drakes pull, probes absent from this trace: icc.state\n", banner(drakes_pull(), declared))

    def test_a_pull_of_neither_kind_calls_nothing_absent(self):
        records = opening(3, "bossstate") + [{"t": 90000, "e": "end", "out": "wipe"}]
        self.assertIn("neither sg.phase nor any sg.drake.* key written", banner(records))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            sg.show_banner(trace)
            for _, _, section in sg.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Sindragosa pull"), len(sg.SECTIONS))


if __name__ == "__main__":
    unittest.main()

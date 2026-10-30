"""Tests for the Sindragosa gauntlet reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 10N pull pins each section. The banner reads a fixed declared set, so the source can gain
or lose keys under it, and the shared v14 fixture shows every section reads empty on a trace that is
not the gauntlet rather than raising.
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

from bosses import sindragosa_gauntlet as gauntlet  # noqa: E402
from raidobs import probes  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
CHAMPION = CREATURE | 980
WEBWEAVER = CREATURE | 981
WARDEN = CREATURE | 982
SORCERESS = CREATURE | 983
CHAMPION_AGAIN = CREATURE | 984
BROODLING = CREATURE | 985
BLADE = CREATURE | 986
BULWARK = 7401  # tank
SHIV = 7402     # melee
MEND = 7403     # healer

ROSTER = ((BULWARK, "Bulwark", "tank"), (SHIV, "Shiv", "melee"), (MEND, "Mend", "heal"))

UNITS = ((CHAMPION, 37501, "Nerub'ar Champion"), (WEBWEAVER, 37502, "Nerub'ar Webweaver"),
         (WARDEN, 37228, "Frostwarden Warrior"), (SORCERESS, 37229, "Frostwarden Sorceress"),
         (CHAMPION_AGAIN, 37501, "Nerub'ar Champion"), (BROODLING, 37232, "Nerub'ar Broodling"),
         (BLADE, 38199, "Frostblade"))

# Each add's sampled span: the first wave's spiders, the Frostwardens, the third wave's spiders.
SAMPLES = {CHAMPION: (0, 20000), WEBWEAVER: (1000, 22000), WARDEN: (30000, 50000), SORCERESS: (31000, 48000),
           CHAMPION_AGAIN: (60000, 80000), BROODLING: (10000, 65000)}

# Sindragosa's own keys are declared under the same prefix, and the banner must not name them.
DECLARED = {key: (probes.HOLDER, "ICCHelpers_SindragosaAdds.cpp:1") for key in (
    "sg.gauntlet.target", "sg.gauntlet.taunt", "sg.gauntlet.dodge", "sg.gauntlet.kick", "sg.gauntlet.free",
    "sg.phase", "sg.tank", "sg.drake.tank", "sg.group")}


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def blade_hit(when: int, guid: int) -> dict:
    return {"t": when, "e": "dmg", "s": BLADE, "d": guid, "sp": 70305, "a": 3000}


def aura(when: int, guid: int, spell: int, source: int) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": source, "sp": spell, "r": 0, "st": 1}


def snaps() -> list[dict]:
    stamps = sorted({stamp for span in SAMPLES.values() for stamp in span})
    return [{"t": when, "e": "snap",
             "u": [[guid, 4170.0, 2480.0, 211.0, 0.0, 100, 0, 0, 0, 0, 0, 0, 0, 0]
                   for guid, (first, last) in SAMPLES.items() if first <= when <= last]}
            for when in stamps]


def pull(boss: str = "sindragosa-gauntlet") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": 0, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": 0} for guid, name, role in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        *({"t": 0, "e": "unit", "g": guid, "en": entry, "n": name} for guid, entry, name in UNITS),
        *snaps(),
        note(100, SHIV, "sg.gauntlet.target", "champion"),
        note(5000, SHIV, "sg.gauntlet.target", "skull"),
        note(30000, BULWARK, "sg.gauntlet.target", "rescue"),
        note(31000, BULWARK, "sg.gauntlet.target", "sorceress"),
        # Bulwark owes a taunt on the Warrior hitting a healer, then on a third-wave Champion.
        note(32000, BULWARK, "sg.gauntlet.taunt", "warrior"),
        note(34000, BULWARK, "sg.gauntlet.taunt", "none"),
        note(61000, BULWARK, "sg.gauntlet.taunt", "champion"),
        # Shiv steps out of the blade twice and is still hit twice; Bulwark never moves.
        note(40000, SHIV, "sg.gauntlet.dodge", "melee"),
        blade_hit(41000, SHIV),
        blade_hit(41500, SHIV),
        note(42000, SHIV, "sg.gauntlet.dodge", "none"),
        blade_hit(43000, BULWARK),
        note(44000, MEND, "sg.gauntlet.dodge", "clear"),
        note(45000, MEND, "sg.gauntlet.dodge", "none"),
        note(46000, SHIV, "sg.gauntlet.dodge", "melee"),
        note(47000, SHIV, "sg.gauntlet.dodge", "none"),
        # Shiv kicks the first Ice Tomb; nobody owes the second, which lands on Bulwark.
        {"t": 35000, "e": "cast", "s": SORCERESS, "sp": 71330, "tgt": BULWARK, "ct": 1500},
        note(35200, SHIV, "sg.gauntlet.kick", "sorceress"),
        note(35600, SHIV, "sg.gauntlet.kick", "none"),
        {"t": 39000, "e": "cast", "s": SORCERESS, "sp": 71330, "tgt": BULWARK, "ct": 1500},
        aura(40500, BULWARK, 71330, SORCERESS),
        # A Web pins Shiv in the second whirl, and Mend frees him; the Frost Nova on Bulwark is outside one.
        aura(45900, SHIV, 71327, WEBWEAVER),
        note(46100, MEND, "sg.gauntlet.free", "hand of freedom"),
        note(46600, MEND, "sg.gauntlet.free", "none"),
        aura(36000, BULWARK, 71320, SORCERESS),
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


def banner(records: list[dict]) -> str:
    with mock.patch.object(probes, "declared_keys", return_value=DECLARED):
        return printed(gauntlet.show_banner, load(records))


class Waves(unittest.TestCase):
    def test_spiders_split_at_the_first_frostwarden(self):
        self.assertEqual(gauntlet.wave_rows(load(pull())), [
            ("spiders, first wave", 2, 0, 22000),
            ("frostwardens", 2, 30000, 50000),
            ("spiders, third wave", 1, 60000, 80000),
            ("broodlings", 1, 10000, 65000),
        ])

    def test_section(self):
        self.assertEqual(printed(gauntlet.show_waves, load(pull())), (
            "WAVES\n"
            "    kind                  units      first       last\n"
            "    spiders, first wave       2   0:00.000   0:22.000\n"
            "    frostwardens              2   0:30.000   0:50.000\n"
            "    spiders, third wave       1   1:00.000   1:20.000\n"
            "    broodlings                1   0:10.000   1:05.000\n"))

    def test_no_frostwarden_keeps_every_spider_in_the_first_wave(self):
        records = [rec for rec in pull() if rec.get("en") not in (37228, 37229)]
        rows = gauntlet.wave_rows(load(records))
        self.assertEqual(rows[0], ("spiders, first wave", 3, 0, 80000))


class Targets(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(gauntlet.show_targets, load(pull())), (
            "TARGETS\n"
            "  per bot: spans of each resolver rule\n"
            "\n"
            "    Bulwark  rescue 1, sorceress 1\n"
            "    Shiv  champion 1, skull 1\n"))


class Taunts(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(gauntlet.show_taunts, load(pull())), (
            "TAUNTS\n"
            "  sg.gauntlet.taunt per tank: each rescue taunt it owed, from when\n"
            "\n"
            "    Bulwark  warrior 0:32.000, champion 1:01.000\n"))


class Dodges(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(gauntlet.show_dodges, load(pull())), (
            "DODGES\n"
            "  per player: sg.gauntlet.dodge rules, Frostblade hits\n"
            "\n"
            "    Bulwark  dodges -  hits 1\n"
            "    Mend  dodges clear 1  hits 0\n"
            "    Shiv  dodges melee 2  hits 2\n"))


class Kicks(unittest.TestCase):
    def test_rows_read_kickers_and_landing(self):
        rows = gauntlet.kick_rows(load(pull()))
        self.assertEqual([(row["start"], row["kickers"], row["landed"]) for row in rows],
                         [(35000, [SHIV], False), (39000, [], True)])

    def test_section(self):
        self.assertEqual(printed(gauntlet.show_kicks, load(pull())), (
            "KICKS\n"
            "  2 Ice Tomb cast(s): who owed the kick, whether the tomb landed\n"
            "\n"
            "     0:35.000  kick Shiv  stopped\n"
            "     0:39.000  kick nobody  landed\n"))


class Freedom(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(gauntlet.show_freedom, load(pull())), (
            "FREEDOM\n"
            "  sg.gauntlet.free per bot: each freeing spell it owed, from when\n"
            "    Mend  hand of freedom 0:46.100\n"
            "  Frost Nova and Web roots taken: Bulwark 1, Shiv 1\n"))


class Cheats(unittest.TestCase):
    def test_none(self):
        self.assertEqual(printed(gauntlet.show_cheats, load(pull())), "CHEATS\n  no cheat fired\n")

    def test_uses_come_from_other_code(self):
        records = pull() + [{"t": 5000, "e": "cheat", "g": BULWARK, "k": "sg.nitro", "n": 1}]
        self.assertEqual(printed(gauntlet.show_cheats, load(records)),
                         "CHEATS\n  1 use(s), none from gauntlet code: icecrown_citadel.py --cheats\n")


class Banner(unittest.TestCase):
    def test_spares_sindragosas_keys(self):
        self.assertIn("all sg.gauntlet.* probes present\n", banner(pull()))

    def test_a_missing_probe_is_named(self):
        records = [rec for rec in pull() if rec.get("k") != "sg.gauntlet.taunt"]
        self.assertIn("probes absent from this trace: sg.gauntlet.taunt\n", banner(records))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            gauntlet.show_banner(trace)
            for _, _, section in gauntlet.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Sindragosa gauntlet pull"), len(gauntlet.SECTIONS))


if __name__ == "__main__":
    unittest.main()

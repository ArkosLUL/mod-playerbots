"""Tests for the Blood-Queen Lana'thel reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25N pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not the Blood-Queen rather than raising.
"""
from __future__ import annotations

import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from bosses import blood_queen_lana_thel as bql  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
QUEEN = CREATURE | 950
BULWARK = 7601  # main tank
AEGIS = 7602    # mirror tank
MEND = 7603     # healer
ARCANE = 7604   # ranged
SHIV = 7605     # melee
ARKOS = 7606    # human ranged

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (AEGIS, "Aegis", "tank", 0), (MEND, "Mend", "heal", 0),
          (ARCANE, "Arcane", "ranged", 0), (SHIV, "Shiv", "melee", 0), (ARKOS, "Arkos", "ranged", 1))


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def aura(when: int, guid: int, spell: int, removed: int = 0) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": QUEEN, "sp": spell, "r": removed}


def pull(boss: str = "blood-queen-lana-thel") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": 1, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "mage", "h": human}
                    for guid, name, role, human in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        {"t": 1, "e": "unit", "g": QUEEN, "en": 37955, "n": "Blood-Queen Lana'thel", "lvl": 83, "b": 1},
        {"t": 1, "e": "spell", "sp": 871, "n": "Shield Wall"},
        {"t": 1, "e": "spell", "sp": 32375, "n": "Mass Dispel"},
        note(100, BULWARK, "bql.phase", "ground"),
        note(127000, BULWARK, "bql.phase", "center"),
        note(127100, MEND, "bql.phase", "center"),
        note(135000, MEND, "bql.phase", "air"),
        note(147000, BULWARK, "bql.phase", "ground"),
        # Roles; Aegis is held off her until the first vampire, Shiv braked once later on.
        note(50, BULWARK, "bql.tank", "1"),
        note(50, AEGIS, "bql.tank", "2"),
        note(60, BULWARK, "bql.target", "boss"),
        note(60, AEGIS, "bql.target", "brake"),
        note(60, AEGIS, "bql.brake", "1"),
        note(15500, AEGIS, "bql.brake", "0"),
        note(15500, AEGIS, "bql.target", "boss"),
        note(60, SHIV, "bql.target", "boss"),
        note(15000, SHIV, "bql.target", "bite"),
        note(18100, SHIV, "bql.target", "boss"),
        note(50000, SHIV, "bql.brake", "1"),
        note(50000, SHIV, "bql.target", "brake"),
        note(52000, SHIV, "bql.brake", "0"),
        note(52000, SHIV, "bql.target", "boss"),
        note(95000, SHIV, "bql.target", "kill"),
        note(100000, SHIV, "bql.target", "boss"),
        # Shiv is handed Mend, then Arcane once Mend took a shadow, and bites, round a shadow on the way;
        # Mend never bites.
        aura(15000, SHIV, 71474),
        note(15100, SHIV, "bql.bite", f"{MEND}"),
        note(15500, SHIV, "bql.bite", f"{ARCANE}"),
        note(15600, SHIV, "bql.detour", "via"),
        note(16500, SHIV, "bql.detour", "none"),
        aura(18000, SHIV, 71474, 1),
        aura(18000, SHIV, 71525),
        note(18100, SHIV, "bql.bite", "0"),
        aura(75000, MEND, 71474),
        note(75100, MEND, "bql.bite", f"{AEGIS}"),
        aura(90000, MEND, 71474, 1),
        aura(90000, MEND, 70923),
        note(90200, ARCANE, "bql.cc", f"{MEND}"),
        # Arcane runs out of CC, nobody else can land one, and Mend is killed.
        note(93000, ARCANE, "bql.cc", "0"),
        note(95000, MEND, "bql.kill", "1"),
        aura(100000, MEND, 70923, 1),
        note(100100, MEND, "bql.kill", "0"),
        # One Pact: both bots walk to the first spot, then to the human, who carries none of the probes.
        aura(20000, ARCANE, 71340),
        aura(20000, SHIV, 71340),
        aura(20000, ARKOS, 71340),
        note(20100, ARCANE, "bql.pact", "spot 1"),
        note(20100, SHIV, "bql.pact", "spot 1"),
        note(23000, ARCANE, "bql.pact", "human"),
        note(23000, SHIV, "bql.pact", "human"),
        aura(24000, ARCANE, 71340, 1),
        aura(24000, SHIV, 71340, 1),
        aura(24000, ARKOS, 71340, 1),
        note(24100, ARCANE, "bql.pact", "none"),
        note(24100, SHIV, "bql.pact", "none"),
        # Swarming Shadows on Arcane: warning aura, then the trail; it switches lanes once.
        aura(30000, ARCANE, 71264),
        aura(32000, ARCANE, 71264, 1),
        aura(32000, ARCANE, 71265),
        note(30100, ARCANE, "bql.curve", "1"),
        note(33000, ARCANE, "bql.curve", "3"),
        aura(38000, ARCANE, 71265, 1),
        note(38200, ARCANE, "bql.curve", "0"),
        note(10, ARCANE, "bql.slot", "3"),
        note(40000, ARCANE, "bql.slot", "5"),
        note(40000, MEND, "bql.dodge", "clear"),
        note(41000, MEND, "bql.dodge", "none"),
        note(10, MEND, "bql.slot", "4"),
        note(10, SHIV, "bql.group", "1"),
        note(127100, SHIV, "bql.airslot", "29"),
        note(140000, SHIV, "bql.airslot", "0"),
        # Incite Terror fears four; Mend, warded, Mass Dispels Arcane and Shiv free, the other two run it out.
        aura(127200, ARCANE, 73070),
        aura(127200, SHIV, 73070),
        aura(127200, BULWARK, 73070),
        aura(127300, ARKOS, 73070),
        note(127400, MEND, "bql.massdispel", f"{ARCANE}"),
        {"t": 129000, "e": "cast", "s": MEND, "sp": 32375, "tgt": ARCANE},
        aura(129000, ARCANE, 73070, 1),
        aura(129000, SHIV, 73070, 1),
        note(129100, MEND, "bql.massdispel", "0"),
        aura(131200, BULWARK, 73070, 1),
        aura(131300, ARKOS, 73070, 1),
        # Bulwark under 40%: offered Shield Wall, casts it a second later.
        note(60000, BULWARK, "bql.defensive", "shield wall"),
        {"t": 61000, "e": "cast", "s": BULWARK, "sp": 871, "tgt": BULWARK},
        note(66000, BULWARK, "bql.defensive", "idle"),
        {"t": 160000, "e": "end", "out": "kill"},
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


class Phases(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(bql.show_phases, load(pull())), (
            "PHASES\n"
            "  bql.phase, each step from the first bot to note it\n"
            "\n"
            "         from       held  phase\n"
            "     0:00.100   2:06.900  ground\n"
            "     2:07.000   0:08.000  center\n"
            "     2:15.000   0:12.000  air\n"
            "     2:27.000   0:13.000  ground\n"))


class Targets(unittest.TestCase):
    def test_branches_and_brakes_per_bot(self):
        self.assertEqual(printed(bql.show_targets, load(pull())), (
            "TARGETS\n"
            "  per bot: spans of each resolver branch, threat brake spans\n"
            "\n"
            "    Aegis  boss 1, brake 1  brake 1\n"
            "    Bulwark  boss 1  brake 0\n"
            "    Shiv  boss 4, brake 1, bite 1, kill 1  brake 1\n"))


class Tanks(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(bql.show_tanks, load(pull())), (
            "TANKS\n"
            "  per tank: its role, from when\n"
            "\n"
            "    Aegis  mirror 0:00.050\n"
            "    Bulwark  main 0:00.050\n"))


class Bites(unittest.TestCase):
    def test_rows_read_picks_and_outcome(self):
        rows = bql.bite_rows(load(pull()))
        self.assertEqual([(row["biter"], row["start"], row["stop"], row["picks"], row["outcome"]) for row in rows], [
            (SHIV, 15000, 18000, [f"{MEND}", f"{ARCANE}"], "bit"),
            (MEND, 75000, 90000, [f"{AEGIS}"], "frenzy"),
        ])

    def test_section(self):
        self.assertEqual(printed(bql.show_bites, load(pull())), (
            "BITES\n"
            "  2 frenzy window(s): the biter, each player it was handed, how it ended\n"
            "\n"
            "     0:15.000   0:03.000  Shiv  Mend (heal) -> Arcane (ranged)  bit\n"
            "     1:15.000   0:15.000  Mend  Aegis (tank)  frenzy\n"))


class Cc(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(bql.show_cc, load(pull())), (
            "CC\n"
            "  1 mind-controlled vampire(s): the bots bql.cc set on each, when bql.kill marked it\n"
            "\n"
            "     1:30.000   0:10.000  Mend  Arcane  kill 1:35.000\n"))


class Fear(unittest.TestCase):
    def test_waves_count_the_dispelled(self):
        waves = bql.fear_waves(load(pull()))
        self.assertEqual([(wave["start"], len(wave["feared"]), wave["early"]) for wave in waves], [(127200, 4, 2)])
        self.assertEqual(waves[0]["priests"], {MEND: {"centres": [f"{ARCANE}"], "casts": [129000]}})

    def test_section(self):
        self.assertEqual(printed(bql.show_fear, load(pull())), (
            "FEAR\n"
            "  1 Incite Terror(s): players feared and freed early, each priest's Mass Dispel centres and casts\n"
            "\n"
            "     2:07.200  feared 4, freed early 2\n"
            "      Mend  on Arcane (ranged)  cast +1.8s\n"))


class Pact(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(bql.show_pact, load(pull())), (
            "PACT\n"
            "  3 Pact member window(s): each member and the spots bql.pact noted for it\n"
            "\n"
            "     0:20.000   0:04.000  Arcane  spot 1 -> human\n"
            "     0:20.000   0:04.000  Shiv  spot 1 -> human\n"
            "     0:20.000   0:04.000  Arkos human  -\n"))


class Shadows(unittest.TestCase):
    def test_warning_and_trail_read_as_one_window(self):
        self.assertEqual(printed(bql.show_shadows, load(pull())), (
            "SHADOWS\n"
            "  1 Swarming Shadows: the carrier and the lanes bql.curve noted inside\n"
            "\n"
            "     0:30.000   0:08.000  Arcane  outer rising -> inner rising\n"))


class Spread(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(bql.show_spread, load(pull())), (
            "SPREAD\n"
            "  per bot: ground nodes (moves), air nodes, melee group, shadow dodges, walks round shadows\n"
            "\n"
            "    Arcane  ground 3/5 (moved 1)  dodges -\n"
            "    Mend  ground 4 (moved 0)  dodges clear 1\n"
            "    Shiv  air 29  group 1  dodges -  detours via 1\n"))


class Defensives(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(bql.show_defensives, load(pull())), (
            "DEFENSIVES\n"
            "  1:00.000  Bulwark     0:06.000  offered shield wall; cast Shield Wall +1.0s\n"))


class Cheats(unittest.TestCase):
    def test_none_fired(self):
        self.assertEqual(printed(bql.show_cheats, load(pull())), "CHEATS\n  no cheat of this encounter fired\n")


class Banner(unittest.TestCase):
    def test_every_probe_present(self):
        self.assertIn("all bql.* probes present", printed(bql.show_banner, load(pull())))

    def test_a_missing_probe_is_named(self):
        records = [rec for rec in pull() if rec.get("k") != "bql.pact"]
        self.assertIn("probes absent from this trace: bql.pact", printed(bql.show_banner, load(records)))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bql.show_banner(trace)
            for _, _, section in bql.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Blood-Queen Lana'thel pull"), len(bql.SECTIONS))


if __name__ == "__main__":
    unittest.main()

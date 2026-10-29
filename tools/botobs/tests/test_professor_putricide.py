"""Tests for the Professor Putricide reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not Putricide rather than raising.
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

from bosses import professor_putricide as pp  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
PUTRICIDE = CREATURE | 800
OOZE = CREATURE | 801
CLOUD = CREATURE | 802
PUDDLE = CREATURE | 803
BULWARK = 7401  # main tank
AEGIS = 7402    # assist tank, the driver
MEND = 7403     # healer
SHIV = 7404     # melee, a Death Knight sharer
ARCANE = 7405   # ranged
ARKOS = 7406    # human ranged

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (AEGIS, "Aegis", "tank", 0), (MEND, "Mend", "heal", 0),
          (SHIV, "Shiv", "melee", 0), (ARCANE, "Arcane", "ranged", 0), (ARKOS, "Arkos", "ranged", 1))


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def aura(when: int, guid: int, spell: int, source: int, removed: int = 0, stacks: int = 1) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": source, "sp": spell, "r": removed, "st": stacks}


def pull(boss: str = "professor-putricide", diff: int = 3) -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": diff, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 1},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": human}
                    for guid, name, role, human in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        {"t": 1, "e": "unit", "g": PUTRICIDE, "en": 36678, "n": "Professor Putricide", "lvl": 83, "b": 1},
        {"t": 1, "e": "unit", "g": OOZE, "en": 37697, "n": "Volatile Ooze"},
        {"t": 1, "e": "unit", "g": CLOUD, "en": 37562, "n": "Gas Cloud"},
        {"t": 1, "e": "unit", "g": PUDDLE, "en": 37690, "n": "Growing Ooze Puddle"},
        note(100, BULWARK, "pp.phase", "1"),
        note(110, MEND, "pp.phase", "1"),
        note(90000, MEND, "pp.phase", "2"),
        note(90100, BULWARK, "pp.phase", "2"),
        note(200000, BULWARK, "pp.phase", "3"),
        note(100, ARCANE, "pp.target", "boss"),
        note(30000, ARCANE, "pp.target", "ooze"),
        # The ooze roots Arcane; Mend and Aegis stand on it, Arcane's own note stays none.
        aura(30000, ARCANE, 72836, OOZE),
        note(30100, MEND, "pp.stack", str(ARCANE)),
        note(30200, AEGIS, "pp.stack", str(ARCANE)),
        note(30100, ARCANE, "pp.stack", "none"),
        aura(36000, ARCANE, 72836, OOZE, removed=1),
        note(36100, MEND, "pp.stack", "none"),
        # Shiv is bloated, runs away, then along the wall; Arkos is bloated later and dies to it.
        aura(70000, SHIV, 72455, CLOUD, stacks=10),
        note(70100, SHIV, "pp.kite", "away"),
        note(72000, SHIV, "pp.kite", "wall"),
        note(73000, SHIV, "pp.kite", "away"),
        aura(90000, SHIV, 72455, CLOUD, removed=1),
        aura(150000, ARKOS, 72455, CLOUD, stacks=10),
        {"t": 155000, "e": "death", "g": ARKOS, "killer": CLOUD},
        aura(155000, ARKOS, 72455, CLOUD, removed=1),
        # Aegis drives: drinks, eats a puddle, spits on the cloud, slashes him.
        note(100, 0, "pp.driver", "0"),
        note(2000, 0, "pp.driver", str(AEGIS)),
        note(2000, AEGIS, "pp.abom", "drink"),
        note(6000, AEGIS, "pp.abom", f"eat {PUDDLE}"),
        note(9000, AEGIS, "pp.abom", f"slash {PUTRICIDE}"),
        note(71000, AEGIS, "pp.abom", f"spit {CLOUD}"),
        note(72000, AEGIS, "pp.abom", f"slash {PUTRICIDE}"),
        note(80000, AEGIS, "pp.abom", "exit"),
        aura(71500, CLOUD, 72457, CREATURE | 900),
        note(200100, 0, "pp.driver", "0"),
        # Phase 3: Bulwark holds two, Aegis takes him, Shiv shares one, Bulwark takes him back.
        aura(210000, BULWARK, 72463, PUTRICIDE, stacks=1),
        aura(220000, BULWARK, 72463, PUTRICIDE, stacks=2),
        note(229000, AEGIS, "pp.swap", "take"),
        note(229500, AEGIS, "pp.swap", "none"),
        aura(230000, AEGIS, 72463, PUTRICIDE, stacks=1),
        aura(240000, AEGIS, 72463, PUTRICIDE, stacks=2),
        note(247500, SHIV, "pp.sharehp", "45"),
        note(247600, SHIV, "pp.swap", "share"),
        note(248000, SHIV, "pp.swap", "none"),
        note(257500, SHIV, "pp.sharehp", "20"),
        aura(250000, SHIV, 72463, PUTRICIDE, stacks=1),
        note(250300, BULWARK, "pp.swap", "back"),
        note(250300, SHIV, "pp.brake", "1"),
        note(254000, SHIV, "pp.brake", "0"),
        # Heroic: Arcane carries the plague, steps clear, then walks onto Mend.
        aura(40000, ARCANE, 72856, PUTRICIDE),
        note(40100, ARCANE, "pp.plague", "clear"),
        note(41000, ARCANE, "pp.plague", "hold"),
        note(47000, ARCANE, "pp.receiver", str(MEND)),
        note(48000, ARCANE, "pp.plague", "pass"),
        aura(49000, ARCANE, 72856, PUTRICIDE, removed=1),
        note(49100, ARCANE, "pp.plague", "none"),
        note(49000, ARCANE, "pp.receiver", "0"),
        aura(49000, MEND, 72856, PUTRICIDE),
        aura(60000, MEND, 72856, PUTRICIDE, removed=1),
        # Bulwark catches it holding him, so Arcane walks onto Bulwark.
        aura(100000, BULWARK, 72856, PUTRICIDE),
        note(100100, BULWARK, "pp.plague", "hold"),
        note(108000, ARCANE, "pp.plague", "receive"),
        note(109000, ARCANE, "pp.plague", "none"),
        aura(109000, BULWARK, 72856, PUTRICIDE, removed=1),
        # Dodges: goo on Arcane, gas under Shiv, a carrier near Mend, and holds that don't count.
        note(95000, ARCANE, "pp.dodge", "goo move 8"),
        note(95100, ARCANE, "pp.dodge", "hold"),
        note(120000, SHIV, "pp.dodge", "gas move 4"),
        note(121000, SHIV, "pp.dodge", "gas tight 3"),
        note(44000, MEND, "pp.dodge", "carrier none"),
        # The spot moves once; Arcane is pushed off its inner slot onto the outer ring.
        note(100, 0, "pp.spot", "1"),
        note(130000, 0, "pp.spot", "4"),
        note(100, 0, "pp.rings", "0"),
        note(8000, 0, "pp.rings", "1"),
        note(100, ARCANE, "pp.slot", "3"),
        note(100, MEND, "pp.slot", "1"),
        note(131000, ARCANE, "pp.slot", "20"),
        {"t": 26000, "e": "cheat", "g": BULWARK, "k": "pp.heroicbuff", "n": 4, "tgt": ARKOS},
        {"t": 27000, "e": "cheat", "g": MEND, "k": "rf.spitefulfury", "n": 2},
        {"t": 260000, "e": "end", "out": "kill"},
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
        self.assertEqual(printed(pp.show_phases, load(pull())), (
            "PHASES\n"
            "  pp.phase, each step from the first bot to note it\n"
            "\n"
            "         from       held  phase\n"
            "     0:00.100   1:29.900  1\n"
            "     1:30.000   1:50.000  2\n"
            "     3:20.000   1:00.000  3\n"))


class Adds(unittest.TestCase):
    def test_windows_carry_peak_stacks_and_deaths(self):
        rows = pp.aura_windows(load(pull()), pp.GASEOUS_BLOAT)
        self.assertEqual([(row["victim"], row["start"], row["stop"], row["stacks"], row["died"]) for row in rows],
                         [(SHIV, 70000, 90000, 10, False), (ARKOS, 150000, 155000, 10, True)])

    def test_stackers_count_only_the_victim(self):
        self.assertEqual(pp.stackers(load(pull()), ARCANE, 30000, 36000), {MEND, AEGIS})

    def test_section(self):
        self.assertEqual(printed(pp.show_adds, load(pull())), (
            "ADDS\n"
            "  1 Volatile Ooze victim(s); stack counts the bots that noted standing on each\n"
            "     0:30.000   0:06.000  Arcane  stack 2\n"
            "\n"
            "  2 Gaseous Bloat(s); legs counts the pp.kite branch switches inside each\n"
            "     1:10.000   0:20.000  Shiv  away 2, wall 1\n"
            "     2:30.000   0:05.000  Arkos human  died  -\n"))


class Abomination(unittest.TestCase):
    def test_duties_drop_the_unit(self):
        self.assertEqual(pp.duty_counts(load(pull())), {"drink": 1, "eat": 1, "exit": 1, "slash": 2, "spit": 1})

    def test_section(self):
        self.assertEqual(printed(pp.show_abom, load(pull())), (
            "ABOMINATION\n"
            "  driver: none@0:00.100, Aegis@0:02.000, none@3:20.100\n"
            "  duty switches: drink 1, eat 1, exit 1, slash 2, spit 1\n"
            "  1 Regurgitated Ooze\n"
            "     1:11.500  Gas Cloud\n"))


class Plague(unittest.TestCase):
    def test_peaks(self):
        self.assertEqual(pp.plague_peaks(load(pull())), {BULWARK: 2, AEGIS: 2, SHIV: 1})

    def test_swaps_skip_none(self):
        self.assertEqual(pp.swap_rows(load(pull())),
                         [(229000, AEGIS, "take"), (247600, SHIV, "share"), (250300, BULWARK, "back")])

    def test_share_model_spans_lowest_to_highest(self):
        self.assertEqual(pp.share_health(load(pull())), {SHIV: (20, 45)})

    def test_receivers_only_their_walks(self):
        self.assertEqual(pp.receive_rows(load(pull())), [(108000, ARCANE)])

    def test_section(self):
        self.assertEqual(printed(pp.show_plague, load(pull())), (
            "PLAGUE\n"
            "  Mutated Plague peak stacks: Aegis 2, Bulwark 2, Shiv 1\n"
            "  3 swap election(s)\n"
            "     3:49.000  Aegis           take\n"
            "     4:07.600  Shiv            share\n"
            "     4:10.300  Bulwark         back\n"
            "  share health model: Shiv 20-45% (passed over)\n"
            "  braked: Shiv\n"
            "\n"
            "  3 Unbound Plague carrier(s)\n"
            "     0:40.000   0:09.000  Arcane  hold 1, clear 1, pass 1\n"
            "     0:49.000   0:11.000  Mend  -\n"
            "     1:40.000   0:09.000  Bulwark  hold 1\n"
            "  receivers walking in: Arcane@1:48.000\n"))

    def test_normal_mode_has_no_unbound_plague(self):
        self.assertIn("normal mode: no Unbound Plague", printed(pp.show_plague, load(pull(diff=1))))


class Dodges(unittest.TestCase):
    def test_counts_drop_yards_and_holds(self):
        self.assertEqual(pp.dodge_counts(load(pull())),
                         {"goo move": 1, "gas move": 1, "gas tight": 1, "carrier none": 1})

    def test_section(self):
        self.assertIn("  by bot: Shiv 2, Mend 1, Arcane 1\n", printed(pp.show_dodges, load(pull())))


class Spread(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(pp.show_spread, load(pull())), (
            "SPREAD\n"
            "  boss spot: 1@0:00.100, 4@2:10.000\n"
            "  rings: released@0:00.100, held@0:08.000\n"
            "\n"
            "  pp.slot per bot, each slot it held\n"
            "    Arcane          inner 3@0:00.100, outer 20@2:11.000\n"
            "    Mend            inner 1@0:00.100\n"))

    def test_slot_names(self):
        self.assertEqual([pp.slot_name(value) for value in ("0", "14", "15", "38")],
                         ["released", "inner 14", "outer 15", "outer 38"])


class Cheats(unittest.TestCase):
    def test_own_keys_only_and_the_rest_counted(self):
        self.assertEqual(printed(pp.show_cheats, load(pull())), (
            "CHEATS\n"
            "  pp.heroicbuff  4 use(s) from 0:26.000\n"
            "    by  Bulwark 4\n"
            "    on  Arkos human\n"
            "  2 use(s) under other encounters' keys: icecrown_citadel.py --cheats\n"))


class Banner(unittest.TestCase):
    def test_normal_mode_does_not_miss_the_heroic_probes(self):
        records = [rec for rec in pull(diff=1) if rec.get("k") != "pp.receiver"]
        self.assertNotIn("pp.receiver", pp.missing_probes(load(records)))

    def test_heroic_names_them(self):
        records = [rec for rec in pull() if rec.get("k") != "pp.receiver"]
        self.assertIn("pp.receiver", pp.missing_probes(load(records)))

    def test_every_probe_the_pull_writes_counts(self):
        gone = pp.missing_probes(load(pull()))
        for key in ("pp.phase", "pp.target", "pp.stack", "pp.kite", "pp.driver", "pp.abom", "pp.swap", "pp.sharehp",
                    "pp.brake", "pp.plague", "pp.receiver", "pp.dodge", "pp.spot", "pp.rings", "pp.slot"):
            self.assertNotIn(key, gone)


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            pp.show_banner(trace)
            for _, _, section in pp.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Professor Putricide pull"), len(pp.SECTIONS))


if __name__ == "__main__":
    unittest.main()

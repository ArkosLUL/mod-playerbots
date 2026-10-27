"""Tests for the raid-wide Icecrown Citadel reader and the v14 cheat record.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H Sindragosa pull pins `--cheats` and `--phases` and the validity banner's two ICC
lines, smaller ones pin a pre-v14 trace and one filed under the map, and the shared v14 fixture
pins the cheat record off ICC.
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

from bosses import icecrown_citadel as icc  # noqa: E402
from raidobs import validity  # noqa: E402
from raidobs.trace import Trace, cheat_uses  # noqa: E402
from raidobs.verify import verify_checks  # noqa: E402

FULL_V13 = BOTOBS / "fixtures" / "full-v13.ndjson"
FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
SINDRAGOSA = CREATURE | 500

BULWARK = 7101  # tank
AEGIS = 7102    # tank
MEND = 7103     # healer
SHIV = 7104     # melee
ARKOS = 7105    # human melee

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (AEGIS, "Aegis", "tank", 0), (MEND, "Mend", "heal", 0),
          (SHIV, "Shiv", "melee", 0), (ARKOS, "Arkos", "melee", 1))


def header(boss: str, diff: int, version: int, iccbuffs: int, map_id: int = 631) -> dict:
    cfg = {"cheats": "food,taxi,raid", "mapthreads": 2, "hardmode": {}}
    if version >= 14:
        cfg["iccbuffs"] = iccbuffs
    return {"e": "hdr", "v": version, "ts": 1789800000000, "map": map_id, "inst": 5, "diff": diff,
            "boss": boss, "bin": 1789799000000, "cfg": cfg,
            "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": human}
                       for guid, name, role, human in ROSTER]}


def phase(when: int, guid: int, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": "sg.phase", "txt": value}


def cheat(when: int, guid: int, key: str, uses: int, target: int | None = None) -> dict:
    rec = {"t": when, "e": "cheat", "g": guid, "k": key, "n": uses}
    if target is not None:
        rec["tgt"] = target
    return rec


def pull(boss: str = "sindragosa", diff: int = 3, version: int = 14, iccbuffs: int = 1,
         cheats: bool = True) -> list[dict]:
    records = [
        header(boss, diff, version, iccbuffs),
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        {"t": 1, "e": "unit", "g": SINDRAGOSA, "en": 36853, "n": "Sindragosa", "lvl": 83, "mhp": 38000000,
         "b": 1},
        {"t": 0, "e": "snap", "hz": [], "u": [[guid, 0.0, 0.0, 0.0, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0]
                                              for guid, _, _, _ in ROSTER]},
        # two bots note each step, and ground comes back after the air phase
        phase(100, BULWARK, "ground"),
        phase(150, MEND, "ground"),
        phase(60000, MEND, "air"),
        phase(60250, BULWARK, "air"),
        phase(105000, BULWARK, "ground"),
        phase(105500, MEND, "ground"),
        phase(140000, MEND, "last"),
        {"t": 150000, "e": "end", "out": "kill"},
    ]
    if cheats:
        records += [
            cheat(500, BULWARK, "icc.heroicbuff", 1, ARKOS),
            cheat(10600, BULWARK, "icc.heroicbuff", 48, MEND),
            cheat(60500, SHIV, "sg.nitro", 1),
            cheat(60600, MEND, "sg.nitro", 1),
            cheat(75000, SHIV, "sg.nitro", 2),
            cheat(80000, SHIV, "rf.nitro", 1),
            cheat(120000, AEGIS, "sg.mysticbuffet", 1),
        ]
    return records


def write(folder: str, records: list[dict]) -> pathlib.Path:
    path = pathlib.Path(folder) / f"631_5_{records[0]['boss']}_1789800000.ndjson"
    path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
    return path


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        return Trace(write(folder, records))


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


def banner_lines(trace: Trace) -> list[str]:
    return printed(validity.show_validity, trace).splitlines()


class CheatUses(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.uses = cheat_uses(load(pull()))

    def test_uses_sum_n_and_first_is_the_earliest_row(self):
        nitro = self.uses["sg.nitro"]
        self.assertEqual((nitro["n"], nitro["first"]), (4, 60500))
        self.assertEqual(nitro["bots"], {SHIV: 3, MEND: 1})

    def test_targets_are_the_units_rows_name_and_no_tgt_is_the_bot(self):
        self.assertEqual(self.uses["icc.heroicbuff"]["targets"], {ARKOS, MEND})
        self.assertEqual(self.uses["sg.nitro"]["targets"], {0})

    def test_every_key_is_read(self):
        self.assertEqual(sorted(self.uses), ["icc.heroicbuff", "rf.nitro", "sg.mysticbuffet", "sg.nitro"])


class Cheats(unittest.TestCase):
    def test_each_key_first_used_first(self):
        self.assertEqual(printed(icc.show_cheats, load(pull())), (
            "CHEATS\n"
            "  conf BotCheats food,taxi,raid, EnableICCBuffs 1\n"
            "  4 kind(s), 55 use(s), first used first\n"
            "\n"
            "  icc.heroicbuff  49 use(s) from 0:00.500\n"
            "    by  Bulwark 49\n"
            "    on  Arkos human, Mend\n"
            "\n"
            "  sg.nitro  4 use(s) from 1:00.500\n"
            "    by  Shiv 3, Mend 1\n"
            "    on  self\n"
            "\n"
            "  rf.nitro  1 use(s) from 1:20.000  other encounter\n"
            "    by  Shiv 1\n"
            "    on  self\n"
            "\n"
            "  sg.mysticbuffet  1 use(s) from 2:00.000\n"
            "    by  Aegis 1\n"
            "    on  self\n"))

    def test_a_map_named_trace_marks_no_key_as_another_encounters(self):
        rows = icc.cheat_rows(load(pull(boss="icecrown-citadel")))
        self.assertEqual({row["own"] for row in rows}, {None})

    def test_a_pull_with_no_cheat_says_so(self):
        self.assertIn("  no cheat fired\n", printed(icc.show_cheats, load(pull(cheats=False))))

    def test_a_pre_v14_trace_cannot_say(self):
        self.assertEqual(printed(icc.show_cheats, load(pull(version=13, cheats=False))),
                         "CHEATS\n  schema v13 predates the cheat record (v14)\n")


class Phases(unittest.TestCase):
    def test_steps_merge_the_bots_and_keep_a_phase_that_comes_back(self):
        self.assertEqual(icc.phase_steps(load(pull())), [
            ("ground", 100, 60000), ("air", 60000, 105000), ("ground", 105000, 140000), ("last", 140000, 150000),
        ])

    def test_section(self):
        self.assertEqual(printed(icc.show_phases, load(pull())), (
            "PHASES\n"
            "  sg.phase, each step from the first bot to note it\n"
            "\n"
            "         from       held  phase\n"
            "     0:00.100   0:59.900  ground\n"
            "     1:00.000   0:45.000  air\n"
            "     1:45.000   0:35.000  ground\n"
            "     2:20.000   0:10.000  last\n"))

    def test_the_gauntlet_shares_sindragosas_prefix(self):
        self.assertEqual(icc.phase_key(load(pull(boss="sindragosa-gauntlet"))), "sg.phase")

    def test_never_written(self):
        records = [rec for rec in pull() if rec.get("e") != "note"]
        self.assertEqual(printed(icc.show_phases, load(records)), "PHASES\n  sg.phase never written\n")

    def test_a_map_named_trace_has_no_phase_key(self):
        trace = load(pull(boss="icecrown-citadel"))
        self.assertIsNone(icc.phase_key(trace))
        self.assertEqual(printed(icc.show_phases, trace),
                         "PHASES\n  icecrown-citadel has no contract prefix, so there is no phase key to read\n")


class Banner(unittest.TestCase):
    def test_banner(self):
        self.assertEqual(printed(icc.show_banner, load(pull())), "sindragosa 25H  kill at 2:30.000  0 death(s)\n")

    def test_a_map_named_trace_says_it_has_no_encounter(self):
        self.assertIn("  icecrown-citadel is no encounter in the ICC naming contract, so no phase key applies\n",
                      printed(icc.show_banner, load(pull(boss="icecrown-citadel"))))

    def test_main_runs_every_section(self):
        with tempfile.TemporaryDirectory() as folder:
            path = write(folder, pull())
            with mock.patch.object(sys, "argv", ["icecrown_citadel.py", str(path)]):
                out = io.StringIO()
                with contextlib.redirect_stdout(out):
                    self.assertEqual(icc.main(), 0)
        text = out.getvalue()
        self.assertLess(text.index("sindragosa 25H"), text.index("CHEATS"))
        self.assertLess(text.index("CHEATS"), text.index("PHASES"))


class ValidityBanner(unittest.TestCase):
    def test_heroic_with_the_buffs_on_is_flagged(self):
        self.assertIn("iccbuffs Playerbots.EnableICCBuffs = 1  <-- heroic: the buffs acted on this pull",
                      banner_lines(load(pull())))

    def test_normal_with_the_buffs_on_says_they_never_act(self):
        self.assertIn("iccbuffs Playerbots.EnableICCBuffs = 1  (normal mode: the buffs only act on heroic)",
                      banner_lines(load(pull(diff=1))))

    def test_the_buffs_off_are_plain(self):
        self.assertIn("iccbuffs Playerbots.EnableICCBuffs = 0", banner_lines(load(pull(iccbuffs=0))))

    def test_the_cheat_line_lists_the_most_used_first(self):
        self.assertIn("cheated 4 kind(s), 55 use(s): icc.heroicbuff, sg.nitro, rf.nitro, sg.mysticbuffet",
                      banner_lines(load(pull())))

    def test_a_long_cheat_line_is_cut(self):
        records = pull(cheats=False) + [cheat(1000 + index, SHIV, f"sg.kind{index}", 1) for index in range(8)]
        line = next(line for line in banner_lines(load(records)) if line.startswith("cheated"))
        self.assertTrue(line.endswith("sg.kind5 and 2 more"), line)

    def test_an_icc_pull_with_no_cheat_says_none(self):
        self.assertIn("cheated none recorded", banner_lines(load(pull(cheats=False))))

    def test_a_pre_v14_icc_pull_cannot_say(self):
        lines = banner_lines(load(pull(version=13, cheats=False)))
        self.assertFalse([line for line in lines if line.startswith(("cheated", "iccbuffs"))])

    def test_neither_line_disqualifies(self):
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(validity.show_validity(load(pull())), 0)


class FullV14(unittest.TestCase):
    def test_every_invariant_passes(self):
        failures = [(label, count) for label, count, _ in verify_checks(Trace(FULL_V14)) if count]
        self.assertEqual(failures, [])

    def test_cheat_rows_load_with_and_without_a_target(self):
        rows = Trace(FULL_V14).of("cheat")
        self.assertEqual([("tgt" in rec) for rec in rows], [True, False, False])
        uses = cheat_uses(Trace(FULL_V14))
        self.assertEqual(uses["fixture.speed"]["n"], 4)
        self.assertEqual(uses["fixture.fixate"]["targets"], {4294967400})

    def test_off_icc_the_banner_names_cheats_but_not_the_buffs(self):
        lines = banner_lines(Trace(FULL_V14))
        self.assertIn("cheated 2 kind(s), 5 use(s): fixture.speed, fixture.fixate", lines)
        self.assertFalse([line for line in lines if line.startswith("iccbuffs")])

    def test_a_v13_trace_has_no_cheat_line(self):
        self.assertFalse([line for line in banner_lines(Trace(FULL_V13)) if line.startswith("cheated")])


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            icc.show_banner(trace)
            for _, _, section in icc.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace, every section below reads empty", out.getvalue())
        self.assertNotIn("fixture.speed", out.getvalue())


if __name__ == "__main__":
    unittest.main()

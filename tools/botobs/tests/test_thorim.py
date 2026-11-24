"""Tests for the Thorim reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic phase 2 pins one tank trade, a Chain Lightning cast from both tanks into slot 2, the waiting tank
on the off-tank spot and the hazards on each tank; a Sif Blizzard over slot 2 pins the ring Blizzard section. The
shared v14 fixture shows every section reads empty elsewhere.
"""
from __future__ import annotations

import contextlib
import io
import json
import math
import pathlib
import sys
import tempfile
import unittest

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from bosses import thorim  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

TANK_A = 8801
TANK_B = 8802
MELEE = 8803
RANGED = 8804
HEALER = 8805
THORIM = (1 << 32) | 1
GUARD = (1 << 32) | 2

TANK_SPOT = (2110.75, -252.65)
OFFTANK_SPOT = (2107.25, -252.65)
BOSS = (2115.5, -256.5)
FACING = math.atan2(TANK_SPOT[1] - BOSS[1], TANK_SPOT[0] - BOSS[0])


def snap(t: int) -> dict:
    holder, waiting = (TANK_A, TANK_B) if t < 20000 else (TANK_B, TANK_A)
    melee_hp = 100 if t < 30000 else 0
    units = [
        [THORIM, BOSS[0], BOSS[1], 419.5, FACING, 100 - (t - 1000) / 1000, 0, holder],
        [holder, TANK_SPOT[0], TANK_SPOT[1], 419.44, 0, 90, 0, THORIM],
        [waiting, OFFTANK_SPOT[0], OFFTANK_SPOT[1], 419.37, 0, 95, 0, THORIM],
        [MELEE, 2121.0, -249.0, 419.6, 0, melee_hp, 0, THORIM],
        [RANGED, 2135.0, -265.0, 419.8, 0, 100, 80, THORIM],
    ]
    return {"t": t, "e": "snap", "u": units}


def pull() -> list[dict]:
    roster = [{"g": TANK_A, "n": "Anvil", "r": "tank", "c": 1, "h": 0},
              {"g": TANK_B, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
              {"g": MELEE, "n": "Cleaver", "r": "melee", "c": 4, "h": 0},
              {"g": RANGED, "n": "Darter", "r": "ranged", "c": 3, "h": 0},
              {"g": HEALER, "n": "Mender", "r": "heal", "c": 5, "h": 0}]
    records = [
        {"e": "hdr", "v": 14, "ts": 1795381228000, "map": 603, "inst": 4, "diff": 1, "boss": "thorim",
         "bin": 1795380000000, "cfg": {"cheats": "", "mapthreads": 6, "hardmode": {}, "iccbuffs": 0},
         "roster": roster},
        {"t": 0, "e": "unit", "g": THORIM, "en": 32865, "n": "Thorim", "b": 1},
        {"t": 0, "e": "unit", "g": GUARD, "en": 32874, "n": "Iron Ring Guard"},
        {"t": 0, "e": "spell", "sp": 62331, "n": "Impale"},
        {"t": 0, "e": "pull", "boss": "thorim", "src": "step"},
        {"t": 500, "e": "death", "g": HEALER, "killer": GUARD, "x": 2220.0, "y": -300.0, "z": 412.1,
         "blow": [GUARD, 9000], "rewind": [[500, GUARD, 62331, 9000]]},
        {"t": 1000, "e": "note", "g": TANK_A, "k": "thorim.p2start", "txt": 1000},
        {"t": 1000, "e": "note", "g": MELEE, "k": "thorim.ringspot", "txt": "slot 2 bearing 44 solved 44"},
        {"t": 2000, "e": "note", "g": RANGED, "k": "thorim.sifjoined", "txt": 1},
        {"t": 10000, "e": "dmg", "s": THORIM, "d": TANK_B, "sp": 62602, "a": 4500, "ok": 0, "hp": 90.0},
        {"t": 17000, "e": "move", "g": TANK_B, "k": "point", "ok": 1, "by": "thorim phase 2 positioning action"},
        {"t": 18000, "e": "dmg", "s": THORIM, "d": TANK_A, "sp": 64390, "a": 5000, "ok": 0, "hp": 85.0},
        {"t": 18001, "e": "dmg", "s": THORIM, "d": TANK_B, "sp": 64390, "a": 7500, "ok": 0, "hp": 88.0},
        {"t": 18002, "e": "dmg", "s": THORIM, "d": MELEE, "sp": 64390, "a": 11250, "ok": 0, "hp": 70.0},
        {"t": 19000, "e": "aura", "d": TANK_A, "s": THORIM, "sp": 62130, "r": 0, "st": 1, "dur": 15000, "p": 0},
        {"t": 20000, "e": "move", "g": MELEE, "k": "point", "ok": 1, "by": "thorim phase 2 positioning action"},
        {"t": 25000, "e": "move", "g": TANK_A, "k": "point", "ok": 1, "by": "reach melee"},
        {"t": 30000, "e": "dmg", "s": THORIM, "d": TANK_B, "sp": 62466, "a": 20000, "ok": 0, "hp": 60.0},
        {"t": 30000, "e": "death", "g": MELEE, "killer": THORIM, "x": 2121.0, "y": -249.0, "z": 419.6,
         "blow": [THORIM, 11250], "rewind": [[30000, THORIM, 64390, 11250]]},
    ]
    records += [snap(t) for t in range(1000, 40001, 1000)]
    records.append({"t": 40000, "e": "end", "out": "kill"})
    return records


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / "603_4_thorim_1795381228.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


class Phase2(unittest.TestCase):
    def setUp(self):
        self.text = printed(thorim.show_phase2, load(pull()))

    def test_header_and_hard_mode(self):
        self.assertIn("from 0:01.000, hard mode, 39.0 s on the floor; settled snapshots with a tank holding him 25",
                      self.text)

    def test_chain_lightning_from_the_tanks_into_slot_2(self):
        self.assertIn("casts 1, hits on players 3; on tanks 2 (12k), on slot 0/1/2 0/0/1 (slot 2 11k); "
                      "tank-to-ring hops 1; deaths 1", self.text)

    def test_thorim_spot_and_facing(self):
        self.assertIn("off the tank spot p50 6.1 p90 6.1 yd; facing off the tank-spot axis p50 0.0 p90 0.0 deg, "
                      "over 20 deg 0.0%", self.text)

    def test_trade_and_swap(self):
        self.assertIn("1 tank-to-tank, Thorim moved p50 0.0 p90 0.0 yd across one; strike to swap p50 1.0 max 1.0 s, "
                      "0 unanswered; on a non-tank with a tank up 0.0 s", self.text)

    def test_pickup_at_the_landing(self):
        self.assertIn("pickup            a tank on him at +0.0 s; before it Thorim's melee on non-tanks 0k from 0 swings",
                      self.text)

    def test_pickup_after_a_loose_landing(self):
        records = pull()
        for rec in records:
            if rec.get("e") == "snap" and rec["t"] <= 3000:
                rec["u"][0][7] = RANGED
        records.append({"t": 2500, "e": "dmg", "s": THORIM, "d": RANGED, "a": 21000, "ok": 0, "hp": 5.0})
        records.sort(key=lambda rec: rec.get("t", -1))
        text = printed(thorim.show_phase2, load(records))
        self.assertIn("pickup            a tank on him at +3.0 s; before it Thorim's melee on non-tanks 21k from 1 swing",
                      text)

    def test_opening_trade_from_the_off_tank_spot(self):
        self.assertIn("opening           first trade +19.0 s, the taker 3.5 yd off the tank spot, Thorim up to 6.1 yd "
                      "off it in the next 8 s; 1.00 %/s over the first 30 s", self.text)

    def test_waiting_tank_on_the_off_tank_spot(self):
        self.assertIn("offset from the tank spot (-3.5, +0.0) yd, on the holder 0.0%, from Thorim p50 9.1 p90 9.1 yd, "
                      "past 9.0 yd 100.0%; moves: thorim phase 2 positioning action 1, reach melee 1", self.text)

    def test_bridges_ring_and_camp(self):
        self.assertIn("waiting tank within 8 yd of a non-tank body 0.0%, of a ring body 0.0%, of slot 2 0.0%; "
                      "holder of a ring body 0.0%", self.text)
        self.assertIn("off Thorim p50 9.3 yd, in his front arc 100.0%, two slots within 8 yd -, on Thorim to +100 s "
                      "100.0%, moves per ring bot a minute 2.5", self.text)
        self.assertIn("off Thorim p50 21.3 yd, a camp body within 8 yd of a tank 0.0%", self.text)

    def test_hazards_split_by_holder(self):
        self.assertIn("holder / waiting: Blizzard 0 (0k) / 1 (4k), Lightning Charge 1 (20k) / 0 (0k), "
                      "Frost Nova 0 (0k) / 0 (0k)", self.text)
        self.assertIn("100.0% to 61.0%, 1.00 %/s", self.text)

    def test_untouchable_without_the_probe(self):
        self.assertIn("untouchable       no thorim.unreach probe (a build before it); "
                      "held without losing health 0x, 0.0 s", self.text)

    def test_untouchable_flag_and_held_stretch(self):
        records = pull()
        for rec in records:
            if rec.get("e") == "snap" and 5000 <= rec["t"] <= 9000:
                rec["u"][0][5] = 96.0
        records += [
            {"t": 5000, "e": "note", "k": "thorim.unreach", "txt": str(TANK_A)},
            {"t": 6500, "e": "move", "g": TANK_A, "k": "point", "ok": 1, "by": "thorim unreachable step action"},
            {"t": 9000, "e": "note", "k": "thorim.unreach", "txt": "0"},
        ]
        text = printed(thorim.show_phase2, load(records))
        self.assertIn("untouchable       cannot-reach flag 1x, 4.0 s, longest 4.0 s (Anvil 1); step moves 1; "
                      "held without losing health 1x, 4.0 s; +4.0..+8.0 s 96.0% to max 96.0%", text)

    def test_no_phase_2(self):
        records = [rec for rec in pull() if rec.get("k") != "thorim.p2start"]
        self.assertIn("phase 2 never started", printed(thorim.show_phase2, load(records)))


BUNNY = (1 << 32) | 3
ZONE = (2125.0, -245.0)


def blizzard_pull() -> list[dict]:
    records = pull()
    for rec in records:
        if rec.get("e") != "snap":
            continue
        if 20000 <= rec["t"] <= 25000:
            rec["u"].append([BUNNY, 2130.0, -240.0, 419.5, 0, 100, 0, 0])
        if 20000 <= rec["t"] <= 27000:
            rec["hz"] = [[62602, ZONE[0], ZONE[1], 419.5, 8.0, 1]]
        if rec["t"] >= 28000:
            rec["u"][3][1:3] = [2120.0, -250.0]
    records += [
        {"t": 0, "e": "unit", "g": BUNNY, "en": 32879, "n": "Thorim Controller"},
        {"t": 20500, "e": "note", "g": MELEE, "k": "thorim.ringstep", "txt": "1"},
        {"t": 20600, "e": "move", "g": MELEE, "k": "point", "ok": 1, "by": "thorim sif blizzard action"},
        {"t": 27500, "e": "note", "g": MELEE, "k": "thorim.ringstep", "txt": "0"},
    ]
    records += [{"t": t, "e": "dmg", "s": THORIM, "d": MELEE, "sp": 62602, "a": 4500, "ok": 0, "hp": 90.0}
                for t in (21000, 22000, 23000)]
    return sorted(records, key=lambda rec: rec.get("t", 0))


class RingBlizzard(unittest.TestCase):
    def test_one_blizzard_on_slot_2(self):
        text = printed(thorim.show_blizzard, load(blizzard_pull()))
        self.assertIn("blizzard 1 +19.0..+34.0 s: slot points covered 2 7.0 s; in it Cleaver 7.0 s (1x, longest "
                      "7.0 s, 3 ticks 13k); deaths 0; steps 1, stepped 7.0 bot-s, out of reach 7.0 of them; walk back "
                      "p50 0.5 max 0.5 s, 0 ticks on the way", text)
        self.assertIn("all: ring melee in a zone 7.0 bot-s, stays p50 7.0 max 7.0 s, 3 ticks 13k, deaths 0; steps 1, "
                      "stepped 7.0 bot-s, out of reach 7.0; walk back p50 0.5 max 0.5 s, 0 ticks on the way", text)

    def test_without_the_probe(self):
        records = [rec for rec in blizzard_pull() if rec.get("k") != "thorim.ringstep"]
        self.assertIn("deaths 0; no thorim.ringstep probe (a build before it)",
                      printed(thorim.show_blizzard, load(records)))

    def test_normal_mode(self):
        self.assertIn("no Sif Blizzard in phase 2", printed(thorim.show_blizzard, load(pull())))


class Deaths(unittest.TestCase):
    def test_by_phase_and_mechanic(self):
        self.assertEqual(printed(thorim.show_deaths, load(pull())), (
            "DEATHS BY MECHANIC\n"
            "  phase 1    1  Iron Ring Guard Impale\n"
            "  phase 2    1  Chain Lightning\n"))

    def test_banner(self):
        self.assertIn("outcome kill at 0:40.000, 2 death(s), phase 2 at 0:01.000, hard mode",
                      printed(thorim.show_banner, load(pull())))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
            thorim.show_banner(trace)
            for _, _, section in thorim.SECTIONS:
                section(trace)
        self.assertIn("not a Thorim pull", out.getvalue())


if __name__ == "__main__":
    unittest.main()

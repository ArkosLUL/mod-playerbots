"""Tests for the Thorim reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic phase 2 pins one tank trade, a Chain Lightning cast from both tanks into slot 2, the waiting tank
on the off-tank spot and the hazards on each tank; a Sif Blizzard over slot 2 pins the ring Blizzard section; a
second cast from the camp through a stepped slot 0 into slot 2 pins the chains section; one lit orb with a slowed
camp bot dying short of its row and another on a cone step pins the shelters section; the same orb over a held
ring walk and a ring walker dying past reach pins the ring cone section; a Runic Colossus Smash over a healer on his
target and one behind him pins the colossus section; orb 1 with two camp walks held until the chain lands pins the
chain wait section. The shared v14 fixture shows every section reads empty elsewhere.
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
                      "7.0 s, 3 ticks 13k); deaths 0; steps 1 (0 onto the bunny's path ahead, 0 under a zone within "
                      "8 s, 0 across it), stepped 7.0 bot-s, out of reach 7.0 of them, in another slot's jump 0.0; "
                      "walk back p50 0.5 max 0.5 s, 0 ticks on the way", text)
        self.assertIn("all: ring melee in a zone 7.0 bot-s, stays p50 7.0 max 7.0 s, 3 ticks 13k, deaths 0; steps 1 "
                      "(0 onto the bunny's path ahead, 0 under a zone within 8 s, 0 across it), stepped 7.0 bot-s, "
                      "out of reach 7.0, in another slot's jump 0.0; walk back p50 0.5 max 0.5 s, 0 ticks on the way",
                      text)

    def test_stepped_in_another_slots_jump(self):
        records = blizzard_pull()
        records[0]["roster"].append({"g": MELEE_B, "n": "Edge", "r": "melee", "c": 4, "h": 0})
        for rec in records:
            if rec.get("e") == "snap":
                spot = (2127.0, -251.0) if rec["t"] <= 24000 else (2140.0, -265.0)
                rec["u"].append([MELEE_B, spot[0], spot[1], 419.6, 0, 100, 0, THORIM])
        records.append({"t": 1000, "e": "note", "g": MELEE_B, "k": "thorim.ringspot",
                        "txt": "slot 0 bearing 200 solved 200"})
        text = printed(thorim.show_blizzard, load(sorted(records, key=lambda rec: rec.get("t", 0))))
        self.assertIn("stepped 7.0 bot-s, out of reach 7.0 of them, in another slot's jump 4.0;", text)

    def test_step_points_on_the_bunny_path_and_under_a_zone(self):
        records = blizzard_pull()
        for rec in records:
            if rec.get("e") == "snap" and 20000 <= rec["t"] <= 25000:
                rec["u"][-1][1:3] = [2123.35, -235.35]
            if rec.get("by") == "thorim sif blizzard action":
                rec.update(x=2135.0, y=-234.0)
        records.append({"t": 22100, "e": "move", "g": MELEE, "k": "point", "ok": 1, "x": 2120.0, "y": -247.0,
                        "by": "thorim sif blizzard action"})
        text = printed(thorim.show_blizzard, load(sorted(records, key=lambda rec: rec.get("t", 0))))
        self.assertIn("steps 2 (1 onto the bunny's path ahead, 1 under a zone within 8 s, 0 across it)", text)

    def test_a_step_across_the_bunny_path(self):
        records = blizzard_pull()
        for rec in records:
            if rec.get("e") == "snap" and 20000 <= rec["t"] <= 25000:
                rec["u"][-1][1:3] = [2110.0, -245.77]
            if rec.get("by") == "thorim sif blizzard action":
                rec.update(x=2120.0, y=-225.0)
        text = printed(thorim.show_blizzard, load(records))
        self.assertIn("1 across it with the bunny p50 13.7 yd short of the crossing, 3 ticks before the next order",
                      text)

    def test_without_the_probe(self):
        records = [rec for rec in blizzard_pull() if rec.get("k") != "thorim.ringstep"]
        self.assertIn("deaths 0; no thorim.ringstep probe (a build before it)",
                      printed(thorim.show_blizzard, load(records)))

    def test_normal_mode(self):
        self.assertIn("no Sif Blizzard in phase 2", printed(thorim.show_blizzard, load(pull())))


MELEE_B = 8806


def chain_pull() -> list[dict]:
    records = pull()
    records[0]["roster"].append({"g": MELEE_B, "n": "Edge", "r": "melee", "c": 4, "h": 0})
    for rec in records:
        if rec.get("e") == "snap":
            rec["u"].append([MELEE_B, 2128.0, -258.0, 419.6, 0, 100, 0, THORIM])
    records += [
        {"t": 1000, "e": "note", "g": MELEE_B, "k": "thorim.ringspot", "txt": "slot 0 bearing 200 solved 200"},
        {"t": 26000, "e": "aura", "d": RANGED, "s": RANGED, "sp": 32182, "r": 0, "st": 1, "dur": 40000, "p": 1},
        {"t": 26000, "e": "aura", "d": MELEE_B, "s": RANGED, "sp": 32182, "r": 0, "st": 1, "dur": 40000, "p": 1},
        {"t": 28000, "e": "note", "g": MELEE_B, "k": "thorim.ringstep", "txt": "1"},
        {"t": 29000, "e": "dmg", "s": THORIM, "d": RANGED, "sp": 64390, "a": 4000, "ok": 0, "hp": 100.0},
        {"t": 29001, "e": "dmg", "s": THORIM, "d": MELEE_B, "sp": 64390, "a": 6000, "ok": 0, "hp": 100.0},
        {"t": 29002, "e": "dmg", "s": THORIM, "d": MELEE, "sp": 64390, "a": 9000, "ok": 0, "hp": 100.0},
    ]
    return sorted(records, key=lambda rec: rec.get("t", -1))


class Chains(unittest.TestCase):
    def setUp(self):
        self.text = printed(thorim.show_chains, load(chain_pull()))

    def test_each_cast_hop_by_hop(self):
        self.assertIn("+17.0 s: 3 hits, 0 dead; tank x2, 14.2 yd to slot 2 x1", self.text)
        self.assertIn("+28.0 s: 3 hits, 1 dead; camp x1, 9.9 yd (lust) to slot 0 stepped x1, 11.4 yd to slot 2 x1 "
                      "1 dead", self.text)

    def test_deaths_by_crossing(self):
        self.assertIn("all: casts 2, hits 6, deaths 1, 1 before the first tank death (after slot to slot 1; "
                      "through a stepped slot: slot to slot 1); crossings tank to slot 1, camp to slot 1, slot to slot "
                      "1; through a stepped slot: camp to slot 1, slot to slot 1", self.text)

    def test_walking_back_still_counts_as_stepped(self):
        records = chain_pull()
        records.append({"t": 28500, "e": "note", "g": MELEE_B, "k": "thorim.ringstep", "txt": "0"})
        text = printed(thorim.show_chains, load(sorted(records, key=lambda rec: rec.get("t", -1))))
        self.assertIn("to slot 0 stepped x1", text)

    def test_no_phase_2(self):
        records = [rec for rec in chain_pull() if rec.get("k") != "thorim.p2start"]
        self.assertIn("phase 2 never started", printed(thorim.show_chains, load(records)))


CASTER = 8807
ORB = (1 << 32) | 4


def shelter_pull() -> list[dict]:
    records = pull()
    records[0]["roster"].append({"g": CASTER, "n": "Flicker", "r": "ranged", "c": 8, "h": 0})
    for rec in records:
        if rec.get("e") == "snap":
            rec["u"].append([CASTER, 2140.0, -255.0, 419.7, 0, 100, 80, THORIM])
    records += [
        {"t": 1000, "e": "note", "g": RANGED, "k": "thorim.rangedslot", "txt": "5"},
        {"t": 1000, "e": "note", "g": CASTER, "k": "thorim.rangedslot", "txt": "4"},
        {"t": 5000, "e": "note", "g": RANGED, "k": "thorim.lightningorb", "txt": str(ORB)},
        {"t": 5100, "e": "note", "g": RANGED, "k": "thorim.shelter", "txt": "slot 5 orb 0"},
        {"t": 5100, "e": "move", "g": RANGED, "k": "point", "ok": 1, "x": 2125.0, "y": -250.0,
         "by": "thorim phase 2 positioning action"},
        {"t": 5200, "e": "note", "g": CASTER, "k": "thorim.shelter", "txt": "slot 4 orb 0 step"},
        {"t": 5200, "e": "move", "g": CASTER, "k": "point", "ok": 1, "x": 2140.0, "y": -255.0,
         "by": "thorim phase 2 positioning action"},
        {"t": 6000, "e": "aura", "d": RANGED, "s": THORIM, "sp": 62604, "r": 0, "st": 1, "dur": 4000, "p": 0},
        {"t": 10000, "e": "dmg", "s": THORIM, "d": RANGED, "sp": 62466, "a": 24000, "ok": 2000, "hp": 100.0},
        {"t": 10000, "e": "death", "g": RANGED, "killer": THORIM, "x": 2135.0, "y": -265.0, "z": 419.8,
         "blow": [THORIM, 24000], "rewind": [[10000, THORIM, 62466, 24000]]},
    ]
    return sorted(records, key=lambda rec: rec.get("t", -1))


class Shelters(unittest.TestCase):
    def test_a_slowed_walk_short_of_its_row_and_a_step_there(self):
        text = printed(thorim.show_shelters, load(shelter_pull()))
        self.assertIn("+4.0 s: walks 2 (steps 1), there 1, hit 1, dead 1; camp hit off a walk 0; Darter row 18.0 yd "
                      "short 18.0 slowed Volley dead", text)
        self.assertIn("all: walks 2 (steps 1), there 1, hit 1 (1 slowed), dead 1; camp hit off a walk 0, dead 0; "
                      "before the first tank death walks 2 (steps 1), hit 1, dead 1", text)

    def test_without_the_probe(self):
        self.assertIn("no thorim.lightningorb probe or no lit orb in phase 2",
                      printed(thorim.show_shelters, load(pull())))


def ring_cone_pull() -> list[dict]:
    records = chain_pull()
    records += [
        {"t": 5000, "e": "note", "g": RANGED, "k": "thorim.lightningorb", "txt": str(ORB)},
        {"t": 5200, "e": "note", "g": MELEE, "k": "thorim.ringconehold", "txt": "hold"},
        {"t": 6000, "e": "move", "g": MELEE_B, "k": "point", "ok": 1, "x": 2131.0, "y": -258.0,
         "by": "thorim sif blizzard action"},
        {"t": 9500, "e": "note", "g": MELEE, "k": "thorim.ringconehold", "txt": "none"},
        {"t": 10000, "e": "dmg", "s": THORIM, "d": MELEE_B, "sp": 62466, "a": 26000, "ok": 4000, "hp": 100.0},
        {"t": 10000, "e": "death", "g": MELEE_B, "killer": THORIM, "x": 2128.0, "y": -258.0, "z": 419.6,
         "blow": [THORIM, 26000], "rewind": [[10000, THORIM, 62466, 26000]]},
    ]
    return sorted(records, key=lambda rec: rec.get("t", -1))


class RingCone(unittest.TestCase):
    def test_a_walker_past_reach_dies_and_a_held_walk_stays_clear(self):
        text = printed(thorim.show_ringcone, load(ring_cone_pull()))
        self.assertIn("+4.0 s: ring hit 1, dead 1; held 1 (0 hit); Edge walking (thorim sif blizzard action), "
                      "past reach, dead", text)
        self.assertIn("all: ring hit 1 (blocked 0, walking 1, held 0, standing 0; past reach 1), dead 1; held walks "
                      "1, 0 hit; before the first tank death hit 1, dead 1", text)

    def test_without_the_hold_probe(self):
        records = [rec for rec in ring_cone_pull() if rec.get("k") != "thorim.ringconehold"]
        self.assertIn("dead 1; no thorim.ringconehold probe (a build before it)",
                      printed(thorim.show_ringcone, load(records)))


COLOSSUS = (1 << 32) | 3
COLOSSUS_AT = (2230.4, -385.4)
COLOSSUS_FACING = math.radians(255)
HEALER_B = 8806


def colossus_pull() -> list[dict]:
    roster = [{"g": TANK_A, "n": "Anvil", "r": "tank", "c": 1, "h": 0},
              {"g": HEALER, "n": "Mender", "r": "heal", "c": 5, "h": 0},
              {"g": HEALER_B, "n": "Soother", "r": "heal", "c": 11, "h": 0}]
    front = (COLOSSUS_AT[0] + 4.4 * math.cos(COLOSSUS_FACING), COLOSSUS_AT[1] + 4.4 * math.sin(COLOSSUS_FACING))
    behind = (COLOSSUS_AT[0] - 10.6 * math.cos(COLOSSUS_FACING), COLOSSUS_AT[1] - 10.6 * math.sin(COLOSSUS_FACING))
    records = [
        {"e": "hdr", "v": 14, "ts": 1795381228000, "map": 603, "inst": 4, "diff": 1, "boss": "thorim",
         "bin": 1795380000000, "cfg": {"cheats": "", "mapthreads": 6, "hardmode": {}, "iccbuffs": 0},
         "roster": roster},
        {"t": 0, "e": "unit", "g": THORIM, "en": 32865, "n": "Thorim", "b": 1},
        {"t": 0, "e": "unit", "g": COLOSSUS, "en": 32872, "n": "Runic Colossus"},
        {"t": 0, "e": "pull", "boss": "thorim", "src": "step"},
        {"t": 1000, "e": "move", "g": HEALER, "k": "point", "ok": 1, "x": front[0], "y": front[1],
         "by": "thorim gauntlet positioning action"},
        {"t": 1000, "e": "move", "g": HEALER_B, "k": "point", "ok": 1, "x": behind[0], "y": behind[1],
         "by": "thorim gauntlet positioning action"},
        {"t": 5000, "e": "cast", "s": COLOSSUS, "sp": 62339, "ct": 3000},
        {"t": 8000, "e": "dmg", "s": COLOSSUS, "d": TANK_A, "sp": 62339, "a": 9000, "ok": 0, "hp": 90.0},
        {"t": 8000, "e": "dmg", "s": COLOSSUS, "d": HEALER, "sp": 62339, "a": 26000, "ok": 3000, "hp": 100.0},
        {"t": 8000, "e": "death", "g": HEALER, "killer": COLOSSUS, "x": front[0], "y": front[1], "z": 412.1,
         "blow": [COLOSSUS, 26000], "rewind": [[8000, COLOSSUS, 62339, 26000]]},
    ]
    for t in range(2000, 9001, 1000):
        records.append({"t": t, "e": "snap", "u": [
            [COLOSSUS, COLOSSUS_AT[0], COLOSSUS_AT[1], 412.1, COLOSSUS_FACING, 80, 0, TANK_A],
            [TANK_A, front[0], front[1], 412.1, 0, 90, 0, COLOSSUS],
            [HEALER, front[0], front[1], 412.1, 0, 100 if t < 8000 else 0, 90, 0],
            [HEALER_B, behind[0], behind[1], 412.1, 0, 100, 90, 0]]})
    records.append({"t": 9000, "e": "end", "out": "wipe"})
    return sorted(records, key=lambda rec: rec.get("t", -1))


class Colossus(unittest.TestCase):
    def test_a_healer_on_his_target_dies_and_one_behind_him_stays_clear(self):
        text = printed(thorim.show_colossus, load(colossus_pull()))
        self.assertIn("0:05.000 on Anvil (tank): Mender 4.4 yd 0 deg, 0.0 yd off his target, dead (thorim gauntlet "
                      "positioning action); Soother 10.6 yd 180 deg, 15.0 yd off his target (thorim gauntlet "
                      "positioning action)", text)
        self.assertIn("all: casts 1, healers within 20 yd 2, within 6 yd of his target 1, hit 1, dead 1", text)

    def test_no_cast(self):
        self.assertIn("no Smash cast", printed(thorim.show_colossus, load(pull())))


THUNDER_ORB = (1 << 32) | 5


def chain_wait_pull() -> list[dict]:
    records = pull()
    records[0]["roster"].append({"g": CASTER, "n": "Flicker", "r": "ranged", "c": 8, "h": 0})
    for rec in records:
        if rec.get("e") == "snap":
            rec["u"].append([CASTER, 2132.5, -257.5, 419.8, 0, 100, 80, THORIM])
            rec["u"].append([THUNDER_ORB, 2164.2, -233.47, 433.3, 0, 100, 0, 0])
    records += [
        {"t": 0, "e": "unit", "g": THUNDER_ORB, "en": 33378, "n": "Thunder Orb"},
        {"t": 1000, "e": "note", "g": RANGED, "k": "thorim.rangedslot", "txt": "4"},
        {"t": 1000, "e": "note", "g": CASTER, "k": "thorim.rangedslot", "txt": "3"},
        {"t": 5000, "e": "note", "g": RANGED, "k": "thorim.lightningorb", "txt": str(THUNDER_ORB)},
        {"t": 5100, "e": "note", "g": CASTER, "k": "thorim.shelter", "txt": "slot 3 orb 1 wait"},
        {"t": 5100, "e": "note", "g": RANGED, "k": "thorim.shelter", "txt": "slot 4 orb 1 wait"},
        {"t": 6000, "e": "dmg", "s": THORIM, "d": MELEE, "sp": 64390, "a": 4000, "ok": 0, "hp": 100.0},
        {"t": 6001, "e": "dmg", "s": THORIM, "d": CASTER, "sp": 64390, "a": 6000, "ok": 0, "hp": 100.0},
        {"t": 6200, "e": "note", "g": CASTER, "k": "thorim.shelter", "txt": "slot 3 orb 1"},
        {"t": 6200, "e": "note", "g": RANGED, "k": "thorim.shelter", "txt": "slot 4 orb 1 step"},
        {"t": 9900, "e": "dmg", "s": THORIM, "d": RANGED, "sp": 62466, "a": 18000, "ok": 0, "hp": 100.0},
    ]
    return sorted(records, key=lambda rec: rec.get("t", -1))


class ChainWait(unittest.TestCase):
    def test_two_waits_released_by_the_landing(self):
        text = printed(thorim.show_chainwait, load(chain_wait_pull()))
        self.assertIn("+4.0 s orb 1: chain camp hops 2, 0 dead; waits 2 (released by the landing 2, by the cap 0); "
                      "waiters hit by the chain standing 1 / walking 0, dead 0; by the cone 1, dead 0; steps 1", text)
        self.assertIn("all: orbs 1-2 lit 1, chains with 4+ camp hits 0, camp chain deaths 0 (0 before the first tank "
                      "death); waits 2 (released by the landing 2, by the cap 0), wait p50 1.1 max 1.1 s; waiters hit "
                      "by the chain standing 1 / walking 0, dead 0; by the cone 1, dead 0 (0 before the first tank "
                      "death); steps after a wait 1", text)

    def test_an_orb_that_holds_no_walk_is_left_out(self):
        records = chain_wait_pull()
        for rec in records:
            if rec.get("e") == "snap":
                rec["u"][-1][1:3] = [2145.5, -222.62]
        text = printed(thorim.show_chainwait, load(records))
        self.assertIn("all: orbs 1-2 lit 0", text)

    def test_without_the_probe(self):
        self.assertIn("no thorim.lightningorb probe or no lit orb in phase 2",
                      printed(thorim.show_chainwait, load(pull())))


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

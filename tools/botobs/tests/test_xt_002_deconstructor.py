"""Tests for the XT-002 reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull held at the tank spot pins the banner, the Heart windows, a west pile spawn and the
trash pack its Pummeller calls in. The shared v14 fixture shows every section reads empty elsewhere.
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

from bosses import xt_002_deconstructor as xt  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

TANK = 7701
HEALER = 7702
XT = (1 << 32) | 1
HEART = (1 << 32) | 2
PUMMELLER = (1 << 32) | 3
SALVAGEBOT = (1 << 32) | 4


def snap(t: int, pummeller_target: int = 0, trash_target: int = 0, heart_hp: float = 100.0) -> dict:
    return {"t": t, "e": "snap", "u": [
        [TANK, 895.0, -12.0, 409.7, 0, 100, 100, XT],
        [HEALER, 866.0, -6.0, 409.8, 0, 100, 100, 0],
        [XT, 887.0, -12.0, 409.6, 0, 80, 0, TANK],
        [HEART, 887.0, -12.0, 409.6, 0, heart_hp, 0, 0],
        [PUMMELLER, 801.0, -96.0, 409.9, 0, 100, 0, pummeller_target],
        [SALVAGEBOT, 807.0, -102.0, 409.9, 0, 100, 0, trash_target],
    ]}


def pull(heartbreak: bool = False) -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1795280000000, "map": 603, "inst": 2, "diff": 1,
         "boss": "xt-002-deconstructor", "bin": 1795279000000,
         "cfg": {"cheats": "food,taxi,raid", "mapthreads": 6, "hardmode": {"xt-002": 1}, "iccbuffs": 0},
         "roster": [{"g": TANK, "n": "Bulwark", "r": "tank", "c": 1, "h": 0},
                    {"g": HEALER, "n": "Mender", "r": "heal", "c": 5, "h": 0}]},
        {"t": 0, "e": "unit", "g": XT, "en": 33293, "n": "XT-002 Deconstructor", "b": 1},
        {"t": 0, "e": "unit", "g": HEART, "en": 33329, "n": "Heart of the Deconstructor"},
        {"t": 0, "e": "unit", "g": PUMMELLER, "en": 33344, "n": "XM-024 Pummeller"},
        {"t": 0, "e": "unit", "g": SALVAGEBOT, "en": 34269, "n": "XR-949 Salvagebot"},
        {"t": 0, "e": "pull", "boss": "xt-002-deconstructor", "src": "engage"},
        {"t": 76000, "e": "cast", "s": XT, "sp": 37751},
        snap(31000),
        snap(80000, heart_hp=40.0),
        snap(81000, pummeller_target=HEALER, heart_hp=20.0),
        snap(83200, pummeller_target=HEALER, trash_target=HEALER, heart_hp=20.0),
        {"t": 90000, "e": "death", "g": HEALER, "killer": SALVAGEBOT, "x": 866.0, "y": -6.0, "z": 409.8,
         "rewind": [[89900, SALVAGEBOT, 0, 7000]]},
        {"t": 120000, "e": "end", "out": "wipe"},
    ]
    if heartbreak:
        records.insert(-1, {"t": 100000, "e": "cast", "s": XT, "sp": 64193})
    return records


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / "603_2_xt-002-deconstructor_1795280000.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


class Banner(unittest.TestCase):
    def test_outcome_and_spot(self):
        text = printed(xt.show_banner, load(pull()))
        self.assertIn("outcome wipe, no Heartbreak, normal mode", text)
        self.assertIn("XT at (887, -12) from 0:30", text)

    def test_heartbreak_is_named(self):
        self.assertIn("Heartbreak at 1:40.000", printed(xt.show_banner, load(pull(heartbreak=True))))


class Heart(unittest.TestCase):
    def test_window_reports_the_lowest_hp(self):
        self.assertEqual(printed(xt.show_heart, load(pull())), (
            "HEART\n"
            "  window at 1:16.000  lowest 20%  XT at (887, -12), piles NE 80 SE 78 SW 125 NW 122 yd\n"
            "  Heart survived every window\n"))

    def test_window_shuts_on_the_stand(self):
        records = pull()
        records.insert(-1, {"t": 106000, "e": "cast", "s": XT, "sp": 37752})
        records.insert(-1, snap(110000, heart_hp=5.0))
        self.assertIn("lowest 20%", printed(xt.show_heart, load(records)))

    def test_hunters_are_set_against_the_ranged_median(self):
        hunter, mage = 7703, 7704
        records = pull()
        records[0]["roster"] += [{"g": hunter, "n": "Fletcher", "r": "ranged", "c": 3, "h": 0},
                                 {"g": mage, "n": "Sparks", "r": "ranged", "c": 8, "h": 0}]
        for rec in records:
            if rec.get("e") == "snap":
                dealt = max(rec["t"] - 76000, 0)
                rec["u"] += [[hunter, 850.0, -9.0, 409.8, 0, 100, 100, HEART, 0, 0, 0, dealt // 10],
                             [mage, 866.0, -6.0, 409.8, 0, 100, 100, HEART, 0, 0, 0, dealt * 10]]
        self.assertIn("hunters on it 1k against a ranged median 72k", printed(xt.show_heart, load(records)))


class Trash(unittest.TestCase):
    def test_pack_pull_is_tied_to_the_pummeller(self):
        self.assertEqual(printed(xt.show_trash, load(pull())), (
            "SCRAPYARD TRASH\n"
            "  1:23.200  1 at the SW pile on Mender, after a SW Pummeller engaged at 1:21.000 8.5 yd from the pack\n"
            "  deaths to trash: 1\n"))

    def test_adds_are_filed_by_pile(self):
        self.assertIn("SW pile from 0:31.000: 1 Pummeller", printed(xt.show_adds, load(pull())))

    def test_deaths_by_mechanic(self):
        self.assertIn("1  Scrapyard trash", printed(xt.show_deaths, load(pull())))

    def test_unnamed_xt_spell_is_not_melee(self):
        records = pull()
        records[-2] = {"t": 90000, "e": "death", "g": HEALER, "killer": XT, "x": 866.0, "y": -6.0,
                       "z": 409.8, "rewind": [[89900, XT, 62776, 9000]]}
        self.assertIn("1  spell 62776", printed(xt.show_deaths, load(records)))


class Sparks(unittest.TestCase):
    STUCK = (1 << 32) | 5
    QUICK = (1 << 32) | 6

    def spark_pull(self, evading: bool, released: bool = False) -> list[dict]:
        records = pull(heartbreak=True)
        end = records.pop()
        records += [
            {"t": 110000, "e": "unit", "g": self.STUCK, "en": 34004, "n": "Life Spark"},
            {"t": 110000, "e": "unit", "g": self.QUICK, "en": 34004, "n": "Life Spark"},
            {"t": 109990, "e": "aura", "d": TANK, "sp": 65121, "r": 1},
            {"t": 119990, "e": "aura", "d": HEALER, "sp": 65121, "r": 1},
        ]
        for t in range(110000, 160001, 5000):
            back = released and t >= 145000
            rows = [[TANK, 898.0, -4.0, 410.1, 0, 100, 100, self.STUCK if back else XT],
                    [HEALER, 866.0, -6.0, 409.8, 0, 100, 100, self.STUCK if t < 140000 or back else XT],
                    [self.STUCK, 896.0, -5.0, 409.6, 0, 100, 0, HEALER]]
            if 120000 <= t <= 130000:
                rows.append([self.QUICK, 846.0, -22.0, 409.6, 0, 50, 0, HEALER])
            records.append({"t": t, "e": "snap", "u": rows})
        if evading:
            records.append({"t": 112000, "e": "note", "g": HEALER, "k": "xt002.evading", "txt": str(self.STUCK)})
        if released:
            records.append({"t": 141000, "e": "note", "g": HEALER, "k": "xt002.evading", "txt": "none"})
        records.append({**end, "t": 160000})
        return records

    def test_long_spark_names_its_carrier(self):
        self.assertEqual(printed(xt.show_sparks, load(self.spark_pull(evading=False))), (
            "LIFE SPARKS\n"
            "  2 sparks, at most 2 up at once, median life 30 s\n"
            "  1:50.000 on Bulwark (tank) at (896, -5): lived 50 s, still up at the end, most bots on it at once 1\n"
            "  tank carrier Bulwark at 1:50.000: spark 53 yd off the Searing Light spot, lived 50 s\n"))

    def test_evade_note_shows_who_kept_hitting_it(self):
        self.assertIn("    evading 1:52.000 on (bots noting it 1), most bots still on it 2 s in 1\n",
                      printed(xt.show_sparks, load(self.spark_pull(evading=True))))

    def test_bots_back_on_a_released_spark_are_not_counted(self):
        self.assertIn("    evading 1:52.000 to 2:21.000 (bots noting it 1), most bots still on it 2 s in 1\n",
                      printed(xt.show_sparks, load(self.spark_pull(evading=True, released=True))))


class Tanks(unittest.TestCase):
    OFFTANK = 7705

    def steal_pull(self) -> list[dict]:
        records = pull()
        end = records.pop()
        records = [rec for rec in records if rec.get("e") != "snap"]
        records[0]["roster"].append({"g": self.OFFTANK, "n": "Stonewall", "r": "tank", "c": 11, "h": 0})

        def at(t: int, victim: int, offtank: tuple[float, float]) -> dict:
            return {"t": t, "e": "snap", "u": [
                [TANK, 870.0, -20.0, 409.7, 0, 100, 100, XT],
                [self.OFFTANK, offtank[0], offtank[1], 409.7, 0, 100, 100, XT],
                [XT, 860.0, -25.0, 409.6, 0, 80, 0, victim]]}

        records += [
            at(1000, TANK, (850.0, -30.0)),
            at(3000, self.OFFTANK, (850.0, -30.0)),
            at(20000, self.OFFTANK, (895.8, -12.5)),
            at(25000, self.OFFTANK, (896.0, -12.0)),
            at(40000, 0, (896.0, -12.0)),
            at(45000, 0, (896.0, -12.0)),
            {"t": 3100, "e": "note", "g": self.OFFTANK, "k": "xt002.tank", "txt": "holds XT"},
            {"t": 3100, "e": "note", "g": TANK, "k": "xt002.tank", "txt": "pummellers"},
            {"t": 5000, "e": "cast", "s": TANK, "sp": 31789, "tgt": self.OFFTANK},
            {"t": 6000, "e": "cast", "s": self.OFFTANK, "sp": 6795, "tgt": PUMMELLER},
            {"t": 7000, "e": "cast", "s": TANK, "sp": 62124, "tgt": XT},
            {**end, "t": 50000},
        ]
        return records

    def test_holders_duties_and_taunts_at_xt(self):
        self.assertEqual(printed(xt.show_tanks, load(self.steal_pull())), (
            "TANKS\n"
            "  0:01.000  Bulwark (tank) for 2.0 s\n"
            "  0:03.000  Stonewall (tank) for 37.0 s, 0 yd off the tank spot from 15 s in\n"
            "  0:40.000  nobody for 5.0 s\n"
            "  XT on a non-tank: never\n"
            "  Stonewall: holds XT 0:03.100\n"
            "  Bulwark: pummellers 0:03.100\n"
            "  taunts at XT: 2 (Bulwark spell 31789 1, Bulwark spell 62124 1)\n"))

    def test_steals_and_threat_brake_are_summed(self):
        rogue, mage = 7706, 7707
        records = self.steal_pull()
        end = records.pop()
        records[0]["roster"] += [{"g": rogue, "n": "Shiv", "r": "melee", "c": 4, "h": 0},
                                 {"g": mage, "n": "Sparks", "r": "ranged", "c": 8, "h": 0}]
        records += [
            {"t": 2000, "e": "snap", "u": [[XT, 860.0, -25.0, 409.6, 0, 80, 0, rogue]]},
            {"t": 2500, "e": "note", "g": rogue, "k": "xt002.brake", "txt": "1"},
            {"t": 2600, "e": "note", "g": mage, "k": "xt002.brake", "txt": "1"},
            # a container restating its value when the trace opens is no new hold
            {"t": 2700, "e": "note", "g": mage, "k": "xt002.brake", "txt": "1"},
            {"t": 9500, "e": "note", "g": rogue, "k": "xt002.brake", "txt": "0"},
            {"t": 4600, "e": "note", "g": mage, "k": "xt002.brake", "txt": "0"},
            {"t": 30000, "e": "note", "g": mage, "k": "xt002.brake", "txt": "1"},
            end,
        ]
        text = printed(xt.show_tanks, load(records))
        self.assertIn("  0:02.000  Shiv (melee) for 1.0 s\n", text)
        self.assertIn("  XT on a non-tank: 1.0 s over 1 runs\n", text)
        self.assertIn("  threat brake: 2 dps bots held, 29 s in all, longest 20.0 s (Sparks from 0:30.000)\n", text)

    def test_healer_buffs_are_measured_on_the_holder(self):
        records = self.steal_pull()
        end = records.pop()
        records += [
            {"t": 2000, "e": "note", "k": "xt002.buffs", "txt": str(TANK)},
            {"t": 8000, "e": "note", "k": "xt002.buffs", "txt": str(self.OFFTANK)},
            {"t": 500, "e": "aura", "d": TANK, "s": HEALER, "sp": 53563, "r": 0},
            {"t": 10000, "e": "aura", "d": TANK, "s": HEALER, "sp": 53563, "r": 1},
            {"t": 10000, "e": "aura", "d": self.OFFTANK, "s": HEALER, "sp": 53563, "r": 0},
            {"t": 8000, "e": "aura", "d": self.OFFTANK, "s": HEALER, "sp": 49284, "r": 0},
            # the Sacred Shield absorb proc and a tank's own shield are not healer tank buffs
            {"t": 12000, "e": "aura", "d": self.OFFTANK, "s": HEALER, "sp": 58597, "r": 0},
            {"t": 12000, "e": "aura", "d": TANK, "s": TANK, "sp": 53601, "r": 0},
            end,
        ]
        text = printed(xt.show_tanks, load(records))
        self.assertIn("  healer buff tank: Bulwark 0:02.000, Stonewall 0:08.000\n", text)
        self.assertIn("  tank buffs over 37 s of holds of 15 s or more, % on the holder/the other tank:"
                      " Beacon of Light 81/19%, Earth Shield 86/0%\n", text)


class Bombs(unittest.TestCase):
    CARRIER = 7708

    def bomb_pull(self, sent: bool) -> list[dict]:
        records = [rec for rec in pull(heartbreak=True) if rec.get("e") not in ("snap", "death", "end")]
        records[0]["roster"].append({"g": self.CARRIER, "n": "Cleaver", "r": "melee", "c": 1, "h": 0})
        records += [
            {"t": 150000, "e": "note", "g": HEALER, "k": "xt002.slot", "txt": "0/14 866,-6,410"},
            {"t": 200000, "e": "aura", "d": self.CARRIER, "s": XT, "sp": 64234, "r": 0},
            {"t": 209000, "e": "aura", "d": self.CARRIER, "s": XT, "sp": 64234, "r": 1},
        ]
        healer_spots = {200000: (866.0, -6.0), 203000: (866.0, -6.0), 205000: (874.0, -20.0),
                        208000: (874.0, -20.0), 211000: (870.0, -12.0), 213000: (866.0, -6.5)}
        carrier_spots = {200000: (880.0, -20.0), 203000: (893.0, -48.0), 205000: (893.0, -48.0),
                         208000: (893.0, -48.0), 211000: (893.0, -48.0), 213000: (880.0, -20.0)}
        for t, carrier in carrier_spots.items():
            healer = healer_spots[t] if sent else (866.0, -6.0)
            records.append({"t": t, "e": "snap", "u": [
                [HEALER, healer[0], healer[1], 409.8, 0, 100, 100, 0],
                [self.CARRIER, carrier[0], carrier[1], 409.8, 0, 50, 0, XT]]})
        if sent:
            records += [
                {"t": 203100, "e": "note", "g": self.CARRIER, "k": "xt002.bombheal", "txt": str(HEALER)},
                {"t": 206000, "e": "heal", "s": HEALER, "d": self.CARRIER, "sp": 48071, "a": 9000, "oh": 0},
                {"t": 207500, "e": "heal", "s": HEALER, "d": self.CARRIER, "sp": 48071, "a": 9000, "oh": 2500},
                # the carrier's own heal and the sent healer's heal on someone else don't count
                {"t": 207000, "e": "heal", "s": self.CARRIER, "d": self.CARRIER, "sp": 48071, "a": 900, "oh": 0},
                {"t": 207200, "e": "heal", "s": HEALER, "d": TANK, "sp": 48071, "a": 9000, "oh": 0},
                {"t": 209000, "e": "abs", "d": self.CARRIER, "s": HEALER, "sp": 48066, "a": 4200},
                {"t": 209000, "e": "dmg", "s": XT, "d": self.CARRIER, "sp": 64234, "a": 10300, "ab": 4200,
                 "hp": 93.5},
                {"t": 209050, "e": "note", "g": self.CARRIER, "k": "xt002.bombheal", "txt": "0"},
            ]
        else:
            records.append({"t": 209500, "e": "death", "g": self.CARRIER, "killer": XT, "x": 893.0, "y": -48.0,
                            "z": 409.8, "rewind": [[209000, XT, 64234, 14550]]})
        records.append({"t": 220000, "e": "end", "out": "kill"})
        return records

    def test_sent_healer_is_measured_on_its_carrier(self):
        self.assertEqual(printed(xt.show_bombs, load(self.bomb_pull(sent=True))), (
            "GRAVITY BOMBS\n"
            "  1 bombs, 1 carriers out of every healer's 38.5 yd, 1 at or under 90% before the hit,"
            " 0 killed by their own\n"
            "  3:20.000 Cleaver (melee) out of range from 3:23.000, Mender sent 3:23.100, closest 34 yd,"
            " 34 yd at expiry, back on its slot 4 s after, nearest healer 34 yd at expiry,"
            " carrier 94% when the bomb hit (10.3k, 4.2k absorbed)\n"
            "    Mender in range 3:25.000 with the carrier at 50%, 2 heals on it for 15.5k (2.5k over),"
            " first 3:26.000, its shields absorbed 4.2k\n"
            "  sent healers: 1 carriers, at the hit median 94%, lowest 94%, 1 of 1 above 90%, 0 killed\n"
            "  after Heartbreak 1 bombs hit 0 other raiders, 0 dead within 5 s; melee carriers' 1 hit 0\n"))

    def in_reach_pull(self, upgrade: bool) -> list[dict]:
        """The carrier stays beside the healer at 70%, so the latch is an in-reach one; with `upgrade` it then walks
        out of every healer's reach and the same healer goes after it."""
        records = [rec for rec in self.bomb_pull(sent=True)
                   if rec.get("e") != "snap" and rec.get("k") != "xt002.bombheal"]
        end = records.pop()
        far = (893.0, -48.0) if upgrade else (870.0, -10.0)
        for t, carrier in ((200000, (868.0, -8.0)), (203000, (870.0, -10.0)), (205000, far), (208000, far)):
            records.append({"t": t, "e": "snap", "u": [
                [HEALER, 866.0, -6.0, 409.8, 0, 100, 100, 0],
                [self.CARRIER, carrier[0], carrier[1], 409.8, 0, 70, 0, XT]]})
        records += [
            {"t": 201000, "e": "note", "g": self.CARRIER, "k": "xt002.bombnear", "txt": "1"},
            {"t": 201000, "e": "note", "g": self.CARRIER, "k": "xt002.bombheal", "txt": str(HEALER)},
        ]
        if upgrade:
            records.append({"t": 205100, "e": "note", "g": self.CARRIER, "k": "xt002.bombnear", "txt": "0"})
        records += [
            {"t": 209050, "e": "note", "g": self.CARRIER, "k": "xt002.bombheal", "txt": "0"},
            {"t": 209050, "e": "note", "g": self.CARRIER, "k": "xt002.bombnear", "txt": "0"},
            end,
        ]
        return records

    def test_in_reach_healer_heals_from_where_it_stands(self):
        text = printed(xt.show_bombs, load(self.in_reach_pull(upgrade=False)))
        self.assertIn("  1 bombs, 0 carriers out of every healer's 38.5 yd, 1 at or under 90% before the hit", text)
        self.assertIn("  3:20.000 Cleaver (melee), Mender in reach 3:21.000, closest 6 yd, 6 yd at expiry,"
                      " nearest healer 6 yd at expiry, carrier 94% when the bomb hit (10.3k, 4.2k absorbed)\n", text)
        self.assertIn("    Mender in range 3:23.000 with the carrier at 70%, 2 heals on it for 15.5k", text)
        self.assertIn("  healers in reach: 1 carriers, at the hit median 94%, lowest 94%, 1 of 1 above 90%, 0 killed\n",
                      text)
        self.assertNotIn("sent", text)

    def test_in_reach_latch_turns_into_a_send(self):
        text = printed(xt.show_bombs, load(self.in_reach_pull(upgrade=True)))
        self.assertIn("  3:20.000 Cleaver (melee) out of range from 3:25.000, Mender in reach 3:21.000, sent 3:25.100,"
                      " closest 50 yd, 50 yd at expiry, never back on its slot", text)
        self.assertIn("  sent healers: 1 carriers", text)
        self.assertNotIn("healers in reach:", text)

    def test_healer_carrier_heals_itself(self):
        records = self.bomb_pull(sent=False)
        records[0]["roster"][-1]["r"] = "heal"
        for rec in records:
            if rec.get("e") == "death":
                rec["t"] = 260000
        records.insert(-1, {"t": 207000, "e": "heal", "s": self.CARRIER, "d": self.CARRIER, "sp": 48071, "a": 9000,
                            "oh": 1000})
        text = printed(xt.show_bombs, load(records))
        self.assertIn("  3:20.000 Cleaver (heal), a healer, nearest healer 0 yd at expiry, carrier 50% at expiry\n"
                      "    lowest 50% before the hit, 1 heals on it for 8.0k (1.0k over), first 3:27.000\n", text)
        self.assertIn("  healer carriers: 1, no bomb hit read\n", text)

    def test_a_real_healer_carrier_reads_its_bot_healer(self):
        records = self.in_reach_pull(upgrade=False)
        records[0]["roster"][-1].update(r="heal", h=1)
        text = printed(xt.show_bombs, load(records))
        self.assertIn("  3:20.000 Cleaver (heal), Mender in reach 3:21.000, closest 6 yd, 6 yd at expiry,"
                      " nearest healer 0 yd at expiry, carrier 94% when the bomb hit (10.3k, 4.2k absorbed)\n", text)
        self.assertIn("    Mender in range 3:23.000 with the carrier at 70%, 2 heals on it for 15.5k", text)
        self.assertIn("  healers in reach: 1 carriers", text)
        self.assertNotIn("healer carriers:", text)

    def test_a_healer_carrier_never_under_the_ceiling_is_left_out(self):
        records = self.bomb_pull(sent=False)
        records[0]["roster"][-1]["r"] = "heal"
        records = [rec for rec in records if rec.get("e") != "death"]
        for rec in records:
            if rec.get("e") == "snap":
                rec["u"][1][5] = 100
        text = printed(xt.show_bombs, load(records))
        self.assertIn("0 at or under 90% before the hit", text)
        self.assertNotIn("Cleaver", text)

    def test_carrier_nobody_reached_is_killed_by_its_bomb(self):
        self.assertIn("  3:20.000 Cleaver (melee) out of range from 3:23.000, nobody sent, nearest healer 50 yd"
                      " at expiry, carrier 50% at expiry, killed\n",
                      printed(xt.show_bombs, load(self.bomb_pull(sent=False))))

    def test_healer_sent_after_the_last_snapshot_gets_no_distance(self):
        records = self.bomb_pull(sent=True)
        next(rec for rec in records if rec.get("k") == "xt002.bombheal")["t"] = 208500
        text = printed(xt.show_bombs, load(records))
        self.assertIn("  3:20.000 Cleaver (melee) out of range from 3:23.000, Mender sent 3:28.500,"
                      " back on its slot 4 s after, nearest healer 34 yd at expiry,"
                      " carrier 94% when the bomb hit (10.3k, 4.2k absorbed)\n", text)
        self.assertIn("    Mender never in range, 2 heals on it for 15.5k (2.5k over), first 3:26.000,"
                      " its shields absorbed 4.2k\n", text)

    def test_raiders_the_splash_hit_are_counted(self):
        bystander = 7709
        records = self.bomb_pull(sent=False)
        end = records.pop()
        records[0]["roster"].append({"g": bystander, "n": "Brawler", "r": "melee", "c": 1, "h": 0})
        records += [
            {"t": 209400, "e": "dmg", "s": XT, "d": bystander, "sp": 64233, "a": 14500, "hp": 60.0},
            {"t": 209400, "e": "dmg", "s": XT, "d": TANK, "sp": 64233, "a": 9000, "hp": 100.0},
            # the carrier's own splash row is its bomb, not a bystander hit
            {"t": 209400, "e": "dmg", "s": XT, "d": self.CARRIER, "sp": 64233, "a": 14500, "hp": 10.0},
            {"t": 211000, "e": "death", "g": bystander, "killer": XT, "x": 893.0, "y": -40.0, "z": 409.8,
             "rewind": [[209400, XT, 64233, 14500]]},
            end,
        ]
        self.assertIn("  after Heartbreak 1 bombs hit 2 other raiders, 1 dead within 5 s; melee carriers' 1 hit 2\n",
                      printed(xt.show_bombs, load(records)))

    def test_a_tank_carrier_counts_with_the_melee_lot(self):
        records = self.bomb_pull(sent=False)
        records[0]["roster"][-1]["r"] = "tank"
        self.assertIn("melee carriers' 1 hit 0\n", printed(xt.show_bombs, load(records)))

    def test_no_bystander_line_without_heartbreak(self):
        records = [rec for rec in self.bomb_pull(sent=False) if not (rec.get("e") == "cast" and rec.get("sp") == 64193)]
        self.assertNotIn("after Heartbreak", printed(xt.show_bombs, load(records)))

    def test_a_bomb_hit_far_from_the_expiry_is_not_read(self):
        records = self.bomb_pull(sent=True)
        next(rec for rec in records if rec.get("e") == "dmg")["t"] = 204000
        text = printed(xt.show_bombs, load(records))
        self.assertIn("nearest healer 34 yd at expiry, carrier 50% at expiry\n", text)
        self.assertIn("  sent healers: 1 carriers, no bomb hit read\n", text)


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
            xt.show_banner(trace)
            for _, _, section in xt.SECTIONS:
                section(trace)
        self.assertIn("not an XT-002 pull", out.getvalue())


if __name__ == "__main__":
    unittest.main()

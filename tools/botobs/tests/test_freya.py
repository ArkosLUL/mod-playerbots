"""Tests for the Freya reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins a trio wave where the add tank's Challenging Roar puts Freya on it, the main
tank's Righteous Defense takes the Snaplasher, and the add tank dies (bare, or under an own button and
externals that overlap once), after which the Snaplasher runs
loose on a healer who kites it, then a ranged bot, while two hits keep its bark up, plus three beam
dodges (one off a beam the next snapshot first shows) and one healer in range. A second one pins a
lasher wave a ghoul holds for two snapshots, with one ranged bot out of reach on Freya. A third pins a
melee bot walking back into a low lasher's blast and dying to its Detonate. A fourth pins a low lasher
chasing a ranged bot for 9 s while another ranged bot in reach shoots a ghoul-held lasher and bails
from it once, and a fifth two low lashers, each shot by the other one's victim. The shared v14 fixture shows every section reads empty elsewhere.
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

from bosses import freya as fr  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

MAIN = 7701
ADD = 7702
CASTER = 7703
HEALER = 7704
FREYA = (1 << 32) | 1
SNAPLASHER = (1 << 32) | 2
STORM = (1 << 32) | 3
SPIRIT = (1 << 32) | 4
GIFT = (1 << 32) | 5
BEAM = (1 << 32) | 6
FRESH_BEAM = (1 << 32) | 7


def snap(t: int, freya_on: int, snap_on: int, gift: bool = True, trio: bool = True) -> dict:
    units = [
        [MAIN, 2360.0, -43.0, 425.3, 0, 100, 100, FREYA],
        [ADD, 2355.0, -40.0, 425.3, 0, 100, 100, SNAPLASHER],
        [CASTER, 2340.0, -60.0, 425.3, 0, 100, 100, GIFT],
        [HEALER, 2392.0, -40.0, 425.3, 0, 100, 100, 0],
        [FREYA, 2362.0, -43.0, 425.3, 0, 100, 0, freya_on],
        [BEAM, 2340.0, -55.0, 425.3, 0, 100, 0, 0],
    ]
    if t >= 12000:
        units.append([FRESH_BEAM, 2392.0, -40.0, 425.3, 0, 100, 0, 0])
    if trio:
        units += [[SNAPLASHER, 2356.0, -40.0, 425.3, 0, 80, 0, snap_on],
                  [STORM, 2358.0, -41.0, 425.3, 0, 80, 0, MAIN],
                  [SPIRIT, 2357.0, -38.0, 425.3, 0, 80, 0, ADD]]
    if gift:
        units.append([GIFT, 2345.0, -60.0, 425.3, 0, 60, 0, 0])
    return {"t": t, "e": "snap", "u": units}


def cast(t: int, caster: int, spell: int, target: int = 0) -> dict:
    return {"t": t, "e": "cast", "s": caster, "sp": spell, "tgt": target, "ct": 0}


def move(t: int, mover: int, x: float, y: float) -> dict:
    return {"t": t, "e": "move", "g": mover, "k": "point", "x": x, "y": y, "z": 425.3, "ok": 1, "by": fr.DODGE_ACTION}


def pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 14, "ts": 1795343672000, "map": 603, "inst": 38, "diff": 1, "boss": "elder-stonebark",
         "bin": 1795338959000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 6, "hardmode": {}, "iccbuffs": 0},
         "roster": [{"g": MAIN, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": ADD, "n": "Bruin", "r": "tank", "c": 11, "h": 0},
                    {"g": CASTER, "n": "Caster", "r": "ranged", "c": 8, "h": 0},
                    {"g": HEALER, "n": "Mender", "r": "heal", "c": 5, "h": 0}]},
        {"t": 0, "e": "unit", "g": FREYA, "en": 32906, "n": "Freya", "b": 1},
        {"t": 0, "e": "unit", "g": SNAPLASHER, "en": 32916, "n": "Snaplasher"},
        {"t": 0, "e": "unit", "g": STORM, "en": 32919, "n": "Storm Lasher"},
        {"t": 0, "e": "unit", "g": SPIRIT, "en": 33202, "n": "Ancient Water Spirit"},
        {"t": 0, "e": "unit", "g": GIFT, "en": 33228, "n": "Eonar's Gift"},
        {"t": 0, "e": "unit", "g": BEAM, "en": 33170, "n": "Sun Beam"},
        {"t": 0, "e": "unit", "g": FRESH_BEAM, "en": 33170, "n": "Sun Beam"},
        {"t": 0, "e": "pull", "boss": "elder-stonebark", "src": "engage"},
        cast(1000, FREYA, 62859),
    ]
    holders = [(10000, MAIN, ADD), (12000, MAIN, ADD), (14000, ADD, ADD), (16000, ADD, ADD), (16500, ADD, MAIN),
               (18000, MAIN, MAIN), (20000, MAIN, ADD), (24000, MAIN, ADD)]
    for t, freya_on, snap_on in holders:
        records.append(snap(t, freya_on, snap_on))
    records += [
        cast(13500, ADD, 5209),
        cast(16200, MAIN, 31789, ADD),
        cast(16200, MAIN, 31789, ADD) | {"tr": 1},
        move(12500, CASTER, 2330.0, -60.0),
        move(13000, CASTER, 2330.0, -60.0),
        move(15000, MAIN, 2364.0, -43.0),
        move(11000, HEALER, 2381.0, -40.0),
        {"t": 22000, "e": "death", "g": ADD, "killer": SNAPLASHER, "x": 2355.0, "y": -40.0, "z": 425.3,
         "rewind": [[21900, SNAPLASHER, 0, 43481]]},
        {"t": 25000, "e": "death", "g": CASTER, "killer": CASTER, "x": 2340.0, "y": -60.0, "z": 425.3,
         "blow": [CASTER, 4600], "rewind": [[24900, CASTER, 62438, 4600]]},
        snap(30000, MAIN, 0, gift=False, trio=False),
        {"t": 31000, "e": "end", "out": "wipe"},
    ]
    records += [
        cast(21000, SNAPLASHER, 64190) | {"tr": 1},
        cast(22500, SNAPLASHER, 64190) | {"tr": 1},
        cast(22900, SNAPLASHER, 64190) | {"tr": 1},
    ]
    return records


def loose() -> list[dict]:
    return [
        snap(22000, MAIN, HEALER),
        snap(23000, MAIN, CASTER),
        {"t": 22100, "e": "note", "g": HEALER, "k": fr.KITE_NOTE, "txt": "6.8 vs 7.0"},
        {"t": 22600, "e": "note", "g": HEALER, "k": fr.KITE_NOTE, "txt": "7.2 vs 7.0"},
        {"t": 22700, "e": "note", "g": HEALER, "k": fr.KITE_NOTE, "txt": ""},
        {"t": 22200, "e": "move", "g": HEALER, "k": "point", "x": 2400.0, "y": -40.0, "z": 425.3, "ok": 1,
         "by": fr.KITE_ACTION},
        {"t": 22800, "e": "death", "g": HEALER, "killer": SNAPLASHER, "x": 2392.0, "y": -40.0, "z": 425.3,
         "rewind": [[22700, SNAPLASHER, 0, 61000]]},
    ]


LASHERS = [(1 << 32) | 20, (1 << 32) | 21, (1 << 32) | 22]
GHOUL = (1 << 32) | 30
FAR = 7705
NEAR = 7706


def lasher_snap(t: int, held: bool) -> dict:
    focus_on = GHOUL if held else FAR
    units = [
        [FREYA, 2362.0, -43.0, 425.3, 0, 100, 0, MAIN],
        [LASHERS[0], 2340.0, -50.0, 425.3, 0, 90, 0, focus_on],
        [LASHERS[1], 2344.0, -50.0, 425.3, 0, 90, 0, GHOUL],
        [LASHERS[2], 2342.0, -46.0, 425.3, 0, 90, 0, GHOUL],
        [GHOUL, 2341.0, -49.0, 425.3, 0, 100, 0, LASHERS[0] if held else LASHERS[1]],
        [FAR, 2380.0, -50.0, 425.3, 0, 100, 100, FREYA],
        [NEAR, 2360.0, -50.0, 425.3, 0, 100, 100, LASHERS[1]],
    ]
    return {"t": t, "e": "snap", "u": units}


def lasher_pull() -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1795343672000, "map": 603, "inst": 38, "diff": 1, "boss": "elder-stonebark",
         "bin": 1795338959000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 6, "hardmode": {}, "iccbuffs": 0},
         "roster": [{"g": MAIN, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": FAR, "n": "Faraway", "r": "ranged", "c": 8, "h": 0},
                    {"g": NEAR, "n": "Nearby", "r": "ranged", "c": 3, "h": 0}]},
        {"t": 0, "e": "unit", "g": FREYA, "en": 32906, "n": "Freya", "b": 1},
        *({"t": 0, "e": "unit", "g": g, "en": 32918, "n": "Detonating Lasher"} for g in LASHERS),
        {"t": 0, "e": "unit", "g": GHOUL, "en": 24207, "n": "Army of the Dead Ghoul"},
        {"t": 0, "e": "pull", "boss": "elder-stonebark", "src": "engage"},
        lasher_snap(60000, True),
        lasher_snap(60500, True),
        lasher_snap(61000, False),
        {"t": 60600, "e": "move", "g": FAR, "k": "point", "x": 2368.0, "y": -50.0, "z": 425.3, "ok": 1,
         "by": fr.REACH_ACTION},
        {"t": 62000, "e": "end", "out": "wipe"},
    ]


MELEE = 7707


def blast_snap(t: int, low_hp: float, melee_x: float) -> dict:
    return {"t": t, "e": "snap", "u": [
        [FREYA, 2362.0, -43.0, 425.3, 0, 100, 0, MAIN],
        [LASHERS[0], 2340.0, -50.0, 425.3, 0, low_hp, 0, MELEE],
        [LASHERS[1], 2344.0, -50.0, 425.3, 0, 90, 0, MAIN],
        [MELEE, melee_x, -50.0, 425.3, 0, 100, 100, FREYA],
    ]}


def melee_move(t: int, x: float, by: str = "reach melee") -> dict:
    return {"t": t, "e": "move", "g": MELEE, "k": "point", "x": x, "y": -50.0, "z": 425.3, "ok": 1, "by": by}


def blast_pull() -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1795343672000, "map": 603, "inst": 38, "diff": 1, "boss": "elder-stonebark",
         "bin": 1795338959000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 6, "hardmode": {}, "iccbuffs": 0},
         "roster": [{"g": MAIN, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": MELEE, "n": "Blade", "r": "melee", "c": 4, "h": 0}]},
        {"t": 0, "e": "unit", "g": FREYA, "en": 32906, "n": "Freya", "b": 1},
        *({"t": 0, "e": "unit", "g": g, "en": 32918, "n": "Detonating Lasher"} for g in LASHERS[:2]),
        {"t": 0, "e": "pull", "boss": "elder-stonebark", "src": "engage"},
        blast_snap(60000, 90, 2350.0),
        blast_snap(60500, 10, 2350.0),
        blast_snap(61000, 10, 2360.0),
        melee_move(60600, 2352.0),
        melee_move(60700, 2358.0, "freya lasher about to blow"),
        melee_move(60800, 2370.0),
        {"t": 61200, "e": "dmg", "s": LASHERS[0], "d": MELEE, "sp": 62937, "a": 9000, "ok": 0, "sc": 8, "ab": 0,
         "rs": 0, "hp": 30.0},
        {"t": 61300, "e": "death", "g": MELEE, "killer": LASHERS[0], "x": 2352.0, "y": -50.0, "z": 425.3,
         "rewind": [[61200, LASHERS[0], 62937, 9000]]},
        {"t": 62000, "e": "end", "out": "wipe"},
    ]


def low_snap(t: int, low_hp: float, near_on: int) -> dict:
    return {"t": t, "e": "snap", "u": [
        [FREYA, 2362.0, -43.0, 425.3, 0, 100, 0, MAIN],
        [LASHERS[0], 2340.0, -50.0, 425.3, 0, low_hp, 0, FAR],
        [LASHERS[1], 2344.0, -60.0, 425.3, 0, 90, 0, GHOUL],
        [GHOUL, 2344.0, -61.0, 425.3, 0, 100, 0, LASHERS[1]],
        [FAR, 2343.0, -50.0, 425.3, 0, 100, 100, LASHERS[0]],
        [NEAR, 2352.0, -50.0, 425.3, 0, 100, 100, near_on],
    ]}


def low_pull() -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1795343672000, "map": 603, "inst": 38, "diff": 1, "boss": "elder-stonebark",
         "bin": 1795338959000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 6, "hardmode": {}, "iccbuffs": 0},
         "roster": [{"g": MAIN, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": FAR, "n": "Chased", "r": "ranged", "c": 8, "h": 0},
                    {"g": NEAR, "n": "Nearby", "r": "ranged", "c": 3, "h": 0}]},
        {"t": 0, "e": "unit", "g": FREYA, "en": 32906, "n": "Freya", "b": 1},
        *({"t": 0, "e": "unit", "g": g, "en": 32918, "n": "Detonating Lasher"} for g in LASHERS[:2]),
        {"t": 0, "e": "unit", "g": GHOUL, "en": 24207, "n": "Army of the Dead Ghoul"},
        {"t": 0, "e": "pull", "boss": "elder-stonebark", "src": "engage"},
        low_snap(60000, 10, LASHERS[1]),
        low_snap(64000, 10, LASHERS[1]),
        low_snap(69000, 8, LASHERS[0]),
        {"t": 64500, "e": "move", "g": NEAR, "k": "point", "x": 2360.0, "y": -50.0, "z": 425.3, "ok": 1,
         "by": fr.CAMP_ACTION},
        {"t": 70000, "e": "end", "out": "wipe"},
    ]


def two_low_snap(t: int) -> dict:
    return {"t": t, "e": "snap", "u": [
        [FREYA, 2362.0, -43.0, 425.3, 0, 100, 0, MAIN],
        [LASHERS[0], 2340.0, -50.0, 425.3, 0, 10, 0, FAR],
        [LASHERS[2], 2348.0, -50.0, 425.3, 0, 12, 0, NEAR],
        [FAR, 2343.0, -50.0, 425.3, 0, 100, 100, LASHERS[2]],
        [NEAR, 2352.0, -50.0, 425.3, 0, 100, 100, LASHERS[0]],
    ]}


def two_low_pull() -> list[dict]:
    header, freya = low_pull()[:2]
    return [
        header,
        freya,
        *({"t": 0, "e": "unit", "g": g, "en": 32918, "n": "Detonating Lasher"} for g in (LASHERS[0], LASHERS[2])),
        {"t": 0, "e": "pull", "boss": "elder-stonebark", "src": "engage"},
        two_low_snap(60000),
        two_low_snap(61000),
        {"t": 62000, "e": "end", "out": "wipe"},
    ]


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / "603_38_elder-stonebark_1795343672.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


class Banner(unittest.TestCase):
    def test_elders_from_their_empowers(self):
        self.assertIn("hard mode: 2 of 3 Elders (Stonebark, Brightleaf), from their empowers",
                      printed(fr.show_banner, load(pull())))

    def test_elders_from_their_channels_and_the_kill_chest(self):
        records = pull() + [cast(2, (1 << 32) | 40 + i, spell) | {"tr": 1}
                            for i, spell in enumerate((62483, 62484, 62485))] + [cast(30500, FREYA, 62952) | {"tr": 1}]
        text = printed(fr.show_banner, load(records))
        self.assertIn("3 of 3 Elders (Stonebark, Ironbranch, Brightleaf), from their essence channels", text)
        self.assertIn("kill chest 62952: 3 Elder(s)", text)


class Waves(unittest.TestCase):
    def test_trio_wave_and_a_gift_that_lived_to_heal(self):
        self.assertEqual(printed(fr.show_waves, load(pull())), (
            "WAVES\n"
            "  0:10.000 .. 0:24.000  trio        3 add(s), 1 raid death(s) meanwhile\n"
            "  0:10.000 Eonar's Gift 14.0s, lived to heal, 60% left\n"))


class Tanks(unittest.TestCase):
    def test_holders_and_area_taunts(self):
        self.assertEqual(printed(fr.show_tanks, load(pull())), (
            "TANKS\n"
            "  Freya on Bulwark/tank 84.8% of the pull\n"
            "    0:14.000 .. 0:16.500 on Bruin/tank\n"
            "  Snaplasher 0:10.000 .. 0:24.000: Bruin/tank 87%, Bulwark/tank 13%\n"
            "  Snaplasher 0:10.000 on Bruin/tank 12.0s: own 0.0s, external 0.0s, both 0.0s, none 12.0s; "
            "died 0:22.000 under nothing\n"
            "  0:13.500 Bruin Challenging Roar\n"
            "  0:16.200 Bulwark Righteous Defense on Bruin/tank\n"))

    def test_a_loose_snaplasher_its_deaths_bark_and_kite(self):
        self.assertIn("  0:22.000 .. 0:24.000 Snaplasher of 0:10.000 loose 2.0s, 80% to 80%, on Mender/heal, "
                      "Caster/ranged: 1 death(s) to it, 2 hit(s) on its bark, 1 kite order(s), slower than its "
                      "victim in 1 of 2 speed read(s)\n",
                      printed(fr.show_tanks, load(pull() + loose())))

    def test_a_loose_snaplasher_without_kite_rows(self):
        records = [rec for rec in pull() + loose() if rec.get("e") not in ("note", "move", "death")]
        self.assertIn("Snaplasher of 0:10.000 loose 2.0s, 80% to 80%, on Mender/heal, Caster/ranged: "
                      "0 death(s) to it, 2 hit(s) on its bark, 0 kite order(s)\n",
                      printed(fr.show_tanks, load(records)))

    def test_add_tank_cooldowns_under_the_snaplasher(self):
        notes = [{"t": 12000, "e": "note", "g": ADD, "k": "freya.adddef", "txt": "barkskin"},
                 {"t": 12100, "e": "note", "g": HEALER, "k": "freya.addext", "txt": "pain suppression"},
                 {"t": 12200, "e": "note", "g": ADD, "k": "freya.adddef", "txt": "covered"},
                 {"t": 12300, "e": "note", "g": HEALER, "k": "freya.addext", "txt": "none"}]
        text = printed(fr.show_tanks, load(pull() + notes))
        self.assertIn("  0:12.000 Bruin defensive under the Snaplasher: barkskin\n", text)
        self.assertIn("  0:12.100 Mender external on the Snaplasher's tank: pain suppression\n", text)
        self.assertNotIn("covered", text)
        self.assertNotIn(": none", text)

    def test_add_tank_cover_from_own_buttons_and_externals(self):
        def aura(t: int, spell: int, removed: int) -> dict:
            return {"t": t, "e": "aura", "d": ADD, "s": HEALER, "sp": spell, "r": removed, "st": 1, "dur": 0, "p": 1}

        auras = [aura(10000, 22812, 0), aura(14000, 22812, 1), aura(13000, 33206, 0), aura(17000, 33206, 1),
                 aura(21000, 47788, 0), aura(22000, 47788, 1), aura(23000, 6940, 0)]
        self.assertIn("  Snaplasher 0:10.000 on Bruin/tank 12.0s: own 4.0s, external 5.0s, both 1.0s, none 4.0s; "
                      "died 0:22.000 under Guardian Spirit\n", printed(fr.show_tanks, load(pull() + auras)))


class Beams(unittest.TestCase):
    def test_dodges_per_role_and_healers_on_the_holder(self):
        trigger = fr.dodge_trigger_yd()
        self.assertIsNotNone(trigger)
        self.assertEqual(printed(fr.show_beams, load(pull())), (
            "BEAMS\n"
            f"  heal      1 dodge(s), 0 from past {trigger:g} yd, median walk 11.0 yd, 0 ended farther from Freya\n"
            f"  ranged    1 dodge(s), 0 from past {trigger:g} yd, median walk 10.0 yd, 1 ended farther from Freya\n"
            f"  tank      1 dodge(s), 1 from past {trigger:g} yd, median walk 4.0 yd, 0 ended farther from Freya\n"
            "  Snaplasher 0:10.000 .. 0:24.000: healers within 38.5 yd of its holder median 1, under 2 for 100% of it\n"))


class Ghouls(unittest.TestCase):
    def test_ranged_out_of_reach_while_a_ghoul_holds_the_focus(self):
        self.assertEqual(printed(fr.show_ghouls, load(lasher_pull())), (
            "GHOULS\n"
            "  1:00.000 lashers, held 1:00.000 .. 1:00.500: of 4 ranged DPS samples, no lasher within 28.5 yd 50%, "
            "on Freya 50%, inside 18 yd of one 50%; 1 'freya reach held pack' order(s)\n"))

    def test_trio_pull_has_no_lasher_wave(self):
        self.assertEqual(printed(fr.show_ghouls, load(pull())), "GHOULS\n  ghouls never held the pack focus\n")


class Blasts(unittest.TestCase):
    def test_a_walk_back_into_a_low_lashers_blast(self):
        self.assertEqual(printed(fr.show_blasts, load(blast_pull())), (
            "BLASTS\n"
            "  1:00.000 lashers: melee took 1 Detonate hit(s), 1 death(s); 1 of 2 approach order(s) into a low "
            "lasher's blast; melee inside one 33% of samples\n"))

    def test_trio_pull_has_no_lasher_wave(self):
        self.assertEqual(printed(fr.show_blasts, load(pull())), "BLASTS\n  no lasher wave\n")


class Lows(unittest.TestCase):
    def test_a_low_lasher_left_chasing_the_back_line(self):
        self.assertEqual(printed(fr.show_lows, load(low_pull())), (
            "LOWS\n"
            "  1:00.000 lashers: 1 under the bail line, 1 of them 8s+ there (1 hitting a player); ranged DPS in reach "
            "of a low lasher hitting a player: 6 sample(s), 67% on one, 33% on another lasher; 1 back-line bail "
            "order(s) from one hitting the back line\n"))

    def test_shooting_the_other_of_two_low_lashers_counts_as_on_one(self):
        self.assertEqual(printed(fr.show_lows, load(two_low_pull())), (
            "LOWS\n"
            "  1:00.000 lashers: 2 under the bail line, 0 of them 8s+ there (0 hitting a player); ranged DPS in reach "
            "of a low lasher hitting a player: 4 sample(s), 100% on one, 0% on another lasher; 0 back-line bail "
            "order(s) from one hitting the back line\n"))

    def test_trio_pull_has_no_lasher_wave(self):
        self.assertEqual(printed(fr.show_lows, load(pull())), "LOWS\n  no lasher wave\n")


class Deaths(unittest.TestCase):
    def test_by_killer_and_own_spell(self):
        text = printed(fr.show_deaths, load(pull()))
        self.assertIn("1  Snaplasher melee", text)
        self.assertIn("1  own spell 62438", text)


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
            fr.show_banner(trace)
            for _, _, section in fr.SECTIONS:
                section(trace)
        self.assertIn("not a Freya pull", out.getvalue())


if __name__ == "__main__":
    unittest.main()

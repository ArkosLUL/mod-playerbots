#!/usr/bin/env python3
"""Build the playerbots_sim_weights migration from wowsim's stat weights (tools/statweights).

Reads the weights.json that tool writes and emits one SQL file that drops and recreates the table:
per DPS spec and content phase, each bot stat's DPS per point relative to the spec's anchor stat,
plus the average item level of that phase's BiS gear. Tank and healer builds are skipped.

  python generate_weights.py --weights "G:/DevStuff/GitHub/wowsimwotlk/tmp/statweights/weights.json"

How the weights are consumed is in docs/systems/itemization.md, under "Sim stat weights".
"""

import argparse
import datetime
import json
import os
import re
import sys

BS = chr(92)

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CORE_ROOT = os.path.abspath(os.path.join(REPO_ROOT, "..", ".."))

DEFAULT_ITEM_TEMPLATE = os.path.join(CORE_ROOT, "data", "sql", "base", "db_world", "item_template.sql")
DEFAULT_OUT_DIR = os.path.join(REPO_ROOT, "data", "sql", "playerbots", "updates")
OUT_SUFFIX = "_playerbots_sim_weights.sql"

# entry, class, subclass, SoundOverrideSubclass, name, displayid, Quality, Flags, FlagsExtra, BuyCount,
# BuyPrice, SellPrice, InventoryType, AllowableClass, AllowableRace, ItemLevel. item_template names
# escape quotes as both ' and '', so the name class has to exclude a literal backslash.
ITEM_RE = re.compile(
    r"\((\d+),\s*\d+,\s*\d+,\s*-?\d+,\s*'(?:[^'" + BS + BS + "]|" + BS + BS + r".|'')*',"
    r"\s*\d+,\s*(\d+),\s*\d+,\s*\d+,\s*\d+,\s*-?\d+,\s*\d+,\s*\d+,\s*-?\d+,\s*-?\d+,\s*(\d+)"
)
ITEM_QUALITY_HEIRLOOM = 7

# build key -> (class id, tab, anchor stat). Tabs are the bots' talent tabs; 13 is the Smite priest,
# which has none of its own (SimWeights::TAB_PRIEST_SMITE).
BUILDS = {
    "warrior_arms": (1, 0, "STRENGTH"),
    "warrior_fury": (1, 1, "STRENGTH"),
    "retribution_paladin": (2, 2, "STRENGTH"),
    "hunter_mm": (3, 1, "AGILITY"),
    "hunter_sv": (3, 2, "AGILITY"),
    "rogue_assassination": (4, 0, "AGILITY"),
    "rogue_combat": (4, 1, "AGILITY"),
    "rogue_subtlety": (4, 2, "AGILITY"),
    "shadow_priest": (5, 2, "SPELL_POWER"),
    "smite_priest": (5, 13, "SPELL_POWER"),
    "dk_blood": (6, 0, "STRENGTH"),
    "dk_frost": (6, 1, "STRENGTH"),
    "dk_unholy": (6, 2, "STRENGTH"),
    "elemental_shaman": (7, 0, "SPELL_POWER"),
    "enhancement_shaman": (7, 1, "AGILITY"),
    "mage_arcane": (8, 0, "SPELL_POWER"),
    "mage_fire": (8, 1, "SPELL_POWER"),
    "mage_frost": (8, 2, "SPELL_POWER"),
    "warlock_affliction": (9, 0, "SPELL_POWER"),
    "warlock_demonology": (9, 1, "SPELL_POWER"),
    "warlock_destruction": (9, 2, "SPELL_POWER"),
    "balance_druid": (11, 0, "SPELL_POWER"),
    "feral_druid": (11, 1, "AGILITY"),
}
# the sim's survival and threat depend on its healing model, so tanks keep the hand-written weights
SKIPPED = {"tank_dk_blood", "tank_dk_frost", "feral_tank_druid", "protection_paladin", "protection_warrior"}

# bot stat (StatsType without STATS_TYPE_) -> the sim stats it sums. A rating item gives both the
# melee and the spell half, and a hunter's attack power counts for both of its attack powers.
STAT_SOURCES = {
    "STRENGTH": ["StatStrength"],
    "AGILITY": ["StatAgility"],
    "INTELLECT": ["StatIntellect"],
    "SPIRIT": ["StatSpirit"],
    "SPELL_POWER": ["StatSpellPower"],
    "MANA_REGENERATION": ["StatMP5"],
    "ATTACK_POWER": ["StatAttackPower", "StatRangedAttackPower"],
    "HIT": ["StatMeleeHit", "StatSpellHit"],
    "CRIT": ["StatMeleeCrit", "StatSpellCrit"],
    "HASTE": ["StatMeleeHaste", "StatSpellHaste"],
    "ARMOR_PENETRATION": ["StatArmorPenetration"],
    "EXPERTISE": ["StatExpertise"],
    "MELEE_DPS": ["PseudoStatMainHandDps"],
    "RANGED_DPS": ["PseudoStatRangedDps"],
}

# Player::GetAverageItemLevelForDF skips the shirt, tabard, off-hand and ranged slots. In the sim's
# ItemSlot order that leaves Head through MainHand; the sim has no shirt or tabard slot.
COUNTED_SIM_SLOTS = range(0, 15)
COUNTED_SLOT_COUNT = 15

PHASES = [1, 2, 3, 4, 5]


def load_item_levels(path):
    """entry -> (ItemLevel, Quality) for every row in the item_template dump."""
    levels = {}
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line.startswith("("):
                continue
            for match in ITEM_RE.finditer(line):
                levels[int(match.group(1))] = (int(match.group(3)), int(match.group(2)))
    return levels


def gear_ilvl(items, levels, missing):
    total = 0
    for slot in COUNTED_SIM_SLOTS:
        item = items[slot] if slot < len(items) else 0
        if not item:
            continue
        if item not in levels:
            missing.add(item)
            continue
        ilvl, quality = levels[item]
        # heirlooms count as level * 2.33 there, and the sim is always level 80
        total += 80 * 2.33 if quality == ITEM_QUALITY_HEIRLOOM else ilvl
    return total / COUNTED_SLOT_COUNT


def bot_weights(slopes, anchor):
    """bot stat -> DPS per point over the anchor's, for the stats the sim measured."""
    raw = {}
    for stat, sources in STAT_SOURCES.items():
        measured = [slopes[s]["dps"] for s in sources if s in slopes]
        if measured:
            raw[stat] = sum(measured)
    if raw.get(anchor, 0.0) <= 0.0:
        raise ValueError("anchor {} has no positive slope".format(anchor))
    return {stat: value / raw[anchor] for stat, value in raw.items()}


def build_rows(data, levels, problems):
    rows = []
    missing = set()
    for build in data["builds"]:
        key = build["key"]
        if key in SKIPPED:
            continue
        if key not in BUILDS:
            problems.append("unmapped build {}".format(key))
            continue
        cls, tab, anchor = BUILDS[key]
        phases = {entry["phase"]: entry for entry in build["phases"]}
        for phase in PHASES:
            entry = phases.get(phase)
            if entry is None:
                problems.append("{} has no phase {}".format(key, phase))
                continue
            ilvl = gear_ilvl(entry["items"], levels, missing)
            try:
                weights = bot_weights(entry["slopes"], anchor)
            except ValueError as error:
                problems.append("{} P{}: {}".format(key, phase, error))
                continue
            for stat, weight in sorted(weights.items()):
                rows.append((cls, tab, phase, ilvl, stat, weight, 1 if stat == anchor else 0, key))
    if missing:
        problems.append("gear items absent from item_template: {}".format(", ".join(map(str, sorted(missing)))))
    return rows


HEADER = """-- Stat weights for level-80 DPS bots, measured by wowsim at each content phase's BiS gear.
-- Generated by apps/simweights/generate_weights.py from wowsim tools/statweights; edit the
-- generator, not this file.
--
-- sim commit {commit}, generated {generated}, {iterations} iterations, seed {seed}.
--
-- tab: talent tab 0-2, plus 13 = Smite priest (a non-Shadow priest that isn't healing).
-- phase: WotLK content phase 1..5 (T7..T10, RS).
-- gear_ilvl: average ItemLevel of that phase's BiS gear, counted like
--   Player::GetAverageItemLevelForDF. Bots blend the two phases around their own average.
-- stat: StatsType without STATS_TYPE_. Only stats the sim measured have a row.
-- weight: DPS per point over the anchor stat's; the anchor row (anchor = 1) is always 1.0.

DROP TABLE IF EXISTS `playerbots_sim_weights`;
CREATE TABLE `playerbots_sim_weights` (
    `class`     TINYINT UNSIGNED NOT NULL,
    `tab`       TINYINT UNSIGNED NOT NULL,
    `phase`     TINYINT UNSIGNED NOT NULL,
    `gear_ilvl` FLOAT NOT NULL,
    `stat`      VARCHAR(24) NOT NULL,
    `weight`    FLOAT NOT NULL,
    `anchor`    TINYINT UNSIGNED NOT NULL,
    `build`     VARCHAR(32) NOT NULL,
    PRIMARY KEY (`class`, `tab`, `phase`, `stat`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `playerbots_sim_weights` VALUES
"""


def sql_quote(text):
    return "'" + text.replace("'", "''") + "'"


def write_sql(rows, data, path):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(HEADER.format(commit=data.get("sim_commit", "unknown"), generated=data.get("generated_at", "unknown"),
                                   iterations=data.get("iterations", "?"), seed=data.get("seed", "?")))
        for index, (cls, tab, phase, ilvl, stat, weight, anchor, key) in enumerate(rows):
            handle.write("({}, {}, {}, {:.2f}, {}, {:.4f}, {}, {}){}\n".format(
                cls, tab, phase, ilvl, sql_quote(stat), weight, anchor, sql_quote(key),
                ";" if index == len(rows) - 1 else ","))


def default_out(stamp):
    """Reuses this generator's file for the date, else the date's first free _NN_ index: the module's
    DB updater refuses two update files with the same name."""
    taken = set()
    for name in os.listdir(DEFAULT_OUT_DIR):
        if name.startswith(stamp + "_"):
            if name.endswith(OUT_SUFFIX):
                return os.path.join(DEFAULT_OUT_DIR, name)
            taken.add(name[len(stamp) + 1:len(stamp) + 3])
    index = next(i for i in range(100) if "{:02d}".format(i) not in taken)
    return os.path.join(DEFAULT_OUT_DIR, "{}_{:02d}{}".format(stamp, index, OUT_SUFFIX))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--weights", required=True, help="weights.json from wowsim's tools/statweights")
    parser.add_argument("--item-template", default=DEFAULT_ITEM_TEMPLATE)
    parser.add_argument("--out", help="target .sql path (default: a dated file in the updates dir)")
    parser.add_argument("--date", help="YYYY_MM_DD for the default output name")
    args = parser.parse_args()

    with open(args.weights, encoding="utf-8") as handle:
        data = json.load(handle)

    print("reading item_template ...")
    levels = load_item_levels(args.item_template)
    print("  {} items".format(len(levels)))

    problems = []
    rows = build_rows(data, levels, problems)
    if problems:
        for problem in problems:
            print("error: " + problem, file=sys.stderr)
        return 1

    out = args.out or default_out(args.date or datetime.date.today().strftime("%Y_%m_%d"))
    write_sql(rows, data, out)
    specs = {(r[0], r[1]) for r in rows}
    print("wrote {} rows for {} specs to {}".format(len(rows), len(specs), out))
    return 0


if __name__ == "__main__":
    sys.exit(main())

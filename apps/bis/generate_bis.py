#!/usr/bin/env python3
"""Build the playerbots_bis_ranked migration from the Bistooltip addon's BiS list files.

Reads Bistooltip_{classic,tbc,wotlk}_bislists.lua and emits one SQL file that drops and recreates
the table. Item ids absent from item_template are skipped, so custom-realm ids never reach the DB.

  python generate_bis.py --addon-dir "A:/WOW/.../Interface/AddOns/Bistooltip"

How the ranks are consumed is in docs/systems/itemization.md, under "Ranked BiS lists".
"""

import argparse
import os
import re
import sys
from collections import defaultdict

BS = chr(92)

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CORE_ROOT = os.path.abspath(os.path.join(REPO_ROOT, "..", ".."))

DEFAULT_ITEM_TEMPLATE = os.path.join(CORE_ROOT, "data", "sql", "base", "db_world", "item_template.sql")
DEFAULT_OUT_DIR = os.path.join(REPO_ROOT, "data", "sql", "playerbots", "updates")

# (dir suffix, expansion id, lua global, phase labels in order)
EXPANSIONS = [
    ("classic", 0, "Bistooltip_classic_bislists", ["PR", "P1", "P2", "P3", "P4", "P5", "P6"]),
    ("tbc", 1, "Bistooltip_tbc_bislists", ["PR", "T4", "T5", "T6", "ZA", "SWP"]),
    ("wotlk", 2, "Bistooltip_wotlk_bislists", ["PR", "T7", "T8", "T9", "T10", "RS"]),
]

EXPANSION_NAMES = {0: "Vanilla", 1: "TBC", 2: "WotLK"}

CLASS_IDS = {
    "Warrior": 1,
    "Paladin": 2,
    "Hunter": 3,
    "Rogue": 4,
    "Priest": 5,
    "Death knight": 6,
    "Shaman": 7,
    "Mage": 8,
    "Warlock": 9,
    "Druid": 11,
}

# Talent tab per addon spec name. 10/11/12 are sentinels for role splits a tab cannot express.
SPEC_TABS = {
    "Warrior": {"Arms": 0, "Fury": 1, "Protection": 2, "Fury-Prot": 12},
    "Paladin": {"Holy": 0, "Protection": 1, "Retribution": 2},
    "Hunter": {"Beast mastery": 0, "Marksmanship": 1, "Survival": 2},
    "Rogue": {"Assassination": 0, "Combat": 1, "Subtlety": 2},
    "Priest": {"Discipline": 0, "Holy": 1, "Shadow": 2},
    "Death knight": {"Blood dps": 0, "Frost": 1, "Unholy": 2, "Blood tank": 11},
    "Shaman": {"Elemental": 0, "Enhancement": 1, "Restoration": 2},
    "Mage": {"Arcane": 0, "Fire": 1, "Fire FFB": 1, "Frost": 2},
    "Warlock": {"Affliction": 0, "Demonology": 1, "Destruction": 2},
    "Druid": {"Balance": 0, "Feral dps": 1, "Restoration": 2, "Feral tank": 10},
}

DUAL_SLOTS = {"Finger", "Trinket"}
# Fury is the only dual-wield spec the lists treat this way. Frost DK, Enhancement and Rogue also
# dual-wield but keep genuinely distinct main/off-hand lists, so their rank 2 stays a runner-up.
FURY_SLOTS = {"Weapon", "Off hand"}

ROW_RE = re.compile(
    r'^(\w+)' + r'\["([^"]+)"\]' + r'\["([^"]+)"\]' + r'\["([^"]+)"\]' +
    r'\[\d+\] = \{ \["slot_name"\] = "([^"]+)"'
)
RANK_RE = re.compile(r'\[(\d)\] = (-?\d+)')
TWIN_RE = re.compile(r'Bistooltip_horde_to_ali\[(\d+)\] = (\d+);')

# item_template names escape quotes as both ' and '', so the class has to exclude a literal
# backslash - which needs two of them in the pattern source.
ITEM_RE = re.compile(
    r"\((\d+),\s*\d+,\s*\d+,\s*-?\d+,\s*'((?:[^'" + BS + BS + "]|" + BS + BS + r".|'')*)'"
)

RANKS_MARKER = ", [1] = "


def unescape_sql(text):
    return text.replace(BS + "'", "'").replace("''", "'").replace(BS + BS, BS)


def load_item_names(path):
    """entry -> name for every row in the item_template dump."""
    names = {}
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line.startswith("("):
                continue
            for match in ITEM_RE.finditer(line):
                names[int(match.group(1))] = unescape_sql(match.group(2))
    return names


def load_twins(path):
    """Alliance/Horde drop pairs, both directions."""
    twins = defaultdict(set)
    with open(path, encoding="utf-8") as handle:
        for match in TWIN_RE.finditer(handle.read()):
            horde, ally = int(match.group(1)), int(match.group(2))
            twins[horde].add(ally)
            twins[ally].add(horde)
    return twins


def parse_bislists(path, expected_global):
    """(class, spec, phase, slot) -> {rank: item_id}, last assignment winning.

    The WotLK file assigns 461 keys twice - two complete pre-raid blocks merged by the addon's own
    generator. Lua takes the later one, so the import has to as well or an eighth of the pre-raid
    rows disagree with what players see in the tooltip.
    """
    rows = {}
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            match = ROW_RE.match(line.rstrip())
            if not match:
                continue
            global_name, cls, spec, phase, slot = match.groups()
            if global_name != expected_global:
                continue
            marker = line.find(RANKS_MARKER)
            if marker < 0:
                continue
            ranks = {}
            for rank, item in RANK_RE.findall(line[marker:]):
                item = int(item)
                if item > 0:
                    ranks[int(rank)] = item
            rows[(cls, spec, phase, slot)] = ranks
    return rows


def ranked_pairs(cls, spec, slot, ranks):
    """(rank, item_id) pairs, with the dual-slot partner promoted to rank 1.

    Both rings and both trinkets are worn at once, so the list's rank 2 is the intended partner
    rather than a runner-up; Fury dual-wields, so its two weapons are the same case. Ranks 3-6 keep
    their numbers - only the pair is equal.
    """
    dual = slot in DUAL_SLOTS or (cls == "Warrior" and spec == "Fury" and slot in FURY_SLOTS)
    return [(1 if (dual and rank == 2) else rank, item) for rank, item in sorted(ranks.items())]


class Report:
    def __init__(self):
        self.unmapped_specs = set()
        self.missing_items = defaultdict(set)
        self.per_expansion = defaultdict(int)
        self.specs_per_expansion = defaultdict(set)

    def dump(self, stream=sys.stdout):
        for exp_id in sorted(self.per_expansion):
            print("  {:<8} {:>6} rows, {:>2} spec keys".format(
                EXPANSION_NAMES[exp_id], self.per_expansion[exp_id],
                len(self.specs_per_expansion[exp_id])), file=stream)
        if self.unmapped_specs:
            print("  unmapped specs: " + ", ".join(sorted(self.unmapped_specs)), file=stream)
        for exp_id in sorted(self.missing_items):
            ids = sorted(self.missing_items[exp_id])
            print("  {} ids absent from item_template ({}): {}".format(
                EXPANSION_NAMES[exp_id], len(ids), ids[:20]), file=stream)


def build_rows(addon_dir, item_names, twins, report):
    """One tuple per DB row, deduplicated on the primary key keeping the best rank."""
    best = {}
    meta = {}

    for suffix, exp_id, global_name, phases in EXPANSIONS:
        path = os.path.join(addon_dir, "Bistooltip_{}_bislists.lua".format(suffix))
        if not os.path.exists(path):
            raise SystemExit("missing data file: " + path)

        phase_ids = {label: index for index, label in enumerate(phases)}
        parsed = parse_bislists(path, global_name)

        for (cls, spec, phase, slot), ranks in sorted(parsed.items()):
            class_id = CLASS_IDS.get(cls)
            tab = SPEC_TABS.get(cls, {}).get(spec)
            phase_id = phase_ids.get(phase)
            if class_id is None or tab is None or phase_id is None:
                report.unmapped_specs.add("{}/{}/{}".format(cls, spec, phase))
                continue

            for rank, item in ranked_pairs(cls, spec, slot, ranks):
                for item_id in [item] + sorted(twins.get(item, ())):
                    name = item_names.get(item_id)
                    if name is None:
                        report.missing_items[exp_id].add(item_id)
                        continue
                    key = (exp_id, class_id, tab, phase_id, slot, item_id)
                    if key not in best or rank < best[key]:
                        best[key] = rank
                        meta[key] = (cls, spec, phase, name)

    rows = []
    for key in sorted(best):
        exp_id, class_id, tab, phase_id, slot, item_id = key
        cls, spec, phase, name = meta[key]
        rows.append((exp_id, class_id, tab, phase_id, slot, item_id, best[key], cls, spec, phase, name))
        report.per_expansion[exp_id] += 1
        report.specs_per_expansion[exp_id].add((class_id, tab))
    return rows


HEADER = """-- Ranked BiS lists, imported from the Bistooltip addon (Vanilla, TBC and WotLK).
-- Answers "how much does this bot want this item", scoring loot rolls and equip decisions.
-- Sister table playerbots_bis_gear answers "dress this bot" and is keyed by ilvl instead;
-- the two are not interchangeable and neither can be generated from the other.
--
-- Generated by apps/bis/generate_bis.py - edit the generator, not this file.
--
-- expansion: 0=Vanilla 1=TBC 2=WotLK. Phase numbering restarts per expansion, and lookups only ever
--   match within the bot's own expansion, so the two columns are meaningless apart.
-- phase: Vanilla 0=PR 1..6=P1..P6; TBC 0=PR 1=T4 2=T5 3=T6 4=ZA 5=SWP;
--   WotLK 0=PR 1=T7 2=T8 3=T9 4=T10 5=RS.
-- tab: talent tab 0-2, plus sentinels 10 = Druid feral tank, 11 = DK blood tank,
--   12 = Warrior fury-prot (Vanilla only - a fury-specced tank).
-- bis_rank: 1 = best. Finger/Trinket (and Fury weapons) fill two slots, so their rank 2 is
--   stored as rank 1 - it is the intended pair, not a second choice.
-- bis_rank is deliberately NOT in the primary key: merging Mage Fire/Fire FFB and duplicating
--   faction twins both put two different items at the same rank in one slot.

DROP TABLE IF EXISTS `playerbots_bis_ranked`;
CREATE TABLE `playerbots_bis_ranked` (
    `expansion`  TINYINT UNSIGNED NOT NULL,
    `class`      TINYINT UNSIGNED NOT NULL,
    `tab`        TINYINT UNSIGNED NOT NULL,
    `phase`      TINYINT UNSIGNED NOT NULL,
    `slot_name`  VARCHAR(16) NOT NULL,
    `item_id`    INT UNSIGNED NOT NULL,
    `bis_rank`   TINYINT UNSIGNED NOT NULL,
    `class_name` VARCHAR(16) NOT NULL,
    `spec_name`  VARCHAR(32) NOT NULL,
    `phase_name` VARCHAR(8)  NOT NULL,
    `item_name`  VARCHAR(96) NOT NULL,
    PRIMARY KEY (`expansion`, `class`, `tab`, `phase`, `slot_name`, `item_id`),
    KEY `idx_item` (`item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `playerbots_bis_ranked` VALUES
"""


def sql_quote(text):
    return "'" + text.replace("'", "''") + "'"


def write_sql(rows, path):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(HEADER)
        for index, row in enumerate(rows):
            exp_id, class_id, tab, phase_id, slot, item_id, rank, cls, spec, phase, name = row
            handle.write("({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}){}\n".format(
                exp_id, class_id, tab, phase_id, sql_quote(slot), item_id, rank,
                sql_quote(cls), sql_quote(spec), sql_quote(phase), sql_quote(name[:96]),
                ";" if index == len(rows) - 1 else ","))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--addon-dir", required=True, help="Bistooltip addon directory")
    parser.add_argument("--item-template", default=DEFAULT_ITEM_TEMPLATE)
    parser.add_argument("--out", help="target .sql path (default: a dated file in the updates dir)")
    parser.add_argument("--date", help="YYYY_MM_DD for the default output name")
    args = parser.parse_args()

    out = args.out
    if not out:
        import datetime
        stamp = args.date or datetime.date.today().strftime("%Y_%m_%d")
        out = os.path.join(DEFAULT_OUT_DIR, "{}_00_playerbots_bis_ranked.sql".format(stamp))

    print("reading item_template ...")
    item_names = load_item_names(args.item_template)
    print("  {} items".format(len(item_names)))

    twins = load_twins(os.path.join(args.addon_dir, "Bistooltip_horde_to_ali.lua"))
    print("  {} faction twin ids".format(len(twins)))

    report = Report()
    rows = build_rows(args.addon_dir, item_names, twins, report)

    write_sql(rows, out)
    print("wrote {} rows to {}".format(len(rows), out))
    report.dump()

    if report.unmapped_specs:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

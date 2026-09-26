#!/usr/bin/env python3
"""Static sweep for the failures this engine produces without an error message.

Everything here is wired by string and resolved at runtime, so a misspelled trigger name is not a
compile error - `Engine::ProcessTriggers` skips the node with `continue` and says nothing. Two Ulduar
nodes were dead from the day they were written and were found only by reading the registration tables
side by side; three Zul'Aman bosses and one Sunwell node still are. The catalogue this checks against
is docs/engine/pitfalls.md.

    pblint.py [path ...]         default: the whole src tree
    pblint.py --warnings         also print the advisory findings
    pblint.py --only CHECK       run one check by name
    pblint.py --spell-difficulty also sweep raid spell ids against the world DB plus the client
                                 SpellDifficulty CSV in mod-spell-tweaks (needs the DB)

Exit status is 1 if any error-level finding is reported; warnings alone exit 0.

**The whole tree is not clean.** It reports 31 errors today, every one of them a real dead node or
a documented trap, so this gates a commit only when pointed at the paths that commit touches - which
is what a path argument is for. `src/Ai/Raid/Uld` is clean.

Parsing is over comment-stripped source, in the style of apps/codestyle/codestyle-cpp.py. That is
enough because every pattern here is a literal in a registration table, but it does mean a name built
at runtime is invisible - see KNOWN_DYNAMIC.
"""
from __future__ import annotations

import argparse
import bisect
import csv
import os
import pathlib
import re
import shlex
import subprocess
import sys
from collections import defaultdict
from typing import NamedTuple

REPO = pathlib.Path(__file__).resolve().parents[2]
SRC = REPO / "src"

# Names assembled at runtime rather than written as a literal, so no creators[] key can match them by
# inspection. TwoTriggers' key must equal getName() = "<name1> and <name2>"; the resistance and burst
# nodes take a boss name as a constructor argument.
KNOWN_DYNAMIC = re.compile(r" and $|^$")

RE_CLASS_CTX = re.compile(r"\bclass\s+\w+\s*:\s*public\s+NamedObject(?:Context|Factory)<(\w+)>")
RE_CLASS_DECL = re.compile(r"^\s*class\s+(\w+)")
RE_BASE = re.compile(r":\s*public\s+(\w+)")
RE_CREATOR = re.compile(r'\bcreators\[\s*"([^"]*)"\s*\]')
RE_NEXT_ACTION = re.compile(r'\bNextAction\(\s*"([^"]*)"')
RE_TRIGGER_NODE = re.compile(r'\bTriggerNode\(\s*"([^"]*)"')
RE_ADD_STRATEGY = re.compile(r'\baddStrategies?\w*\s*\(\s*"([^"]*)"')
RE_ADD_STRATEGY_LIST = re.compile(r'"([^"]+)"')
# `: Action(botAI, "name")` / `: Trigger(botAI, "name", 200)` in a member-init list.
RE_SELF_NAME = re.compile(r':\s*(?:public\s+)?(\w+)\s*\(\s*(?:bot)?AI\s*,\s*"([^"]*)"')

# Encounter definitions (Raid/RaidEncounter.h). A class there spells its name once, as a `Name`
# constant, and a row registers both creators and the node from it, so none of the literals above
# ever appear.
RE_NAME_CONST = re.compile(r'\bstatic\s+constexpr\s+(?:char\s+const|const\s+char)\s*\*\s*Name\s*=\s*"([^"]*)"')
RE_CLASS_OPEN = re.compile(r"^\s*(?:class|struct)\s+(\w+)\b(?!\s*;)(?:[^:]*:\s*(?:public\s+)?(\w+))?")
RE_ROW_TEMPLATE = re.compile(r"\bNode\s*<\s*(\w+)\s*,\s*(\w+)\s*>\s*\(")
RE_ROW_SHARED = re.compile(r"\bNode\s*<\s*(\w+)\s*>\s*\(")
RE_ROW_EXPLICIT = re.compile(r"\.Node\s*\(")
RE_RULE_CALL = re.compile(r"\.(?:OwnMovement|Block|Exclusive)\s*\(")
RE_STRING = re.compile(r'"((?:[^"\\\n]|\\.)*)"')


def strip_comments(text: str) -> list[str]:
    """Blank out comments while keeping line numbers, so a finding still points at the right line."""
    out: list[str] = []
    in_block = False
    for line in text.splitlines():
        result = []
        i = 0
        while i < len(line):
            two = line[i:i + 2]
            if in_block:
                if two == "*/":
                    in_block = False
                    i += 2
                    continue
                i += 1
                continue
            if two == "//":
                break
            if two == "/*":
                in_block = True
                i += 2
                continue
            result.append(line[i])
            i += 1
        out.append("".join(result))
    return out


class Source:
    def __init__(self, path: pathlib.Path):
        self.path = path
        self.rel = path.relative_to(REPO).as_posix()
        self.raw = path.read_text(encoding="utf-8", errors="replace")
        self.lines = strip_comments(self.raw)


class Finding:
    def __init__(self, check: str, rel: str, line: int, message: str, error: bool = True):
        self.check, self.rel, self.line, self.message, self.error = check, rel, line, message, error

    def __str__(self) -> str:
        return f"{self.rel}:{self.line}: [{self.check}] {self.message}"


def collect(paths: list[pathlib.Path]) -> list[Source]:
    files: list[pathlib.Path] = []
    for path in paths:
        # A path given on the command line is relative to the caller's cwd, not to the repo.
        path = path.resolve()
        if path.is_file():
            files.append(path)
        else:
            files += [p for p in path.rglob("*") if p.suffix in (".h", ".hpp", ".cpp")]
    return [Source(p) for p in sorted(set(files))]


def creators_by_kind(sources: list[Source]) -> dict[str, dict[str, tuple[str, int]]]:
    """Every creators[] key, bucketed by the NamedObject{Context,Factory}<K> class that holds it.
    Both layers matter: NextAction resolves against an Action creator or an ActionNode alias, and the
    ActionNode factories are where "taunt spell" and the other per-class aliases live. Bucketing
    matters: an action name registered only in a trigger context is exactly the bug being looked for,
    and one pooled set of names would call it resolved."""
    found: dict[str, dict[str, tuple[str, int]]] = defaultdict(dict)
    for src in sources:
        kind = None
        for number, line in enumerate(src.lines, 1):
            match = RE_CLASS_CTX.search(line)
            if match:
                kind = match.group(1)
                continue
            for key in RE_CREATOR.findall(line):
                if kind:
                    found[kind].setdefault(key, (src.rel, number))
    return found


def call_arguments(text: str, open_paren: int) -> list[str]:
    """The top-level arguments of the call whose `(` sits at open_paren, so a lambda's own commas and
    strings stay inside the argument that holds them."""
    args: list[str] = []
    depth, start, i = 0, open_paren + 1, open_paren
    while i < len(text):
        string = RE_STRING.match(text, i)
        if string:
            i = string.end()
            continue
        char = text[i]
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
            if depth == 0:
                args.append(text[start:i].strip())
                return args
        elif char == "," and depth == 1:
            args.append(text[start:i].strip())
            start = i + 1
        i += 1
    return args


def literal(arg: str) -> str | None:
    match = RE_STRING.fullmatch(arg)
    return match.group(1) if match else None


class Row(NamedTuple):
    trigger: str
    action: str
    rel: str
    line: int
    shared: bool = False  # the action comes from a shared context; the row registers only the trigger


class Registry(NamedTuple):
    creators: dict[str, dict[str, tuple[str, int]]]
    rows: list[Row]
    rule_names: list[tuple[str, str, int]]  # an action name a rule lists, where it is listed
    name_constants: dict[str, tuple[str, str, int]]  # class -> its Name, where it is declared

    def row_triggers(self) -> set[str]:
        return {row.trigger for row in self.rows}

    def row_actions(self) -> set[str]:
        return {row.action for row in self.rows if not row.shared}


def name_constants(sources: list[Source]) -> dict[str, tuple[str, str, int]]:
    """class -> (Name, file, line), including a Name a class only inherits: `T::Name` compiles for a
    subclass that declares none of its own, and means the base's."""
    own: dict[str, tuple[str, str, int]] = {}
    base_of: dict[str, str] = {}
    for src in sources:
        current = None
        for number, line in enumerate(src.lines, 1):
            opened = RE_CLASS_OPEN.match(line)
            if opened:
                current = opened.group(1)
                if opened.group(2):
                    base_of.setdefault(current, opened.group(2))
            match = RE_NAME_CONST.search(line)
            if match and current:
                own.setdefault(current, (match.group(1), src.rel, number))

    found = dict(own)
    for cls in base_of:
        seen, walk = set(), cls
        while walk not in own and walk in base_of and walk not in seen:
            seen.add(walk)
            walk = base_of[walk]
        if walk in own:
            found.setdefault(cls, own[walk])
    return found


def registry(sources: list[Source]) -> Registry:
    names = name_constants(sources)
    rows: list[Row] = []
    rule_names: list[tuple[str, str, int]] = []
    for src in sources:
        text = "\n".join(src.lines)

        def line_of(pos: int) -> int:
            return text.count("\n", 0, pos) + 1

        for match in RE_ROW_TEMPLATE.finditer(text):
            trigger, action = names.get(match.group(1)), names.get(match.group(2))
            if trigger and action:
                rows.append(Row(trigger[0], action[0], src.rel, line_of(match.start())))
        for match in RE_ROW_SHARED.finditer(text):
            trigger = names.get(match.group(1))
            args = call_arguments(text, match.end() - 1)
            if trigger and args and literal(args[0]) is not None:
                rows.append(Row(trigger[0], literal(args[0]), src.rel, line_of(match.start()), shared=True))
        for match in RE_ROW_EXPLICIT.finditer(text):
            args = call_arguments(text, match.end() - 1)
            if len(args) >= 3 and literal(args[0]) is not None and literal(args[2]) is not None:
                rows.append(Row(literal(args[0]), literal(args[2]), src.rel, line_of(match.start())))
        for match in RE_RULE_CALL.finditer(text):
            # The first argument is the rule's own name; only a brace list holds action names.
            for arg in call_arguments(text, match.end() - 1)[1:]:
                if arg.startswith("{") or arg.startswith("std::vector"):
                    for string in RE_STRING.finditer(arg):
                        rule_names.append((string.group(1), src.rel, line_of(match.start())))
    return Registry(creators_by_kind(sources), rows, rule_names, names)


def base_name(name: str) -> str:
    """What create() actually looks up. It splits at "::" and passes the tail to Qualified::Qualify,
    so `say::taunt` resolves the creator `say` (NamedObjectContext.h:54)."""
    head, sep, _ = name.partition("::")
    return head if sep else name


def references(sources: list[Source]) -> tuple[list, list]:
    """Every name a node asks for. Matched over the whole file rather than line by line: 705 of the
    1,942 TriggerNode calls put the name on the line after the paren, and a per-line scan sees none of
    them - which is how the Yogg-Saron typo stayed visible only from the creators side."""
    actions, triggers = [], []
    for src in sources:
        text = "\n".join(src.lines)
        for pattern, sink in ((RE_NEXT_ACTION, actions), (RE_TRIGGER_NODE, triggers)):
            for match in pattern.finditer(text):
                number = text.count("\n", 0, match.start()) + 1
                sink.append((base_name(match.group(1)), src.rel, number))
    return actions, triggers


# --- checks -------------------------------------------------------------------


def check_unresolved(sources, reg: Registry) -> list[Finding]:
    """A node naming something no context can build. The engine skips it silently and forever. A rule
    listing an action nothing registers is the same failure: it blocks or passes nothing, and says
    nothing."""
    out = []
    creators = reg.creators
    actions, triggers = references(sources)
    known = set(creators.get("Action", {})) | set(creators.get("ActionNode", {})) | reg.row_actions()
    for name, rel, number in actions:
        if name and name not in known and not KNOWN_DYNAMIC.search(name):
            out.append(Finding("unresolved-action", rel, number,
                               f'NextAction("{name}") has no creators[] entry in any action context'))
    known_triggers = set(creators.get("Trigger", {})) | reg.row_triggers()
    for name, rel, number in triggers:
        if name and name not in known_triggers and not KNOWN_DYNAMIC.search(name):
            out.append(Finding("unresolved-trigger", rel, number,
                               f'TriggerNode("{name}") has no creators[] entry in any trigger context'))
    for row in reg.rows:
        if row.shared and row.action not in known and not KNOWN_DYNAMIC.search(row.action):
            out.append(Finding("unresolved-action", row.rel, row.line,
                               f'a row runs "{row.action}", which no action context registers'))
    for name, rel, number in reg.rule_names:
        if name not in known:
            out.append(Finding("unresolved-rule-action", rel, number,
                               f'a rule lists "{name}", which no action context or row registers'))
    return out


def check_orphan_creators(sources, reg: Registry) -> list[Finding]:
    """Registered and never referenced: a behaviour somebody wrote that nothing can run."""
    out = []
    actions, triggers = references(sources)
    used_actions = {name for name, _, _ in actions} | {row.action for row in reg.rows}
    used_triggers = {name for name, _, _ in triggers} | reg.row_triggers()
    for kind, used in (("Action", used_actions), ("Trigger", used_triggers)):
        for name, (rel, number) in reg.creators.get(kind, {}).items():
            if name not in used:
                out.append(Finding(f"orphan-{kind.lower()}", rel, number,
                                   f'creators["{name}"] is never named by any node', error=False))
    return out


def check_definition_rows(sources, reg: Registry) -> list[Finding]:
    """A creators[] entry left behind for a name a definition row now registers. The row's creator is
    the gated one; which of the two a bot builds depends on registration order, so a missed delete
    during a migration can quietly ungate a trigger."""
    out = []
    rows = {}
    for row in reg.rows:
        rows.setdefault(row.trigger, row)
        if not row.shared:
            rows.setdefault(row.action, row)
    for kind in ("Action", "Trigger"):
        for name, (rel, number) in reg.creators.get(kind, {}).items():
            row = rows.get(name)
            if row:
                out.append(Finding("row-duplicate", rel, number,
                                   f'creators["{name}"] duplicates the definition row at '
                                   f"{row.rel}:{row.line}; delete this entry"))
    return out


def check_ctor_name(sources, reg: Registry) -> list[Finding]:
    """A leaf class whose own name string is registered nowhere. Both halves have to match for a node
    to run, so a name that appears only in the constructor is a behaviour nothing can build.

    Base classes are exempt and there are many: `cure party member` names itself that and leaves its
    subclasses to register under spell names. A class anybody derives from is therefore skipped -
    without that the check reports 28 findings of which 4 are real."""
    out = []
    creators = reg.creators
    registered = (set(creators.get("Action", {})) | set(creators.get("Trigger", {})) | reg.row_actions()
                  | reg.row_triggers())
    bases = {b for src in sources for line in src.lines for b in RE_BASE.findall(line)}

    # The Name form: a class nothing derives from, whose constant no row or creator uses.
    reported: set[tuple[str, int]] = set()
    for cls, (name, rel, number) in reg.name_constants.items():
        if cls in bases or name in registered or (rel, number) in reported:
            continue
        reported.add((rel, number))
        out.append(Finding("unregistered-class", rel, number,
                           f'{cls} names itself "{name}" and nothing registers that name', error=False))

    for src in sources:
        current = None
        for number, line in enumerate(src.lines, 1):
            declared = RE_CLASS_DECL.match(line)
            if declared:
                current = declared.group(1)
            match = RE_SELF_NAME.search(line)
            if not match:
                continue
            base, name = match.group(1), match.group(2)
            # A trailing space is a concatenation prefix - the real name is built at runtime.
            if base not in ("Action", "Trigger") or not name or name.endswith(" "):
                continue
            if KNOWN_DYNAMIC.search(name) or current in bases:
                continue
            if name not in registered:
                out.append(Finding("unregistered-class", src.rel, number,
                                   f'{current or base} names itself "{name}" and nothing registers '
                                   f"that name", error=False))
    return out


def check_strategy_activation(sources, reg: Registry) -> list[Finding]:
    """Registration is not activation. A strategy with a creator that no addStrategies* call ever
    names can never run - DpsAoeStrategy is the standing example."""
    out = []
    added: set[str] = set()
    for src in sources:
        for line in src.lines:
            if "addStrateg" not in line:
                continue
            added.update(RE_ADD_STRATEGY_LIST.findall(line))
    # The lists are often built across several lines; sweep whole-file for any quoted token that sits
    # inside an addStrategies argument list, which the line sweep above misses on a wrapped call.
    for src in sources:
        for block in re.findall(r"addStrateg\w*\s*\((.*?)\)", "\n".join(src.lines), re.S):
            added.update(RE_ADD_STRATEGY_LIST.findall(block))

    for name, (rel, number) in reg.creators.get("Strategy", {}).items():
        if name and name not in added:
            out.append(Finding("strategy-never-added", rel, number,
                               f'strategy "{name}" has a creator but no addStrategies* call adds it',
                               error=False))
    return out


def check_raid_sites(sources, reg: Registry) -> list[Finding]:
    """The four raid registration sites are pure name lists, so a merge that drops one side
    unregisters a raid strategy with nothing failing to compile."""
    out = []
    text = {src.rel: "\n".join(src.lines) for src in sources}

    ai = next((t for r, t in text.items() if r.endswith("Bot/PlayerbotAI.cpp")), None)
    if ai is None:
        return out

    # Every strategy context, not just the raid one: the five-man keys live in DungeonStrategyContext.
    keys = set(reg.creators.get("Strategy", {}))
    listed = set()
    block = re.search(r"allInstanceStrategies\s*=\s*\{(.*?)\}", ai, re.S)
    if block:
        listed = set(RE_ADD_STRATEGY_LIST.findall(block.group(1)))
    arms = set(re.findall(r'strategyName\s*=\s*"([^"]+)"', ai))

    rel = "src/Bot/PlayerbotAI.cpp"
    for name in sorted(arms - listed):
        out.append(Finding("raid-sites", rel, 1,
                           f'map arm sets "{name}" but allInstanceStrategies omits it, so it is '
                           f"applied and never removed on a zone change"))
    for name in sorted(listed - arms):
        out.append(Finding("raid-sites", rel, 1,
                           f'allInstanceStrategies lists "{name}" but no case arm ever selects it',
                           error=False))
    for name in sorted(arms - keys):
        out.append(Finding("raid-sites", rel, 1,
                           f'map arm sets "{name}" but no strategy context has a creator for it'))
    return out


def check_trigger_interval(sources) -> list[Finding]:
    """Trigger's checkInterval is normalised as `< 100 ? value * 1000 : value`, so 2..99 silently
    means seconds. A positioning trigger throttled to 5 s is not a positioning trigger."""
    out = []
    pattern = re.compile(r':\s*Trigger\s*\(\s*(?:bot)?AI\s*,\s*(?:"[^"]*"|Name)\s*,\s*(\d+)\s*\)')
    for src in sources:
        for number, line in enumerate(src.lines, 1):
            match = pattern.search(line)
            if match and 2 <= int(match.group(1)) <= 99:
                out.append(Finding("trigger-interval", src.rel, number,
                                   f"checkInterval {match.group(1)} means {match.group(1)} SECONDS, "
                                   f"not ms - pass 100+ for milliseconds or 1 for every tick",
                                   error=src.rel.startswith("src/Ai/Raid/")))
    return out


def check_arc_defaults(sources) -> list[Finding]:
    """isInFront and isInBack both default to arc = M_PI, so front-half plus back-half is the whole
    circle and the test is always true.

    Only the defaulted pair is wrong. Passing an arc to both narrows each cone and leaves real side
    wedges, which is the fix, so a call with two arguments is deliberate and not reported.
    """
    out = []
    pattern = re.compile(
        r"isInFront\s*\((?P<front>[^;()]*(?:\([^()]*\)[^;()]*)*)\)\s*\|\|"
        r"\s*[\w>.\-]*isInBack\s*\((?P<back>[^;()]*(?:\([^()]*\)[^;()]*)*)\)"
    )
    for src in sources:
        for number, line in enumerate(src.lines, 1):
            match = pattern.search(line)
            if match and not ("," in match.group("front") and "," in match.group("back")):
                out.append(Finding("arc-always-true", src.rel, number,
                                   "isInFront() || isInBack() covers the whole circle - always true"))
    return out


def check_move_inside_zero(sources) -> list[Finding]:
    """MoveInside returns false only when the bot is already within `distance`, so at 0 it succeeds
    every tick, outranks combat and pins every melee on one coordinate."""
    out = []
    pattern = re.compile(r"\bMoveInside\s*\([^;]*,\s*0(?:\.0*f?)?\s*[,)]")
    for src in sources:
        for number, line in enumerate(src.lines, 1):
            if pattern.search(line):
                out.append(Finding("moveinside-zero", src.rel, number,
                                   "MoveInside(..., 0) never returns false and stacks the raid on one point"))
    return out


def check_assist_index(sources) -> list[Finding]:
    """ignoreDeadPlayers defaults to false, which deletes a role the moment its holder dies."""
    out = []
    pattern = re.compile(r"\bIsAssist(?:Tank|Heal|RangedDps)OfIndex\s*\(([^;]*?)\)")
    for src in sources:
        for number, line in enumerate(src.lines, 1):
            match = pattern.search(line)
            if match and match.group(1).count(",") < 2:
                out.append(Finding("assist-index-default", src.rel, number,
                                   "IsAssist*OfIndex without ignoreDeadPlayers: the role vanishes when "
                                   "its holder dies", error=False))
    return out


def check_shared_base_cast(sources) -> list[Finding]:
    """A veto must dynamic_cast to the concrete redirect action. BuffOnMainTankAction is also paladin
    Beacon, shaman Earth Shield and druid Thorns - casting to it vetoes all of them."""
    out = []
    for src in sources:
        for number, line in enumerate(src.lines, 1):
            if "dynamic_cast<BuffOnMainTankAction" in line.replace(" ", ""):
                out.append(Finding("shared-base-cast", src.rel, number,
                                   "dynamic_cast to BuffOnMainTankAction also catches Beacon, Earth "
                                   "Shield and Thorns - cast to the concrete action", error=False))
    return out


class SpellDifficulty(NamedTuple):
    rows: dict[int, tuple[int, ...]]  # row id -> its distinct spell ids, in difficulty order
    row_of: dict[int, int]            # every member spell id -> its row id


def merge_spell_difficulty(dbc_rows, db_rows) -> SpellDifficulty:
    """The table GetSpellIdForDifficulty really reads, from rows of [row id, 4 difficulty ids].

    Same steps as the core: the DB overlays the DBC by row id, the DB winning (DBCDatabaseLoader),
    then a row is skipped unless its first two ids are set, and every id of what is left maps to its
    row (DBCStores.cpp). So the lookup resolves from any member, not only the 10-man base."""
    merged = {row[0]: tuple(row[1:]) for row in dbc_rows}
    merged.update({row[0]: tuple(row[1:]) for row in db_rows})

    rows: dict[int, tuple[int, ...]] = {}
    row_of: dict[int, int] = {}
    for row_id in sorted(merged):
        ids = merged[row_id]
        if len(ids) < 2 or ids[0] <= 0 or ids[1] <= 0:
            continue
        kept = tuple(dict.fromkeys(spell for spell in ids if spell > 0))
        if len(kept) < 2:
            continue
        rows[row_id] = kept
        # ascending row id, last write wins, same order the core fills its lookup map in
        for spell in kept:
            row_of[spell] = row_id
    return SpellDifficulty(rows, row_of)


def _cell(value: str) -> int:
    value = value.strip()
    return int(value) if value.lstrip("-").isdigit() else 0


def _world_db_rows() -> list[list[int]]:
    command = os.environ.get(
        "PB_MYSQL",
        "docker exec ac-database mysql -uroot -ppassword -N -B acore_world",
    )
    query = ("SELECT ID,DifficultySpellID_1,DifficultySpellID_2,DifficultySpellID_3,"
             "DifficultySpellID_4 FROM spelldifficulty_dbc")
    try:
        out = subprocess.run(shlex.split(command) + ["-e", query],
                             capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError) as err:
        raise RuntimeError(f"could not reach the world DB ({err}); set PB_MYSQL") from err
    if out.returncode != 0:
        raise RuntimeError(f"world DB query failed: {out.stderr.strip() or out.stdout.strip()}")
    rows = [[_cell(c) for c in line.split("\t")] for line in out.stdout.splitlines() if line.strip()]
    if not rows:
        raise RuntimeError("world DB returned no spelldifficulty_dbc rows; set PB_MYSQL")
    return rows


def _client_csv_rows() -> list[list[int]]:
    path = pathlib.Path(os.environ.get("PB_SPELL_DIFFICULTY_CSV") or REPO.parents[1] / "modules" /
                        "mod-spell-tweaks" / "data" / "dbc-reference" /
                        "spelldifficulty.reference.csv")
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            rows = [[_cell(c) for c in record] for record in csv.reader(handle)
                    if record and record[0].strip().isdigit()]
    except OSError as err:
        raise RuntimeError(f"could not read the client spell difficulty CSV ({err}); "
                           f"set PB_SPELL_DIFFICULTY_CSV") from err
    if not rows:
        raise RuntimeError(f"{path} holds no rows; set PB_SPELL_DIFFICULTY_CSV")
    return rows


def spell_difficulty_table() -> SpellDifficulty:
    """World DB spelldifficulty_dbc merged over the client SpellDifficulty.dbc, as the core loads it.

    Neither source is enough alone: they share no row. The DB's 604 are keyed by base spell id and
    hold Mimiron's Plasma Blast 62997 -> 64529 and Valithria's Emerald Vigor 70873 -> 71941; the
    client's 581 (mod-spell-tweaks' reference CSV) keep their DBC ids and hold every ToC combat remap
    and the rest of ICC's. Either one alone reads the other's remaps as "none", so a missing source is
    an error, never a one-source sweep."""
    return merge_spell_difficulty(_client_csv_rows(), _world_db_rows())


# Creature entries, gameobjects, items and map ids share the numeric range with spell ids, and a few
# collide with a real spell difficulty row: NPC_ICEHOWL is 34797, which is also a remapped spell.
NOT_A_SPELL = re.compile(
    r"(?i)(npc|entr(?:y|ies)|creature|gameobject|go_|item|map|area|zone|faction|quest)")

SPELL_DIFFICULTY_OK = "pblint: spell-difficulty-ok"

# 4-6 digits standing alone, so the integer part of a float coordinate is not an id.
RE_ID_LITERAL = re.compile(r"(?<![\w.])(\d{4,6})(?![\w.])")
RE_NAMED_ID = re.compile(r"\b([A-Za-z_]\w{3,})\s*=\s*(\d{4,6})(?![\w.])")
RE_REMAP_CALL = re.compile(r"\bGetSpellIdForDifficulty\s*\(")
RE_REMAP_TOKEN = re.compile(r"[A-Za-z_]\w*|(?<![\w.])\d{4,6}(?![\w.])")
RE_BRACE_INIT = re.compile(r"\b([A-Za-z_]\w*)\s*(\[[^\]\n]*\]\s*)?(=\s*)?\{")
RE_WORD_BEFORE = re.compile(r"(\w+|[>*&])\s+$")
NOT_A_DECLARED_NAME = {"namespace", "struct", "class", "enum", "union", "else", "do", "try",
                       "return", "const", "override", "final", "noexcept", "mutable"}
NOT_A_DECLARATION = {"namespace", "struct", "class", "enum", "union", "public", "protected",
                     "private", "virtual", "return", "new", "throw"}


def brace_initializers(text: str) -> list[tuple[int, int, str]]:
    """(open, close, name) of every `name = {...}`, `name[] = {...}` and `Type name{...}`, so a
    literal in a list takes the list's name. Class, enum and namespace bodies are not lists."""
    spans = []
    for match in RE_BRACE_INIT.finditer(text):
        name = match.group(1)
        if name in NOT_A_DECLARED_NAME:
            continue
        if not (match.group(2) or match.group(3)):
            before = RE_WORD_BEFORE.search(text, max(0, match.start() - 64), match.start())
            if not before or before.group(1) in NOT_A_DECLARATION:
                continue
        start = match.end() - 1
        depth = 0
        for end in range(start, len(text)):
            if text[end] == "{":
                depth += 1
            elif text[end] == "}":
                depth -= 1
                if depth == 0:
                    spans.append((start, end, name))
                    break
    return spans


def remap_arguments(text: str) -> set[str]:
    """Every identifier and id literal in the first argument of each GetSpellIdForDifficulty call,
    read up to the top-level comma, so `ToCSpells::SPELL_X` and `static_cast<uint32>(SPELL_X)`
    count as much as a bare `SPELL_X`."""
    tokens: set[str] = set()
    for match in RE_REMAP_CALL.finditer(text):
        depth = 0
        end = match.end()
        while end < len(text):
            char = text[end]
            if char in "([{":
                depth += 1
            elif char in ")]}":
                if depth == 0:
                    break
                depth -= 1
            elif char == "," and depth == 0:
                break
            end += 1
        tokens.update(RE_REMAP_TOKEN.findall(text, match.end(), end))
    return tokens


def check_spell_difficulty(sources, table: SpellDifficulty) -> list[Finding]:
    """A raid handling only some difficulties of a remapped spell. The rule is built on the wrong id
    half the time it matters, and a clean lookup on one id proves nothing about the others.

    Works per row, within one raid directory: a row is reported once, at its first referenced
    member, when some of its ids appear nowhere and no referenced member is handled. Handled means
    the constant or literal sits in the first argument of a GetSpellIdForDifficulty call, the
    constant is declared and never used, its name (or its brace list's name) says it is not a
    spell, or a comment on its line or the one above carries `pblint: spell-difficulty-ok`."""
    out = []
    by_raid: dict[str, list[Source]] = defaultdict(list)
    for src in sources:
        match = re.match(r"src/Ai/Raid/([^/]+)/", src.rel)
        if match:
            by_raid[match.group(1)].append(src)

    for raid, files in sorted(by_raid.items()):
        # literal -> every (file, line, enclosing brace list name) it appears at
        occurrences: dict[int, list[tuple[Source, int, str | None]]] = defaultdict(list)
        declared: dict[int, list[str]] = defaultdict(list)
        remapped: set[str] = set()
        used: set[str] = set()

        for src in files:
            text = "\n".join(src.lines)
            line_starts = [0] + [i + 1 for i, char in enumerate(text) if char == "\n"]
            spans = brace_initializers(text)
            for match in RE_NAMED_ID.finditer(text):
                if match.group(1) not in declared[int(match.group(2))]:
                    declared[int(match.group(2))].append(match.group(1))
            remapped |= remap_arguments(text)
            for match in re.finditer(r"\b([A-Za-z_]\w{3,})\b(?!\s*=\s*\d)", text):
                used.add(match.group(1))
            for match in RE_ID_LITERAL.finditer(text):
                pos = match.start()
                holders = [s for s in spans if s[0] < pos < s[1]]
                holder = max(holders)[2] if holders else None
                number = bisect.bisect_right(line_starts, pos)
                occurrences[int(match.group(1))].append((src, number, holder))

        def marked(src: Source, number: int) -> bool:
            raw = src.raw.splitlines()
            return any(1 <= n <= len(raw) and SPELL_DIFFICULTY_OK in raw[n - 1]
                       and SPELL_DIFFICULTY_OK not in src.lines[n - 1]
                       for n in (number - 1, number))

        def handled(spell: int) -> bool:
            names = declared.get(spell, [])
            if str(spell) in remapped or any(name in remapped for name in names):
                return True
            if names and not any(name in used for name in names):
                return True
            holders = [holder for _, _, holder in occurrences[spell] if holder]
            if any(NOT_A_SPELL.search(name) for name in names + holders):
                return True
            return any(marked(src, number) for src, number, _ in occurrences[spell])

        referenced: dict[int, list[int]] = defaultdict(list)
        for spell in occurrences:
            if spell in table.row_of:
                referenced[table.row_of[spell]].append(spell)

        for row_id, members in sorted(referenced.items()):
            ids = table.rows[row_id]
            missing = [spell for spell in ids if spell not in occurrences]
            if not missing or any(handled(spell) for spell in members):
                continue

            first = min(members, key=lambda s: (occurrences[s][0][0].rel, occurrences[s][0][1]))
            src, number, holder = occurrences[first][0]
            name = (declared.get(first) or [holder])[0]
            label = f"{name} ({first})" if name else str(first)
            out.append(Finding("spell-difficulty", src.rel, number,
                               f"{label} is in spell difficulty row {row_id} "
                               f"({'/'.join(str(s) for s in ids)}); {raid} never references "
                               f"{', '.join(str(s) for s in missing)} and passes no member through "
                               f"GetSpellIdForDifficulty, so the rule may run on one difficulty only",
                               error=False))
    return out


CHECKS = {
    "unresolved": (check_unresolved, True),
    "orphan-creators": (check_orphan_creators, True),
    "definition-rows": (check_definition_rows, True),
    "ctor-name": (check_ctor_name, True),
    "strategy-activation": (check_strategy_activation, True),
    "raid-sites": (check_raid_sites, True),
    "trigger-interval": (check_trigger_interval, False),
    "arc-defaults": (check_arc_defaults, False),
    "move-inside-zero": (check_move_inside_zero, False),
    "assist-index": (check_assist_index, False),
    "shared-base-cast": (check_shared_base_cast, False),
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="*", type=pathlib.Path,
                        help="files or directories to check (default: the whole src tree)")
    parser.add_argument("--warnings", action="store_true", help="also print advisory findings")
    parser.add_argument("--only", metavar="CHECK", help=f"one of: {', '.join(sorted(CHECKS))}")
    parser.add_argument("--spell-difficulty", action="store_true",
                        help="also sweep raid spell ids against spelldifficulty_dbc merged over "
                             "the client SpellDifficulty CSV (needs the DB; override the mysql "
                             "command with PB_MYSQL, the CSV path with PB_SPELL_DIFFICULTY_CSV)")
    args = parser.parse_args()

    if args.only and args.only not in CHECKS:
        print(f"unknown check {args.only}; known: {', '.join(sorted(CHECKS))}", file=sys.stderr)
        return 2

    # The name checks need every context in the module, not only the paths under review: a raid node
    # resolves against creators registered in Ai/Base. Narrowing happens when findings are reported.
    world = collect([SRC])
    scope = {src.rel for src in collect(args.paths)} if args.paths else None
    reg = registry(world)

    findings: list[Finding] = []
    for name, (func, needs_creators) in CHECKS.items():
        if args.only and name != args.only:
            continue
        findings += func(world, reg) if needs_creators else func(world)

    if args.spell_difficulty and not args.only:
        try:
            findings += check_spell_difficulty(world, spell_difficulty_table())
        except RuntimeError as err:
            print(f"spell-difficulty: {err}", file=sys.stderr)
            return 2

    if scope is not None:
        findings = [f for f in findings if f.rel in scope]

    errors = [f for f in findings if f.error]
    warnings = [f for f in findings if not f.error]

    for finding in sorted(errors, key=lambda f: (f.rel, f.line)):
        print(finding)
    if args.warnings:
        for finding in sorted(warnings, key=lambda f: (f.rel, f.line)):
            print(finding)

    shown = len(errors) + (len(warnings) if args.warnings else 0)
    hidden = 0 if args.warnings else len(warnings)
    summary = f"\n{len(errors)} error(s)"
    summary += f", {len(warnings)} warning(s)" + (" (--warnings to show)" if hidden else "")
    print(summary if shown or warnings else "\nclean")

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())

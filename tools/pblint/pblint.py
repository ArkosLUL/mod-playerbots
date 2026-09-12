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
    pblint.py --spell-difficulty also sweep raid spell ids against the world DB (needs the DB)

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
import os
import pathlib
import re
import shlex
import subprocess
import sys
from collections import defaultdict

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


def check_unresolved(sources, creators) -> list[Finding]:
    """A node naming something no context can build. The engine skips it silently and forever."""
    out = []
    actions, triggers = references(sources)
    for name, rel, number in actions:
        known = set(creators.get("Action", {})) | set(creators.get("ActionNode", {}))
        if name and name not in known and not KNOWN_DYNAMIC.search(name):
            out.append(Finding("unresolved-action", rel, number,
                               f'NextAction("{name}") has no creators[] entry in any action context'))
    for name, rel, number in triggers:
        if name and name not in creators.get("Trigger", {}) and not KNOWN_DYNAMIC.search(name):
            out.append(Finding("unresolved-trigger", rel, number,
                               f'TriggerNode("{name}") has no creators[] entry in any trigger context'))
    return out


def check_orphan_creators(sources, creators) -> list[Finding]:
    """Registered and never referenced: a behaviour somebody wrote that nothing can run."""
    out = []
    actions, triggers = references(sources)
    used_actions = {name for name, _, _ in actions}
    used_triggers = {name for name, _, _ in triggers}
    for kind, used in (("Action", used_actions), ("Trigger", used_triggers)):
        for name, (rel, number) in creators.get(kind, {}).items():
            if name not in used:
                out.append(Finding(f"orphan-{kind.lower()}", rel, number,
                                   f'creators["{name}"] is never named by any node', error=False))
    return out


def check_ctor_name(sources, creators) -> list[Finding]:
    """A leaf class whose own name string is registered nowhere. Both halves have to match for a node
    to run, so a name that appears only in the constructor is a behaviour nothing can build.

    Base classes are exempt and there are many: `cure party member` names itself that and leaves its
    subclasses to register under spell names. A class anybody derives from is therefore skipped -
    without that the check reports 28 findings of which 4 are real."""
    out = []
    registered = set(creators.get("Action", {})) | set(creators.get("Trigger", {}))
    bases = {b for src in sources for line in src.lines for b in RE_BASE.findall(line)}

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


def check_strategy_activation(sources, creators) -> list[Finding]:
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

    for name, (rel, number) in creators.get("Strategy", {}).items():
        if name and name not in added:
            out.append(Finding("strategy-never-added", rel, number,
                               f'strategy "{name}" has a creator but no addStrategies* call adds it',
                               error=False))
    return out


def check_raid_sites(sources, creators) -> list[Finding]:
    """The four raid registration sites are pure name lists, so a merge that drops one side
    unregisters a raid strategy with nothing failing to compile."""
    out = []
    text = {src.rel: "\n".join(src.lines) for src in sources}

    ai = next((t for r, t in text.items() if r.endswith("Bot/PlayerbotAI.cpp")), None)
    if ai is None:
        return out

    # Every strategy context, not just the raid one: the five-man keys live in DungeonStrategyContext.
    keys = set(creators.get("Strategy", {}))
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
    pattern = re.compile(r':\s*Trigger\s*\(\s*(?:bot)?AI\s*,\s*"[^"]*"\s*,\s*(\d+)\s*\)')
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
    circle and the test is always true."""
    out = []
    pattern = re.compile(r"isInFront\s*\([^;]*?\)\s*\|\|\s*[\w>.\-]*isInBack\s*\(")
    for src in sources:
        for number, line in enumerate(src.lines, 1):
            if pattern.search(line):
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


def spell_difficulty_table() -> dict[int, list[int]]:
    """spelldifficulty_dbc from the world DB, as base id -> the ids each difficulty really uses.

    The world DB, not the client DBC: SpellDifficulty.dbc can be empty for a spell the DB does remap.
    Mimiron's Plasma Blast is exactly that - 62997 -> 64529 is in the table and absent from
    mod-spell-tweaks' reference CSV (582 rows against the DB's 604), so a DBC-only sweep reads it as
    "no remap" and the 25-man defensive stays dead."""
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

    table: dict[int, list[int]] = {}
    for row in out.stdout.splitlines():
        parts = [p for p in row.split("\t") if p != ""]
        if len(parts) < 2:
            continue
        base, variants = int(parts[0]), [int(p) for p in parts[1:]]
        table[base] = [v for v in variants if v]
    return table


# Creature entries, gameobjects, items and map ids share the numeric range with spell ids, and a few
# collide with a real spelldifficulty_dbc row - NPC_ICEHOWL is 34797, which is also a remapped spell.
NOT_A_SPELL = re.compile(r"(?i)(npc|entry|creature|gameobject|go_|item|map|area|zone|faction|quest)")


def difficulty_is_discussed(src: Source, number: int) -> bool:
    """Whether the author has already written about the remap next to the constant. Naxx's spell ids
    carry "25-man remaps these through spelldifficulty_dbc" and then match on name or dispel type
    instead - a deliberate answer the id-pair test cannot see. Read from the raw text, since the
    scanned copy has its comments stripped."""
    lines = src.raw.splitlines()
    window = lines[max(0, number - 4):number]
    return any("difficult" in line.lower() or "25-man" in line.lower() for line in window)


def check_spell_difficulty(sources, table) -> list[Finding]:
    """A raid handling only one side of a 10/25 spell pair. The rule is built on the wrong id half the
    time it matters, and a clean lookup on one id proves nothing about the other: 28 of Ulduar's 139
    constants remap, and three checks were reading only the 10-man number."""
    out = []
    by_raid: dict[str, list[Source]] = defaultdict(list)
    by_rel: dict[str, Source] = {src.rel: src for src in sources}
    for src in sources:
        match = re.match(r"src/Ai/Raid/([^/]+)/", src.rel)
        if match:
            by_raid[match.group(1)].append(src)

    for raid, files in sorted(by_raid.items()):
        seen: dict[int, tuple[str, int]] = {}
        named: dict[int, str] = {}
        remapped: set[str] = set()
        used: set[str] = set()

        for src in files:
            text = "\n".join(src.lines)
            # Both halves of the honest pattern: the constant's name, and whether anything hands it
            # to the core's own remapper. A constant passed through GetSpellIdForDifficulty is correct
            # on every difficulty and must not be reported.
            for match in re.finditer(r"\b([A-Za-z_]\w{3,})\s*=\s*(\d{4,6})\b", text):
                named.setdefault(int(match.group(2)), match.group(1))
            for match in re.finditer(r"GetSpellIdForDifficulty\(\s*([A-Za-z_]\w*)", text):
                remapped.add(match.group(1))
            for match in re.finditer(r"\b([A-Za-z_]\w{3,})\b(?!\s*=\s*\d)", text):
                used.add(match.group(1))
            for number, line in enumerate(src.lines, 1):
                for literal in re.findall(r"\b(\d{4,6})\b", line):
                    seen.setdefault(int(literal), (src.rel, number))

        for spell, (rel, number) in sorted(seen.items()):
            variants = table.get(spell)
            if not variants or len(set(variants)) < 2:
                continue
            missing = [v for v in set(variants) if v != spell and v not in seen]
            if not missing:
                continue

            name = named.get(spell)
            if name and (name in remapped or name not in used or NOT_A_SPELL.search(name)):
                continue
            if difficulty_is_discussed(by_rel[rel], number):
                continue

            names = ", ".join(str(v) for v in sorted(missing))
            label = f"{name} ({spell})" if name else str(spell)
            out.append(Finding("spell-difficulty", rel, number,
                               f"{label} remaps by difficulty to {names}, which {raid} never "
                               f"references and nothing passes it through "
                               f"GetSpellIdForDifficulty - the rule may run on one difficulty only",
                               error=False))
    return out


CHECKS = {
    "unresolved": (check_unresolved, True),
    "orphan-creators": (check_orphan_creators, True),
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
                        help="also sweep raid spell constants against the world DB's "
                             "spelldifficulty_dbc (needs the DB; override the client with PB_MYSQL)")
    args = parser.parse_args()

    if args.only and args.only not in CHECKS:
        print(f"unknown check {args.only}; known: {', '.join(sorted(CHECKS))}", file=sys.stderr)
        return 2

    # The name checks need every context in the module, not only the paths under review: a raid node
    # resolves against creators registered in Ai/Base. Narrowing happens when findings are reported.
    world = collect([SRC])
    scope = {src.rel for src in collect(args.paths)} if args.paths else None
    creators = creators_by_kind(world)

    findings: list[Finding] = []
    for name, (func, needs_creators) in CHECKS.items():
        if args.only and name != args.only:
            continue
        findings += func(world, creators) if needs_creators else func(world)

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

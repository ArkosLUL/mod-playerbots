"""Whether a trace counts as evidence, before anything is read out of it.

Roughly half the analysis in a pull write-up used to go into establishing these four facts by hand,
and getting the first one wrong cost a session: three Freya pulls on 2026-09-05 were read as evidence
against code the running binary predated by two hours. The trace carries them from v11 on, so the
report states them instead.
"""
from __future__ import annotations

import datetime
import pathlib
import subprocess

from obstrace import Trace, boss_from_path, canonical_boss, filed_under_map, map_of, slugify

# Raid difficulty ids. Only raid maps are tracked unless Obs.Maps names one, so these are the labels
# that apply; a 5-man would read 0/1 as normal/heroic instead.
DIFFICULTY = {0: "10-man normal", 1: "25-man normal", 2: "10-man heroic", 3: "25-man heroic"}


def boss_of(trace: Trace) -> str:
    """The corrected boss slug. hdr.boss is line one of an append-only file, so a session that opened
    before its boss engaged still carries the map name there; the rename record is authoritative."""
    renames = [p for p in trace.of("pull") if p.get("src") == "rename"]
    if renames:
        return str(renames[-1].get("boss") or "")
    return str(trace.header.get("boss") or "")


def engaged_of(trace: Trace) -> str:
    """The encounter read out of the loaded records: the boss-flagged unit that traded damage.

    The same recovery `obstrace.recover_boss` does off disk, but free here because the trace is
    already in memory. Several creatures can carry the flag, so the damage is what picks one.
    """
    flagged = {guid for guid in trace.bosses}
    if not flagged:
        return ""
    for rec in trace.of("dmg"):
        for guid in (rec.get("s"), rec.get("d")):
            if guid in flagged and trace.names.get(guid):
                return canonical_boss(slugify(trace.names[guid]))
    return ""


def encounter_of(trace: Trace) -> str:
    """The fight this trace belongs to, which is what a census counts and what the conf keys on. The
    boss slug says which creature engaged, and for a council or an elder pull that is not the same.

    A slug that is still the map's name joins to no encounter, so the units decide there. Nowhere
    else: a pull filed under its encounter never gets a rename, and the first boss-flagged unit to
    trade damage is often an add or a vehicle rather than the boss the encounter is named after.
    """
    filed = boss_of(trace) or boss_from_path(trace.path)
    if filed_under_map(trace.header.get("map", map_of(trace.path)), filed):
        return engaged_of(trace) or canonical_boss(filed)
    return canonical_boss(filed)


def build_time(trace: Trace) -> datetime.datetime | None:
    ms = trace.header.get("bin")
    if not ms:
        return None
    return datetime.datetime.fromtimestamp(ms / 1000, datetime.timezone.utc)


def head_commit(repo: pathlib.Path) -> tuple[str, datetime.datetime] | None:
    """HEAD's short hash and commit time, or None outside a repo. This is the default thing a build is
    compared against: the question is almost always "is the running binary newer than what I just
    wrote", and HEAD is what was just written."""
    try:
        out = subprocess.run(
            ["git", "-C", str(repo), "log", "-1", "--format=%h %cI"],
            capture_output=True, text=True, timeout=10,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    if out.returncode != 0 or not out.stdout.strip():
        return None
    sha, _, iso = out.stdout.strip().partition(" ")
    try:
        return sha, datetime.datetime.fromisoformat(iso)
    except ValueError:
        return None


def resolve_since(repo: pathlib.Path, since: str | None) -> tuple[str, datetime.datetime] | None:
    """`since` is a commit-ish or an ISO timestamp; absent, HEAD is used."""
    if since is None:
        return head_commit(repo)

    try:
        return "given", datetime.datetime.fromisoformat(since)
    except ValueError:
        pass

    try:
        out = subprocess.run(
            ["git", "-C", str(repo), "log", "-1", "--format=%h %cI", since],
            capture_output=True, text=True, timeout=10,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    if out.returncode != 0 or not out.stdout.strip():
        return None
    sha, _, iso = out.stdout.strip().partition(" ")
    try:
        return sha, datetime.datetime.fromisoformat(iso)
    except ValueError:
        return None


# Roles whose absence changes what a pull proves. A human doing damage is noise; a human tanking or
# healing means the strategy was never asked to do the job.
DECIDING_ROLES = ("tank", "heal")

# A human holding tank or heal always disqualifies: the strategy was not asked to do the job, whatever
# the pull was testing.
ALWAYS_DECIDABLE = {"human-role"}

# The other two only disqualify once the reader says what is under test. Both are true of almost every
# pull otherwise - the loop commits after each pull, so every trace predates HEAD, and every
# normal-mode pull of a hard-mode-capable boss has hard mode off. A disqualifier that fires on the
# whole sample rejects the whole sample and says nothing. Lives here rather than in a view, so one
# definition decides for all of them.
ON_ASK = {"stale-build": "--since", "hardmode-off": "--hardmode"}


def decidable_kinds(since: str | None = None, hardmode: bool = False) -> set[str]:
    """Which warnings count against a pull, given what the reader asked for."""
    kinds = set(ALWAYS_DECIDABLE)
    if since:
        kinds.add("stale-build")
    if hardmode:
        kinds.add("hardmode-off")
    return kinds


def human_roles(trace: Trace) -> dict[str, str]:
    """Each human's name against the role the trace recorded for them.

    Before the recorder learned to read a human's talent tab this was the literal string "human" for
    all of them, so a "human" here means unknown, not a role.
    """
    return {trace.name(guid): trace.roles.get(guid, "?") for guid in trace.humans}


def humans(trace: Trace) -> list[str]:
    """Off trace.humans, not off the roster: someone who zoned in after the header was written appears
    only in a `unit` record, and that is the same person most likely to have picked up a role
    mid-pull. Two derivations of one predicate would disagree exactly where it matters."""
    return sorted(trace.name(guid) for guid in trace.humans)


REPO = pathlib.Path(__file__).resolve().parents[2]


def inspect(trace: Trace, ref: tuple[str, datetime.datetime] | None) -> tuple[dict, list[tuple[str, str]]]:
    """The four disqualifier facts, and what is wrong with them.

    `ref` is already resolved rather than a commit-ish, because a corpus sweep would otherwise shell
    out to git once per trace. Warnings are (kind, text) so a census can group them.
    """
    hdr = trace.header
    cfg = hdr.get("cfg") or {}
    boss = boss_of(trace)
    facts = {
        "built": build_time(trace),
        "boss": boss,
        "encounter": encounter_of(trace),
        "diff": DIFFICULTY.get(hdr.get("diff"), f"difficulty {hdr.get('diff')}"),
        "hardmode": None,
        "humans": humans(trace),
        "cheats": cfg.get("cheats") or "",
        "mapthreads": cfg.get("mapthreads"),
        "age_hours": None,
    }
    warnings: list[tuple[str, str]] = []

    if facts["built"] is not None and ref:
        sha, when = ref
        hours = (facts["built"] - when).total_seconds() / 3600
        facts["age_hours"] = hours
        if hours < 0:
            warnings.append(
                ("stale-build", f"binary predates {sha} by {abs(hours):.1f}h: it cannot contain that code")
            )

    hard = cfg.get("hardmode") or {}
    key = facts["encounter"]
    if key in hard:
        facts["hardmode"] = bool(hard[key])
        if not hard[key]:
            warnings.append(
                ("hardmode-off", f"{key} hard mode was OFF: a kill here is not a hard-mode kill")
            )

    # A human doing damage among 24 bots barely dents a raid strategy; a human tanking or healing
    # means the strategy never played the role the pull was meant to test, which is what invalidated
    # the Mimiron pull. Older traces record every human's role as the literal "human", so for those
    # the question cannot be answered and the warning stays informational.
    played = {name: role for name, role in human_roles(trace).items() if role in DECIDING_ROLES}
    if played:
        held = ", ".join(f"{name} ({role})" for name, role in sorted(played.items()))
        warnings.append(("human-role", f"a human held a role the strategy was meant to play: {held}"))
    elif facts["humans"]:
        blind = any(role == "human" for role in human_roles(trace).values())
        detail = " and the trace predates real roles for humans" if blind else ""
        warnings.append((
            "human-in-raid",
            f"{len(facts['humans'])} human(s) in the raid{detail}",
        ))

    return facts, warnings


def show_validity(trace: Trace, since: str | None = None, hardmode: bool = False) -> int:
    """Prints the banner. Returns the number of things that disqualify the pull, so a caller can skip
    it without parsing the text. Informational warnings print and do not count."""
    ref = resolve_since(REPO, since)
    decisive = decidable_kinds(since, hardmode)
    facts, warnings = inspect(trace, ref)
    # Naming a ref is what makes the build count, so a typo in one can't quietly pass the pull.
    if since and ref is None:
        warnings.append(("stale-build", f"cannot resolve --since {since}: the build was not checked"))

    if facts["built"] is None:
        print("build   unknown (trace predates schema v11)")
    else:
        line = f"build   {facts['built']:%Y-%m-%d %H:%M} UTC"
        hours = facts["age_hours"]
        if hours is not None:
            line += (f"  <-- {abs(hours):.1f}h OLDER than {ref[0]}" if hours < 0
                     else f"  ({hours:.1f}h after {ref[0]})")
        print(line)

    mode = facts["diff"]
    if facts["hardmode"] is not None:
        mode += f", {facts['encounter']} hard mode {'ON' if facts['hardmode'] else 'OFF'}"
    print(f"mode    {mode}")

    if facts["humans"]:
        print(f"humans  {len(facts['humans'])} in the raid: {', '.join(facts['humans'])}")
    if facts["cheats"]:
        print(f"cheats  {facts['cheats']}")
    if facts["mapthreads"]:
        print(f"threads MapUpdate.Threads = {facts['mapthreads']}")

    for kind, warning in warnings:
        if kind in decisive:
            print(f"  !!    {warning}")
        else:
            ask = ON_ASK.get(kind)
            note = f"   (pass {ask} to make this decide)" if ask else "   (informational)"
            print(f"  --    {warning}{note}")
    if not any(kind in decisive for kind, _ in warnings) and facts["built"] is not None:
        print("  ok    nothing disqualifying")
    print()

    return sum(1 for kind, _ in warnings if kind in decisive)

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

from .encounter import boss_of, encounter_of
from .paths import REPO
from .trace import Trace, roster_guids

# Raid difficulty ids. Only raid maps are tracked unless Obs.Maps names one, so these are the labels
# that apply; a 5-man would read 0/1 as normal/heroic instead.
DIFFICULTY = {0: "10-man normal", 1: "25-man normal", 2: "10-man heroic", 3: "25-man heroic"}


def build_time(trace: Trace) -> datetime.datetime | None:
    ms = trace.header.get("bin")
    if not ms:
        return None
    return datetime.datetime.fromtimestamp(ms / 1000, datetime.timezone.utc)


def _commit(repo: pathlib.Path, ref: str) -> tuple[str, datetime.datetime] | None:
    """A ref's short hash and commit time, or None when git cannot resolve it."""
    try:
        out = subprocess.run(
            ["git", "-C", str(repo), "log", "-1", "--format=%h %cI", ref],
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


def head_commit(repo: pathlib.Path) -> tuple[str, datetime.datetime] | None:
    """HEAD's short hash and commit time, or None outside a repo. This is the default thing a build is
    compared against: the question is almost always "is the running binary newer than what I just
    wrote", and HEAD is what was just written."""
    return _commit(repo, "HEAD")


def resolve_since(repo: pathlib.Path, since: str | None) -> tuple[str, datetime.datetime] | None:
    """`since` is a commit-ish or an ISO timestamp; absent, HEAD is used. A timestamp without an offset
    is local time, the way git reads one."""
    if since is None:
        return head_commit(repo)

    try:
        given = datetime.datetime.fromisoformat(since)
    except ValueError:
        return _commit(repo, since)
    # build and pull stamps are aware, and comparing a naive time with either raises
    return "given", given if given.tzinfo else given.astimezone()


# Roles whose absence changes what a pull proves. A human doing damage is noise; a human tanking or
# healing means the strategy was never asked to do the job.
DECIDING_ROLES = ("tank", "heal")

# A human holding tank or heal always disqualifies: the strategy was not asked to do the job, whatever
# the pull was testing. So does a raid lying dead when the trace opened, which is nobody pulling at all.
ALWAYS_DECIDABLE = {"human-role", "raid-dead"}

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


def alive_at_open(trace: Trace) -> tuple[int, int] | None:
    """(alive, sampled) roster members in the first snapshot of the pull, or None without one."""
    roster = roster_guids(trace)
    for snap in trace.of("snap"):
        if snap["t"] < 0:
            continue
        rows = [row for row in snap.get("u", []) if row[0] in roster]
        if not rows:
            return None
        return sum(1 for row in rows if row[5] > 0), len(rows)
    return None


def inspect(trace: Trace, ref: tuple[str, datetime.datetime] | None) -> tuple[dict, list[tuple[str, str]]]:
    """The disqualifier facts, and what is wrong with them.

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

    # More than half dead, the same line the recorder calls a wipe at. One Yogg trace opened 45 s after a
    # wipe with only the human up, who then used .die, and read as a one-death wipe.
    alive = alive_at_open(trace)
    if alive and alive[0] * 2 < alive[1]:
        warnings.append((
            "raid-dead",
            f"only {alive[0]} of {alive[1]} raiders alive when the trace opened: nobody pulled",
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

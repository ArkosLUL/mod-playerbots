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

from obstrace import Trace

# Raid difficulty ids. Only raid maps are tracked unless Obs.Maps names one, so these are the labels
# that apply; a 5-man would read 0/1 as normal/heroic instead.
DIFFICULTY = {0: "10-man normal", 1: "25-man normal", 2: "10-man heroic", 3: "25-man heroic"}

# hdr.cfg.hardmode is keyed by the conf's own boss names, which are not always the slug the trace
# names itself with - that comes from the DBC encounter name. Only the ones that differ need an entry.
BOSS_ALIASES = {
    "assembly-of-iron": "iron-assembly",
    "xt-002-deconstructor": "xt-002",
    "general-vezax": "vezax",
    "yogg-saron-": "yogg-saron",
}


def boss_of(trace: Trace) -> str:
    """The corrected boss slug. hdr.boss is line one of an append-only file, so a session that opened
    before its boss engaged still carries the map name there; the rename record is authoritative."""
    renames = [p for p in trace.of("pull") if p.get("src") == "rename"]
    if renames:
        return str(renames[-1].get("boss") or "")
    return str(trace.header.get("boss") or "")


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


def humans(trace: Trace) -> list[str]:
    """Off trace.humans, not off the roster: someone who zoned in after the header was written appears
    only in a `unit` record, and that is the same person most likely to have picked up a role
    mid-pull. Two derivations of one predicate would disagree exactly where it matters."""
    return sorted(trace.name(guid) for guid in trace.humans)


def show_validity(trace: Trace, since: str | None = None) -> int:
    """Prints the banner. Returns the number of things that make this trace weak evidence, so a batch
    can skip a pull without parsing the text."""
    hdr = trace.header
    repo = pathlib.Path(__file__).resolve().parents[2]
    warnings: list[str] = []

    built = build_time(trace)
    if built is None:
        print("build   unknown (trace predates schema v11)")
    else:
        line = f"build   {built:%Y-%m-%d %H:%M} UTC"
        ref = resolve_since(repo, since)
        if ref:
            sha, when = ref
            delta = built - when
            hours = delta.total_seconds() / 3600
            if delta.total_seconds() < 0:
                line += f"  <-- {abs(hours):.1f}h OLDER than {sha}"
                warnings.append(f"binary predates {sha} by {abs(hours):.1f}h: it cannot contain that code")
            else:
                line += f"  ({hours:.1f}h after {sha})"
        print(line)

    diff = DIFFICULTY.get(hdr.get("diff"), f"difficulty {hdr.get('diff')}")
    boss = boss_of(trace)
    mode = diff
    cfg = hdr.get("cfg") or {}
    hard = cfg.get("hardmode") or {}
    key = BOSS_ALIASES.get(boss, boss)
    if key in hard:
        state = "ON" if hard[key] else "OFF"
        mode += f", {key} hard mode {state}"
        if not hard[key]:
            warnings.append(f"{key} hard mode was OFF: a kill here is not a hard-mode kill")
    print(f"mode    {mode}")

    present = humans(trace)
    if present:
        print(f"humans  {len(present)} in the raid: {', '.join(present)}")
        warnings.append(
            f"{len(present)} human(s) in the raid - any role they held was not played by the strategy"
        )

    if cfg.get("cheats"):
        print(f"cheats  {cfg['cheats']}")
    if cfg.get("mapthreads"):
        print(f"threads MapUpdate.Threads = {cfg['mapthreads']}")

    for warning in warnings:
        print(f"  !!    {warning}")
    if not warnings and built is not None:
        print("  ok    nothing disqualifying")
    print()

    return len(warnings)

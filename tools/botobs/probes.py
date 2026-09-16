"""What the encounter said about its own decisions, and what kept changing its mind.

The raid code publishes 125 probe keys and, before this, two of them were read. The rest were printed
raw or counted. A key does not need its own scorer to be legible - what it needs is to be told apart
from the other shapes, because the emitters mean different things:

    ObsValue                 one value for the whole raid, deduped globally, attributed to whichever
                             bot happened to be current when it changed - so the `g` on the record is
                             noise, not the subject
    ObsGuidMap / ObsGuidSet  one value per guid, deduped per guid
    NoteDerived              one value per bot, deduped per bot
    RaidObs::Note            no dedup at all: an event, not a state

Shape is read out of the C++ declaration rather than guessed from the rows. Guessing looked possible
and is not: a latch written by twenty different bots and a per-bot key written by twenty bots produce
the same record stream, and the only thing that separates them is where the dedup happened.

Two streams that are not notes get read the same way, because "what keeps changing its mind" does not
care which record carries it: `move.by`, the action that owns each accepted move, and `act.won`, the
action that won the tick. The Mimiron phase-1 flip-flop lived in the first of those.
"""
from __future__ import annotations

import collections
import functools
import pathlib
import re

from obstrace import Trace, clock
from validity import encounter_of as trace_encounter

# The module's own src/, two levels up from tools/botobs/.
SRC_ROOT = pathlib.Path(__file__).resolve().parents[2] / "src"

LATCH = "latch"      # one raid-wide value
HOLDER = "holder"    # one value per guid
EVENT = "event"      # every call emits, so there is no state to hold

TIMELINE_STEPS = 12
# Flipping pairs promoted to their own metric per action stream, worst first.
PAIR_METRICS = 10

MOVE_STREAM = "move.by"
ACT_STREAM = "act.won"

# First pattern to match a key wins, so a container declaration beats a bare call site. `\bNote\s*\(`
# does not match NoteDerived/NoteAssignment/NoteHazard: those have a letter where the paren must be.
DECLARATIONS: tuple[tuple[re.Pattern, str], ...] = (
    (re.compile(r'ObsValue\s*<[^>]*>\s*\w+\s*[{(]\s*"([a-z0-9._]+)"'), LATCH),
    (re.compile(r'ObsGuidMap\s*<[^>]*>\s*\w+\s*[{(]\s*"([a-z0-9._]+)"'), HOLDER),
    (re.compile(r'ObsGuidSet\s+\w+\s*[{(]\s*"([a-z0-9._]+)"'), HOLDER),
    (re.compile(r'NoteDerived\s*\([^,]*,\s*"([a-z0-9._]+)"'), HOLDER),
    (re.compile(r'(?:RaidObs::)?\bNote\s*\([^,]*,\s*"([a-z0-9._]+)"'), EVENT),
)


@functools.lru_cache(maxsize=4)
def declared_keys(root: pathlib.Path = SRC_ROOT) -> dict[str, tuple[str, str]]:
    """Every probe key the source declares, as key -> (shape, "file:line").

    Parsed rather than listed because a hand-kept list is what CONSUMED_KEYS was, and it was already
    wrong about one of its two entries.
    """
    found: dict[str, tuple[str, str]] = {}
    for path in sorted(root.rglob("*.h")) + sorted(root.rglob("*.cpp")):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if "Obs" not in text and "Note" not in text:
            continue
        # Whole text, not line by line: a call wrapped after its opening paren puts the key on the
        # next line, and reading one line at a time misses it. `yogg.deathray` is written that way.
        for pattern, kind in DECLARATIONS:
            for match in pattern.finditer(text):
                key = match.group(1)
                if key not in found:
                    found[key] = (kind, f"{path.name}:{text.count(chr(10), 0, match.start()) + 1}")
    return found


def encounter_of(key: str) -> str:
    return key.split(".", 1)[0] if "." in key else key


def prefix_matches_boss(prefix: str, boss: str) -> bool:
    """Whether a key prefix names the boss a trace fought.

    Prefixes are written three ways and all are regular: `thorim`/`mimiron`/`algalon` spell the slug
    out with the punctuation dropped, `fl` is the slug's initials, and `yogg` is its first word.
    Deriving all three beats a fourth hand-mirrored prefix table - there are already two of those and
    they drift. Without the first-word form no `yogg.*` key matches its boss, because `yoggsaron`
    and `ys` are all the other two forms derive.
    """
    words = [word for word in re.split(r"[^a-z0-9]+", boss.lower()) if word]
    flat = "".join(words)
    initials = "".join(word[0] for word in words)
    return prefix in (flat, initials, words[0] if words else "")


GUID_TOKEN = re.compile(r"\b\d{4,}\b")


def resolve_guids(trace: Trace, text: str) -> str:
    """Name every guid inside a payload, not only a payload that is nothing but a guid.

    Four digits minimum: a payload of "0" or "5" is a counter or a phase, and a player's guid key is
    its bare counter, so a shorter number is far more likely to be a slot index than a unit.
    """
    def swap(match: re.Match) -> str:
        guid = int(match.group(0))
        return trace.name(guid) if guid in trace.names else match.group(0)

    return GUID_TOKEN.sub(swap, text)


Window = tuple[int, int]


def latch_spans(trace: Trace, key: str, end: int = 1 << 62) -> list[tuple[str, int, int]]:
    """`(value, start, stop)` for every value a latch key held, in order.

    A latch stream is change-only, so a value runs until the next mark and the last one runs to
    `end`. Pass the trace's last stamp for a span you mean to print; the default is only useful for
    testing whether a time falls inside one.
    """
    marks = sorted((rec["t"], str(rec.get("txt", ""))) for rec in trace.of("note")
                   if rec.get("k") == key)
    spans = []
    for index, (when, held) in enumerate(marks):
        stop = marks[index + 1][0] if index + 1 < len(marks) else end
        spans.append((held, when, stop))
    return spans


def latch_windows(trace: Trace, key: str, value: str) -> list[Window]:
    """The spans where a latch key held `value`.

    Churn has to be scoped or a late-phase storm buries an early-phase defect: Mimiron's phase-1
    flip-flop reads 57 A-B-A inside phase 1 and disappears into ~700 whole-pull ones.
    """
    return [(start, stop) for held, start, stop in latch_spans(trace, key) if held == value]


def parse_during(spec: str) -> tuple[str, str]:
    key, _, value = spec.partition("=")
    return key, value


def in_windows(when: int, windows: list[Window] | None) -> bool:
    return windows is None or any(start <= when < end for start, end in windows)


class Series:
    """One stream's records, and the churn they add up to."""

    def __init__(self, key: str, kind: str, rows: list[tuple[int, int, str]]):
        self.key = key
        self.kind = kind
        self.rows = rows
        self.holders = {guid for _, guid, _ in rows}
        self.values = {value for _, _, value in rows}

        # A latch is one value for the raid, so its trajectory is the global record order. Everything
        # else is deduped per guid, so each guid carries its own.
        if kind == LATCH:
            tracks: dict[int, list] = {0: rows}
        else:
            grouped: dict[int, list] = collections.defaultdict(list)
            for row in rows:
                grouped[row[1]].append(row)
            tracks = grouped

        self.changes = 0
        self.flips = 0
        # A-B-A counted per unordered pair, so the ranking can name the two rules rather than only
        # saying the stream is noisy.
        self.flip_pairs: collections.Counter = collections.Counter()
        holds: list[int] = []
        for track in tracks.values():
            seen: list[tuple[int, str]] = []
            for when, _, value in track:
                if seen and seen[-1][1] == value:
                    continue
                if seen:
                    self.changes += 1
                    holds.append(when - seen[-1][0])
                    # A-B-A: the value went back to what it held before last. One of these is a
                    # retune; a hundred is two rules fighting over the same bot.
                    if len(seen) >= 2 and seen[-2][1] == value:
                        self.flips += 1
                        self.flip_pairs[tuple(sorted((value, seen[-1][1])))] += 1
                seen.append((when, value))
        self.mean_hold_ms = sum(holds) / len(holds) if holds else 0

    def rate_per_min(self, minutes: float) -> float:
        return self.changes / minutes if minutes > 0 else 0.0

    def pair_flips(self, first: str, second: str) -> int:
        """A-B-A restricted to one pair of values, which is how a specific fight between two rules is
        counted - `follow` against `mimiron arc spread action`, say."""
        wanted = {first, second}
        tracks: dict[int, list[str]] = collections.defaultdict(list)
        for _, guid, value in self.rows:
            if value in wanted:
                tracks[0 if self.kind == LATCH else guid].append(value)
        total = 0
        for track in tracks.values():
            runs = [v for i, v in enumerate(track) if i == 0 or v != track[i - 1]]
            total += max(len(runs) - 2, 0)
        return total


def duration_min(trace: Trace, windows: list[Window] | None = None) -> float:
    """Pull length in minutes, from the last sample rather than the last record: the close writes
    records after combat ends and they would stretch every rate."""
    snaps = trace.of("snap")
    last = snaps[-1]["t"] if snaps else 0
    if windows:
        covered = sum(min(end, last) - start for start, end in windows if start < last)
        return max(covered, 0) / 60000.0
    return max(last, 0) / 60000.0


def collect(trace: Trace, prefix: str | None = None,
            windows: list[Window] | None = None) -> list[Series]:
    """Every stream in the trace: one Series per note key, plus the two action streams."""
    declared = declared_keys()
    rows: dict[str, list[tuple[int, int, str]]] = collections.defaultdict(list)

    for rec in trace.of("note"):
        key = rec.get("k", "")
        if in_windows(rec.get("t", 0), windows):
            rows[key].append((rec.get("t", 0), rec.get("g", 0), str(rec.get("txt", ""))))

    # An accepted move names the action that owns it, and the OK verdict names the action that won the
    # tick. Both are per-bot state that thrashes exactly the way an assignment does.
    for rec in trace.of("move"):
        if rec.get("ok") and in_windows(rec.get("t", 0), windows):
            rows[MOVE_STREAM].append((rec["t"], rec.get("g", 0), rec.get("by") or "?"))
    for rec in trace.of("act"):
        if rec.get("vd") == "OK" and in_windows(rec.get("t", 0), windows):
            rows[ACT_STREAM].append((rec["t"], rec.get("g", 0), rec.get("a") or "?"))

    # A key the parse missed is read as per-guid: that is the reading that stays correct if the guid
    # turns out not to matter, where the reverse silently merges every bot's trajectory into one.
    return [
        Series(key, declared.get(key, (HOLDER, ""))[0], values)
        for key, values in rows.items()
        if not prefix or key.startswith(prefix)
    ]


def emitted_keys(trace: Trace) -> set[str]:
    return {rec.get("k", "") for rec in trace.of("note")}


def silent_keys(emitted: set[str], boss: str) -> list[tuple[str, str, str]]:
    """Keys declared for `boss` that `emitted` does not contain.

    Over one pull this is mostly "did not happen" - a wipe before anyone reached a corner leaves
    `fl.corner` silent for an honest reason. Over every pull of a boss it is a defect: `fl.frozen` is
    written each pass and keyed on a vehicle guid, and NoteAssignment resolves its key with FindPlayer
    and drops the record when it is not a player, so it has never once reached a trace. Pass the union
    across a selection to tell the two apart.
    """
    return [
        (key, kind, where)
        for key, (kind, where) in sorted(declared_keys().items())
        if key not in emitted and prefix_matches_boss(encounter_of(key), boss)
    ]


def probe_metrics(trace: Trace, windows: list[Window] | None = None) -> dict[str, float]:
    """Churn per stream, for the corpus comparison. One metric per key rather than a single roll-up:
    a raid-wide churn number cannot say which rule started fighting."""
    minutes = duration_min(trace, windows)
    metrics: dict[str, float] = {}
    for series in collect(trace, windows=windows):
        if series.kind == EVENT:
            continue
        metrics[f"churn.{series.key}"] = round(series.rate_per_min(minutes), 2)
        if series.flips:
            # Per minute like everything else: a ten-minute kill and a one-minute wipe are not
            # comparable on a raw A-B-A count, and the longer pull always looks worse.
            metrics[f"flip.{series.key}"] = round(series.flips / minutes, 2) if minutes else 0.0

        # Which two rules are fighting, for the action streams only. The aggregate says a stream is
        # noisy; only the pair says what to go and fix, and the pair is what recurs across pulls.
        if series.key in (MOVE_STREAM, ACT_STREAM):
            for (first, second), count in series.flip_pairs.most_common(PAIR_METRICS):
                metrics[f"flip.{series.key}:{first} <-> {second}"] = round(count / minutes, 2)
    return metrics


def show_timeline(trace: Trace, series: Series) -> None:
    print(f"\n{series.key}  ({series.kind}, {len(series.rows)} rows)\n")

    if series.flip_pairs:
        print("  flipped back and forth between:")
        for (first, second), count in series.flip_pairs.most_common(8):
            print(f"    {count:5d}  {resolve_guids(trace, first)} <-> {resolve_guids(trace, second)}")
        print()

    if series.kind == LATCH:
        held = None
        for when, _, value in series.rows:
            if value == held:
                continue
            held = value
            print(f"{clock(when):>9}  {resolve_guids(trace, value)}")
        return

    by_holder: dict[int, list] = collections.defaultdict(list)
    for row in series.rows:
        by_holder[row[1]].append(row)
    for guid, track in sorted(by_holder.items(), key=lambda kv: trace.name(kv[0])):
        held = None
        steps = []
        for when, _, value in track:
            if value == held:
                continue
            held = value
            steps.append(f"{clock(when)} {resolve_guids(trace, value)}")
        # A thrashing stream has hundreds of steps per bot and the tail says nothing the pair ranking
        # above has not already said. Keep the head, which is where it started going wrong.
        if len(steps) > TIMELINE_STEPS:
            steps = steps[:TIMELINE_STEPS] + [f"... {len(steps) - TIMELINE_STEPS} more"]
        print(f"  {trace.name(guid):<20} {' -> '.join(steps)}")


def show_probes(trace: Trace, prefix: str | None = None, during: str | None = None) -> int:
    windows = None
    if during:
        key, value = parse_during(during)
        windows = latch_windows(trace, key, value)
        if not windows:
            print(f"{key} never held {value!r} in this trace")
            return 1

    series = collect(trace, prefix, windows)
    series = [s for s in series if s.rows]
    if not series:
        scope = f" matching {prefix!r}" if prefix else ""
        print(f"no probe keys{scope} in this trace")
        return 0

    # One key asked for by name gets the whole trajectory; a prefix or nothing gets the ranking.
    if prefix and len(series) == 1:
        show_timeline(trace, series[0])
        return 0

    minutes = duration_min(trace, windows)
    scope = f" while {during}" if during else ""
    print(f"streams over {minutes:.1f} min{scope}, worst churn first\n")
    print(f"  {'key':<30} {'shape':<7} {'rows':>6} {'held':>5} {'vals':>5} "
          f"{'changes':>8} {'/min':>7} {'flips':>6} {'hold':>7}")
    for entry in sorted(series, key=lambda s: (-s.flips, -s.changes)):
        hold = f"{entry.mean_hold_ms / 1000:.1f}s" if entry.mean_hold_ms else "-"
        print(f"  {entry.key:<30} {entry.kind:<7} {len(entry.rows):6d} {len(entry.holders):5d} "
              f"{len(entry.values):5d} {entry.changes:8d} {entry.rate_per_min(minutes):7.1f} "
              f"{entry.flips or '-':>6} {hold:>7}")

    print("\n  flips are A-B-A: a value that went back to what it held before last, which is what two"
          "\n  rules fighting over one bot looks like from outside. Scope with --during KEY=VALUE:"
          "\n  a late-phase storm buries an early-phase defect in the whole-pull number.")

    silent = silent_keys(emitted_keys(trace), trace_encounter(trace))
    if silent:
        print(f"\ndeclared for this boss, silent in this pull - {len(silent)} key(s)\n")
        for key, kind, where in silent:
            print(f"  {key:<30} {kind:<7} {where}")
        print("\n  one pull is weak evidence: run batch.py --probes to see which of these is silent"
              "\n  in every pull of this boss, which is the shape of a probe the recorder is dropping")
    return 0

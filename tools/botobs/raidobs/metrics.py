"""One pull reduced to named numbers, so two selections of pulls can be compared.

Every other view answers "what happened in this pull". None of them answers "did the change help",
which is the question the whole build-and-pull loop exists to serve - it was answered by reading two
reports side by side, which is how twelve Lightning Charge shelters got solved against three pulls.

Everything here is a **rate per minute** except the `cov.` bucket counts, which are node counts and do
not scale with how long the pull lasted. A 30-second wipe and a 10-minute kill are only comparable
once the counts are divided by the clock.
"""
from __future__ import annotations

import statistics

from .coverage import coverage_metrics
from .probes import duration_min, probe_metrics
from .space import clump_histogram
from .stuck import stall_windows
from .trace import Trace, combat_deaths

KEY_WIDTH = 54
STALL_MIN_MS = 6000
CLUMP_YARDS = 10.0


def trace_metrics(trace: Trace) -> dict[str, float]:
    minutes = duration_min(trace) or 1e-9

    metrics: dict[str, float] = {
        "mins": round(minutes, 2),
        "deaths.per_min": round(len(combat_deaths(trace)) / minutes, 2),
    }

    # `snap.u[11]` is cumulative damage dealt, kept up to date by AccrueDamageDealt with its own
    # pet-to-owner attribution. A change that fixes positioning and quietly costs the raid a third of
    # its damage should not read as a win.
    # Cumulative per unit and monotonic, so the last value each one reached is its total, and the
    # raid's is the sum of those - a corpse stops being sampled and must still count what it did.
    dealt: dict[int, int] = {}
    for snap in trace.of("snap"):
        for row in snap.get("u", []):
            if len(row) > 11 and row[11]:
                dealt[row[0]] = max(dealt.get(row[0], 0), row[11])
    if dealt:
        metrics["dps.raid"] = round(sum(dealt.values()) / (minutes * 60.0), 1)

    stalls = stall_windows(trace, STALL_MIN_MS)
    stalled_ms = sum(window["end"] - window["start"] for window in stalls)
    metrics["stall.windows_per_min"] = round(len(stalls) / minutes, 2)
    metrics["stall.sec_per_min"] = round(stalled_ms / 1000 / minutes, 1)

    histogram = clump_histogram(trace, CLUMP_YARDS)
    frames = sum(histogram.values())
    if frames:
        # The median frame's largest circle, which is the number an AoE would have caught. A mean
        # would be dragged around by the pre-pull stack.
        sizes = [size for size, count in histogram.items() for _ in range(count)]
        metrics["clump.median"] = float(statistics.median(sizes))

    metrics.update(coverage_metrics(trace))
    metrics.update(probe_metrics(trace))
    return metrics


class Side:
    """One selection's value for every metric, with the spread that says whether a move is real."""

    def __init__(self, label: str, rows: list[dict]):
        self.label = label
        self.n = len(rows)
        self.values: dict[str, list[float]] = {}
        for row in rows:
            for key, value in (row.get("metrics") or {}).items():
                self.values.setdefault(key, []).append(value)

    def stat(self, key: str) -> tuple[float, float, float] | None:
        """Median, low and high. A metric missing from a pull is missing, not zero: a key that never
        fired and a key whose encounter was not reached read the same as a zero and are not."""
        seen = self.values.get(key)
        if not seen:
            return None
        return statistics.median(seen), min(seen), max(seen)

    def pulls_with(self, key: str) -> int:
        return len(self.values.get(key, ()))


# The smallest change worth calling a move, as a share of the larger side. Ranges can sit a hair apart
# on three pulls and mean nothing; below this the sample cannot tell a shift from the roster.
MIN_EFFECT = 0.10


def compare(before: Side, after: Side, limit: int = 40) -> list[dict]:
    """Every metric either side carries, worst-moved first.

    `moved` means the two ranges do not overlap at all. That is a deliberately blunt test: these are
    three to thirty pulls with different RNG, rosters and durations, and anything finer than "the
    ranges are disjoint" reads significance into a sample that cannot carry it.
    """
    findings = []
    for key in sorted(set(before.values) | set(after.values)):
        left, right = before.stat(key), after.stat(key)
        # Another encounter's probe leaks into a pull and reads zero on both sides. It is not a
        # finding, and there are enough of them to push the real rows off the screen.
        if left and right and not any(left) and not any(right):
            continue
        if left is None or right is None:
            # A stream only one side carries is usually that side's roster, not the change: one
            # hunter in one pull invents `explosive shot <-> steady shot`. Worse, the sides are rarely
            # the same size, so with twelve pulls before and three after anything occasional shows up
            # on the bigger side and nowhere else. Ask for it in most of the pulls it could have
            # appeared in, and for it to be non-zero there at all.
            side = after if left is None else before
            stat = right if left is None else left
            if side.n > 1 and side.pulls_with(key) * 2 <= side.n:
                continue
            if not any(stat):
                continue
            findings.append({"key": key, "before": left, "after": right, "moved": True,
                             "delta": None, "only": "after" if left is None else "before"})
            continue
        # The same bar a one-sided stream has to clear. A phase only one of two pulls reached leaves that
        # pull's value alone on its side, and a single value is a range nothing overlaps.
        thin = any(side.n > 1 and side.pulls_with(key) * 2 <= side.n for side in (before, after))
        delta = right[0] - left[0]
        # Disjoint ranges, but also a gap big enough to see: the printed row carries two decimals, and
        # calling 0.14 against 0.13 a move puts noise at the top of a list read for signal.
        scale = max(abs(left[0]), abs(right[0]))
        moved = (not thin
                 and (left[2] < right[1] or right[2] < left[1])
                 and abs(delta) >= MIN_EFFECT * scale
                 and f"{left[0]:.2f}" != f"{right[0]:.2f}")
        findings.append({"key": key, "before": left, "after": right, "moved": moved,
                         "delta": delta, "only": None})

    findings.sort(key=lambda f: (
        not f["moved"],
        -abs(f["delta"] or 0) / (abs(f["before"][0]) if f["before"] and f["before"][0] else 1),
    ))
    return findings[:limit]


def show_compare(before: Side, after: Side) -> int:
    print(f"\n{before.label}: {before.n} pull(s)   ->   {after.label}: {after.n} pull(s)")
    if not before.n or not after.n:
        print("one side is empty, nothing to compare")
        return 1

    def cell(stat: tuple[float, float, float] | None) -> str:
        if stat is None:
            return f"{'-':>20}"
        return f"{stat[0]:7.2f} [{stat[1]:.2f}-{stat[2]:.2f}]".rjust(20)

    print(f"\n  {'metric':<{KEY_WIDTH}} {'before':>20} {'after':>20}   moved")
    for finding in compare(before, after):
        mark = "only " + finding["only"] if finding["only"] else ("yes" if finding["moved"] else "")
        key = finding["key"]
        if len(key) > KEY_WIDTH:
            key = key[: KEY_WIDTH - 1] + "~"
        print(f"  {key:<{KEY_WIDTH}} {cell(finding['before'])} {cell(finding['after'])}   {mark}")

    print("\n  median [min-max] per side. `moved` means the two ranges do not overlap at all - with"
          "\n  this many pulls that is the only claim the sample supports.")
    return 0

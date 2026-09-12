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

from coverage import coverage_metrics
from obstrace import Trace
from probes import duration_min, probe_metrics
from views import clump_histogram, stall_windows

KEY_WIDTH = 54
STALL_MIN_MS = 6000
CLUMP_YARDS = 10.0


def trace_metrics(trace: Trace) -> dict[str, float]:
    minutes = duration_min(trace) or 1e-9

    metrics: dict[str, float] = {
        "mins": round(minutes, 2),
        "deaths.per_min": round(len(trace.of("death")) / minutes, 2),
    }

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
            findings.append({"key": key, "before": left, "after": right, "moved": True,
                             "delta": None, "only": "after" if left is None else "before"})
            continue
        moved = left[2] < right[1] or right[2] < left[1]
        delta = right[0] - left[0]
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
        return f"{stat[0]:7.2f} [{stat[1]:.1f}-{stat[2]:.1f}]".rjust(20)

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

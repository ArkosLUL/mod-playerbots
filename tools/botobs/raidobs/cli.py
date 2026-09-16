"""What the readers' main() functions share: opening the trace they were handed, and the boss
readers' one-flag-per-section shape."""
from __future__ import annotations

import argparse
import pathlib
import sys

from .trace import Trace


def open_trace(path: pathlib.Path) -> Trace | None:
    if not path.is_file():
        print(f"no such trace: {path}", file=sys.stderr)
        return None
    return Trace(path)


def run_sections(doc: str, sections, banner=None) -> int:
    """A boss reader's whole main(). `sections` holds `(flag, help, show)` rows. No flag runs every
    row in table order, and a blank line goes between sections and after the banner."""
    parser = argparse.ArgumentParser(description=doc.splitlines()[0])
    parser.add_argument("file", type=pathlib.Path)
    for flag, text, _ in sections:
        parser.add_argument(f"--{flag}", action="store_true", help=text)
    args = parser.parse_args()

    trace = open_trace(args.file)
    if trace is None:
        return 1

    picked = [getattr(args, flag) for flag, _, _ in sections]
    every = not any(picked)
    printed = banner is not None
    if banner is not None:
        banner(trace)
    for wanted, (_, _, show) in zip(picked, sections):
        if not (wanted or every):
            continue
        if printed:
            print()
        show(trace)
        printed = True
    return 0

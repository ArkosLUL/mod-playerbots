"""The trace files on disk: which ones a selection means, and when each was pulled.
"""
from __future__ import annotations

import datetime
import pathlib

from .encounter import boss_from_path, boss_key, canonical_boss, filed_under_map, map_of, recover_boss


def pull_time(path: pathlib.Path) -> datetime.datetime | None:
    """When the pull was recorded, off the epoch the recorder puts in the file name.

    Weaker evidence than `hdr.bin`, which says what the binary was: this only says when you played,
    and assumes you rebuilt before you pulled. It is the only thing the 118 pre-v11 traces carry.
    """
    stem = path.stem.rsplit("_", 1)
    if len(stem) != 2 or not stem[1].isdigit():
        return None
    return datetime.datetime.fromtimestamp(int(stem[1]), datetime.timezone.utc)


def find_traces(roots, boss: str | None = None) -> list[pathlib.Path]:
    """Trace paths under `roots`, newest first, deduplicated by resolved path.

    Filtering on the filename means a boss sweep never opens the other files; the corpus runs to
    1.4 GB and individual traces reach 21 MB. The one exception is a file still named after its map,
    which is what a pull the recorder never renamed looks like. Only those get opened, to see who
    engaged.
    """
    wanted = canonical_boss(boss) if boss else None
    found: dict[pathlib.Path, float] = {}
    for root in roots:
        root = pathlib.Path(root)
        candidates = [root] if root.is_file() else sorted(root.glob("*.ndjson"))
        for path in candidates:
            if wanted and boss_key(path) != wanted and not (
                    filed_under_map(map_of(path), boss_from_path(path))
                    and recover_boss(path) == wanted):
                continue
            resolved = path.resolve()
            if resolved not in found:
                found[resolved] = path.stat().st_mtime
    return sorted(found, key=lambda p: -found[p])

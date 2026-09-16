"""Where the module checkout and its traces live, worked out once so moving a file cannot break it."""
from __future__ import annotations

import pathlib

REPO = pathlib.Path(__file__).resolve().parents[3]
SRC_ROOT = REPO / "src"
RAID_ROOT = SRC_ROOT / "Ai" / "Raid"
# env/ belongs to the server checkout two levels up, not to this module
LOG_ROOT = REPO.parents[1] / "env" / "dist" / "logs" / "botobs"

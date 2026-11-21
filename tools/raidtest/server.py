"""The live server side of a cycle: the lock, the dirty-tree guard, the capped rebuild and restart. Every
external command goes through a `run` callable shaped like subprocess.run, so tests can stand in for git
and docker.
"""
from __future__ import annotations

import datetime
import os
import pathlib
import socket
import subprocess
import time
from typing import Callable

CONTAINER = "ac-worldserver"
COMPOSE_FILES = (
    "docker-compose.yml",
    "docker-compose.override.yml",
    ".agents/plans/worldserver-build/compose.build.yml",
)
READY_LINE = "worldserver-daemon) ready"
LOCK_DIR = pathlib.Path("G:/DevStuff/GitHub/.wave-loop/locks/server")
OWNER_FILE = "owner"

Run = Callable[..., subprocess.CompletedProcess]


class Stop(Exception):
    """A failure that ends a cycle or a whole window; `reason` goes into the report."""

    def __init__(self, reason: str):
        super().__init__(reason)
        self.reason = reason


class LockHeld(Exception):
    def __init__(self, path: pathlib.Path, owner: str):
        super().__init__(f"server lock {path} is held by: {owner or '(no owner file)'}")
        self.owner = owner


class ServerLock:
    """mkdir is atomic, so whoever creates the directory owns the server until it's removed."""

    def __init__(self, path: pathlib.Path, owner: str):
        self.path = pathlib.Path(path)
        self.owner = owner
        self.held = False

    def acquire(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        try:
            self.path.mkdir()
        except FileExistsError:
            raise LockHeld(self.path, self.read_owner()) from None
        try:
            (self.path / OWNER_FILE).write_text(self.owner + "\n", encoding="utf-8")
        except BaseException:
            (self.path / OWNER_FILE).unlink(missing_ok=True)
            self.path.rmdir()
            raise
        self.held = True

    def read_owner(self) -> str:
        try:
            return (self.path / OWNER_FILE).read_text(encoding="utf-8").strip()
        except OSError:
            return ""

    def release(self) -> None:
        if not self.held:
            return
        self.held = False
        owner = self.read_owner()
        if owner and owner != self.owner.strip():
            # someone removed ours and took it since: leave theirs alone
            return
        try:
            (self.path / OWNER_FILE).unlink(missing_ok=True)
            self.path.rmdir()
        except OSError:
            pass

    def __enter__(self) -> "ServerLock":
        self.acquire()
        return self

    def __exit__(self, *exc) -> None:
        self.release()


def owner_text(what: str) -> str:
    stamp = datetime.datetime.now().astimezone().isoformat(timespec="seconds")
    return f"{what} (pid {os.getpid()} on {socket.gethostname()}, since {stamp})"


def git_repos(core: pathlib.Path) -> list[pathlib.Path]:
    """The core and every modules/* checkout that is a repo of its own."""
    repos = [core]
    modules = core / "modules"
    if modules.is_dir():
        repos += [path for path in sorted(modules.iterdir()) if path.is_dir() and (path / ".git").exists()]
    return repos


def _git(run: Run, repo: pathlib.Path, *args: str) -> subprocess.CompletedProcess:
    env = dict(os.environ, GIT_CEILING_DIRECTORIES=str(repo.parent))
    return run(["git", "-C", str(repo), *args], capture_output=True, text=True, env=env)


def dirty_trees(core: pathlib.Path, run: Run = subprocess.run) -> list[str]:
    """Why a build now would ship something uncommitted: any modified tracked file, or an untracked,
    unignored file under src/, in the core or a module. Empty when the build is safe."""
    problems = []
    for repo in git_repos(core):
        name = "core" if repo == core else f"modules/{repo.name}"
        tracked = _git(run, repo, "status", "--porcelain", "--untracked-files=no")
        untracked = _git(run, repo, "ls-files", "--others", "--exclude-standard", "--", "src")
        if tracked.returncode != 0 or untracked.returncode != 0:
            error = (tracked.stderr or untracked.stderr or "").strip()
            problems.append(f"{name}: git could not read the tree ({error or 'no message'})")
            continue
        for line in tracked.stdout.splitlines():
            if line.strip():
                problems.append(f"{name}: modified {line[3:].strip()}")
        for line in untracked.stdout.splitlines():
            if line.strip():
                problems.append(f"{name}: untracked {line.strip()}")
    return problems


def compose(*args: str) -> list[str]:
    command = ["docker", "compose"]
    for path in COMPOSE_FILES:
        command += ["-f", path]
    return command + list(args)


def build(core: pathlib.Path, log_path: pathlib.Path, run: Run = subprocess.run) -> int:
    """The capped worldserver build, logged to log_path. Returns compose's own exit code."""
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with open(log_path, "wb") as log:
        result = run(compose("build", "--progress", "plain", CONTAINER), cwd=str(core),
                     stdout=log, stderr=subprocess.STDOUT)
    return result.returncode


def container_id(run: Run = subprocess.run) -> str:
    """The container's id and start time, so a recreate and a start of a stopped one both change it."""
    result = run(["docker", "inspect", "-f", "{{.Id}} {{.State.StartedAt}}", CONTAINER],
                 capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else ""


def restart(core: pathlib.Path, run: Run = subprocess.run, now: Callable[[], float] = time.time) \
        -> tuple[int, str, bool]:
    """`up -d --no-deps` on the new image: (exit code, log --since stamp, whether a worldserver started)."""
    before = container_id(run)
    # a few seconds early, in case the Docker VM's clock trails the host's
    since = datetime.datetime.fromtimestamp(now() - 5, datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    result = run(compose("up", "-d", "--no-deps", CONTAINER), cwd=str(core), capture_output=True, text=True)
    after = container_id(run)
    return result.returncode, since, bool(after) and after != before


def wait_ready(since: str, timeout_s: float, run: Run = subprocess.run,
               clock: Callable[[], float] = time.monotonic, sleep: Callable[[float], None] = time.sleep,
               poll_s: float = 10.0) -> bool:
    deadline = clock() + timeout_s
    while True:
        result = run(["docker", "logs", "--since", since, CONTAINER], capture_output=True, text=True,
                     encoding="utf-8", errors="replace")
        if READY_LINE in (result.stdout or "") or READY_LINE in (result.stderr or ""):
            return True
        if clock() >= deadline:
            return False
        sleep(poll_s)

import argparse
import pathlib
import re
import subprocess
import sys
from dataclasses import dataclass

EXTENSIONS = (".h", ".hpp", ".cpp")
HEADER_MARK = "This file is part of the mod-playerbots module"
DIRECTIVE = re.compile(r"pblint:|NOLINT|clang-format\s+(?:off|on)")
PARAM_NAME = re.compile(r"/\*\s*[A-Za-z_]\w*\s*\*/")
RAW_PREFIXES = {"R", "u8R", "uR", "UR", "LR"}
CHAR_PREFIXES = {"u8", "u", "U", "L"}
TOKEN = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|\w+|\S')
TIGHT_AFTER = "),;]"
TIGHT_BEFORE = "(["


class StripError(Exception):
    pass


@dataclass
class Comment:
    start: int
    end: int
    kind: str
    raw: str
    reason: str = ""

    @property
    def text(self) -> str:
        return " ".join(self.text_lines)

    @property
    def text_lines(self) -> list[str]:
        if self.kind == "line":
            return [self.raw[2:].strip()]
        out = []
        for line in self.raw[2:-2].split("\n"):
            line = line.strip()
            if line.startswith("*"):
                line = line[1:].strip()
            if line:
                out.append(line)
        return out


def is_ident(ch: str) -> bool:
    return ch.isalnum() or ch == "_"


def ident_before(src: str, i: int) -> str:
    k = i
    while k > 0 and is_ident(src[k - 1]):
        k -= 1
    return src[k:i]


def line_of(src: str, offset: int) -> int:
    return src.count("\n", 0, offset) + 1


def skip_quoted(src: str, i: int, quote: str, name: str) -> int:
    j = i + 1
    while j < len(src):
        ch = src[j]
        if ch == "\\":
            j += 2
            continue
        if ch == quote:
            return j + 1
        if ch == "\n":
            break
        j += 1
    raise StripError(f"{name}:{line_of(src, i)}: unterminated literal")


def lex(src: str, name: str) -> list[Comment]:
    out = []
    i = 0
    n = len(src)
    while i < n:
        two = src[i:i + 2]
        ch = src[i]
        if two == "//":
            j = src.find("\n", i)
            j = n if j < 0 else j
            if src[i:j].endswith("\\"):
                raise StripError(f"{name}:{line_of(src, i)}: line comment continued with a backslash")
            out.append(Comment(i, j, "line", src[i:j]))
            i = j
        elif two == "/*":
            j = src.find("*/", i + 2)
            if j < 0:
                raise StripError(f"{name}:{line_of(src, i)}: unterminated block comment")
            out.append(Comment(i, j + 2, "block", src[i:j + 2]))
            i = j + 2
        elif ch == '"':
            if ident_before(src, i) in RAW_PREFIXES:
                m = re.match(r'"([^()\\ \t\n]{0,16})\(', src[i:])
                if not m:
                    raise StripError(f"{name}:{line_of(src, i)}: malformed raw string")
                close = ")" + m.group(1) + '"'
                j = src.find(close, i + m.end())
                if j < 0:
                    raise StripError(f"{name}:{line_of(src, i)}: unterminated raw string")
                i = j + len(close)
            else:
                i = skip_quoted(src, i, '"', name)
        elif ch == "'":
            prefix = ident_before(src, i)
            if prefix and prefix not in CHAR_PREFIXES:
                raise StripError(f"{name}:{line_of(src, i)}: digit separator or unknown literal prefix")
            i = skip_quoted(src, i, "'", name)
        else:
            i += 1
    return out


def normalize(src: str, name: str) -> tuple[str, str]:
    eol = "\r\n" if "\r\n" in src else "\n"
    text = src.replace("\r\n", "\n")
    if "\r" in text:
        raise StripError(f"{name}: mixed line endings")
    return text, eol


def line_starts(text: str) -> list[int]:
    starts = [0]
    for m in re.finditer("\n", text):
        starts.append(m.end())
    return starts


def position(starts: list[int], offset: int) -> tuple[int, int]:
    lo, hi = 0, len(starts) - 1
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if starts[mid] <= offset:
            lo = mid
        else:
            hi = mid - 1
    return lo, offset - starts[lo]


def alone_on_line(text: str, c: Comment) -> bool:
    ls = text.rfind("\n", 0, c.start) + 1
    le = text.find("\n", c.end)
    le = len(text) if le < 0 else le
    return not text[ls:c.start].strip() and not text[c.end:le].strip()


def classify(text: str, comments: list[Comment], keep_texts: set[str], keep_params: bool) -> None:
    for c in comments:
        if c.kind == "block" and not text[:c.start].strip() and HEADER_MARK in c.raw:
            c.reason = "header"
        elif DIRECTIVE.search(c.raw):
            c.reason = "directive"
        elif keep_params and c.kind == "block" and PARAM_NAME.fullmatch(c.raw) and not alone_on_line(text, c):
            c.reason = "param"
        elif c.text in keep_texts:
            c.reason = "keep"


def join(left: str, right: str) -> str:
    if not right.strip():
        return left.rstrip()
    if not left.strip():
        return left + right.lstrip()
    if left[-1].isspace() and right[0].isspace():
        right = right.lstrip()
    if left[-1].isspace() and right[0] in TIGHT_AFTER:
        left = left.rstrip()
    if left[-1] in TIGHT_BEFORE and right[0].isspace():
        right = right.lstrip()
    if is_ident(left[-1]) and is_ident(right[0]):
        return left + " " + right
    return left + right


def strip_text(text: str, removed: list[Comment], name: str) -> str:
    if not text.endswith("\n"):
        raise StripError(f"{name}: no final newline")
    lines = text.split("\n")[:-1]
    starts = line_starts(text)
    spans: dict[int, list[tuple[int, int]]] = {}
    gone: set[int] = set()
    for c in removed:
        sl, sc = position(starts, c.start)
        el, ec = position(starts, c.end)
        if sl == el:
            spans.setdefault(sl, []).append((sc, ec))
            continue
        if lines[sl][:sc].strip() or lines[el][ec:].strip():
            raise StripError(f"{name}:{sl + 1}: code shares a line with a multi-line block comment")
        gone.update(range(sl, el + 1))
    current = list(lines)
    for ln, line_spans in spans.items():
        s = lines[ln]
        for a, b in sorted(line_spans, reverse=True):
            s = join(s[:a], s[b:])
        if s.strip():
            current[ln] = s
        else:
            gone.add(ln)
    drop_extra_blanks(current, gone)
    return "".join(current[i] + "\n" for i in range(len(current)) if i not in gone)


def drop_extra_blanks(lines: list[str], gone: set[int]) -> None:
    def blank(i: int) -> bool:
        return not lines[i].strip()

    def neighbour(i: int, step: int):
        i += step
        while 0 <= i < len(lines) and i in gone:
            i += step
        return i if 0 <= i < len(lines) else None

    changed = True
    while changed:
        changed = False
        i = 0
        while i < len(lines):
            if i not in gone or (i > 0 and i - 1 in gone):
                i += 1
                continue
            prev = neighbour(i, -1)
            nxt = neighbour(i, 1)
            victim = None
            if nxt is not None and blank(nxt) and (prev is None or blank(prev)
                                                    or lines[prev].rstrip().endswith("{")):
                victim = nxt
            elif prev is not None and blank(prev) and (nxt is None or lines[nxt].lstrip().startswith("}")):
                victim = prev
            if victim is not None:
                gone.add(victim)
                changed = True
            i += 1


def code_tokens(src: str, name: str) -> list[str]:
    text, _ = normalize(src, name)
    parts = []
    pos = 0
    for c in lex(text, name):
        parts.append(text[pos:c.start])
        parts.append(" ")
        pos = c.end
    parts.append(text[pos:])
    return TOKEN.findall("".join(parts))


def git(root: pathlib.Path, *args: str) -> bytes:
    result = subprocess.run(["git", "-C", str(root), *args], capture_output=True)
    if result.returncode != 0:
        raise StripError(f"git {' '.join(args)}: {result.stderr.decode(errors='replace').strip()}")
    return result.stdout


def list_files(root: pathlib.Path, paths: list[str], rev: str | None) -> list[str]:
    if rev:
        names = git(root, "ls-tree", "-r", "--name-only", rev, "--", *paths).decode().splitlines()
    else:
        names = []
        for p in paths:
            full = root / p
            if full.is_dir():
                names += [f.relative_to(root).as_posix() for f in full.rglob("*") if f.is_file()]
            elif full.is_file():
                names.append(full.relative_to(root).as_posix())
            else:
                raise StripError(f"{p}: no such file or directory")
    return sorted({n for n in names if n.endswith(EXTENSIONS)})


def read(root: pathlib.Path, rel: str, rev: str | None) -> str:
    data = git(root, "show", f"{rev}:{rel}") if rev else (root / rel).read_bytes()
    return data.decode("utf-8")


def load_keep_dir(directory: pathlib.Path) -> dict[str, dict[str, bool]]:
    keep: dict[str, dict[str, bool]] = {}
    for f in sorted(directory.glob("*.txt")):
        for number, line in enumerate(f.read_text(encoding="utf-8").splitlines(), start=1):
            if not line.strip():
                continue
            if "\t" not in line:
                raise StripError(f"{f}:{number}: expected <path><TAB><comment text>")
            path, text = line.split("\t", 1)
            keep.setdefault(path.strip(), {})[text.strip()] = False
    return keep


def full_line_groups(text: str, comments: list[Comment]) -> list[list[Comment]]:
    groups: list[list[Comment]] = []
    for c in comments:
        prev = groups[-1][-1] if groups else None
        if (prev is not None and alone_on_line(text, c) and alone_on_line(text, prev)
                and line_of(text, c.start) == line_of(text, prev.end) + 1):
            groups[-1].append(c)
        else:
            groups.append([c])
    return groups


def blank_comments(text: str, comments: list[Comment]) -> list[str]:
    chars = list(text)
    for c in comments:
        for k in range(c.start, c.end):
            if chars[k] != "\n":
                chars[k] = " "
    return "".join(chars).split("\n")


def cmd_dump(args, root: pathlib.Path) -> int:
    total = 0
    files = 0
    for rel in list_files(root, args.paths, args.rev):
        text, _ = normalize(read(root, rel, args.rev), rel)
        comments = lex(text, rel)
        classify(text, comments, set(), not args.param_names_strip)
        shown = [c for c in comments if not c.reason]
        if not shown:
            continue
        files += 1
        total += len(shown)
        code_lines = blank_comments(text, comments)
        print(f"== {rel}")
        for group in full_line_groups(text, shown):
            first = line_of(text, group[0].start)
            last = line_of(text, group[-1].end)
            where = f"L{first}" if first == last else f"L{first}-{last}"
            if alone_on_line(text, group[0]):
                after = next((s.strip() for s in code_lines[last:] if s.strip()), "<end of file>")
                print(f"{where}  before: {after[:120]}")
            else:
                print(f"{where}  on: {code_lines[first - 1].strip()[:120]}")
            for c in group:
                for line in c.text_lines:
                    print(f"    {line}")
    print(f"{total} comments in {files} files", file=sys.stderr)
    return 0


def cmd_strip(args, root: pathlib.Path) -> int:
    keep = load_keep_dir(root / args.keep) if args.keep else {}
    counts = {"removed": 0, "header": 0, "directive": 0, "param": 0, "keep": 0}
    changed = 0
    for rel in list_files(root, args.paths, None):
        src = read(root, rel, None)
        text, eol = normalize(src, rel)
        keep_texts = set(keep.get(rel, {}))
        if args.keep_from:
            try:
                ref, _ = normalize(read(root, rel, args.keep_from), rel)
                keep_texts |= {c.text for c in lex(ref, rel)}
            except StripError:
                pass
        comments = lex(text, rel)
        classify(text, comments, keep_texts, not args.param_names_strip)
        for c in comments:
            counts[c.reason or "removed"] += 1
            if c.reason == "keep" and c.text in keep.get(rel, {}):
                keep[rel][c.text] = True
        removed = [c for c in comments if not c.reason]
        if not removed:
            continue
        new = strip_text(text, removed, rel).replace("\n", eol)
        if new != src:
            changed += 1
            if not args.dry_run:
                (root / rel).write_bytes(new.encode("utf-8"))
    unmatched = [(p, t) for p, texts in keep.items() for t, hit in texts.items() if not hit]
    for p, t in unmatched:
        print(f"keep entry matched nothing: {p}\t{t}", file=sys.stderr)
    verb = "would change" if args.dry_run else "changed"
    print(f"{verb} {changed} files; removed {counts['removed']} comments; kept {counts['header']} headers, "
          f"{counts['directive']} directives, {counts['param']} parameter names, {counts['keep']} keep entries")
    return 1 if unmatched else 0


def cmd_check(args, root: pathlib.Path) -> int:
    failed = 0
    old_files = set(list_files(root, args.paths, args.rev))
    new_files = set(list_files(root, args.paths, None))
    for rel in sorted(old_files ^ new_files):
        print(f"{rel}: only in {'the working tree' if rel in new_files else args.rev}")
        failed += 1
    for rel in sorted(old_files & new_files):
        old = code_tokens(read(root, rel, args.rev), rel)
        new = code_tokens(read(root, rel, None), rel)
        if old != new:
            at = next((k for k, (a, b) in enumerate(zip(old, new)) if a != b), min(len(old), len(new)))
            print(f"{rel}: code differs at token {at}: {old[at:at + 6]} vs {new[at:at + 6]}")
            failed += 1
    print(f"checked {len(old_files & new_files)} files, {failed} differ")
    return 1 if failed else 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Strip C++ comments, keeping license headers, tool directives, parameter-name comments "
                    "and a keep list, and prove only comments and whitespace changed.")
    parser.add_argument("--root", default=".", help="repository root the paths are relative to")
    sub = parser.add_subparsers(dest="command", required=True)

    dump = sub.add_parser("dump", help="list every strippable comment with its line and nearby code")
    dump.add_argument("--rev", help="read files from this git revision instead of the working tree")
    dump.add_argument("--param-names-strip", action="store_true", help="list parameter-name comments too")
    dump.add_argument("paths", nargs="+")

    strip = sub.add_parser("strip", help="remove comments in place")
    strip.add_argument("--keep", help="directory of *.txt keep lists, lines of <path><TAB><comment text>; "
                                      "an entry matching nothing fails the run")
    strip.add_argument("--keep-from", help="also keep each comment that exists in this revision's copy of the file")
    strip.add_argument("--param-names-strip", action="store_true", help="remove parameter-name comments too")
    strip.add_argument("--dry-run", action="store_true")
    strip.add_argument("paths", nargs="+")

    check = sub.add_parser("check", help="compare code tokens, comments ignored, against a revision")
    check.add_argument("--rev", required=True)
    check.add_argument("paths", nargs="+")

    args = parser.parse_args(argv)
    root = pathlib.Path(args.root).resolve()
    try:
        return {"dump": cmd_dump, "strip": cmd_strip, "check": cmd_check}[args.command](args, root)
    except StripError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

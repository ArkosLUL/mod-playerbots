#!/usr/bin/env python3
"""Fetch the raid guides their sites let us read, and turn them into Markdown for strategy work.

    python -I guide_text.py fetch   --manifest sources/<raid>.json --cache DIR [--slug S ...] [--images]
    python -I guide_text.py convert --manifest sources/<raid>.json --cache DIR [--slug S ...]

A manifest names, per encounter slug, the source URLs with a short tag, and the Wowhead guide a page
the user saved should match. It holds URLs only, never page text.

fetch obeys each host's robots.txt for this tool's agent and for Anthropic's (ClaudeBot, Claude-Web,
anthropic-ai), waits at least 2 s or the Crawl-delay between requests to one host, and stops a host
for the rest of the run on 403 or 429 instead of retrying it. It never contacts wowhead.com or its
image host: Wowhead's robots.txt opts out Anthropic's agents, so its text only comes from pages the
user saves into <cache>/wowhead/.

Cache layout:
    raw/<tag>/<slug>.html     fetched pages
    img/<tag>/<slug>/         their article images (--images)
    wowhead/                  pages the user saved from Wowhead, any file name, .html or .mhtml
    text/<slug>.<tag>.md      converted text; saved Wowhead pages get the tag `wowhead`
    fetch-log.json            what fetch did per slug and source
    convert-log.json          what convert wrote per slug and source
    INDEX.md                  status per slug and source
"""
from __future__ import annotations

import argparse
import codecs
import datetime as dt
import email
import email.policy
import hashlib
import html.parser
import http.client
import json
import os.path
import pathlib
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import urllib.robotparser
from dataclasses import dataclass, field
from typing import Callable, Iterable, Iterator, NamedTuple, TextIO

USER_AGENT = "mod-playerbots-guide-reader/1.0"
ROBOTS_AGENTS = (USER_AGENT, "ClaudeBot", "Claude-Web", "anthropic-ai")
BANNED_DOMAINS = {
    "wowhead.com": "wowhead.com is never fetched: its robots.txt opts out Anthropic's agents",
    "zamimg.com": "zamimg.com is Wowhead's asset host and is never fetched",
}
MIN_HOST_GAP = 2.0
STOP_STATUSES = (403, 429)
REDIRECTS = (301, 302, 303, 307, 308)
MAX_REDIRECTS = 5
MAX_PAGE_BYTES = 20 * 1024 * 1024
MAX_IMAGE_BYTES = 10 * 1024 * 1024
MAX_ROBOTS_BYTES = 512 * 1024
MIN_IMAGE_SIDE = 64
SAVED_TAG = "wowhead"
SAVED_SUFFIXES = (".html", ".htm", ".mhtml", ".mht")
TITLE_NOISE = frozenset({"strategy", "guide"})

SLUG_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
TAG_RE = re.compile(r"^[a-z0-9]+$")
ENTITY_RE = re.compile(r"(?:^|[/?&#.])(spell|npc|item)[=/](\d+)")
SAVED_FROM_RE = re.compile(r"saved from url=\(\d+\)(\S+?)\s*-->")
META_CHARSET_RE = re.compile(rb"""<meta[^>]+charset\s*=\s*["']?\s*([A-Za-z0-9_.:-]+)""", re.I)
IMAGE_FILE_RE = re.compile(r"\.(?:png|jpe?g|gif|webp|bmp)$", re.I)
DIAGRAM_RE = re.compile(r"\[diagram: [^\]\n]*\]\([^)\s]*\)")
WS_RE = re.compile(r"[ \t\r\n\f\v\xa0\u200b]+")
BR = "\x00"


class ManifestError(ValueError):
    pass


# ---------------------------------------------------------------------------------------------------
# Manifest


@dataclass(frozen=True)
class Source:
    slug: str
    tag: str
    url: str
    sections: tuple[str, ...] = ()

    @property
    def key(self) -> str:
        return f"{self.slug}|{self.tag}"


@dataclass
class Manifest:
    path: pathlib.Path
    raid: str
    name: str
    slugs: list[str]
    sources: list[Source]
    wowhead: dict[str, str] = field(default_factory=dict)
    title_words: dict[str, frozenset[str]] = field(default_factory=dict)
    notes: dict[str, str] = field(default_factory=dict)

    def select(self, slugs: Iterable[str] | None) -> list[Source]:
        wanted = set(slugs or ())
        return [s for s in self.sources if not wanted or s.slug in wanted]

    def wowhead_slug(self, url: str) -> str | None:
        target = _url_key(url)
        if target is None or banned_reason(target[0]) is None:
            return None
        for slug, expected in self.wowhead.items():
            if _url_key(expected) == target:
                return slug
        last = target[1].rsplit("/", 1)[-1]
        matches = [slug for slug, expected in self.wowhead.items() if _url_key(expected)[1].rsplit("/", 1)[-1] == last]
        return matches[0] if len(matches) == 1 else None

    def title_slug(self, title: str) -> str | None:
        have = set(words(title))
        scored = sorted(((len(want), slug) for slug, want in self.title_words.items() if want and want <= have),
                        reverse=True)
        if not scored or (len(scored) > 1 and scored[0][0] == scored[1][0]):
            return None
        return scored[0][1]


def load_manifest(path: pathlib.Path) -> Manifest:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as err:
        raise ManifestError(f"{path}: {err}") from err
    raid, name, entries = data.get("raid"), data.get("name"), data.get("slugs")
    if not isinstance(raid, str) or not isinstance(name, str) or not isinstance(entries, dict):
        raise ManifestError(f"{path}: needs string 'raid' and 'name' and an object 'slugs'")
    manifest = Manifest(path, raid, name, [], [])
    for slug, entry in entries.items():
        if not SLUG_RE.match(slug) or not isinstance(entry, dict):
            raise ManifestError(f"{path}: bad slug entry {slug!r}")
        manifest.slugs.append(slug)
        tags: set[str] = set()
        for src in entry.get("sources", []):
            tag, url, sections = src.get("tag"), src.get("url"), src.get("sections", [])
            if not isinstance(tag, str) or not TAG_RE.match(tag) or tag == SAVED_TAG or tag in tags:
                raise ManifestError(f"{path}: {slug}: bad or repeated source tag {tag!r}")
            if not isinstance(url, str) or urllib.parse.urlsplit(url).scheme not in ("http", "https"):
                raise ManifestError(f"{path}: {slug}/{tag}: bad url {url!r}")
            if not isinstance(sections, list) or not all(isinstance(s, str) and s for s in sections):
                raise ManifestError(f"{path}: {slug}/{tag}: 'sections' must be a list of heading texts")
            tags.add(tag)
            manifest.sources.append(Source(slug, tag, url, tuple(sections)))
        if "wowhead" in entry:
            url = entry["wowhead"]
            key = _url_key(url) if isinstance(url, str) else None
            if key is None or banned_reason(key[0]) is None:
                raise ManifestError(f"{path}: {slug}: 'wowhead' must be a wowhead.com URL")
            manifest.wowhead[slug] = url
            hint = entry.get("wowhead_title")
            last = key[1].rsplit("/", 1)[-1]
            manifest.title_words[slug] = frozenset(words(hint) if isinstance(hint, str) else
                                                   (w for w in words(last) if w not in TITLE_NOISE))
        if isinstance(entry.get("note"), str):
            manifest.notes[slug] = entry["note"]
    return manifest


def words(text: str) -> list[str]:
    return re.findall(r"[^\W_]+", re.sub(r"['’`]", "", text.lower()))


def _url_key(url: str) -> tuple[str, str] | None:
    parts = urllib.parse.urlsplit(url.strip())
    if parts.scheme not in ("http", "https") or not parts.hostname:
        return None
    host = parts.hostname.lower()
    host = host[4:] if host.startswith("www.") else host
    return host, urllib.parse.unquote(parts.path).rstrip("/").lower()


def banned_reason(host: str) -> str | None:
    host = host.lower().rstrip(".")
    for domain, reason in BANNED_DOMAINS.items():
        if host == domain or host.endswith("." + domain):
            return reason
    return None


# ---------------------------------------------------------------------------------------------------
# Fetching


class Response(NamedTuple):
    status: int  # 0 when no HTTP answer came back
    headers: dict[str, str]
    body: bytes
    reason: str


class Outcome(NamedTuple):
    status: str  # ok, refused, error
    reason: str
    url: str
    body: bytes = b""
    content_type: str = ""


HttpGet = Callable[[str, int], Response]


class _NoRedirect(urllib.request.HTTPRedirectHandler):
    # hands redirects back to the caller so every hop gets the ban and robots checks again
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def http_get(url: str, limit: int) -> Response:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept-Language": "en"})
    opener = urllib.request.build_opener(_NoRedirect)
    try:
        with opener.open(request, timeout=30) as resp:
            return Response(resp.status, _lower(resp.headers), resp.read(limit + 1), resp.reason or "")
    except urllib.error.HTTPError as err:
        headers = _lower(err.headers) if err.headers is not None else {}
        err.close()
        return Response(err.code, headers, b"", str(err.reason))
    except (urllib.error.URLError, http.client.HTTPException, OSError, ValueError) as err:
        return Response(0, {}, b"", str(getattr(err, "reason", err)))


def _lower(headers) -> dict[str, str]:
    return {k.lower(): v for k, v in headers.items()}


def _host(url: str) -> str:
    return (urllib.parse.urlsplit(url).hostname or "").lower()


class Fetcher:
    def __init__(self, get: HttpGet = http_get, sleep: Callable[[float], None] = time.sleep,
                 clock: Callable[[], float] = time.monotonic):
        self._get, self._sleep, self._clock = get, sleep, clock
        self._robots: dict[str, urllib.robotparser.RobotFileParser | str] = {}
        self._stopped: dict[str, str] = {}
        self._last: dict[str, float] = {}
        self._gap: dict[str, float] = {}
        self._seen: dict[tuple[str, int], Outcome] = {}

    def fetch(self, url: str, limit: int = MAX_PAGE_BYTES) -> Outcome:
        if (url, limit) not in self._seen:
            self._seen[(url, limit)] = self._fetch(url, limit)
        return self._seen[(url, limit)]

    def refusal(self, url: str) -> str | None:
        parts = urllib.parse.urlsplit(url)
        if parts.scheme not in ("http", "https") or not parts.hostname:
            return "not an http(s) URL"
        host = parts.hostname.lower()
        banned = banned_reason(host)
        if banned:
            return banned
        if host in self._stopped:
            return f"{host} stopped earlier in this run ({self._stopped[host]})"
        robots = self._robots_for(parts)
        if isinstance(robots, str):
            return robots
        for agent in ROBOTS_AGENTS:
            if not robots.can_fetch(agent, url):
                return f"robots.txt disallows {agent}"
        return None

    def _fetch(self, url: str, limit: int) -> Outcome:
        for _ in range(MAX_REDIRECTS + 1):
            refusal = self.refusal(url)
            if refusal:
                return Outcome("refused", refusal, url)
            resp = self._request(url, limit)
            if resp.status in REDIRECTS and resp.headers.get("location"):
                url = urllib.parse.urljoin(url, resp.headers["location"])
                continue
            if resp.status in STOP_STATUSES:
                host = _host(url)
                self._stopped[host] = f"HTTP {resp.status}"
                return Outcome("refused", f"HTTP {resp.status}; {host} stopped for this run", url)
            if resp.status != 200:
                reason = f"HTTP {resp.status} {resp.reason}".strip() if resp.status else resp.reason
                return Outcome("error", reason, url)
            if len(resp.body) > limit:
                return Outcome("error", f"larger than {limit} bytes", url)
            return Outcome("ok", "", url, resp.body, resp.headers.get("content-type", ""))
        return Outcome("error", "too many redirects", url)

    def _robots_for(self, parts: urllib.parse.SplitResult) -> urllib.robotparser.RobotFileParser | str:
        origin = f"{parts.scheme}://{parts.netloc.lower()}"
        if origin not in self._robots:
            self._robots[origin] = self._load_robots(origin + "/robots.txt", parts.hostname.lower())
        return self._robots[origin]

    def _load_robots(self, url: str, host: str) -> urllib.robotparser.RobotFileParser | str:
        for _ in range(MAX_REDIRECTS + 1):
            banned = banned_reason(_host(url))
            if banned:
                return f"robots.txt redirects: {banned}"
            resp = self._request(url, MAX_ROBOTS_BYTES)
            if resp.status in REDIRECTS and resp.headers.get("location"):
                url = urllib.parse.urljoin(url, resp.headers["location"])
                continue
            parser = urllib.robotparser.RobotFileParser(url)
            if resp.status == 200:
                parser.parse(resp.body.decode("utf-8", "replace").splitlines())
                self._gap[host] = max([MIN_HOST_GAP] + _robots_delays(parser))
                return parser
            if resp.status in (401,) + STOP_STATUSES:
                self._stopped[host] = f"HTTP {resp.status} on robots.txt"
                return f"robots.txt answered HTTP {resp.status}; {host} stopped for this run"
            if 400 <= resp.status < 500:
                parser.parse([])
                return parser
            return f"robots.txt unavailable ({f'HTTP {resp.status}' if resp.status else resp.reason})"
        return "robots.txt redirects too often"

    def _request(self, url: str, limit: int) -> Response:
        host = _host(url)
        if host in self._last:
            wait = self._last[host] + self._gap.get(host, MIN_HOST_GAP) - self._clock()
            if wait > 0:
                self._sleep(wait)
        try:
            return self._get(url, limit)
        finally:
            self._last[host] = self._clock()


def _robots_delays(parser: urllib.robotparser.RobotFileParser) -> list[float]:
    delays = []
    for agent in ROBOTS_AGENTS:
        delay = parser.crawl_delay(agent)
        if delay:
            delays.append(float(delay))
        rate = parser.request_rate(agent)
        if rate and rate.requests:
            delays.append(rate.seconds / rate.requests)
    return delays


# ---------------------------------------------------------------------------------------------------
# HTML tree

VOID_TAGS = frozenset({"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source",
                       "track", "wbr"})
HEADINGS = {f"h{n}": n for n in range(1, 7)}
BLOCK_TAGS = frozenset({"address", "article", "aside", "blockquote", "caption", "center", "dd", "details", "dialog",
                        "div", "dl", "dt", "fieldset", "figcaption", "figure", "footer", "form", "header", "hgroup",
                        "hr", "li", "main", "nav", "ol", "p", "pre", "section", "summary", "table", "tbody", "td",
                        "tfoot", "th", "thead", "tr", "ul"} | set(HEADINGS))
P_CLOSERS = BLOCK_TAGS - {"li", "dd", "dt", "td", "th", "tr", "tbody", "thead", "tfoot", "caption", "summary"}
DROP_TAGS = frozenset({"script", "style", "noscript", "template", "iframe", "svg", "canvas", "nav", "form", "button",
                       "select", "input", "textarea", "aside", "footer", "object", "embed", "video", "audio"})
COMMENT_TOKENS = frozenset({"comment", "comments", "commentlist", "respond"})
DROP_TOKENS = COMMENT_TOKENS | frozenset({
    "share", "sharing", "sharedaddy", "social", "related",
    "relatedposts", "yarpp", "sidebar", "ad", "ads", "adsbygoogle", "advert", "advertisement", "sponsored",
    "newsletter", "subscribe", "breadcrumb", "breadcrumbs", "menu", "navigation", "navbar", "cookie", "popup",
    "modal", "pagination", "pager", "toc", "footer",
})
# themes put layout tokens like "fl-nav-collapse-menu" on these, so never drop them by class
NEVER_DROPPED = frozenset({"html", "body", "main", "article"})
# body furniture no token catches, e.g. Warcraft Tavern's strip of links to the raid's other guides
DROP_SELECTORS = (".sub-guide-blocks",)


class Node:
    __slots__ = ("tag", "attrs", "children", "parent")

    def __init__(self, tag: str, attrs: dict[str, str], parent: Node | None):
        self.tag, self.attrs, self.parent = tag, attrs, parent
        self.children: list[Node | str] = []

    def classes(self) -> set[str]:
        return set(self.attrs.get("class", "").split())

    def elements(self) -> Iterator[Node]:
        for child in self.children:
            if isinstance(child, Node):
                yield child

    def descendants(self) -> Iterator[Node]:
        for child in self.elements():
            yield child
            yield from child.descendants()

    def text(self) -> str:
        return "".join(c if isinstance(c, str) else c.text() for c in self.children)

    def matches(self, selector: str) -> bool:
        tag, ident, classes = _parse_selector(selector)
        return ((not tag or self.tag == tag) and (not ident or self.attrs.get("id") == ident)
                and classes <= self.classes())


def _parse_selector(selector: str) -> tuple[str, str, set[str]]:
    tag = re.match(r"[a-z0-9]*", selector).group(0)
    ident = next(iter(re.findall(r"#([\w-]+)", selector)), "")
    return tag, ident, set(re.findall(r"\.([\w-]+)", selector))


class _TreeBuilder(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.root = Node("#root", {}, None)
        self.cur = self.root

    def handle_starttag(self, tag, attrs):
        self._implicit_close(tag)
        node = Node(tag, {k: v or "" for k, v in attrs}, self.cur)
        self.cur.children.append(node)
        if tag not in VOID_TAGS:
            self.cur = node

    def handle_startendtag(self, tag, attrs):
        self._implicit_close(tag)
        self.cur.children.append(Node(tag, {k: v or "" for k, v in attrs}, self.cur))

    def handle_endtag(self, tag):
        node = self.cur
        while node is not self.root and node.tag != tag:
            node = node.parent
        if node is not self.root:
            self.cur = node.parent

    def handle_data(self, data):
        self.cur.children.append(data)

    def _implicit_close(self, tag: str) -> None:
        if tag in P_CLOSERS:
            self._close_open("p", stop={"div", "td", "th", "li", "blockquote", "section", "article"})
        if tag == "li":
            self._close_open("li", stop={"ul", "ol"})
        elif tag in ("dt", "dd"):
            self._close_open("dt", "dd", stop={"dl"})
        elif tag == "tr":
            self._close_open("tr", stop={"table"})
        elif tag in ("td", "th"):
            self._close_open("td", "th", stop={"tr", "table"})

    def _close_open(self, *tags: str, stop: set[str]) -> None:
        node = self.cur
        while node is not self.root and node.tag not in stop:
            if node.tag in tags:
                self.cur = node.parent
                return
            node = node.parent


@dataclass
class Document:
    root: Node

    def find_all(self, selector: str) -> list[Node]:
        return [n for n in self.root.descendants() if n.matches(selector)]

    def find(self, selector: str) -> Node | None:
        return next((n for n in self.root.descendants() if n.matches(selector)), None)

    def find_byline(self, selector: str) -> Node | None:
        """The first match outside reader comments, whose authors and dates are not the page's."""
        return next((n for n in byline_nodes(self.root) if n.matches(selector)), None)

    def meta(self, *names: str) -> str:
        for node in self.root.descendants():
            if node.tag == "meta" and (node.attrs.get("name") in names or node.attrs.get("property") in names):
                value = node.attrs.get("content", "").strip()
                if value:
                    return value
        return ""


def parse_html(text: str) -> Document:
    builder = _TreeBuilder()
    builder.feed(text)
    builder.close()
    return Document(builder.root)


def decode_html(raw: bytes, declared: str | None = None) -> str:
    for bom, codec in ((codecs.BOM_UTF8, "utf-8"), (codecs.BOM_UTF16_LE, "utf-16-le"),
                       (codecs.BOM_UTF16_BE, "utf-16-be")):
        if raw.startswith(bom):
            return raw[len(bom):].decode(codec, "replace")
    candidates = [declared] if declared else []
    sniffed = META_CHARSET_RE.search(raw[:8192])
    if sniffed:
        candidates.append(sniffed.group(1).decode("ascii", "replace"))
    for charset in candidates + ["utf-8"]:
        try:
            codecs.lookup(charset)
        except LookupError:
            continue
        return raw.decode(charset, "replace")
    return raw.decode("utf-8", "replace")


def charset_of(content_type: str) -> str | None:
    match = re.search(r"charset\s*=\s*\"?([\w.:-]+)", content_type or "", re.I)
    return match.group(1) if match else None


def _name_tokens(node: Node) -> set[str]:
    return set(re.split(r"[\s_-]+", f"{node.attrs.get('class', '')} {node.attrs.get('id', '')}".lower()))


def is_dropped(node: Node) -> bool:
    if node.tag in DROP_TAGS:
        return True
    if node.tag in NEVER_DROPPED:
        return False
    return bool(_name_tokens(node) & DROP_TOKENS) or any(node.matches(s) for s in DROP_SELECTORS)


def kept_descendants(node: Node) -> Iterator[Node]:
    for child in node.elements():
        if not is_dropped(child):
            yield child
            yield from kept_descendants(child)


def byline_nodes(node: Node) -> Iterator[Node]:
    # skips reader comments only, blogs put the post's own byline in a footer the body drops
    for child in node.elements():
        if not _name_tokens(child) & COMMENT_TOKENS:
            yield child
            yield from byline_nodes(child)


# ---------------------------------------------------------------------------------------------------
# Article body and metadata

SITE_BODIES = {
    "wowhead": ("#guide-body", ".guide-body", "#guide-content", ".guide-content"),
    "wordpress": ("#ftwp-postcontent", ".fl-post-content", ".entry-content", ".post-content",
                  ".elementor-widget-theme-post-content", ".article-content", "article"),
    "blogger": (".post-body", ".entry-content", ".postcontent", ".post-content", ".hentry"),
    "generic": ("article", "main"),
}


def detect_site(url: str, doc: Document) -> str:
    host = _host(url)
    if banned_reason(host):
        return "wowhead"
    generator = doc.meta("generator").lower()
    if "blogger" in generator or host.endswith(".blogspot.com"):
        return "blogger"
    if "wordpress" in generator or host.endswith(".wordpress.com"):
        return "wordpress"
    for node in doc.root.descendants():
        asset = node.attrs.get("href", "") + node.attrs.get("src", "")
        if node.tag in ("link", "script") and "/wp-content/" in asset:
            return "wordpress"
    return "generic"


def select_body(doc: Document, site: str) -> Node:
    for selector in SITE_BODIES.get(site, ()) + SITE_BODIES["generic"]:
        found = [n for n in doc.find_all(selector) if len(_visible_text(n)) >= 200]
        if found:
            return max(found, key=lambda n: len(_visible_text(n)))
    return _densest(doc.root)


def _visible_text(node: Node) -> str:
    parts = []
    for child in node.children:
        if isinstance(child, str):
            parts.append(child)
        elif not is_dropped(child):
            parts.append(_visible_text(child))
    return WS_RE.sub(" ", "".join(parts)).strip()


def _densest(root: Node) -> Node:
    scores: dict[int, float] = {}
    nodes: dict[int, Node] = {}
    for node in kept_descendants(root):
        if node.tag not in ("p", "li", "h2", "h3", "h4", "td"):
            continue
        weight = len(_visible_text(node))
        for share, holder in ((1.0, node.parent), (0.5, node.parent.parent if node.parent else None)):
            if holder is not None and holder is not root:
                scores[id(holder)] = scores.get(id(holder), 0.0) + share * weight
                nodes[id(holder)] = holder
    if not scores:
        return root
    return nodes[max(scores, key=scores.get)]


@dataclass
class PageInfo:
    title: str = ""
    author: str = ""
    updated: str = ""
    published: str = ""
    canonical: str = ""
    og_url: str = ""
    saved_from: str = ""


def page_info(doc: Document, body: Node | None, raw_text: str) -> PageInfo:
    info = PageInfo()
    ld = _json_ld(doc)
    h1 = next((n for n in kept_descendants(body) if n.tag == "h1"), None) if body is not None else None
    title_node = doc.find("title")
    info.title = _first(h1 and _visible_text(h1), doc.meta("og:title"), ld.get("headline"),
                        title_node and WS_RE.sub(" ", title_node.text()).strip())
    info.author = _first(ld.get("author"), _not_url(doc.meta("author", "article:author")), _microformat_author(doc))
    info.updated = _date(_first(ld.get("dateModified"), doc.meta("article:modified_time", "og:updated_time"),
                                _attr(doc, "time.updated", "datetime")))
    info.published = _date(_first(ld.get("datePublished"), doc.meta("article:published_time"),
                                  _attr(doc, "time.published", "datetime"), _attr(doc, "time", "datetime"),
                                  _attr(doc, "abbr.published", "title"),
                                  _byline_text(doc, ".date-header")))
    canonical = next((n for n in doc.root.descendants()
                      if n.tag == "link" and "canonical" in n.attrs.get("rel", "").lower().split()), None)
    info.canonical = canonical.attrs.get("href", "").strip() if canonical is not None else ""
    info.og_url = doc.meta("og:url")
    saved = SAVED_FROM_RE.search(raw_text[:4096])
    info.saved_from = saved.group(1) if saved else ""
    return info


def _first(*values) -> str:
    for value in values:
        if isinstance(value, str) and value.strip():
            return WS_RE.sub(" ", value).strip()
    return ""


def _not_url(value: str) -> str:
    return "" if re.match(r"https?://", value) else value


def _attr(doc: Document, selector: str, attr: str) -> str:
    node = doc.find_byline(selector)
    return node.attrs.get(attr, "") if node is not None else ""


def _byline_text(doc: Document, selector: str) -> str:
    node = doc.find_byline(selector)
    return _visible_text(node) if node is not None else ""


def _date(value: str) -> str:
    match = re.match(r"\d{4}-\d{2}-\d{2}", value)
    return match.group(0) if match else value


def _microformat_author(doc: Document) -> str:
    nodes = list(byline_nodes(doc.root))
    for selector in (".author .fn", ".vcard .fn", ".post-author .fn", ".byline .author", ".author-name"):
        outer, _, inner = selector.partition(" ")
        for node in (n for n in nodes if n.matches(outer)):
            target = next((n for n in node.descendants() if n.matches(inner)), None) if inner else node
            if target is not None and _text_of(target):
                return _text_of(target)
    rel = next((n for n in nodes if n.tag == "a" and "author" in n.attrs.get("rel", "").split()), None)
    return _text_of(rel) if rel is not None else ""


def _text_of(node: Node) -> str:
    return WS_RE.sub(" ", node.text()).strip()


def _json_ld(doc: Document) -> dict[str, str]:
    objects: list[dict] = []
    for node in doc.root.descendants():
        if node.tag == "script" and node.attrs.get("type", "").lower() == "application/ld+json":
            try:
                _collect_objects(json.loads(node.text()), objects)
            except (json.JSONDecodeError, RecursionError):
                continue
    by_id = {o["@id"]: o for o in objects if isinstance(o.get("@id"), str)}
    found: dict[str, str] = {}
    for obj in objects:
        for key in ("headline", "dateModified", "datePublished"):
            if key not in found and isinstance(obj.get(key), str):
                found[key] = obj[key]
        if "author" not in found and "author" in obj:
            name = _ld_name(obj["author"], by_id)
            if name:
                found["author"] = name
    return found


def _collect_objects(value, out: list[dict]) -> None:
    if isinstance(value, dict):
        out.append(value)
        for child in value.values():
            _collect_objects(child, out)
    elif isinstance(value, list):
        for child in value:
            _collect_objects(child, out)


def _ld_name(value, by_id: dict[str, dict]) -> str:
    if isinstance(value, list):
        return next((n for n in (_ld_name(v, by_id) for v in value) if n), "")
    if isinstance(value, str):
        return "" if re.match(r"https?://", value) else value
    if isinstance(value, dict):
        if isinstance(value.get("name"), str):
            return value["name"]
        target = by_id.get(value.get("@id")) if isinstance(value.get("@id"), str) else None
        if target is not None and isinstance(target.get("name"), str):
            return target["name"]
    return ""


# ---------------------------------------------------------------------------------------------------
# Rendering


# blogs mark sections with a bold line instead of a heading. ranks below every real heading, so a section
# opened by one ends at the next bold line or heading
PSEUDO_HEADING = 7


class Block(NamedTuple):
    level: int  # 1-6 for headings, PSEUDO_HEADING for a bold-only line, 0 otherwise
    text: str
    kind: str = "p"


def is_content_image(node: Node) -> bool:
    for side in ("width", "height"):
        value = node.attrs.get(side, "").strip().removesuffix("px")
        if value.isdigit() and int(value) < MIN_IMAGE_SIDE:
            return False
    names = set(re.split(r"[\s_-]+", node.attrs.get("class", "").lower()))
    return not names & {"emoji", "smiley", "avatar", "icon", "logo"} and bool(image_src(node))


def image_src(node: Node) -> str:
    parent = node.parent
    if parent is not None and parent.tag == "a" and IMAGE_FILE_RE.search(urllib.parse.urlsplit(
            parent.attrs.get("href", "")).path):
        return parent.attrs["href"].strip()
    for attr in ("data-src", "data-lazy-src", "data-original", "data-orig-file", "src"):
        value = node.attrs.get(attr, "").strip()
        if value and not value.startswith("data:"):
            return value
    return ""


def article_images(body: Node) -> list[str]:
    seen: list[str] = []
    for node in kept_descendants(body):
        if node.tag == "img" and is_content_image(node):
            src = image_src(node)
            if src not in seen:
                seen.append(src)
    return seen


class Renderer:
    def __init__(self, image_ref: Callable[[str], str]):
        self.image_ref = image_ref
        self.blocks: list[Block] = []
        self._buf: list[str] = []
        self._captions: dict[int, str] = {}
        self._skip: set[int] = set()
        self._level = 0

    def render(self, body: Node) -> list[Block]:
        self._children(body, 0)
        self._flush()
        return self.blocks

    def _children(self, node: Node, depth: int) -> None:
        for child in node.children:
            if isinstance(child, str):
                self._buf.append(WS_RE.sub(" ", child))
            elif id(child) in self._skip or is_dropped(child):
                continue
            elif child.tag in BLOCK_TAGS:
                self._block(child, depth)
            else:
                self._buf.append(self._inline_one(child))

    def _block(self, node: Node, depth: int) -> None:
        tag = node.tag
        classes = node.classes()
        self._flush()
        if tag in HEADINGS:
            self._heading(HEADINGS[tag], self._inline(node))
        elif "wp-block-kadence-tabs" in classes:
            titles = [self._inline(t) for t in _owned(node, ".kt-title-text", "wp-block-kadence-tabs")]
            self._titled(list(zip(titles, _owned(node, ".wp-block-kadence-tab", "wp-block-kadence-tabs"))), depth)
        elif "wp-block-kadence-pane" in classes:
            title = next(iter(_owned(node, ".kt-blocks-accordion-title", "wp-block-kadence-pane")), None)
            panel = next(iter(_owned(node, ".kt-accordion-panel", "wp-block-kadence-pane")), node)
            self._titled([(self._inline(title) if title is not None else "", panel)], depth)
        elif tag == "details" and any(c.tag == "summary" for c in node.elements()):
            summary = next(c for c in node.elements() if c.tag == "summary")
            self._skip.add(id(summary))
            self._titled([(self._inline(summary), node)], depth)
        elif tag in ("ul", "ol"):
            self._list(node, depth)
        elif tag == "table" and "tr-caption-container" not in node.classes():
            self._table(node, depth)
        elif tag == "pre":
            code = node.text().strip("\n")
            if code.strip():
                self.blocks.append(Block(0, f"```\n{code}\n```", "pre"))
        elif tag == "blockquote":
            inner = Renderer(self.image_ref).render(node)
            if inner:
                text = "\n>\n".join("\n".join(f"> {line}" for line in b.text.split("\n")) for b in inner)
                self.blocks.append(Block(0, text, "quote"))
        elif tag == "figure" or node.classes() & {"wp-caption", "tr-caption-container"}:
            self._figure(node, depth)
        elif tag != "hr":
            self._children(node, depth)
        self._flush()

    def _heading(self, level: int, inline: str) -> None:
        text = _clean(inline)
        images = DIAGRAM_RE.findall(text)
        text = WS_RE.sub(" ", DIAGRAM_RE.sub(" ", text).replace("**", "")).strip()
        if text:
            self.blocks.append(Block(level, f"{'#' * level} {text}", "h"))
            self._level = level
        self.blocks += [Block(0, image) for image in images]

    def _titled(self, parts: list[tuple[str, Node]], depth: int) -> None:
        """Tab and accordion titles live in the widget, not in a heading. Each one becomes a heading one level under
        the heading the widget sits below."""
        base = self._level
        for title, content in parts:
            self._heading(min(base + 1, 6), title)
            self._children(content, depth)
            self._flush()
            self._level = base

    def _figure(self, node: Node, depth: int) -> None:
        caption = next((n for n in node.descendants()
                        if n.tag == "figcaption" or n.classes() & {"wp-caption-text", "tr-caption"}), None)
        image = next((n for n in node.descendants() if n.tag == "img" and is_content_image(n)), None)
        if caption is not None and image is not None:
            self._captions[id(image)] = _clean(self._inline(caption)).replace("\n", " ")
            self._skip.add(id(caption))
        if image is not None and node.tag == "table":
            self._buf.append(self._image(image))
        else:
            self._children(node, depth)

    def _list(self, node: Node, depth: int) -> None:
        number = int(node.attrs["start"]) if node.attrs.get("start", "").isdigit() else 1
        for item in node.elements():
            if is_dropped(item):
                continue
            if item.tag in ("ul", "ol"):
                self._list(item, depth)
                continue
            parts: list[str] = []
            nested: list[Node] = []
            self._item_parts(item, parts, nested)
            text = _clean("".join(parts))
            marker = f"{number}." if node.tag == "ol" else "-"
            if text:
                pad = "  " * depth
                lines = text.split("\n")
                body = "\n".join([lines[0]] + [pad + " " * (len(marker) + 1) + line for line in lines[1:]])
                self.blocks.append(Block(0, f"{pad}{marker} {body}", "li"))
                number += 1
            for sub in nested:
                self._list(sub, depth + 1)

    def _item_parts(self, node: Node, parts: list[str], nested: list[Node]) -> None:
        for child in node.children:
            if isinstance(child, str):
                parts.append(WS_RE.sub(" ", child))
            elif id(child) in self._skip or is_dropped(child):
                continue
            elif child.tag in ("ul", "ol"):
                nested.append(child)
            elif child.tag in BLOCK_TAGS:
                parts.append(BR)
                self._item_parts(child, parts, nested)
                parts.append(BR)
            else:
                parts.append(self._inline_one(child))

    def _table(self, node: Node, depth: int) -> None:
        rows = [[self._cell(cell) for cell in tr.elements() if cell.tag in ("td", "th") and not is_dropped(cell)]
                for tr in _table_rows(node)]
        rows = [row for row in rows if any(row)]
        if not rows:
            return
        width = max(len(row) for row in rows)
        if width == 1:
            self._children(node, depth)
            return
        rows = [row + [""] * (width - len(row)) for row in rows]
        lines = ["| " + " | ".join(rows[0]) + " |", "|" + "---|" * width]
        lines += ["| " + " | ".join(row) + " |" for row in rows[1:]]
        self.blocks.append(Block(0, "\n".join(lines), "table"))

    def _cell(self, cell: Node) -> str:
        parts: list[str] = []
        nested: list[Node] = []
        self._item_parts(cell, parts, nested)
        for sub in nested:
            parts.append(BR + "; ".join(_clean(self._inline(li)) for li in sub.elements() if li.tag == "li"))
        return _clean("".join(parts)).replace("\n", "<br>").replace("|", "\\|")

    def _inline(self, node: Node) -> str:
        return "".join(self._inline_one(child) for child in node.children)

    def _inline_one(self, child: Node | str) -> str:
        if isinstance(child, str):
            return WS_RE.sub(" ", child)
        if id(child) in self._skip or is_dropped(child):
            return ""
        tag = child.tag
        if tag == "br":
            return BR
        if tag == "img":
            return self._image(child)
        if tag == "a":
            return self._link(child)
        if tag in ("strong", "b"):
            return _wrap(self._inline(child), "**")
        if tag in ("em", "i"):
            return _wrap(self._inline(child), "*")
        if tag == "code":
            return _wrap(self._inline(child), "`")
        if tag in BLOCK_TAGS:
            return " " + self._inline(child) + " "
        return self._inline(child)

    def _link(self, node: Node) -> str:
        text = self._inline(node)
        match = ENTITY_RE.search(node.attrs.get("href", ""))
        if not match:
            return text
        ref = f"{match.group(1)} {match.group(2)}"
        label = text.strip()
        if not label:
            return ref
        if label.endswith(f"({ref})"):
            return text
        return f"{text[:len(text) - len(text.lstrip())]}{label} ({ref}){text[len(text.rstrip()):]}"

    def _image(self, node: Node) -> str:
        if not is_content_image(node):
            return ""
        label = _first(self._captions.get(id(node)), node.attrs.get("alt"), node.attrs.get("title"), "image")
        target = urllib.parse.quote(self.image_ref(image_src(node)), safe=":/?#[]@!$&'*+,;=%~")
        return f"{BR}[diagram: {label.replace(']', ')')}]({target}){BR}"

    def _flush(self) -> None:
        if not self._buf:
            return
        text = "".join(self._buf)
        self._buf = []
        for para in re.split(f"{BR}[ ]*(?:{BR}[ ]*)+", text):
            para = _clean(para)
            if not para:
                continue
            first, _, rest = para.partition("\n")
            if _is_bold_line(first) and rest:
                self.blocks += [Block(PSEUDO_HEADING, first), Block(0, rest)]
            else:
                self.blocks.append(Block(PSEUDO_HEADING if _is_bold_line(para) else 0, para))


def _owned(widget: Node, selector: str, owner_class: str) -> list[Node]:
    """Descendants matching selector whose nearest owner_class ancestor is widget, so nested widgets keep theirs."""
    found = []
    for node in widget.descendants():
        if node.matches(selector):
            owner = node.parent
            while owner is not None and owner is not widget and owner_class not in owner.classes():
                owner = owner.parent
            if owner is widget:
                found.append(node)
    return found


def _is_bold_line(text: str) -> bool:
    return len(text) <= 100 and re.fullmatch(r"\*\*[^*\n]+\*\*:?", text) is not None


def _table_rows(table: Node) -> Iterator[Node]:
    for child in table.elements():
        if child.tag == "tr":
            yield child
        elif child.tag in ("thead", "tbody", "tfoot"):
            yield from (tr for tr in child.elements() if tr.tag == "tr")


def _wrap(inner: str, mark: str) -> str:
    core = inner.strip()
    if not core or core.replace(BR, "").strip() == "":
        return inner
    return f"{inner[:len(inner) - len(inner.lstrip())]}{mark}{core}{mark}{inner[len(inner.rstrip()):]}"


def _clean(text: str) -> str:
    lines = [WS_RE.sub(" ", line).strip() for line in text.replace(BR, "\n").split("\n")]
    return "\n".join(line for line in lines if line)


def select_sections(blocks: list[Block], wanted: Iterable[str]) -> tuple[list[Block], list[str], list[str]]:
    picked: list[Block] = []
    found: list[str] = []
    missing: list[str] = []
    for pattern in wanted:
        needle = " ".join(words(pattern))
        start = next((i for i, b in enumerate(blocks)
                      if b.level and needle in " ".join(words(b.text))), None)
        if start is None:
            missing.append(pattern)
            continue
        level = blocks[start].level
        end = next((i for i in range(start + 1, len(blocks)) if blocks[i].level and blocks[i].level <= level),
                   len(blocks))
        picked.extend(blocks[start:end])
        found.append(_clean(blocks[start].text.lstrip("#").replace("**", "")))
    return picked, found, missing


def join_blocks(blocks: list[Block]) -> str:
    out: list[str] = []
    for i, block in enumerate(blocks):
        if i:
            out.append("\n" if block.kind == "li" and blocks[i - 1].kind == "li" else "\n\n")
        out.append(block.text)
    return "".join(out)


@dataclass
class Rendered:
    markdown: str
    info: PageInfo
    sections_found: list[str]
    sections_missing: list[str]


ImageRef = Callable[[str, str], str]


def render_page(text: str, url: str, *, sections: Iterable[str] = (), image_ref: ImageRef | None = None,
                header: Iterable[tuple[str, str]] = ()) -> Rendered:
    """image_ref(src, base) turns an image's src attribute into the link the text shows."""
    doc = parse_html(text)
    body = select_body(doc, detect_site(url, doc))
    info = page_info(doc, body, text)
    base = _base_url(doc, url)
    resolve = image_ref or (lambda src, page_base: urllib.parse.urljoin(page_base, src))
    blocks = Renderer(lambda src: resolve(src, base)).render(body)
    if blocks and blocks[0].kind == "h" and words(blocks[0].text) == words(info.title):
        blocks = blocks[1:]
    wanted = list(sections)
    found: list[str] = []
    missing: list[str] = []
    if wanted:
        picked, found, missing = select_sections(blocks, wanted)
        if picked:
            blocks = picked
    lines = [f"# {info.title or 'Untitled'}", ""]
    meta = [("Source", url), ("Author", info.author), ("Updated", info.updated)]
    if info.published and info.published != info.updated:
        meta.append(("Published", info.published))
    meta += list(header)
    if wanted:
        meta.append(("Sections", "; ".join(found) if found else "none found, whole page kept"))
        if found and missing:
            meta.append(("Sections not found", "; ".join(missing)))
    lines += [f"- {key}: {value}" for key, value in meta if value]
    markdown = "\n".join(lines) + "\n\n" + join_blocks(blocks).rstrip() + "\n"
    return Rendered(markdown, info, found, missing)


# ---------------------------------------------------------------------------------------------------
# Cache


def raw_path(cache: pathlib.Path, src: Source) -> pathlib.Path:
    return cache / "raw" / src.tag / f"{src.slug}.html"


def text_path(cache: pathlib.Path, slug: str, tag: str) -> pathlib.Path:
    return cache / "text" / f"{slug}.{tag}.md"


def image_path(cache: pathlib.Path, src: Source, url: str) -> pathlib.Path:
    name = urllib.parse.unquote(urllib.parse.urlsplit(url).path.rsplit("/", 1)[-1])
    name = re.sub(r"[^A-Za-z0-9._-]+", "_", name)[-60:] or "image"
    return cache / "img" / src.tag / src.slug / f"{hashlib.sha1(url.encode('utf-8')).hexdigest()[:10]}-{name}"


def _read_json(path: pathlib.Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return data if isinstance(data, dict) else {}


def _write_json(path: pathlib.Path, data: dict) -> None:
    path.write_text(json.dumps(data, indent=2, sort_keys=True, ensure_ascii=False) + "\n", encoding="utf-8",
                    newline="\n")


def _relpath(path: pathlib.Path, start: pathlib.Path) -> str:
    try:
        return pathlib.Path(os.path.relpath(path, start)).as_posix()
    except ValueError:
        return path.as_posix()


@dataclass
class SavedPage:
    path: pathlib.Path
    text: str
    location: str
    slug: str | None
    how: str


def read_saved(path: pathlib.Path) -> tuple[str, str]:
    raw = path.read_bytes()
    if path.suffix.lower() in (".mhtml", ".mht"):
        message = email.message_from_bytes(raw, policy=email.policy.default)
        snapshot = str(message.get("Snapshot-Content-Location", ""))
        for part in message.walk():
            if part.get_content_type() == "text/html":
                payload = part.get_payload(decode=True) or b""
                location = str(part.get("Content-Location", "")) or snapshot
                return decode_html(payload, part.get_content_charset()), location
        return "", snapshot
    return decode_html(raw), ""


def scan_saved(manifest: Manifest, cache: pathlib.Path) -> list[SavedPage]:
    folder = cache / SAVED_TAG
    pages = []
    files = sorted((p for p in folder.iterdir() if p.is_file() and p.suffix.lower() in SAVED_SUFFIXES),
                   key=lambda p: p.stat().st_mtime) if folder.is_dir() else []
    for path in files:
        text, location = read_saved(path)
        slug, how = identify_saved(manifest, text, location)
        pages.append(SavedPage(path, text, location, slug, how))
    return pages


def identify_saved(manifest: Manifest, text: str, location: str = "") -> tuple[str | None, str]:
    doc = parse_html(text)
    info = page_info(doc, None, text)
    for how, url in (("canonical link", info.canonical), ("og:url", info.og_url),
                     ("saved-from comment", info.saved_from), ("MHTML location", location)):
        slug = manifest.wowhead_slug(url) if url else None
        if slug:
            return slug, how
    title_node = doc.find("title")
    title = _first(doc.meta("og:title"), title_node and title_node.text())
    slug = manifest.title_slug(title) if title else None
    if slug:
        return slug, "title"
    return None, f"no canonical link, saved-from URL or title matched the manifest (title {title!r})"


# ---------------------------------------------------------------------------------------------------
# Commands


def cmd_fetch(manifest: Manifest, cache: pathlib.Path, slugs: list[str] | None, images: bool, fetcher: Fetcher,
              out: TextIO, today: str) -> int:
    log_path = cache / "fetch-log.json"
    log = _read_json(log_path)
    for src in manifest.select(slugs):
        print(f"fetch {src.slug} [{src.tag}] {src.url}", file=out, flush=True)
        outcome = fetcher.fetch(src.url)
        previous = log.get(src.key, {})
        entry: dict[str, object] = {"url": src.url, "date": today, "status": outcome.status}
        if outcome.reason:
            entry["reason"] = outcome.reason
        if outcome.status == "ok":
            path = raw_path(cache, src)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(outcome.body)
            entry["final_url"] = outcome.url
            entry["charset"] = charset_of(outcome.content_type) or ""
            if images:
                entry["images"] = fetch_images(fetcher, cache, src, outcome, out)
        elif raw_path(cache, src).exists() and previous.get("final_url"):
            entry["kept"] = previous.get("kept") or previous.get("date")
            for key in ("final_url", "charset", "images"):
                if key in previous:
                    entry[key] = previous[key]
        log[src.key] = entry
        print(f"  {outcome.status}{': ' + outcome.reason if outcome.reason else ''}", file=out, flush=True)
        _write_json(log_path, log)
    write_index(manifest, cache, log, _read_json(cache / "convert-log.json"))
    return 0


def fetch_images(fetcher: Fetcher, cache: pathlib.Path, src: Source, outcome: Outcome, out: TextIO) -> dict[str, int]:
    doc = parse_html(decode_html(outcome.body, charset_of(outcome.content_type)))
    base = _base_url(doc, outcome.url)
    counts = {"ok": 0, "refused": 0, "error": 0}
    for src_attr in article_images(select_body(doc, detect_site(outcome.url, doc))):
        url = urllib.parse.urljoin(base, src_attr)
        path = image_path(cache, src, url)
        if path.exists():
            counts["ok"] += 1
            continue
        result = fetcher.fetch(url, MAX_IMAGE_BYTES)
        if result.status == "ok" and not result.content_type.lower().startswith("image/"):
            result = result._replace(status="error", reason=f"not an image ({result.content_type or 'no type'})")
        if result.status == "ok":
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(result.body)
        else:
            print(f"  image {result.status}: {url}: {result.reason}", file=out, flush=True)
        counts[result.status] += 1
    return counts


def _base_url(doc: Document, url: str) -> str:
    base = doc.find("base")
    return urllib.parse.urljoin(url, base.attrs.get("href", "")) if base is not None else url


def cmd_convert(manifest: Manifest, cache: pathlib.Path, slugs: list[str] | None, out: TextIO) -> int:
    log = _read_json(cache / "fetch-log.json")
    conv_path = cache / "convert-log.json"
    conv = _read_json(conv_path)
    text_dir = cache / "text"
    text_dir.mkdir(parents=True, exist_ok=True)
    for src in manifest.select(slugs):
        raw = raw_path(cache, src)
        if not raw.exists():
            continue
        entry = log.get(src.key, {})
        page_url = entry.get("final_url") or src.url

        def image_ref(value: str, base: str, src=src) -> str:
            url = urllib.parse.urljoin(base, value)
            local = image_path(cache, src, url)
            return _relpath(local, text_dir) if local.exists() else url

        text = decode_html(raw.read_bytes(), entry.get("charset") or None)
        fetched = entry.get("kept") or entry.get("date") or ""
        rendered = render_page(text, page_url, sections=src.sections, image_ref=image_ref,
                               header=[("Fetched", str(fetched))])
        conv[src.key] = _write_text(text_path(cache, src.slug, src.tag), rendered, src.sections)
        print(f"convert {src.slug} [{src.tag}] -> {text_path(cache, src.slug, src.tag).name}", file=out)
    saved = scan_saved(manifest, cache)
    chosen: dict[str, SavedPage] = {}
    for page in saved:
        if page.slug is None:
            print(f"unmatched saved page {page.path.name}: {page.how}", file=out)
        elif not slugs or page.slug in slugs:
            if page.slug in chosen:
                print(f"{page.slug}: {page.path.name} is newer than {chosen[page.slug].path.name}, using it", file=out)
            chosen[page.slug] = page
    for slug in manifest.wowhead:
        if slug not in chosen and (not slugs or slug in slugs):
            conv.pop(f"{slug}|{SAVED_TAG}", None)
            text_path(cache, slug, SAVED_TAG).unlink(missing_ok=True)
    for slug, page in chosen.items():
        def saved_ref(value: str, base: str, folder=page.path.parent) -> str:
            if urllib.parse.urlsplit(value).scheme in ("http", "https"):
                return value
            local = (folder / urllib.parse.unquote(value.split("?", 1)[0].split("#", 1)[0])).resolve()
            return _relpath(local, text_dir) if local.exists() else urllib.parse.urljoin(base, value)

        saved_on = dt.date.fromtimestamp(page.path.stat().st_mtime).isoformat()
        rendered = render_page(page.text, manifest.wowhead[slug], image_ref=saved_ref,
                               header=[("Saved", f"{saved_on} ({page.path.name}, matched by {page.how})")])
        key = f"{slug}|{SAVED_TAG}"
        conv[key] = _write_text(text_path(cache, slug, SAVED_TAG), rendered, ())
        conv[key]["file"] = page.path.name
        conv[key]["saved"] = saved_on
        print(f"convert {slug} [{SAVED_TAG}] {page.path.name} -> {text_path(cache, slug, SAVED_TAG).name}", file=out)
    _write_json(conv_path, conv)
    write_index(manifest, cache, log, conv, saved)
    return 0


def _write_text(path: pathlib.Path, rendered: Rendered, sections: Iterable[str]) -> dict[str, object]:
    path.write_text(rendered.markdown, encoding="utf-8", newline="\n")
    entry: dict[str, object] = {"bytes": len(rendered.markdown.encode("utf-8"))}
    if tuple(sections):
        entry["sections_found"] = rendered.sections_found
        entry["sections_missing"] = rendered.sections_missing
    return entry


def write_index(manifest: Manifest, cache: pathlib.Path, log: dict, conv: dict,
                saved: list[SavedPage] | None = None) -> None:
    if saved is None:
        saved = scan_saved(manifest, cache)
    lines = [f"# Guide cache: {manifest.name}", "",
             f"Written by guide_text.py from `{manifest.path.name}`. Text files are `text/<slug>.<source>.md`.", "",
             "| Slug | Source | URL | Date | Status | Text |", "|---|---|---|---|---|---|"]
    for slug in manifest.slugs:
        rows = 0
        for src in (s for s in manifest.sources if s.slug == slug):
            lines.append(_index_row(cache, src, log.get(src.key), conv.get(src.key)))
            rows += 1
        if slug in manifest.wowhead:
            entry = conv.get(f"{slug}|{SAVED_TAG}")
            path = text_path(cache, slug, SAVED_TAG)
            if entry and path.exists():
                lines.append(_row(slug, SAVED_TAG, manifest.wowhead[slug], str(entry.get("saved", "")),
                                  f"ok: saved page `{entry.get('file', '?')}`", _size(path)))
            else:
                lines.append(_row(slug, SAVED_TAG, manifest.wowhead[slug], "", "missing: not saved yet", ""))
            rows += 1
        if not rows or slug in manifest.notes:
            lines.append(_row(slug, "", "", "", f"missing: {manifest.notes.get(slug, 'no source listed')}", ""))
    unmatched = [p for p in saved if p.slug is None]
    if unmatched:
        lines += ["", "## Saved pages that matched no slug", ""]
        lines += [f"- `{p.path.name}`: {p.how}" for p in unmatched]
    (cache / "INDEX.md").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def _index_row(cache: pathlib.Path, src: Source, entry: dict | None, conv: dict | None) -> str:
    path = text_path(cache, src.slug, src.tag)
    if not entry:
        return _row(src.slug, src.tag, src.url, "", "missing: not fetched yet", "")
    status = str(entry.get("status", "?"))
    if entry.get("reason"):
        status += f": {entry['reason']}"
    if entry.get("kept"):
        status += f" (kept the copy from {entry['kept']})"
    images = entry.get("images")
    if isinstance(images, dict) and any(images.values()):
        status += "; images " + ", ".join(f"{n} {k}" for k, n in images.items() if n)
    if conv and conv.get("sections_found"):
        status += "; sections " + "; ".join(conv["sections_found"])
        if conv.get("sections_missing"):
            status += "; not found " + "; ".join(conv["sections_missing"])
    elif conv and "sections_found" in conv:
        status += "; sections not found, whole page kept"
    return _row(src.slug, src.tag, src.url, str(entry.get("date", "")), status, _size(path) if path.exists() else "")


def _row(*cells: str) -> str:
    return "|" + "|".join(f" {cell.replace('|', '/')} " if cell else " " for cell in cells) + "|"


def _size(path: pathlib.Path) -> str:
    return f"{path.stat().st_size / 1024:.1f} KB"


def main(argv: list[str] | None = None, *, fetcher: Fetcher | None = None, today: str | None = None) -> int:
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n", 1)[0])
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("fetch", "convert"):
        command = commands.add_parser(name)
        command.add_argument("--manifest", required=True, type=pathlib.Path)
        command.add_argument("--cache", required=True, type=pathlib.Path)
        command.add_argument("--slug", action="append", help="only this slug; repeatable")
        if name == "fetch":
            command.add_argument("--images", action="store_true", help="also download each article's images")
    args = parser.parse_args(argv)
    try:
        manifest = load_manifest(args.manifest)
    except ManifestError as err:
        print(f"error: {err}", file=sys.stderr)
        return 2
    unknown = sorted(set(args.slug or ()) - set(manifest.slugs))
    if unknown:
        print(f"error: unknown slug(s) {', '.join(unknown)}", file=sys.stderr)
        return 2
    (args.cache / SAVED_TAG).mkdir(parents=True, exist_ok=True)
    if args.command == "fetch":
        return cmd_fetch(manifest, args.cache, args.slug, args.images, fetcher or Fetcher(), sys.stdout,
                         today or dt.date.today().isoformat())
    return cmd_convert(manifest, args.cache, args.slug, sys.stdout)


if __name__ == "__main__":
    sys.exit(main())

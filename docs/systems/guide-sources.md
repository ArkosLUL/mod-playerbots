# Raid guide sources

Raid strategy takes mechanics (ids, timers, radii, targeting) from the core script and tactics (kill
order, positions, assignments) from community guides; on a conflict the script wins. Guides reach agents
only through `tools/guides/guide_text.py`, which fetches the pages a site lets it read and converts them,
plus Wowhead pages the user saves, into Markdown in a cache outside git.

## Sources

| Source | Tag | Role | Access |
|---|---|---|---|
| Warcraft Tavern | `wt25`, `wt10` | Base guide. 25-man at `warcrafttavern.com/wotlk/guides/<slug>/`, a real 10-man edition at `<slug>-10/` | robots.txt allows the tool |
| Wowhead | `wowhead` | Detail on top of the base | only pages the user saves |
| instant-carry | `ic` | Raid overview with a short section per boss | allowed |
| Old raid blogs (Blogger, WordPress.com) | `blog` | Trash and gauntlets the main guides skip | allowed |
| noobtoboss | `ntb` | — | robots.txt disallows `ClaudeBot`: refused |
| Icy Veins, warcraft.wiki.gg, wowwiki-archive.fandom.com | — | — | answer scripts with 403; not worked around |

Warcraft Tavern serves no author or update date, so its text files carry the fetch date only.

## Fetch rules

The tool enforces these; no agent fetches a guide page by any other route.

- **robots.txt** is obeyed for the tool's agent `mod-playerbots-guide-reader/1.0` and for Anthropic's
  `ClaudeBot`, `Claude-Web` and `anthropic-ai`: a URL any of them may not fetch is refused. A robots.txt
  that fails or answers 5xx refuses the host; a 404 means no rules.
- **Wowhead** disallows `/` for all three Anthropic agents. A plain request still gets a 200, so the
  opt-out, not a wall, is why it is never fetched. The tool refuses `wowhead.com` and its asset host
  `zamimg.com` outright, redirects included.
- **Pace:** at least 2 s between requests to one host, longer if `Crawl-delay` or `Request-rate` asks.
- **403 or 429** (robots.txt included) stops that host for the run, never retried under another agent.
- Each redirect hop is re-checked against all of the above.

## Commands and cache

From the module root, for ICC (cache `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-icc-wt/_guides`):

```
python -I tools/guides/guide_text.py convert --manifest tools/guides/sources/icc.json --cache <cache>
python -I tools/guides/guide_text.py fetch --images --manifest tools/guides/sources/icc.json --cache <cache>
```

`convert` picks up newly saved Wowhead pages and rewrites every text file and `INDEX.md`; run it before
reading the guides. `fetch` is needed only for a new or changed manifest entry, since raw pages are kept.
Both take `--slug S` (repeatable). A failed re-fetch keeps the earlier raw copy, and `INDEX.md` says so.

| Path in the cache | Holds |
|---|---|
| `INDEX.md` | Per slug and source: URL, date, status (ok, refused with the reason, missing), text size; saved pages that matched no slug |
| `text/<slug>.<tag>.md` | Converted text; read these |
| `raw/<tag>/<slug>.html`, `img/<tag>/<slug>/` | Fetched pages and their article images |
| `wowhead/` | Pages the user saves from Wowhead, any file name |
| `fetch-log.json`, `convert-log.json` | What each command did, for `INDEX.md` |

**Manifest** `tools/guides/sources/<raid>.json`, URLs only. Its slugs are the raid's trace-name
contract slugs plus one for the raid overview. Per slug:

- `sources`: `tag`, `url`, and optional `sections`, which keeps only the parts of a multi-topic page under
  headings containing that text, each up to the next heading of equal or higher rank. A bold-only line
  counts as a heading below every real one, since blogs mark sections that way. No match keeps the whole
  page, and `INDEX.md` says so.
- `wowhead`: the guide URL a saved page must match; `INDEX.md` lists the ones not saved yet.
- `wowhead_title` overrides the title words a saved page is matched by; `note` explains a slug no guide
  covers.

**Saved Wowhead pages** may be "HTML only", "complete" or `.mhtml`. A page matches by canonical link,
`og:url`, the browser's `saved from url` comment or the MHTML `Content-Location`, then by title words
(the URL's last segment minus "strategy"/"guide"). The newest file wins per slug.

**Conversion** keeps title, author, dates, headings with their levels, paragraphs, lists and tables.
Tab and accordion titles (Warcraft Tavern's role summaries and ability lists) become headings under the
heading they sit below. A link carrying `spell=N`, `npc=N` or `item=N`, or the `/spell/N` form
Warcraft Tavern's wowclassicdb links use, becomes `Name (spell N)`. An image becomes
`[diagram: <caption or alt>](<local path or URL>)`; icons under 64 px are dropped. A per-site body
selector, falling back to the densest text block, drops menus, comments, ads, tables of contents and
related-guide strips.

## Guide text stays out of git

The cache sits outside every worktree and is never committed. Guide text enters docs only as
paraphrased decisions naming the source, never pasted. The tool's tests use synthetic pages only.

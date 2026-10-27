# r0b-guides — guide fetcher and converter, ICC cache

Wave 1, parallel with `r0a-restructure`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Python only;
no C++.

## Scope

1. **`tools/guides/guide_text.py`**, stdlib only, run as `python -I`, UTF-8 output whatever the console
   codepage (`sys.stdout.reconfigure(encoding="utf-8")`; this console is cp1251). It is generic across
   raids: a manifest per raid names the sources.
   - **Manifest** `tools/guides/sources/icc.json`. For each contract slug (the ICC README naming table):
     the source URLs with a source tag (`wt25`, `wt10`, `ntb`, `blog`, …) and the Wowhead guide URL it
     expects from a saved page. Plus the ICC overview. The manifest holds URLs only, never page text.
   - **`fetch --manifest <m> --cache <dir> [--slug s] [--images]`**:
     - send the user agent `mod-playerbots-guide-reader/1.0`;
     - read each host's robots.txt with `urllib.robotparser`, and refuse a URL it disallows for that
       agent or for `ClaudeBot`, `Claude-Web` or `anthropic-ai`;
     - refuse wowhead.com outright;
     - honour `Crawl-delay`, and wait at least 2 s between requests to one host;
     - on 403 or 429, stop for that host and record it, never retrying under another agent;
     - save raw HTML under `<cache>/raw/<source>/<slug>.html`;
     - with `--images`, download the article's images through the same checks into `<cache>/img/`.
   - **`convert --manifest <m> --cache <dir>`**: turn every raw page plus every page under
     `<cache>/wowhead/` into `<cache>/text/<slug>.<source>.md`.
     - Identify a saved Wowhead page by its canonical link or its `saved from url` comment, falling
       back to the title against the manifest. Saved pages are whatever the user's browser wrote,
       "HTML only" or "complete".
     - Keep: title, author and updated date, headings with their levels, paragraphs, lists, tables,
       and the role sections ("Tips for Tanks" etc.).
     - Links to spells, NPCs and items (`spell=N`, `npc=N`, `item=N` in any href) become
       `Name (spell N)`.
     - Images become `[diagram: <alt or caption>](<url or img path>)`.
     - Drop menus, comments, ads and related-guide lists, using a per-site body selector (Wowhead
       guide body, WordPress article content, Blogger post body) and a generic fallback.
   - **`INDEX.md`** in the cache: per slug and source, the URL, fetch date, status (ok, refused with
     the robots or HTTP reason, missing), and text size.
2. **Tests** in `tools/guides/tests/`, on synthetic fixtures only (no copied guide text, no network):
   - a Wowhead-shaped saved page (canonical link, headings, spell links, a diagram);
   - a WordPress article;
   - a Blogger post;
   - robots refusal from an injected robots.txt that disallows `ClaudeBot`;
   - the hard Wowhead refusal;
   - title fallback;
   - non-ASCII text.
3. **Fill the ICC cache** at `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-icc-wt/_guides/`, outside
   every worktree and never committed. Create `wowhead/` there if it's missing (the user saves pages
   into it). Run `fetch` with `--images` and then `convert` for every manifest entry. Record refusals
   and misses in `INDEX.md` and the report.
4. **Doc** `docs/systems/guide-sources.md`, folded with `compact-docs-writer`:
   - which sources are used and why;
   - the robots rule, including Wowhead's opt-out for Anthropic's agents, so Wowhead comes only from
     pages the user saves;
   - the sites that answer scripts with 403 and are not worked around;
   - the commands and the cache layout;
   - that guide text is never committed, only paraphrased into decisions.
   - Link it from `docs/README.md` where the systems docs are listed.

## Verified facts

- Warcraft Tavern allows this agent: its robots.txt only blocks admin, armory and community paths. It
  answers `mod-playerbots-guide-reader/1.0` with 200.
  - 25-man guides: `https://www.warcrafttavern.com/wotlk/guides/<slug>/`, with slugs
    `icecrown-citadel-25-raid-guide`, `lord-marrowgar`, `lady-deathwhisper`, `gunship-battle`,
    `deathbringer-saurfang`, `festergut`, `rotface`, `professor-putricide`, `blood-prince-council`,
    `blood-queen-lanathel`, `valithria-dreamwalker`, `sindragosa`, `the-lich-king`.
  - The 10-man guide is reported at the same slug plus `-10` (e.g. `lord-marrowgar-10/`). Verify it.
- Backups:
  - noobtoboss: `https://noobtoboss.com/the-lich-king-guide-icc-raid-wotlk-classic/`,
    `/gunship-battle-guide/`, `/lady-deathwhisper-guide/`, `/valithria-dreamwalker-guide/`,
    `/sindragosa-guide-icc/`, and the overview `/icecrown-citadel-guide/`;
  - instant-carry: `https://instant-carry.com/blog/wotlk-classic-icecrown-citadel-guide/`.
- Trash and gauntlet blogs:
  - Rotting Frost Giant:
    `https://dontstandinthefire.wordpress.com/2010/02/04/guide-to-completing-the-icecrown-citadel-weekly-raid-quests/`;
  - Stinky and Precious:
    `https://battlechicken.wordpress.com/2010/01/14/icecrown-citadel-the-plagueworks-getting-there/`;
  - Svalna, the Sindragosa gauntlet, Rimefang and Spinestalker:
    `http://mischiefmagicandrocketscience.blogspot.com/2010/09/mages-guide-to-icecrown-citadel-part_18.html`.
  - Nothing covers the Crimson Hall trash.
- Check robots.txt for each of these hosts through the tool; nobody has checked them yet.
- Wowhead guide URLs the user is asked to save:
  `https://www.wowhead.com/wotlk/guide/raids/icecrown-citadel/<x>`, with `<x>` = `overview`,
  `lord-marrowgar-strategy`, `lady-deathwhisper-strategy`, `gunship-battle-strategy`,
  `deathbringer-saurfang-strategy`, `festergut-strategy`, `rotface-strategy`,
  `professor-putricide-strategy`, `blood-prince-council-strategy`, `blood-queen-lanathel-strategy`,
  `valithria-dreamwalker-strategy`, `sindragosa-strategy`, `the-lich-king-strategy`.
  - The article text, headings and `/wotlk/spell=N` links are in the served HTML; no script is needed
    to render them.
  - Wowhead's robots.txt disallows `/` for `anthropic-ai`, `Claude-Web` and `ClaudeBot`.
- Icy Veins (`/wotlk-classic/...`), warcraft.wiki.gg and wowwiki-archive.fandom.com answer 403 to
  scripts.

## Owns

`tools/guides/**`, `docs/systems/guide-sources.md`, the link line in `docs/README.md`, this brief, and
the cache directory above (not in git).

## Task list

- [x] `tools/guides/guide_text.py` with `fetch` and `convert`, stdlib only, UTF-8 output.
- [x] Manifest `tools/guides/sources/icc.json`: 17 slugs (the 15 contract slugs plus the overview
  `icecrown-citadel`), 38 source URLs, 13 Wowhead guide URLs.
- [x] Tests `tools/guides/tests/test_guide_text.py` on synthetic fixtures (Wowhead saved page, MHTML,
  WordPress, Blogger, robots refusals, Wowhead refusal, title fallback, non-ASCII, fetch/convert/INDEX).
- [x] Cache filled with `fetch --images` and `convert`; `INDEX.md` written.
- [x] `docs/systems/guide-sources.md`, linked from `docs/README.md`.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| noobtoboss is not used: its robots.txt disallows `ClaudeBot` (checked through the tool 2026-10-27). Lanes that list it get Warcraft Tavern 25/10 and instant-carry. | robots rule in this brief | The user saves noobtoboss pages by hand; `convert` would need a saved-pages folder for non-Wowhead sites |
| `zamimg.com` is refused like `wowhead.com`: it is Wowhead's image and icon host. | "never wowhead.com by any route" | Fetch it under its own robots.txt |
| The overview slug is `icecrown-citadel`, the raid's doc directory name. | `docs/raids/icecrown-citadel/` | `overview` |
| Warcraft Tavern's 10-man overview exists at `icecrown-citadel-10-raid-guide/` and is listed as `wt10`. Every boss `-10` page is a real 10-man edition (e.g. Marrowgar: 3 players at range, 1 impale). | fetched pages | 25-man overview only |
| Sections: instant-carry's overview is cut to "Gunship Battle" for the gunship slug and kept whole for the overview; the Svalna blog to "The Frostwing Halls"; the weekly-quest blog to "Securing the Ramparts" for the frost giant. The Sindragosa gauntlet and the Stinky and Precious blogs stay whole: their parts have no heading of their own. | page structure | Paragraph-anchored sections |
| Rimefang and Spinestalker have no source of their own; their text is in `sindragosa-gauntlet.blog.md`. | the blog | A `blog` source on the `sindragosa` slug |
| Content hidden by CSS or `hidden` is kept: tabs and accordions hide every panel but one until clicked. | Warcraft Tavern markup | Drop hidden content |
| A bold-only line counts as a heading below every real one, for sections in blogs. | Blogger and WordPress.com posts | Real headings only |
| A robots.txt that fails or answers 5xx refuses the host; 401, 403 and 429 on it stop the host. | `urllib.robotparser`'s own handling, made stricter | Treat a missing answer as allow-all |
| Images with a declared side under 64 px are dropped as icons. | Warcraft Tavern's 18 px spell icons | Keep every image |

## Carried over

- `r14-closeout`: `docs/plans/toc-rework/toc-rework.PLAN.md` says "Wowhead blocks automated reads".
  The real reason is its robots opt-out for Anthropic's agents. Another session has that file dirty,
  so it isn't edited here.
- Merge stage: the briefs of `r3`, `r4`, `r11`, `r12` and `r13` list noobtoboss among their guides; it
  is refused by robots.txt, so those lanes report it as a missing source.
- User: no Wowhead page is saved yet. `INDEX.md` lists the 13 URLs to save into
  `build-icc-wt/_guides/wowhead/`; the next `convert` picks them up.

## Known gaps

- Wowhead's body selectors (`#guide-body`, `.guide-body`, …) are unverified: no saved page existed to
  check. A miss falls back to the densest text block; compare the first converted page with the saved
  one.
- Nothing covers the Crimson Hall trash (`blood-prince-trash`).
- Warcraft Tavern pages carry no author or update date; their text files show the fetch date only.
- `convert` reads saved pages for Wowhead only.

## Blocked

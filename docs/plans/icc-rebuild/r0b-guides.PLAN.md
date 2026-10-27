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

## Decisions for review

## Carried over

- `r14-closeout`: `docs/plans/toc-rework/toc-rework.PLAN.md` says "Wowhead blocks automated reads".
  The real reason is its robots opt-out for Anthropic's agents. Another session has that file dirty,
  so it isn't edited here.

## Known gaps

## Blocked

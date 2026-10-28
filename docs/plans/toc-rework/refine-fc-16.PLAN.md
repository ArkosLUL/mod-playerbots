# refine-fc-16: no holders, an ice duty per bot DK, a grip chain for any bot DK

Wave 4y, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4x `52b587fd7`, which the
pulls below ran.

## Why

Three 25H wipes on 2026-10-28 (`env/dist/logs/botobs/649_2_*_17932{20208,20633,21081}.ndjson`: 22, 27, 28 deaths)
against the 10-25 kill (`649_39_liandra-suncaller_1792947097`, 7 deaths).

- **Melee champions did the killing:** 49-64% of the champion damage behind the first 8 deaths (kill 31%).
- **Holders fail both ways:** heroic has no taunt, so a champion sat on its holder a median 5-29% of the hold (kill
  57%). Loose ones killed the soft targets; every holder that kept its champion died with it.
- **Ret and Enh, no gap closer, were top killers:** 17% (P2) and 26% (P3).
- **fc-15 regression:** Hand of Protection went to melee bots at the pull, before any hit (predicted picks).
- fc-15's heal reach worked (kites past 30 yd of every healer 30/53 → 2-6 of 38-50); the Reincarnation wait is too
  loose (3 of 6 died again within 10 s), left as a gap.

## Scope (the user's calls, 2026-10-28)

1. **Holders out.** The tactic is the kite.
2. **Ice duty** (`fc.ice`, `fc.iceoff`): each bot DK keeps one melee champion in Chains of Ice, dealt Ret, Enh, Rogue,
   DK, Warrior, kill target last, nearest DK; hits it while it's on someone else, kites when it's the victim (a tank
   DK too); Chains at 4 s left, a walk-in to 15 yd; Frost spenders and the class Chains zeroed for it.
3. **CC elsewhere:** snares, peels, CC duties, danger and escapees skip a champion carrying its duty DK's Chains.
4. **Kite:** a crawling chaser (rooted, out of control, 60% run speed or less) counts only within 10 yd.
5. **Grip chain for any bot DK** (`ToCFactionChampionsGripChain.h`, native test): links dealt at the open, tank DKs
   first then by guid, up to three posts (tank line, 54, 78 yd); a no-tank `pull` opens it through a bot DK; skips on
   cooldown, death or timeout (`fc.linkskip`); the player last while a post is free. `fc.link` is a map so a chain
   opened before the trace still shows.
6. **Hand of Protection** on a melee bot only at 50% health or under.
7. **Reader:** `--hold` → `--ice` (uptime from casts, gap, spenders, travel, kills), `--chain` by link post.

Departures from the plan: an ice duty survives a stun on its DK, a link takes a duty once it has gripped, the
physical switch skips an ice DK, and `--peel` leaves the duty's own Chains to `--ice`.

## Targets (next 25H, today's roster)

`--chain` Felesta (post 1) → Obliteration (post 2) → Deathsong; both bot DKs on an ice duty after the chain, uptime
85%+ (strong 70%+), no Frost spender; melee champions well under 45% of the damage behind the first 8 deaths; no
snare, peel or CC on an iced champion; no Hand of Protection on a melee bot above 50%; fewer than 22 deaths; 25N
still kills.

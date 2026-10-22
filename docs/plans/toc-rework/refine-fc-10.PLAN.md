# refine-fc-10: break the focus, predict it, finish the second healer

Wave 4o, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4n `11cd45ed8`; the
pulls below ran the 4m + 4n build.

## Why

2026-10-22: 25N kill `env/dist/logs/botobs/649_6_caiphus-the-stern_1792688949.ndjson` (6:32, both after-kill
chains led to kills); 25H wipe `649_1_caiphus-the-stern_1792693183.ndjson` (2:24, Thrakgar only).

- **How the champions pick** (`boss_faction_champions.cpp:94-113, 205-208`): every 8.75-9.25 s, all ten
  reset each player's threat to `1e7 × score`; score is current health (`> 40000 → 2`, else
  `(60000 − hp)/10000`) × distance (ranged free to 35 yd, melee to 5 yd) × armor (melee only). Damage
  doesn't count. The core switches past 110% (new victim in melee range) or 130%
  (`ThreatManager.cpp:654-664`). Immune to physical = suppressed for every champion
  (`ThreatManager.cpp:123-130`): Hand of Protection, Divine Shield, Ice Block end a focus. Retail also reset
  every 9 s and ganged up; not a bug.
- **Focus:** 5-9 champions on one member in 77% of samples, back to back until death; the four frontline
  melee DPS dead by 0:47 (Mighty 3.4 s after 8 turned to him). Externals landed 1.2-1.8 s after a focus
  started.
- **Divine Shield never cast:** `PickPersonal` asks `Castable(642)`, which fails on the GCD (Divine Shield
  1.5 s, Divine Protection off it), so Divine Protection went out and Forbearance locked the shield out.
- **No second kill:** the after-kill chain carried Liandra, champions chasing others walked within 12 yd,
  `pulled` never held, `FactionChampionsKillSquadWait` kept the melee out 94 s; 99 → 87%.
- Three healthstones at 79-90%.

## Scope (user's calls: Hand of Protection stays with healers and ranged, Pain Suppression unchanged)

1. **Immunities first.** `ToCPredictedHit.focus` (4+ champions, actual or predicted). Divine Shield (non-tank
   paladin, measured hit) and Ice Block (mage, focus hit) count as ready on the GCD or a hardcast; only a
   cooldown or their excluded caster aura (61988, 41425) stops them. Healthstone at 70% or under.
2. **Kill squad** stops waiting once the chain's bot link landed on the latched kill target
   (`FactionChampionsChainLanded`).
3. **Focus clock.** Per champion, a re-pick is a tick where 2+ threat entries moved over 1%. In the lead
   window (1 s before the earliest next re-pick to 250 ms past the latest) predict its victim with the
   script's score and the core's switch rule, counted when it clears the runner-up by 10%. `UpdateProtect`
   and `FactionChampionsPersonalHit` use `max(actual, predicted)`. Probes `fc.repick`, `fc.nextfocus`.
4. **Hand of Protection** on a healer or ranged with 4+ champions on it needs no physical one.
5. **Reader**: `--focus` (episodes, cover in them, prediction hit rate); `--cc` self-break counts only ends
   with over 1 s left; `--personal` flags healthstones above 70%. Tests, docs.

## Targets (next 25H)

`fc.nextfocus` right after 60%+ of re-picks; externals and immunities within 1 s of an episode's start or
before it; paladins' first personal under focus Divine Shield, mages' Ice Block; no healthstone above 70%;
no `kill squad wait` after the chain landed, a second kill; 25N still kills.

# refine-anubarak: heroic burrower tanks, sphere economy, submerge drag, boss-first DPS, the heroic kite

Wave 4y, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[anubarak.md](../../raids/trial-of-the-crusader/anubarak.md). Base: `52b587fd7`; the pulls ran a build carrying
`108783a74` (no Anub'arak code after `20c1cf9af`). Two commits: wipe fixes (A, B, C, E), then the heroic kite (D).

## Findings from the 2026-10-25 pulls

Three 25H wipes, `env/dist/logs/botobs/`. Humans Dragon (paladin tank) and Deathsong (DK). Boss 35.35M, burrower
753k, scarab 212k, sphere 12.9k.

| Pull | Trace | Wipe | Boss | Tanks |
|---|---|---|---|---|
| 1 | `649_39_anub-arak_1792954445.ndjson` | 1:54 | 80.2% | Bulwark only bot tank |
| 2 | `649_39_anub-arak_1792955186.ndjson` | 3:32 | 63.3% | Bulwark main, Ecoterrorist and Dragon on burrowers |
| 3 | `649_39_anub-arak_1792955692.ndjson` | 5:23 | 56.1% | as 2 |

- **Pull 1:** side slots went to Dragon and nobody. Burrowers on a tank 18% of their time, 3-11% on Permafrost,
  two submerges each, 7 alive at once.
- **Pull 2:** north tank patch never latched: both spheres shot for it landed 31.5/31.6 yd from its point (latch
  30). P2's four kites spent all four patches, the side-1 tank patch first (1:30). After the emerge 0 patches, 2
  flying, refused by `ANUBARAK_HEROIC_KITE_RESERVE`. Wave 3 0% on Permafrost, submerged back to full, wave 4
  joined: 8 alive, 28 pairs inside 12 yd.
- **Pull 3:** tank patches landed 15 yd apart, (738.5,129.7) and (723.4,128.5). Bulwark tanked him at their
  midpoint, inside patch0 (0.0 yd); the submerge leg planned at 1:05.4 crawled ~0.9 yd/s (80% slow), he followed
  from 1:12 and submerged 0.7 yd from patch0, which the spike spent at 1:25.7. Deathsong spent patch1 at 4:05.
  Ecoterrorist died 3:30: Expose Weakness ×9, 3 burrowers, Mandibles ×12 (53k of his last 140k).
- **Every pull:** Druidica's Starfall (53190/53195) on sphere duty hits a second sphere, so the pull makes 3 patches;
  shooters open with DoTs (Corruption, Curse of Agony, Immolate, Insect Swarm), so a sphere dies late after
  drifting; Hellflame's imp hits a sphere.
- **Pickup:** first tank 0-23 s after spawn (81 s in pull 1); in every wave 2 the north-exit pair ran loose 13-20 s;
  burrowers on a tank 18/36/41% of their time; two warlocks died 1:00-1:13 every pull. At the spawn of pull 3
  waves 2 and 4 Ecoterrorist was hitting the boss.
- **Boss damage:** raid 138-147k/s in P1. With burrowers focusable DPS spent 55-64% on them and he lost 14-17% a
  phase; pull 2 P1b, no patch, 94% on him: 22% in 70 s. P2 raid damage only 2.8-3.5M (scarabs 61-78%).
- **Kites:** 1-15 s each, every one ending on a patch, 4-5 patches per P2. Dragon (human) marked 31 s, 12 Impales.
- **Fine:** spike dodge (0-1 Impale on non-kiters), Shadow Strike kicks (1 landed in 3 pulls), Penetrating Cold heals.

Spell facts (DBC): Hand of Protection 10 s, 5 min; Sprint and Dash +70% 15 s, 3 min (11.9 yd/s vs the spike's
11.4); Blink 15 s; Demonic Circle 6 min, Teleport 40 yd, 30 s; Death Grip 30 yd, 35 s; Growl 30 yd, 8 s; Righteous
Defense 40 yd, 8 s; Challenging Roar 3 min. Death Grip's core boss check skips the sphere (rank 0, no dungeon-boss
flag). Burrow exits: (694.9,102.5), (732.0,83.4), (694.5,185.4), (740.2,193.4). Sphere slots (z 155.67, wander
20): (786.6,108.2), (806.8,150.6), (759.1,164.0), (744.4,119.5), (710.0,120.8), (706.6,161.5).

## Decisions (the user's calls, 2026-10-25)

- Two bot off-tanks; no human takes a side slot.
- DPS boss first, each wave dead before the next: focus starts once (remaining Permafrost burrower health / raid
  damage rate) + 5 s ≥ time to the next wave, or while he is submerged; phase 3 too.
- Death Grip places tank patches (any unmarked bot DK but the main tank; a DK side tank for its own side; two DKs
  at once); one failed grip (no patch within 3 s and 3 yd of the DK) switches the pull to shooting. Shooting: wait
  up to 10 s for a sphere within 12 yd of the spot, then the nearest within 20 yd; his spot and the side spots
  follow where patches land, a second patch under 13 yd from the first is shot farther out.
- Tank spots at room centre y ± 7.
- Heroic budget: tank refills first; kite spheres per submerge = left / submerges left (projected from his loss
  per surfaced phase), at least 1 while any fly; 10H one tank patch per bot side tank.
- A side tank dies: the other takes all; battle res goes to the dead side tank first.
- P2: side tanks hold their patches, only the main tank picks up scarabs; a marked side tank's burrowers go to the
  other one's patch, the 2-per-patch limit paused.
- Drag window from 70 s.
- Heroic kite only (normal keeps patch-first): run, chained boosts, then immunity, a patch last; ring 40 yd;
  Demonic Circle on the warlock's staging stand before the pull, recast under 100 s left.
- Heroic P2: everyone but the kiter and side tanks gathers ~5 yd past the submerge spot away from the tank
  patches, at most 25 yd from the room centre.

## Task list

### Commit 1

**A. Spheres**
1. Heroic: sphere guard zeroes Starfall for every bot while he is engaged.
2. Guard zeroes periodic, channelled and area spells aimed at a sphere; sphere duty skips warlocks.
3. Latch each side need's sphere while it qualifies (as `StockSphere`).
4. Death Grip placement and its failure switch (decisions); shooters hold fire on a gripped sphere until it is
   within 3 yd of the DK.
5. Shooting fallback (decisions); latch, drag anchor and side spots read real patches.
6. `ANUBARAK_TANK_PATCH_OFFSET` 12 → 7, navprobe-checked.
7. Budget (decisions): no reserve gate for sides, `ANUBARAK_P2_PATCH_STOCK_HEROIC` 0, kite allowance, no heroic
   kite on a tank patch, 10H one tank patch per side tank.

**B. Side tanks**
1. `RankSideTanks` bots only; `anub.sides` probe.
2. Side tanks never target him while he is up; a ready node replaces tank assist.
3. Wave timer (first 5-8 s after engage or emerge, then +45 s), probe `anub.wave`.
4. Ready on its patch (spot before one lands); no scarab taunts from 8 s before a wave.
5. Pickup by exit (north side 0, south side 1, at most 2 a side, decisions for death and marks); cascade:
   Righteous Defense on the ally with most loose burrowers, Challenging Roar/Shout at 2+ within 10 yd, class taunt
   (healer's first), Feral Charge, Intercept, Avenger's Shield, Death Grip, Dark Command, else walk in.
6. P2: hold patches; scarab node main tank only.
7. `NextTankDefensive` at Expose Weakness 5+ or under 50%.
8. Battle res prefers a dead side tank.

**C. Drag**
1. Main tank never within 7.5 yd of a patch; a slowed tank steps out before a leg.
2. `ANUBARAK_SUBMERGE_PREP_MS` 70000.
3. Submerge spot prefers the side away from both tank patches.

**E. DPS:** `GetAnubarakDpsTarget` skull > him > focus burrower, the focus ahead of him per the decisions; rate =
boss plus burrower health lost per second over 10 s.

**Commit 1 as built.** Probes: `anub.side0`/`anub.side1` (side tank guids, replacing `anub.sides`), `anub.wave`
(waves seen), `anub.focusdue` (1 while the focus burrower goes ahead of him), `anub.grip` (1 grip placement, 2
failed), `anub.allow` (heroic kite allowance, written at the submerge), `anub.bside` (per burrower, its side);
per bot `anub.sphere` gains `grip0`/`grip1`, `anub.gripstep` (`walk`/`wait`/`cast`/`kill`/`hold`/`none`),
`anub.sidetank` (`hold`/`pickup`/`ready`), `anub.pickup` gains `stepout`. A grip counts as landed once the sphere
is within 3 yd (and 4 yd up) of the DK; not landed 3 s after the cast is the failure. A ready DK on its spot with
no sphere in reach for 12 s hands that need to the shooters. Pre-pull pickup stand moved 10 yd west of the anchor
(735, 135), navprobe-clean; 40 yd ring and 25 yd ring clean 16/16. Battle res: the lowest-guid bot druid with
Rebirth ready.

### Commit 2

**D. Heroic kite**
1. Ring 40 yd (navprobe), spike route 14.5 yd off every patch it shouldn't take; warlock rings away from its
   circle within 40 yd.
2. Boosts at spike ≤ 15 yd and gaining, chained: Sprint, Dash (Cat Form), Nitro Boosts, Blink along the heading,
   Demonic Circle: Teleport (circle 15+ yd farther from the spike).
3. Then Hand of Protection (claim, 12 yd) or Divine Shield; no Ice Block, Feign Death vetoed when marked; keep
   running.
4. Patch last (spike ≤ 9 yd with nothing left, or immunity ending in 2 s); `kite` duty fires when the immunity
   starts, within the allowance; no kite patch within 16.75 yd of a tank patch.
5. Demonic Circle pre-pull and recast.
6. Kite veto spares the kite's own casts.
7. P2 gathering stand node (dodge overrides).
8. Probe `anub.kitestage`.

### Reader and tests

`anub_arak.py`: `--burrowers` exit, side, spawn to first tank and side tank, side tank's distance from its patch at
spawn, focus start vs next wave; `--spheres` killer (caster, pet, spell, periodic/area), landing distance from its
spot, grips; per-submerge use vs allowance; `--spike` kite stages and boosts; `--threat` main tank in a patch,
submerge spot distance to tank patches; banner for missing side tanks. Tests in `test_anub_arak.py`.

### Close

Each commit: syntax check, pblint, codestyle, Python suite. After commit 2: fold into anubarak.md
(`/compact-docs-writer`), trim `w5-anubarak-rest-rest.PLAN.md` (drop Spider Frenzy spacing, burrower pacing, P2
positioning), delete this file, status row `done in session`.

## Targets (next 25H)

Two patches from the pull, both latched (gripped within 5 yd of their spot, else 22); every burrower on a bot side
tank within 4 s; 80%+ of burrower life on a tank; no P1 death to a burrower on a healer or ranged; both tank
patches alive through every P2; 22%+ per surfaced phase; bot kites 20 s+; patches per submerge within the
allowance; main tank never in a patch; submerge 16.75+ yd from every patch; 25N still kills.

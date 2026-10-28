# Auriaya

No hard mode. Entries are shared; Sonic Screech is the only difficulty pair the strategy reads.

| Mechanic | Ids | Handling |
|---|---|---|
| Sonic Screech | 64422 / 64688 | **Soaked, never dodged** — see below |
| Terrifying Screech | 64386 | Fear every 35s from the pull, so the whole fight is one anti-fear window |
| Sentinel Blast | 64389 | Raid-wide, **not** a cone: no `spell_cone` row, and its SpellScript strips non-players. Healed through |
| Sanctum Sentry | 34014 | Assist tank 0 taunts each loose one; Strength of the Pack (64369) buffs the boss while they live |
| Feral Defender | 34035 | Random aggro (61906) makes it untankable — focus-killed, never tanked |
| Seeping Feral Essence | 34098 | Non-selectable stalker, one per Defender life. Aura 64458 (25N 64676) ticks 64459 (25N 64675) every 1s: ~4,500 damage at exactly **5 yd** from its centre, both sizes. Dodged at 7 |
| Guardian Swarm | 64396 | Tank DoT, left to the generic dispel |
| Enrage | 47008 | 10 min, unhandled — no enrage awareness exists anywhere in the module |

**Sonic Screech is a damage split, not a dodge.** `spell_custom_attr` carries
`SPELL_ATTR0_CU_SHARE_DAMAGE` on both ids (64422 also `IGNORE_ARMOR`; 64688 does not — upstream
asymmetry), so the 120° cone (`spell_cone`; radius index 31, **80 yd**) divides **60,125–69,875** (10N) or **190,000–210,000** (25N) among
everyone it hits. She faces her victim, so the old design — non-tanks sidestepping out while the main
tank arc-stepped her away from the raid centroid — left the tank eating it unsplit, a guaranteed death
at 25N. It also never converged: each tank step swept the cone across the raid, and the bots it clipped
moved, shifting the centroid the tank steered by. Bots soak it now, and there is no
cone node left.

**Anchoring is hybrid**, because she walks to her victim and cannot be pinned to world coordinates
the way XT-002 is:

- **Main tank** → a fixed spot from `ULDUAR_AURIAYA_MAINTANK_SPOTS`. A stationary tank is the entire
  facing control; there is nothing left to steer.
- **Ranged and healers** → a **soak band** off her live boss→victim bearing, not a point: the cone
  reaches 80 yd, so no exact range matters. A bot inside the stay band (±45°, 8-28 yd, 7 yd clear of
  pools) holds; one outside walks to the nearest clear spot (8 yd) on the narrower target band (±30°,
  12-24 yd), so a small turn of hers moves nobody. All bots also hold **while she walks**, or every
  tank station walk re-aims the whole ranged half. Reading her live victim keeps the split working
  when a human tanks.
- **Melee and assist tank 0** → unanchored. Melee sit behind her and do not soak; at either raid size
  the remaining soakers already make each share small.

Its `OwnMovement` rule zeroes generic movers for the anchored roles only, or the anchor oscillates.
It keeps `Attack`, `Reach` and `ReachHeal`: a blanket veto would kill targeting and strand healers
out of heal range. 28 yd stays inside `SpellDistance` (28.5).

**The floor is a round room ~45-50 yd across with a drop past its edge** (navprobe, map 603), not a
box. A fixed spot 20 yd past the tank put the ranged stack 2-3 yd from that drop by the third station.
Soak candidates (9 bearings × 4 ranges) are tried clear-first, then fewest pools, then shortest walk,
each raycast from the bot with `CheckCollisionAndGetValidCoords`; one clipped over 1 yd is past the
edge and dropped.

**The pools are permanent**: summon 64457 has `DurationIndex 21` (−1), no SmartAI touches 34098, and
the Defender's 30s respawn never despawns them, so up to 9 accumulate per pull. Hence stations: three
tank spots 5/15/25 yd from her spawn (1956.2, 49.32, 411.36) along her home facing (−0.955, 0.296),
away from the corridor at +x. The tank takes the **lowest station whose spot is 10 yd clear of every
pool** (pool radius plus its 3 yd tolerance). A fouled spot never turns clean, so the index only moves
west, by construction. All fouled: the spot furthest from its nearest pool. Only the tank computes
it; the rest follow her, so no two bots can disagree.

**Retirement must not read anything derived from her live position**, however much cheaper it looks.
She follows the main tank and the main tank follows the verdict, so it oscillates: retire station 0,
the MT walks to 1, she follows, station 0 reads clean, the MT walks back. A pool *count* with ties to
the lowest index reversed too: one pool on spot 1 fouled all three equally and sent the tank back east.

**Pool distances are centre distances.** `IsWithinDist` adds both units' sizes (the stalker's
combat reach is 1 or 3, a player's 1.5), so a 7 yd grid search reaches 9.5-11.5 yd; `IsInAuriayaPool`
re-tests with `GetExactDist2d`. In a pool is under 7 yd, a clear spot 8, so a bot parked on one can't
drift back into a dodge.

**The escape** goes to the bot's own spot when that is 8 yd clear: the tank's station, a ranged bot's
soak spot. Otherwise, and for melee leashed to her, it takes the nearest ground on the floor within
12 yd: fewest pools, then shortest walk. Furthest-from-pools dragged the tank up to 12 yd and swung
her cone over the raid. It interrupts a cast first and moves at `MOVEMENT_FORCED`, the encounter's
only forced mover: a cast pins the feet ([pitfalls](../../engine/pitfalls.md#a-bot-mid-cast-cannot-be-moved-at-all)),
and a 25N warlock once stood 37 s in a pool chain-casting. `AuriayaRaidPositionTrigger` stands down while
the bot is in a pool, and while its own spot is fouled (every station, or the whole band), or the
escape and the hold take turns walking it back in.

Probes: `auriaya.station` (MT's index) and `auriaya.soak` per ranged bot (`hold`, `move`, `boxed`,
`none`). Baseline before the band (Chronicle, 25N, 16 possible soakers): Screech split 15-17 per cast
on most kills, 3-4 on the ~168 s cast in two of six; at most 12 pool ticks on one player, except the
39 above on the one bot-tanked kill (2026-10-17).

Kill order is **Sentries → Feral Defender → boss**: sentries stay dead and drop the boss's buff, where
each Defender kill costs a pool and buys 35s. Targets are picked in code — bots set no icons, but a
mark a player sets still wins. Melee take the Defender only within 15 yd of the boss, or they chase it
across the room as it re-rolls aggro. It feigns at 1 HP wearing `UNIT_FLAG_NOT_SELECTABLE`, so every
candidate is checked with `IsDownOrFeigning` after `GetUldUnitByEntry`. Savage Pounce (64666)
fires only at 8–25 yd from the sentry's own victim, so a tank holding it in melee is the whole
counter — the taunt needs no positioning code behind it.

**Every Auriaya trigger resolves her through `GetAuriaya`, by entry**: through
[`"find target"`](../../engine/pitfalls.md#things-the-bot-cannot-see) any bot fighting a sentry or the
Defender silently lost its dodges and its anti-fear. `GetAuriaya` and `GetAuriayaRoomPools`
share one scan per bot per tick (`RefreshedAuriayaRoomScan`, `thread_local`, keyed on `botAI` and
`getMSTime()`), since each node otherwise ran both grid sweeps twice on the tick it fired, trigger then
action. It is a cache, not a latch, so `thread_local` is safe. It holds guids, pools included, so a
despawn between reads drops out instead of dangling.

**Nothing pulls before a player does.** Every node that pulls waits for `IsAuriayaEngaged` (her
`IsInCombat()`): targeting, the sentry taunt, the anchor and both rules. Ungated, the targeting
action's own `Attack()` pulled the room for the first bot to see her, and the anchor walked the raid
to its spots early. A Sentry's `JustEngagedWith` puts her in combat, so the flag covers any opener. The
pool dodge, anti-fear and fall from floor run on presence. Relevance ranks urgency: fall from floor
`ACTION_RAID + 4`, sentry taunt +3 (a loose Sentry buffs her and pounces anything 8-25 yd off), pool
dodge and anti-fear +2, targeting +1, position last. The fall-from-floor teleport goes to the tank's
station or a soak spot raycast from her (the bot is under the floor), and to the master only without
one, since the master may be out in the corridor.

**No burst cooldown fired here, for a reason nothing in this room explains.** Sara (33134), the idle
Yogg-Saron phase-1 NPC, is a static spawn 114 yd away, and `Map::OnCreateMap` loads every grid of an
instance map, so she is alive and findable from the moment it is created. `UlduarBurstWindowMultiplier`'s
Yogg branch matched her on **presence** within 200 yd and returned "not phase 3", zeroing lust, potions
and trinkets for the whole fight. It keys on Yogg-Saron's boss state (`BossStateGateLive`) instead. When
burst goes missing on a boss with no burst logic of its own, suspect a neighbouring encounter first.

Even open, the framework dwell keys on the **bot's own** target, and Sentry and Defender both carry
`type_flags 108` (boss mob), so they pass `IsBossCreature` and never redirect to the main tank. Expect
lust in the Defender-down windows rather than at the pull.

**Crazy Cat Lady requires no sentry killed, so it is incompatible with the kill order.** Bots
optimise for the kill and, per the follower model, never chase achievements.


# Auriaya

No hard mode. Entries are shared; Sonic Screech is the only difficulty pair the strategy reads.

| Mechanic | Ids | Handling |
|---|---|---|
| Sonic Screech | 64422 / 64688 | **Soaked, never dodged** — see below |
| Terrifying Screech | 64386 | Fear every 35s from the pull, so the whole fight is one anti-fear window |
| Sentinel Blast | 64389 | Raid-wide, **not** a cone: no `spell_cone` row, and its SpellScript strips non-players. Healed through |
| Sanctum Sentry | 34014 | Assist tank 0 taunts each loose one; Strength of the Pack (64369) buffs the boss while they live |
| Feral Defender | 34035 | Random aggro (61906) makes it untankable — focus-killed, never tanked |
| Seeping Feral Essence | 34098 | Non-selectable stalker, one per Defender life. Its aura 64458 ticks 64459 every 1s: ~4,500 damage at **5 yd** (radius index 8, no `spelldifficulty` row, so both sizes). Dodged at 7 |
| Guardian Swarm | 64396 | Tank DoT, left to the generic dispel |
| Enrage | 47008 | 10 min, unhandled — no enrage awareness exists anywhere in the module |

**Sonic Screech is a damage split, not a dodge.** `spell_custom_attr` carries
`SPELL_ATTR0_CU_SHARE_DAMAGE` on both ids (64422 also `IGNORE_ARMOR`; 64688 does not — upstream
asymmetry), so the 120° cone (`spell_cone`) divides **60,125–69,875** (10N) or **190,000–210,000** (25N) among
everyone it hits. She faces her victim, so the old design — non-tanks sidestepping out while the main
tank arc-stepped her away from the raid centroid — left the tank eating it unsplit, a guaranteed death
at 25N. It also never converged: each tank step swept the cone across the raid, and the bots it clipped
moved, shifting the centroid the tank steered by. Bots soak it now, and there is no
cone node left.

**Anchoring is hybrid**, because she walks to her victim and cannot be pinned to world coordinates
the way XT-002 is:

- **Main tank** → a fixed spot from `ULDUAR_AURIAYA_MAINTANK_SPOTS`. A stationary tank is the entire
  facing control; there is nothing left to steer.
- **Ranged and healers** → boss + 20 yd along the boss→victim bearing, rounded to π/16 so tank drift
  cannot shuffle twenty bots. Reading her live victim rather than a fixed bearing is what keeps the
  split working when a human tanks.
- **Melee and assist tank 0** → unanchored. Melee sit behind her and do not soak; at either raid size
  the remaining soakers already make each share small.

`AuriayaMovementGuardMultiplier` zeroes generic movers for the anchored roles only, or the anchor
oscillates. It spares `AttackAction` and `ReachTargetAction` — both are `MovementAction`s, and a
blanket veto would kill targeting and strand healers out of heal range.

**The pools are permanent**: summon 64457 has `DurationIndex 21` (−1), no SmartAI touches 34098, and
the Defender's 30s respawn never despawns them, so up to 9 accumulate per pull. Hence stations: three
tank spots 10 yd apart along her home facing, which runs away from the corridor at +x. The one in use
is whichever has the fewest pools within 15 yd of either of its spots — the raid's true footprint,
arrival tolerance plus pool radius — ties to the lowest index. Only the tank computes it; everyone
else inherits the move through the bearing, so no two bots can disagree.

**Retirement must not read anything derived from her live position**, however much cheaper it looks.
She follows the main tank and the main tank follows the verdict, so it oscillates: retire station 0,
the MT walks to 1, she follows, station 0 reads clean, the MT walks back. A pool count against fixed
geometry only ever grows, so the index slides west and cannot reverse.

**The dodge is leash-absolute.** Candidates are the bot's own spot, its anchor, and 8 directions × 3 yd
out to the 12 yd leash; the winner clears the most pools, then stands furthest from what is left, then
moves least — so a bot already on the best ground stays and casts. Maximising distance is safe only
because the leash bounds it: unbounded it is what walked bots up the corridor, and the flee fallback
that did the same is gone. Boxed in, a bot takes the least bad point rather than leaving the leash.
`AuriayaRaidPositionTrigger` also stands down while a pool sits on the anchor itself, or the dodge and
the anchor take turns walking the bot back in.

Kill order is **Sentries → Feral Defender → boss**: sentries stay dead and drop the boss's buff, where
each Defender kill costs a pool and buys 35s. Targets are picked in code — bots set no icons, but a
mark a player sets still wins. Melee take the Defender only within 15 yd of the boss, or they chase it
across the room as it re-rolls aggro. It feigns at 1 HP wearing `UNIT_FLAG_NOT_SELECTABLE`, so it
resolves through `GetFirstLiveUnitByEntry`, never `GetFirstAliveUnitByEntry`. Savage Pounce (64666)
fires only at 8–25 yd from the sentry's own victim, so a tank holding it in melee is the whole
counter — the taunt needs no positioning code behind it.

**Every Auriaya trigger used to resolve the boss through `"find target"`**, which walks only the
bot's own threat list: any bot fighting a sentry or the Defender silently lost its dodges and its
anti-fear. All of them go through `GetAuriaya`, by entry, now.

**No burst cooldown fired here, for a reason nothing in this room explains.** Sara (33134), the idle
Yogg-Saron phase-1 NPC, is a static spawn 114 yd away, and `Map::OnCreateMap` loads every grid of an
instance map, so she is alive and findable from the moment it is created. `UlduarBurstWindowMultiplier`'s
Yogg branch matched her on **presence** within 200 yd and returned "not phase 3", zeroing lust, potions
and trinkets for the whole fight. It reads `IsInCombat()` now and searches SightDistance. When burst
goes missing on a boss with no burst logic of its own, suspect a neighbouring encounter first.

Even open, the framework dwell keys on the **bot's own** target, and Sentry and Defender both carry
`type_flags 108` (boss mob), so they pass `IsBossCreature` and never redirect to the main tank. Expect
lust in the Defender-down windows rather than at the pull.

**Crazy Cat Lady requires no sentry killed, so it is incompatible with the kill order.** Bots
optimise for the kill and, per the follower model, never chase achievements.


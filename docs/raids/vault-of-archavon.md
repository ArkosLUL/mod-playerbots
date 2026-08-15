# Vault of Archavon (map 624)

One shared `voa` strategy covers all four bosses. Cross-raid conventions are in
[README.md](README.md).

## Heroic is free

Creature entries and spell ids are identical across 10N/25N/10H/25H — difficulty is an instance
property. Koralon's spells are difficulty-scaled server-side, so one spell id per ability covers both
modes. Toravon's orb count differs (1 at 10-man, 3 at 25-man) but a nearest-creature scan handles any
count.

## Ruled out — do not implement

- **Archavon has no Choking Cloud in this core.** `EVENT_CHOKING_CLOUD` actually casts
  `SPELL_CRUSHING_LEAP` (58960), a random-target leap, not a persistent ground DoT. There is nothing
  to dodge.
- **The boss-name string mismatch was a false alarm** — token compression dropped the word "the" in
  an earlier read. All strings are consistently `"<boss> the <x> watcher"`.
- **The resistance-trigger naming omission is cosmetic.** `BossNatureResistanceTrigger` /
  `BossShadowResistanceTrigger` omit the `bossName +` prefix, but **the engine resolves triggers by
  their registration name**; the internal name only feeds log and perf labels.

## Archavon

Skull-marking genuinely drives kills — `DpsTargetValue::Calculate` → `RtiTargetValue` with `rti`
defaulting to `"skull"` — so Emalon's overcharged-minion marking correctly forces a burn.

## Emalon the Storm Watcher

### Spells

| Spell | Id 10 / 25 | Facts |
|---|---|---|
| Lightning Nova | 64216 / 65279 | Radius **20yd** at 10-man, **100yd** at 25-man. `spell_voa_lightning_nova` scales damage by `(70 − min(70,dist))/70`, so **25-man cannot be dodged, only faded**. ~20.8k / ~30k. Every 40s from 40s |
| Chain Lightning | 64213 (5 jumps) / 64215 (8) | Random target, jump radius **10yd** (`Spell::SearchChainTargets`, magic). Every 25s from 5s. ~4.6k / ~7.4k |
| Overcharge | 64218 | One random minion (`SPELLVALUE_MAX_TARGETS, 1`), every 40s from 47s. **`SpellHitTarget` sets the minion to full health, so pre-damage is wasted** |
| Overcharged | 64217 | +10% haste and +20% damage **per stack**, applied every 2000ms. Ten stacks is a **20-second** burn window, and the off-tank is eating +200% by the end |
| Overcharged Blast | 64219 | Radius **50000** — raid-wide and unavoidable. Positioning cannot mitigate it; only the kill can |
| Minion Shock | 64363 | Single-target ~5k on its own victim, **no AoE**; call-for-help 80yd. Parking the adds far from the raid is therefore safe |
| Berserk | 26662 | 6 min |

`spelldifficulty_dbc` maps 64213→64215 and 64216→65279 and **nothing else**.

### The room

Swept off the map-624 navmesh (tile `6243232.mmtile`, 658 `NAV_GROUND` polys). Floor z ≈ 91.5–92.8;
widest waist 80.5yd at Y −292…−286. **Above Y −318 the floor splits into two alcoves with a wall
between them** — nothing goes there. Below Y −256 is the entrance ramp at z ≈ 96.8.

Fixed spawns: Emalon `(-218.726, -288.570, 91.549)` o `1.588`, and four Tempest Minions at
`(-203.98, -281.29)`, `(-233.49, -281.14)`, `(-233.27, -297.10)`, `(-203.84, -297.10)` — the corners
of a 29.5 × 16 rectangle whose centre **is** Emalon's spawn. Respawns roll one of those four corners.

Arc sweep: at R 26 the full ±90° is floor; at R 30 it is ±80°.

### The layout

Main tank anchor `(-218.75, -310.3)`, which settles the boss at Y ≈ −300. Healers ring R 23 at ±60°,
ranged ring R 27 at ±75°, both on a constant +Y bearing. Off-tank camp `(-187, -288)`, 33.9yd from the
boss.

**The two ring radii are pinned between hard walls**: greater than 20yd (nova immunity at 10-man) and
at most 28.2yd (heal range to a tank standing 10.3yd beyond the boss). That leaves a 22–28yd band for
both rings, which is why healers sit **inside** ranged here rather than outside as in Anub'rekhan.

**Emalon's `CombatReach` is 7.5**, so `GetMeleeRange` against a player is ≈ **10.3yd** — the drag
distance is `anchor − 10.3`.

Tempest Minions have `HealthModifier` 10.5 / 38.5 against a level-82 base of ~4964, so ~52k HP at
10-man and ~191k at 25-man. Ranged alone burn the overcharged minion well inside the 20-second
window, so **melee never leave Emalon** — which is what lets the add camp sit far away.

`MINION_TAUNT_RANGE` is 28 because the two far spawn corners are 47yd out, which no taunt reaches.
The `ROOM_*` bounds exclude Toravon's room at X −43 and the corridor Tempest Warders at Y −229 and
−196.

### Decisions and traps

**`boss_emalon` never calls `SetInCombatWithZone()` for himself** — only `summons.DoZoneInCombat()`
for the minions. So bots that never attack him (the off-tank, the whole ranged ring) are not on his
threat list and `"find target"` returns null for them. Every trigger here resolves him by entry
instead; the general rule is in [README.md](README.md).

**The nova trigger is deliberately not distance-gated.** It drives the movement suppression as well as
the run-out, so gating it on standing inside 22yd made the suppression drop the instant a bot got
clear — and `ReachMeleeAction` then walked it straight back in, mid-cast. The action returns false
once clear instead. The cast is 5s, ample for a 22yd step.

**Chain Lightning is not a design driver — do not re-audit.** The band above leaves ~8yd between
neighbours at 25-man, under the 10yd jump radius. Widening the rings would break heal range or nova
immunity, and the spell is only ~5–7k. Accept the chaining.

**No main-tank drag latch**, unlike Obsidian Sanctum: `MoveToClamped` already drops the move once the
tank is inside its 2yd tolerance. OS needs its latch only because Sartharion's 18yd reach means he may
never follow at all; Emalon's is 7.5.

**No melee positioning action.** Melee stay on Emalon and nothing else wants them — adding an owner
for them would only create something else to fight.

**The off-tank pickup prefers a loose minion over the overcharged one.** A respawn arrives with an
empty threat list and takes whoever is nearest, so it is on a healer within seconds; the overcharged
minion is already held, and its stacks only hurt the tank holding it.

Emalon's Lightning Nova run-out is covered by `EmalonLightningNovaMultiplier`, matching Koralon and
Toravon. Skull-marking goes through `MarkTargetWithSkull` (`RaidBossHelpers.h:8`), which null-checks
the group and only re-sets when the icon differs.

## Koralon (entry 35013)

| Spell | Id | Handling |
|---|---|---|
| Burning Fury | 68168 | Passive self-buff at pull — no bot action |
| Burning Breath | 66665 | **Real cast** (`CastSpell(..., false)`), frontal cone, boss slow-rotates during it. Detect via `UNIT_STATE_CASTING` + `FindCurrentSpellBySpellId`. Non-tanks step ~10 yd behind the boss |
| Flaming Cinder | 66681 → missile 66682 | Random player's location, splash, `MaxAffectedTargets=1`. Ranged spread |
| Meteor Fists | 66725 | Melee proc landing only on the boss's melee victim — **out of scope by decision** |

**The generic `avoid aoe` already handles lingering ground fire** (CombatStrategy → `AreaDebuffValue`),
so no custom "stand out of fire" action was needed.

`IsBotInFrontalCone(bot, source, coneAngle, range)` was promoted out of
`TrialOfTheCrusaderHelpers` into the shared `RaidBossHelpers` for this.

## Toravon (entry 38433)

| Mechanic | Id | Handling |
|---|---|---|
| Whiteout — raid-wide frost AoE, ~40s | 72034 | Unavoidable → frost resistance aura |
| Frozen Mallet — tank frost melee debuff | 71993 | Automatic, no action |
| Freezing Ground — void zone under a random player, ~20s | 72090 | Move out |
| Frozen Orb — pulses frost damage, switches target ~10s | NPC 38456, damage 72081 | Flee the orbs |

The Freezing Ground dodge is the reusable **area-debuff flee idiom**: read
`AI_VALUE(Aura*, "area debuff")`, confirm the spell id, and flee `GetDynobjOwner()->GetPosition()` —
mirroring the engine's own `AvoidAuraWithDynamicObj`.

`ToravonAvoidMultiplier` zeroes `CastReachTargetSpellAction`, `ReachTargetAction`,
`CombatFormationMoveAction` and `FollowAction` while either avoid trigger is active.

## Known tuning issue

The Archavon rock-shards and Koralon flaming-cinder spread triggers fire whenever a ranged bot is
within `8.0f` of **any** groupmate, for the whole fight — so ranged reposition continuously
(throttled to 1/s), bleeding DPS uptime against Archavon's 5-minute and Emalon's 6-minute berserk.
`FleePosition` also only flees the single *nearest* player, so a 3-stack can oscillate. The fix shape
is establish-and-hold: tighten the radius so bots settle, raise the action `minInterval`, and
consider fleeing the group centroid rather than the nearest player.

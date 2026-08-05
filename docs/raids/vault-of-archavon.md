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

## Archavon and Emalon

Skull-marking genuinely drives kills — `DpsTargetValue::Calculate` → `RtiTargetValue` with `rti`
defaulting to `"skull"` — so Emalon's overcharged-minion marking correctly forces a burn.

Emalon's Lightning Nova (64216 / 65279) is a PBAoE that non-tanks must clear, but the run-out action
had **no movement-suppression multiplier**, unlike Koralon and Toravon — so `ReachTargetAction`,
`FollowAction` and `CombatFormationMoveAction` could fight the run-out and a bot ate avoidable damage.

`EmalonOverchargeAction` reimplemented ~45 lines of skull-marking with bespoke tank-authority logic,
a raw `group->SetTargetIcon` and an **unchecked `group` pointer**. `MarkTargetWithSkull`
(`RaidBossHelpers.h:8`) already null-checks the group and only re-sets when the icon differs.

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

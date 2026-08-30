# XT-002: hard-mode detection, healer combat engine, Searing Light spot

## Context

Wipe analysis of the 2026-08-30 XT-002 pull, trace
`env/dist/logs/botobs/603_1_xt-002-deconstructor_1788114450.ndjson` (25-man, 4 healers, wipe at
7:27, 30 deaths). Read with `python modules/mod-playerbots/tools/botobs/postmortem.py`. Every number
below is measured from that file, from `navprobe` on map 603, from the DBC reference CSVs under
`modules/mod-spell-tweaks/data/dbc-reference/`, or from source.

This pull ran commit `6afda29a8` (the previous round's fix). That work landed and is visible —
14 `xt002.slot` notes show the ranged/healer formation dealing distinct slots, and the fight lasted
7:27 against 5:10. But the encounter's whole hard-mode branch never executed, for a reason no amount
of movement work could have fixed.

The user reported three things; two more came up during analysis. All five are explained below.

### What killed the raid

| source | damage to raid |
|---|---|
| Tympanic Tantrum | 1,869,186 |
| **Searing Light 65120** | **1,802,416** |
| Static Charged (Life Sparks) | 841,725 |
| Gravity Bomb (64234 + 64233) | 417,746 |
| Consumption (Void Zones) | 92,429 |

Searing Light went **up**, 1.80M against 1.53M last pull. Tympanic Tantrum is unavoidable and
room-wide. The wipe cascade is 7 deaths in 6 s at t=303–309, six of them killed by Life Sparks.

## Root causes

### A. Heartbreak is detected with the 10-man spell id, so hard mode is invisible in 25-man

`SPELL_XT002_HEARTBREAK = 65737` ([UldBossHelper.h:365](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.h#L365))
is the **10-man** id. `IsXT002HeartbreakActive`
([UldHardMode.cpp:112](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldHardMode.cpp#L112)) checks only
that one, so on 25-man it is false forever.

Proof chain, all verified:

1. The core spawns a Void Zone only when `xt002->HasAura(aurEff->GetAmount())`, reading the id from
   the Gravity Bomb aura's own effect
   ([boss_xt002.cpp:770](src/server/scripts/Northrend/Ulduar/Ulduar/boss_xt002.cpp#L770)).
2. `GetAmount()` is `EffectBasePoints_1 + 1` when `EffectDieSides_1 == 1`, which holds for all four
   spells. 10-man Gravity Bomb 63024 → 65736+1 = **65737**, exactly the core's `SPELL_HEARTBREAK`
   constant, which validates the convention. 25-man 64234 → 64192+1 = **64193**.
3. Spell **64193 is "Heartbreak"**, same effect layout as 65737 (77/6/6, auras 0/133/79,
   DurationIndex 21 = permanent).
4. 10 Void Zones and 11 Life Sparks spawned in this pull, so XT demonstrably carried 64193.
5. Independent confirmation from the trace: `XT002BurstWindowMultiplier::EvaluateWindow`
   ([UldMultipliers.cpp:158](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.cpp#L158)) returns
   1.0f the instant Heartbreak is up, yet **1,842 burst-window vetoes** fire from t=0.9 to t=443.3.

Everything behind that flag was dead code all fight:

- the entire parking-lot branch, so **every** Gravity Bomb carrier fell through to `MoveClearOf`;
- the Searing Light spot's Void Zone filter;
- the formation slots' Void Zone filter;
- the burst window, so no bot used Heroism, potions, trinkets or tinkers after the Heart died. That
  is most of why a 25-man took 7:27 and still wiped.

### B. Healers hold no target, so their combat engine never runs

Measured share of alive snapshots with **no target**:

| | Razorscale (same map, same session) | XT-002 previous pull | XT-002 this pull |
|---|---|---|---|
| all four healers | 32.5–37.7% | **100%** | **100%** |

`XT002SetDpsPriorityAction::Execute` explicitly clears a healer's target, and
`XT002TargetGuardMultiplier` zeroes `dps assist` for every non-tank
([UldMultipliers.cpp:218](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.cpp#L218)), so
nothing gives one back. A bot with no target never attacks, never enters combat, and runs the
**non-combat** engine.

`PriestNonCombatStrategy` carries renew / penance / greater heal only.
`HealPriestStrategy` is where Power Word: Shield, Prayer of Mending, Pain Suppression, Shadowfiend
and Hymn of Hope live; `HealPaladinStrategy` is where Beacon of Light lives. Evaluations of
`power word: shield*` / `beacon of light*`:

| Razorscale | Ignis | XT-002 prev | XT-002 this |
|---|---|---|---|
| 435 / 6 | 184 / 4 | 2 / 0 | **0 / 0** |

Diffing Prayer's evaluated action set between Razorscale and XT-002, everything lost is
combat-engine content: `shoot` (117), `avoid aoe` (103), `prayer of mending on party` (90),
`power word: shield on party` (345), `renew on main tank`, `flash heal on party`, `shadowfiend`,
`hymn of hope`, `pain suppression`, `fade`. This answers both the PW:S and the Beacon question, and
it predates the previous commit.

### C. The Searing Light spot and its alternates sit inside the raid

- The spot `(862.737, 12.779)` is **7.88 yd** from formation slot 9 `(864, 5)`. Searing Light is
  8 yd, so the spot itself is inside the raid.
- Carriers never reach it anyway. `MoveToSearingLightSpot` sorts spot-plus-ring **nearest to the
  bot**, and from the raid the southern ring points are far nearer than the spot. Measured carrier
  destinations: **80** to `(862.7, 0.8)` and **52** to `(871.2, 4.3)`, which are the 270° and 315°
  ring points. Those are **0.85 yd** from slot 3 `(863, 0)` and **1.5 yd** from slot 8 `(872, 3)`.
- At every Searing Light expiry on a ring point, 4–6 living raid members were inside 8 yd. Tree and
  Nightwarrior both died standing on `(862.74, 0.78)`.

The previous round moved the formation 6 yd north to clear the Gravity Bomb pull. That is what
pushed it into the Searing Light ring. The two fixed layouts are unaware of each other.

### D. The avoid-hazard trigger and action measure the hazard differently, which freezes bots

`XT002AvoidHazardTrigger` uses `TooCloseToCreature` → `bot->FindNearestCreature(entry, 6.0)`, whose
range test subtracts both objects' sizes. `MoveClearOf`'s score uses `GetExactDist2d` centre to
centre and caps at zero (`std::min(worst, 0.0f)`), so once the bot is at the raw 6 yd the standing
score is 0 and **no candidate can beat it** — the action returns false forever while the trigger
stays true.

`XT002RaidPositionTrigger` stands down whenever that trigger is active, so the anchor cannot recover
the bot either. Smartface dropped its own puddle at `(839.8, 13.8)`, stepped to `(839.79, 19.83)` —
exactly 6.0 yd away — and **never moved again**, sitting at **64.7 yd from XT** from t=184 until it
died at t=309. That is observation 1 exactly. `xt002 avoid hazard action` returned FAILED 12 times
in that window.

This also explains why `xt002 raid position action` was evaluated only ~80 times across 25 bots in
447 s.

### E. `MoveClearOf` is LOS-gated, so it usually has exactly one candidate

`MoveClearOf` keeps `bot->IsWithinLOS(...)` on every candidate — the same gate already deleted from
`ParkVoidZone` because the Ulduar WMO ends at y ≈ −29 and LOS is unreliable here. Of 823 movement
ticks, **820 logged exactly one destination**, so the retry loop nearly always has nothing to retry.

Malediction took Gravity Bomb at t=281.7 standing in the raid, issued `MoveTo(896.21, 14.70)` and
got `nopath` **8 times in a row**, never moved a yard, and dropped the puddle with **15 raid members
inside 20 yd**. That is observation 3.

Observation 2 is the same defect one level up: with the lot branch dead (cause A), consecutive melee
carriers all fell to `MoveClearOf`, whose ring is deterministic and bot-relative, so Angry, Totemist,
Shadow and Assasin were each sent to the **identical** point `(884.10, -51.38)` — the 270° candidate
at the ring's 30 yd maximum. Four deaths there, all stacked on the previous carrier's puddle.
Justice survived because it was bombed from a different position, so its ring answer differed and
landed 1.2 yd from a real lot cell.

## Established facts

Verified this session — do not re-derive.

| Fact | Source |
|---|---|
| 25-man Heartbreak = **64193**, 10-man = 65737, both permanent (DurationIndex 21) | spell.reference.csv, spelldifficulty has no row for either |
| `bot->GetSpeed(MOVE_RUN)` in play here ≈ 6.65 y/s measured (Totemist, 29.93 yd in 4.5 s) | trace snapshots |
| `MoveClearOf` ring: 8 headings, 3 yd increments, max radius = maxClearance + 5 = **30 yd** | [UldActions_XT002.cpp](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_XT002.cpp) |
| Melee lot origin (871.52, −42.04), ranged (837.07, −41.01), 5×4 cells, 6 yd step | UldBossHelper.cpp:105-106 |
| The 14 formation slots this pull: (866,−6) (872,−7) (869,0) (863,0) (860,−6) (863,−13) (869,−13) (875,−7) (872,3) (864,5) (858,−1) (858,−12) (864,−18) (872,−16) | trace `xt002.slot` notes |
| Life Sparks live 6–13 s and do die; Static Charged is a 500 yd aura, so distance from the spark buys nothing | trace; existing code comment |
| The region **north** of the raid is `PATHFIND_SHORTCUT\|PATHFIND_NOPATH` from every melee position tested; the **south-west** is `PATHFIND_NORMAL` from all six positions tested | navprobe |
| `IsWithinLOS` is unreliable across the y ≈ −29 WMO edge; paths across it are fine | prior session, re-confirmed |

## Decisions

Settled with the user this session.

1. **Heartbreak via a difficulty helper**, matching `GetXT002GravityBombSpellId` /
   `GetXT002SearingLightSpellId` in the same file.
2. **Healers keep a target**, so they stay on the combat engine and regain the full heal kit.
3. **Relocate the Searing Light spot and its ring**, with every point navprobe-verified.

## New coordinates (navprobe-verified)

Chosen by constraint search: ≥14 yd from every formation slot, ≥12 yd from every lot cell, ≥25 yd
from XT, and ≥6 of 8 ring headings clearing formation by 12 yd and lot cells by 8 yd. Ranked by
travel from the melee stack.

**`ULDUAR_XT002_SEARING_LIGHT_SPOT` = (846.0, −22.0, 409.597)**

- 38.0 yd from the melee stack `(884, −21)`, **shorter** than the current spot's 39.9
- 15.6 yd from the nearest formation slot, 19.2 yd from the nearest lot cell, 47.9 yd from XT
- on mesh, 0.247 yd to poly; `PATHFIND_NORMAL` from all six carrier positions tested
  `(884,−21) (881,−4.5) (890,−22) (866,−6.5) (860,−6.5) (875,−7)`

**`ULDUAR_XT002_SEARING_LIGHT_DETOUR` = 10.0** (was 12.0). The six headings that clear both layouts
are 90°, 135°, 180°, 225°, 270°, 315°:

| point | to poly | settles | from melee | from anchor |
|---|---|---|---|---|
| (846.00, −12.00) | 0.247 | 409.803 | NORMAL | NORMAL |
| (838.93, −14.93) | 0.247 | 409.803 | NORMAL | NORMAL |
| (836.00, −22.00) | 0.248 | 409.803 | NORMAL | NORMAL |
| (838.93, −29.07) | 0.248 | 409.803 | NORMAL | NORMAL |
| (846.00, −32.00) | 0.248 | 409.803 | NORMAL | NORMAL |
| (853.07, −29.07) | 0.248 | 409.803 | NORMAL | NORMAL |

The 0° and 45° headings fail the formation clearance and must be dropped by the runtime filter
below, not hardcoded away.

## Files to change

### `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp` — cause A

- Add `SPELL_XT002_HEARTBREAK_25 = 64193` beside the existing 65737, renaming the latter `_10` to
  match `SPELL_XT002_GRAVITY_BOMB_10/_25`.
- Add `uint32 GetXT002HeartbreakSpellId(Player* bot)`, copying `GetXT002GravityBombSpellId` exactly.
- Move `ULDUAR_XT002_SEARING_LIGHT_SPOT` to the coordinates above and set
  `ULDUAR_XT002_SEARING_LIGHT_DETOUR = 10.0f`.
- Add `ULDUAR_XT002_SEARING_LIGHT_SLOT_CLEARANCE = 12.0f` — the margin a Searing Light destination
  must keep from a formation slot.
- Export the slot geometry so the carrier can test candidates against it. `XT002RingSlotPoint` is a
  file-local static today; the cheapest route is a helper
  `bool XT002PointClearOfFormation(Player* bot, float x, float y, float clearance)` that rebuilds the
  roster the same way `GetXT002RangedSlot` does and checks every slot.

### `src/Ai/Raid/Uld/Util/UldHardMode.cpp` — cause A

`IsXT002HeartbreakActive` uses `GetXT002HeartbreakSpellId(botAI->GetBot())`. This single line is what
brings the parking lot, both Void Zone filters and the burst window back to life.

### `src/Ai/Raid/Uld/Action/UldActions_XT002.cpp` — causes B, C, E

- **`XT002SetDpsPriorityAction::Execute`**: delete the healer branch that clears the target and let
  healers fall through to `ResolveTarget` like everyone else. Keep the comment's real point as a note
  that healers are pinned by the mover sweep, which is what stops a target turning into a walk.
- **`MoveToSearingLightSpot`**: rank candidates by **clearance from the raid**, descending, instead
  of by distance from the bot — this is what currently drags carriers onto the formation. Reject any
  candidate failing `XT002PointClearOfFormation(..., ULDUAR_XT002_SEARING_LIGHT_SLOT_CLEARANCE)`,
  and keep the existing Void Zone filter (live again once A lands) and the `IssueMove` retry loop.
- **`MoveClearOf`**: delete the `bot->IsWithinLOS(...)` term, exactly as was done for `ParkVoidZone`,
  so the ranked list has more than one entry and the retry loop can work.
- **`XT002AvoidHazardAction::Execute`**: build the Void Zone entries with
  `ULDUAR_XT002_VOID_ZONE_RADIUS + ULDUAR_XT002_BOMB_CELL_ARRIVED` (6 + 2) rather than the bare
  radius, so a bot the trigger calls "too close" always has an improving candidate and the score can
  no longer sit pinned at zero. Boombot clearance is unchanged.

### `src/Ai/Raid/Uld/UldMultipliers.cpp` — cause B

Exempt healers from the `dps assist` stand-down:
`if (!botAI->IsTank(bot) && !botAI->IsHeal(bot) && dynamic_cast<DpsAssistAction*>(action))`.
The `MovementAction` sweep already pins healers to their slot and already zeroes `reach spell`, so a
healer with a target still will not walk at an add.

### `src/Ai/Raid/Uld/Trigger/UldTriggers_XT002.cpp` — cause D

`XT002RaidPositionTrigger` currently stands down for the whole of `XT002AvoidHazardTrigger`. With the
margin fix the avoid action should always succeed when its trigger fires, but keep a floor: the
anchor should not defer while the bot is outside `ULDUAR_XT002_VOID_ZONE_RADIUS` of any puddle by the
action's own 2D measure, so a measurement disagreement can never strand a bot again.

### Docs

Update the XT-002 section of `docs/raids/ulduar.md`: the 25-man Heartbreak id and what it gated, the
healer target/combat-engine coupling, the relocated Searing Light spot, and the rule that a fixed
destination here is checked against the formation as well as against puddles. Run
`/compact-docs-writer` first, per the governing-doc rule.

### Not touched

`docs/engine/pitfalls.md` still tells the reader to read navprobe's `settledZ` column; this build
prints both `settledZ` and `UpdateAllowedPositionZ`. Still the user's call.

## Verification

The module cannot be compiled in this environment; build and in-game checks are the user's.

**Static:** `GetXT002HeartbreakSpellId` used everywhere 65737 was; no bare `SPELL_XT002_HEARTBREAK`
left; no `IsWithinLOS` left in `MoveClearOf`; every `IssueMove` loop still bounded by
`ULDUAR_XT002_MOVE_CANDIDATE_ATTEMPTS`; XT-002 node relevances still unique (91, 90, 64, 63, 61, 60);
brace balance on every touched file.

**In game, hard mode on**, then re-run `postmortem.py` and compare against this pull:

1. Burst-window vetoes stop shortly after the Heart dies. This pull: 1,842, last at t=443.3. This is
   the single clearest signal that cause A is fixed.
2. `xt002 debuff carrier action` destinations land on lot cells. This pull: ~10 of 249, and those
   were `StopShortOf`/ring points, not cells.
3. No two carriers drop puddles within 6 yd of each other. This pull: four at `(884.10, −51.38)`.
4. Searing Light damage to the raid drops from 1,802,416, and no Searing Light expiry has a living
   raid member inside 8 yd. This pull: 4–6 every time on a ring point.
5. `power word: shield*` and `beacon of light*` evaluations rise from 0. Razorscale on the same
   roster gives 435 and 6.
6. Healer targetless share drops from 100%.
7. No bot holds a position more than 45 yd from XT for more than ~15 s. This pull: Smartface at
   64.7 yd for 125 s until it died.
8. `xt002 raid position action` evaluations rise from ~80 across the raid in 447 s.

**In game, hard mode off:** no Void Zones or Life Sparks exist, so carriers still use the dynamic
spread and the relocated Searing Light spot; add waves, the Pummeller taunt, the Heart floor and the
leash/reach targeting gates are unchanged.

## On approval

Copy this file to `docs/plans/xt002-hardmode-detection/xt002-hardmode-detection.PLAN.md` before
starting implementation, per the plans-directory rule.

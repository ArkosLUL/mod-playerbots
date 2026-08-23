# Hodir: tank target ownership, movement economy, paladin aura

## Context

Trace `env/dist/logs/botobs/603_3_hodir_1787582677.ndjson` (2026-08-24, 6:55, wipe, boss never below
~60%). Read it with `python modules/mod-playerbots/tools/botobs/postmortem.py <file>`.

The previous round shipped as `f31082543` and this trace contains it. Three problems remain, all
raised from watching the pull:

1. Tanks attack Flash Freeze victims, which drags Hodir into the ranged formation. Tanks should not
   be part of that task at all.
2. Ranged spend most of the fight walking. Hodir hard mode is an enrage race, so DPS uptime is the
   thing to protect.
3. The paladin frost resistance aura is unreliable and spec-blind.

### 1. Nothing owns a tank's target

`HodirSetDpsPriorityTrigger::IsActive` (`UldTriggers_Hodir.cpp:208`) returns
`!botAI->IsTank(bot) && !botAI->IsHeal(bot)`, and `HodirGuardMultiplier` (`UldMultipliers.cpp:927`)
only zeroes `DpsAssistAction` for non-tanks. So the generic `TankAssistAction` picks a tank's target
with nothing to override it.

| tank | alive samples | on Hodir | Flash Freeze block (32938) | something else |
|---|---|---|---|---|
| Ecoterrorist | 1202 | 50.9% | 6.7% | 41.3% |
| Bulwark | 838 | 53.6% | 9.7% | 35.2% |

Mean distance from the tank spot tracks the target: 4.8 yd while on Hodir, 9.2 on a block, 13.1 on
"something else". Hodir follows - median 11.8 yd from `ULDUAR_HODIR_MAINTANK_SPOT`, p90 52.3, max
76.0. At 311 s he stood 9.2 yd from the raid anchor with 9 of 13 ranged inside 15 yd. Over the whole
fight 24.4% of ranged samples were within 15 yd of him.

### 2. Ranged move 57% of the time, cast 27%

Attribution by the action that issued each accepted move, over alive samples:

| issuer | % of all ranged time | % of heal time |
|---|---|---|
| `hodir raid position action` | 16.7 | 18.8 |
| `hodir icicle dodge action` | 16.0 | 16.1 |
| `hodir biting cold shed` | 9.8 | 11.3 |
| `hodir move snowpacked icicle` | 8.0 | 8.0 |
| `hodir spread storm cloud` | 3.7 | 2.0 |

**Starlight is unreachable and has never worked.** An earlier session claimed 65-87% uptime; that was
wrong. Measured the same way on both traces, aura 62807 totals 111 bot-seconds across 20 bots in
`603_3_ulduar_1787492421.ndjson` and ~264 across 24 bots here - under 1% per bot either way.

The cause is the radius. 62807's DBC row gives 8 (`spellradius.reference.csv`, radius index 14) but
the effective reach measures ~4: bots holding the aura sit at a median 2.0 yd from the zone centre,
p90 3.3; bots without it are at p10 7.6. `ULDUAR_HODIR_STARLIGHT_RADIUS = 8.0f`
(`UldBossHelper.h:747`) is therefore wrong, and so is the `static_assert` at `:786` and the
"ring rides Starlight" premise the whole layout rests on. Slots are 4.5 yd apart for icicle safety,
so a 4 yd zone holds one bot.

That makes `GetHodirDruidHelper(botAI) && !bot->HasAura(SPELL_HODIR_STARLIGHT)`
(`UldTriggers_Hodir.cpp:180`) permanently true for nearly every bot - the top mover in the table is
the raid chasing a buff it cannot have. The druid also drops the zone wherever it stands, which is
the tank corner: zones landed 17-29 yd from the raid anchor, 60 s lifetime each.

**Toasty Fire is the opposite.** 62821 measures true to its DBC 11 (with-aura p90 11.9), zones are
stationary, live 29-48 s, and one sat 6.8 yd from the raid anchor. It already stands the Biting Cold
trigger down (`UldTriggers_Hodir.cpp:60`), so a formation parked in one loses the shuttle as well.
Creature `33342 Toasty Fire` is confirmed in `creature_template` (faction 1665, unit_flags
0x02000000 NOT_SELECTABLE), so `FindNearestCreature` reaches it.

Adoption rate measured against this trace, per snapshot with Hodir alive: a fire within 25 yd of the
raid anchor and 15 yd clear of Hodir exists in 49.6% of samples - about 81% of the samples where any
fire is up. The fixed anchor covers the rest.

**The dodge triggers on the clear distance, not the blast.** `HodirIcicleDodgeTrigger::IsActive`
(`UldTriggers_Hodir.cpp:94`, `:99`) tests `ULDUAR_HODIR_ICE_SHARDS_CLEAR` (6) and
`ULDUAR_HODIR_BIG_SHARDS_CLEAR` (9), but the pools kill at 4 and 7 (62457 radius 4, 65370 radius 7,
both confirmed in `spellradius.reference.csv`). The 2 yd margin belongs on the destination, which is
where the bot ends up, not on the decision to leave. The trace cannot size this: RaidObs sweeps only
23 distinct Icicle guids across a fight with roughly 200 spawns, so any proximity figure from it is a
floor. The change stands on the geometry instead.

### 3. Paladin frost resistance is spec-blind and leaks

Justice (retribution) cast 48945 at 1.9 s and raid coverage averaged 84% of the fight, so the aura
does go up. Two defects behind that:

- `BossFrostResistanceTrigger::IsActive` (`BossAuraTriggers.cpp:92`) picks the **first alive paladin
  in group order**, with no spec test. This raid has three (retribution, holy, protection), so the
  same code can just as easily strip Devotion off the prot tank or Concentration off the holy healer.
  Bulwark (prot) also cast 48945 at 11.0 s, which that logic alone does not explain.
- `BossFrostResistanceAction::Execute` (`BossAuraActions.cpp:37`) does
  `ChangeStrategy("+rfrost")` and nothing ever removes it - there is no `"-rfrost"` anywhere in the
  tree. The pick outlives the encounter and is wrong on every later pull. This is the same defect the
  anti-fear section of `docs/raids/README.md` documents for `ChangeStrategy("+tremor")`.

## Approach

### 1. Tanks hold Hodir and nothing else

Three edits, no new files.

- `HodirSetDpsPriorityTrigger::IsActive` (`UldTriggers_Hodir.cpp:208`) - drop the tank exclusion,
  leaving `return !botAI->IsHeal(bot);`.
- `HodirSetDpsPriorityAction::ResolveTarget` (`UldActions_Hodir.cpp:147`) - return `hodir`
  immediately for a tank, before the `nearest npcs` scan. Tanks skip both the trapped-raider branch
  and the helper-block branch. Comment must say why: the boss follows whoever holds him, so a tank
  walking to a block puts him on the ranged stack, and freeing ice is worth less than that costs.
- `HodirGuardMultiplier::GetValue` (`UldMultipliers.cpp:927`) - extend the targeting stand-down to
  tanks by also zeroing `dynamic_cast<TankAssistAction*>(action)`. Keep healers on the generic picker
  as today.

Safe to suppress unconditionally because the ladder ends in a terminal fallback: `ResolveTarget`
always returns Hodir for a tank, so no tank is ever left nodeless. That is the exact condition
`docs/raids/README.md` sets for targeting suppression.

Leave `reach melee` alone - `ReachTargetAction` stays at 1.0 in the multiplier, and with the target
pinned to Hodir it now walks tanks to the right place.

### 2. Ring on Toasty Fire, Starlight per-bot, dodge on the blast radius

**Constants** (`UldBossHelper.h`):

```cpp
constexpr float ULDUAR_HODIR_STARLIGHT_RADIUS = 4.0f;   // was 8.0f
constexpr float ULDUAR_HODIR_RAID_RING_OUTER = 9.0f;    // was 11.0f
constexpr float ULDUAR_HODIR_FIRE_ADOPT_RADIUS = 25.0f; // new
constexpr float ULDUAR_HODIR_DODGE_TRIGGER_MARGIN = 0.5f; // new
constexpr float ULDUAR_HODIR_CENTRE_MIN_BOSS_GAP = 15.0f; // was 18.0f
```

Record the measurement next to `ULDUAR_HODIR_STARLIGHT_RADIUS`: the DBC row says 8, bots holding the
aura sit at a median 2.0 yd and p90 3.3 from the zone, so 4 is what it behaves like.

Delete the `static_assert` at `:786` - it cannot hold once the radius is 4 - and replace it with one
tying the ring to the fire instead:

```cpp
static_assert(ULDUAR_HODIR_RAID_RING_OUTER + ULDUAR_HODIR_RING_SPOT_TOLERANCE <=
                  ULDUAR_HODIR_TOASTY_FIRE_RADIUS,
              "the outer ring plus its arrival tolerance has to stay inside a Toasty Fire");
```

That holds exactly (9 + 2 = 11), so a bot at the far edge of its tolerance sits on the fire boundary.
The icicle assert at `:788` still holds (9 - 4.5 = 4.5 > 4). Rewrite the comment block at `:770-774`,
which currently explains the layout in terms of a Starlight radius that does not exist.

**New helper** in `UldBossHelper.{h,cpp}`, next to `GetHodirSharedShelter`:

```cpp
Creature* GetHodirRaidFire(PlayerbotAI* botAI, Player* bot);
```

Sweeps `NPC_TOASTY_FIRE` within `ULDUAR_HODIR_ROOM_SEARCH_RADIUS` and returns the one nearest
`ULDUAR_HODIR_RAID_ANCHOR` that is within `ULDUAR_HODIR_FIRE_ADOPT_RADIUS` of it and at least
`ULDUAR_HODIR_CENTRE_MIN_BOSS_GAP` from live Hodir. Nearest the anchor rather than nearest the bot,
for the same reason `GetHodirSharedShelter` does it: two derivations of "which fire" would disagree
and oscillate. Emit a `hodir.fire` RaidObs note the way the shelter lookup emits `hodir.shelter`.

**`GetHodirRingCentre`** (`UldBossHelper.cpp:660`) - replace the druid branch with the fire. Return
the fire's position unquantised (it is a stationary object, so the quantum only pushes the centre off
it), else `ULDUAR_HODIR_RAID_ANCHOR`. `ULDUAR_HODIR_CENTRE_QUANTUM` and
`ULDUAR_HODIR_ZONE_ADOPT_RADIUS` fall out of use along with the assert at `:794`; remove all three.

**`GetHodirAnchor`** (`UldBossHelper.cpp:915`) - after the ring slot resolves, allow one opportunistic
Starlight step. A zone within `ULDUAR_HODIR_STARLIGHT_RADIUS + ULDUAR_HODIR_RING_SPOT_TOLERANCE` of
the bot's own slot replaces the slot as the anchor. Find zones with the existing
`GetDynamicObjectPositions(bot, ULDUAR_HODIR_ROOM_SEARCH_RADIUS, SPELL_HODIR_STARLIGHT)`
(`RaidBossHelpers.h:40`) - do not write a second dynobj sweep.

**No cap on how many bots share a zone.** Stacking is what a real raid does here and the arithmetic
backs it: one Ice Shards hit is a median 41% of a bot's max health (p90 54%, max 63%), so an icicle
catching two bots in one zone costs two heals rather than two lives, and +50% to every cast and swing
is worth that in an enrage race. Each bot gets a bearing of its own instead - the stand point sits
`ULDUAR_HODIR_STARLIGHT_STAND_RADIUS` from the zone centre on the bearing of the slot it came from,
so claimants spread around the zone rather than piling on one point, and no cross-bot state is needed.

Two gates on the resulting spot, both load-bearing:

- **Still inside the fire.** A bot outside it starts shedding Biting Cold, and that node sits at
  `ACTION_RAID + 1` above the anchor, so it would shuttle straight back out of the zone it just walked
  to. Tested against the ring centre, not the slot.
- **Still out of Hodir's reach** (`ULDUAR_HODIR_RANGED_MIN_BOSS_GAP`). The druid casts where it is
  standing, which is next to Hodir - measured zones landed 17-29 yd from the raid anchor, in the tank
  corner - so a good share of the zones on the floor are ones no caster can use.

The nearest passing zone wins, so the detour stays short.

**Do not gate the step on the bot already lacking the aura.** That hands the bot its ring slot back
the moment Starlight lands, walks it out of the zone, and starts the trip again. The anchor holds the
zone until the zone expires. The step also carries its own arrival tolerance
(`ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE`, 1.0 against the ring's 2.0), because 2 yd of slack at the
ring's tolerance would put a bot outside what Starlight actually reaches.

**`HodirRaidPositionTrigger::IsActive`** (`UldTriggers_Hodir.cpp:178-181`) - delete the Starlight
constraint outright. The anchor now carries the step, so the existing
`bot->GetExactDist2d(&anchor) <= tolerance` gate at `:156` handles it and nothing new fires.

**`HodirIcicleDodgeTrigger::IsActive`** (`:94`, `:99`) - test
`ULDUAR_HODIR_ICE_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN` and
`ULDUAR_HODIR_BIG_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN`. Leave
`HodirIcicleDodgeAction::Execute` on the CLEAR values - the margin still governs where the bot lands,
only not whether it leaves. Leave the segment test at `:139-142` on CLEAR too; that one tests a path
the bot is about to walk, where margin is correct.

### 3. Hodir-local paladin frost resistance

Per decision, the shared `BossAuraTriggers` / `BossAuraActions` stay untouched - Thorim and VoA keep
using them.

New `HodirFrostResistanceTrigger` and `HodirFrostResistanceAction` in the Hodir files, plus a
`HodirPaladinAuraMultiplier`.

- Trigger: Hodir engaged; `bot->getClass() == CLASS_PALADIN`; bot knows any of
  `SPELL_FROST_RESISTANCE_AURA_RANK_1..5` (`BossAuraTriggers.h:17-32`); does not already have
  `botAI->HasAura("frost resistance aura", bot)`; and is the chosen paladin.
- Chosen paladin: walk the group in order and take the first alive paladin that is neither
  `botAI->IsTank` nor `botAI->IsHeal`; if there is none, take the first alive paladin of any spec.
  Put this in `UldBossHelper` as `GetHodirResistancePaladin` so trigger and multiplier agree.
- Action: `botAI->DoSpecificAction("frost resistance aura", Event(), true)` and nothing else. No
  `ChangeStrategy`, so there is nothing to leak.
- `HodirPaladinAuraMultiplier`: while Hodir is engaged and this bot is the chosen paladin, return
  `0.0f` for `CastDevotionAuraAction`, `CastRetributionAuraAction`, `CastConcentrationAuraAction`,
  `CastCrusaderAuraAction`, `CastSanctityAuraAction`, `CastShadowResistanceAuraAction` and
  `CastFireResistanceAuraAction` (`PaladinActions.h:30-37`); `1.0f` otherwise. Without it the
  paladin's own buff strategy re-casts Retribution Aura on the next GCD - the aura slot is exclusive,
  the same failure the anti-fear component documents for the shaman earth totem.

Wiring: repoint the `hodir frost resistance trigger` node in `UldStrategy.cpp:376` at the new trigger
and action, keeping `ACTION_RAID`; update the two factories in `UldTriggerContext.h:225` and
`UldActionContext.h:226`; register the multiplier in `UldStrategy::InitMultipliers` next to
`HodirGuardMultiplier`. Leave the Thorim node alone.

## Files

- `src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.{h,cpp}`
- `src/Ai/Raid/Uld/Action/UldActions_Hodir.{h,cpp}`
- `src/Ai/Raid/Uld/Util/UldBossHelper.{h,cpp}`
- `src/Ai/Raid/Uld/UldMultipliers.{h,cpp}`
- `src/Ai/Raid/Uld/UldStrategy.cpp`, `UldActionContext.h`, `UldTriggerContext.h`

No `CMakeLists.txt` - AzerothCore globs module sources.

## Deliberately out of scope

- The shelter run (`hodir move snowpacked icicle`, 8% of ranged time). It works and it is what keeps
  raiders out of Flash Freeze.
- `hodir spread storm cloud` issuing 1977 moves that all come back `wait`. Wasteful but it is not
  moving anyone; 3.7% of ranged time.
- The shared `BossFrostResistanceTrigger` spec-blindness and the never-removed `rfrost` strategy.
  Real, still broken for Thorim and VoA, deliberately left for a separate change.
- Thorim's strategy running live through the whole Hodir fight (4264 `thorim.squad` notes). Second
  trace in a row; separate.
- `ULDUAR_HODIR_TOASTY_FIRE_RADIUS` and the Biting Cold shuttle constants - unchanged, the fire
  parking is expected to make the shuttle mostly unnecessary rather than replace it.

## Verification

The module cannot be compiled headless here. Hand the build off, then verify from a fresh trace.

1. Rebuild and restart `ac-worldserver`. Confirm effective config with
   `docker exec ac-worldserver env | grep ^AC_` - `configurationOverrides/*.env` overrides
   `playerbots.conf`.
2. Pull Hodir with the same 24-bot raid, by boss so the trace stays boss-scoped.
3. On the new trace in `env/dist/logs/botobs/`:
   - **Tanks**: fraction of alive tank samples with `u[7] == hodir` must go from ~51-54% to near
     100%, and no tank sample may carry a 32926/32938 target. Hodir's distance from
     `ULDUAR_HODIR_MAINTANK_SPOT` (median 11.8, p90 52.3 here) should collapse for as long as a tank
     lives.
   - **Movement**: ranged/heal moving share of alive samples, 57-58% here. Target under 40%. Cast
     share, 27% here, should rise with it. Re-run the per-issuer attribution -
     `hodir raid position action` should drop well below its 16.7/18.8% and
     `hodir biting cold shed` below 9.8/11.3% whenever a fire is adopted.
   - **Fire adoption**: count `hodir.fire` notes resolving to a creature rather than `none`. Roughly
     50% of samples is the measured ceiling for this trace's fire placement; well below that means
     the gates are too tight.
   - **Starlight**: aura 62807 bot-seconds should rise above the ~264 here. `hodir.anchor` notes
     distinguish the two anchor kinds by their tolerance suffix - `~2.0` is a ring slot, `~1.0` is a
     Starlight stand - so the share of ticks spent on a zone is readable directly. The gain is a
     bonus, not the point; what matters is that Starlight is no longer driving movement.
   - **Dodge**: `hodir icicle dodge action` accepted moves, 2300 here, should fall. No deaths to
     62457 or 65370 - there was one 65370 death here (Tree).
   - **Paladin**: exactly one paladin casting 48945, and it must be the retribution one. Confirm the
     tank and holy paladins keep their own auras.
   - `postmortem.py <file>` summary: tanks alive past 5:11, boss below 60%.
4. Keep `603_3_hodir_1787582677.ndjson` for before/after comparison. Retention is 7 days by default.

Once the work ships, fold the durable findings into
`modules/mod-playerbots/docs/raids/ulduar.md` (Hodir section) and delete the plan directory. The
Starlight radius correction in particular contradicts what that file currently says.

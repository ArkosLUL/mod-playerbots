# Ritual of Souls + Soulwell for Playerbots — Implementation Plan

## Context

Warlock playerbots can create Healthstones one-at-a-time (`create healthstone`), but there is **no** Ritual of Souls / Soulwell behavior anywhere in the module. Confirmed by case-insensitive searches for `ritual of souls`, `soulwell`, `181621`, `ritual of summoning`, `healthwell` — all zero matches.

Goal: a warlock bot casts **Ritual of Souls**, which spawns a ritual portal GO; the core requires additional players to click it to finish the channel. So **2 other bots** must walk to the portal and click it to complete the ritual. On completion a **Soulwell** GO spawns, and then **every bot lacking a healthstone** walks to the Soulwell and clicks it to receive a (Fel) Healthstone.

User decisions:
- **Trigger:** warlock auto-casts out of combat when group members lack healthstones (and it has a soul shard), *and* it is also castable on demand via chat command.
- **Enablement:** **opt-in**, via a named non-combat strategy toggled with `nc +ritualofsouls` (mirrors the soulstone/pet-summon toggle pattern). Off by default. The on-demand chat command works regardless of the toggle (it invokes the action directly).

The mechanic itself (ritual channel, participant counting, soulwell spawn, healthstone hand-out) is handled server-side by AzerothCore's `GameObject::Use` for `GAMEOBJECT_TYPE_RITUAL` and the Soulwell GO. The module only needs bot **behaviors**: cast, walk-and-click the portal, walk-and-click the soulwell.

## Architecture split

The whole feature ships behind one opt-in non-combat strategy name, **`"ritualofsouls"`**, enabled with `nc +ritualofsouls`. Because behavior is needed by two different audiences, the strategy is registered in two contexts under the same name:

- **All classes** — joining the ritual as a helper-clicker, and grabbing from the soulwell. A base `RitualOfSoulsStrategy` (in `src/Ai/Base/Strategy/`) wires the join + soulwell triggers. Its actions/triggers are class-independent, so they register in the shared `src/Ai/Base/ActionContext.h` and `src/Ai/Base/TriggerContext.h`. Registered as `"ritualofsouls"` in the base `StrategyContext` (`src/Ai/Base/StrategyContext.h`), off by default. Every non-warlock class gets exactly this when the toggle is on.
- **Warlock** — additionally casts the ritual. A `WarlockRitualOfSoulsStrategy : RitualOfSoulsStrategy` overrides `InitTriggers` to call the base (join + soulwell) *and* add the cast trigger. Registered as `"ritualofsouls"` in `WarlockAiObjectContext`, which overrides the base entry for warlocks only, so a warlock bot gets cast + join + soulwell from one toggle.

Strategy-name lookup resolves through the class context first, then shared (`AiObjectContext::GetStrategy`), so the warlock override wins for warlocks and the base version serves everyone else — no duplication of the join/soulwell triggers.

Note: the on-demand chat command invokes the `"ritual of souls"` action directly and does **not** depend on the toggle being on. But the helper-click and soulwell-grab behaviors *do* require `nc +ritualofsouls` to be enabled on the group's bots — so a command-cast is only useful when the group has the strategy enabled (document this).

## Reused patterns (do not reinvent)

- **Move-to-GO then click:** `EnterTwilightPortalAction::Execute` — `src/Ai/Raid/OS/OSActions.cpp:182-199`: `FindNearestGameObject(entry, range)` → if `!IsAtInteractDistance(bot)` `MoveTo(go, ...)` → else build `WorldPacket(CMSG_GAMEOBJ_USE) << go->GetGUID()` and `bot->GetSession()->HandleGameObjectUseOpcode(data)`. Copy this exactly for both the portal-join and soulwell-use actions.
- **Cross-bot reservation to avoid stampede/duplication:** `soulstoneReservations` static map + mutex + `CleanupSoulstoneReservations()` — `src/Ai/Class/Warlock/WarlockActions.cpp:250-266`. Reuse this shape to (a) cap ritual helpers at the required count and (b) ensure only one warlock in a group casts.
- **Self-cast warlock spell:** `CastCreateHealthstoneAction : CastBuffSpellAction` — `src/Ai/Class/Warlock/WarlockActions.h:73-77`. The base `CastBuffSpellAction` resolves the spell by name (highest known rank) and self-targets. Ritual of Souls is a self-cast, so this is the base class.
- **Trigger + strategy wiring for a warlock non-combat cast:** `no healthstone` → `create healthstone` — `GenericWarlockNonCombatStrategy.cpp:91`, trigger `HasHealthstoneTrigger`/`WarlockConjuredItemTrigger` — `WarlockTriggers.h:108-131`.
- **Inventory item lookup by name (class-independent):** `AI_VALUE2(std::vector<Item*>, "inventory items", "healthstone")` and `"soul shard"` (same accessor the soulstone code uses at `WarlockActions.cpp:239`). Confirm the `"healthstone"` item-name mapping resolves the Fel Healthstone ids too; if not, add the ids.
- **Shared game-object-use action registration precedent:** `UseMeetingStoneAction` registered as `"use meeting stone"` in the base `WorldPacketActionContext.h:66,134`.

## IDs to verify against the DB (do this first)

Query the world DB before hardcoding:

```sql
SELECT entry, name, type FROM gameobject_template WHERE name LIKE '%Ritual of Souls%' OR name LIKE '%Soulwell%';
SELECT id, name FROM spell_dbc WHERE name LIKE 'Ritual of Souls%'; -- expect 29893 (R1), 58887 (R2)
```

Expected: Soulwell GO ~`181621`; the Ritual of Souls portal GO entry and the required-participant count come from `gameobject_template` (`type = 18` = `GAMEOBJECT_TYPE_RITUAL`, its `data` fields hold the participant requirement and the completion spell). Define the confirmed entries as named constants. Also confirm whether the caster is auto-added as owner/channeler (so only 2 *helpers* are needed) or must also click.

## Components to build

### New shared file: `src/Ai/Base/Actions/RitualOfSoulsActions.h` / `.cpp`

Holds the class-independent actions + triggers + GO/spell constants + the reservation map.

1. **`JoinRitualOfSoulsAction : Action`** — name `"join ritual of souls"`.
   - Find nearest ritual GO within range owned by a **group member** (by entry `GO_RITUAL_OF_SOULS`, guarded with `GetGoType() == GAMEOBJECT_TYPE_RITUAL` and owner-in-group). If bot is the owner (the caster), return false.
   - Reservation cap: keyed by the ritual GO GUID, allow only the required number of helpers to claim a slot (mirror `soulstoneReservations`, with expiry + `CleanupRitualReservations()`); if slots full and this bot isn't a holder, return false. This prevents the whole raid stampeding — overshoot is harmless server-side, but the cap keeps it tidy.
   - If not at interact distance → `MoveTo`; else `CMSG_GAMEOBJ_USE` (OS pattern).

2. **`UseSoulwellAction : Action`** — name `"use soulwell"`.
   - If `AI_VALUE2(std::vector<Item*>, "inventory items", "healthstone")` is non-empty → false (already has one).
   - Find nearest `GO_SOULWELL` within range owned by a group member → none → false.
   - If not at interact distance → `MoveTo`; else `CMSG_GAMEOBJ_USE`. Soulwell has limited charges (enough for a raid), overshoot fine.

3. **`RitualPortalNearbyTrigger : Trigger`** — name `"ritual of souls portal nearby"`. Active when a `GO_RITUAL_OF_SOULS` owned by a group member is in range, bot is out of combat, and bot is not the owner.

4. **`SoulwellNearbyTrigger : Trigger`** — name `"soulwell nearby"`. Active when a `GO_SOULWELL` owned by a group member is in range, bot is out of combat, and bot has no healthstone.

### Warlock caster side

5. **`CastRitualOfSoulsAction : CastBuffSpellAction`** — name `"ritual of souls"`, in `WarlockActions.h/.cpp`.
   - `isUseful()` gate: bot in a group; not in combat; has a soul shard (reagent); no ritual GO **and** no soulwell already nearby (avoid recasting); at least K group members (incl. self) lack a healthstone; and enough nearby out-of-combat group members to complete the ritual. Add a **group-level cast reservation** (mirror the reservation map, keyed by group GUID) so multiple warlocks in one group don't all cast.
   - `Execute()` = base self-cast.

6. **`RitualOfSoulsTrigger`** — in `WarlockTriggers.h/.cpp`, name e.g. `"group needs healthstones"`. Active when the `isUseful` preconditions hold (has shard, group members lack healthstones, no ritual/soulwell nearby). Keep the heavy checks in one place (trigger or action) and keep the other lightweight.

### Opt-in strategy classes

7. **`RitualOfSoulsStrategy : NonCombatStrategy`** — new, `src/Ai/Base/Strategy/RitualOfSoulsStrategy.h/.cpp`. `getName()` → `"ritualofsouls"`. `InitTriggers` wires:
   - `TriggerNode("ritual of souls portal nearby", { NextAction("join ritual of souls", 27.0f) })`
   - `TriggerNode("soulwell nearby", { NextAction("use soulwell", 26.0f) })`
   (Do **not** call `NonCombatStrategy::InitTriggers` here if that would pull in unrelated defaults — keep it to just these two.)

8. **`WarlockRitualOfSoulsStrategy : RitualOfSoulsStrategy`** — new, in `src/Ai/Class/Warlock/Strategy/GenericWarlockNonCombatStrategy.h/.cpp` (or its own file). `InitTriggers` calls `RitualOfSoulsStrategy::InitTriggers(triggers)` first, then adds `TriggerNode("group needs healthstones", { NextAction("ritual of souls", 26.5f) })`.

## Wiring sites

1. **`src/Ai/Class/Warlock/WarlockActions.h/.cpp`** — declare/define `CastRitualOfSoulsAction` (+ group-cast reservation helpers).
2. **`src/Ai/Class/Warlock/WarlockTriggers.h/.cpp`** — declare/define `RitualOfSoulsTrigger`.
3. **`src/Ai/Class/Warlock/WarlockAiObjectContext.cpp`** — register: action `"ritual of souls"` (creator + factory fn, ~lines 244-381) and trigger `"group needs healthstones"` (~lines 139-237), following the `create healthstone` / `no healthstone` entries; **and** strategy `"ritualofsouls"` → `WarlockRitualOfSoulsStrategy` in the warlock strategy factory (~lines 23-137), overriding the base entry for warlocks.
4. **`src/Ai/Class/Warlock/Strategy/GenericWarlockNonCombatStrategy.h/.cpp`** — add `WarlockRitualOfSoulsStrategy` (component #8). Do **not** touch `GenericWarlockNonCombatStrategy::InitTriggers` (the auto-cast now lives in the opt-in strategy, not the always-on `nc`).
5. **`src/Ai/Base/ActionContext.h`** — register shared actions `"join ritual of souls"` and `"use soulwell"` (creators block ~80-281, factory fns ~285-488).
6. **`src/Ai/Base/TriggerContext.h`** — register shared triggers `"ritual of souls portal nearby"` and `"soulwell nearby"` (creators ~34-247, factories ~251-461).
7. **`src/Ai/Base/StrategyContext.h`** — register strategy `"ritualofsouls"` → `RitualOfSoulsStrategy` (creators ~73-129, factories ~162-204), following `"worldbuff"`/`"rtsc"`. This is the base (all-class) version; off by default.
8. **Chat command** — expose `"ritual of souls"` so the master can type it on demand. Follow whatever plumbing existing on-demand warlock casts use to reach the action by name (check `ChatCommandHandlerStrategy` / `SayAction.cpp:38` and the chat action context); if command-cast reuses the same action name, no extra class is needed, only the command registration.
9. **`CMakeLists.txt` / module source globbing** — confirm the new files (`src/Ai/Base/Actions/RitualOfSoulsActions.cpp`, `src/Ai/Base/Strategy/RitualOfSoulsStrategy.cpp`) are picked up (the module likely globs `src/**`; verify, add if not).

## Behavior notes / edge cases

- **Combat suppression:** all four triggers gate on out-of-combat so bots don't run off mid-fight.
- **Reagent:** Ritual of Souls consumes a Soul Shard — the `has soul shard` gate avoids wasted cast attempts; the existing `create soul shard` non-combat trigger keeps shards stocked.
- **Range:** use a generous find range (~40–60 yd) for GO discovery; movement handles the approach.
- **One warlock casts:** group-keyed cast reservation prevents duplicate rituals.
- **Helper cap:** ritual-GUID-keyed reservation caps helpers at the required participant count; extra bots' `JoinRitualOfSoulsAction` returns false after slots fill.
- **Owner exclusion:** the casting warlock must not try to "join" its own ritual portal.

## Verification

**Static (here):**
- After editing, grep to confirm every new action/trigger name is registered exactly once and referenced in a strategy; confirm no dangling `NextAction("...")` without a matching creator.
- Confirm the new `.cpp` files are in the build (CMake/glob).
- Cannot compile the module headless in this environment — hand off the build to the user or CI.

**In-game manual test (user runs after build):**
0. Enable the toggle on the group's bots: `nc +ritualofsouls`. Confirm it is **off** without this (no auto-cast, no auto-join).
1. Group = 1 warlock bot + ≥3 other bots, out of combat, warlock holding a soul shard, no one has a healthstone.
2. Expect: warlock casts Ritual of Souls → portal GO spawns.
3. Expect: exactly 2 (or the required count) other bots run to the portal and click; ritual completes; Soulwell spawns.
4. Expect: every bot without a healthstone walks to the Soulwell and clicks, receiving a Fel Healthstone; bots that already have one don't.
5. Issue the chat command form and confirm on-demand cast works even with the toggle off (helpers only respond if their toggle is on).
6. Pull a mob mid-sequence and confirm bots stop running to the GO while in combat.
7. Two warlocks in group → only one ritual is cast.

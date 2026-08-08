# Ignis the Furnace Master — playerbot raid strategy

## Context

`docs/raids/ulduar.md` lists Ignis under "Normal-mode gaps still open" as *"Entire fight
unimplemented except a fire-resistance buff. No Slag Pot, Flame Jets, Scorch ground fire, or Iron
Construct tanking/kiting"*. The code has since grown two more nodes (scorched-ground avoid, and a
skull mark on any activated construct), but the **encounter's defining loop is still missing**: an
Iron Construct only dies to the Molten → Brittle → Shatter chain, and every construct left alive
adds a stack of Strength of the Creator (64473) to the boss. Bots today let constructs pile up and
lose to the boss's ramping damage.

Goal: bots kill Ignis in 10/25 normal and heroic without the raid cheat carrying the fight —
constructs get driven through Scorched Ground into water and shattered, Slag Pot victims get
healed, and nobody stands next to a Molten construct.

**Decisions already taken with the user:**
- Construct kiting is **assist-tank only**. No main-tank or DPS fallback — a raid without an assist
  tank simply burns constructs down where they stand.
- Scope: brittle/shatter loop, Slag Pot healing + hold-still, Molten-construct avoidance,
  skull-on-Brittle-only.
- **Flame Jets (62680) is deliberately not handled** — raid-wide, no dodge, no soak, and generic
  healing already covers it.

## Encounter facts (verified against this core)

Source: `src/server/scripts/Northrend/Ulduar/Ulduar/boss_ignis.cpp`.

| Id | Meaning |
|---|---|
| 33118 | Ignis (`NPC_IGNIS`, from core `ulduar.h`; **not** currently in `UldScripts.h`) |
| 33121 | Iron Construct (already `NPC_IGNIS_IRON_CONSTRUCT`) |
| 33123 | Scorched Ground (already `NPC_IGNIS_SCORCHED_GROUND`) |
| 22515 | generic World Trigger, doubles as the water trigger |
| 38757 | dormant-construct aura + `UNIT_FLAG_NOT_SELECTABLE` |
| 65667 | Heat (stacks on a construct standing in Scorched Ground) |
| 62373 | Molten — applied at 10 Heat stacks, **wipes the construct's threat table** |
| 62382 / 67114 | Brittle — 10-man / 25-man |
| 62717 / 63477 | Slag Pot — 10-man / 25-man |
| 64473 | Strength of the Creator (one stack per live activated construct) |

Chain: construct activates every 40s (10-man) / 30s (25-man) → drag into Scorched Ground → 10 Heat
stacks → Molten → bring within **18 yd of a water trigger** → Brittle → one hit of ≥5000 (62382) /
≥3000 (67114) damage shatters and kills it, removing a Strength stack.

Scorch spawns its ground NPC 20 yd in front of Ignis for 30s and **only ignites if it is more than
25 yd from water** — so the fire patches and the pools are always separate places, and the tank
genuinely has to walk the construct from one to the other.

**Water pool positions** (`creature` table, entry 22515, map 603, inside the Ignis leash box
X 490–690 / Y 130–410):

```
west  (526.771, 277.796, 360.802)
east  (646.771, 277.796, 360.802)
```

Hardcode these as `Position` constants rather than scanning for entry 22515 — that entry is a
generic world trigger reused all over Ulduar.

**Heroic**: the only differences are the paired spell ids above and the construct cadence. Predicates
that check *both* ids give 10/25 and normal/heroic for free, per the recipe. There is no Ignis hard
mode.

## Implementation

All work is inside `src/Ai/Raid/Uld/`. The `ulduar` strategy is already registered in all four
external wiring sites — **no edits outside `Uld/` and `docs/`**.

### 1. `Util/UldBossHelper.h` / `.cpp`

Extend the existing `// Ignis the Furnace Master` block of `UlduarIDs` (currently two entries) with
`NPC_IGNIS = 33118` and the spell ids from the table above (`SPELL_IGNIS_CONSTRUCT_INACTIVE`,
`SPELL_IGNIS_MOLTEN`, `SPELL_IGNIS_BRITTLE_10/_25`, `SPELL_IGNIS_SLAG_POT_10/_25`).

Add `constexpr float` radii next to the existing ones: brittle radius `18.0f`, scorched-ground
parking distance `3.0f`, Molten avoid radius `12.0f`, construct search radius `60.0f`.

Add the two `Position` constants (`extern const` in the header, defined at the top of the `.cpp`
beside `ULDUAR_THORIM_*`).

New free predicates (declared with the other encounter-state predicates, ~line 299):

```cpp
Unit* GetIgnis(PlayerbotAI* botAI);
bool IsIgnisConstructActivated(Unit const* construct);
bool IsIgnisConstructMolten(Unit const* construct);
bool IsIgnisConstructBrittle(Unit const* construct);
Unit* GetIgnisBrittleConstruct(PlayerbotAI* botAI);
Unit* GetIgnisNearestMoltenConstruct(PlayerbotAI* botAI, WorldObject const* from);
Unit* GetIgnisDrivenConstruct(PlayerbotAI* botAI, Player* tank);
Unit* GetIgnisNearestScorchedGround(PlayerbotAI* botAI, WorldObject const* from);
Position const& GetIgnisNearestWaterPool(WorldObject const* from);
Player* GetIgnisConstructTank(PlayerbotAI* botAI, Player* bot);
Player* GetIgnisSlagPotVictim(PlayerbotAI* botAI);
```

Notes:
- `GetIgnis` uses `GetFirstAliveUnitByEntry(botAI, NPC_IGNIS)`, **not** `"find target"`. A bot
  parked on a construct does not have Ignis on its threat list, so the name lookup returns null —
  the same reason `GetXT002` exists. Switch the two existing Ignis triggers over to it as well;
  they are in the file being edited and share the bug.
- `IsIgnisConstructActivated` = alive, not `UNIT_FLAG_NOT_SELECTABLE`, no aura 38757.
- `GetIgnisDrivenConstruct` returns the nearest activated, **non-Brittle** construct — once Brittle
  the tank's job is done and the raid takes over.
- `GetIgnisConstructTank` is `GetGroupAssistTank(botAI, bot, 0)` with **no fallback**; returning
  null disables the whole kiting path by design.

### 2. `Trigger/UldTriggers_Ignis.{h,cpp}`

Keep `IgnisScorchedGroundTrigger`. **Replace** `IgnisIronConstructTrigger` (marks any activated
construct) with the brittle-only marker. Every trigger opens with a `GetIgnis(botAI)` alive gate.

| Trigger | Active when |
|---|---|
| `ignis construct tank trigger` | `GetIgnisConstructTank(botAI, bot) == bot` and `GetIgnisDrivenConstruct` returns a construct |
| `ignis brittle construct mark trigger` | `IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID)` and either a Brittle construct is up and unmarked, **or** skull is on a construct that is no longer Brittle (so the mark falls back to Ignis) |
| `ignis attack brittle construct trigger` | a Brittle construct is up, bot is not the construct tank, bot's current target is not it |
| `ignis molten construct avoid trigger` | bot is not the construct tank and a Molten construct is within the avoid radius |
| `ignis slag pot heal trigger` | `botAI->IsHeal(bot)` and `GetIgnisSlagPotVictim` returns someone |

### 3. `Action/UldActions_Ignis.{h,cpp}`

Keep `IgnisScorchedGroundAction`. Each action's `isUseful()` re-instantiates its trigger and
delegates, matching the existing file.

**`IgnisConstructTankAction : MovementAction`** — the core of the change:

```
construct = GetIgnisDrivenConstruct(botAI, bot); if (!construct) return false;
if (current target != construct)      return Attack(construct);
if (construct->GetVictim() != bot)    return botAI->DoSpecificAction("taunt spell", event, true);

if (IsIgnisConstructMolten(construct))
    pool = GetIgnisNearestWaterPool(construct);
    if (construct within brittle radius - margin of pool) return false;   // stand still, let the 1s poll fire
    return MoveTo(pool);

fire = GetIgnisNearestScorchedGround(botAI, construct);
if (!fire) return false;                                  // no patch up yet, hold it where it is
if (bot within parking distance of fire) return false;    // already on the patch, construct follows into it
return MoveTo(fire position);
```

Aggro is re-established before every move so the construct actually follows — Molten wipes the
threat table, which is exactly when the tank needs to re-taunt. Mirror the `MoveTo` call shape used
at `src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp:217` (`MovementPriority::MOVEMENT_FORCED`,
exact waypoint).

**`IgnisBrittleConstructMarkAction : Action`** — skull + `SetRtiTarget(botAI, "skull", …)` on the
Brittle construct; if none, put both back on Ignis so `attack rti target` never dangles on a corpse.

**`IgnisMoltenConstructAvoidAction : MovementAction`** — find the nearest construct carrying 62373
and call `FleePosition(construct->GetPosition(), radius)` (`MovementActions.cpp:2250`).
`MoveAwayFromCreatureAction` cannot be reused here: it keys on creature **entry**, so it would flee
every construct including the one the tank is driving.

**`IgnisSlagPotHealAction : Action`** — direct copy of `JaraxxusHealIncinerateTargetAction`
(`src/Ai/Raid/ToC/ToCActions.cpp:410`) with the Slag Pot ids: walk `"group members"` for the aura,
then try the direct-heal name list via `botAI->CanCastSpell` / `CastSpell`.

### 4. `UldMultipliers.{h,cpp}` — `IgnisMultiplier`

Early-out `1.0f` unless `GetIgnis(botAI)` is up, then two suppressions:

- Slag Pot victim + any `MovementAction` → `0.0f`. The pot is a vehicle ride; movement orders just
  fight the ride and the victim cannot reposition anyway.
- `"ignis scorched ground action"` for the designated construct tank → `0.0f`. It has to stand in
  the fire it is dragging the construct through; the generic avoid would undo the kite every tick.

### 5. `UldStrategy.cpp`

Replace the three-node Ignis block:

| Trigger | Action | Priority |
|---|---|---|
| `ignis fire resistance trigger` | `ignis fire resistance action` | `ACTION_RAID` |
| `ignis scorched ground trigger` | `ignis scorched ground action` | `ACTION_RAID + 2` |
| `ignis construct tank trigger` | `ignis construct tank action` | `ACTION_RAID + 3` |
| `ignis brittle construct mark trigger` | `ignis brittle construct mark action` | `ACTION_RAID + 4` |
| `ignis attack brittle construct trigger` | `attack rti target` | `ACTION_RAID + 2` |
| `ignis molten construct avoid trigger` | `ignis molten construct avoid action` | `ACTION_EMERGENCY` |
| `ignis slag pot heal trigger` | `ignis slag pot heal action` | `ACTION_EMERGENCY + 1` |

Marking outranks tanking so the raid's kill target stays current through the kite; both outrank
`attack rti target` so nobody swaps before the mark moves. Dodging a Molten construct and saving the
pot victim are `ACTION_EMERGENCY` — both kill a bot outright.

`InitMultipliers`: `multipliers.push_back(new IgnisMultiplier(botAI));`

### 6. `UldTriggerContext.h` **and** `UldActionContext.h`

Register every new name in **both** files — `creators[…]` entry plus the private static factory.
These are two separate edits and missing one is the single most common bug here: `thorim fall from
floor action` is wired in the strategy and registered on the trigger side only, and is dead today.
Remember to delete the two `ignis iron construct …` entries being replaced.

### 7. `docs/raids/ulduar.md`

Drop the Ignis row from "Normal-mode gaps still open" and add an `### Ignis` section: the two water
pool coordinates, the Molten → Brittle → Shatter loop, the assist-tank-only decision and what
happens without one, and Flame Jets recorded as deliberately unhandled with the reason.

## Verification

1. `python apps/codestyle/codestyle-cpp.py` — must pass before this is called done.
2. Name-wiring sweep: for each new trigger/action name, `grep -c` across `UldStrategy.cpp`,
   `UldTriggerContext.h`, `UldActionContext.h` — every action name must appear in the strategy and
   the action context, every trigger name in the strategy and the trigger context. This is the check
   that catches the Thorim-class bug.
3. Header/definition sweep: every predicate declared in `UldBossHelper.h` has a definition in
   `UldBossHelper.cpp`.
4. **Compiling is not possible in this environment** (headless, no toolchain for the module) — do
   not claim a build. State plainly that it is unbuilt and hand off to the user for
   `worldserver` compile plus an in-game 10N and 25H Ignis pull.
5. In-game check, if the user runs it: constructs get taunted, walked onto a fire patch, go Molten,
   get walked to a pool, go Brittle, take the skull and die; Strength of the Creator never climbs
   past a couple of stacks; a Slag Pot victim gets direct-healed and does not try to run.

# Mimiron: cheat-free strategy rebuild

## Context

A 25-man Mimiron attempt wiped in phase 2 — every bot stacked in front of VX-001 and died to
P3Wx2 Laser Barrage. Investigation found the barrage trigger essentially never fires, the dodge
geometry is built on a wrong model of the mechanic, and the phase-2 positioning parks the raid on
top of the boss. Several other mechanics have no handling at all, and two existing behaviours are
cheats.

Goal: a Mimiron strategy that beats the encounter — normal and Firefighter hard mode — using only
movement, target selection and cooldowns a human raid could execute.

**Hard constraint: zero cheats.** No `bot->TeleportTo` dodges, no `unit->Kill()` on mechanic adds,
no `HasCheat(BotCheatMask::raid)` paths. One deliberate, gated exception is documented in W8.

Guide cross-checked: <https://www.warcrafttavern.com/wotlk/guides/mimiron-strategy-guide-ulduar-25/>
(fetch 403s; content via search). **Where the guide and this server's script disagree, the script
wins** — see the barrage geometry below, where they differ materially.

Assumed applied before testing: `data/sql/updates/db_world/2026_08_10_00.sql`. It spawns NPC 33576
and its waypoint path. It is currently **not** applied (latest applied `2026_08_07_01`;
`SELECT COUNT(*) FROM creature WHERE id=33576` returns 0), and without it `FaceBarrageArc` returns
early and the beam never rotates.

---

## Encounter facts (verified against `boss_mimiron.cpp`, Spell.dbc exports and the live world DB)

### Laser Barrage — the geometry the old plan got wrong

| Fact | Source |
|---|---|
| `SPELL_SPINNING_UP` 63414 — instant, **4000 ms** aura. The only warning. | SpellDuration idx 35 |
| `SPELL_P3WX2_LASER_BARRAGE` 63274 — **10000 ms** aura on VX-001 | idx 1 |
| **63297 / 64042 are `SPELL_EFFECT_DUMMY`** — beam *visuals* only, placed via `TARGET_DEST_CASTER_FRONT` (60 yd) + `TARGET_DEST_DEST_LEFT` 4 yd / `TARGET_DEST_DEST_RIGHT` 6 yd. **They deal no damage.** | spell.reference.csv |
| **The damage is 63293**: `SPELL_EFFECT_SCHOOL_DAMAGE`, `TARGET_UNIT_CONE_ENEMY_104`, radius idx 28 = **50000 yd** | spell.reference.csv |
| `TARGET_UNIT_CONE_ENEMY_104` → **104° cone** (`±52°` via `HasInArc`, which compares `arc/2` and adds an angular target-size bonus). No `spell_cone_angle` table exists in this DB, so no override. | `Spell.cpp:1257`, `Position.cpp:148-180` |
| Beam bearing = `VX-001 → NPC 33576`, which orbits the room on a 19-point CatmullRom spline, **one lap per 34016 ms → 10.6 °/s, clockwise** (waypoint order is angle-decreasing) | `FaceBarrageArc`, `boss_mimiron.cpp:1228-1245`; `2026_08_10_00.sql` |
| Repeats every **60 s**. Rapid Burst / Hand Pulse are pushed out 14.5 s, so VX-001's facing is otherwise frozen for the duration. | `boss_mimiron.cpp:1435-1447` |

**Therefore: distance from VX-001 is irrelevant.** The cone is effectively unbounded. Only bearing
matters. The guide's "30° arc" is retail's *visual*; this server's damage cone is 104°.

Let `δ` = a bot's bearing relative to the latched arc `start`. The cone covers
`[θ−52°, θ+52°]` with `θ = start − 10.6·t`, `t ∈ [0,10]`.

- Union of everything swept: `[start−158°, start+52°]`.
- Permanently safe wedge: `(start+52°, start+202°)`, ~150° wide.
- A bot is in the cone at `t=0` iff `−52° < δ < +52°`.
- A bot with `δ < −52°` is **outside at t=0** and gets swept later, at `t = (−52−δ)/10.6`.

### Everything else

| Mechanic | Facts |
|---|---|
| Rapid Burst 63387/64019 | **Also `TARGET_UNIT_CONE_ENEMY_104`**, 100 yd. Aimed at a random player every 3.2 s. **Not dodgeable by spreading.** |
| Heat Wave 64533 | Raid-wide, 50000 yd, every 10 s in P2. Unavoidable — healing check. |
| Plasma Blast 62997 | 3 s cast on the MK II's victim, every 22 s in P1. **`CumulativeAura = 0` — does not stack.** 6 s duration. |
| Napalm Shell 63666 | Random player >15 yd from MK II, **5 yd** splash (radius idx 8) |
| Shock Blast 63631 | On victim, every 30 s; schedules 10 Proximity Mines 8 s later |
| Proximity Mine 34362 | **`unit_flags = 2` (NON_ATTACKABLE) — cannot be killed legitimately.** Arms 2.5 s after spawn, then polls every 500 ms for a player within **1.9 yd**; auto-detonates at 35 s. Blast 66351 = **3 yd**. |
| Bomb Bot 33836 | `speed_run` 1.14286 = **player run speed** — cannot be outrun. Explodes on melee contact (`SMART_EVENT_DAMAGED_TARGET` → 63801, **5 yd**). `HealthModifier` 1.5873 — dies to a few ranged globals. Spawns every 15 s in P3. |
| Assault Bot 34057 | Casts Magnetic Field 64668. `HealthModifier` 15. **Drops item 46029 "Magnetic Core" at 100%.** Every 30 s. |
| Magnetic Core | Item 46029 → spell 64444 summons NPC 34068 → aura 64436 grounds the ACU (`DO_DISABLE_AERIAL`), `boss_mimiron.cpp:2084-2110` |
| Junk Bot 33855 | `HealthModifier` 5, every 10 s |
| Emergency Fire Bot 34147 | Hard mode only, 3 at a time every 45 s |
| Rocket Strike 63041 | **3 yd** blast (radius idx 15), every 20 s; both rockets in P4 |
| Hand Pulse 64348/64352 | 850 ms cast, cone, every 1.75 s in P4 |
| Taunt | All three mechs carry `flags_extra` 0x80001 = `OBEYS_TAUNT_DIMINISHING_RETURNS`. DR degrades taunt *duration* 1.0 → 0.65 → 0.4225 → 0.274625 → immune, and resets only after **15 s** without a hit (`Unit.cpp:11814`, `:11864-11885`). **None are taunt-immune** — `MechanicsMask` 0x26CB3F7F omits taunt. |
| Berserk | **15 min normal, 10 min hard mode 25** (`boss_mimiron.cpp:352,357`) |

---

## Root cause of the wipe

1. **Trigger never fires.** `MimironP3Wx2LaserBarrageTrigger` (`UldTriggers_Mimiron.cpp:75-97`) ANDs
   the aura check with `FindCurrentSpellBySpellId`. Spinning Up and the barrage are **auras**; the
   100 ms retriggers are instant triggered casts that clear `m_currentSpells` in the same update.
   The `&&` neuters the aura path.
2. **Dodge geometry is built on the wrong mechanic.** The action moves to `orientation + 22.5°` at
   radius `clamp(dist, 10, 24)` — a model of a narrow beam with width. Against a ±52° cone, 22.5°
   is *inside* it, at every radius.
3. **All bots converge on one point** — same angle, same clamped radius. No `isUseful()` override.
4. **Rapid Burst positioning parks the raid on the boss.** Six fixed spots 5-15 yd from VX-001
   (`UldBossHelper.cpp:47-53`) with `disperse distance = 0`. Its yield to the barrage trigger is
   dead because of #1 — and the mechanic those spots exist for is itself an undodgeable 104° cone.

---

## Design

### W1 — Constants and helpers

`src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp`, extending the Mimiron block at `:107-121`:

```cpp
NPC_MIMIRON_DB_TARGET  = 33576,
NPC_JUNK_BOT           = 33855,
NPC_EMERGENCY_FIRE_BOT = 34147,
NPC_MAGNETIC_CORE      = 34068,
SPELL_MIMIRON_LASER_BARRAGE_DAMAGE = 63293,   // the actual 104 deg cone
SPELL_MIMIRON_PLASMA_BLAST   = 62997,
SPELL_MIMIRON_NAPALM_SHELL   = 63666,
SPELL_MIMIRON_MAGNETIC_FIELD = 64668,
ITEM_MIMIRON_MAGNETIC_CORE   = 46029,
```

Replace `ULDUAR_MIMIRON_BARRAGE_MIN_RADIUS` (`UldBossHelper.h:371`) — radius no longer gates safety:

```cpp
ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE  = 52.0f * float(M_PI) / 180.0f;  // TARGET_UNIT_CONE_ENEMY_104
ULDUAR_MIMIRON_BARRAGE_MARGIN      = 12.0f * float(M_PI) / 180.0f;
ULDUAR_MIMIRON_BARRAGE_SWEEP_TOTAL = 106.0f * float(M_PI) / 180.0f; // 10.6 deg/s over 10 s
ULDUAR_MIMIRON_SPREAD_RADIUS_MAX   = 24.0f;   // see W3: caps rotation cost
ULDUAR_MIMIRON_BARRAGE_RELATCH_DIST = 5.0f;
ULDUAR_MIMIRON_MINE_CLEARANCE      = 5.0f;    // 3 yd blast + margin
ULDUAR_MIMIRON_NAPALM_RADIUS       = 6.0f;
ULDUAR_MIMIRON_BOMB_BOT_RADIUS     = 8.0f;    // was 6.0f; blast is 5 yd
```

Helpers:

```cpp
// Beams point at NPC 33576, which orbits the room clockwise on a fixed spline, so the bearing to it
// is the cone centreline. Falls back to VX-001's own facing if 33576 is absent.
float GetMimironBarrageAngle(Player* bot, Unit* vx001);

// Mines arm 2.5s in and fire inside 1.9 yd with a 3 yd blast. They are non-attackable, so avoidance
// is the only handling - callers reject destinations that sit in one.
bool IsMimironSpotMineSafe(Player* bot, Position const& dest,
                           float clearance = ULDUAR_MIMIRON_MINE_CLEARANCE);
```

`GetMimironBarrageAngle`: `bot->FindNearestCreature(NPC_MIMIRON_DB_TARGET, 250.0f)` →
`vx001->GetAngle(that)`. `GetAngle` is world-space, so phase 4 needs no special case — the core's
seat-local adjustment exists only because `SetFacingTo` takes a transport-relative angle.

`IsMimironSpotMineSafe`: scan `AI_VALUE(GuidVector, "nearest npcs")` for `NPC_PROXIMITY_MINE`.
Mines are non-selectable so they never reach `"possible targets"` — same reason the hard-mode
flames scan uses the raw npc list (`UldTriggers_Mimiron.cpp:401-403`).

### W2 — Barrage trigger

`UldTriggers_Mimiron.cpp:75-97`. Drop every `FindCurrentSpellBySpellId` check and the `&&`:

```cpp
Unit* boss = AI_VALUE2(Unit*, "find target", "vx-001");
if (!boss || !boss->IsAlive())
    return false;

// Spinning Up is the 4s warning, 63274/63300 the 10s barrage. All are auras - the 100ms damage
// retriggers never linger in m_currentSpells, so current-spell checks miss the whole cast.
return boss->HasAura(SPELL_SPINNING_UP) ||
       boss->HasAura(SPELL_P3WX2_LASER_BARRAGE_AURA_1) ||
       boss->HasAura(SPELL_P3WX2_LASER_BARRAGE_AURA_2);
```

### W3 — Barrage dodge, rewritten

`UldActions_Mimiron.cpp:108-132` + header `UldActions_Mimiron.h:33-46`.

**Latch.** Instance-keyed static (not per-bot), so all 25 bots compute an identical wedge — mirrors
`lurkerSpoutTimer` in `src/Ai/Raid/SSC/SSCActions.cpp`, cleared when the boss despawns exactly as
`SSCActions.cpp:38-40` does. Store `{arcAngle, vx001Position}`. Re-latch only if VX-001 has moved
more than `RELATCH_DIST` since the latch — phase 4 rides the chassis up to 30 yd off centre, which
shifts the bearing by as much as 16°.

**Selective.** Compute `δ = NormalizeOrientation(vx001->GetAngle(bot) − arcAngle)` mapped to
`(−π, π]`. Wedge test in the shape of `TheLurkerBelowRunAroundBehindBossAction`
(`SSCActions.cpp:487-491`). Bots already safe do nothing and keep DPSing.

**Proactive.** `FaceBarrageArc` runs at `EVENT_SPELL_SPINNING_UP` *before* the cast
(`boss_mimiron.cpp:1440-1441`), so the arc is knowable a full 4 s ahead. Everyone who needs to move
moves during Spinning Up.

**Constant-radius rotation.** Keep the bot's current distance, change only bearing. Melee stay at
melee range — free, since radius does not affect safety.

**Direction — split rule.** Bot angular rate is `401/r` °/s against the cone's 10.6 °/s:

| `δ` | action | travel | why |
|---|---|---|---|
| `≥ +52°` | hold | 0 | already outside on the CCW side; the cone sweeps away |
| `0 ≤ δ < +52°` | rotate **CCW** to `start + 52° + margin` | ≤ 52°+margin | ≤3.6 s at r=24, fits the 4 s window |
| `< 0` | rotate **CW** past `start − 158° − margin` | up to 106° | already behind the descending edge, or nearly; gap only widens |

Counter-clockwise from the clockwise half would cross the entire 104° cone — that is why a single
global direction does not work. Clockwise travel is safe *while in progress*: the bot descends at
`401/r` °/s against an edge descending at 10.6 °/s, so at the 24 yd cap it gains 6.1 °/s.

**Radius cap.** `ULDUAR_MIMIRON_SPREAD_RADIUS_MAX = 24` is enforced by W4's arc spread, not here.
It is what makes the worst-case 52° rotation cost 22 yd ≈ 3.1 s, inside the window, with 1.6×
margin against the sweep. Break-even is r = 38 yd, where a bot can never out-rotate the cone.

**Cast blocking.** Return `true` (ending the tick, nothing else runs) **only while this bot is
relocating**. Bots already in the wedge are never blocked.

**Mines.** The barrage node **skips** `IsMimironSpotMineSafe` — the cone kills instantly, a mine
does not. Every other Mimiron movement action still filters (W12).

Delete the "huddle on the master" fallback (`:113-119`) — it walks bots into the room centre. Add
the missing `isUseful()` override matching `MimironShockBlastAction::isUseful` (`:90-94`).

Node raised to **`ACTION_RAID + 5`**.

### W4 — Replace the six fixed P2 spots with an arc spread

Delete `ULDUAR_MIMIRON_PHASE2_SIDE*_SPOT` (`UldBossHelper.cpp:47-52`) and the spot-picking loops in
`MimironRapidBurstAction` / `MimironRapidBurstTrigger`. They stack the raid in three clumps to dodge
Rapid Burst — which is a 104° cone and cannot be dodged by spreading.

Replace with a `TheLurkerBelowSpreadRangedInArcAction`-shaped action (`SSCActions.cpp:539-596`):

- Deterministic per-bot index across the arc, cached in a GUID-keyed static, cleared when VX-001
  despawns.
- **Anchored to a fixed room bearing**, not VX-001's facing — the facing swings to whoever it last
  Rapid Burst, every 3.2 s.
- **Radius capped at `ULDUAR_MIMIRON_SPREAD_RADIUS_MAX` (24 yd)** — this is what makes W3's
  rotation always feasible.
- Spacing wide enough for Napalm Shell (5 yd splash) and Rocket Strike (3 yd).
- Keeps `ULDUAR_MIMIRON_PHASE4_TANK_SPOT` for the MT.

Node stays at `ACTION_RAID`.

### W5 — Plasma Blast tank swap

62997, 3 s cast on the MK II's victim, every 22 s in P1. Non-stacking, so the swap is about not
letting the hit land on a low tank, not about stack management.

- Partner: `IsAssistTankOfIndex(bot, 0)`. **P1 only** — in P4 the MT must hold the chassis, since
  moving it drags the barrage cone origin.
- **Alternate, never taunt back.** Tanks trade the boss on each cast and hold until the next one.
  One taunt per tank per 22 s > the 15 s DR reset, so taunt DR never engages and every taunt lands
  at full duration. Swapping back would put two taunts 11 s apart and cut duration to 65%.
- Use `DoSpecificAction("taunt spell")`, as `UldActions_IronAssembly.cpp:124` and
  `UldActions_Kologarn.cpp:74` already do.

Node at `ACTION_RAID + 1`.

### W6 — Napalm Shell spread

Trigger: bot carries aura 63666, or a player within `ULDUAR_MIMIRON_NAPALM_RADIUS` does.
Action: subclass `MoveAwayFromPlayerWithDebuffAction`
(`src/Ai/Base/Actions/MovementActions.h:318`) with `spellId = 63666`, `range = 6.0f`.
Node at `ACTION_RAID + 2`.

### W7 — Delete the teleport dodges

`MimironShockBlastAction::Execute` (`UldActions_Mimiron.cpp:69-87`) and
`MimironRocketStrikeAction::Execute` (`:322-340`) both call `bot->TeleportTo(...)` in their P3/P4
branch. Two problems:

1. **Ungated cheat.** Blinking out of damage is not something a player can do, and neither call sits
   behind `HasCheat`. The P1/P2 branch of these same actions already dodges honestly with
   `MoveAway`; the teleport exists only because nobody made the movement work with the extra mechs
   up.
2. **Stale pointer.** `target` is the leftover loop variable from the `possible targets` sweep, so
   `target->GetMapId()` can hit null or an unrelated mob.

Delete both `else` branches; let the `MoveAway` / `FleePosition` path handle every phase. If it
proves too slow, fix with movement priority or `BestPositionForMeleeToFlee` /
`BestPositionForRangedToFlee` (`MovementActions.cpp:2115`, `:2182`) — not by teleporting.

Rocket Strike stays **reactive** (the spawned marker gives enough warning, and pre-spreading would
fight the barrage wedge) but moves from `ACTION_RAID` to **`ACTION_RAID + 4`** — its current
priority is why the move often never happens before impact.

### W8 — Phase 3 adds and the Magnetic Core

Kill priority (implemented in W10's action, not here):
**Bomb Bot → Assault Bot → Emergency Fire Bot (HM) → Junk Bot → ACU.**
Assault Bot outranks fire bots because it is the only Magnetic Core source and gates the phase.

**Magnetic Core acquisition — the one gated exception to the no-cheats rule.** Bots cannot loot in
combat: looting is registered only in `LootNonCombatStrategy`
(`src/Ai/Base/Strategy/LootNonCombatStrategy.cpp:12`). Even with a loot window open,
`StoreLootAction::IsLootAllowed` (`LootAction.cpp:465-520`) would reject item 46029 — Quality 1
white consumable, `SellPrice` 0, `StartQuest` 0, no quest requires it, so it falls through to
`lootStrategy->CanLoot()` which discards junk.

Grant it directly with `StoreNewItem`, the way SSC moves the Tainted Core
(`SSCActions.cpp:2586-2591`), **gated to reproduce the real acquisition constraints**:

- an Assault Bot has actually died, **and**
- the tracker bot is within looting range of the corpse, **and**
- it does not already hold one.

The Assault Bot drops the core at 100%, so a real raid always gets it — what is bypassed is the
missing in-combat loot packet path, not a game rule. Ungated fabrication would additionally skip
the kill, the travel and the proximity, which *would* be an advantage. Put the reasoning in a
comment at the call site.

Carrier: the designated tracker bot via `IsMechanicTrackerBot`
(`src/Ai/Raid/RaidBossHelpers.h:36`). Use action follows the `UseItemAction` family
(`src/Ai/Base/Actions/UseItemAction.h:17`); gate on `bot->GetItemCount(46029, false)` and range to
the ACU. Node at `ACTION_RAID + 1`.

### W9 — Phase 4 Hand Pulse

A cone that re-aims every 1.75 s cannot be dodged, so the answer is spread, not movement. Replace
the dead commented block in `MimironPhase4MarkDpsAction` (`UldActions_Mimiron.cpp:411-414`) with
`SET_AI_VALUE(float, "disperse distance", 4.0f)` for non-tanks.

### W10 — Direct targeting, RTI removed

Mimiron currently steers DPS through raid icons: `MimironAerialCommandUnitAction` (`:264-284`) and
`MimironPhase4MarkDpsAction` (`:393-406`) set skull/cross, write the `"rti"` string, then call
`DoSpecificAction("attack rti target")`; their triggers read the string back
(`UldTriggers_Mimiron.cpp:275-277`, `:352`). That round-trip only works if every bot's `"rti"` value
happens to match.

Replace with the SWP idiom — an `AttackAction` subclass calling the protected `Attack(Unit*)`
(`src/Ai/Base/Actions/AttackAction.h:25`), which writes `"current target"`, `SetSelection`, faces
the unit and enters combat.

**New `MimironSetDpsPriorityAction : public AttackAction`**, `"mimiron set dps priority"`. Copy
`XT002SetDpsPriorityAction` (`UldActions_XT002.h:163-182`, `UldActions_XT002.cpp:464-478`) — same
raid, already direct.

```cpp
Unit* target = ResolveTarget();
if (!target) return false;
if (AI_VALUE(Unit*, "current target") != target)
    return Attack(target);
return false;   // already correct - let lower-priority nodes run this tick
```

Returning `false` on the no-op path keeps the avoid nodes running (`UldActions_XT002.cpp:471-472`).

| Phase | Non-tank order |
|---|---|
| P1 | Leviathan MK II |
| P2 | VX-001 |
| P3 | Bomb Bot (**ranged only**, see W11) → Assault Bot → Emergency Fire Bot (HM) → Junk Bot → ACU |
| P4 | highest-current-HP mech — the rule `MimironPhase4MarkDpsTrigger` already uses (`:327-349`) |

Proximity Mines must **never** appear (non-attackable). Bomb Bots must appear for ranged and never
for melee, who cannot reach one without triggering it.

Anti-flicker: keep the current target if it still matches the wanted entry, and only switch to
another instance of that entry if it is more than 10 yd closer — `SelectMuruEncounterTarget`
(`SWPActions_Muru.cpp:358-393`).

Trigger: non-tank, and at least one mech alive. Node at bare `ACTION_RAID`.

Tanks keep their own node: `MimironPhase4MarkDpsAction`'s MT branch drops the `"rti"` writes and
`DoSpecificAction("attack rti target")` and calls `Attack(highestHealthUnit)` directly.

Icons become cosmetic — one `MarkTargetWithSkull` from the tracker bot so a human raid leader can
see the focus, the Kil'jaeden pattern (`SWPActions_KJ.cpp:166-170`). Nothing reads it back.

**Multiplier, required.** Without it `DpsAssistAction` fights the node every tick. Add a Mimiron
clause to `UldMultipliers.cpp` copying `:106-110`:

```cpp
if (!botAI->IsTank(bot) && dynamic_cast<DpsAssistAction*>(action))
    return 0.0f;
```

gated on any mech being alive. Scope to `DpsAssistAction` only — leave `AttackRtiTargetAction` alone
so a raid leader's deliberate mark still steers bots.

### W11 — Delete the cheat-kill; make mine and bomb handling stand alone

Delete `MimironCheatTrigger` (`UldTriggers_Mimiron.cpp:357-379`) and `MimironCheatAction`
(`UldActions_Mimiron.cpp:420-436`), which have the MT `unit->Kill()` every nearby mine and bomb bot,
plus their context entries and the node at `UldStrategy.cpp:441-443`.

The two adds need opposite replacements:

- **Bomb Bots must be killed, not avoided.** They match player run speed, so `MoveAwayFrom...` at
  6 yd is an unwinnable race — the bot backpedals until the bomb lands a melee hit. **Three ranged
  bots picked by group index** own bomb duty (W10 priority list); `HealthModifier` 1.5873 means
  three ranged kill one well before contact. Keep the move-away action for melee and widen its
  radius 6.0f → **8.0f** (5 yd blast + margin).
- **Proximity Mines can only be avoided** — non-attackable, no legitimate removal. Keep
  `MimironProximityMineAction` at 6.0f, comfortably outside both the 1.9 yd arming poll and the
  3 yd blast. What is missing is the *pathing* (W12).

After W7 and W11, `UldTriggers_Mimiron.*` and `UldActions_Mimiron.*` must contain zero `HasCheat`,
`TeleportTo` or `Kill(` calls. `StoreNewItem` appears once, in W8, gated and commented.

### W12 — Mine-aware destinations

Every Mimiron movement action picks a destination with no idea mines exist, so a bot fleeing Shock
Blast walks through the ten mines that land 8 s later. Wire `IsMimironSpotMineSafe` into:

- **W4 arc spread** — if the assigned slot is not mine-safe, hold position rather than walking in.
- **W7 Shock Blast / Rocket Strike** — reject mine-fouled candidates from
  `BestPositionFor*ToFlee` rather than taking the first result.
- **W6 Napalm Shell** — same.
- **Not the barrage node** (W3) — the cone kills instantly, a mine does not.

Without this, deleting the cheat-kill converts mine deaths from "never" to "constantly".

### W13 — Docs

Update `docs/raids/ulduar.md`. The Mimiron section is now wrong in three places: lines 643-651
describe the pre-`FaceBarrageArc` fixed-cadence arc model; line 747 says there is no ground-fire
avoidance; line 756 says mines and bomb bots are cheat-kill only. Rewrite against the facts table
above, and record the 104°-cone finding explicitly — it is the single most load-bearing number and
it contradicts every public guide.

---

## Priority ladder

`UldStrategy.cpp:400-454` (`ACTION_RAID` = 60.0f, `Strategy.h:53-65`):

| Relevance | Nodes |
|---|---|
| `+5` | laser barrage |
| `+4` | rocket strike, dodge flames (HM), frost bomb (HM) |
| `+3` | proximity mine, bomb bot avoid |
| `+2` | shock blast, napalm shell |
| `+1` | plasma blast tank swap, magnetic core |
| `+0` | arc spread, ACU tanking, set dps priority, phase 4 mark dps, fire resistance |

The `cheat` node is gone.

## Wiring checklist

Per new component:

1. Class in `src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.h` + impl in the `.cpp`.
2. Class in `src/Ai/Raid/Uld/Action/UldActions_Mimiron.h` + impl in the `.cpp`.
3. `creators[...]` entry **and** the static factory fn in `UldTriggerContext.h` (map 72-80,
   factories 203-211) and `UldActionContext.h` (map 72-80, factories 200-208).
4. `TriggerNode` in `RaidUlduarStrategy::InitTriggers`, `UldStrategy.cpp:400-454`.
5. W10 only: the `DpsAssistAction` suppression clause in `UldMultipliers.cpp`, registered from
   `RaidUlduarStrategy::InitMultipliers` (`UldStrategy.cpp:611-644`, currently no Mimiron entry).

Deletions (W7, W11, W4) touch the same sites in reverse. No CMake edit — the module globs sources.

Hard mode stays declared by `AiPlayerbot.UlduarMimironHardMode` via `IsMimironHardModeActive`
(`UldHardMode.cpp:87`) — already wired and tested, and hard mode is a deliberate pre-pull choice.

## Verification

1. Apply `data/sql/updates/db_world/2026_08_10_00.sql`; confirm
   `SELECT COUNT(*) FROM creature WHERE id=33576` returns 1, and restart.
2. Cheat-free acceptance check:
   ```
   grep -n "HasCheat\|TeleportTo\|->Kill(" src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.* \
                                            src/Ai/Raid/Uld/Action/UldActions_Mimiron.*
   ```
   Must return nothing. `grep -rn "mimiron cheat" src/` must return nothing. The only `StoreNewItem`
   is W8's, and its guard must check kill + proximity + not-already-held.
3. Build the module (the worldserver build is the only way to compile this — it cannot be built
   headless from this checkout).
4. **Phase 2 barrage.** At Spinning Up: bots with `δ ≥ +52°` must not move at all; bots in
   `[0, +52°)` rotate counter-clockwise; bots below 0 rotate clockwise. Nobody crosses the cone.
   No bot takes 63293 damage — check the combat log for "P3Wx2 Laser Barrage". Bots that never
   moved should still be casting throughout.
5. Confirm the arc spread keeps everyone inside 24 yd — a bot beyond that cannot finish the
   rotation in the 4 s window.
6. **Phase 1.** MT and assist tank 0 alternate on each Plasma Blast and never taunt back. Napalm
   Shell target steps away.
7. **Proximity Mines.** After a Shock Blast, watch the ten mines land. No bot may walk into one
   while repositioning or fleeing — this is what the deleted cheat was hiding, so it is the main
   regression risk. Mines must survive and despawn on their own 35 s timer.
8. **Bomb Bots.** The three assigned ranged kill each bomb before contact. A bomb reaching melee
   range means the assignment is wrong — backpedalling cannot fix it.
9. **Phase 3.** DPS switch targets with no icon involved (inspect targets, not raid marks). Assault
   Bot dies, the tracker bot receives 46029 only after being near the corpse, uses it, and the ACU
   drops (`DO_DISABLE_AERIAL`).
10. **Phase 4.** No bot teleports — watch for instant repositions during Shock Blast and Rocket
    Strike; bots must visibly run. Non-tanks stay spread, DPS follow the highest-HP mech, and no bot
    writes `"rti"`.
11. **Berserk budget.** Normal is 15 min and comfortable. Re-run hard mode
    (`AiPlayerbot.UlduarMimironHardMode = 1`, `conf/playerbots.conf.dist:425-431`) against its
    10 min timer and confirm the kill lands — blocking relocating bots costs throughput. Read the
    effective config with `docker exec ac-worldserver env | grep ^AC_`;
    `configurationOverrides/*.env` overrides the conf file.

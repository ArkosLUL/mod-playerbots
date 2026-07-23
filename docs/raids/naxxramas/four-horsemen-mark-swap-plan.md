# Four Horsemen (Naxxramas) Playerbot Strategy Rework

## Context

The current 4H strategy (`src/Ai/Raid/Naxx/`) is a minimal timer-driven implementation: front zerg with fixed kill order + 2-4 rear "attractors" that swap corners on a wall-clock timer (67.5s — comment claims "24s-15s-15s", code diverged). It has **zero mark handling** — no mark spell IDs anywhere, no stack counting, no tank swap, no Void Zone dodge. Wipe scenarios:

1. Baiters eat 4-5 mark stacks per side (12.5k+/application at 5) before the timer flips → death → empty rear corner → Lady/Sir cast raid-wide punish **every AI tick** while their nearest player is >45yd → wipe.
2. Per-bot `_combat_start_ms` desync → baiters swap at different times → corner briefly empty → punish.
3. Mark aura lasts 75s from last application vs 67.5s away-time → returning baiter still carries old stacks → ratchet.
4. No front tank swap → tank stacks unbounded; front group eats both front marks.
5. Void Zones (victim-targeted — spawn **at the baiter's feet**) never dodged.
6. Attractor selection ignores dead members (`ignoreDeadPlayers` defaults false) → dead baiter never replaced.
7. After Thane+Baron die, raid tunnels Lady→Sir with no rotation.

**Agreed scope (user):** keep the zerg-front + rear-baiter skeleton; make all swaps mark-stack-driven; add all four safeties (baiter death fallback, Void Zone dodge, rear-phase rotation, Holy Wrath spread).

## Verified encounter facts (authoritative: `src/server/scripts/Northrend/Naxxramas/boss_four_horsemen.cpp` in parent repo)

- Spell IDs (script lines 26-47): Marks 28832 Korth'azz / 28833 Blaumeux / 28834 Rivendare / 28835 Zeliek; MarkDamage 28836 (script-computed per stack: 0/500/1500/4000/12500/20000/20000+1k·(n−7) — **same IDs both difficulties**); Meteor 28884; Shadow Bolt 57374; Void Zone 28863 (summons NPC **16697**, casts Consumption 28865 via SmartAI every 2.5s); Unyielding Pain 57381; Holy Bolt 57376; Holy Wrath 28883; Condemnation 57377; Unholy Shadow 28882; Berserk 26662 (10 min).
- Mark cadence: first mark 24s after that boss reaches its corner, then 15s (Lady/Sir) / 12s (Thane/Baron). Per-boss timers, not raid-synchronized — deltas self-sync **per corner** only.
- Lady/Sir: stationary, victim = `SelectNearestPlayer(300yd)` every tick; ≤45yd → bolt at victim, else → **raid-wide punish every tick** (`TABLE_SPELL_PUNISH = {Condemnation, UnyieldingPain, 0, 0}`). Baiting = being nearest; threat irrelevant. Both rear corners must always have someone within 45yd.
- Thane/Baron: normal threat melee.
- Corner waypoints (script lines 92-110): Thane (2542.9, −3015.0, 241.35), Lady (2469.4, −2947.6, 241.28), Baron (2583.9, −2971.6, 241.35), Sir (2517.8, −2896.6, 241.28). Existing bait anchors are good: Sir anchor ~21yd from Sir / ~49yd from Lady; anchors' midpoint ≤41yd from both rear bosses → crossing never triggers punish.
- Baron entry 30549 ("baron rivendare"); the `"highlord mograine"` fallback is dead weight but harmless — keep.
- Engine: per-bot trigger/action/multiplier instances (per-bot `AiObjectContext`), but trigger/action/multiplier each hold **separate** `FourhorsemanBossHelper` members → cross-object state must be shared explicitly (SSC file-static map precedent). `IsAssistRangedDpsOfIndex`/`IsAssistHealOfIndex`/`IsAssistTankOfIndex` accept `ignoreDeadPlayers` (default false) — passing `true` shifts indices past dead members → automatic promotion ([PlayerbotAI.h:437-439](modules/mod-playerbots/src/Bot/PlayerbotAI.h#L437-L439)).
- Generic `AvoidAoeAction` **cannot** see Void Zone NPC 16697 (needs dynobj/GO-trap/periodic aura; 16697 casts via SmartAI on-update) → bespoke dodge required.
- Reuse patterns: Gluth stack trigger + taunt suppression ([NaxxTriggers.cpp:223-248](modules/mod-playerbots/src/Ai/Raid/Naxx/NaxxTriggers.cpp#L223-L248), [NaxxMultipliers.cpp:517-554](modules/mod-playerbots/src/Ai/Raid/Naxx/NaxxMultipliers.cpp#L517-L554)); Festergut forced class-taunt lambda with `RemoveSpellCooldown(..., true)` ([ICCActions_FG.cpp:24-67](modules/mod-playerbots/src/Ai/Raid/ICC/Action/ICCActions_FG.cpp#L24-L67)); Sindragosa position-gated taunt; `GetCreatureListWithEntryInGrid` scan (ZAHelpers.cpp:102); `botAI->GetGroupSlotIndex(bot)`.

## Files to modify (no new files; keep existing Naxx layout)

| File | Change |
|---|---|
| `NaxxSpellIds.h` | Add Four Horsemen section |
| `NaxxBossHelper.h` | Rework `FourhorsemanBossHelper` (lines 1044-1145) |
| `Action/NaxxActions.h` | Rework action classes, rename `Attact`→`Attack`, add tank-swap action |
| `Action/NaxxActions_FourHorsemen.cpp` | New implementations + static tank-state map definition |
| `NaxxTriggers.h/.cpp` | Adjust 2 triggers, add `HorsemanFrontTankTrigger` |
| `NaxxTriggerContext.h` | Register `"horseman front tank"` |
| `NaxxActionContext.h` | Register `"horseman tank swap"`; update renamed class factory |
| `NaxxStrategy.cpp` | Add front-tank TriggerNode (~line 132) |
| `NaxxMultipliers.h/.cpp` | Rework `FourhorsemanGenericMultiplier` (add helper member + taunt suppression) |

All paths relative to `modules/mod-playerbots/src/Ai/Raid/Naxx/`.

## Implementation steps

### 1. `NaxxSpellIds.h` — Four Horsemen section

```cpp
// The Four Horsemen (boss_four_horsemen.cpp)
// Marks identical in 10/25 (damage script-computed via MarkDamage);
// bolts/punishes scale via spelldifficulty DBC from these base IDs.
static constexpr uint32 MarkOfKorthazz = 28832;
static constexpr uint32 MarkOfBlaumeux = 28833;
static constexpr uint32 MarkOfRivendare = 28834;
static constexpr uint32 MarkOfZeliek = 28835;
static constexpr uint32 KorthazzMeteor = 28884;
static constexpr uint32 BlaumeuxVoidZone = 28863;
static constexpr uint32 ZeliekHolyWrath = 28883;
static constexpr uint32 HorsemanVoidZoneEntry = 16697;  // summoned by 28863
```
(Add bolt/punish IDs only if actually referenced.)

### 2. `NaxxBossHelper.h` — `FourhorsemanBossHelper` rework

Corner model:
```cpp
enum HorsemanCorner : int { CORNER_SIR = 0, CORNER_LADY = 1, CORNER_THANE = 2, CORNER_BARON = 3, CORNER_NONE = -1 };
```
- Keep rear `attractPos[2]` + `posZ`. Add front tank spots: Thane `{2542.9f, -3015.0f}`, Baron `{2583.9f, -2971.6f}`, z 241.35f, 3yd arrival tolerance.
- Cache all four bosses (`_thane/_lady/_sir/_baron`, keep mograine fallback). `UpdateBossAI()` returns true iff **any** horseman found alive (gate fix — currently keys on Zeliek only). Delete `_combat_start_ms`, `posToGo`, `CalculatePosToGo`, `CurrentAttractPos`, `CurrentAttackTarget`, stale "24s-15s-15s" comment.
- `Unit* CornerBoss(int corner)`; `uint32 CornerMarkId(int corner)`; `uint32 GetMarkStacks(Unit* unit, int corner)` — `NaxxSpellIds::GetAnyAura` + name fallback ("mark of korth'azz" — mind apostrophe), Gluth dual-lookup pattern.
- `bool IsRearPhase()` — active and Thane+Baron dead/absent.
- `bool IsAttracter(Player* bot)` — same role split, but pass `ignoreDeadPlayers = true` on every index call (auto-promotion), and return true unconditionally in rear phase (whole raid joins crossover).
- `int HomeCorner(Player* bot)` — rdps#0 (+heal#1 25m) → `CORNER_LADY`; heal#0 (+heal#2 25m) → `CORNER_SIR`; anyone else (rear phase) → `CORNER_LADY`. Alive-aware indices.
- `int GetFrontTankAssignment(Player* bot)` — front phase only: MT → `CORNER_THANE`, assist tank #0 (alive-aware) → `CORNER_BARON`; MT dead → assist #1 → `CORNER_THANE`; else `CORNER_NONE`.
- Shared tank state (action + multiplier both need it; they hold separate helper instances):
```cpp
struct HorsemanTankState
{
    int corner = CORNER_NONE;
    bool arrived = false;
    uint32 baselineStacks = 0;
    bool crossing = false;
};
static std::unordered_map<ObjectGuid, HorsemanTankState> tankStates;  // defined in NaxxActions_FourHorsemen.cpp
```
`Reset()` erases this bot's entry.
- `std::pair<float,float> CornerSlotPos(int corner, int slotRank)` — anchor + lateral offset perpendicular to anchor→boss direction, ±13yd / ±26yd alternating by rank (Holy Wrath chain ~10yd; ±26 stays <45yd bait range). Rank 0 = anchor.
- `bool FindVoidZoneSafeSlot(...)` — `GetCreatureListWithEntryInGrid(list, HorsemanVoidZoneEntry, 20.0f)`; slot unsafe if zone within 7yd; ring of 8 candidate offsets (radius 6-8yd, fallback 12yd) around assigned slot; only reposition when a zone is within 6yd of bot (hysteresis).

### 3. Actions (`NaxxActions.h` + `NaxxActions_FourHorsemen.cpp`)

**3a. `HorsemanAttractAlternativelyAction`** (key `"horseman attract alternatively"` unchanged) — baiter + rear-phase crossover. Per-instance state: `int _side = CORNER_NONE; bool _arrived = false; uint32 _baseline = 0;` (action instance is per-bot; reset when `UpdateBossAI()` fails / out of combat).

Per tick:
1. Gate on `UpdateBossAI()`.
2. `_side == CORNER_NONE` → `_side = HomeCorner(bot)`.
3. My side's boss dead → `_side` = other rear corner, `_arrived = false` (converge on survivor; crossing stops naturally — no live caster, no delta).
4. Slot rank: 0 for primary baiter, else stable rank from `GetGroupSlotIndex` (Holy Wrath spread). Rear-phase melee skip anchor — go fight the boss directly.
5. Void Zone dodge first: zone within 6yd → `MoveTo` nearest safe ring slot, return true.
6. Not within 3yd of slot → `MoveTo(..., MovementPriority::MOVEMENT_COMBAT)`, return true if started.
7. First tick within tolerance: `_arrived = true; _baseline = GetMarkStacks(bot, _side);`.
8. Crossover: `stacks < _baseline` → re-baseline (aura expired while away — avoids bogus delta from 75s persistence); `stacks - _baseline >= 3` → flip `_side`, `_arrived = false`, return true. Mid-cross applications fold into next arrival baseline.
9. Target `CornerBoss(_side)`; `Attack` if current target differs.

Rear phase: `IsAttracter` true for all → zerg (home Lady) and ex-baiters (retain sides) counter-rotate on per-corner deltas — both rear corners stay covered (mandatory: punish fires every tick the nearest player is >45yd). **Never** send whole raid to one corner.

**3b. `HorsemanAttackInOrderAction`** — rename from `HorsemanAttactInOrderAction` (context key `"horseman attack in order"` untouched). Logic unchanged; front tanks no longer route here.

**3c. `HorsemanTankSwapAction`** (new, key `"horseman tank swap"`) — uses `tankStates[bot->GetGUID()]`:
1. `assignment = GetFrontTankAssignment(bot)`; bail if `CORNER_NONE`. Init `state.corner` first tick.
2. TANKING (`!state.crossing`):
   - Move to corner tank spot if >3yd (MOVEMENT_COMBAT).
   - Target corner boss; `Attack` if not current target.
   - First arrival: `arrived = true`, capture `baselineStacks`.
   - Boss's victim != me, within 30yd of boss, delta < 3 → Festergut-style class taunt (`RemoveSpellCooldown(SPELL_TAUNT_*, true)` + cast), retried every tick (covers resists). 30yd boss-proximity gate (not spot-gate): Thane/Baron chase, mid-swap boss may drift; taunting recalls him.
   - `stacks < baseline` → re-baseline; `delta >= 3` **and** other front corner has live assigned tank → `crossing = true`, `corner` = other front corner, `arrived = false`. No partner → hold (rising stacks beat loose boss).
3. CROSSING: move to new spot; within 3yd → `crossing = false`, capture baseline, fall into TANKING (taunt next tick). No `AttackStop` needed.
4. Both tanks cross on own-corner deltas (12s cadence, near-simultaneous); machine tolerates asymmetry — arriving at occupied corner just taunts, displaced tank's delta trips within ≤12s.

Define `std::unordered_map<ObjectGuid, FourhorsemanBossHelper::HorsemanTankState> FourhorsemanBossHelper::tankStates;` at top of `NaxxActions_FourHorsemen.cpp`.

### 4. Triggers

- `HorsemanAttractorsTrigger` — unchanged (`UpdateBossAI && IsAttracter`); inherits rear-phase-everyone + alive-aware.
- `HorsemanExceptAttractorsTrigger` — add `&& GetFrontTankAssignment(bot) == CORNER_NONE`.
- New `HorsemanFrontTankTrigger`: `UpdateBossAI() && GetFrontTankAssignment(bot) != CORNER_NONE`.

### 5. Registration

- `NaxxTriggerContext.h`: `creators["horseman front tank"]` + factory.
- `NaxxActionContext.h`: `creators["horseman tank swap"]` + factory; update renamed-class factory.
- `NaxxStrategy.cpp` (~line 132):
```cpp
triggers.push_back(new TriggerNode("horseman front tank",
    { NextAction("horseman tank swap", ACTION_RAID + 2) }));
```
Existing two nodes stay at `ACTION_RAID + 1`; trigger populations disjoint.

### 6. `FourhorsemanGenericMultiplier`

- Add `FourhorsemanBossHelper helper` member; gate on `helper.UpdateBossAI()` (not "sir zeliek" lookup).
- Keep `neglect threat` + zeroing `DpsAssistAction`/`TankAssistAction`.
- Add Gluth-pattern taunt suppression: `tankStates` entry present and (`crossing` or (`arrived` and delta ≥ 3)) → return 0.0f for `CastTauntAction`/`CastDarkCommandAction`/`CastHandOfReckoningAction`/`CastGrowlAction`. (Forced taunts inside the swap action bypass multipliers — direct cast.)

## Edge cases

| Case | Handling |
|---|---|
| Mid-cross mark application | Folds into arrival baseline |
| Mark expired while away (75s) | `stacks < baseline` → re-baseline |
| Baiter dies | `ignoreDeadPlayers=true` shifts indices → auto-promotion; corner uncovered only for promoted bot's travel time |
| Taunt resist/CD | Forced CD reset + per-tick retry |
| Outgoing tank taunts back | Multiplier zeroes taunt actions while delta ≥3 / crossing |
| Partner tank dead | Crossing suppressed, sole tank holds; alive-aware indices promote AT1 |
| Lady or Sir dies in rear phase | Sides collapse to survivor, crossing stops |
| Boss dragged off corner mid-swap | 30yd-to-boss taunt gate recalls; spot MoveTo re-anchors |
| Rez mid-fight | `_arrived=false` path re-anchors + re-baselines |
| Wipe/out of combat | `Reset()` clears bosses, per-instance state, `tankStates` entry |

## Verification (no build possible in this environment)

1. Greps: each new context key appears in strategy TriggerNode + context creators + class; `rg "Attact" src/Ai/Raid/Naxx` → zero hits.
2. Static map: exactly one definition (`NaxxActions_FourHorsemen.cpp`), header declares only.
3. Re-verify spell IDs against `boss_four_horsemen.cpp:26-47`.
4. `python apps/codestyle/codestyle-cpp.py` from module root; fix 4-space/Allman violations.
5. Review: no per-tick allocation in Execute paths, `ObjectGuid` map keys (not `Unit*`), difficulty only via `bot->GetRaidDifficulty()`, 10m and 25m paths both exercised in `IsAttracter`/`HomeCorner`.
6. Hand off build/in-game test to user (module can't compile headless here).

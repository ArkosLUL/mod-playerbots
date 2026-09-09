# Iron Assembly — get the last 3% of Steelbreaker

## Context

Five 25-man hard-mode pulls traced 2026-09-09, after `3f4766204` (carrier holds the boss) was built
and deployed. Same roster throughout: **one bot tank** (Bulwark, prot paladin), two human death
knights, 22 bots.

| | file (`env/dist/logs/botobs/`) | duration | deaths | phase 3 | Steelbreaker low |
|---|---|---|---|---|---|
| P1 | `603_1_stormcaller-brundir_1788977267.ndjson` | 3:42 | 30 | 16.9 s | 98.56% |
| P2 | `603_1_runemaster-molgeim_1788977683.ndjson` | 4:17 | 31 | 65.8 s | 79.39% |
| P3 | `603_1_runemaster-molgeim_1788978332.ndjson` | 4:51 | 30 | 97.1 s | **6.28%** |
| P4 | `603_1_runemaster-molgeim_1788978840.ndjson` | 4:45 | 31 | 95.3 s | **6.03%** |
| P5 | `603_1_runemaster-molgeim_1788979301.ndjson` | 5:07 | 30 | 119.2 s | **3.08%** |

(The five ~33 s files interleaved with these are post-wipe re-engage fragments, not pulls.)

**`3f4766204` worked.** Steelbreaker's phase-3 travel fell from 180 yd to 25–53, his furthest reach
from the anchor from 36.3 yd to 15.7–17.8, and the phase went from 29.7 s to 97–119 s. Melee are on
him: cast rate on the boss is 2.8–3.0/s for melee and 0.93–1.07/s for ranged, near-identical across
P3/P4/P5. Focus switching is clean — all 22 bots re-target within 0.7 s of each transition.

The raid now brings Steelbreaker from 99.5% to **3.08%** with **all 25 alive**, then loses the fight
in 20 seconds. This plan is about the last 3%.

## What the traces say

### The endgame, P5 (the boss HP curve against the death list)

```
3:07.6  phase 3 begins, 25 alive
3:51.2  Bulwark dies (Meltdown instakill)   47% -> 55.7%   +9.6%
4:28.6  Bulwark dies again                  12.5% -> 22.3%  +9.8%
4:45.7  Bulwark dies again                   3.08% -> 12.9%  +9.8%   <-- 371k short of a kill
4:48.1  Malediction                         10.9% -> 20.7%  +9.8%
4:51.1  Holylight                           16.9% -> 26.8%  +9.8%
4:57.1  Stormweaver + Druidica              21.3% -> 41.0%  +19.7%
4:59.2  Dragon                              -> 49.2%
5:00.2  five at once                        48.5% -> 99.9%  +51.4%
5:03.2  eleven at once                      wipe
```

For 98 seconds the raid loses **only the tank**, and only to the guaranteed Overwhelming Power
instakill every ~36 s. Then 22 people die in 20 seconds. Each corpse returns ~9.7% of a 12.05M bar
(~1.17M) and adds a permanent +25% to Electrical Charge, so the cascade is self-feeding.

**High Voltage is what kills them**: 72–86% of all phase-3 damage taken and 63 of 90 killing blows
across P3/P4/P5. Its median per hit tracks the corpse count — P5 went 2910 → 3820 → 4400 → 6111 →
8819 → **28717** as deaths stacked. Meltdown is 10 of 90 blows and 1.6–6.1% of damage; Fusion Punch
3, Static Disruption 1. There is no positional answer to High Voltage: it is a 50,000 yd pulse. The
only levers are damage into the phase and not dying.

### 1. The ranged ring overlaps the Meltdown blast, and the tank oscillates

**Geometry.** `DeriveIronAssemblyRaidSpot` builds the spread ring on the **stack point**
(`UldEncounter_IronAssembly.cpp:783`), which is `anchor + 10 yd @ 180°` = `(1577.18, 121.02)`, with
`SPREAD_RING_RADIUS = 18` over 16 slots. The Steelbreaker tank spot is `anchor + 16 yd @ 135°` =
`(1575.87, 132.33)` — only **11.4 yd from that ring centre**. So the ring's near arc runs 6.6 yd
from the carrier, and slot distances to the tank spot are:

```
slot bearing   45°    67.5°   90°    112.5°  135°   157.5°  22.5°
distance      14.1    9.8     6.8     7.7    11.5    16.0    18.5
```

**Five of sixteen slots sit inside Meltdown's 15 yd**, permanently. Traced: Fel, Elemena, Hellflame,
Prayer and Agony are caught in every blast, at 5–14 yd. Per blast the hit list is 5–16 players
(P5 at 3:51 caught 16, including 4 healers). The carrier always shows `0 dmg, hp_after 0.0%` — that
is the INSTAKILL, and it is unavoidable. Everyone else took 3.6k–23.5k and survived at 71–100%, so
the blast is not itself lethal; it is chip damage that arrives on top of a High Voltage pulse and
pushes the cascade over.

**Oscillation.** `tank face` (`TankFaceAction`, `MovementActions.h:167`, pushed by `CombatStrategy.cpp:80`)
and `iron assembly tank assignment action` both issue moves at `MOVEMENT_COMBAT` and alternate
destinations ~6.8 yd apart, every second:

```
3:26.7 (1582.55,134.45)   3:27.7 (1575.87,132.33)   3:28.7 (1581.95,134.26)
3:29.7 (1575.87,132.33)   3:30.8 (1582.08,134.30)   3:31.8 (1576.02,132.39)   ...
```

Phase-3 tank moves: P3 99 (28 `tank face` / 71 assignment), P4 84 (25/59), P5 116 (37/77) — 45–57
destination flips over 3 yd and 385–515 yd of destination wander. It happens in phases 1–2 too
(P4: 29 `tank face` moves before phase 3). Beyond the wasted movement it drags the Meltdown centre
6.8 yd toward the raid, which is what pulls the extra ranged into the blast.

### 2. Offensive cooldowns and potions are spent on Brundir

`IronAssemblyHoldDpsCooldownsMultiplier` (`UldMultipliers_IronAssembly.cpp:103`) identifies burst via
`IsDpsCooldownAction(bot, action)` (`EncounterHelpers.cpp:520`), a `dynamic_cast` chain. It misses:

| Escapes | Why |
|---|---|
| `offensive potion` | `UseOffensivePotion` is a `UseItemAction`; the chain only casts to `UseTrinketAction` |
| `use tinker` | `UseTinkerAction` is never cast to |
| `shadowfiend`, `power infusion` | `default: break; // Priest =(` at `EncounterHelpers.cpp:596` |
| `killing machine` | on the name registry, absent from the cast list |
| `blood fury`, `berserking` | **the race switch is wrong** — see below |

Measured in P5 before phase 3 (i.e. spent on Brundir and Molgeim): **69 `use tinker`** (22 bots × 3),
**17 `offensive potion`** (one each — the whole raid's potion allowance), 14 `blood fury`,
2 `berserking`, 2 `shadowfiend`, 1 `power infusion`. Same in P3 and P4. Verified on a single tick:
at `0:05.36` Assasin's `adrenaline rush`, `blade flurry` and `killing spree` were vetoed by
`iron assembly hold dps cooldowns` while his `offensive potion` fired `OK`, and `blood fury` fired
`OK` 0.4 s later. The multiplier is live and correct; the predicate is the hole.

**The racial leak is a real defect, not a config quirk.** `RacialsStrategy.cpp:87` arms on
`botAI->HasSpell("blood fury")`, but `IsDpsCooldownAction` gates on `bot->getRace() == RACE_ORC`.
`acore_characters.characters` says the seven bots casting Blood Fury are races 1/3/4/11
(Human, Dwarf, Night Elf, Draenei) and the one casting Berserking is race 4 — **all Alliance**. These
bots have the spell without the race, so the race switch can never match. Keying on the action name
fixes it; keying on race cannot.

`UlduarBurstWindowMultiplier` does not catch the overflow either: its Iron Assembly branch returns
`allowAll = true` for the whole encounter (`UldMultipliers_Shared.cpp:162-169`).

### 3. Pets attack the wrong boss

Iron Assembly has no pet handling at all — grep for `pet` across its six files returns nothing. The
generic `PetAttackAction` is dead code (`NextAction("pet attack", ...)` has zero matches repo-wide;
it is reachable only from the `.pet attack` chat command), and `AttackAction::Attack` has its pet
re-target block commented out (`AttackAction.cpp:219-239`). So a pet keeps whatever `PetAI` first
latched onto and never follows its owner's focus.

P5: **124 of 958 pet casts (13%) hit a boss the raid was not focusing.** Two pets account for 122 of
them — `Worm` (87) and `Flaaghun` (35) both sat on Steelbreaker for the entire Molgeim phase. In hard
mode that damage is not merely misdirected, it is **discarded**: Molgeim's death restores Steelbreaker
to full.

### Checked and not a defect

- **Melee uptime is fine.** An 8 yd proximity proxy reported a 2% median in P4, but those melee were
  parked 9.3 yd out and had stopped issuing move orders — inside melee range of a large boss model.
  Cast-rate on the boss is the honest measure and it is flat across pulls (melee 2.8–3.0/s).
- **Static Disruption spread works.** 1–2 players caught per blast in every phase-3 pull, at the
  current 7.0 yd slot spacing.
- **Focus and the kill order are correct.** All 22 bots switch within 0.7 s of each transition, and
  `ironassembly.alive` emits one row per transition.

## Approach

### 1. Re-centre the phase-3 ranged ring on the boss, and stop the oscillation

**1a. Ring.** In the empowered phase only, centre the spread ring on the **Steelbreaker tank spot**
instead of the stack point, at radius **22**. Every slot then sits 22 yd from the carrier — 7 yd
clear of Meltdown — and slot spacing rises from 7.0 to 8.6 yd, which also helps Static Disruption.
Melee, the boss and the tank spot itself do not move, so there is no tow at the transition.

The centre must be the **derived** tank spot, not the tank's or the boss's live position, or the ring
chases a moving point and never settles — the lesson already paid for by the Rune of Power drag-out
and the hazard-escape stations. Extract the boss → (bearing, radius) mapping plus rune shift out of
`TryGetIronAssemblyTankSpot` (`UldEncounter_IronAssembly.cpp:524`) into a helper that takes an NPC
entry rather than a bot, so the tank and the ring read one source of truth. Fall back to today's
stack-centred ring if it cannot be derived.

**Radius 22, not the 24 discussed.** navprobe, bot filter `--nav 0x09`, ring centred on
`(1575.87, 132.33, 427.27)`, 16 headings:

```
r=21  16/16 settle 427.267-427.287
r=22  16/16 settle on the floor
r=23  16/16 settle on the floor
r=24  135° slot (1558.899, 149.301) settles to Z -438.107   <-- hole in the floor
```

Note the trailing `16/16 on mesh` line prints for r=24 as well; the `settledZ` column is what
disqualifies it, exactly as `docs/engine/pitfalls.md:120` warns. 22 keeps two yards of buffer from
that failure. Re-probed at every rune-shifted tank spot (bearings 67.5° through 202.5° at 16 yd):
21, 22 and 23 are clean at all five centres. Use 23 only if the extra yard is wanted.

Melee stay inside the blast — they are on the boss, and that is inherent, not a bug. Traced Meltdowns
hit melee for 3.6–6.5k with everyone surviving at 84–95%.

**1b. Oscillation.** Add `IronAssemblyDisableTankFaceMultiplier`, following the existing
`ZuljinDisableTankFaceMultiplier` (`ZAMultipliers.cpp:322-336`): return `0.0f` for
`dynamic_cast<TankFaceAction*>(action)` when the bot holds an Iron Assembly tank assignment and the
formation is active. The tank spot is authoritative for where a tank stands here; facing is already
handled separately by `set facing`. Cover the whole encounter, not just phase 3.

### 2. Close the burst hole

In `IronAssemblyHoldDpsCooldownsMultiplier::GetValue`, widen the predicate to the union of the two
that exist:

```cpp
bool const burst = IsDpsCooldownAction(bot, action) ||
                   (PlayerbotAI::IsDps(bot) && IsBurstCooldownAction(action->getName()));
```

`IsBurstCooldownAction` (`BurstCooldowns.cpp:49`) is the documented single registry
(`docs/systems/consumables-and-burst.md:13-20`) and already lists `offensive potion`, `use tinker`,
`blood fury`, `berserking`, `killing machine`, `shadowfiend` and `power infusion`. The `IsDps` guard
on that branch preserves today's deliberate exclusion of healers and tanks — a healer's Shadowfiend
is a mana cooldown and must not be held for three minutes. Keeping `IsDpsCooldownAction` in the union
retains the nine cooldowns that are on the cast list but not the registry (`bladestorm`, `cold blood`,
`cold snap`, `starfall`, `kill command`, `feral spirit`, `deathchill`, `empower rune weapon`,
`arcane torrent`), so nothing regresses.

Do **not** extend `burstCooldownNames` itself in this change — that registry is shared with Naxx, OS,
XT002 and the global tank-hold, and widening it there is a separate decision with its own
verification. Note it as a follow-up.

### 3. Point pets at the focus

Add `CommandPetAttack(botAI, focus);` to `IronAssemblySetDpsPriorityAction::Execute`
(`UldActions_IronAssembly.cpp:319`) **above** the `current target == focus` early return at line 327 —
below it the call is unreachable in the steady state. This is exactly what `FreyaSetDpsPriorityAction`
does (`UldActions_Freya.cpp:325-343`), including the comment explaining why it must run every tick.
`EncounterHelpers.h` is already included and `using namespace EncounterHelpers;` is already in scope;
`CommandPetAttack` (`EncounterHelpers.cpp:474`) no-ops when the pet is already on target, respects
`REACT_PASSIVE`, and covers hunter pets, warlock demons, ghouls and Feral Spirits via
`GetGuardianPet()`.

Known gap to leave alone: `IronAssemblySetDpsPriorityTrigger::IsActive` bails for tanks
(`UldTriggers_IronAssembly.cpp:156`), so a tank's ghoul is unaffected. Hunters and warlocks are never
tanks, so this covers the reported symptom. A dedicated pet node (the Razorscale/Mimiron pattern)
would close it, at the cost of six more wiring sites — not worth it until a trace shows a tank pet
costing something.

## Files

| Path | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.h` | Add `ULDUAR_IRON_ASSEMBLY_EMPOWERED_SPREAD_RING_RADIUS = 22.0f`; declare the entry-keyed tank-spot helper |
| `src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp` | Extract the tank-spot derivation (`:524`); centre the empowered ring on it in `DeriveIronAssemblyRaidSpot` (`:744`) |
| `src/Ai/Raid/Uld/Multiplier/UldMultipliers_IronAssembly.{h,cpp}` | Add `IronAssemblyDisableTankFaceMultiplier`; widen the burst predicate (`:103`) |
| `src/Ai/Raid/Uld/Action/UldActions_IronAssembly.cpp` | `CommandPetAttack` in `IronAssemblySetDpsPriorityAction::Execute` (`:319`) |
| `src/Ai/Raid/Uld/UldStrategy.cpp` | Register the new multiplier in `InitMultipliers` (near `:896`) |
| `docs/raids/ulduar/iron-assembly.md` | Findings below, via `/compact-docs-writer` |

Do not touch `burstCooldownNames`, `IsDpsCooldownAction`, `TankHasHeldBoss`, `BurstWindowStrategy`,
`MELEE_BOSS_RADIUS`, the tank-spot bearings, the non-empowered stack/ring, or any other encounter.

### Doc

`docs/raids/ulduar/iron-assembly.md` is reachable from the module `CLAUDE.md`, so invoke
`/compact-docs-writer` **up front**. Content: the phase-3 arithmetic (raid reaches 3% with 25 alive,
then 22 die in 20 s; High Voltage is 72–86% of damage and 63 of 90 blows and scales with the corpse
count, 2.9k → 28.7k per hit); the ring/Meltdown geometry with the 5-of-16 slot overlap and the
navprobe table including the r=24 hole; that `tank face` fights the tank spot and is vetoed; that the
burst hold keys on the action name because racials here exist without their race; and that pets
follow the focus. Replace the "Overwhelming Power ... the node walks them clear of the raid instead"
sentence, which `3f4766204` already made wrong.

## Verification

Static, here: `python apps/codestyle/codestyle-cpp.py`. **The module cannot be compiled in this
environment (no `compile_commands.json`, both build volumes empty) — hand the build off rather than
claiming one.**

In-game, one 25-man hard-mode pull, then `tools/botobs/postmortem.py`:

1. `--notes ironassembly.` — the Meltdown circle holds one position for the whole 35 s aura instead
   of alternating between two points 6.8 yd apart, and `tank face` no longer appears in the tank's
   move stream at all (was 25–37 moves per phase 3, 29 before it).
2. At each Meltdown, players within 15 yd of the carrier are melee and the tank only. Ranged and
   healers caught should go from 5 per blast to zero; total caught from 5–16 to the melee count.
3. Before phase 3, `offensive potion` fires 0 times (was 17), `use tinker` 0 (was 69), `blood fury`
   and `berserking` 0 (was 14 and 2), `shadowfiend` and `power infusion` 0 (was 2 and 1). All of them
   appear inside phase 3 instead. `iron assembly hold dps cooldowns` should now show vetoes against
   those names, which it never did.
4. Pet casts on a boss other than the current `ironassembly.focus` fall from 124/958 to ~0. Watch
   `Worm` and `Flaaghun` specifically.
5. Steelbreaker's low-water mark beats 3.08%. This is the metric that decides the plan: the raid was
   371k short on a 12.05M bar, and 17 potions plus 69 tinkers plus 13% of pet damage moved into the
   phase should cover it.
6. Deaths in phase 3 before the cascade stay at the one-per-36 s Overwhelming Power floor. If ranged
   and healers still die before the tank's third death, the Meltdown fix did not take.

If the boss still survives with the ring clear, no burst wasted and pets on target, the phase is a
throughput or healing problem rather than a behavioural one, and the next lever is the first
avoidable death — each one hands back ~1.17M.

## Step 0

Copy this document to
`docs/plans/iron-assembly-meltdown-ring-and-burst/iron-assembly-meltdown-ring-and-burst.PLAN.md`
before starting.

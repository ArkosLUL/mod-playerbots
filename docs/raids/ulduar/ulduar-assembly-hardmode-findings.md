# Ulduar Assembly of Iron Hard-Mode — Server-Truth Findings

Phase 2 of [ulduar-hard-mode-plan.md](ulduar-hard-mode-plan.md). Vezax (Phase 1) is
done + config-gated. Hodir was next in the table but its hard mode is a 3-min DPS race
gated on `boss_hodir GetData(3)` (`bAchievCacheRare`) and the plan flags it as
**dependent on the Sev-1 Flash-Freeze / Biting-Cold fixes landing first** — those are
not in yet. So Assembly of Iron is the next self-contained boss.

## What the hard mode is

Assembly of Iron = 3 council bosses (Steelbreaker `NPC_STEELBREAKER`, Runemaster
Molgeim `NPC_MOLGEIM`, Stormcaller Brundir `NPC_BRUNDIR`). Each survivor gains a phase
every time a council member dies: on death the dying boss casts `SPELL_SUPERCHARGE`
(61920) on the others, and each `SpellHit(SUPERCHARGE)` calls `UpdatePhase()`
(`boss_assembly_of_iron.cpp:317-321`, `487-491`, `688-692`). So the **last one alive
reaches `_phase == 3`** and unlocks its empowered kit.

The "hard mode" the raid opts into is the **kill order**: leaving **Steelbreaker last**
so he hits phase 3 (his empowered kit is the deadliest, and it's the achievement /
best-loot path). The raid picks the order; bots follow.

## Detection (server truth) — no clean GetData flag

Steelbreaker's `_phase` is a private `uint8`; `boss_steelbreaker` has **no `GetData`
override** (only Brundir exposes `GetData(DATA_BRUNDIR)` for a stun achievement). So we
cannot read the empowered flag directly. Infer it instead:

```
Steelbreaker empowered  ==  Steelbreaker alive
                            AND Molgeim NOT alive
                            AND Brundir NOT alive
```

Both others dead → Steelbreaker took 2 Supercharges → `_phase == 3`. Clean, live, no
server coupling beyond `GetFirstAliveUnitByEntry` for each of the 3 entries (same helper
Vezax uses, `RaidBossHelpers.h:21`).

`NPC_STEELBREAKER`, `NPC_MOLGEIM`, `NPC_BRUNDIR` enums live in `ulduar.h` (already used
by the server). Need to mirror the three entry ids into `UldBossHelper.h` (Vezax added
`NPC_VEZAX_SARONITE_ANIMUS` the same way).

## Steelbreaker phase-3 empowered kit (what bots must survive)

| Spell | id | Phase | Behaviour | Bot delta |
|---|---|---|---|---|
| Fusion Punch | 61903 | 1 | Melee hit on victim + heavy Nature DoT that must be out-healed / dispelled | **Tank swap** off the DoT-stacked tank, OR just heal through — see Q3 |
| Static Disruption | 61911 | 2 | Random non-melee target, Nature dmg + ~5y splash | Ranged **spread** (server already prefers a target >10y out) |
| Overwhelming Power | 64637 | 3 | On current victim (tank): buff that **instakills the tank when it expires** unless dispelled/removed | Highest-priority **dispel** (Magic) before expiry; or accept tank death |
| Electrical Charge | 61902 | 3 | Steelbreaker gains a stacking damage nova aura each time a **player dies** (and via Meltdown from Rune-of-Summoning elementals) | Melee **back off** while the nova pulses at high stacks |

Phase-1/2 abilities are already live the whole fight — the phase-3 additions are
Overwhelming Power (the dispel/tank-death mechanic) and Electrical Charge (death-fueled
nova). Meltdown (`spell_assembly_meltdown`) feeds a charge when a summoned Lightning
Elemental is killed (`boss_assembly_of_iron.cpp:865-869`).

## Proposed bot deltas (mirrors Vezax structure)

Config switch `AiPlayerbot.UlduarAssemblyHardMode` (default off), gate detector
`IsAssemblyHardModeActive(botAI)` in `UldHardMode.{h,cpp}` on it (same pattern as
[ulduar-vezax-hardmode-config-plan.md](ulduar-vezax-hardmode-config-plan.md)).

Triggers/actions in `Trigger/UldTriggers_Assembly.{h,cpp}` +
`Action/UldActions_Assembly.{h,cpp}`, registered in `UldTriggerContext.h` /
`UldActionContext.h`, wired in `UldStrategy.cpp`. (These Assembly files don't exist yet —
Assembly currently has no per-boss strategy file at all; confirm one should be created.)

## IMPLEMENTED (decisions locked)

User picked: full empowered kit, enforce Steelbreaker-last via RTI, extend files (the
`IronAssembly` trigger/action pair **already existed** — no new files created).

- Config `AiPlayerbot.UlduarIronAssemblyHardMode` (default 0), gated in
  `IsIronAssemblyHardModeActive` / `IsSteelbreakerEmpowered` (`UldHardMode.cpp`).
- `iron assembly kill order trigger/action` — main tank skull-marks
  Brundir → Molgeim → Steelbreaker (`GetIronAssemblyNextKillTarget`), `ACTION_RAID`.
- `iron assembly fusion punch swap trigger/action` — `ACTION_RAID + 2`, mirrors
  `ThorimUnbalancingStrikeSwap`: off-tank taunts the empowered Steelbreaker off a tank
  carrying Fusion Punch (61903) **or** Overwhelming Power (64637).
- **Electrical Charge (61902) deliberately NOT handled positionally.** It is a stacking
  damage buff Steelbreaker gains on each player death, not an avoidable ground effect —
  a healer/DPS-uptime problem, not a movement one. Meltdown is moot in the empowered
  phase (Molgeim, who summons the elementals, is dead by then).
- Enums added to `UldBossHelper.h`: `NPC_STEELBREAKER/MOLGEIM/BRUNDIR`,
  `SPELL_FUSION_PUNCH`, `SPELL_OVERWHELMING_POWER`.

## Open questions (resolved above)

1. **Scope.** Do you want the full empowered kit (dispel Overwhelming Power + melee
   back-off on Electrical Charge + tank-swap on Fusion Punch), or the minimum viable
   slice first (just the one that most changes survival)? My read: **Overwhelming Power
   dispel** is the single mechanic that wipes an unprepared group — I'd start there.

2. **Kill order / focus.** Should hard-mode bots actively enforce "Steelbreaker last"
   by RTI-marking Brundir→Molgeim as skull kill targets, or stay pure follower (attack
   whatever the raid marks) and only handle the empowered survival mechanics? Follower
   model in the plan argues for the latter.

3. **Tank swap.** Bot tank-swap machinery — does the raid framework expose a clean
   "swap to off-tank" for Fusion Punch/Overwhelming Power, or do we only have
   `GetGroupAssistTank` / `SetRtiTarget`? If no real swap primitive, I'd model Fusion
   Punch as "heal through" and only act on Overwhelming Power via dispel.

4. **Assembly strategy file.** Confirm creating new `UldTriggers_Assembly` /
   `UldActions_Assembly` pair (Assembly has none today) vs. folding into an existing
   file.

## Verification (per plan)
- `python apps/codestyle/codestyle-cpp.py` before done.
- In-game: `UlduarAssemblyHardMode = 0` → bots ignore empowered kit; `= 1` + kill the
  other two first → bots dispel Overwhelming Power / handle Electrical Charge.

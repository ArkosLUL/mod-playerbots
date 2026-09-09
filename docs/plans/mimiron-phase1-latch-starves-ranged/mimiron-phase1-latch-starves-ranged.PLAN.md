# Mimiron: ranged and healers were disabled for the whole pull by a stale latch

## Context

Two Firefighter attempts on the 09th (`603_1_mimiron_1788980436`, `603_1_mimiron_1788980619`) both
wiped **in phase 1** at 1:16 and 1:05. VX-001 never spawned in either. The user's report — "ranged
bots don't cast spells, they only move" — is exact, and the cause is a bug I shipped in `ab43383d3`
yesterday.

That commit changed `MimironPhase1PositioningAction` to set `disperse distance` from `6.0f` to
`ULDUAR_MIMIRON_DISPERSE_DISTANCE` (5.5). It did **not** change the trigger that gates it, which
still ends:

```cpp
// UldTriggers_Mimiron.cpp:99
return AI_VALUE(float, "disperse distance") != 6.0f;
```

`5.5 != 6.0` forever, so the latch never closes. `MimironPhase1PositioningAction::Execute` returns
`true` at `ACTION_RAID` (60), and `Engine::DoNextAction` **breaks the pass on the first action
returning true** (`src/Bot/Engine/Engine.cpp:262`). So for every bot the trigger accepts, that node
won every tick of phase 1 and nothing below relevance 60 ever ran — no DPS, no heals, no
`set facing`, no `combat formation move`.

`MimironPhase1PositioningTrigger::IsActive` opens with `if (!botAI->IsRanged(bot)) return false;`,
and `IsRanged` is true for healers. That is exactly who broke.

## Evidence

Yesterday's two pulls (`y1`/`y2`, 08th) against today's (`d1`/`d2`), combat only:

| | y1 | y2 | **d1** | **d2** |
|---|---|---|---|---|
| ranged real casts | – | 808 (0.30/bot/s) | – | **8 (0.01/bot/s)** |
| ranged `%cast` (snapshots mid-cast) | 43.3% | 39.6% | **0.0%** | **0.4%** |
| ranged dps/bot | 3,556 | 3,487 | **348** | **286** |
| healer casts (4 bots) | – | 438 | – | **11** |
| effective HPS | 11,903 | 9,455 | **1,302** | **1,422** |
| incoming/s | 12,834 | 10,938 | 8,071 | 6,855 |
| melee casts | – | 0.30/bot/s | – | **0.45/bot/s** |
| phase 2 reached | yes | yes | **no** | **no** |

Corroborating, all from the traces:

- **Mana never moves.** Power (mage) starts 99.8%, min 99.8%, ends 100.0%. Every ranged bot the same.
- **The remaining ranged "damage" is pets.** `snap.dealt` credits a pet to its owner, and the only
  ranged bots with output are the two hunters and three warlocks. Mages, priest, druid and shaman
  dealt **exactly 0** across a full 65 s while alive the whole time.
- **The action stream lost every DPS node.** Yesterday ranged had `reach spell` 41, `wrath` 35,
  `explosive shot` 28, `steady shot` 25, `mind flay` 21, `starfire` 20, `set facing` 144 as OK
  verdicts. Today: none of them appear at all, and `mimiron phase 1 positioning action` — absent from
  yesterday's list entirely — is OK 46 times. Power's whole 65 s is 61 `act` rows containing no DPS
  action in any verdict, not even FAILED.
- **No multiplier is responsible.** The veto stream is clean: `mimiron formation guard` fired once
  all pull.
- **Second-order kill.** `combat formation move` sits below `ACTION_RAID`, so the unstacker was
  starved too. Ranged+healer nearest-neighbour over phase 1 went 5.5 yd median / 45% inside 5 yd
  (y2) to **4.8 yd / 59% inside 5 yd** (d2), and Napalm Shell (5 yd splash) killed Prayer, Elemena,
  Trueshot and Nightwarrior between 0:19 and 0:21. The death blocks show 5-6 bots inside 4 yd.

## Second bug, found on the re-read

`MimironChargeGuardMultiplier` (`UldMultipliers_Mimiron.cpp:81`) was widened to
`dynamic_cast<ReachTargetAction*>(action)`. That base class is not just `reach melee` — it also
covers **`reach spell`**, **`reach party member to heal`**, `reach party member to resurrect` and
`reach pull`. So during any `MimironLethalWindowActive` window under Firefighter — which now includes
the flames dodge firing at under 60% health or on two nodes — healers are blocked from walking into
heal range and ranged from walking into spell range.

The intent was only `reach melee` (F2). The rest of this file already matches by name for exactly
this reason (`MimironFormationGuardMultiplier`, `MimironThreatRedirectGuardMultiplier`).

Not yet observed in a trace, because the tick starvation hid it — but it targets healing during the
windows where healing decides the pull, and phase 2 was already a losing healing race.

## The other two changes from `ab43383d3`

Re-read against the engine semantics above; both are inert in phase 1 and structurally sound:

- `MimironRapidBurstTrigger::IsActive` opens on `GetFirstAliveUnitByEntry(botAI, NPC_VX001)`, and the
  arc-spread yield is inside `if (Unit* vx001 = ...)`. Neither can fire before phase 2. Consistent
  with the traces: no `rapidburst` note anywhere.
- The `hmlap` branch sits behind `p4tank` and ahead of `stagemelee`, gated on
  `staging && IsMimironHardModeActive`. Normal mode still reaches `stagemelee`/`stagering`.

Both remain **completely unverified** — no pull has reached the code.

## Fix

**1. Close the latch** — `src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.cpp:99`

```cpp
return AI_VALUE(float, "disperse distance") != ULDUAR_MIMIRON_DISPERSE_DISTANCE;
```

The file already includes `UldEncounter_Mimiron.h` and uses 12 other `ULDUAR_MIMIRON_` constants, so
this is a drop-in. Both sides now read one symbol and cannot drift again.

**2. Stop bookkeeping claiming the tick** — `src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp:280-284`

`MimironPhase1PositioningAction::Execute` returns `false` after the `SET_AI_VALUE`. This is the
pattern its own neighbour eight lines above already uses — `MimironResetEncounterStateAction`
returns false with the comment *"Never claims the tick: clearing state is bookkeeping, and whatever
the bot is standing in still needs the nodes below this one to run."* With this in place, a future
constant drift costs one wasted trigger evaluation instead of the entire fight.

**3. Same hardening on the phase-4 sibling** — `UldActions_Mimiron.cpp:534-537`

The non-tank tail of `MimironPhase4FocusAction::Execute` sets `disperse distance` to `4.0f` and
returns `true`. Its latch is currently self-consistent so it is not broken, but the shape is
identical, and returning true there actively blocks the `combat formation move` that the disperse
value exists to feed. Return `false`. The tank branch above it must keep returning `true`.

**4. Narrow the charge guard** — `UldMultipliers_Mimiron.cpp:78-82`

Keep the `CastReachTargetSpellAction` branch (Charge, Intercept, both Feral Charges). Replace the
`ReachTargetAction` dynamic_cast with a name test for `"reach melee"`, and correct the comment, which
currently states the `reach spell` capture as if it were intended.

**5. Docs** — invoke `/compact-docs-writer` up front, per the governing-docs rule:

- `docs/engine/action-selection.md` — the selection loop already says "break on the first action
  returning `true`". Add the two rules that follow from it and that this bug broke: an action that
  only writes state must return `false`; a latch trigger must test the same constant its action
  writes.
- `docs/raids/ulduar/mimiron.md` — the disperse-distance section (around line 482-497) describes the
  5.5 change as landed and working. Correct it: the change needed the trigger's literal too, and
  without it phase 1 ran with no ranged DPS and no healing.

## Verification

Per-TU `-fsyntax-only` against the build image's `compile_commands.json` on the three changed TUs
(`UldTriggers_Mimiron.cpp`, `UldActions_Mimiron.cpp`, `UldMultipliers_Mimiron.cpp`), then the
worldserver Docker rebuild — the only compile path — then one Firefighter pull.

**This fix, read first. All four are pass/fail, not judgement calls:**

1. Ranged `%cast` back near 40% from 0.0%, and ranged dps/bot back near 3,500 from 286.
2. Mages, priest, druid and shaman deal non-zero damage.
3. Effective HPS back near 9,500 from 1,422.
4. Phase 2 reached — VX-001 present in the trace at all.

If any of those still fails, `reach spell` / `reach party member to heal` being vetoed is the next
suspect; check the veto stream for `mimiron charge guard`.

**Only once the above passes** are the four changes from `ab43383d3` testable for the first time.
Nothing from today's traces says anything about them:

- **F1 oscillation.** `combat formation move` accepted for ranged near 51, not 198-261. Ranged
  fraction-moving toward 26% from 42%, reversals toward 56 from 180-184. Phase 1 raid DPS over
  110,000 and under 95 s. Ranged nearest-neighbour inside 5 yd must **not** exceed 30-31% — that is
  the Napalm Shell check on the 5.5 threshold.
- **F2 melee.** Melee time within 5 yd of the Mk II over 23%, from 11%. `flames ok` overtaking
  `flames fallback`. Dodge hop below 11.9 yd.
- **F3 Rapid Burst.** `rapidburst ok` rows appear. Rapid Burst per living bot per second below
  590/598. Phase-4 tank does **not** step.
- **F4 handover.** `hmlap` in `mimiron.slot` across the whole handover, `follow` near zero against
  368.

Also: normal-mode pass (the disperse edit and F3 both land there), and the cheat grep on the Mimiron
files stays clean.

## Files

- `src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.cpp` — the latch constant
- `src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp` — two bookkeeping returns
- `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Mimiron.cpp` — narrow the charge guard
- `docs/engine/action-selection.md`, `docs/raids/ulduar/mimiron.md`

Another session has been editing Thorim files in this tree; stage only Mimiron and doc work.

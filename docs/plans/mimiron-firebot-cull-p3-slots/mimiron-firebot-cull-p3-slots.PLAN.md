# Mimiron 2026-09-12: the phase 4 rendezvous misses its 15 s window, the fire bot cull outranks the
# boss, the tank kills the protected pair, and phase 3 gives up on its slots

## Context

Two Mimiron pulls today, both on the worldserver built 2026-09-12 11:57:38, which carries
`eb9db6855` (phase 1 camp sight, Plasma Blast order, fire detour) but not the later perf commits.
Traces in `ac-worldserver:/azerothcore/env/dist/logs/botobs/`:

- **A** = `603_1_mimiron_1789218273.ndjson` — **Firefighter**, wipe at 10:04.277, 31 deaths.
- **B** = `603_1_mimiron_1789219132.ndjson` — **normal mode**, kill at 9:21.424, 7 deaths.

B is not hard mode: no Emergency Mode 64582, no Flames, no Frost Bomb, no Emergency Fire Bots. The
bots still ran hard mode logic (`hmwedge` slots, 186 `mimiron avoid aoe guard` vetoes), so
`ulduarMimironHardMode` was on and the Big Red Button was not pressed. **A is the only Firefighter
attempt today**, and every finding below is from A unless it says otherwise.

Reader `tools/botobs/obstrace.py`; scratch scripts in the session scratchpad (`mm.py` loader,
`dps.py`, `targets.py`, `firebot.py`, `kept.py`, `deaths.py`, `pace.py`, `osc.py`, `slots.py`,
`final.py`).

The user asked four questions: why bots died, DPS in phases 2-4, why bots focused Emergency Fire
Bots in phase 3 with the ACU alive, and oscillations/positioning. The findings answer those; the
changes address what they turned up.

## Findings

### 1. A was a berserk wipe, not a mechanics wipe

Berserk fired at **10:00.175** on all three constructs; the raid wiped 4 s later with **2,327,568 hp
left** (MK II 8.8%, VX-001 9.3%, ACU 14.6%). At phase 4's on-boss rate that is **~30 s short**.

Deaths: 1 in phase 1, 4 in phase 2, **0 in phase 3**, 26 in phase 4 — and **20 of those in the last
25 s**, after berserk. Before berserk the raid was 20 up. So the wipe is a pace failure; the deaths
are a symptom.

| death | when | cause |
|---|---|---|
| Assasin | 0:38.2 | Shock Blast 89,433, killed mid-leg of its own `mimiron shock blast action` (`[STILL WALKING]`) |
| Power, Angry | 4:12.3 | Flames, after standing in it (Angry took 63,757 over 23 hits) |
| Elemena, Assasin | 4:23.4 | VX-001 Heat Wave + Rapid Burst |
| 6 more | 8:16-9:24 | Proximity Mine x2, VX-001 x2, Flames, ACU |
| 20 | 9:39-10:04 | berserk; 9 of them to one Laser Barrage at 10:01-10:02 |

Flames did **2.22M, 23% of all damage the raid took**, across 762 hits. In phase 3 it was 71.6% of
everything taken, and phase 3 still killed nobody.

B's 7 deaths were all phase 4, almost all VX-001 Hand Pulse. Nothing to chase.

### 2. DPS, phases 2-4 (engagement windows, not phase-note windows)

"on it" is damage into that window's own construct; the rest went to adds.

| window | A (Firefighter) | B (normal) |
|---|---|---|
| MK II | 0:17.5-1:41.5, **84 s**, 122.2k raid / 116.2k on it | 0:11.0-1:11.9, 61 s, 134.8k / 132.0k |
| VX-001 | 2:33.8-4:22.2, **108 s**, 94.5k / 94.5k | 2:03.7-3:13.7, 70 s, 117.1k / 117.1k |
| ACU | 4:52.7-7:10.5, **138 s**, 78.8k / **42.9k** | 3:44.5-4:53.7, 69 s, 115.9k / 64.5k |
| phase 4 | 7:44.0-10:04.3, 140 s, 78.8k / 77.9k | 5:26.0-9:21.4, 235 s, 96.7k / 93.8k |

The scripted transitions are **identical in both pulls** — 52 s, 31 s, 32-34 s, 117 s total with
nothing attackable — and the raid re-engaged 3-5 s after each construct became attackable. Handover
dead air is encounter cost, not a bot problem. All the recoverable time is inside the damage phases,
and phase 3 is where it is: **only 54% of output reached the ACU**.

Phase 3 output split (A): ACU 54%, Assault Bot 23%, Junk Bot 8%, **Fire Bot 7%**, Bomb Bot 3%.
Phase 3 ranged bot-seconds (A): ACU 32%, **Fire Bot 21%**, Assault Bot 14%, Junk Bot 12%, Bomb Bot
7%, no target 14%. Ranged spent two thirds as much time on fire bots as on the boss.

### 3. The fire bot focus: 83% by design, 17% the tank

Six fire bots spawned, in two waves of three (guids ...4784/4785/4786, ...4863/4864/4865). **All six
were killed**, with the ACU between 93.7% and 45.8% — far above the 15% cleanup threshold. At
**4:56, eight seconds into phase 3 with the ACU at 97.9%, all 21 non-tank bots switched their
`mimiron.dpsrule` to `firebot` at once.**

That much is intended. `ULDUAR_MIMIRON_FIREBOT_KEEP = 2` keeps the two oldest and culls the rest
(`UldEncounter_Mimiron.h:550-556`), and in hard mode `BuildPriorityList` ranks a non-kept fire bot
**second for ranged, behind only the Bomb Bot and ahead of the grounded ACU, the Assault Bot, the
Junk Bot and the mech tail** (`UldActions_Mimiron.cpp:992-995`). So the whole ranged group drops the
boss the moment a third fire bot exists. Cost: **769,961 damage over 361 bot-seconds**, about **18 s
of ACU time** on a pull that missed by ~30 s.

The other 17% is a real bug. Splitting the damage by attacker:

| fire bot | damage | who |
|---|---|---|
| 4786, 4864, 4865 | 368k, 191k, 161k | the ranged group — the intended cull |
| **4785** | **32,731** | **Bulwark (tank), alone, 5:06-5:34** |
| **4784** | **17,824** | **Bulwark (tank), alone, 5:57-6:12** |

4784 and 4785 were the **protected pair**. The tank killed both. Bulwark spent **34% of phase 3**
targeting fire bots.

Why nothing stopped it: `MimironSetDpsPriorityTrigger::IsActive` opens with
`if (botAI->IsTank(bot)) return false;` (`UldTriggers_Mimiron.cpp:474`), and
`MimironTargetGuardMultiplier` only zeroes `DpsAssistAction` for **non-tanks**
(`UldMultipliers_Mimiron.cpp:249-265`). So a tank is picked by the generic `TankAssistAction` →
`TankTargetValue`, which knows nothing about fire bots, and a fire bot reaches its attacker list as
soon as anyone damages one (`AttackersValue::AddAttackersOf` walks `GetThreatenedByMeList`).
`RaidUlduarStrategy` has no `AppendTargetExclusions` override — Kara, MC and SWP do — so the
engine-level exclusion channel in `GatherStrategyTargetExclusions` (`TargetValue.cpp:17-33`) is
inert for Ulduar. `dps aoe` is unguarded for the same reason: the guard's
`dynamic_cast<DpsAssistAction*>` does not match its sibling `DpsAoeAction`.

### 4. Oscillation: the pairs the last round targeted are fixed, `follow` is what is left

Counting A-B-A only between two movement nodes (spell/move alternation is normal interleaving, not
flapping):

| pair | P1 | P2 | P3 | P4 |
|---|---|---|---|---|
| `follow` <-> `mimiron proximity mine action` | **38** | - | - | - |
| `follow` <-> `mimiron dodge flames action` | **35** | - | **35** | - |
| `mimiron arc spread action` <-> `mimiron fire bot action` | - | - | 11 | - |
| `mimiron arc spread action` <-> `reach spell` | - | - | 10 | - |
| `mimiron rapid burst action` <-> `set behind` | - | 16 | - | - |

Shipped in `eb9db6855` and confirmed: **`follow` <-> `arc spread` in phase 1 is 0** (was 58 and 49),
**`flame dodge` <-> `reach spell` in phase 3 is 1** (was 89), and `mimiron.campturn` never fired —
the static turn to 25°/345° was enough on its own.

What is left is the same root cause the pitfalls doc already records, with a different partner: 166
`drop target` acts (**116 of them in phase 3**) push bots into the non-combat engine, where `follow`
competes with the dodges. `follow` issued **1,163 move records in phase 3**, more than the formation
itself (1,138).

### 5. Positioning: phase 2 was fixed, phase 3 gives up

Ranged/heal distance from the standing slot assignment, sampled every snapshot:

| phase | median | p75 | p90 | >10 yd |
|---|---|---|---|---|
| P1 `p1stack` | 2.6 | 10.0 | 19.8 | 25% |
| P2 `hmwedge` | **6.6** | 15.1 | 21.4 | 38% |
| P3 `p3wedge` | **14.2** | 41.3 | 63.7 | 54% |
| P4 `hmwedge` | 8.1 | 20.0 | 30.3 | 44% |

Phase 2 met the last round's ≤10 yd target (was 14.6). Phase 3 did not (was 23.5).

The slots are not the problem: `p3wedge` slots sit 17-27 yd from the room centre and move a median
1.0 yd between reassignments. The bots are. Splitting ranged phase 3 bot-seconds:

- 33% on slot (≤6 yd)
- 37% off slot, walking
- **30% off slot, standing still** — the formation is not even trying

Two mechanisms, both measured:

1. **Lock contention.** Arc spread walks at `combat`. Of 1,138 attempts in phase 3, 236 were issued
   and **584 lost to `wait`** — 335 to `forced` (the flame and fire bot dodges, which should win)
   and **249 to an equal-priority `combat` walk**, since `IsWaitingForLastMove` wants strictly
   higher. The other `combat` walkers are `reach spell` (795 moves) and `reach melee` (576).
2. **No candidate at all.** `GetMimironSlotApproaches`
   (`UldEncounter_Mimiron.cpp:349`) tries rings of 2, 4 and 6 yd × 12 bearings around a burning
   slot and **returns empty when none is standable**, which leaves the trigger inactive and the bot
   standing where it is. The fire field was saturated for the whole of phase 3 — the trace's
   40-creature watch cap (`OBS_MAX_WATCHED`, `RaidObsSession.h:41`) was filled by Flames alone from
   4:53 to 7:34, so there were at least 40 nodes at 5 yd radius each the entire time. A 6 yd ring
   around a burning slot is very likely burning too.

Only 56 `mimiron.approach` notes fired in phase 3 (34 detour, 22 substitute) against 1,138 arc
spread attempts, which is consistent with the second mechanism.

Side note: the bot tank sat a median 23.3 yd from the room centre in phase 3 against the ≤8 yd
target, but a **human (Dragon) main-tanked** this pull — Dragon took every Plasma Blast in phase 1 —
so the bot tank was not the one holding the centre and that target is not measurable here. The
Plasma Blast changes from `eb9db6855` are untested for the same reason.

### 6. Phase 4: the three parts do not converge, and B paid a full extra phase for it

Raised by the user after the first pass, and it is the largest single loss measured today.

**The mechanic.** In phase 4 a part that runs out of health does not die. `boss_mimiron.cpp:1061-1078`
sets `UNIT_FLAG_NON_ATTACKABLE` and casts `SPELL_SELF_REPAIR` (64383) on itself; `:1208-1215`
restores it to attackable when that lands. The encounter only ends when **all three are channelling
Self Repair at the same moment** (`:893` → `EVENT_FINISH`). The trace gives the cast time as
**15000 ms**, so the whole raid has a **15 s window** to put all three under.

**B missed it by about 1.7 s and repeated the phase.** Self Repair starts: MK II 6:57.985, VX-001
7:05.115, ACU ~7:14.7 (no cast row — it had dropped out of the watch set — but its restore at
7:29.721 is exactly 15 s later). First to last is **16.7 s**. The MK II's repair completed at
7:12.963, so all three were never channelling together, and each came back at 50%:

```
7:12.963  MK II   0.2% -> 50.2%
7:20.208  VX-001  0.1% -> 50.1%
7:29.721  ACU     0.3% -> 50.3%
```

Phase 4 ran 5:26 to 9:21, **235 s for what the second attempt did in 112 s**. The second run had a
7.2 s spread and finished. On a Firefighter pull this is not a delay, it is the pull.

**Ranged do prioritise the ACU — that is not the problem.** Phase 4 ranged put **94% of their damage
at it in both pulls** (61% of bot-seconds in B, 48% in A). The ACU still arrives last:

| | MK II | VX-001 | ACU |
|---|---|---|---|
| B, rate over the first run | 46.0k hp/s | 44.3k hp/s | **25.6k hp/s** |
| B, aimed in phase 4 | 4.21M | 5.50M | **12.37M** |
| B, health actually lost | 8.36M | 8.36M | **5.57M** |

Two structural reasons. **Melee cannot reach it**: the three parts share one x,y and are stacked
15 yd apart vertically, so melee targeted the ACU for 9% of their bot-seconds and landed under 3% of
their damage on it. And **about half of what ranged put out while pointed at the ACU does not land
on it** — one point for all three means pets, DoTs, totems and any AoE spread across the stack.
Measured over the hold window, ranged aimed 45k/s at the ACU and it lost 24k/s.

**The hold that should fix this is already there, and it parks too low.**
`GetMimironPhase4Focus` (`UldEncounter_Mimiron.cpp:797`) bars melee and non-ranged-dps from the ACU
(`:839`), then holds anything at or under `ULDUAR_MIMIRON_PHASE4_HOLD_PCT` = 10%
(`UldEncounter_Mimiron.h:232`) so the parts come down level. But by the time the ground pair reaches
that floor the ACU is already **20.2%** (6:30.7, MK II crossing 10% with VX at 11.1%). Ten points of
ground-pair headroom cannot absorb the ~23 s the ACU still needs, and it did not:

```
6:30  MK 10.3%  VX 11.3%  ACU 20.5%      <- hold floor reached
6:46  MK  4.8%  VX  4.6%  ACU 12.6%
6:58  MK  0.2%  VX  1.5%  ACU  7.9%      <- MK II starts Self Repair
7:08  MK  0.2%  VX  0.1%  ACU  4.4%
```

What pushed the pair through the floor over those 36 s, by share: splash and untargeted output
~44%, the two humans ~29% (527k, 71% of it into the ground pair, outside bot control), melee ~20%,
tank ~6%. Melee and tank **players** dealt 379k in the hold window with **281k of it into the ground
pair**, so the hold is also leaking at the source; the trace cannot say whether that is the focus
node handing back a ground part, bleeds already running, or cleave, and pets cannot be separated
(`dealt` is 0 on pet rows).

## Changes

Scope: the phase 4 rendezvous (raised by the user), fire bot fixes (tank + demote), phase 3 slot
holding, and the doc. The `follow`/`drop target` path is explicitly **out of scope** this round.

All paths relative to `modules/mod-playerbots/`. Match surrounding style — `UldEncounter_Mimiron.cpp`
and `UldMultipliers_Mimiron.*` are CRLF, `UldEncounter_Mimiron.h`, `UldTriggers_Mimiron.cpp` and
`UldStrategy.*` are LF; 120 column limit; comments state mechanism, no history.

### 1. Phase 4 converges on the part that cannot be helped, instead of on a fixed floor

`GetMimironPhase4Focus`, `src/Ai/Raid/Uld/Util/UldEncounter_Mimiron.cpp:797`.

The ACU is the constrained part: only ranged dps can damage it, and only about half of what they aim
lands on it. Everything else in the raid can hit the ground pair. So the ground pair must be paced
**to the ACU**, and the absolute 10% floor is the wrong currency because it does not know where the
ACU is.

Replace the `ULDUAR_MIMIRON_PHASE4_HOLD_PCT` floor test at `:848-864` with a floor that is the
**greater of** the existing 10% and `ACU health pct - ULDUAR_MIMIRON_PHASE4_CONVERGE_PCT`, a new
constant beside `ULDUAR_MIMIRON_PHASE4_HOLD_PCT` (`UldEncounter_Mimiron.h:232`). Start it at 4.0f,
two focus bands, so the ground pair parks just under the ACU rather than 10 points under it and the
three walk down together. Keep the absolute 10% as the lower bound so the old behaviour survives
once the ACU is itself low, and keep the `levelled` short circuit — with a relative floor it becomes
"all three within the converge margin", which is exactly the state the 15 s window needs.

Applied to B this parks the ground pair near 16% at 6:30 instead of driving it to 0.2%, and the
15 s window is a formality rather than a 16.7 s miss.

Also cover the leak at the source: the hold only reaches nodes that ask `GetMimironPhase4Focus`, and
melee plus tank players still put 281k of 379k into the ground pair while holding. Before changing
the floor, note `mimiron.dpsrule` as `p4hold:<part>` with the part the bot was holding off, so the
next trace says whether the leak is the focus node handing back a ground part or damage the node
never sees (bleeds, cleave, pets). Do not try to hold the tank's target — it needs threat; if the
tank's share matters later, hold its damage abilities the way `IsHeldTankDefensive` does, not its
target.

The two humans contributed 29% of the slide and no bot change reaches them. Worth telling the user
plainly: in phase 4 the humans should be on the ACU, not the ground pair.

### 2. Protected fire bots are off limits to every picker, not just the dps priority node

Add `HasTargetExclusions` / `AppendTargetExclusions` to `RaidUlduarStrategy`
(`src/Ai/Raid/Uld/UldStrategy.h`, `.cpp`), following `src/Ai/Raid/Kara/KaraStrategy.h:19-20` and
`src/Ai/Raid/MC/MCStrategy.h:19-20`. Append every guid from `GetMimironKeptFireBots(botAI, bot)`.

This feeds `GatherStrategyTargetExclusions` (`src/Ai/Base/Value/TargetValue.cpp:17-33`), so it
covers `TankTargetValue`, `dps target`, `dps aoe target` and the attacker-without-aura values in one
place — every path that is unguarded today. It also makes
`MimironSetDpsPriorityAction::ResolveTarget`'s fallback safe, which returns
`AI_VALUE(Unit*, "dps target")` with no `IsAllowedTarget` re-check
(`UldActions_Mimiron.cpp:1180-1188`).

`GetMimironKeptFireBots` already folds its grid scan on `ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS` per
instance, so this costs a vector copy per call. Keep the existing `IsMimironFireBotProtected` checks
in `BuildPriorityList` and `IsAllowedTarget` — they still do the right thing and cost nothing.

### 3. The cull stops outranking the boss

`MimironSetDpsPriorityAction::BuildPriorityList`, `src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp:992-995`.
Move the ranged `NPC_EMERGENCY_FIRE_BOT` entry from second place to **after** the mech tail at
1025-1027, so a cull happens with spare capacity rather than instead of the ACU. Leave the Bomb Bot
first — it explodes. Leave the melee/healer branch at 1007-1009 where it is; it already sits behind
the grounded ACU and the Assault Bot.

Rewrite the comment to say why the cull exists at all and why it is last: the Deafening Siren and the
Water Spray line (`UldEncounter_Mimiron.h:550-566`) are what a stray fire bot costs, and the
movement nodes already dodge both, so the kill is cleanup rather than a threat response.

### 4. Phase 3 keeps looking when the ring is all on fire

`GetMimironSlotApproaches`, `src/Ai/Raid/Uld/Util/UldEncounter_Mimiron.cpp:349`.

Widen the substitute search outside phase 1: add
`constexpr float ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS_WIDE = 12.0f;` beside
`ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS` (`UldEncounter_Mimiron.h:526`) and step the ring loop at
line 398 out to it in 2 yd steps when the MK II is not the live mech. Phase 1 keeps 6 yd — its camp
rows are 6 yd apart and a stand-in squeezed between them is how two bots share a Napalm Shell, which
the existing early-out at 379-381 already says.

12 yd is affordable in phase 3 specifically: the `p3wedge` orbit sits 17-27 yd from the room centre
against a ~35 yd cast range, and the candidate screen already enforces
`ULDUAR_MIMIRON_DISPERSE_DISTANCE` against every other member, so widening cannot clump the raid.

Then make the empty answer visible: when no candidate on any ring is standable, note
`mimiron.approach` as `none r<radius>` before returning empty. Today an empty return is silent and
indistinguishable in a trace from the trigger simply not firing, which is why finding 5 has to infer
it. That one note prices the remaining standing-still time on the next pull.

Do **not** add a "stand in the fire anyway" tier. The flame dodge runs at `forced` and outranks the
formation at `combat`, so a burning destination would just be walked back off, and that is the
`arc spread` <-> `dodge flames` flapping in finding 4.

The `combat`-vs-`combat` lock contention (249 waits) is left alone this round: `reach spell` losing
to the formation would cost casts, and the right trade needs its own measurement.

### 5. Docs

`docs/raids/ulduar/mimiron.md` — run `/compact-docs-writer` fresh first, per the governing-docs rule
(the invocation from the previous cycle is spent):

- The phase 4 rendezvous: Self Repair 64383 is a 15 s cast, the encounter ends only when all three
  channel together (`boss_mimiron.cpp:893`), B missed by 1.7 s and paid 112 s, and the ACU is the
  constrained part because melee cannot reach it and half of the ranged output aimed at it lands
  elsewhere.

- Today's two pulls, and that B was normal mode with hard mode logic live, so it prices nothing about
  Firefighter.
- The engagement-window table from finding 2, and that the 117 s of transitions is encounter cost.
- The berserk miss: 2.33M left, ~30 s.
- The fire bot finding: the cull is intended, the tank killing the protected pair is not, and what it
  cost (770k, 361 bot-seconds, 21% of ranged phase 3 time).
- Phase 3 slot holding: 33/37/30 on-slot / walking / standing still, and the empty-approach cause.
- Confirmation that the `eb9db6855` camp, campturn and detour work landed, and that the Plasma Blast
  order is still untested because a human main-tanked.

Save this plan to `docs/plans/mimiron-firebot-cull-p3-slots/mimiron-firebot-cull-p3-slots.PLAN.md`
first.

## Verification

Per-TU `-fsyntax-only` on every changed TU (scratchpad `syntax2.sh`, needs `MSYS_NO_PATHCONV=1` from
Git Bash), then a worldserver rebuild, then a **Firefighter** pull with the Big Red Button actually
pressed. On the new trace:

1. **Phase 4 rendezvous.** The spread between the first and last Self Repair cast under **15 s**
   (`phase4.py` prints the cast rows and any health going back up). No part restored to 50%. The
   ground pair never more than the converge margin below the ACU while the ACU is above 10%.
2. **Fire bots.** No tank damage into any fire bot (`firebot.py` per-attacker table). The two lowest
   guids alive stay alive until the ACU is under 15%. No `mimiron.dpsrule` = `firebot` while the ACU
   is above 15% and any of Bomb Bot / Assault Bot / grounded ACU / Junk Bot / ACU is up.
3. **Phase 3 pace.** Ranged bot-seconds on fire bots under 5% (was 21%); share of phase 3 output
   reaching the ACU above 65% (was 54%); ACU window under 120 s (was 138).
4. **Slot holding.** Ranged off-slot median ≤10 yd in phase 3 (was 14.2); ranged off-slot-and-still
   under 15% of phase 3 bot-seconds (was 30%); `mimiron.approach none r12` rows present and rare.
5. **No regression.** Phase 2 off-slot median stays ≤10 yd (6.6); phase 1 off-slot median stays ≤5 yd
   (2.6); `follow` <-> `arc spread` stays at 0 in phase 1; no new movement A-B-A pair above 30; raid
   nearest-neighbour in phase 1 stays clear of the Napalm pairing (camp nn ≥ 5 yd).
6. **The point of it.** Phase 4 reached before 7:30 and the berserk at 10:00 not reached.

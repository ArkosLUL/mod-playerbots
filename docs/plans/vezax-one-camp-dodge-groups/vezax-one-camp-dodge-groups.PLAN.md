# Vezax — one camp, two dodge groups

Slug for the plans directory: `vezax-one-camp-dodge-groups`.

## Context

The first ever traced Vezax pull is `env/dist/logs/botobs/603_1_general-vezax_1788614797.ndjson`
(25-man, 23 bots + 2 humans, wipe at 3:47, 30 deaths, first death 1:58). It says the three-block
formation from `5ecdb8633` does not hold up, and the user has asked for a specific replacement:
**stack healers and ranged DPS in one camp, split into two groups 5 yd apart; on Shadow Crash one
group strafes left and the other right; then both return and stand on the Shadow Crash field.**

Decisions already taken. Do not re-open these — the healer one was put to the user twice, the second
time with the field's full effect list in front of them, and reaffirmed:

- "the puddle that provides buffs" is the **Shadow Crash field (63277)**, not the Saronite puddle.
- Healers **join the camp and stand in fields**, accepting the −75% healing done.
- The mana economy is **out of scope** for this plan.

### What the trace established

Verified against the DBC, `spell_linked_spell`, the core scripts and the trace — not assumed.

| Fact | Evidence |
|---|---|
| 21 crashes, 91 bot-in-blast-at-cast events: **62 escaped, 29 caught**, 10 of the caught moved <2 yd | position deltas between the cast snapshot and cast + flight |
| Escapes clear by only 1-3 yd (11.2-13.6 vs a 10 yd impact) | `DODGE_CLEARANCE` is 12, so the margin is 2 yd by design |
| One crash covers a whole six-man block at 3.7 yd spacing | 5 bots in one blast at 1:16, 1:36, 1:46; 4 of 5 caught at 1:46 |
| **The Shadow Crash branch of `VezaxHazardListenerScript` never runs** | zero `haz` rows for 62659 across 21 crashes; all 5 `haz` rows are vapor puddles |
| **Mark of the Faceless is the largest damage source: 781,140**, and 63278 effect 2 is `HEALTH_LEECH`, so all of it healed Vezax | 196 `dmg` rows; melee ball worst hit (Obliteration 81k, Mighty 80k, Justice 75k, Bulwark 69k) |
| The old healer ring worked, and kept healers out of every field | 2087 of 3581 samples at 11.3 yd; both crashed healers (16.0 and 19.7 yd) were outside it on vapor excursions |
| Field soak works for DPS and is already carrying their mana | 63277 uptime: Agony 124 s, Fel 111 s, Stormweaver 89 s; healers only 3-9 s |
| Melee **can** be crash targets | Shadow targeted at 16.1 yd; the 2:19 impact at 16.0 yd caught Justice, Assasin and Obliteration |
| Every mana user still hit 0; healers empty from ~1:10, deaths from 1:58 | `snap.u` mana%, raid average 98% → 50% (1:00) → 28% (2:00) |

### The field's real effect list

`63277`'s own DBC row carries only magic damage (school mask 94), shadow damage (mask 32) and healing
done (mask 127) — stored base points `+99`, `+74`, `−76`, which are `+100%`, `+75%`, `−75%` once the
DBC's off-by-one is undone. The rest lives in a **linked spell**, which is why a DBC-only read gets
this wrong:

```
spell_linked_spell:  63277 → 65269  (type 2, aura link)
                    -63277 → -65269 (removal)
```

`65269` is `MOD_CASTING_SPEED_NOT_STACK` and `MOD_POWER_COST_SCHOOL_PCT` at school mask 127 (`+99`
and `−71` stored, so +100% cast speed and −70% cost).
So the field is **+100% cast speed and −70% mana cost on all schools**, exactly as
[UldEncounter_Vezax.h:80-82](../../../src/Ai/Raid/Uld/Util/UldEncounter_Vezax.h#L80) says. The trace
confirms it applies: 346 `65269` rows against 351 `63277` rows. `VezaxCanSoakShadowCrashField`'s mana
gate is correct, not a leftover. `63277 → 65269` is the only link across all 20 Vezax spell ids.

### Two facts in the header that *are* wrong

- **Vapors spawn within 45 yd of Vezax**, not on him: 63081 is `TARGET_DEST_CASTER_RADIUS` (72 is
  `_RANDOM`, 73 is `_RADIUS`) at radius index 11 = 45. `npc_ulduar_saronite_vapors` is a
  `NullCreatureAI` doing `MoveRandom(4.0f)`, so it never chases, never enters combat, and **cannot be
  pulled**. The five observed puddles landed 31, 37, 42, 46 and 50 yd from the anchor.
- **The puddle ticks every 2 s**, not 4: 63323 has `EffectAuraPeriod` 2000 with
  `PERIODIC_TRIGGER_SPELL` → 63322, and the damage rows are exactly 2.00 s apart.
  `VezaxShouldLeaveVaporPuddle` predicts the next tick on a 4 s clock.

Correct the comments; do not change the behaviour they describe (mana is out of scope).

---

## Geometry

All boss-relative. The boss can settle yards off his spawn on a ranged pull, which is why the old
healer ring was already measured from him.

**Camp** — bearing `ULDUAR_VEZAX_ARC_ORIENTATION` (π/2, north) as today, leaving the southern half
for the Mark spots.

- Two groups, **L** and **R**, on either side of the bearing. Group centre lines at tangential ∓4 yd.
- Each group is **2 files × 5 rows**: files 3 yd apart tangentially (∓5.5 and ∓2.5 from the camp
  centre line), rows 3 yd apart radially at **22 / 25 / 28 / 31 / 34 yd** from the boss.
- 10 slots per group, 20 total. 25-man needs 18 (12 ranged + 6 healers); 10-man needs 8.
- Nearest L member to nearest R member: **5 yd**, as asked.
- Slot index space: `[0,10)` = L, `[10,20)` = R. Fill order alternates L/R and fills near rows first,
  so under-fill stays balanced and healers spread across both groups rather than piling into one.

**Dodge** — a fixed group strafe, not a per-bot search. L translates to tangential −15 yd from its
resting slot, R to +15, both holding their radius so each group keeps its shape.

- Worst case clearance is `D − groupTangentialWidth` = 15 − 3 = **12 yd** against the 10 yd impact.
  That is the far-file member having to cross the impact point; it is the binding constraint on `D`
  and it must stay written down next to the constant.
- Flight budget: the impact sits 22-34 yd from the boss, missile speed 10 yd/s → **2.2-3.4 s**.
  15 yd at ~7 yd/s is **2.14 s**. The near row is the tight case and is the first thing the trace has
  to check.
- Fallback: if the strafe target is off-mesh or buried under a puddle this bot must avoid, fall back
  to the existing `FindNearestPositionClearOfHazards`.
- `ULDUAR_VEZAX_DODGE_BAND_MIN` / `_MAX` and `ULDUAR_VEZAX_HEALER_DODGE_BAND_MAX` go, and with them
  the clamp-revert branch that fired 7 times in the trace.

**navprobe, map 603, `--nav 0x09` — already run for this plan, do not redo.** The 28 yd ring is
16/16 on mesh. Every camp and dodge-lane extreme (x 1832.28-1873.28, y 103.4-115.4) is on mesh with
poly distance ≤ 0.36 yd. Z is 342.378 out to y ≈ 105 and settles 341.66-342.15 beyond that — WMO
rubble in the northern hall, a ~0.5 yd dip that `UpdateAllowedPositionZ` handles, not a hole.

---

## Changes

### 1. `UldBotScripts.cpp` — the dead Shadow Crash hook

[UldBotScripts.cpp:80-84](../../../src/Ai/Raid/Uld/Util/UldBotScripts.cpp#L80) reads
`spell->GetUniqueTargetInfo()`, but 62660's two effects are both `TRIGGER_MISSILE` with
`TARGET_DEST_TARGET_ENEMY` — a destination, no unit target — so the list is empty and the branch
returns before doing anything at all.

Read the destination from `spell->m_targets.GetDstPos()`, falling back to `GetUnitTarget()`, exactly
as `TryGetVezaxShadowCrashImpact` already does.

This is the highest-value change in the plan. It restores the `haz` circle for the missile (zero rows
across 21 crashes today) **and** `InterruptVezaxCastersNear` — which is why 10 of the 29 caught bots
never moved: mid-cast, with nothing breaking the cast.

### 2. `UldEncounter_Vezax.h` — constants

Replace the three-block constants (`HEALER_RADIUS`, `HEALER_SPACING`, `RANGED_NEAR/FAR/OVERFLOW_RADIUS`,
`RANGED_GROUP_OFFSET`, `RANGED_SPACING`, `BLOCK_ROW_SLOTS`, the `*_SLOTS` counts, the dodge bands) with
the camp set above: camp centre radius, row spacing, file spacing, group offset, rows per group,
files per group, and the strafe distance. Keep `ULDUAR_VEZAX_HAZARD_*`, the tolerances, the arena
bubble, and the Mark constants.

Correct the two wrong comments (vapor spawn radius and pull-ability; the 2 s puddle tick). Leave the
field's effect list alone — it is right.

### 3. `UldEncounter_Vezax.cpp` — formation and dodge

- `VEZAX_SLOT_ROWS` → the new rows (no healer ring, no overflow rows).
- `VEZAX_SLOT_FILL_ORDER` → alternating L/R, near rows first.
- `VezaxSlotBlockName` → `L` / `R`.
- **Drop `VezaxSlotSuitsBot`** and its call in the displacement loop. Healers no longer have their own
  slot kind, so any clear slot will do.
- `VezaxSlotToleranceFor` collapses to one tolerance (`ULDUAR_VEZAX_HEALER_SLOT_TOLERANCE` existed only
  to keep healers between the melee ring and the 12.5 yd exclusion line).
- `TryGetVezaxDodgeSpot` → the fixed strafe. It needs the bot's slot to know its group; the assignment
  is already in `vezaxEncounterStates`, so look it up rather than threading a parameter through.
- `TryGetVezaxMarkSpot` → **delete the "18 yd outward along your own bearing" branch**; everyone takes
  the three fixed southern spots. With one camp, an 18 yd outward step from a near-row bot lands at
  radius 40, which is 6 yd from the far row — inside the 15 yd leech. The southern spots (bearings
  π/2 ± 2.3208 and π/2 + π at radius 26) are ≥ 38 yd from every camp and dodge-lane slot.

`VezaxTakesSlot` needs no change: `PlayerbotAI::IsRanged` already covers healers, which is how they
held slots in the old layout.

### 4. Healers stop avoiding fields

Healers now stand in the camp, and the camp is where fields land — one 8 yd field is 201 yd², larger
than the whole ~11 × 12 yd camp footprint, and one lands every 10 s for 20 s. Leaving the avoidance in
would displace every healer out of the camp on that cadence, which is the opposite of the design.

- **Delete `VezaxMustLeaveShadowCrashField`.** Its only caller is `VezaxBuildAvoidPositions`.
- `VezaxBuildAvoidPositions` drops `avoidFields` and the `isShadowCrashField ? … : …` branch: `avoid`
  now only ever holds vapor puddles the bot is not entitled to.
- Keep `VezaxHazard::isShadowCrashField` and `TryGetVezaxNearestHazard`'s `wantShadowCrashField` —
  the soak action still asks for fields.
- Leave `VezaxCanSoakShadowCrashField` excluding healers. Not avoiding a field is not the same as
  walking to one: for a healer the field is 0.25× healing per cast and 0.83× healing per mana point,
  so it is worth accepting where they already stand and never worth travelling to.

### 5. Telemetry

- `vezax.block` → `L` / `R` / `tank` / `unslotted` / `loose` / `stuck`. The old `heal`, `left`, `right`
  and overflow names go with the layout.
- `vezax.dodge` → `strafe` (the fixed vector), `search` (fallback), `none`. Replaces
  `band` / `heal` / `blast`.

Nothing else moves. The restored 62659 `haz` row is what lets a postmortem measure clearance directly
instead of inferring it from snapshot deltas.

### 6. `docs/raids/ulduar.md`

Rewrite the Vezax chapter for the new formation and fix the two wrong facts. **Open a fresh
`/compact-docs-writer` cycle before editing** — `docs/engine/pitfalls.md` is referenced by
`modules/mod-playerbots/CLAUDE.md` and itself points at `docs/raids/**`, so the compaction rule
applies.

Worth a line in the chapter, since it cost a wrong conclusion this session: 63277's mana and cast
speed halves live in `spell_linked_spell` → 65269, not in its own DBC row.

---

## Deliberately not in scope

Recorded so a later session does not quietly add them:

- **The mana economy.** The user scoped it out. The field already carries the DPS half (−70% cost, and
  Agony/Fel/Stormweaver held 89-124 s of it). The healers' half is the puddle, and it is not working:
  5 bots ever got a stack, 23 ticks, ~7k mana for the whole raid over 227 s, because
  `VAPOR_SOAK_MAX_TRAVEL` is 25 yd and the puddles were 31-50 yd out. Moving healers into the field
  changes this picture on its own — see the verification table.
- **The `vezax.formation` gate flapping** (`outside`↔`on` 52 times). Vapor excursions cross the 45 yd
  arena bubble and drop the gate mid-run. Tied to the mana work above.
- **Melee dodging.** `DodgesShadowCrash` requires `IsRanged`, and the trigger comment claims nothing
  lands nearer than 14.5 yd so melee cannot be caught. The trace disproves it: Shadow was targeted at
  16.1 yd and the 2:19 impact caught three melee. Correct the comment; leave the behaviour. It is 2 of
  21 crashes and 3 of 29 caught events — real, but secondary to the camp rework.

## Files

| File | Change |
|---|---|
| [UldBotScripts.cpp](../../../src/Ai/Raid/Uld/Util/UldBotScripts.cpp) | read the crash destination from `m_targets`, not `GetUniqueTargetInfo` |
| [UldEncounter_Vezax.h](../../../src/Ai/Raid/Uld/Util/UldEncounter_Vezax.h) | camp constants replace the block constants; drop `VezaxMustLeaveShadowCrashField`; two comment corrections |
| [UldEncounter_Vezax.cpp](../../../src/Ai/Raid/Uld/Util/UldEncounter_Vezax.cpp) | slot rows, fill order, block names, drop `VezaxSlotSuitsBot` and the field avoidance, rewrite the dodge, trim the Mark spot |
| [UldTriggers_Vezax.h](../../../src/Ai/Raid/Uld/Trigger/UldTriggers_Vezax.h) | correct the dodge trigger's "melee cannot be caught" comment |
| [docs/raids/ulduar.md](../../../docs/raids/ulduar.md) | Vezax chapter, via `/compact-docs-writer` |

No node ladder change: the dodge stays at `ACTION_EMERGENCY + 9` and the soak at `ACTION_RAID + 2`.
No reader change and no `SCHEMA_VERSION` bump.

## Verification

1. `py -3 apps/codestyle/codestyle-cpp.py`. Pre-existing hits under `src/server/` and `src/tools/`
   are not ours.
2. Build is a hand-off — the module cannot be compiled headless here.
3. `docker exec ac-worldserver env | grep ^AC_AI_PLAYERBOT_OBS_` before pulling; never trust the
   `.conf`.
4. navprobe is done (see Geometry). Re-run only if the radii change.
5. **25-man pull, then read the trace only.** Each row must be answerable without a second pull.
   Baselines are from `603_1_general-vezax_1788614797.ndjson`:

| Question | Command / check | Baseline |
|---|---|---|
| Did the camp hold? | `--notes vezax.block` — ≤10 `L`, ≤10 `R`, one `tank`, no `unslotted`/`stuck`/`loose` | 5 `left`, 5 `right`, 4 `heal` |
| Did the fixed strafe fire? | `--notes vezax.dodge` — mostly `strafe`; heavy `search` means the vector lands off-mesh or on a puddle | 15 `band`, 7 `blast`, 1 `heal` |
| Is the missile visible at last? | `haz` rows for 62659 — one per crash | **0** across 21 crashes |
| Did the dodge actually work? | re-run escaped/caught against `haz` centre + 10 yd at cast + flight; **no** bot moving <2 yd | 62 escaped / 29 caught, 10 immobile |
| Did one crash still catch a whole group? | bots per impact — one group, never both | 5 of 6 in one block, three times |
| Did the raid stop feeding the boss? | total 63278 damage | **781,140** |
| Did healers survive standing in fields? | healer `65269` uptime, healer mana% floor, healer effective healing | uptime 3-9 s; all four OOM by 1:50; 3,295,466 effective |
| Did the DPS keep their buff? | 63277 uptime per ranged DPS | Agony 124 s, Fel 111 s, Stormweaver 89 s |
| Overall | `end.out`, death count, time | wipe, 30 deaths, 3:47 |

The healer row is the one that decides whether "one camp, healers eat the field" was the right call.
It cuts healing to 25% and mana cost to 30% at the same time; if effective healing falls and the raid
still dies at 2:00, the healer stand is the first thing to revisit.

6. **Wipe and re-pull twice** — `vezax.slot` must show a full erase between pulls.
7. **10-man** — 8 slots filled, both groups balanced, no `unslotted`.

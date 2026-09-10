# Mimiron Firefighter: fire the Plasma Blast defensive, and stop Napalm deleting the ranged camp

## Context

Two Firefighter attempts on 2026-09-10, the first pulls running `55977b89e` (tank threat-hold
before the drag, Mimiron threat redirect, Plasma Blast defensives, Shock Blast above the fire dodge,
four rotating stack anchors). Traces `603_1_mimiron_1789072883` (**b1**) and `1789073123` (**b2**)
in `/azerothcore/env/dist/logs/botobs/` inside `ac-worldserver`. The three pulls before the change
are `1789063575` / `1789063939` / `1789064624` (**a1**/**a2**/**a3**).

Both new pulls wiped in phase 1, at 92.4 s and 123.9 s; a2 and a3 had reached phase 2.

**Three of the five changes did exactly what they were built to do, and must not be disturbed.**

| | a3 | b1 | b2 |
|---|---|---|---|
| MK II off the tank anchor | med 10.5, p90 107.8 | med 5.4, p90 15.0 | med 5.4, p90 13.4 |
| Shock Blast damage / deaths | 597,868 / 7 | **0 / 0** | 76,792 / 1 |
| `shock locked` (a1/a2/a3: 58/63/127) | 127 | 21 | 43 |
| `rapidburst` / `rocket` / `frostbomb locked` | 194 / 74 / 85 | **0 / 0 / 0** | **0 / 0 / 0** |
| Flames damage | 625,042 | 339,860 | 216,836 |

The MK II goes to the main tank at 13.7 s and stays unbroken to 50.4 s. The generic `tricks of the
trade` node is vetoed by `mimiron generic redirect guard` 11 / 16 times and `mimiron redirect threat
action` fires in its place (9 / 8 casts). Two things are still broken.

### Finding 1 — the Plasma Blast defensive has never fired, because the spell id is the 10-man one

Zero `mimiron.plasma` notes. `mimiron plasma blast defensive action` does not appear in the act
stream at all, in either pull — the trigger has never once returned true.

`SPELL_MIMIRON_PLASMA_BLAST = 62997` (`UldEncounter_Mimiron.h:52`), but `spelldifficulty_dbc` in the
**world DB** (not `SpellDifficulty.dbc`, which has no row for it) maps `62997 -> 64529` on 25-man,
and the trace confirms the cannon casting **64529** (83 snapshot samples). So
`FindCurrentSpellBySpellId(62997)` can never match, at `UldTriggers_Mimiron.cpp:371` and
`UldActions_Mimiron.cpp:1075`.

Cost: Bulwark died to Plasma Blast twice in each pull — 52.1 s and 76.5 s (b1), 51.2 s and 73.5 s
(b2). Plasma Blast was 11.5 % / 17.8 % of all damage taken.

An audit of all 139 Ulduar spell constants against `spelldifficulty_dbc` found 28 with a 25-man
remap. Every one is already handled as an explicit 10/25 pair (`SPELL_IRON_ROOTS_DAMAGE_10/25`,
`SPELL_IGNIS_SLAG_POT_10/25`, ...) except two: this one, and `SPELL_IGNIS_SCORCH` 62546 -> 63474 at
`UldEncounter_Ignis.cpp:321`.

### Finding 2 — Napalm Shell now deletes the whole ranged camp

Napalm was **61.0 % / 46.9 %** of all damage taken (1,069,335 / 834,293). Eleven ranged and healers
died between 38.0 s and 43.0 s in b1, ten between 37.9 s and 41.9 s in b2.

Napalm Shell 65026 is a **5 yd** blast: 9,424 on impact plus a periodic aura of 5,999 every 1000 ms
for 8000 ms — about **48k** to everything inside the radius, against 22,000-30,000 HP pools. The
boss fires it every 14 s at a random player.

| victims per cast | a1 | a2 | a3 | b1 | b2 |
|---|---|---|---|---|---|
| mean | 1.19 | 1.76 | 1.33 | **8.18** | 3.03 |
| worst cast | 2 | 5 | 3 | **13** (564,724 dmg) | **14** (402,769 dmg) |

**Both causes are consequences of the tank hold working.**

*The raid now actually stands on the anchor.* `MimironFormationGuardMultiplier`
(`UldMultipliers_Mimiron.cpp:104`) zeroes `combat formation move` once a bot is within
`ULDUAR_MIMIRON_SPREAD_TOLERANCE` (5.0) of its slot — and all 14 ranged share **one** slot, because
`MimironPhase1StackSlot` has no per-bot term. So the unstacker switches itself off on arrival and
they stand on one point. Share of ranged pairs closer than 5 yd: 56.1 / 38.2 / 65.3 % before,
**97.1 % / 85.9 %** now.

*The boss is now held next to them.* The script picks uniformly among players with
`plr->GetDistance2d(mkII) > 15.0f`, and `WorldObject::GetDistance2d` subtracts both combat reaches
(`Object.cpp:1319` -> `GetObjectSize()` returns `UNIT_FIELD_COMBATREACH`, which is **8** for entry
33432). The real cutoff is therefore raw **> ~24.5 yd**, and an empty pool falls through to
`SelectTarget(Random, 0, 100.0f, true)` — a threat-list pick with no distance floor, which lands in
the camp.

| | a1 | a2 | a3 | b1 | b2 |
|---|---|---|---|---|---|
| MK II -> ranged centroid, median | 21.5 | 29.2 | 20.0 | 19.0 | 17.2 |
| ranged past the 24.5 yd cutoff | 29.8 % | 60.6 % | 47.4 % | **16.5 %** | **2.7 %** |
| eligible pool empty | 12.6 % | 6.7 % | 6.2 % | **35.8 %** | **68.1 %** |

The old pulls were flattered by this: a human player parked 48-97 yd out was the only eligible
target and soaked 4 of 5 shells in a1 and 3 of 6 in a3.

`ULDUAR_MIMIRON_PHASE1_STACK_DISPERSE = 3.0` was chosen deliberately — the comment at
`UldEncounter_Mimiron.h:149-153` says it "deliberately fails to clear Napalm's 5 yd" and prices that
at "1.38 victims a cast". The measured price is 13 and 14, twice, and a phase-1 wipe each time. That
comment states the opposite of what the data now says and has to be rewritten, not just retuned.

### What is not in scope

Bomb Bot `Explosion` rose to 204,144 (11.6 %) in b2, but every one of those deaths is after 116 s
with the healers already dead — a wipe cascade, not a cause. `mimiron dodge flames action` returned
FAILED 58 / 55 times ("no safe spot", 770-790 candidate bearings rejected) and ranged `%cast` is
still 45.1 % / 37.5 % against the 63-65 % the centre fight managed. Both are real and both are
downstream of losing 11 bots at 38 s; re-measure them on a pull that survives.

---

## Change 1 — check the difficulty-remapped spell id

Add `SPELL_MIMIRON_PLASMA_BLAST_25 = 64529` beside the existing constant and match either id. Both
call sites want the same thing, so put the lookup in the util next to the other Mimiron helpers:

```cpp
// The cannon's cast is remapped per difficulty (spelldifficulty_dbc 62997 -> 64529), so the
// 10-man id alone never matches in a 25-man raid.
Spell* GetMimironPlasmaBlastCast(Unit* cannon);
```

- `src/Ai/Raid/Uld/Util/UldEncounter_Mimiron.{h,cpp}` — the constant and the helper.
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.cpp:371` and
  `src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp:1075` — call it instead of
  `FindCurrentSpellBySpellId`.
- `src/Ai/Raid/Uld/Util/UldEncounter_Ignis.cpp:321` — same fix, adding `SPELL_IGNIS_SCORCH_25 =
  63474`. No trace to verify against; it goes in as a correctness fix.

## Change 2 — let the unstacker work inside the phase-1 camp

Keep the camp — the flame-convergence reasoning behind it is sound and Flames damage did fall from
625k to 217-340k. Make it a camp rather than a point, by giving the phase-1 stack a slot tolerance
wide enough that the unstacker is not switched off inside it.

- `ULDUAR_MIMIRON_PHASE1_STACK_DISPERSE`: **3.0 -> 6.0**, clearing Napalm's 5 yd with a yard of
  pathing slop. Rewrite the comment above it: the clump is still deliberate, what is no longer
  deliberate is packing inside the splash.
- New `ULDUAR_MIMIRON_PHASE1_STACK_TOLERANCE = 10.0f`. Fourteen bots at 6 yd separation need roughly
  a 12 yd radius to pack into; 10 keeps the formation quiet while they settle without letting the
  camp sprawl past casting reach.
- `GetMimironSpreadSlot` gains `float* outTolerance = nullptr`, set to the new constant when
  `branch` is `p1stack` and to `ULDUAR_MIMIRON_SPREAD_TOLERANCE` otherwise. It is the single place
  that already knows the branch.
- Both existing readers must use it or they will disagree about where the slot ends:
  `MimironFormationGuardMultiplier::GetValue` (`UldMultipliers_Mimiron.cpp:104`) and
  `MimironPhase1PositioningTrigger::IsActive` (`UldTriggers_Mimiron.cpp:158`).

No new geometry: `CombatFormationMoveAction::Execute` (`MovementActions.cpp:2423`) already enforces
disperse as a minimum pairwise separation, and `GetMimironPhase1DisperseDistance` already feeds it
the Firefighter value.

**Known risk.** The camp can now be about 20 yd across against a 24.5 yd casting reach, with anchors
20-22 yd from the boss, so a bot on the far edge can fall out of range. `reach spell` at
`ACTION_HIGH` pulls it back in and the wider tolerance stops the formation fighting that, but this
is the thing to watch — check 5 below. If it bites, the anchors move in a couple of yards rather
than the tolerance coming back down; the boss now sits a median 5.4 yd off the anchor, so the 18 yd
Shock Blast floor is finally predictable enough to plan against.

## Docs

`docs/engine/pitfalls.md` is referenced by the project `CLAUDE.md`, so run `/compact-docs-writer`
before editing it — a fresh invocation, not the one from the previous cycle. Two entries:

- A spell id in a boss script is the 10-man id; `spelldifficulty_dbc` in the **world DB** remaps it
  and the client `SpellDifficulty.dbc` may have no row at all. Any `FindCurrentSpellBySpellId` or
  aura check against a boss cast has to match both.
- `WorldObject::GetDistance2d(WorldObject const*)` subtracts both combat reaches, so a script's
  `> 15.0f` against a creature with `CombatReach 8` is really `> ~24.5` raw. Reading a range check
  off the literal is wrong for large creatures.

`docs/raids/ulduar/mimiron.md` gets the two findings, the before/after table, and the correction to
the Napalm trade.

## Verification

Per-TU `-fsyntax-only` on every changed TU against the build image's `compile_commands.json`, then
the worldserver Docker rebuild, then one Firefighter pull. All pass/fail:

1. **Plasma Blast is covered.** `mimiron.plasma` notes exist at all — that alone is the fix landing.
   Exactly one button per window, tank or healer and never both, fired while the cannon is casting.
   No tank death inside a Plasma window.
2. **Napalm stops being a wipe.** Victims per cast back under ~2, worst cast in single figures, and
   no mass-casualty window. Napalm falls from 47-61 % of damage taken.
3. **The camp is a camp.** Share of ranged pairs closer than 5 yd drops from 86-97 % toward the
   38-65 % the old pulls managed. `combat formation move` still appears in the act stream during
   phase 1 rather than being vetoed to silence by `mimiron formation guard`.
4. **No regression on what works.** MK II within ~5 yd median of the tank anchor, Shock Blast still
   at or near zero deaths, `rapidburst`/`rocket`/`frostbomb locked` still zero, Flames no worse than
   340k, phase-1 centre flame damage still 0 %.
5. **The camp stays in range.** `reach spell` does not dominate the phase-1 move stream and ranged
   `%cast` recovers from 37.5-45.1 % toward 63-65 %. Sustained `reach spell` means the camp outgrew
   casting reach and the anchors need moving in.
6. **Phase 2 is reached again**, so the parts of the previous change that were never exercised —
   the stack rotation past two switches, the cleanse window — get a pull to be measured on.

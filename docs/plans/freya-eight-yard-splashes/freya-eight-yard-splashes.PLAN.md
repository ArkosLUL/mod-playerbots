# Freya: stop the raid dying in one 8-yard circle

## Context

`env/dist/logs/botobs/603_1_elder-stonebark_1788717562.ndjson` (2026-09-06 21:04) wiped at 4:59.127
with Freya at 100% — phase 1, 120 of 150 Attuned to Nature stacks off, which is *on pace*: the kill
`1788613108` had 150 off at 5:22, and the other wipe today `1788716763` needed 10:17 for the same 150.
The wave clear was not the problem. Both pulls today ran the binary built 2026-09-05 23:58 local, three
minutes after `a4c220ee5`, so the lasher-camp change is in.

**Seven bots died inside 85 ms at 4:12.28-4:12.37, eight inside nine seconds.** Ingredients, in order:

| time | event | raid damage |
|---|---|---|
| 3:44.438 | Ground Tremor (raid-wide, undodgeable) | 124,509 |
| 3:47.972 | Freya Sunbeam on Mighty, 15 hit | 159,329 |
| 3:50-4:00 | Nature's Fury on **Elemena, a healer**, 17 allies inside 8 yd | 202,424 |
| 4:03.225 | Sunbeam on a **hunter pet** that ran into the ball mid-cast, 16 hit | 164,739 |
| 4:04-4:12 | Nature's Fury on Druidica, 13 allies inside 8 yd | 88,508 |
| 4:12.244 | Ground Tremor, 24 hit | **185,389** |

Healing was already flat out and losing — 394k/405k landed against 430k/516k taken in the two 15 s
buckets, overheal down to 12% from 40-50% earlier — and two of four healers were in the ball at 20-30%
when the tremor landed. The raid never recovered: 10 dead by 4:19, wipe at 4:59.

**Root cause: the raid stood twice as tight as in the kill, and Freya's two big mechanics are both 8 yd
splashes.** Per minute, against the kill and the other wipe:

| pull | out | dmg/min | Sunbeam | Ground Tremor | Nature's Fury | median victims per Sunbeam | raid inside 8 yd |
|---|---|---|---|---|---|---|---|
| `1788613108` | kill | 899k | 137k | 243k | 105k | 4.5 | 5.0 |
| `1788716763` | wipe | 948k | 137k | 224k | 56k | 5.0 | 5.0 |
| **`1788717562`** | **wipe** | **1,163k** | **271k** | 222k | **157k** | **12.5** | **8.0** |

Ground Tremor is flat across all three — it is `EffectRadiusIndex 28`, 50000 yd, and nothing can be done
about it. That is the control: the fight did not get harder, the raid got tighter, and the two mechanics
that scale with tightness went up 2x and 1.5x. Median nearest-neighbour distance was **0.6 yd** against
1.3 in the kill.

DBC facts behind that (`modules/mod-spell-tweaks/data/dbc-reference/`):

- **Nature's Fury** `62589` (10) / `63571` (25): 10 s aura on one player, `EffectAuraPeriod 2000`,
  triggering `62590`/`63570` five times. The trigger is `EffectRadiusIndex 14` = **8 yd** around the
  carrier. Measured in the trace: victims sat a median 2.3 yd from the carrier, max 9.1. The Conservator
  casts it every 14 s at a random target within 100 yd (`boss_freya.cpp:1249`).
- **Sunbeam** `62623` (10) / `62872` (25): `CastingTimeIndex 16` = **1.5 s cast** at a random threat-list
  target, `EffectRadiusIndex 14` = **8 yd** at that target's location. `Spell::SelectSpellTargets()` runs
  from `Spell::cast()`, not from `prepare()`, so the destination follows the target until the cast
  finishes — the 4:01 cast is the proof: the pet moved 15.3 yd during it and **15 of its 16 victims were
  outside 8 yd when the cast started**.

Three separate things drove the tightness, and all three are ours:

1. **The Conservator wave stacks the raid by design.** `GetFreyaTargetSpore` gives ranged and healers
   their *nearest* Healthy Spore, and since they start in one ball the nearest is the same spore for all
   of them. At 3:55 **18 of 25 bots held Potent Pheromones from one spore** while four other spores had
   one bot each. Spore groups sit 15-45 yd apart, so the separation was there and unused.
2. **The ranged camp is a point.** Since `a4c220ee5` it returns the anchor bot's exact position when no
   lasher is low, and `FreyaRangedCampAction` walks to that point. Back line inside 8 yd during lasher
   waves: 9.0 in the kill, 12-13 in both pulls after the change.
3. **Nothing reacts to either mechanic.** `grep -i "fury"` over `src/Ai/Raid/Uld` returns only Hodir's
   Fury, from Flame Leviathan.

Separately, and reported by the user: **the main tank walks Freya out of the room.**
`FreyaTankNatureBombAction` aims its escape "at the far side of her from the back line"
(`UldActions_Freya.cpp:156-159`), bombs land every 18 s, and nothing ever walks back — so the tank
random-walks in one direction. In `1788716763` Freya ended **100 yd** from where she was tanked, out at
X 2453 where the floor settles to Z 419.9, six yards below the tanking spot and down the slope toward
the water.

Intended outcome: a Nature's Fury or a Sunbeam lands on two or three bots instead of sixteen, the raid
survives a Ground Tremor at the end of a Conservator wave, and Freya stays where she is tanked.

## Approach

Five changes. Wave clear pace must not regress — that is the guard on all of them.

### A. Split the back line across spores — `GetFreyaTargetSpore`, `UldEncounter_Freya.cpp:278`

Melee and the tank keep the parked spore (unchanged: they need melee range of the Conservator, and
`GetFreyaConservatorSpore` keys off the boss so tank and melee agree without communicating).

Ranged and healers take the nearest spore **with room** instead of the nearest outright: fewer than
`ULDUAR_FREYA_SPORE_CROWD` (6) raid members within its 6 yd aura. That leaves the parked spore over the
line from the start, since the melee group is already on it, which is exactly the pile to break.

- No latch and no shared state. The trigger already stands down the moment the bot holds Potent
  Pheromones, so the choice is only ever made on the way in and cannot churn once sheltered.
- All spores full — three alive at a time (22 s despawn in `boss_freya_healthy_spore::UpdateAI`, one
  summoned every 8 s by `62566`) against fifteen bots that need one — falls back to the nearest.
  Sheltered and stacked still beats pacified.
- **The trigger and the action must both keep calling `GetFreyaTargetSpore`** — deriving the spore twice
  is the bug `docs/raids/ulduar/freya.md:86` records.

A compass-quadrant split was the first plan and is wrong: the four summon spells (`62582` / `62591` /
`62592` / `62593`) carry `ImplicitTargetA` 82-85, which are **caster-relative** (front-left, back-left,
…), not compass directions, and the core only ever casts `62566` → `62582`. Quadrants keyed off world
bearing would have been fiction.

This reverses the rule at `freya.md:107-108` ("the 'go to the parked spore' rule is what makes the raid
re-converge afterwards instead of smearing across three spores"). Re-converging is what killed this pull;
the doc paragraph has to change with the code.

### B. Nature's Fury bail — new node

- Trigger `freya nature fury bail`: `bot->HasAura(62589) || bot->HasAura(63571)`, and at least one other
  living raid member within `ULDUAR_FREYA_NATURES_FURY_RADIUS`.
- Action: walk to the live Healthy Spore with no other raid member inside the splash radius, preferring
  the nearest. No such spore, or no Conservator up: `FindNearestPositionClearOfHazards` with the other
  raid members' positions as the hazards at `_NATURES_FURY_CLEAR`. Latch on `spot`/`spotMs` for
  `_NATURES_FURY_LATCH_MS`, returning `true` while in flight — the same shape as
  `FreyaDodgeUnstableSunBeamAction` (`UldActions_Freya.cpp:527`).
- Relevance `ACTION_RAID + 4`, alongside the other escapes, so it outranks the spore node (`+2`) that
  would otherwise walk the carrier straight back into the ball.
- Going to another spore rather than to open floor is what keeps the carrier out of Conservator's Grip
  (`62532`, `APPLY_AREA_AURA_ENEMY` + `MOD_PACIFY_SILENCE` at 50000 yd, permanent while the Conservator
  lives) for all but the ~4 s walk.

### C. Sunbeam step-out — new node

- Trigger `freya step out of sunbeam`: ranged and healers only; Freya's `GetCurrentSpell(CURRENT_GENERIC_SPELL)`
  is `62623` or `62872`; `spell->m_targets.GetUnitTarget()` is not this bot; the bot is within
  `ULDUAR_FREYA_SUNBEAM_AVOID_RADIUS`. The `GetCurrentSpell` + `m_targets.GetUnitTarget()` idiom is
  already used in `BTTriggers.cpp:239`, `HyjalHelpers.cpp:141` and `ICCScripts.cpp:71`.
- Action: `FindNearestPositionClearOfHazards` with the target's position plus
  `GetFreyaEscapeHazards(...)`, clearance `_SUNBEAM_CLEAR_RADIUS`. Latch `_SUNBEAM_LATCH_MS` (1500, the
  cast). Relevance `ACTION_RAID + 4`.
- Melee and the tank are excluded: they cannot leave what they are hitting, and 13 of this pull's 16
  volleys were centred on a ranged bot or its pet anyway. The target itself is exempt because the
  destination follows it — it cannot dodge its own beam.

### D. Camp stops piling — `FreyaRangedCampAction::Execute`, `UldActions_Freya.cpp:663`

Walk to a point `ULDUAR_FREYA_RANGED_CAMP_SPACING` (6 yd) out from the camp spot on the bearing from the
camp to the bot, instead of onto the camp spot itself. Bots keep the direction they came from, so they
land spread around the ring with no shared state and no extra churn; the trigger's tolerance is
untouched, so a bot already inside it still does not move.

Be honest about the ceiling in the code comment: 12 bots on a ring only clear 8 yd from each other at a
radius of 15.5 yd, which would be a 31 yd ball — past `AiPlayerbot.SpellDistance` and wide enough to
spread the lasher pack, since lashers charge random players and `AoeTrigger::IsActive` counts attackers
within 8 yd of the **current target**, not of the bot. This change stops the 0.6 yd pile; C is what
actually answers Sunbeam.

### E. Leash Freya to her tanking spot

- `ULDUAR_FREYA_TANK_ANCHOR = (2360.0847, -43.1235, 425.333)` — the user's spot, navprobe-verified on
  the bot filter (`--nav 0x09`): nearest poly 0.52 yd, `UpdateAllowedPositionZ` 425.333, and a 12 yd ring
  around it is 8/8 on mesh at Z 423.8-425.8.
- `FreyaTankNatureBombAction`: prefer escape candidates within `ULDUAR_FREYA_TANK_LEASH` (20 yd) of the
  anchor, and when the tank is already outside the leash, bias the escape toward the anchor instead of
  "away from the back line". Fall back to any clear spot if the leash has none — eating a bomb is worse.
- New node `freya tank hold freya`: main tank only (`PlayerbotAI::IsMainTank`), active while Freya is
  beyond `_TANK_LEASH` from the anchor and no bomb is pressing, walking the tank back to the anchor so
  she follows. Relevance `ACTION_RAID + 1` — under the bomb escape at `+4`, over the DPS ladder.
- `UldTriggers_Freya.cpp:85` says "The main tank is out because Freya is never repositioned"; that is no
  longer true and the comment goes with the change.

## Files

- `src/Ai/Raid/Uld/Util/UldEncounter_Freya.h` — the new spell ids (`62589`/`63571`, `62623`/`62872`),
  `_NATURES_FURY_RADIUS`/`_CLEAR`/`_LATCH_MS`, `_SUNBEAM_AVOID_RADIUS`/`_CLEAR_RADIUS`/`_LATCH_MS`,
  `_RANGED_CAMP_SPACING`, `_TANK_ANCHOR`, `_TANK_LEASH`; the reworded `GetFreyaTargetSpore` contract.
- `src/Ai/Raid/Uld/Util/UldEncounter_Freya.cpp` — the crowd-capped spore choice, `GetFreyaSpores`,
  `CountFreyaRaidNear`, `GetFreyaNaturesFuryShelter` and `GetFreyaSunbeamTarget`.
- `src/Ai/Raid/Uld/Action/UldActions_Freya.{h,cpp}` — two new actions, the camp ring, the bomb leash,
  the tank hold.
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.{h,cpp}` — three new triggers.
- `src/Ai/Raid/Uld/UldActionContext.h`, `src/Ai/Raid/Uld/UldTriggerContext.h` — creator entries.
- `src/Ai/Raid/Uld/UldStrategy.cpp` — three `NextAction` registrations near lines 389-461.
- `docs/raids/ulduar/freya.md` — the Conservator/spore section (the reversal in A), the camp paragraph,
  and a new section for the two 8 yd mechanics carrying the table above. **Governed doc: invoke
  `/compact-docs-writer` before editing it.**
- Plan copied to `docs/plans/freya-eight-yard-splashes/freya-eight-yard-splashes.PLAN.md`.

## Verification

Only step 1 can be done here; the module cannot be linked in this environment.

1. **Static**: `python apps/codestyle/codestyle-cpp.py`, then a per-TU `-fsyntax-only` check of the
   changed translation units against `acore/ac-wotlk-build:master` using
   `/azerothcore/build/compile_commands.json` — mount the working tree's `src` read-only at
   `/azerothcore/modules/mod-playerbots/src`, pull the entry, strip `-c`/`-o`, insert `-fsyntax-only`,
   run from the entry's `directory`; needs `MSYS_NO_PATHCONV=1` in Git Bash.
2. **Build** the worldserver in Docker and confirm the binary's mtime moves past the commit
   (`docker exec ac-worldserver ls -l --time-style=+%F_%R env/dist/bin/worldserver` reports UTC, host is
   UTC+3).
3. **Re-pull Freya 25 hard mode** and measure against `1788717562`:
   - Raid members inside 8 yd, median per living bot: **8.0 → ~5** (the kill's number).
   - Sunbeam: **271k/min → ≤140k**; median victims per volley **12.5 → ≤5**.
   - Nature's Fury: **157k/min → ≤80k**; allies within 8 yd of the carrier, per episode: **median 13-17 →
     ≤2**.
   - Total damage taken: **1,163k/min → ≤950k**.
   - Ground Tremor stays **222-243k/min**. It is the control — if it moves, something else changed.
   - Freya's distance from `_TANK_ANCHOR`: **median 17.6 → ≤10, max 29.8 → ≤20**, and no excursion past
     X 2400.
   - Bots holding Potent Pheromones from a single spore: **18 of 25 → ≤8**.
4. **Guard, and it outranks every number above.** Attuned stacks removed per minute must stay at or
   above this pull's **24.4/min** (kill: 27.9), and the back line's moving share at or below **25%**. If
   either regresses, the Sunbeam step-out (C) is the first thing to cut — it is the change that moves the
   most bots most often — then the camp ring (D). A and B move one bot at a time and are the safe half.
5. `python tools/botobs/postmortem.py <trace>`, plus `--clump 8` and `--stalls`.

## Follow-ups, not in this change

- **Melee eat Sunbeam too** — 383k of this pull's 1.35M. Excluded from C because they cannot leave what
  they are hitting. Worth revisiting only if the melee ball is still taking full volleys after C lands.
- **Two Detonate deaths at 2:33 and 3:20**, both with `freya lasher about to blow` still walking. The
  bail is losing the race on its own, separate from anything here.
- **`freya move to healing spore action` ran 335 OK / 358 FAILED** in this pull — the unlatched
  oscillator recorded in earlier plans, still open.
- **`ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS` is 12.0 in code**; the DBC says `62865 Unstable Energy` is
  `EffectRadiusIndex 8` = 5 yd, which matches what the traces record. Still needs settling.

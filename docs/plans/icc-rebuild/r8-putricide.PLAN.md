# r8-putricide — Professor Putricide

Wave 3d, parallel with `r9-blood-prince-council`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout". It may split.

## Encounter

Slug `professor-putricide`, prefix `pp.`, boss id 6, stem `Putricide`. Core
`boss_professor_putricide.cpp`. Doc `professor-putricide.md`, reader `professor_putricide.py`. Guides
in the cache: Warcraft Tavern 25 and 10, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 1698 lines (`ICCActions_PP.cpp`). The phase comes from health thresholds
  (`IccPutricidePhase`).
- 6 icon writes (`PutricideSetSkull`, 5 callers).
- A hand-rolled flee vector for Gaseous Bloat.
- `IccAddsPutricideMultiplier` vetoes:
  - in the abomination vehicle, everything but the abomination action;
  - for a tank in follow motion, everything but Follow and the goo dodge;
  - with Gaseous Bloat, everything but the cloud and puddle actions (healers exempt);
  - with Unbound Plague, everything but the goo dodge;
  - for the tank with more stacks, everything but movement and the plague action;
  - always: Flee, Formation, Disengage and BlinkBack.
  - One branch can never be true: Volatile Ooze can't also be the goo dodge or the puddle action.
- The gas cloud trigger erases `ppBloatLastDir` inside `IsActive`.
- Cheats:
  - `pp.oozekill` and `pp.gascloudkill` (`IccPutricideVolatileOozeAction`,
    `IccPutricideGasCloudAction`) kill every extra add of the entry;
  - `pp.unboundplague` (`HandleUnboundPlague`) is a suicide with nobody near; the doc notes bots do
    the opposite of passing it;
  - `pp.freeooze` (`TryRegurgitate`) casts Regurgitated Ooze with no energy cost. With the cheat off,
    a cast with no energy still stamps its cooldown and claims the tick.
  - Buffs: `pp.heroicbuff` and `pp.spitefulfury` (keep; `r1` moved them out of the trigger).
- 3 positions.

## Guide answers to build on

- **Volatile Ooze:** everyone stacks on the Adhesive target, and it is the top DPS priority.
- **Gas Cloud:** kited for its whole duration. Gaseous Bloat starts at 10 stacks and loses one per
  tick; at 0 the cloud picks a new target. Also top DPS priority.
- **The abomination:** the off-tank drinks the potion. Eat Ooze gives 4 energy each time a puddle
  shrinks. Regurgitated Ooze costs 45 energy and slows its target 50% for 20 s, good on Gas Clouds.
  Mutated Slash stacks armor reduction on the boss.
- **Mutated Plague:** tank swaps. In phase 3, DPS who can taunt share it so nobody reaches 4 stacks.
- **Heroic Unbound Plague:** a line of ranged DPS passes it along, about 10 s each, 6 players. It
  passes within 5 yd, and Plague Sickness stops passing it back. The guide's alternative is to let
  the carriers die and battle-res them.
- **Heroic intermissions:** an ooze and a gas cloud, with Ooze and Gas Variable splitting the DPS.
  Normal intermissions are 15 s of roleplay.
- Dodge Malleable Goo and the Choking Gas bombs.
- **Setup:** 2-3 tanks, 4-6 healers.

## Focus

All of the above on every difficulty:
- the ooze stack and the cloud kite, each with a priority kill;
- the abomination's puddle-eating duty cycle for its energy;
- the plague swap and sharing;
- the heroic plague pass line and the Variables split.

Replace the four cheats and the family vetoes, and use direct targeting.

## Owns

Stem `Putricide` and its definition, the doc, the reader and its test, this brief.

## Task list

Not split: one state, one scan and one hazard list feed every duty, so the parts don't cut along files.

1. Helpers (`Util/ICCHelpers_Putricide.{h,cpp}`): ids, fixed geometry (room centre, boss spot lattice,
   ring offsets, kite disk), a per-instance `RaidInstanceState` with a 200 ms scan (oozes, clouds,
   puddles with their live radius, bombs with their age, goo impacts with their landing time, the
   abomination, adhesive, bloat and plague carriers, Mutated Plague stacks and the last application),
   duties (boss spot, slots, driver, plague receiver, brake, swap hold), stands, the resolver, hazards,
   kite legs, the swap election, tank defensives, human tank calls, the tick.
2. Rows: dodge, swap, tank defensive, kite, plague pass, ooze stack, abomination, brake stop, target,
   tank, stand. Rules replace `IccAddsPutricideMultiplier`; a brake multiplier holds damage on him.
3. Cheats: `pp.oozekill`, `pp.gascloudkill` (kill order), `pp.unboundplague` (pass line),
   `pp.freeooze` (energy duty cycle) removed; icon writes and the bloat flee vector gone.
4. Hooks for the merge stage: burst window (lust in phase 3), opener boss, shadow resistance.
5. Reader `professor_putricide.py` and its test; doc with the mechanics table.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Boss spot near the room centre, a lattice of 17 navprobed spots; the spot moves only when a puddle (radius + 11 yd) or a bomb (9 yd) fouls it, to the nearest clean one | script: puddles land on 2 random players, bombs within 6 yd of him; WT/Wowhead: drag him off both | Wowhead's spot under the green ooze tank, which leaves the kite and the ooze stack out of heal range |
| Casters and healers on a 12 yd ring (14 slots), hunters and overflow on a 22 yd ring (24 slots), round the boss spot | script: Malleable Goo never picks a player within 7 yd plus both reaches (16.3 yd, 17.5 once he transforms); a hunter's minimum range is 5 yd plus his melee range; slots more than 4.5 yd apart for Unbound Plague's 3 yd pass | guides: ranged spread at range and dodge every goo |
| Gas Cloud: the victim runs legs of 10 yd straight away from it, clamped to a 36 yd disk round the room centre so it follows the wall | WT: kite it the whole bloat; core: the cloud runs 8.05 yd/s against 7; a pure-pursuit simulation catches a ring kite in 10-14 s, a straight run loses 1.05 yd/s | the old scored open-space flee |
| Volatile Ooze: ranged, healers and idle tanks stand within 3.5 yd of the adhesive victim as soon as it is rooted; melee stay on the ooze; the stack ignores puddles and gas | WT/Wowhead: soak with the target; script: erupts 5 yd from the victim, 10 yd round the ooze, damage shared | stack only once the ooze is close |
| Kill order ooze, cloud, boss; of two of one entry the lowest health first | WT/Wowhead: both adds top priority | cloud first |
| Melee leave the cloud by 15 yd and pick something else while its victim has 2 stacks or fewer, or it has no bloated victim | Wowhead: run from it at 2-3 stacks; script: it re-picks a random player at 0 | none |
| Abomination: one bot tank not holding him, assist tank first, drinks after the pull; eats the puddle that crowds the boss spot most; Regurgitated Ooze only with 45 energy, on a cloud first, on an ooze from 90 or once the cloud carries it; Mutated Slash and its swing on the kill target otherwise | WT/Wowhead: off-tank drinks, ooze on clouds; DBC: 45 energy, Eat Ooze +4, no regen | spend on whatever is up |
| Mutated Plague: the holder keeps him for 2 applications, then the eligible carrier with the fewest stacks taunts 1.2 s before the next; a DPS sharer (bot Death Knight or Retribution paladin, 80% health, a physical defensive ready, at most 1 stack, fewer than every tank) takes one with a defensive up and a tank takes him back at once | Wowhead's rotation; `flags_extra` 0x80000: taunt DR, so holds of 20 s keep every taunt at full length | tanks only; the old swap on every application (immune by the fourth taunt) |
| Threat brake on every non-holder while live, 90%/80% of the holder's threat | Festergut's brake, lessons doc | tanks only |
| Unbound Plague: the carrier holds until tick 8 or 50% health, then walks onto a receiver (bot, no Plague Sickness or bounce protection, ranged DPS first, nearest); everyone else keeps 4.5 yd off a carrier | WT: a pass line of about 10 s each; script: 3 yd search after 2 ticks, 5 s bounce protection, Plague Sickness +250% | Wowhead: let the carrier die and battle-res |
| 25H intermissions: Ooze Variable attacks only the ooze, Gas Variable only the cloud | script: 74118/74119 cut damage on the other 100% | none |
| Tank buttons in phase 3 each time a tank takes him and under 40% any phase; class buttons held to 25%; human tank told | WT/Wowhead: defensives for Mutated Strength | also the phase 1-2 swings |
| Choking Gas Bomb: 3 yd gas, plus its 10 yd explosion from 8 s until it despawns at 10 s | DBC: 71279 10 yd on a 10 s period, the summon lives 10 s | the gas only |
| No opening positions: he is pulled at his table and dragged to the spot | overview: each lane decides | stage the rings before the pull |
| Review: the rings let go while anyone but a bot tank holds him more than 15 yd off the spot, back within 10 | lessons doc: a stand held round a boss who moves off; reach is held on the rings, and 15 + 22 yd stays inside a caster's 39 yd reach to him | keep the rings, so a human tank or a chased DPS leaves the casters idle |
| Review: ranged take an add within 30 yd (hunters 35), down from 35 | most casters' spell range; reach is held on a slot | 35 for all, so casters at 30-35 yd stand idle |
| Review: the hold stays at 2 applications | Wowhead: DR clear between swaps | hand over by stack count (never a 4th while anyone has fewer), at more taunts and DR |
| Lust in phase 3, opener hold on him, shadow resistance aura (hook row for the merge stage) | Wowhead | none |

## Carried over

- Merge stage: a `HookRows()` row `{&IccPutricideDefinition, &IccPutricideBurstWindow, &GetPpOpenerBosses,
  &IccPutricideResistSchools, nullptr}` and `#include "ICCHelpers_Putricide.h"` in `Util/ICCSeams.cpp`; until
  then lust, the opener hold and the shadow aura don't run for him.
- Merge stage: the ICC README's cheat table still lists `pp.oozekill`, `pp.gascloudkill`, `pp.unboundplague` and
  `pp.freeooze`, all removed.
- r14: `ppBloatLastDir` and `BloatDir` in `ICCScripts.h`, `NPC_GROWING_OOZE_PUDDLE`, `NPC_CHOKING_GAS_BOMB`,
  `NPC_MALLEABLE_OOZE_STALKER` and `SPELL_GROW_AURA` in `ICCData.h` are unused.
- Never built; CMake needs no re-run (no file added or removed).

## Known gaps

The boss doc's list. Every one waits on a pull or is ruled out offline:

- No 631 trace yet; never built.
- Goo flight timed from where he stands now; impacts older than 9 s dropped.
- Kite has no slow but Regurgitated Ooze; a far-wall kiter can leave heal reach.
- One driver; with one bot tank nobody drinks; a dead 10-man holder is picked up by nobody while the other
  tank rides; the abomination's raid damage isn't weighed.
- Sharers only Death Knights and Retribution paladins; a dead sharer heals him; a sharer stays braked after the
  share until the tank leads its threat by 25%; a human holder is relieved but told only its buttons.
- A hold counts applications: a tank taking him back at 2 holds to 4.
- A plague can jump inside the melee cluster before its carrier steps out; a rooted carrier draws the stack; a
  holder that catches it walks him onto its receiver.
- Phase 3 puddles fill the room, no wall drag; bomb blast timing unconfirmed; the rings form round the centre
  while he is still at the table at the pull.
- Humans hold no duty.

## Blocked

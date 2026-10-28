# r5-saurfang — Deathbringer Saurfang

Wave 3b, parallel with `r4-gunship`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounter

Slug `deathbringer-saurfang`, prefix `dbs.`, boss id 3, stem `Saurfang`. Core
`boss_deathbringer_saurfang.cpp`. Doc `deathbringer-saurfang.md`, reader `deathbringer_saurfang.py`.
Guides in the cache: Warcraft Tavern 25 and 10, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 385 lines (`ICCActions_DBS.cpp`).
- What is handled today:
  - the Rune of Blood swap;
  - beast taunts;
  - a 3×5 ranged grid (`dbs.slot`, where 0 means both slot 0 and erase);
  - the melee skull on beasts (`IccEnsureIconOn`).
- `IccAddsDbsMultiplier` vetoes the AoE family, Army of the Dead, Formation, Follow, Flee and ranged
  ReachSpell. A tank with Rune of Blood gets everything but MovementAction vetoed, taunts included.
- The class taunt reaches one unit per 8 s, so a 25-man wave of five Blood Beasts sits on the raid
  for about 8 s a beast.
- Heroic Scent of Blood is not covered.
- No cheats. One position.

## Guide answers to build on

- 2 tanks swap on Rune of Blood.
- 3-6 healers, one healer per Mark of the Fallen Champion.
- A 12 yd spread for Blood Nova.
- Slows on the beasts, and ranged kill them. No Army of the Dead: it feeds Blood Power.
- Heroic: the beasts generate Blood Power, and a Marked raider's death heals him 20% (5% on normal).

## Focus

- Beast pickup without a taunt loop: a ranged kill duty plus slows, roots and stuns from the kits on
  hand, and Scent of Blood on heroic.
- The Mark heal assignment and the Boiling Blood heals.
- The Blood Nova spread.
- The Rune swap on the real taunt.
- Direct targeting.

## Owns

Stem `Saurfang` and its definition, the doc, the reader and its test, this brief.

## Task list

One implementer, no split: every part shares the helpers' per-instance state.

1. `Util/ICCHelpers_Saurfang.{h,cpp}` (new `.cpp`): ids, the navprobed spots, boss and beast lookup
   (per-instance, 200 ms), roster and slot latch (`dbs.slot`), stands, the resolver (`dbs.target`),
   the rune swap election (`dbs.swap`), the rune yield (`dbs.yield`), the free tank's beast taunt,
   the ranged kill split (`dbs.kill`), the beast control duty (`dbs.cc`), the Mark heal duty
   (`dbs.markheal`), `dbs.phase`, the burst and opener hooks, the tick.
2. Triggers, actions and the definition: rows for swap, yield, beast taunt, control, mark heal,
   target, stand; rules for targeting, stand holds, spell movers, generic taunts, Army of the Dead,
   the yield's pickers, caster form; multipliers for the area-damage hold and the duty paladin's
   beacon (its class nodes carry the spell's name, which pblint can't resolve in a rule). Old icon
   write, grid and `IccTryClassCC` roots go.
3. Reader `deathbringer_saurfang.py` and its test: phases, slots, targets, swaps, kills, control,
   marks, cheats.
4. Boss doc with the mechanics table, this brief.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Tanks stand east of him on the platform, the holder 2.1 yd off his spot, the partner 1.5 yd north of that; he faces east and the raid spreads west | Warcraft Tavern 25 diagram; the old tank spot | Wowhead: tank west, him facing the ship |
| 11 slots 19.5-33 yd west of him, 13.5 yd apart: Blood Nova hits 12 yd round its victim, centre to centre from a creature, plus 0.75 yd park tolerance each (reviewer: 1 yd needed 14 yd, the closest pair is 13.56). 9 sit inside the bots' 32 yd spell reach, 2 outer ones take healers only; overflow stacks with the melee 3.5 yd behind him | guides' 12 yd; 72380 radius; floor | 12.5 yd for 13 slots; a dynamic spread |
| Healers take the central slots first, hunters lead the ranged (no shot from the stack) | reach maths | guid order |
| No opening positions: stands start with his boss state, 17 s before the first Blood Nova | script timers; a pre-pull hold strands bots when the leader leaves | pre-pull stands (he has no aggro radius) |
| Rune swap by the bot tank free of Rune of Blood, taunt ready in its form (bear, Defensive Stance) and in range, lowest guid, when the rune lands on the holder or a non-tank holds him; generic taunts blocked for tanks while live | guides; his `flags_extra` 0x80000 (taunt diminishing returns) | generic taunts on |
| The rune tank not holding him stops hitting him (attack stopped, target cleared, pickers blocked) and takes a beast instead | old code; Algalon's swap-pair lesson | keep hitting (threat flip) |
| Beast taunt: only the free tank, only while its own rune has the taunt cooldown + 0.5 s left or another bot tank free of the rune has its taunt ready, so the next swap is covered; one beast at a time, the one nearest its non-tank victim. On heroic a scented beast lifts the reserve | Wowhead (taunt as last resort); rune timers | the old taunt loop; no taunts |
| Ranged DPS split evenly over the beasts, each on the one furthest from it within 32 yd, sticky while in reach | Wowhead ("by spawn location, the furthest") | nearest |
| Melee hit a beast only once it is in their melee reach | Warcraft Tavern | ignore beasts |
| Beast control without roots: one bot per loose beast, any but the tank holding him (Warcraft Tavern has tanks CC too), best first Hammer of Justice, Concussion Blow, Bash, Chains of Ice, Slow, Concussive Shot, Frost Shock, Curse of Exhaustion, Hamstring, Wing Clip, cast from where it stands. In this core a rooted creature keeps its threat victim, but roots break on their damage cap under focus fire | guides; `ThreatManager::ReselectVictim`; the CC damage cap | roots allowed |
| One bot healer per Mark: holy paladins first with Beacon of Light on the mark (their class beacon held), then holy priest, shaman, druid, discipline priest; heals the mark under 75% unless a tank is under 35% | Wowhead strategy 1 | generic heals |
| Lust waits for Frenzy on every difficulty | both guides | free |
| Opener hold on him | the Marrowgar hook | none |
| Area damage held while live, the AoE picker owned | Resistant Skin (-95% area damage); guides on AoE threat | only while beasts live (reviewer: the guides tie AoE threat to the spawns, and the hold costs Howling Blast, Divine Storm and the like on him between waves) |
| Army of the Dead blocked while his gate is open | both guides | |
| Beasts feed Blood Power on every difficulty; a Mark death heals him 5%, 20% on heroic, as the guides say | `spell_deathbringer_blood_beast_blood_link` on all modes; spelldifficulty 2153 (72260/72278 at 5%, 72279/72280 at 20%) | Warcraft Tavern: no beast Blood Power on normal |
| A player's skull moves DPS and the free tank, never the holder | holder keeps him | skull for all |
| Heroic Scent of Blood keyed on the latest wave's clock, 9-21.5 s: the script scents every live beast, older waves too, 10 s after each wave for 10 s, and his casts can hold the event back. It lifts the beast taunt's swap reserve | script | the aura (72771, radius 0 on the caster, unproven) |
| Heal reach held on slots and the stack (reviewer) | slot 1 reaches every slot and tank spot; a healer walking off its slot breaks the Blood Nova spread | heal reach kept, as `docs/engine/pitfalls.md` advises, so a healer reaches a player off the formation |
## Carried over

- Merge stage: `HookRows()` row `{&IccSaurfangDefinition, &IccSaurfangBurstWindow, &GetDbsOpenerBosses,
  nullptr, nullptr}` and `#include "ICCHelpers_Saurfang.h"` in `Util/ICCSeams.cpp`; until then lust
  and the opener ignore the fight.
- Merge stage or r14: `IccInstanceState::dbsBotSlotMemory` (`Util/ICCScripts.h`) has no reader; the
  stem keeps `dbs.slot` in its own store. `NPC_BLOOD_BEAST1`-`4` (`Util/ICCData.h`) are unused.
  `IccTryClassCC` keeps one caller (Valithria) and still casts roots.
- r14: hoist one stand walk base (this stem's `IccDbsStandAction` and Marrowgar's `IccLmWalkAction`).

## Known gaps

- Those in the boss doc.
- Never built: CMake re-run needed (new `Util/ICCHelpers_Saurfang.cpp`); no `-Werror` pass (the syntax
  check hides warnings).
- Slot sight to him unchecked: the prebuilt navprobe has no `los`.
- A bot counted a tank by spec alone (a Thick Hide cat) takes a tank spot: the ICC stems share that
  tank test (reviewer). Its taunt no longer wins the swap, since the election checks the form.

## Blocked

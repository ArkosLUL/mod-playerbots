# r7-rotface — Rotface

Wave 3c, parallel with `r6-festergut`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounter

Slug `rotface`, prefix `rf.`, boss id 5, stem `Rotface`. Core `boss_rotface.cpp`. Doc `rotface.md`,
reader `rotface.py`. Guides in the cache: Warcraft Tavern 25 and 10, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 1032 lines (`ICCActions_RF.cpp`).
- The most coordinates in ICC: 35 named positions and 3 Z constants, none navprobed. They include
  25 explosion slots and a 22-spot heroic grid.
- What is handled today:
  - the main tank to the centre;
  - the Big Ooze taunt and kite ring;
  - ooze merge routing;
  - the heroic Vile Gas run (the spot resets after 2 s idle).
- One icon write. Nothing holds a Big Ooze but the taunt; heal threat can pull one, and taunts meet
  DR.
- `FindAssignedBigOoze` has no caller.
- `IccRotfaceMultiplier`:
  - with Vile Gas, it vetoes everything but the avoid action;
  - always: TankAssist, Formation, AvoidAoe, Flee (non-hunters), BlinkBack, Army of the Dead, the
    whole cure and cleanse family, the assist tank's AttackRti, TankAssist and all taunts;
  - while a Big Ooze casts its explosion: MovementAction and group position;
  - it calls `AttackStop` and `SetTarget(Empty)` inside `GetValue`.
- The Vile Gas listener in `ICCScripts.cpp` notes `rf.vilegas` on the victim (heroic, through
  `GetSpellIdForDifficulty`).
- No cheats left: `rf.nitro` and `rf.spitefulfury` were removed.

## Guide answers to build on

- 2 tanks: one on the boss, one kiting the Big Ooze.
- An 8 yd spread.
- A Mutated Infection victim's small ooze goes to the kiter.
- The Big Ooze is kited, and everyone shifts 6 yd on the Unstable Ooze Explosion. Move out of Slime
  Spray.
- Heroic: Vile Gas from Putricide, a stronger infection, Ooze Flood slows.
- Setup: 3-5 healers.

## Focus

- The infected raider's walk to the kiter, and dispel timing.
- The kiter's path, computed and navprobed instead of 35 fixed points where possible.
- The Slime Spray, explosion and Ooze Flood dodges on hazard models.
- The Vile Gas spread.
- Replacing the multiplier's family vetoes and the side effect in `GetValue`.
- Direct targeting.

## Owns

Stem `Rotface` and its definition, the doc, the reader and its test, this brief.

## Task list

All done, one lead, no split: the stem shares one state, one hazard model and one tick.

1. Mechanics table in `rotface.md` from `boss_rotface.cpp`, the DBC/DB rows and the guides. Done.
2. State in a stem `RaidInstanceState`: one 200 ms scan of oozes, puddles, explosion marks (each with
   its landing time from the stalker's age and the 5 yd/s flight), the wet stalkers and the spray axis;
   duties every 200 ms. Done.
3. Direct targeting: resolver, `OwnTargeting`, no icon. Done.
4. Main tank stand with the drag; kiter election, ooze spot, pickup taunt, hold under 3 stacks, kite
   legs on a computed 36 yd ring. Done.
5. Carrier walk and the cure duty; class cures blocked. Done.
6. Two-ring spread with flood, puddle and ooze swaps; stack overflow. Done.
7. One forced dodge for spray, marks, puddles, floods, Vile Gas carriers and Big Oozes; stand
   walk-back waits for the spot to clear; hazard hold rule. Done.
8. Tank defensives and human calls; splash hold multiplier; death grip, Army and avoid aoe blocks. Done.
9. Navprobe every ring and spot; reader rewrite with tests. Done.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Spread on two rings 9.5 yd apart, normal included | Warcraft Tavern (everyone 8 yd apart); Vile Gas 8 yd + park | Wowhead normal: stack behind him |
| Healers inner (12 yd), ranged outer (21.5 yd), hunters outer only | Vile Gas picks beyond 14.5 yd when 3 / 8 stand there; his reach puts a hunter's minimum at 15.8 | One ring |
| Kite ring 36 yd, counter-clockwise first, legs of 10-40 degrees | Flood rotation is counter-clockwise (script); 14.5 yd clear of the outer ring | Old 24-34 yd ring, fixed points |
| Ooze spot opposite the last flood | It floods two floods later | Fixed spot |
| Face-tank the Big Ooze under 3 stacks | Wowhead: threat lead while weak | Kite from the pickup |
| One taunt path, skipped at taunt-immune DR; generic taunts blocked for the kiter | `flags_extra` 0x80000 | Old taunt loop |
| Cure at the merge, hold the first, emergency under 35% | Wowhead's order; the script summons on any removal | Never cure (old) |
| Dodge every explosion mark until it lands | Script: stalkers at cast end, 5 yd/s missiles | 25 slots at 40 yd (old) |
| Slime Spray: step out of the cone | Script cone, 2 yd boundary | Stack on his centre (old, inside the boundary) |
| Vile Gas: spread, others step out of 8 yd; carriers don't move | 69248 confuses them 6 s | 40 yd run (old) |
| Main tank drags him back past 4.5 yd | 10.8 yd reach | No drag |
| No kiter without a second bot tank | Humans decision | Main tank kites (old) |
| Holder and kiter skip a player's skull | Each must keep its unit (Saurfang precedent) | Skull wins |
| No opening positions | First hazard at 8 s | Pre-pull stands |
| Old multiplier replaced by rules plus a splash hold | Conventions 11, 31, 32, 35 | Keep the family vetoes |

## Carried over

- From `r0a-restructure`: `spellEntriesFlood` and the commented-out
  `IccRotfaceGroupPositionMultiplier` are deleted with the rewrite.
- For the merge stage or `r14-closeout`: `IccInstanceState::rotfaceVileGasWaitUntil` and
  `rfExplosionSlotMemory` (`rf.explosionslot`) in `Util/ICCScripts.h` have no user; until removed,
  `rotface.py`'s banner lists `rf.explosionslot` as silent. `NPC_PUDDLE`, `NPC_SMALL_OOZE`,
  `NPC_BIG_OOZE`, `SPELL_OOZE_FLOOD`, `SPELL_OOZE_FLOOD_VISUAL`, `SPELL_SLIME_SPRAY` and
  `SPELL_UNSTABLE_OOZE_EXPLOSION` in `ICCData.h` have no user (the stem keeps its own ids).
  `IccRotfaceWalkAction` is a fourth copy of the stand-walk base.

## Known gaps

See `rotface.md`, "Known gaps".

## Blocked

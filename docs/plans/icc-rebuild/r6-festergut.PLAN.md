# r6-festergut — Festergut, Stinky and Precious

Wave 3c, parallel with `r7-rotface`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounters

- **Festergut:** slug `festergut`, prefix `fg.`, boss id 4, stem `Festergut`. Core
  `boss_festergut.cpp`. Doc `festergut.md`, reader `festergut.py`.
- **Stinky and Precious:** slug `stinky-and-precious`, prefix `dogs.`, no boss id (the `r1` unit
  gate), stem `Dogs`. Core `icecrown_citadel.cpp`. Doc `stinky-and-precious.md`, reader
  `stinky_and_precious.py`.
- Guides in the cache:
  - Festergut: Warcraft Tavern 25 and 10, Wowhead if saved;
  - the dogs: the battlechicken blog.

## Facts as of `824468439`

Sites are named by function.

- **Festergut:**
  - The action stem was 530 lines (`ICCActions_FG.cpp`).
  - What is handled today:
    - a tank swap at 6 Gastric Bloat;
    - spore stacking with a raid-wide spot flip;
    - the heroic Malleable Goo dodge;
    - a ranged grid.
  - Vile Gas is answered only by 10 yd spacing. A hand-rolled flee vector sits in the goo dodge.
  - `IccFestergutMultiplier` vetoes Formation, Follow, Flee, Disengage and BlinkBack, plus:
    - a tank at 6 Gastric Bloat: everything but movement;
    - a Gas Spore carrier: all MovementAction, Flee and ReachSpell;
    - the goo-wait window: all MovementAction except the avoid action.
  - The goo-avoid trigger writes `festergutGooWaitUntil` inside `IsActive`.
  - The Malleable Goo listener in `ICCScripts.cpp` notes a 12 yd, 8 s hazard circle per impact.
  - 4 positions.
- **Dogs:**
  - 70 lines: a tank swap at 8 Mortal Wound, and a taunt on adds within 20 yd.
  - `IccDogsMultiplier` vetoes everything but MovementAction for a tank at 8+ stacks, plus Formation
    and Follow.
  - Decimate has no answer.

## Guide answers to build on

- 2 tanks swap at 9 Gastric Bloat.
- 3 spore groups (tanks, melee, ranged) for Inoculated.
- Ranged spread 8 yd for Vile Gas.
- Heroic: Malleable Goo every 10 s.
- Setup: 3-5 healers.
- Dogs, from the blog: a Mortal Wound swap, and heals ready for Decimate.

## Focus

- The spore groups and Inoculated stacks.
- The Gastric Bloat swap threshold (6 now, 9 in the guide: settle it from the script).
- The Vile Gas spread, the Malleable Goo dodge on the hazard model, and defensives for Pungent Blight.
- The dogs' swap and Decimate recovery.
- Direct targeting.

## Owns

Stems `Festergut` and `Dogs` and their definitions, both docs, both readers and their tests, this brief.

## Task list

One lead, no split: the two stems share nothing but the swap shape.

1. Festergut stem, rewritten on `RaidInstanceState<FgState>` (old `fgSlotState` and
   `festergutGooWaitUntil` in `ICCScripts.h` left unused for r14):
   - fight read on his guid slot and combat, not his boss state (moves ~7 s in);
   - navprobed stands: two tank spots, two melee legs, 18 Vile Gas slots, two ranged spore anchors;
     the room is flat at 360.385 out to 45 yd, every stand sees him and both tank spots;
   - spore deal per wave: two groups (tanks + melee at the close anchor, ranged + healers at a far
     anchor), a spare carrier walks to a group with none, a group with none left collapses on the other;
   - Malleable Goo forced dodge on the listener's impacts (5 yd, flight from his balcony), leg and
     far-anchor flips;
   - Gastric Bloat swap at 9 by one elected taunt, generic taunts blocked (taunt DR), threat brake on
     tanks not holding him;
   - tank defensives at Inhaled Blight 2 and 3, under 40%; human tank calls;
   - Pungent Blight raid cover and personal buttons, scaled by Inoculated;
   - direct targeting; shadow resist hook for the merge stage.
2. Dogs stem: direct targeting (free tank on a loose zombie), Mortal Wound swap by stacks, yield,
   Decimate tank button and the Plague Stench cover after it, human tank calls.
3. Readers `festergut.py`, `stinky_and_precious.py` and their tests; both boss docs.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Gastric Bloat swap at 9, taunter at most 7 | WT25, Wowhead; script: 10 explodes, 100 s, 15-17.5 s apart, so any of 7-9 leaves the old aura gone, 6 deadlocks both tanks | 8 for a second taunt try |
| One elected swap taunt, generic taunts blocked while he fights | `flags_extra` 0x80000 (taunt DR) on all four entries; Saurfang precedent | none |
| Threat brake on every tank not holding him, 90 % hold / 80 % free | Wowhead "stop attacking for a while", WT25 off-tank over-aggro; LDW brake figures | full yield until the aura drops (~100 s idle) |
| Two spore groups on every difficulty | WT10, Wowhead (melee and tanks collapse together) | WT25's third tank group in 25-man |
| A group with no carrier collapses on the other, ranged into melee included | script: `DelayEventsToMax(20s)` holds Vile Gas 20 s past every spore | Wowhead's "ranged never collapse into melee" |
| Goo hazard 5 yd (+1.5 trigger margin, +1 park), live until the farthest stand's flight (8.5 s) | DBC 72297 row 2197, creature caster exact radius, Putricide's watch point 66-97 yd away at 12 yd/s | per-impact flight time |
| Pungent Blight hit scaled by the lowest Inoculated stack on the map | `GetRaidCover` ignores aura 229 | raw damage, cover on every cast |
| No pre-pull staging | stands inside his aggro radius would pull him; open round room | opening stands at the door |
| Melee legs at ±135° 6.5 yd, released once he settles 4 yd off his spawn | melee range 10.83 (reach 8 + 1.5 + 4/3) | legs derived from where he settles |
| Dogs: swap at 6 Mortal Wound when the taunter has 4 fewer; the stacked tank yields until its aura drops | script: 15 s, 1.5-2.5 s apart, −10 % healing a stack; blog | fixed 8 |
| Dogs: everyone on the dog, the free tank on loose zombies | script despawns zombies with Precious; blog | zombies first |
| Dogs: Decimate tank button in the cast, raid cover while the dog stuns itself after it | 71123: 3 s cast, everyone to 15 %, 3 s self-stun; Plague Stench 3-4.5k every 3 s | none |

## Carried over

- Merge stage: a `HookRows()` row `{&IccFestergutDefinition, nullptr, nullptr, &IccFestergutResistSchools,
  nullptr}` (include `ICCHelpers_Festergut.h`), so a paladin raises Shadow Resistance Aura and Aura
  Mastery can cover Pungent Blight.
- Merge stage, ICC README: the trigger-name list (`icc dogs` is now `icc dogs` tank defensive, decimate
  cover, swap, yield, target; Festergut's are goo, pungent defensive, pungent cover, tank defensive,
  bloat swap, brake, target, stand), the direct-targeting user list (Festergut `GetFgTarget`, the dogs
  `GetDogsTarget`), the defensives user list (Festergut tank ladder and Pungent Blight cover and
  buttons, the dogs' Decimate button and cover), "Before live" (Festergut: every row, rule and the
  tick from his engage, keyed on his combat; only the brake multiplier waits for live, so
  "every multiplier is live but Festergut's and Rotface's" loses Festergut), the class-taunt paragraph
  (Festergut and the dogs swap by one elected taunt with generic taunts blocked; the dogs' add taunts are
  gone), the spell-listener note (the 12 yd, 8 s goo circle is no
  longer Festergut's model, which is 5 yd for 8.5 s), "Dogs has no helpers" (it has now).
- r14: `fgSlotState`, `festergutGooWaitUntil`, `SpreadSlotState` in `ICCScripts.h` and the 12 yd note
  in `ICCScripts.cpp` are unused by Festergut now; hoist one `TauntUsable`/`TauntName`/`TauntSpellId`
  (Saurfang, Festergut, Dogs) and one walk base (Marrowgar, Saurfang, Festergut).
- CMake re-run: `Util/ICCHelpers_Dogs.cpp` is new.

## Known gaps

- Never built; no 631 trace; no `-Werror` pass.
- Festergut settled more than 3.5 yd off his spawn: melee fall to the generic movers, nobody walks him
  back. A goo on the close stack in a spore window can cost the tanks a stack. Healers walk to heal the
  far slots. Melee fill counts bots only. Humans hold no duty.
- Dogs: eleven zombies a wave on Precious go to the free tank and incidental area damage only; a free
  tank on the dog can still rip it by threat.

## Blocked

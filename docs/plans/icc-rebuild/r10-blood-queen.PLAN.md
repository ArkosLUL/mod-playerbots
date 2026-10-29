# r10-blood-queen — Blood-Queen Lana'thel

Wave 3e, parallel with `r11-svalna-valithria`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounter

Slug `blood-queen-lana-thel`, prefix `bql.`, boss id 8, stem `BloodQueen`. Core
`boss_blood_queen_lana_thel.cpp`. Doc `blood-queen-lana-thel.md`, reader `blood_queen_lana_thel.py`.
Guides in the cache: Warcraft Tavern 25 and 10, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 1286 lines (`ICCActions_BQL.cpp`).
- The phase comes from her passive state and gravity flag (`IccBqlPhaseOf`).
- What is handled today:
  - 19 ground slots and 73 air slots;
  - defensives on Bloodbolt Whirl;
  - Swarming Shadows lanes;
  - bite picks and the Pact meeting.
- 9 positions and 9 Z constants, none navprobed. `bql.curve`, `bql.slot` and `bql.airslot` write 0
  for both slot 0 and an erase.
- A hand-rolled flee vector.
- `IccBqlMultiplier`:
  - ranged: AvoidAoe, Flee, Formation and Disengage;
  - Pact: everything but the pact action;
  - Frenzy: everything but the bite;
  - Swarming Shadows: everything but group position;
  - the air phase: reach and Flee.
- Own-aura cancels are not cheats. No cheats left (`bql.swarmingshadows` was removed).

## Guide answers to build on

- **Bites:** a set order that never bites healers.
- **Pact of the Darkfallen:** a central gathering spot.
- **Swarming Shadows:** go to the room's edge, never near the Pact spot.
- **Twilight Bloodbolt:** a 6 yd spread, melee in 2-3 small groups.
- **Incite Terror:** Fear Ward and Tremor Totem.
- **Blood Mirror:** the off-tank in stamina gear.
- **Heroic:** Presence of the Darkfallen, +5% Shroud of Sorrow per vampire.
- **Setup:** 2 tanks, 4-6 healers.

## Focus

- A bite order duty: DPS first, never healers, plus the bitten raider's next bite before Frenzied
  Bloodthirst ends, and CC for an Uncontrollable Frenzy victim, human or bot.
- The Pact gather, and Shadows lanes to the edge on hazard models.
- The spread and melee groups.
- Fear protection through `RaidAntiFear`, and the air phase.
- Slots derived from geometry where possible, navprobed otherwise.

## Owns

Stem `BloodQueen` and its definition, the doc, the reader and its test, this brief.

## Task list

One lead, no split: the stem shares one state, one shadow scan and one tick.

1. Rewrite the stem on one per-instance state (`bql.*`): her unit by the instance's guid slot 8, one
   shadow scan per 200 ms, the tick doing every assignment every 250 ms.
2. Layout, navprobed with `--nav 0x09` and sight-checked: a tank spot on the dais 3.5 yd from its edge,
   one 7.5 yd lattice (42 nodes within 25 yd of the room centre, 27 usable on the ground), three Pact
   spots 6 yd south of the centre, two Shadows lanes (35.5 and 30.5 yd) from 40° to 320°.
3. Tanks by roster (`bql.tank`): main holds her at the tank spot all fight, mirror stands on the main
   tank (on a human main tank if the group's main tank is one), spares fight in the melee groups.
   Recovery taunt below DR level 3, generic taunts and Death Grip blocked.
4. Direct targeting `GetBqlTarget`: a player's skull, else her. Threat brake 90%/80% on non-holders
   (`bql.brake`); non-main tanks held until the first vampire exists.
5. Stands: healers and ranged on ground nodes, melee in 1-3 groups 7.5 yd round her latched position,
   melee, spare and mirror tanks on air nodes from her walk to the centre until Bloodbolt Whirl ends.
6. Bite duty (`bql.bite`): each frenzied bot gets a target, DPS first, then non-holding tanks, then
   healers, never her victim; bite cast by id. CC duty (`bql.cc`) for an Uncontrollable Frenzy victim.
7. Pact walk to the latched spot (`bql.pact`); Swarming Shadows lane walk (`bql.curve`); shadow dodge
   for everyone else (`bql.dodge`).
8. Anti-fear on Incite Terror (`RaidAntiFear`), Bloodbolt Whirl raid cover and personal defensives,
   tank buttons under 40% and in the last minute, class buttons held to 25%, human tank calls.
9. Hooks for the merge stage: burst (lust at the vampire peak), opener (her), resist (shadow).
10. Reader and tests for the new keys, the boss doc with the mechanics table.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Pact spot 6 yd south of the room centre, two fallbacks 8 yd either side | WT/Wowhead: a pre-picked spot towards the middle; script: 71341 hits unlinked allies within 10 yd of a carrier | the centre medallion itself: 14 caster and 7 healer nodes on the same lattice instead of 17 and 11 |
| Spread at 7.5 yd = 6 yd Bloodbolt Splash + two park tolerances, the same lattice on the ground and in the air | script: 71447 6 yd; WT: 6 yd spread | rings round the centre (old air slots), which can't hold the ground constraints |
| Melee in up to 3 groups 7.5 yd round her (back and ±60°), re-latched past 1.5 yd drift | WT: 2-3 small melee stacks | one stack behind her (generic) |
| The mirror tank stands on the main tank and never attacks her before the first vampire | script: Blood Mirror takes the player nearest her victim; her bite takes the highest-threat non-victim, the off-tank included | let the off-tank attack and risk the first bite |
| Bite order DPS, then non-holding tanks, then healers, never her victim, nearest first, targets above 15k health first | WT/Wowhead: no healers (they gain nothing); WT 10-man counts 8 vampires by 3:30, which needs tanks or healers; 70871 sets threat ×0, so a vampire main tank stops holding | never healers (an Uncontrollable Frenzy instead) |
| CC the Uncontrollable Frenzy victim (Polymorph, Cyclone, Hex, Repentance, Hammer of Justice, Blind, Fear, Scatter Shot), not kill it | brief focus; guides: mind controlled until killed | DPS kill it |
| Lust once vampires reach the round's peak (8 on 10-man, 16 on 25-man) or 280 s in | Wowhead: lust at 16 vampires | lust at the pull |
| Tank class buttons held to 25%; our node presses under 40% and in the last 60 s before Berserk | Wowhead: save tank cooldowns for the end; no predictable tank hit | free class buttons all fight |
| Tanks stay at the tank spot through the air phase; the mirror tank steps onto an air node until the Whirl ends | WT: tanks stay put so she returns; Whirl splashes 6 yd | both tanks stacked through the Whirl |
| Shadows lanes as a horseshoe 40°-320° through the south end, outer lane first | WT/Wowhead: run to the edges, never near the Pact spot | the old Bézier lanes along the two side walls only |
| No opening positions | her 23 yd aggro; the gate is open everywhere between pulls | pre-pull stands |
| Healers hold their nodes against heal reach (resurrect reach passes) | Saurfang and council precedent; every healer node covers the tank spot | heal reach free |
| The bite outranks a shadows trail, which outranks the Pact, when the script stacks them on one bot | the script excludes Pact members from Shadows but not frenzied bots from either; each hold blocks the other's action | nothing (the bot stood still) |

All tasks done.

## Carried over

- Merge stage, `HookRows()`: `{&IccBloodQueenDefinition, &IccBloodQueenBurstWindow, &GetBqlOpenerBosses,
  &IccBloodQueenResistSchools, nullptr}`, in definition order.
- ICC README: BloodQueen joins the direct-targeting users (`GetBqlTarget`) and the defensive users
  (`icc bql tank defensive`: any tank under 40% and her holder and the mirror tank in the last minute,
  class buttons held to 25%, `bql.defensive`, `bql.humancall`; `icc bql whirl cover` and `icc bql whirl
  defensive` on `icc.cover`/`icc.personal`; `icc bql brake`, a `LiveMultiplier` holding non-holders at
  90% of the holder's threat, frees at 80%); its stem keeps `bql.tank`, `bql.slot`, `bql.airslot`,
  `bql.group`, `bql.bite`, `bql.cc`, `bql.curve` and `bql.brake` in its own `RaidInstanceState`, and
  `IccInstanceState`'s three `bql.*` containers join the unused list; trigger names `icc bql` dodge,
  vampiric bite, swarming shadows, pact of darkfallen, whirl defensive, whirl cover, tank defensive, anti
  fear, frenzy cc, taunt, brake, target, stand; drop the Blood-Queen from "Targeting is by raid icon
  outside".
- `docs/raids/README.md` "Anti-fear": the Blood-Queen's Incite Terror joins the consumers.
- r14: unused `bqlBotCurrentCurve`, `bqlAirSlotMemory`, `bqlBotSlotMemory` (`ICCScripts.h`) and
  `NPC_SWARMING_SHADOWS`, `SPELL_PACT_OF_THE_DARKFALLEN`, `SPELL_BLOODBOLT_WHIRL` (`ICCData.h`); hoist one
  walk base.

## Known gaps

In the boss doc's Known gaps. Never built, no 631 trace, no `-Werror` pass beyond `pb-check`.

## Blocked

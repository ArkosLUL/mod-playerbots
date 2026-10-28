# g5-saurfang — close Deathbringer Saurfang's gaps

Gap wave `3ab-gaps`, after `3b`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap lanes"
section included. Builds on the merged `r5-saurfang` lane and its review: read
[r5-saurfang.PLAN.md](r5-saurfang.PLAN.md) first. Its decisions stand unless closing a gap needs a
change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/deathbringer-saurfang.md` and in r5's
brief, plus the two every gap lane does (tank defensives, line of sight).

- Tank defensives: the doc names high Blood Power and Frenzy; check the script for every other
  predictable tank spike, Rune of Blood included.
- Line of sight: all 11 slots, both tank spots and the stack against him, and the healer slots
  against the tank spots.
- "A bot counted a tank by spec alone (a Thick Hide cat)" is not a gap: the user ruled it out. The spec
  test is the shared `PlayerbotAI::IsTank` (`src/Bot/PlayerbotAI.cpp`), not ICC code, and both cat
  presets in `conf/playerbots.conf.dist` (`PremadeSpecLink.11.3.*` and `.11.5.*`) put no point in
  Thick Hide. Remove the bullet from the doc and r5's brief, put the README's matching line in
  `readmeNotes`, and leave the ICC tank test as it is.

## Owns

Stem `Saurfang` and its definition, the doc, the reader and its test, this brief.

## Task list

One lead, no split: every change sits in the stem's helpers and per-instance state.

1. [x] Line of sight: navprobe `los` and `ring --los-from` from all 11 slots, the stack and the tank spots
   to him (up to 2.5 yd off his spot) and to the three tank spots, 144 rays. Slots 10 and 11 (33 yd
   north and south) saw him but no tank spot: moved onto the east causeway. Every stand re-probed for
   floor and path.
2. [x] Tank defensives (convention 43): the holder spends a physical-only button from Blood Power 70
   (40 with a Mark) once per cycle and through Frenzy, any tank under 40%, class nodes held to 25%; a human
   holder is called; probe `dbs.defensive`.
3. [x] Marked bots' own buttons from Blood Power 70 and in Frenzy through `GetRaidPersonalDefensive`
   (`icc.personal`).
4. [x] Hand of Protection on a marked bot with Boiling Blood: paladin duty `dbs.protect`.
5. [x] Human tank partner told to taunt (`dbs.humancall`).
6. [x] Blood Nova victims: melee fill the slots when fewer than 4 (10) / 10 (25) bots hold one.
7. [x] Reader `--defensives`, `--protect`; tests; boss doc.
8. [x] Thick Hide cat bullet removed from the doc and r5's brief; the README line goes to `readmeNotes`.
9. Closed as decisions, with reasons in the boss doc: Frost Trap and Earthbind Totem at his feet,
   knockbacks, Divine Intervention and hearthstone lines, kiting, the stack overflow.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Slots 10 and 11 move to the east causeway, (-478.85, 2211.33) and (-465.35, 2211.33), z 541.11, 17.5 and 31 yd out; both now take DPS too | navprobe `los`: from 33 yd north or south of him the platform edge hides the tank spots while his own spot stays visible (on a 33 yd ring blind from 80° to 95° and from 265° to 285°, clear from 100° and 260° out); the old healer-only slots were healers 4 and 5, who never saw a tank. First robust west spots (-502, 2248 / 2175) sit 37-39 yd from the tank spots, past the 38.5 yd heal reach of the far one | Keep 33 yd; 37 yd slots |
| Tank ladder: the holder one button at Blood Power 70, all of them in Frenzy, any tank under 40%; class nodes held until 25% | Wowhead tank tips (defensives from 70, majors for Frenzy); convention 43; everything he does is physical and Blood Power is +1% a point; g2's shape | A button per Rune of Blood (the swap ends it within a swing or two) |
| A Mark on the holder opens its window at Blood Power 40 | the splash lands on the tank every swing and Blood Power scales it | Same 70 |
| Marked non-tank bots ask `GetRaidPersonalDefensive` about 4 s of splash at the current Blood Power (1.3 swings a second in Frenzy) | Wowhead tips for the marked (personal cooldowns at 70+ and in Frenzy); the shared pick adds an immunity or a healthstone when the model reads lethal, which a Mark's death (boss +5%, +20% heroic) earns | Class buttons only (`NextPhysicalDefensive`) |
| Hand of Protection only on bots that fight with spells (no melee, hunter or tank) | Wowhead (on Boiling Blood, preferably also marked); 1022/10278 is physical immunity plus pacify, and 72255 and 72385 carry no piercing attribute | Any marked victim; the Divine Intervention and Hearthstone lines (cheese) |
| Melee fill free inner slots while fewer bots than Blood Nova's minimum hold one | `spell_deathbringer_blood_nova_targeting`: victim among players off his edge, else the farthest 4 (10) / 10 (25) | Let the stack take the Nova |
| A human holder is called at the windows, a human partner to taunt when the rune sits on a bot holder with no bot tank free | convention 43 and the Humans decision (a call, no duty); `CallHumanTankDefensive` carries a taunt id | No call |
| No Frost Trap, Earthbind Totem or knockback duty, no kiting, no Divine Intervention or hearthstone line, no stack relocation | traps and totems drop at the caster's feet and anyone at his feet breaks the spread; Scent slows the raid 80% and the slots hold the Nova spread; the lines take a player out of the fight; the floor and causeway hold no more stands 13.5 yd apart | Duties for each |
| The Thick Hide bullet is not a gap | the user's ruling: `PlayerbotAI::IsTank` is shared and both cat presets spend no point in Thick Hide | |

## Carried over

- Merge stage: `HookRows()` row and `#include "ICCHelpers_Saurfang.h"` in `Util/ICCSeams.cpp` (from r5, unchanged).
- Merge stage or r14: `IccInstanceState::dbsBotSlotMemory` has no reader; `NPC_BLOOD_BEAST1`-`4` are unused;
  `IccTryClassCC` keeps one caller (Valithria); hoist one stand walk base (all from r5).
- Merge stage, README: drop the Known gaps bullet on the shared tank test and Thick Hide cats; the defensives
  seam bullet gains Saurfang (`icc dbs tank defensive`, `icc dbs mark defensive` on `icc.personal`, calls to a
  human tank); the per-instance state paragraph gains `dbs.protect` beside `dbs.markheal`.

## Known gaps

- Those in the boss doc.
- No 631 trace; never built (no new `.cpp`, no CMake re-run); no `-Werror` pass.
- Healers' external cooldowns (Pain Suppression, Guardian Spirit, Hand of Sacrifice) are not dealt: no shared
  dealer for a single-target cooldown.
- The tank ladder keeps no major cooldown back for Frenzy (Wowhead): `--defensives` says what each window spent.
- A marked mage, paladin or priest that the model reads as lethal takes its class immunity or a healthstone,
  even when its healer would have held. `icc.personal` in the next pull says how often.
- Gameobject collision isn't in navprobe's sight.

## Blocked

# r13-lich-king — The Lich King

Wave 3g, alone. Rules, conventions and the boss lane recipe: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md).
Layout and seams: `docs/raids/icecrown-citadel/README.md`, "Code layout". It splits: phase 1 and the
transitions, phase 2, phase 3 with Frostmourne are natural groups.

## Encounter

Slug `the-lich-king`, prefix `lk.`, boss id 12, stem `LichKing`. Core `boss_the_lich_king.cpp`. Doc
`the-lich-king.md`, reader `the_lich_king.py`. Guides in the cache: Warcraft Tavern 25 and 10,
instant-carry's raid overview, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 4454 lines (`ICCActions_LK.cpp`).
- The phase is stateless, from his react state and the winter row (`IccLichKingPhase`,
  `IccRemorselessWinterOf`). It lags up to 2 s at thresholds.
- 19 icon writes and 14 reads (`LkSetIcon`). The Val'kyr bucket keeps its `rti`.
- A hand-rolled flee vector.
- `IccLichKingAddsMultiplier`:
  - in the Terenas room: melee and warlocks get only movement and the adds action;
  - cures until the plague is delivered;
  - always: Formation, Follow, Blink, Disengage and Flee;
  - phase 1: the AoE family and assist-tank targeting;
  - during a Winter cast: heals only for non-tanks;
  - a sphere victim: all MovementAction;
  - ranged movement while Defile is up.
- `IccLichKingSpiritBombMultiplier` vetoes everything but the bomb action and `SetFacing` under bomb
  threat.
- Cheats:
  - `lk.ragingspiritkill` (`IccLichKingWinterAction`) kills the fullest Raging Spirit past two;
  - `lk.wickedspiritkill` (`HandleSpiritMarkingAndTargeting`) kills the starred Wicked Spirit;
  - `lk.hoj` (`HandleAssistTankAddManagement`) casts Hammer of Justice on Shamblings from any class;
  - `lk.frostmourneescape` (`HandleTeleportationFixes`, normal) snaps a Harvest Soul victim out of
    Frostmourne on its first tick, skipping the Spirit Warden.
  - Buffs `lk.heroicbuff`, `lk.empoweredblood` and `lk.valkyrstun` stay.
- Borderline: in `IccLichKingAddsAction` the Val'kyr edge dive puts the carry aura on itself and
  suicides at the bottom.
- Unstucks through `NearTeleportTo` (6 sites). Among them, a 1 yd platform snap cancels knockbacks
  mid-flight and re-snaps fallen bots; it waits on a navprobe of the Frozen Throne and the cheat
  rule.
- Only the main tank dodges Shadow Traps. Heroic Harvest Souls are only partly handled.
- The Defile listener in `ICCScripts.cpp` notes `lk.defile` and a 6 yd hazard for the 2 s cast.
- 14 positions and 5 Z constants.

## Guide answers to build on

- **Phase 1:**
  - Dispel Necrotic Plague next to the adds so it kills them.
  - Shambling Horrors: the off-tank faces Shockwave away. Dispel Enrage with Tranquilizing Shot or
    Anesthetic Poison, and keep the horror stunned through Frenzy.
  - Shadow Trap is heroic-only (the script).
- **Transitions:**
  - A tank picks up each Raging Spirit at once and faces it away, and the raid burns it before the
    next phase. Its Soul Shriek silence is dispellable.
  - Ranged kill the Ice Spheres.
- **Phase 2:**
  - Val'kyr take a raider to the nearest edge, so the Lich King is tanked in the centre and the raid
    stacks tight. They can be stunned and slowed to 50%, not rooted.
  - Heroic: they drop the raider at 50%, cast Life Siphon, and must die before the next grab.
  - Keep Val'kyr out of Defile; Defile is run out of the raid. Soul Reaper is a tank swap.
- **Phase 3:**
  - Vile Spirits are set off by a tank, Dispersion or Divine Shield.
  - Harvest Soul on normal: the victim fights the Spirit Warden with Terenas, keeps Terenas alive, and
    interrupts or dispels Soul Rip. On failure Destroy Soul kills the victim and the Lich King gets
    +200% damage.
  - Heroic: the whole raid enters Frostmourne for about 40 s. Ranged kill the Wicked Spirits early,
    Spirit Bombs repeat at the same spot, and a death inside wipes the raid (Harvested Soul).
- **Setup:** 2 tanks, 4-7 healers.

## Focus

- Every mechanic above on every difficulty, with direct targeting.
- A stun rotation over the raid's kits and the Enrage dispel duty.
- A Raging Spirit tank pickup and burn.
- Val'kyr stun, slow and kill duties.
- The victim's Frostmourne fight and the heroic Frostmourne raid.
- The knockback question decided by navprobe and the cheat rule.
- Replacing the four cheats and the dive suicide.

## Owns

Stem `LichKing` and its definition, the second stem `LichKingLate` this lane adds for phases two and
three (new files only), the doc, the reader and its test, this brief.

## Task list

Split into three groups on disjoint files plus a finisher. A group edits only its own files, runs no
checks, builds nothing and commits nothing, and reports the rows, keys and seams it added. Ids, timers
and radii: the mechanics table in `docs/raids/icecrown-citadel/the-lich-king.md` (its later sections
describe the old code). Reference patterns: `Util/ICCHelpers_Sindragosa.cpp` (one `RaidInstanceState`,
200 ms scan and 250 ms duty ticks, latched stands, brake, tank roster, per-bot per-ms resolver memo) and
`IccSindragosaBossWalkAction` (`Action/ICCActions_Sindragosa.cpp`); `CONTROL_SPELLS` in
`Util/ICCHelpers_Saurfang.cpp`; `icc rotface self cure` in `Multiplier/ICCMultipliers_Rotface.cpp` (cure
classes vetoed by concrete class); `EncounterHelpers` (`FindNearestPositionClearOfHazards` with `accept`,
`UseNitroBoosts`, `NitroBoostsReady`). All paths below are under `src/Ai/Raid/ICC/`.

**Every group.**

- Trigger and action names are `icc lich king <x>`, each class with `static constexpr char const* Name`.
  No icon writes, no `rti` or `rti cc`; humans hold no duty; a player's skull wins.
- Probe keys exactly as listed, the literal key in `NoteDerived(bot, "lk.x", value)` or an `Obs*`
  declaration (`tools/botobs/raidobs/probes.py` parses those two shapes); per-bot keys change-only,
  `none` when the duty ends.
- Spell ids remapped at the call site (`sSpellMgr->GetSpellIdForDifficulty`); raid size from the map.
- Geometry: polar from C (503.63, -2124.66, 840.857), θ from +x (north) toward +y (west), r in yd.
  Navprobe can't read the Frozen Throne or Frostmourne (GO floors, doc's mechanics section), so nothing
  is probed: every spot stays on the always-solid floor, r ≤ 44, except winter ring slots (r 49-62)
  while the platform isn't `GO_DESTRUCTIBLE_DAMAGED` (GO from instance guid slot 38,
  `GetDestructibleState()`). Every hazard sweep's `accept` rejects points off that floor or more than 2
  yd off Z 840.857. Frostmourne spots stay within 20 yd of F (495.71, -2523.76, 1049.95), the heroic
  teleport's own spread.
- Movers derive group 1's `IccLkWalkAction`.

**Group 1, `lk-ground`**: `Util/ICCHelpers_LichKing.{h,cpp}`, `Trigger/ICCTriggers_LichKing.{h,cpp}`,
`Action/ICCActions_LichKing.{h,cpp}`, `Multiplier/ICCMultipliers_LichKing.{h,cpp}`. Rewrite: the old code
goes wholesale.

1. Core API in `ICCHelpers_LichKing.h`, which group 2 builds on, with these names:
   - `Creature* GetLkBoss(PlayerbotAI*)`: guid slot 12, alive, the bot on map 631 within 120 yd of C or
     50 yd of F. `bool IsLkLive(PlayerbotAI*)`: he is in combat or in the outro.
   - `IccLichKingPhase` and the winter readers stay; the phase gains `Outro` (`IsCombatDisallowed()`,
     noted `outro`).
   - `LK_CENTER`, `LK_FROSTMOURNE`, `bool IsLkRingStandable(Player*)`, `bool LkFloorAccepts(Player*,
     float x, float y)`.
   - `enum class LkTankRole : uint8 { None, Holder, Free }`, `LkTankRole GetLkTankRole(PlayerbotAI*,
     Player*)`, `Player* GetLkHolder(PlayerbotAI*)`, `Player* GetLkFreeTank(PlayerbotAI*)`.
   - `std::vector<Player*> LkMembersOnMap(Player*)`, `bool IsLkDutyBot(Player*)`, `bool
     IsLkTankPlayer(Player*)`, `bool IsLkHealer(Player*)`.
   - `uint32 LkReadyStun(Player*, Unit*)`, `uint32 LkReadySlow(Player*, Unit*)`: the id the bot can
     land now (known, cooldown, cost, form via `CheckShapeshift`, range, sight; a stun only below the
     target's immune DR level), else 0. Stuns: Hammer of Justice, Holy Wrath (all LK adds are undead),
     Concussion Blow, Shockwave, Bash, Intimidation, Gnaw, Shadowfury, War Stomp, Kidney Shot. Slows: the
     snares of `CONTROL_SPELLS` plus Frostbolt and Piercing Howl.
   - `Unit* GetLkTarget(PlayerbotAI*)`, `bool IsLkTargetHeld(PlayerbotAI*)`, `bool
     IsLkMovementOwned(PlayerbotAI*)`, `bool IsLkTankDefensiveHeld(PlayerbotAI*)`, `void
     TickLkEncounter(PlayerbotAI*)`, the walk getters.
   - `IccLkWalkAction` in `ICCActions_LichKing.h`, the shape of `IccSindragosaBossWalkAction`.
   - Hooks: `IccLichKingBurstWindow` (lust in phase one), `GetLkOpenerBosses` ({36597}),
     `IccLichKingResistSchools` (shadow and frost).
2. State: one `RaidInstanceState`; scan every 200 ms within 120 yd of C for ghouls 37695, Shamblings
   37698, Raging Spirits 36701, Ice Spheres 36633 and Shadow Traps 39137 (arming while 73530 is on it);
   duties every 250 ms; reset once he is out of combat and not in the outro.
3. Tick housekeeping: heroic with `EnableICCBuffs` while he is in combat, every member
   `IccApplyHeroicBuffToMember(bot, member, true, true, "lk.heroicbuff")` and Empowered Blood 70227 at
   60-40% (`lk.empoweredblood`, `IccNoteCheat`). The floor snap `lk.floorsnap` (`IccRaidCheat`, asked
   last): a bot under Z 838 within 44 yd of C, or under Z 1046 within 38 yd of F, `NearTeleportTo` in
   place at 840.857 or 1049.95. Nothing else teleports.
4. Tanks, `lk.tank` (`ObsGuidMap<uint8>`, 1 holder, 2 free): the holder is his victim when a group
   tank, else the last holder while it lives, else the group's main tank; free is the first other bot
   tank by guid. Elected taunts, `lk.taunt` per tank (`back`, `add`, `spirit`, `reaper` and the unit),
   through `IccCastClassTaunt`, his taunt DR read before each:
   - the holder waits at (180°, 14), in taunt range of his intro stop, taunts him as the intro ends, and
     taunts him back off anyone else;
   - the free tank takes each phase one add as its 5 s emerge ends (Challenging Shout 1161 or
     Challenging Roar 5209 when two or more within 10 yd hit others);
   - in the winters both tanks take each Raging Spirit as it unroots (the one holding fewer, then guid),
     and any spirit on a non-tank; leftovers in phases two and three stay on the free tank;
   - Soul Reaper on the holder with no button ready for it (`NextTankDefensive` gives 0): the free bot
     tank taunts at once and the roles flip.
5. Tank buttons (`NextTankDefensive` `lk.defensive`, `CallHumanTankDefensive` `lk.humancall`): the
   holder from 15 Plague Siphon stacks, on each Soul Reaper landing, the add tank on an Enrage no hunter
   answers, any tank under 40%; class buttons held to 25%.
6. Brake `lk.brake` (`ObsGuidSet`): non-holders at 90% of the holder's threat on him, freed at 80%, in
   phases one, two and three.
7. Phase one stands, `lk.stand` per bot (`opening`, `holder`, `addtank`, `ranged <n>`, `winter <cluster>
   <n>`, `spirit`, `quake`, `none`), latched (park 1 yd, walk back past 2.5): holder L1 (180°, 12), he
   faces it; melee on generic reach; add pile A (90°, 17) with the add tank at AT (90°, 22), so the adds
   sit between it and the raid and Shockwave points west off everyone; ranged and healers on a
   triangular lattice round R1 (0°, 8), 6 yd spacing, nodes within 12 yd of R1, at least 12 yd from A and
   at most 28 from L1, by guid. Before live, while he walks the intro, the same stands (holder at
   (180°, 14)). A trap fouling a stand re-latches it to the nearest clear spot.
8. Necrotic Plague, `lk.plague` per carrier (`walk <add>`, `delivered <add>`, `isolate`, `none`) and
   `lk.cure` per curer (`delivered`, `addtank`, `isolated`, `emergency` and the carrier):
   - a carrier walks (forced, interrupting, Nitro Boosts past 15 yd) to 2 yd off the nearest pile add on
     the side away from AT, its path clear of traps, and parks;
   - one curer per carrier (priest Cure or Abolish Disease, paladin Cleanse, shaman Cleanse Spirit or
     Cure Toxins; in 30 yd and sight with it ready; healers first, then guid) dispels once the carrier's
     nearest live unit within 10 yd (members and adds, not him) is an add, at once with under 1 s to
     the next tick (`AuraEffect::GetPeriodicTimer`) or the carrier under 35%, and the add tank's plague
     whenever its nearest unit is an add;
   - with no add alive in phase one, and in the winters: the carrier walks 12 yd off every member and
     the curer dispels there;
   - class cure nodes, Cleansing Totem and Stoneform are held off a carrier (multiplier on the concrete
     classes).
9. Shamblings: Tranquilizing Shot 19801 on each Enrage 72143 from the nearest ready hunter, then guid
   (`lk.enrage` per hunter); heroic Frenzy 28747: one stun in flight from `LkReadyStun`, the next
   stunner when it has under 0.5 s left (`lk.stun`, `ObsGuidMap<ObjectGuid>` bot to unit), and melee DPS
   finish it.
10. Heroic Shadow Traps, `lk.trap` (`dodge <trap>`, `none`): a bot within 10.5 yd of a trap walks
    (forced, interrupting) to the nearest spot 11 yd off every trap (review: the throw reaches 10 yd);
    every walk's `accept` refuses a segment within 3.5 yd of one.
11. Multiplier `icc lich king splash`: in phase one, area spells (`SpellSplashes`) hitting within 12 yd
    of a live ghoul or Shambling are held.
12. Winters (`winter1`, `winter2`):
    - On his turn to passive everyone walks (forced, Nitro Boosts past 15 yd) to a slot on two arcs, r 51
      and 57, 6 yd apart along the arc. The tank cluster at 0° takes the tanks, melee and half the healers
      (rounded up; on 10N all of them); the ranged cluster sits at the half-width of the remapped Pain and
      Suffering cone plus 8° (20.5° on 10N, 60° otherwise) and takes the rest. While the platform is
      `DAMAGED`, hold at r 42 on the slot's bearing.
    - Tanks hold their spirits at (∓7°, 62), spirit inward so Soul Shriek points off the platform; melee
      on its inner side. Priests (Dispel Magic) and paladins (Cleanse) dispel Soul Shriek's silence,
      healers first (`lk.silence <member>`).
    - The add tank brings leftover adds to its spirit spot.
    - On the Quake cast every bot past 40 yd walks (forced, interrupting, Nitro Boosts) to r 38 on its
      bearing and holds there until he turns aggressive.
13. Resolver `GetLkTarget`, `lk.target` (rule and unit), memoised per bot per ms:
    1. a player's skull, alive, in combat, selectable;
    2. outro: him while levitating, every role but healers (he can't be pulled);
    3. `GetLkLateTarget` (group 2): a non-null answer wins, and in Frostmourne nothing else is asked;
    4. a bot on a vehicle or stunned by Harvest Soul: nothing;
    5. winters: ranged DPS the sphere nearest its target; tanks their spirit; other DPS Raging Spirits,
       lowest health first, then phase one leftovers; healers nothing;
    6. phase one: the holder him; the free tank an add hitting someone else, else its lowest; melee DPS a
       Frenzied Shambling; everyone else him;
    7. phases two and three: Raging Spirits, lowest health first, then him.

    `IsLkTargetHeld` covers 4 and `IsLkLateTargetHeld`. Trigger and action on
    `IccDirectTargetTrigger`/`IccDirectAttackAction`.
14. Priorities: `ACTION_EMERGENCY` + 6 quake, + 5 shadow trap; `ACTION_RAID` + 14 tank defensive, + 13
    taunt, + 11 plague cure, + 10 plague walk, + 9 tranquilize, + 8 stun, + 6 silence dispel, + 4 target,
    + 2 stand. Movers: quake, shadow trap, plague walk, stand.

**Group 2, `lk-late`**: new `Util/ICCHelpers_LichKingLate.{h,cpp}`, `Trigger/ICCTriggers_LichKingLate.{h,cpp}`,
`Action/ICCActions_LichKingLate.{h,cpp}`, and `Multiplier/ICCMultipliers_LichKingLate.{h,cpp}` if a
multiplier is needed. Builds on group 1's API by the names above. Exports exactly `void
TickLkLate(PlayerbotAI*)`, `Unit* GetLkLateTarget(PlayerbotAI*, char const*& rule)`, `bool
IsLkLateTargetHeld(PlayerbotAI*)`, `bool IsLkLateMovementOwned(PlayerbotAI*)`.

1. State: its own `RaidInstanceState`; a 200 ms scan for Val'kyrs 36609 (carrying while 68985 is on
   it), Defile puddles 38757 with their scale, Vile Spirits 37799 (woken: not passive), Spirit Bombs
   39189, Wicked Spirits 39190 (woken: aggressive), the Spirit Warden 36824 and Terenas 36823/39217;
   duties every 250 ms; the same reset.
2. Phase two, `lk.stack` per bot (`p2 <n>`, `p3 <side> <n>`, `fm`, `none`): he stays at C, the holder at
   (0°, 3), the free tank with any Raging Spirit at (20°, 4), everyone else on a 2 yd lattice of up to 19
   nodes within 4 yd of K2 (180°, 5). A puddle fouling him, the holder spot or K2 slides the set to the
   nearest clear spot on the floor, keeping its offsets.
3. Defile, `lk.defilerun` (`run`, `clear`, `puddle <defile>`, `none`), from the listener's
   `IcecrownHelpers::IccState(instanceId).defileCast` (target, cast time) and the same after the heroic
   return:
   - the target walks (forced, interrupting, Nitro Boosts) to a spot 14 yd or more off every other
     member at cast end, within r 40, 12 yd off live puddles, on its own side of the stack;
   - others within 13 yd of the target step 13 yd off it during the cast;
   - anyone within 10 × scale + 2 yd of a puddle steps out; every walk and stand refuses points and
     segments within 10 × scale + 1.5 yd of one.
4. Val'kyrs, `lk.valkyr` per bot (`dps`, `stun`, `slow` and the Val'kyr, `none`):
   - while one carries a player (heroic: above 50%), every DPS bot and the free tank is bucketed by
     guid, balanced, melee to the nearest; the bucket's Val'kyr is the bot's pick;
   - one stun in flight per Val'kyr from `LkReadyStun`, the next stunner by guid in range when it has
     under 0.5 s left; one slow kept up from `LkReadySlow`;
   - melee chase only while their segment to it clears every puddle, else they stay at K2; healers stay;
   - heroic `EnableICCBuffs`: each carrying Val'kyr without 10308 gets it once (`lk.valkyrstun`,
     `IccNoteCheat`), as before;
   - heroic siphoners (no 68985, Z above 850 within 20 yd of C): the free tank taunts each, one per
     cooldown (`lk.siphon <valkyr>`), and ranged DPS kill them, carriers first.
5. Phase three sides, `lk.side` (`ObsValue<uint8>`, 1 east (270°, 20), 2 west (90°, 20)): east from the
   second Quake, flipped on each Vile Spirits cast (70498, 0.5 s); the holder drags him to the side spot
   and the raid stacks 5 yd inward of him on the K2 lattice, puddles avoided.
6. Vile Spirits, `lk.vile` per bot (`soak`, `trap`, `none`): one soaker with an immunity ready (Divine
   Shield, Ice Block, Dispersion, Cloak of Shadows), else the free tank with a button ready, then guid;
   it stands 10 yd from the stack toward the old side and walks into woken spirits within 8 yd and 5 yd
   above the floor. Hunters drop Frost Trap at the midpoint once they wake. Ranged DPS shoot woken spirits
   within 20 yd of the stack (rule `vile`).
7. Normal Harvest Soul, `lk.harvest` per victim (`warden`, `kick`, `heal`, `dispel`, `none`), inside
   Frostmourne: non-healers attack the Spirit Warden and kick its Soul Rip channel from its second update
   (class interrupts cast directly); healers heal Terenas 36823 under 80% and dispel Soul Rip 69397 off
   him or themselves; movement owned, reach only.
8. Heroic Frostmourne, `lk.bombdodge` per leader (`safe`, `dodge`, `stuck`): the leader is the
   lowest-guid alive bot tank inside, else the lowest-guid bot; when a bomb's ground point (falling or
   landed) lies within 13 yd of it, it walks (forced) to the nearest spot 13 yd off every bomb within 20
   yd of F. Everyone else stacks within 3 yd of it (`lk.stack fm`). Ranged DPS kill the woken Wicked
   Spirit nearest the leader (rule `wicked`). Walks pass Z 1049.95 and judge arrival in 2D.
9. Priorities: `ACTION_EMERGENCY` + 4 defile, + 3 vile spirits, + 2 Frostmourne lead and follow;
   `ACTION_RAID` + 12 siphon taunt, + 7 Val'kyr control, + 5 Harvest Soul duty, + 3 stack. Movers:
   defile, vile spirits, Frostmourne, stack.

**Group 3, `lk-readers`**: `tools/botobs/bosses/the_lich_king.py`, `tools/botobs/tests/test_the_lich_king.py`.

1. Read the keys of groups 1 and 2; drop the icon, winter staging, inbound and stack slot views.
   Sections: phases; tanks (roles, taunts by rule, buttons, human calls, brake); stands and stacks with
   the phase three side; plague (carriers, cures by rule); adds (enrage, stuns); traps; winters (sphere
   and spirit picks from `lk.target`, silence dispels, the Quake hold); Val'kyrs (buckets, stuns, slows,
   siphon taunts); Defile (the listener's `lk.defile` targets, runs); Vile Spirits (soakers, traps, side
   flips); Frostmourne (Harvest Soul duties, bomb rules); targets; cheats.
2. The banner spares the unused `IccInstanceState` keys `lk.winterinbound`, `lk.mtinbound`,
   `lk.rangedinbound`, `lk.sharedslot`; `lk.trap`, `lk.siphon`, `lk.bombdodge` off heroic; `lk.harvest`
   on heroic; and the keys of phases the pull never reached (`lk.phase`).
3. Synthetic-pull tests per section and the `EveryView` check on the full fixture, as
   `tests/test_sindragosa.py` does.

**Finisher**: writes `Definition/ICCDefinition_LichKing.cpp` from both groups' rows; the multipliers
(brake, splash, cure hold, then group 2's), then the rules: `OwnTargeting("icc lich king targeting",
Role::Any, &IccResolverPicks<&GetLkTarget>, DpsAssist | TankAssist | DpsAoe)`; a `Block` on
`IsLkTargetHeld` over the pickers, follow, random moves and pet attack; `OwnMovement` over every Mover
row on `IsLkMovementOwned || IsLkLateMovementOwned`, plus a `Block` of the spell movers on it; `AvoidAoe`,
tank `Taunt` and tank `caster form` blocked while live; the tank defensive hold. The tick runs
`TickLkEncounter`, then `TickLkLate`. Then the checks over every changed and new file, the review
through the wave script's lenses, fixes, the doc fold, Decisions, Carried over and Known gaps, and the
commit. New `.cpp` files mean a CMake re-run.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| The 1 yd platform snap goes: it cancels Shadow Trap and Ice Burst knockbacks mid-flight. A below-floor rescue survives behind the raid cheat (`lk.floorsnap`): under Z 838 within 44 yd of C, under 1046 within 38 yd of F, where only a collision fault puts a bot | cheat rule; navprobe reads neither floor (GO collision only) | drop it; a fall-through then dies at the script's Z 700 check |
| Phase one compact round C: him at (180°, 12), adds at (90°, 17) with their tank 5 yd further out, ranged and healers round (0°, 8) | WT and IC: him near the centre, adds faced off the raid; the plague must reach an add before its 5 s tick | WT's diagram, adds on the rim 50 yd out |
| DPS ignore phase one adds, area damage held near them | WT, WH: the plague kills them, ghouls are its fodder | cleave them |
| Plague cured by one elected curer once delivered; at once under 1 s to the tick or 35% health | WH: dispel before the first tick, at the adds | dispel on landing |
| Enrage answered by hunters' Tranquilizing Shot only, else the add tank's button | WT, WH; Anesthetic Poison needs a weapon poison the bots don't manage | rogues too |
| Stun rotation only on heroic Frenzy, melee finish a Frenzied Shambling | WT, WH | stuns on every Enrage too |
| Shadow Traps: every bot within 10.5 yd walks 12 yd off each trap within its 5 s arming, a bot with no stand to the spot nearest him; stands within 11 yd re-latch (up to 18 yd), so the holder drags him off; walks keep 3.5 yd off | script: arms after 5 s, any unit within 3 yd fires it (pets and totems too), the throw reaches 10 yd; WH: drag him off traps | the lane's 5.5 yd, which a pet or human stepping on the trap turns into a throw off the platform |
| Winters: two clusters on the north ring, the ranged one the Pain and Suffering half-cone plus 8° away (20.5° 10N, 60° otherwise), healers split on 25-man and heroic | WT diagram (raid on one side), WH (ranged and healers apart from tanks and melee); `spell_cone` | one cluster, or the ring spread all round |
| No pre-move before 70% or 40%; the walk starts at his turn to passive | keeps phase two's stack for the Val'kyrs | pre-move at 71.5% and 41.5% |
| Ring slots only while the platform isn't `DAMAGED`; at the second winter bots wait at r 42 | instance script: the ring is gone until the second winter's cast lands | walk out at once |
| Each Raging Spirit taken by the tank holding fewer, held on the rim facing out | WT, WH | one tank for all |
| Quake: back to r 38 on the bot's bearing, held until he turns aggressive | WT, WH: 5 s; cracks at 47-51 yd | straight to the phase two stack |
| Soul Reaper: the victim's button; the free tank taunts only when none is ready | WH: swap once the tank has no cooldown left | swap on every Reaper |
| Brake at 90%/80% on non-holders | his melee one-shots non-tanks; the other lanes' brake | none |
| Lust in phase one | WH: phase one is the DPS check and Plague Siphon ramps | phase three |
| Resist schools shadow and frost | most damage is shadow, the winters frost | shadow only |
| Phase two: him at C, the holder north, the raid stacked 5 yd south on a 2 yd lattice | WT diagram; WH, IC: stack tight behind him | spread for Defile |
| Defile: the target runs during the cast, others within 13 yd step away, no pre-spread | WT, WH; script: the puddle lands at cast end, 10 × scale | spread before each Defile |
| Val'kyrs: DPS buckets by guid, one stun at a time, one slow, melee never across a puddle | WH: stun rotation and slows | everyone on the first |
| Heroic siphoners taunted by the free tank | script: Life Siphon hits the last taunter | ranged kill only |
| Phase three: east and west side spots, swapped on each Vile Spirits cast; one soaker, an immunity first | WT diagram and text, WH | no side swap, kill the spirits |
| Normal Frostmourne: DPS on the warden with a kick, healers on Terenas with a dispel | WH | the escape cheat |
| Heroic Frostmourne: the raid follows one leader that steps off falling bombs; ranged kill woken spirits nearest it | WH: move grouped; IC: group, move together | spread |
| Outro: the resolver hands out the levitating Lich King out of combat | script: combat is disallowed and he can't be pulled | generic DPS |
| A second stem `LichKingLate` for phases two and three, rows in the one definition | disjoint files for the split | one stem |
| Frostmourne spots within 20 yd of F | the heroic teleport's 2-20 yd spread is the only known floor | the script's 40 yd room |
| Splash hold spares tanks | the add tank's area threat holds the pile | hold everyone |
| Cure hold also covers poison, curse and magic cures that strip a disease (Cleanse, Purify, Cleanse Spirit, Cure Toxins, Stoneform), only off a carrier | each removes one effect of every type it covers | disease cures only |
| Reaper swap when no button runs or is ready for the hit's remaining time (`RunningTankDefensive`, `ReadyTankDefensiveSpell`) | `NextTankDefensive` reads 0 while a button runs or on the GCD, which would swap a covered holder | `NextTankDefensive` gives 0 |
| Winter clusters grow away from each other: tanks east from 0°, ranged west from 20.5°/60° | two centred clusters of 6 yd slots don't fit the 8° margin | centred clusters |
| A spirit tank more than 28 yd off its spirit walks to 22 yd of it on the ring | 25-man ranged cluster ~51 yd from the spirit spots, taunts reach 30 | taunt from the spot only |
| Winter melee DPS 3 yd inward of the held spirit or leftover | brief's "melee on its inner side" | a slot |
| Winter ranged picks within 30 yd of the bot, reach held under `LichKing` walks and stands | reach walks a slot bot into Remorseless Winter and off its latch; 30 yd is every caster's base nuke | 40 yd with reach kept |
| Ranged DPS take a leftover Ice Sphere first in phases two and three | it still knocks its target off | spirits first |
| Brake stop node (`icc lich king brake stop`, RAID+1) drops auto attack and shot on him | a braked bot's resolver hands it nothing, which leaves the swing running (Blood Queen's pair) | multiplier only |
| Phase one lattice of 13 nodes, shared least-used first when short | lattice limits | grow the lattice |
| A stand walk unparked for 15 s lets go for 3 s | a spot it can't reach doesn't pin the bot all phase | no cap |
| The 2 yd floor test runs only once the bot's own spot reads the GO floor; the ring counts as standable when the platform object isn't found | a missing collision model would refuse every dodge | always test |
| Taunt precedence reaper, back, spirit, add; the free tank re-taunts loose leftovers in winters | Soul Reaper is the lethal one | equal |
| The curer is kept while it can cure; a stunner whose stun hasn't landed in 1.5 s sits out 2 s | no flapping duties | re-elect every pass |
| Puddle clearance: spots and nodes 10 × scale + 3 yd, segments 10 × scale + 1.5, puddles a bot stands in ignored for its own walk | a parked bot never trips the 2 yd step-out | the brief's 1.5 everywhere |
| Defile run: "12 yd off puddles" past the edge, the side the half-plane through the stack centre (none within 2 yd), else 16/12/8 yd straight out; others also keep 13 yd off the run spot | brief wording | no fallback |
| Val'kyr buckets dealt again only when the carriers change; free tank out while a Raging Spirit attacks it; holder no bucket and no control; ranged chase past 30 yd; next control caster at 0.5 s left with a 1 s grace, one duty per bot | melee keep their Val'kyr as they fly apart | re-deal every pass |
| Heroic siphoners taunted never-taunted first, then longest ago; ranged kill the lowest once their bucket has no carrier | Life Siphon hits the last taunter | ranged only |
| Vile soaker immunity ranked Divine Shield, Dispersion, Cloak of Shadows, Ice Block (it roots the mage), non-healers first; no immunity: the free tank with or without a button | WH | free tank first |
| Frost Trap from the lowest-guid hunter with it ready, at the midpoint once the spirits wake | WT, WH | every hunter |
| Phase three frame: phase two's offsets rotated so 0° points out along the side | the holder 3 yd outward of him, the stack 5 yd inward | fixed north frame |
| Stack: melee and extra tanks nearest him, extras wrap; anchor slides up to 20 yd within 40 of C, back once clear; the free tank's spot isn't in the fouling test | WT, WH | spread |
| Heroic Frostmourne leader: with no spot clear of every bomb, dodge only those landing within ~4 s, else `stuck`; followers forced only while it dodges | bombs fall ~3.5 yd/s | always forced |
| Normal Harvest Soul: kick from the second scan seeing Soul Rip; only priests and paladins dispel; a healer victim attacks nothing | WH; Soul Rip is magic | kick at once |
| The reader's banner treats `lk.stun` as heroic-only | it is the heroic Frenzy stun | every difficulty |
| Movement split in two `OwnMovement` rules: Attack kept under `LichKing` ownership, Attack and Reach under `LichKingLate` | reach would walk winter bots into the aura; Harvest Soul victims and far stack nodes need it | one rule keeping Reach |
| Walks are straight lines at the floor's Z (`exact_waypoint`) | neither floor is on the navmesh, and the path search refuses spots the GO collision doesn't answer (pitfalls, "Movement that silently no-ops") | path search |
| Passive at 40% and under after phase three has run reads `frostmourne` | he stays passive 1 s after the heroic return, which read `winter2` and sent bots to winter slots with Nitro Boosts | per-bot harvested only |
| A plague walk wins over the late stack | the stack walked an isolating carrier back into the raid | stack wins |
| An intro that evades resets the state | opening stands otherwise held bots after he sat back down | wipe or kill only |
| Tank buttons skip Anti-Magic Shell outside Soul Reaper (`physicalOnly` on the siphon ramp, an Enrage and a low tank) | Plague Siphon 74074 is +2% physical damage per stack; Soul Reaper's delayed hit is shadow | Anti-Magic Shell on every window |
| A fresh Val'kyr (not attackable, hanging over him) is no siphoner, and the free tank skips a Val'kyr its taunt can't land on | script: taunt immunity lasts until the siphon spot; `IccCastClassTaunt` falls back to shoot or throw | taunt every Val'kyr above Z 850 |
| A non-tank Vile Spirit soaker whose immunity ran out, with nothing else ready, waits on its spot; the free tank soaks on its health | a burst is 19-37k | keep soaking |
| `lk.valkyrstun` lands once per carrying Val'kyr, as the Task list says | the old code re-applied Hammer of Justice whenever it lapsed, and the overview keeps the heroic buffs "with the same behaviour" | re-apply while it carries |

## Carried over

- Merge stage: the `LichKingLate` headers into the umbrellas (`Action/ICCActions.h`,
  `Trigger/ICCTriggers.h`, `Multiplier/ICCMultipliers.h` if group 2 adds one); `HookRows()`
  `{&IccLichKingDefinition, &IccLichKingBurstWindow, &GetLkOpenerBosses, &IccLichKingResistSchools,
  nullptr}`.
- Merge stage, README: the cheat table loses `lk.ragingspiritkill`, `lk.wickedspiritkill`, `lk.hoj` and
  `lk.frostmourneescape` and gains `lk.floorsnap` (gap, raid); the "Documented, not wrapped" sentence on
  the dive and the 1 yd snap and the "targeting is by raid icon on the Lich King" gap describe deleted
  code; direct targeting users gain `GetLkTarget`, tank defensive users `icc lich king tank defensive`
  (`lk.defensive`, `lk.humancall`); the stem list gains `LichKingLate`; `lk.winterinbound`,
  `lk.mtinbound`, `lk.rangedinbound` and `lk.sharedslot` join the unused per-instance keys.
- CMake re-run: new `.cpp` files.
- r14: `IccInstanceState`'s Lich King members (`ICCScripts.h`) go unused; check `ICC_BUCKET_MARKERS`,
  `IccRtiNameForBucket` and `IccEnsureIconOn` for other users; hoist one walk base.

## Known gaps

- Never built, no 631 trace: every threshold is a model until a pull. Nothing on either floor is
  probed or sight-checked; the ice shards at 41 yd may block sight.
- 25-man and heroic: the ranged winter cluster at 60° is ~44 yd off a sphere's path to the tank
  cluster and ~51 yd from the spirit spots, past 30 yd: tank-cluster spheres go unkilled and can Ice
  Burst it off the ring; spirits are burned by melee and tanks only. A closer cluster eats Pain and
  Suffering's 104° cone: a refine lane decides from `--winters`.
- `lk.phase` reads `two` or `three` for up to the 2 s cast that delays a threshold.
- One bot tank: every spirit on it, no add tank, no Soul Reaper swap.
- A Cleansing Totem already down keeps pulsing; Gnaw and Intimidation's pet stun untested.
- Far stack nodes and the free tank's Raging Spirit can sit just past the 2.5 yd unpark (melee may
  bounce); Val'kyr chasers recheck only the straight line against puddles; heroic Frostmourne
  followers keep reach and can step off the stack toward a Wicked Spirit.
- Heroic Harvest Souls has no timed end in the DBC: a bot it outlived the return on would hold every
  duty (`lk.target` `held`).
- Melee may bounce off a trap close to him: generic reach back to him has no trap check (`--traps`).
  A Paladin's Cleanse of a silenced plague carrier strips the plague too.
- Humans hold no duty.

## Blocked

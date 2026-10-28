# r4-gunship — Gunship Battle

Wave 3b, parallel with `r5-saurfang`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounter

Slug `icecrown-gunship-battle`, prefix `gsb.`, boss id 2, stem `Gunship`. Core
`boss_icecrown_gunship_battle.cpp`. Doc `icecrown-gunship-battle.md`, reader `icecrown_gunship_battle.py`.
Both factions' ships. Guides in the cache: Warcraft Tavern 25 and 10, instant-carry, Wowhead if
saved.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 1067 lines (`ICCActions_GSB.cpp`), with 9 `FindNearestCreature` lookups.
- Icons: the skull on the captain or mage for the boarders, and the star on axethrowers (5 writes).
- What is handled today:
  - cannons and the rocket pack;
  - the main tank jumps to the captain;
  - the assist tank holds the deck and collects boarders.
- `IccGunshipMultiplier` vetoes Formation, Follow and the main tank's AttackRti. More than 30 yd from
  all three jump points, it vetoes everything but the rocket jump.
- The cheat `gsb.battlefury` (`IccGunshipRocketJumpAction`) strips Battle Fury off the enemy captain.
  The code comment's core-bug claim does not hold (doc).
- The fall snap (`NearTeleportTo` with `removeTransport`) is borderline. The core's own rescue already
  saves deeper falls.
- 8 positions, all on transport decks, which navprobe can't check: document them as such.
- The 100 yd and 200 yd radii have no core value behind them.

## Guide answers to build on

- The heroic changes no mechanic, only numbers.
- **Setup:** 2 tanks (one boards, one defends), up to 4 healers with 2 at home, 4 cannoneers of any
  role, ranged at home.
- **Boarding:** the boarders follow the tank across, kill the mage (Below Zero freezes the cannons),
  and return at once.
- **Battle Fury:** +12% damage per stack, 20 s, refreshed with each stack. Staying too long kills the
  tank; leaving the ship lets the stacks fall off.
- **Cannons:** manage heat (Overheat).

## Focus

- The cannon duty with heat.
- The boarding party's timing and a return home that replaces `gsb.battlefury`.
- The home deck's ranged kill of riflemen, axethrowers and mortar.
- The assist tank's boarder pickup.
- Direct targeting instead of skull and star. The fall snap under the cheat rule.

## Owns

Stem `Gunship` and its definition, the doc, the reader and its test, this brief.

## Task list

One lead, no split: every piece reads the same side table, deck test and roster.

- [x] Helpers: side table from `DATA_TEAMID_IN_INSTANCE`, ships from the instance guid slots, deck test
  from the core's own y bounds, unit cache from the ships' passenger sets, roster, boarding state,
  retreat latch, stands, hazards, resolver; all housekeeping in the tick.
- [x] Cannon duty with heat: ranged first, Incinerating Blast at 90 heat, DBC cooldowns on a receipt.
- [x] Boarding: board tank first, boarders once a tank holds the captain, return when the mage dies,
  tank last; retreat on Battle Fury stacks. `gsb.battlefury` removed.
- [x] Home deck: home tank at the portal exit, taunts boarders; healers and ranged on edge slots;
  ranged kill boarders, then riflemen/axethrowers in range.
- [x] Direct targeting; no icon or `rti` writes; legacy `star` rti reset in the tick.
- [x] Rocket Artillery and Burning Pitch dodge on a deck-bounded sweep.
- [x] Fall snap removed; cross-deck movement guard and deck mover blocks replace the blanket veto.
- [x] Reader and tests, boss doc, checks.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| `gsb.battlefury` removed. The party leaves when the freeze mage dies, the tank last (10 s cap); it retreats early once the captain carries +200% from Battle Fury (20 stacks on 10-man, 17 on 25-man) and boards again once his stacks are gone | WT, Wowhead, IC; DBC 69638 row 2155 (16 s, +10/+12%) | tank defensives at a lower stack count; no retreat |
| Fall snap removed: the core's Check for Players (every 5 s, 10 yd under the ship creature) puts every faller back long before the ground, 620 yd down | core, navprobe terrain height | keep it behind `IccRaidCheat` |
| Roster (bots only): board tank = first bot tank (main tank first), home tank = the next; cannoneers = ranged DPS by guid, then melee DPS, one per live cannon (2 on 10, 4 on 25); home healers 2 on 25, 1 on 10, the rest board; melee DPS board, ranged stay home and board only when no melee DPS bot exists | WT25/WT10, Wowhead, DB spawn masks | Wowhead's all-DPS boarding |
| Ejected melee cannoneers board during a freeze, ranged ones take an edge slot | Below Zero ejects every passenger | they wait for the thaw |
| Boarders jump once a tank holds the captain, or 15 s into a freeze with no tank on him | WT ("tank always goes first") | never without a tank |
| Board tank holds the captain 6 yd inside his deck from the landing point toward the mage: 30 yd (A) / 35 yd (H) from the home edge, inside his attack bound | WT, Wowhead (drag him to the edge); core y bounds | his home spot |
| Healers and ranged at home stand on slots 5 yd apart, 4 and 8.5 yd inboard of the cannon line, transport-local from the core's cannon spawns; healers take the slots nearest the landing | WT, Wowhead ("stay close to the edge") | no stands |
| Home ranged order: boarders on the home deck (oldest rank first), then riflemen/axethrowers in range, then mortars in range | Wowhead (riflemen "when you have nothing else"), WT ("kill the oldest adds first") | WT's riflemen first |
| Cannon Blast under 90 heat, Incinerating Blast from 90 (Cannon Blast adds 6-10, Overheat at 100); mortars first, then gunners; cooldowns 500/1500 ms from the DBC, stamped on an energy receipt (was an invented 1000 ms on any cast) | core `spell_igb_cannon_blast`, DBC 69399/69401/70172/70174; guides | the old 1000 ms stamp |
| Rocket Artillery: everyone on deck and out of a cannon leaves 4 yd of each mortar cast's spot (5 s cast) and of the 71371 marker; Burning Pitch patches too; interrupt a cast first on heroic only (knockback). The sweep stays inside a deck box from the core's crew slots: `FindNearestPositionClearOfHazards`' navmesh check refuses every point on a transport deck | Wowhead, WT; DBC 69679/69680-69689/71371/69660 | no dodge |
| No Bladestorm dodge: 69653 has no radius, so it hits nobody here | DBC, `WorldObjectSpellAreaTargetCheck` | the guides' 8 yd |
| No pre-pull stands: the ship flies until the battle. Before live only the rocket pack and the cannon seats | core intro | stands from the transport offsets |
| A hand-written multiplier zeroes reach, heal reach and charge whose target stands on the other deck; follow, formation, flee, run away, random moves, collision steps, blink, disengage and avoid aoe are blocked while live | WT's failure list (walking, blinking off the edge); a rule can't see an action's target | the old blanket veto |
| A bot's legacy `star` rti goes back to `skull` while the gunship gate is open | Conventions 10 | leave it to r14 |
| A unit's ship is its side of the middle of the core's gap (y 2033.5 / 2438); only airborne (z 478+) or the flight aura reads as in flight. Dodge spots keep the strict bounds | core y bounds | the strict bounds for both, which would freeze a bot standing on a deck's edge |
| Unit cache from both ships' passenger sets, the freeze mage by its Below Zero channel, else a mage not channelling the portal | core `PassengerController`, the portal's offset-only teleport | grid searches |

## Carried over

- r14: `IccInstanceState::gsbLastStarPos` and `gsbLastRocketPackUse` (`ICCScripts.h`) are unused now.
- r14: `SPELL_FROZEN_CANNON` 69704 (`ICCData.h`) is in neither the DBC nor `spell_dbc`.
- r14: `RaidRestoreLegacyRti` doesn't cover `star`; the gunship tick resets it for its own window.
- r14: the stem's walk base copies Marrowgar's `IccLmWalkAction`; hoist one.

## Known gaps

- No 631 trace on disk; nothing here was replayed.
- The decks are gameobject floors: navprobe reads every point off mesh with terrain 620 yd below, so
  no coordinate is checked offline.
- The edge slots assume the cannon side faces the enemy ship, and the world points that the ships stop
  where the core's y bounds put them.
- No tank defensive on the captain's stacks, no burst pacing.
- Mortars sit out of every player's reach; only the cannons kill them.
- No `-Werror` pass; the syntax check hides warnings.

## Blocked

# g13-lich-king — close the Lich King's gaps

Gap wave `3efg-gaps`, after `3cd-gaps`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap
lanes" section included. Builds on the merged `r13-lich-king` lane and its review: read
[r13-lich-king.PLAN.md](r13-lich-king.PLAN.md) first. Its decisions stand unless closing a gap needs
a change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/the-lich-king.md` and in r13's brief.

- Neither floor is on the navmesh, so navprobe checks nothing there. A stand the lane adds or moves
  stays inside r13's always-solid envelope, and sight stays unchecked.
- The 25-man and heroic ranged winter cluster against the spheres' paths: with no pull, the script's
  sphere speed and target choice decide, and the choice goes under Decisions for review.
- One bot tank: Raging Spirits, the adds and the Soul Reaper swap. The Gap lanes' one-bot-tank rule
  applies.

## Owns

Stems `LichKing` and `LichKingLate` and the definition, the doc, the reader `the_lich_king.py` and its
test, this brief.

## Task list

One lead, no split: every change sits in the two stems' states, ticks and resolvers.

1. Sphere guards: on 25N/10H/25H, ceil(ranged DPS / 3) ranged DPS bots, hunters first, take the
   middle slots of the winter tank cluster (`lk.stand` `winter guard <n>`).
2. Phase latch: `lk.phase` keeps `one` (`two`) while he's aggressive under 70% (40%) until his winter
   starts; the lust window reads the same.
3. One tank player: a ghoul stand-in, a DK then a paladin damage dealer (`lk.tank` 3), holds the
   phase one ghouls at AT: class taunts on loose ghouls, its pick its own healthiest ghoul, no plague
   walk, the `addtank` cure rule. A spirit on a human tank is never taunted off it.
4. Cleansing Totem: a shaman bot with one down while a carrier lives drops Mana Spring, else Healing
   Stream (`icc lich king totem`, `lk.cure` `totem`).
5. Soul Shriek: a paladin never Cleanses a plague carrier's silence.
6. Harvest Soul's hold reads the stun state with the aura, in both stems.
7. Trap reach: a melee DPS bot whose straight approach to its pick passes within 10.5 yd of a trap
   walks to a clear melee spot round the pick, else holds (`lk.trap` `reach`, `hold`).
8. Leftover spirits: melee DPS bots whose pick is a Raging Spirit within 12 yd of him leave their
   stack node (`lk.stack` `p2 spirit`, `p3 <side> spirit`).
9. Val'kyr chase: a melee chaser whose line crosses a puddle walks a latched waypoint beside it,
   reach held (`lk.valkyr` `round`).
10. Heroic Frostmourne: reach held for every bot inside; ranged picks woken Wicked Spirits within
    30 yd of themselves.
11. Reader, tests, boss doc.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Sphere guards: on 25N/10H/25H, ceil(ranged DPS / 3) ranged DPS bots, hunters first, stand in the middle columns of the tank cluster and shoot its spheres, then its spirits | script: a sphere spawns within 10 yd of him and follows a random player at 3 yd/s (`speed_run` 0.43); the ranged cluster at 60° is 44 yd or more off every path to the tank cluster; a guard at its middle sits 18-24 yd off them and 17-29 yd from the spirit spots; WH: hunters suit spheres best | the ranged cluster closer, inside the 104° Pain and Suffering cone; one cluster |
| `lk.phase` keeps `one` or `two` while he's aggressive under the threshold until his winter's turn to passive, latched per pull | script: the transition waits for the first hit he takes while not casting | stateless, up to 2 s early |
| One tank player: a ghoul stand-in in phase one, a DK then a paladin damage dealer, at AT; class taunt on loose ghouls, its pick a loose ghoul, else its own healthiest; no plague walk, the `addtank` cure rule | Gap lanes' one-bot-tank rule; WH: the ghouls are plague fodder the second tank holds, abysmal damage (8.5-29.4 × 177 per 2 s); Taunt needs Defensive Stance, Growl bear form | warrior damage dealers too; no stand-in |
| No stand-in for Shamblings, Raging Spirits, the Soul Reaper swap or siphon taunts | `creature_template`: Shambling swings 50/131.9/90.5/214.8 × 177 per 2 s with +200% Enrage and a 150% Shockwave, spirits 66.4/152.4/95.2/219.2, him 139/276.1/244.3/492.9 per 1.5 s and +100% haste after Soul Reaper; Life Siphon heals the Val'kyr 10× whoever it hits | a stand-in on 10N spirits |
| A Raging Spirit on a human tank is never taunted off it | Humans decision: a human tank plays its own part | taunt it to its assigned bot tank |
| A Cleansing Totem down while a carrier lives is replaced by Mana Spring, else Healing Stream | Rotface's answer: a new water totem replaces the old | Totemic Recall, which takes every totem |
| Paladins never Cleanse a plague carrier's silence; priests still dispel it | Cleanse takes a disease with the magic, and the plague jumps from wherever the carrier stands | dispel and let it jump |
| Harvest Soul's hold needs the stun state as well as the aura | Harvest Souls 73654 has no duration, its aura ends with his channel; its effect 2 is the stun | the aura alone |
| Trap reach: a melee DPS bot whose straight approach passes within 10.5 yd of a trap walks to the clear spot round its pick nearest it, else holds still | `TRAP_TRIGGER`; generic reach otherwise bounces off the dodge | generic reach |
| Melee DPS bots whose pick is a Raging Spirit within 12 yd of him leave their stack node | WT, WH: leftover spirits are tanked on him and cleaved | keep the node and bounce |
| A melee Val'kyr chaser whose line crosses a puddle walks a waypoint beside the first puddle, latched until reached, reach held | WT: Val'kyr stay out of Defile, which grows on every player it hits | stay at K2 (r13) |
| Heroic Frostmourne holds reach for everyone inside; ranged pick the woken Wicked Spirit nearest the leader within 30 yd of themselves | WH: the raid moves as one group | reach kept, a bot steps off the stack |
| Sight stays unchecked | both floors and the ice shards are gameobjects with no vmap under them, so `los` reads every ray clear | navprobe `los` |

All tasks done. No coordinate added or moved: guard slots come from the winter-slot generator on the
ring, and reach spots and puddle waypoints are derived at runtime through the floor tests r13 set.
Navprobe reads neither floor (gameobject collision only), so nothing was probed.

## Carried over

- Merge stage, README "Trigger names": the Lich King's list gains `totem`.
- Merge stage, README: `lk.tank` gains `3`, the phase one ghoul stand-in with no other tank player;
  the Lich King's movement rules gain `icc lich king late reach hold` (Attack only, heroic Frostmourne
  and the walk round a puddle).
- Merge stage, status row: r13's "25N/10H/25H ranged winter cluster too far from spheres and spirit
  spots", "one bot tank: no add tank or Soul Reaper swap", "heroic Harvest Souls has no DBC end",
  "melee reach near traps unchecked" and "Cleanse strips the plague" are closed or decided here, with
  the phase lag, the Cleansing Totem, the leftover-spirit bounce, the chase round puddles and the
  Frostmourne followers' reach; the leftovers are in the boss doc's Known gaps.
- r14: the Cleansing Totem swap duplicates Rotface's `GetRfTotemSwap`; the r13 items stand.

## Known gaps

- Those in the boss doc.
- No 631 trace; the user built `623638b99` clean, this change needs a rebuild (no new `.cpp`, no CMake
  re-run); no `-Werror` pass beyond `pb-check`.

## Blocked

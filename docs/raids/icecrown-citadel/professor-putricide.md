# Professor Putricide

Boss id 6, slug `professor-putricide`, prefix `pp.`. Code: stem `Putricide` (`src/Ai/Raid/ICC/*/ICC*_Putricide.*`);
nodes `icc putricide` dodge, swap, tank defensive, kite, plague, stack, abomination, brake, target, stand;
multiplier `icc putricide brake`. Raid conventions: [README.md](README.md). Core: `boss_professor_putricide.cpp`.

## Mechanics

Ids are 10N/25N/10H/25H where they differ; code remaps the base id through `GetSpellIdForDifficulty`. Every
area is a creature's, or a player's on players: centre to centre.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Phases | `DamageTaken`: phase 2 at 80%, 3 at 35%; each transition delays his abilities 24 s (+25 heroic). Normal: Tear Gas 71617 stuns the raid while he drinks at his table. Heroic: no stun, two Unstable Experiments, the drink 25 s after he reaches the table (Create Concoction 71621-72852, Guzzle Potions 71893-73122, 4 s casts) | Transitions are a pause on normal, a fight on heroic | `pp.phase` from his health |
| Unstable Experiment 70351/71966/71967/71968 | 2.5 s cast every 35-40 s, first at 30-35 s, P1-P2: alternately a Volatile Ooze from the east tank stalker (4388, 3213) and a Gas Cloud from the west one (4324, 3215) | Both are the top kill priority | Kill order ooze, cloud, him; of two of one entry the lowest health |
| Volatile Ooze 37697 | Taunt-immune, 9 yd/s. Adhesive 70447/72836/72837/72838: 3 s cast on a random player without its 25 s protection, roots and ticks. Erupts 70492/72505/72624/72625 when within 5 yd of the victim (search every 1 s): 10 yd round the ooze, damage shared, knockback; then re-picks | Everyone stacks on the rooted victim | Ranged, healers and tanks with nothing to hold stand within 3.5 yd of it (`pp.stack`); melee stay on the ooze |
| Gas Cloud 37562 | Taunt-immune, 8.05 yd/s against a player's 7. Gaseous Bloat 70672/72455/72832/72833: 3 s cast, 10 stacks, a tick every 2 s (1.2k/1.5k/1.5k/1.9k a stack) that burns one; Expunged Gas 70701 on the whole raid when it reaches its victim, the per-stack amount times (stacks+1)/2. At 0 it re-picks a random player | Kite it the whole bloat; melee leave it at 2-3 stacks | Kite legs (`pp.kite`); melee 15 yd off it from 2 stacks |
| Slime Puddle 70341 | Every 35 s from 10 s, under 2 random players: a Growing Ooze Puddle 37690 hitting 70346/72456/72868/72869 every 1 s within 2.5 yd times its scale. 7 Grow stacks at spawn (6 yd), +1 every 1.5 s (+0.5 yd), 50 at most (27.5 yd) | Spread, drag him off; the abomination eats them | Dodge; the boss spot and slots move off them |
| Malleable Goo 70852 | P2 on, every 25-30 s: 1 player (25N 2, 25H 3), never his last victim nor within 7 yd plus both reaches (16.3 yd, 17.5 once he transforms to reach 9). A 6 yd/s missile to where the target stood; 5 yd impact 70853 with a 15 s cast slow | Dodge | The rings sit inside the exclusion; the dodge clears every impact until it lands |
| Choking Gas Bomb 71255 | P2 on, every 35-40 s: 2 of 3 bombs 38159 within 6 yd of him for 10 s. Gas 71278/72460/72619/72620 within 3 yd every 1 s (-75% hit); explosion 71279/72459/72621/72622 within 10 yd on a 10 s period | Drag him away, step out | The spot moves; the dodge clears the gas, the blast from 8 s |
| Mutated Strength 71603/72461 | P3 buff: +50% damage and attack speed | Tank cooldowns | Tank buttons |
| Mutated Plague 72451/72463/72671/72672 | P3, every 10 s on his victim, stacking for 180 s; every 3 s the raid takes 75 (10N) × 2.5^stacks. Its removal, death included, heals him 300k/1.7M/500k/2.1M a stack | Spread stacks over tanks and taunt-capable DPS; never 4 | The swap rotation (`pp.swap`) |
| Unbound Plague 70911/72854/72855/72856 | Heroic, from 20 s every 90 s (3.5 s retry) on a random player within 45 yd but his victim, 60 s: 200 (10H) or 800 (25H) a second, ×1.25 a tick. Once its searcher 70917 has ticked twice it jumps to a random ally within 3 yd without bounce protection 70955 (5 s), restarting the ramp; the old carrier gets Plague Sickness 70953/73117, +250% from it a stack, 60 s | A pass line about 10 s each, or let the carrier die and battle-res | Pass line (`pp.plague`, `pp.receiver`) |
| Ooze/Gas Variable 70352/74118, 70353/74119 | 25H transitions only: half the raid deals no damage to the cloud, half none to the ooze, until he resumes | Each half on its add | The resolver follows the bot's own Variable |
| Mutated Abomination 37672 (25: 38285) | The Drink Me! table 201584 summons it while he is in P1-P2 and none exists; despawned when P3 resumes. Energy 0, no regen (70385). Eat Ooze 70360/72527: needs the puddle tick's aura on it, eats the nearest puddle within 15 yd for 4 stacks (or the rest at 4 or fewer), +4 energy, 1 s. Regurgitated Ooze 70539/72457/72875/72876: 45 energy, 50 yd, -50% speed for 20 s. Mutated Slash 70542: melee, armor -5% a stack. While it lives its 70402/72511/72512/72513 hits every ally but its driver every 2 s | The off-tank drinks, clears puddles, slows the clouds | The driver (`pp.driver`, `pp.abom`) |
| Berserk | At 10 min | | |

His combat reach is 7.8 (9 transformed): melee range 10.6/11.8 yd, a hunter's minimum range 15.6/16.8. Ooze
Tank Protection 71770 protects whoever he hits from both adds' picks. He obeys taunt diminishing returns
(`flags_extra` 0x80000). `CanAIAttack` takes only targets within 80 yd of (4356, 3211), z 388-410, y above
3157.1. `IccPutricidePhase` reads his health rather than `_phase`, which flips a tick or a cast later.

## Duties

- **Boss spot** (`pp.spot`): the room's centre (the Tear Gas stalker, 4356.64, 3211.78, 389.40), one of 8 at
  8 yd, or one of 8 at 16 yd offset 22.5 degrees. It moves only once a puddle reaches within 11 yd of it or a
  bomb lies within 9, to the nearest spot clear by a yard more; with none clear, to the one the puddles
  leave widest.
- **Holder**: the bot tank he hits stands past the spot on the line he comes in along, 1.5 yd inside his
  melee range, so he stops on the spot. Not while he is passive at his table.
- **Rings** (`pp.slot`): healers, then casters, on 14 slots at 12 yd round the spot; hunters and the overflow
  on 24 at 22 yd. Slots are more than 4.5 yd apart. One covered by a puddle (with a yard of growth), a bomb's
  gas or armed blast, or goo in flight changes for the nearest usable one. Reach and spell movers are held
  while a stand holds; healers keep heal reach. The rings let go while anyone but a bot tank holds him
  more than 15 yd off the spot, and hold again within 10.
- **Kite** (`pp.kite`): the bloated bot runs 10 yd legs straight away from its cloud, turning up to 120
  degrees round puddles, bombs and goo, clamped to a 36 yd disk round the centre so it follows the wall,
  scored for continuing its last leg. It cuts a cast to move and uses Nitro Boosts.
- **Ooze stack** (`pp.stack`): ranged, healers and tanks holding nothing walk to within 3.5 yd of the rooted
  victim, ignoring every hazard, as soon as the adhesive lands. Carriers, the driver and the kiter don't.
- **Abomination** (`pp.driver`, `pp.abom`): in P1-P2 one bot tank not holding him, the assist tank first,
  once a tank holds him; a bot already riding keeps it. It walks to (4356.60, 3265.74), below the table,
  and uses it. Riding: Regurgitated Ooze with 45 energy on a cloud not slowed, on an ooze only from 90 or
  once a cloud is slowed; else it eats the puddle crowding the spot most, latched until gone, from 1.5 yd
  inside its edge; else Mutated Slash and its swing on the ooze, the cloud or him. A refused vehicle cast
  answers true, so the energy is the receipt for the 1 s cooldown stamp.
- **Swap** (`pp.swap`, `icc putricide swap` above the movers): a tank picks him up off anyone else at any
  phase. In P3 the holder keeps him for 2 applications; then 2.5 s before the next a sharer, else 1.2 s
  before it the tank with the fewest stacks below the holder's, taunts. A sharer is a bot Death Knight or
  Retribution paladin damage dealer at 80% health or more, with at most 1 stack and fewer than every tank,
  a physical defensive ready (`pp.sharedefensive`, cast first): it takes one application and a tank takes
  him back as soon as it lands. Never at the immune level of taunt DR or while a taunt aura is on him.
  Generic taunts and Death Grip are blocked.
- **Brake** (`pp.brake`): while live, any bot but a healer and the holder past 90% of the holder's threat
  holds damage on him and area spells within 18 yd, until under 80%. Its resolver hands it nothing on him;
  pickers, follow and random moves are blocked; a bear stays a bear.
- **Unbound Plague** (`pp.plague`, `pp.receiver`): a bot carrier keeps everyone 4.5 yd off, stepping clear
  when crowded. From its 6th tick a receiver is elected (a bot without Plague Sickness, bounce protection,
  the plague, a vehicle or an add on it, not his victim; ranged DPS, then melee, healers, tanks; nearest);
  from tick 8, or under 50%, with its searcher ticked twice and 3 s left, it walks onto the receiver. Others
  dodge a carrier but their own's receiver. A rooted carrier waits.
- **Dodge** (`pp.dodge`, emergency, forced): all but the holder, the driver, the kiter, stackers and the
  rooted victim: within half a yard of a puddle, gas, an armed blast, goo in flight, a carrier, or for melee
  a cloud re-picking within 15 yd, to the nearest spot clear by 2.5 yd (0.75 when nothing is), its walk
  across none, melee within his melee range first. Puddles, gas, blasts and goo landing within 2 s cut a
  cast. A stand walks back only once nothing covers it.
- **Targets** (`pp.target`): the holder stays on him unless he is passive; others take a player's skull,
  their 25H Variable's add, the ooze, the cloud (not melee while it re-picks), him. A ranged bot takes an add
  only within 30 yd (a hunter 35), its spell range from a slot.
- **Tank defensives** (`pp.defensive`): a P3 holder in the first 4 s of each hold (physical buttons only),
  any tank under 40%; class buttons held to 25%. A human tank gets the same as calls, and a call to taunt
  when a bot holder is due to hand him over and no bot can (`pp.humancall`).

All 17 spots' 646 slots sit on the floor (navprobe `--nav 0x09`, settled z 389.39-389.45) and see their spot
(`los`); rings round the centre from 6 to 45 yd are on the floor, at 50 yd the north-east and north-west
corners are not, and every ray from it to 42 yd is clear. The holder's and dodge spots are derived inside
that probed disk. Gameobject collision isn't in navprobe's sight.

## Decisions

| Choice | Source | Alternative |
|---|---|---|
| Boss spot near the room's centre, moved only when a puddle or bomb fouls it | Script: puddles land on random players, bombs within 6 yd of him; guides: drag him off both | Wowhead's spot under the green tank, which leaves the kite and the stack out of heal reach |
| Casters and healers at 12 yd, hunters and overflow at 22 | Goo never picks within 16.3/17.5 yd of him; hunters can't shoot him from there; slots 4.5 yd apart for Unbound Plague's 3 yd jump | Guides: ranged spread at range, dodging every goo |
| Rings let go past 15 yd off the spot unless a bot tank holds him | Lessons: a stand held round a boss who moves off; 15 + 22 yd stays inside a caster's 39 yd reach to him | Keep them, so a human tank's position leaves the casters idle |
| Kite straight away, then along the wall | WT: kite the whole bloat; a pure-pursuit simulation catches a ring kite in 10-14 s, a straight run gains 1.05 yd/s on it | The old scored open-space flee |
| Stack on the rooted victim at once, hazards ignored | WT/Wowhead; the eruption's damage is shared within 10 yd of the ooze | Stack only once it is close |
| Kill order ooze, cloud, him; the lowest health of one entry | WT/Wowhead | Cloud first |
| Melee leave a cloud at 2 stacks or with no victim | Wowhead: run at 2-3 stacks | none |
| Regurgitated Ooze on a cloud first, an ooze only from 90 energy or once a cloud is slowed | WT/Wowhead: the slow is worth most on a cloud; energy only from Eat Ooze | Spend on whatever is up |
| Holds of 2 applications, DPS sharers of one | Wowhead's rotation; taunts 20 s apart stay clear of taunt DR | Tanks only; the old swap on every application went immune by the fourth taunt; a hand-over by stack count, never a 4th while anyone has fewer, at more taunts |
| Brake on every non-holder | Festergut's 90/80% | Tanks only |
| Unbound Plague pass line from tick 8 | WT: about 10 s each | Wowhead: let the carrier die, battle-res |
| Tank buttons for P3 holds | WT/Wowhead: Mutated Strength | Also P1-P2 swings |
| Bomb blast from 8 s | 10 s period, 10 s summon | Gas only |
| No opening positions | He is pulled at his table and dragged to the spot | Pre-pull rings |

## Cheats

Removed: `pp.nitro` (Nitro Boosts are the bot's own), `pp.oozekill` and `pp.gascloudkill` (kill order),
`pp.unboundplague` (the pass line), `pp.freeooze` (the energy duty cycle), the skull icon writes.
Record-only, `EnableICCBuffs`: `pp.heroicbuff` and `pp.spitefulfury` on every member on heroic (README).

## Known gaps

- Never built; no 631 trace yet, so no figure here is measured.
- Goo flight is timed from where he stands now, not where he threw it; the listener keeps an impact 9 s, so
  a throw past 51 yd drops early.
- The kite has no slow but Regurgitated Ooze (no Frost Trap, Hand of Freedom); a kiter at the far wall can
  be out of heal reach of the inner ring. A bloated carrier keeps the Unbound Plague.
- One driver: with one bot tank nobody drinks, and a dead holder in 10-man is picked up by nobody while the
  other tank rides. The driver ignores hazards; the abomination's raid damage isn't weighed.
- Sharers are only Death Knights and Retribution paladins; a sharer dying heals him by its stacks. After a
  share the sharer stays braked until the tank's threat is 25% clear of it (`pp.brake`). A human holder is
  relieved on the bots' rotation but told only its buttons.
- A hold counts applications, not stacks: a tank taking him back at 2 holds to 4, the count WT says never
  to reach (`--plague` peaks).
- A plague landing in the melee cluster can jump before its carrier steps out (1 s); a rooted carrier draws
  the ooze stack onto itself; a holder that catches it walks him onto its receiver.
- In P3 puddles fill the room; the spot takes the least covered one, no wall drag.
- At the pull the rings form round the centre while he is still at the table, 39 yd from the healers.
- The bomb blast timing is read off the DBC, unconfirmed.
- Humans hold no duty; carriers among them are avoided.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `pp.phase` | per bot | `1`, `2`, `3` from his health |
| `pp.target` | per bot | `skull`, `ooze`, `cloud`, `boss`, `brake`, `variable ooze`/`cloud`/`boss` |
| `pp.spot` | raid | The boss spot, 1-17 |
| `pp.slot` | per bot | 1-14 the 12 yd ring, 15-38 the 22 yd; `0` released |
| `pp.stack` | per bot | The rooted victim it stands on, `none` |
| `pp.kite` | kiter | Leg branch `away`, `wall`, `cross`; `none`, `locked` |
| `pp.driver` | raid | The driver's guid, `0` none |
| `pp.abom` | driver | `drink`, `spit <unit>`, `eat <puddle>`, `slash <unit>`, `none` |
| `pp.swap` | per tank, sharer | `take`, `share`, `back`, `pickup`, `none` |
| `pp.sharedefensive` | sharer | The button it raises before taking him |
| `pp.brake` | per bot | Held past 90% of the holder's threat |
| `pp.plague` | per carrier | `hold`, `clear`, `pass`, `none` |
| `pp.receiver` | carrier → bot | Heroic: the bot it walks onto; `0` released |
| `pp.dodge` | per bot | `<hazard> <branch> <yards>`; `hold`, `<hazard> none`, `<hazard> locked`, `pinned` |
| `pp.defensive` | per tank | The ladder's pick, `idle` outside a window |
| `pp.humancall` | per sender | The button or taunt a human tank was told to press |

`tools/botobs/bosses/professor_putricide.py`; its banner names declared keys the pull never wrote, sparing
`pp.receiver` on normal: `--phases`, `--adds` (each ooze victim with its stack count, each bloat with its kite
branches), `--abom` (driver, duty switches, Regurgitated Ooze), `--plague` (Mutated Plague peaks, swap
elections, braked bots, heroic carriers and their branches), `--dodges`, `--spread`, `--cheats`.

Next pull, 25N: every ooze victim with 5+ stackers, no bloat ending in a death, no puddle older than 15 s in
P1-P2, no Mutated Plague carrier above 3 stacks, no goo cast in P2 while everyone holds a ring slot. 25H: every
Unbound Plague carrier passing before tick 12, nobody dying with it.

# r11-svalna-valithria — Sister Svalna and her gauntlet, Valithria Dreamwalker

Wave 3e, parallel with `r10-blood-queen`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout". It may split.

## Encounters

- **Sister Svalna** and the gauntlet before her: slug `sister-svalna`, prefix `ss.`, boss id 9, stem
  `Svalna`. Core `icecrown_citadel.cpp`. Doc `sister-svalna.md`, reader `sister_svalna.py`. Guide:
  the mischiefmagicandrocketscience blog.
- **Valithria Dreamwalker:** slug `valithria-dreamwalker`, prefix `vd.`, boss id 10, stem `Valithria`.
  Core `boss_valithria_dreamwalker.cpp`. Doc `valithria-dreamwalker.md`, reader
  `valithria_dreamwalker.py`. Guides in the cache: Warcraft Tavern 25 and 10, instant-carry's raid
  overview, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function.

- **Svalna:**
  - 66 lines: a spear click, and a spear throw while she is shielded (Aether Shield).
  - Every bot within 100 yd, tanks and healers included, runs for the spear at emergency priority.
- **Valithria:**
  - The action stem was 1272 lines (`ICCActions_VT.cpp`).
  - What is handled today:
    - add taunts and flees, and marks (25-man splits the room in halves);
    - the zombie kite;
    - a healer ranking, which counts humans and never ranks a selfbot;
    - portals, a heal rotation and dream clouds.
  - 7 icon writes. 5 `FleePosition` calls (Mana Void, the exploding zombie).
  - `IccValithriaDreamCloudMultiplier`:
    - always: Follow and Formation;
    - chased by a zombie: everything but the kite;
    - in Dream State above 50% health: everything but the cloud action;
    - icon and assist filters by role.
  - Cheats:
    - `vd.dreamteleport`: a same-map `TeleportTo` onto a human in Dream State;
    - `vd.cloudteleport`: the same onto the shared cloud.
    - Both drop combat. With the raid cheat off, a Dream State bot hovers.
  - The floor snaps (`NearTeleportTo`) skip fall damage; they're borderline.
  - Its 12 raw-id casts are all guarded by known-spell checks.
  - Heroic Nightmare portals and Twisted Nightmares are handled.
  - 4 positions and 4 Z constants.

## Guide answers to build on

- **Portals** spawn every 45 s, open 15 s later, and take up to 8 players in 25. 1-2 healers stay
  outside.
- **Dream State** is 20 s of free flight. Fly into the clouds together for Emerald Vigor, take a
  cloud at once as old stacks run out, and fly low before Dream State ends.
- **Setup:** 5-10 healers and 1 tank for the abominations. Ranged kite the zombies.
- **Kill order:** Skeletons and Archmages, then Worms and Suppressers, then Zombies and Abominations.
- **Heroic:** Twisted Nightmares replaces Emerald Vigor, a stacking DoT, so healers heal each other.
- **Svalna:** one raider throws a spear to break Aether Shield.

## Focus

- Real flight in Dream State: a flight mover through the portal and between clouds, from the
  script's Dream State and flight auras. The floor at the end, without a snap.
- The healer split inside and outside, ranking bots only.
- The add kill order by direct targeting.
- Mana Void, Column of Frost and zombie dodges on hazard models.
- Svalna: one elected spear thrower, and the gauntlet's captains.

## Owns

Stems `Svalna` and `Valithria` and their definitions, both docs, both readers and their tests, this
brief.

## Task list

Not split: Svalna is two small duties, and Valithria's parts share one scan, one state and one helper
file.

1. Svalna: an elected spear puller per Impaling Spear (`ss.spear`), the Infernal Spear thrown only
   into Aether Shield (`ss.hurl`), a resolver over the undead captains and her (`ss.target`), tanks on
   the captains.
2. Valithria state: one `RaidInstanceState` (scan, healer split `vd.healer`, portal claims
   `vd.portalclaim`, abomination owners `vd.tank`), reset when her state leaves `IN_PROGRESS`.
3. Valithria targeting: resolver over the add tiers with the skull first (`vd.target`); icon writes,
   `rti` and the multiplier removed.
4. Valithria hazards: forced dodge of Column of Frost and dying zombies, Mana Void step-out, both on
   `FindNearestPositionClearOfHazards` inside the probed floor disc (`vd.dodge`).
5. Valithria zombies: ring kite at 25 yd round her (`vd.kite`), one snare per chasing zombie
   (`vd.snare`), tanks taunting zombies and worms off healers.
6. Valithria healers: bots only, raid healers outside, the rest claim portals and heal her by name.
7. Dream State: a flight mover (refresh, gather, shared cloud, follow a human, descend before the
   end, fall with real damage when caught airborne), replacing `vd.dreamteleport`, `vd.cloudteleport`
   and both floor snaps (`vd.dream`, `vd.cloud`).
8. Readers, tests, both docs.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Kill order skeleton and archmage, then worm and suppresser, then zombie and abomination; nearest to Valithria within a tier, current target kept inside its tier | Warcraft Tavern 25 (base); Wowhead ranks suppressers second | Wowhead's five-step order |
| Melee never take a zombie; DPS take an abomination only once a tank holds it | Wowhead (melee one-shot, abomination the only add needing a tank) | Melee on zombies with a threat brake |
| Raid healers: 1 on 10-man, 2 on 25-man, never the last bot healer; paladins last for the raid duty (Beacon on her) | Warcraft Tavern 10/25, Wowhead heroic tip | Old druid-first ranking |
| Raid healers heal her too while everyone within 40 yd is above 85% | Instant-carry: all healers on her at first | Raid healers never on her |
| Dream: pop the nearest cloud, then gather on the lowest-guid refreshed bot and take shared clouds; follow a human dreamer instead | Wowhead, Warcraft Tavern | Each bot alone |
| Descend 3.5 s before Dream State ends; a bot still airborne falls through `MoveFall` and `HandleFall`, taking real fall damage | Wowhead (fly low) | Snap to the floor |
| Zombie victims kite a 25 yd ring round her, 30 degree legs, direction away from the zombie; a tank holding an abomination face-tanks | Script speeds (zombie 5 yd/s, player 7); pure pursuit settles ~17 yd behind on that ring | Free-field kite |
| One snare per chasing zombie (Chains of Ice, Concussive Shot, Frost Shock), nearest able non-healer bot | Warcraft Tavern, Wowhead | Roots, old `IccTryClassCC` with melee and fears |
| Tanks own abominations (`vd.tank`), taunt worms and zombies off healers, generic taunts blocked | Warcraft Tavern, Wowhead | Generic tank AI |
| No tank defensive ladder on either encounter: no boss hits a tank | Convention 43 is about boss hits | 40% valve as on the council |
| No Valithria lust or burst hook: the base gate holds lust off non-boss adds and a victimless friendly boss | `consumables-and-burst.md` | A hook that cannot release |
| Resist hook fire, frost, nature (Lay Waste, Frostbolt Volley, Acid Burst and Corrosion), for the merge stage to wire | Script schools | None |
| No opening positions on either: Valithria's archmages and Svalna's landing start the fight | Script | Pre-pull stands |
| Svalna: the nearest bot DPS, else healer, else a free tank pulls the spear; a holder throws only into Aether Shield, walking to 18 yd past 24 | Blog, script (Hurl Spear 25 yd, through immunity) | Everyone runs for it |
| Svalna kill order Arnath, Rupert, Grondel, Brandon (not while Divine Shield), then her when unshielded | Blog: either order; Arnath heals and casts Dominate Mind on 25-man | Svalna first |
| Svalna gauntlet trash stays on the generic AI | Scope decision; blog | Encounter nodes for the Ymirjar |

## Carried over

- Merge stage (done): the `HookRows()` row, the README updates, and `PlayerbotAI::CastSpell`'s
  Dream State 70766 exemption from its `IsFlying()` refusal, so heroic dreamers heal each other.
- r14: `IcecrownHelpers::vtCloudSync` and `vtPortalClaim` (`ICCScripts.h`) and `IccTryClassCC`
  (`ICCHelpers_Shared`) have no caller left; `encounter.py` still needs its `icc valkyre spear` row,
  which the spear trigger keeps.
- Rebuild needed, no CMake re-run (no `.cpp` added or removed); no 631 trace; no `-Werror` pass beyond
  pb-check's two jobs.

## Known gaps

In the boss docs: Svalna's Ymirjar on the generic AI and untraced, 25-man Dominate Mind generic;
Valithria's lust and Guardian Spirit, the slowed kiter, a dreamer at the win left flagged in the air; no tank ladder or opening
positions on either; humans hold no duty.

## Blocked

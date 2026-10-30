# g11-svalna-valithria — close Sister Svalna's and Valithria Dreamwalker's gaps

Gap wave `3efg-gaps`, after `3cd-gaps`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap
lanes" section included. Builds on the merged `r11-svalna-valithria` lane and its review: read
[r11-svalna-valithria.PLAN.md](r11-svalna-valithria.PLAN.md) first. Its decisions stand unless
closing a gap needs a change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/sister-svalna.md`,
`docs/raids/icecrown-citadel/valithria-dreamwalker.md` and r11's brief.

- The Svalna gauntlet is in scope (the overview's Scope row), though r11 left its Ymirjar waves on the
  generic AI. Build it per the boss lane recipe. Her boss state stays `NOT_STARTED` until she engages,
  so it needs its own gate, as `UnitGate` gives Stinky and Precious (`ICCDefinition_Dogs.cpp`). It also
  needs a trace slug per the ICC README's "Boss states and trace names" (the Blood Prince trash's
  `RaidObs::MarkPull` in `ICCScripts.cpp` is the pattern for a pull with no boss state), a reader and
  its tests. Sources: the core script for the waves and their spells, and
  `build-icc-wt/_guides/text/sister-svalna.blog.md`.
- Valithria's healer cooldowns (Guardian Spirit, and Beacon of Light or Earth Shield on her): no shared
  dealer exists for a single-target cooldown, so the answer is ICC-local.
- A new definition or stem file is this lane's. Its registration (`ICCDefinitions.h`, the root
  strategy's `InitMultipliers`, the umbrella headers, `ICC_PREFIXES` in
  `tools/botobs/raidobs/encounter.py`, `tools/botobs/tests/test_icc_naming.py`) goes in Carried over for
  the merge stage, as `r12-sindragosa` did.

## Owns

Stems `Svalna` and `Valithria` and their definitions, any gauntlet stem and definition this lane adds,
both docs, the readers `sister_svalna.py` and `valithria_dreamwalker.py` (plus any gauntlet reader) and
their tests, this brief.

## Task list

One lead, no split: the gauntlet shares the Svalna stem's helpers and the Valithria parts share one state
and scan.

1. Svalna: interrupt duty on Arnath's Dominate Mind (25-man) and Flash Heal (`ss.interrupt`); tank valve
   for a bot tank a captain hits (`icc sister svalna tank defensive`, `ss.defensive`, `ss.humancall`).
2. Svalna gauntlet, definition `IccSvalnaGauntletDefinition` (`UnitGate` on the five Ymirjar, trace
   `svalna-gauntlet` through `IccMarkPull`), nodes `icc sister svalna gauntlet` dodge, interrupt, taunt, tank
   defensive, target: resolver by role (`ss.gauntlet.target`), rescue taunt (`ss.gauntlet.taunt`),
   Deathbringer interrupts (`ss.gauntlet.interrupt`), Whirlwind and Frozen Orb dodges
   (`ss.gauntlet.dodge`), tank valve (`ss.gauntlet.defensive`, `ss.gauntlet.humancall`).
3. Valithria push: lust by one shaman bot at 80%+ after a dream (`vd.push`), Guardian Spirit on her
   (`icc valithria guardian spirit`), Beacon of Light on her (`icc valithria beacon`, generic beacons
   blocked).
4. Valithria: Frostbolt Volley interrupt duty (`vd.interrupt`), freedom for a slowed kiter
   (`vd.freedom`), abomination tank valve (`vd.defensive`, `vd.humancall`).
5. Valithria post-win landing: a gate open past `DONE` for a bot still flagged in the air.
6. Readers (`sister_svalna.py`, new `svalna_gauntlet.py`, `valithria_dreamwalker.py`), tests, both docs.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Arnath's Dominate Mind and Flash Heal get one elected kicker per cast, Dominate Mind first, interrupts and silences | script: 2 s casts, Arnath immune to fear only | Deathwhisper's CC ladder on the dominated player |
| A landed Dominate Mind gets no CC duty | script: 15 s charm every 28-37 s behind a 2 s cast | Cyclone and Polymorph on the victim |
| Captain and gauntlet tanks: a valve under 40%, class buttons held to 25%, human tank called | convention 43; no captain or Ymirjar hit is predictable | none (r11) |
| Gauntlet gate `UnitGate` on Battle-Maiden, Deathbringer, Frostbinder, Huntress, Warlord, Corrupted Ymirjar, Warhawk; slug `svalna-gauntlet`, keys `ss.gauntlet.*`, trigger prefix `icc sister svalna gauntlet` (owned by `sister-svalna` until its `ICC_PREFIXES` row lands) | brief (dogs' gate, `MarkPull`); `sindragosa-gauntlet` naming | a gate on Crok's escort; prefix `icc svalna gauntlet`, which no row owns, so `test_icc_naming.py` fails before the merge |
| Gauntlet rows but the dodge sit below Svalna's (`ACTION_RAID + 1-4`) | her captains rise while Corrupted Ymirjar may still fight; equal relevances pop in insertion order | ties at `+5`/`+6` |
| Gauntlet kill order: ranged Deathbringer first, melee Frostbinder first, then the other caster, Huntress, Battle-Maiden, Warlord, Corrupted Ymirjar and Warhawk, nearest the bot in a tier; area damage left to the generic pickers | blog | one order for everyone |
| One elected rescue taunt per Warlord, Battle-Maiden or Corrupted Ymirjar on a non-tank, generic taunts free | Sindragosa gauntlet; script (casters keep 16 yd) | tank roster per add |
| Deathbringer interrupts: Summon Undead before Empowered Shadow Bolt, interrupts and silences | script: both 2 s casts, its AI stops on silence | class interrupt nodes |
| Non-tanks leave a Warlord's 8 yd Whirlwind during its 2 s cast; everyone leaves a Frostbinder's 6 yd Frozen Orb blast in the last 3 s of its 10 s aura | script and DBC (41056/41057, 71274 ticks once at 10 s into 71286 at the Frostbinder) | generic avoid aoe, which sees no cast |
| Arctic Chill (5k frost a second within 10 yd of a Frostbinder) gets no answer | blog: melee on Frostbinders | ranged only on them |
| No gauntlet opening positions | Crok's escort and the player's pulls start each pack | pack stands |
| Lust by one shaman bot once she is at 80%+, no bot healer dreams and a healer carries Emerald Vigor; the node casts it directly | WT25, Wowhead; the base burst gate holds lust off non-boss adds and a friendly boss | an entry exemption in `BurstCooldowns.cpp` (shared) |
| Guardian Spirit on her in that window, one priest bot at a time | WT25, Wowhead | on cooldown |
| Beacon of Light on her for raid-duty paladin bot healers and every one on heroic, the generic beacon nodes blocked for them; Earth Shield stays on the tank | WT25 and Wowhead heroic tips; she carries `CREATURE_TYPE_FLAG_TREAT_AS_RAID_UNIT`, so Beacon's raid target holds her | every paladin; Earth Shield on her (no guide asks, she is rarely hit) |
| Frostbolt Volley: one elected kicker per casting archmage, interrupts only | WT25, Wowhead; archmages are silence immune | class interrupt nodes on the target |
| A kiter slowed by Volley: its own Hand of Freedom, Cleanse or Dispel Magic, else the nearest paladin bot's Hand of Freedom, else the nearest priest's Dispel Magic or paladin's Cleanse | DBC (snare, magic); Rotface's freedom duty | reverse the kite |
| Abomination tank valve under 40%, 50% under Gut Spray | convention 43; Gut Spray +25% physical | none (r11) |
| The definition stays open past `DONE` while a bot is still flagged in the air within 100 yd of her place, so it lands with real fall damage | script: `DONE` ~6.5 s after 100%, Dream State runs 20 s | leave the flags |
| No Valithria opening positions | script: the archmages channel on her and pull on approach | stands round her |
| A bot may still claim the portal a human takes | Humans decision | skip spots near a human |

All tasks done. Coordinates: her spawn (4203.65, 2483.89), the landing reference, settles at 364.873;
three 12 yd rings over the hall settle at 351.10 (y 2632, 2590) and 358.43 (y 2520, its east edge off the
mesh). No stand added or moved, so no new sight line to check.

## Carried over

- Merge stage, registration: `IccSvalnaGauntletDefinition` declared in `ICCDefinitions.h` and listed
  after `IccSvalnaDefinition`; its `AddMultipliers` after Svalna's in `RaidIccStrategy::InitMultipliers`
  (its rules are multipliers); no umbrella change (the rows live in the Svalna stem); no `HookRows()`
  row.
- Merge stage, `encounter.py`: `ICC_PREFIXES` `"icc sister svalna gauntlet": "svalna-gauntlet"` ahead of
  `"icc sister svalna"`, `ICC_KEY_PREFIXES` `"svalna-gauntlet": "ss"`; then `test_icc_naming.py`
  `NODE_PER_PREFIX` gains an `icc sister svalna gauntlet ...` node.
- Merge stage, README: the naming table's row (`—`, Svalna gauntlet, no boss state, `svalna-gauntlet`,
  `ss.`) and the `MarkPull` sentence; "Code layout" (the Svalna stem's two definitions, the gauntlet's
  `UnitGate` entries in the gates paragraph); direct targeting user `GetSsGauntletTarget`; defensives
  users `icc sister svalna tank defensive`, `icc sister svalna gauntlet tank defensive`, `icc valithria
  tank defensive` (a captain, Ymirjar or abomination tank under 40%, 50% under Gut Spray, class buttons
  held to 25%, `ss.defensive`/`ss.humancall`, `ss.gauntlet.defensive`/`ss.gauntlet.humancall`,
  `vd.defensive`/`vd.humancall`); trigger names (Svalna interrupt and tank defensive, the gauntlet's
  dodge, interrupt, tank defensive, taunt, target; Valithria interrupt, freedom, tank defensive, lust,
  guardian spirit, beacon); the taunt paragraph (the gauntlet's one elected rescue taunt per melee add on
  a non-tank, generic taunts free); the "Before live" row for Valithria (`VdGate` stays open past `DONE`
  for a bot still flagged in the air near her spawn, which lands).
- CMake re-run: one new `.cpp` (`Definition/ICCDefinition_SvalnaGauntlet.cpp`).
- r14: `IsTankPlayer`, `IsDutyBot`, `MembersOnMap`, `SlotCreature`, `CanPayFor` and the kick pick
  (`ReadyKick`) are copied across Svalna, Valithria and Deathwhisper; `IccSsWalkAction` copies
  `IccVdWalkAction`; r11's `vtCloudSync`/`vtPortalClaim` and `IccTryClassCC` remain.

## Known gaps

- Those in both boss docs.
- No 631 trace. The user built `623638b99` clean; this change needs a rebuild with a CMake re-run; no
  `-Werror` pass beyond `pb-check`.

## Blocked

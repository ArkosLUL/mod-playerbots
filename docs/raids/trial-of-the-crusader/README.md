# Trial of the Crusader (map 649)

Strategy key **`trialofthecrusader`**, one strategy for all five encounters. Cross-raid conventions
are in [../README.md](../README.md). Mechanics source:
`src/server/scripts/Northrend/CrusadersColiseum/TrialOfTheCrusader/`.

**`wotlk-toc` is Trial of the *Champion*** (the 5-man dungeon, map 650), **not the raid.**

| Boss | Doc | Code stems | Trace slug | Note-key prefix |
|---|---|---|---|---|
| Northrend Beasts | [northrend-beasts.md](northrend-beasts.md) | `Gormok`, `Jormungars`, `Icehowl`, `NorthrendBeasts` | `northrend-beasts` | `nb.` |
| Lord Jaraxxus | [lord-jaraxxus.md](lord-jaraxxus.md) | `Jaraxxus` | `lord-jaraxxus` | `jaraxxus.` |
| Faction Champions | [faction-champions.md](faction-champions.md) | `FactionChampions`, `FactionChampionsDefence` | `faction-champions` | `fc.` |
| Twin Val'kyr | [twin-valkyr.md](twin-valkyr.md) | `TwinValkyr` | `val-kyr-twins` | `tv.` |
| Anub'arak | [anubarak.md](anubarak.md) | `Anubarak` | `anub-arak` | `anub.` |

Raid-wide notes use `toc.`. Bot-tractable (avoidance, positioning, kill order, interrupts): Beasts,
Jaraxxus, Faction Champions, Anub'arak P1/P3. Harder, needing new state tracking: the Val'kyr
essence system and Anub'arak spike-kiting.

## Instance script

`instance_trial_of_the_crusader.cpp` keeps boss state since core #28117 (`SetBossNumber(5)`: Beasts
0, Jaraxxus 1, Faction Champions 2, Twins 3, Anub'arak 4) and no object data. The module reads three
things:

- **Stage**, `GetData(TYPE_INSTANCE_PROGRESS = 1)`. A wipe never lowers it.

  | Stage | State | Encounter |
  |---|---|---|
  | 0 | before the intro | Beasts |
  | 1 | intro done | Beasts |
  | 2 | Beasts dead, Fizzlebang intro (Jaraxxus not attackable) | Jaraxxus |
  | 3 | Jaraxxus released; after a wipe he respawns at the centre, aggressive | Jaraxxus |
  | 4 | Jaraxxus dead | Faction Champions |
  | 6 | champions dead | Twin Val'kyr |
  | 8 | twins dead, Lich King scene | none |
  | 9 | floor broken, Anub'arak spawned | Anub'arak |
  | 10 | done | none |

- **`IsEncounterInProgress()`**, `InstanceScript`'s own: true while any boss state is `IN_PROGRESS`.
  It goes true before most pulls: from the end of the Beasts intro (stage 1) through every walk-in
  until the last beast dies, at the champions' release, 3.25 s before the twins turn aggressive.
  Jaraxxus and Anub'arak set it on engage. An evade (`TYPE_FAILED`) or an all-dead cleanup sets every
  `IN_PROGRESS` boss back to `NOT_STARTED`.
- **`GetGuidData`**: Gormok (4), Dreadscale (6), Acidmaw (7), the twins by NPC entry (34497 Fjola,
  34496 Eydis), Anub'arak (11, also while submerged). Icehowl, Jaraxxus and the champions only by
  entry. Resolve with `map->GetCreature(GetGuidData(type))`; `InstanceScript::GetCreature(type)`
  reads the unused object-data registry. A guid resolves map-wide, so an "encounter running" test
  also needs the unit in combat ([pitfalls.md](../../engine/pitfalls.md)).

## Arena floor

Every encounter before Anub'arak stands on gameobject 195527 (Argent Coliseum Floor, display 9059
`Coliseum_Intact_Floor.wmo`, destroyed for Anub'arak). The static mesh and vmaps have nothing there:
navprobe finds no poly or height at `ARENA_CENTER`, and rings out to 55 yd hit only the stands, Z
417-458. **navprobe cannot verify an arena point**, so anchor only on the script's own points. Live,
the object's collision gives Z, so `FindNearestPositionClearOfHazards` and `MoveTo` work: a move
takes `PathGenerator`'s player shortcut (`PATHFIND_NORMAL|PATHFIND_NOT_USING_PATH`, a straight line),
and `CheckCollisionAndGetValidCoords` clips a point at the wall with the static LOS ray.

## Spell ids by difficulty

ToC's remaps live only in the client DBC
(`modules/mod-spell-tweaks/data/dbc-reference/spelldifficulty.reference.csv`), not in
`acore_world.spelldifficulty_dbc`; the core merges both. The `Spell` constructor remaps every cast
to the map's difficulty, so auras, current spells and stack counts carry the difficulty's id while
the script names the 10N one. Read one through `sSpellMgr->GetSpellIdForDifficulty(SPELL_X, unit)`
(any unit on the map) or list all four.

| Spell | 10N | 25N | 10H | 25H | Notes |
|---|---|---|---|---|---|
| Impale (Gormok) | 66331 | 67477 | 67478 | 67479 | stacking bleed on the tank |
| Sweep | 66794 | 67644 | 67645 | 67646 | worm cast, 1.5 s, 15 yd knockback |
| Paralytic Toxin | 66823 | 67618 | 67619 | 67620 | player debuff |
| Massive Crash | 66683 | 67660 | 67661 | 67662 | |
| Frothing Rage | 66759 | 67657 | 67658 | 67659 | on Icehowl |
| Arctic Breath | 66689 | 67650 | 67651 | 67652 | `spell_cone` row on 10N only |
| Fel Fireball | 66532 | 66963 | 66964 | 66965 | 2.5 s cast |
| Incinerate Flesh | 66237 | 67049 | 67050 | 67051 | heal absorb |
| Nether Power | 66228 | 67106 | 67107 | 67108 | on Jaraxxus |
| Legion Flame | 66197 | 68123 | 68124 | 68125 | |
| Light Essence | 65686 | 67222 | 67223 | 67224 | |
| Dark Essence | 65684 | 67176 | 67177 | 67178 | |
| Light Vortex | 66046 | 67206 | 67207 | 67208 | Fjola, 8 s cast |
| Dark Vortex | 66058 | 67182 | 67183 | 67184 | Eydis, 8 s cast |
| Touch of Light | 65950 | 67296 | 67297 | 67298 | heroic only, so 67297/67298 |
| Touch of Darkness | 66001 | 67281 | 67282 | 67283 | heroic only, so 67282/67283 |
| Twin's Pact | 65876 | 67306 | 67307 | 67308 | Fjola, 15 s cast |
| Twin's Pact | 65875 | 67303 | 67304 | 67305 | Eydis |
| Penetrating Cold | 66013 | 67700 | 68509 | 68510 | |
| Impale (spike) | 65919 | 67858 | 67859 | 67860 | |
| Leeching Swarm | 66118 | 67630 | 68646 | 68647 | on players only, see [anubarak.md](anubarak.md) |
| Permafrost | 66193 | 67855 | 67856 | 67857 | never on a sphere, see [anubarak.md](anubarak.md) |

No row, one id everywhere: Burning Bile 66869, Pursued by Anub'arak (Mark) 67574, Submerge 65981,
Frost Sphere 67539, Freezing Slash 66012.

Nodes keyed on these, each live on a difficulty only through the remap or all four ids:

| Node | Keys on |
|---|---|
| `gormok tank swap needed` | Impale stacks |
| `northrend worms misplaced` | Paralytic Toxin, Burning Bile, Sweep cast, Spew cone |
| `icehowl frothing rage` | Frothing Rage |
| `icehowl breath spread` | the remapped Arctic Breath's cone width |
| `jaraxxus incinerate flesh on raid` | Incinerate Flesh |
| `jaraxxus fel fireball interruptible` | Fel Fireball cast |
| `jaraxxus nether power active` | Nether Power |
| `jaraxxus legion flame nearby` | Legion Flame |
| `twin valkyr needs base essence`, `… vortex requires essence`, `… touched requires essence`, `… shield requires essence` | essences, Vortex casts, Touch, shields |
| `twin valkyr pact interrupt duty` | Twin's Pact cast |
| `anubarak dps target` | Permafrost on the burrower |
| `anubarak penetrating cold on raid` | Penetrating Cold |
| Anub'arak burst row | Leeching Swarm, lust only in phase 3, latched per instance |

## Code layout

`src/Ai/Raid/ToC/` splits per stem: `Action/ToCActions_<Stem>`, `Trigger/ToCTriggers_<Stem>`,
`Multiplier/ToCMultipliers_<Stem>`, `Util/ToCHelpers_<Stem>`. `NorthrendBeasts` holds what the three
beasts share, `Shared` what several encounters use. `Definition/ToCDefinition_<Encounter>.cpp` holds
an encounter's rules: an `EncounterDefinition` with gate `ToCGate` and no rows or tick, its accessor
declared in `Definition/ToCDefinitions.h`. The raid-wide stems `Staging` and `ResistAura`
([below](#pre-pull-staging)) belong to no encounter: their names take no encounter prefix, so their
triggers run ungated, and their definitions take `ToCRaidWideGate` (always open, never live), id
`ToCEncounter::None` and no trace name.

**Seams.** A boss gains a trigger, action or multiplier by editing only its own stem:

- trigger: class plus `creators[...]` entry in `ToC<Stem>TriggerContext`, node in
  `AddToC<Stem>TriggerNodes` (both `Trigger/ToCTriggers_<Stem>.h`);
- action: class plus entry in `ToC<Stem>ActionContext` (`Action/ToCActions_<Stem>.h`);
- multiplier: `AddToC<Stem>Multipliers` (`Multiplier/ToCMultipliers_<Stem>.h`);
- rule: the encounter's Definition file, wired in `ToCStrategy::InitMultipliers` right after its
  stem multipliers, since a trace credits the first rule or multiplier that zeroes an action;
- burst row: each encounter stem (`NorthrendBeasts`, `Jaraxxus`, `FactionChampions`, `TwinValkyr`,
  `Anubarak`) answers `ToC<Stem>BurstWindow` with a `ToCBurstWindow`. `ToCBurstWindowMultiplier`
  (`Multiplier/ToCMultipliers_Shared.{h,cpp}`) applies the live encounter's row to in-combat burst
  cooldowns, mana-return ones exempt: `allowLust` to Bloodlust/Heroism, `allowAll` to the rest.
  Every row ANDs with the shared tank-held-boss gate.

The root only dispatches: `ToCStrategy.cpp` calls every stem's node and multiplier list and every
Definition's rules, and `ToCActionContext.h`/`ToCTriggerContext.h` merge the stem contexts. The four
registration sites name only these root classes. **Dispatch order is behaviour**: equal relevances
pop in insertion order.

**Encounter gate** (`Util/ToCEncounterGate.{h,cpp}`), read from the instance script once per
instance per ms. Off map 649 the gate is open and nothing is live. `ToCGate` adapts both to an
`EncounterGate` (id `static_cast<uint32>(ToCEncounter::X)`).

- **Open** (`ToCEncounterGateOpen`, gates triggers and Definition rules): the stage's encounter is
  this one, pulled or not, so pre-pull prep (an essence before the twins) runs. Beasts at stage 2
  and Jaraxxus at stage 4 also stay open until the next encounter is live: the kill moves the stage
  while adds live on (snobolds riding players when Gormok dies last, the Mistress and Infernals
  Jaraxxus's portals and volcanoes summoned), and each trigger tests its own units.
- **Live** (`ToCEncounterIsLive`, `ToCLiveEncounter`, gates multipliers and burst rows): also
  `IsEncounterInProgress()` and one of its units alive and in combat, which keeps walk-ins, the
  Fizzlebang intro and the Lich King scene closed. Icehowl, Jaraxxus and the champions are found by
  entry within 200 yd of the bot, since the arena reaches about 80 yd from `ARENA_CENTER`.
- `ToCTriggerContext.h` wraps every trigger whose name leads with `gormok`, `northrend worms`,
  `icehowl`, `jaraxxus`, `faction champions`, `twin valkyr` or `anubarak` in `ToCGatedTrigger`,
  which returns no event while closed. A raid-wide trigger takes none of these prefixes.
- A wrapped trigger firing while live calls `RaidObs::NamePull` with the trace slug. The engage hook
  names every ToC trace first, so this only renames one it left under the map name.

**Constraints.**

- pblint reads a `creators["..."]` key only after a one-line `class X : public NamedObjectContext<…>`
  in the same file, so a stem context keeps its constructor inline in its header. A trigger's
  `: Trigger(botAI, "name")` stays on one line for the same reason.
- `src/Ai/Dungeon/TOC/TOC*.h` (Trial of the Champion) collide case-insensitively with raid `ToC*.h`.
  Hence the raid-unique umbrella headers `ToCRaidActions.h`, `ToCRaidTriggers.h`,
  `ToCRaidMultipliers.h`, and `BuildShared{Action,Trigger}Contexts.cpp` including the root contexts
  by path. A new header needs a name no dungeon header has, ignoring case.
- `Util/ToCData.h` holds only raid-wide values: map id, `ARENA_CENTER`, `ANUBARAK_PIT_CENTER`, the
  `ToCNpcs` and `SPELL_*` mirrors. A new boss-specific id goes in the stem's
  `Util/ToCHelpers_<Stem>.h`. Spell constants are plain `constexpr uint32 SPELL_*`; remap one as
  `sSpellMgr->GetSpellIdForDifficulty(SPELL_X, unit)` at the call, never inside a wrapper: pblint's
  remap check reads that bare identifier.
- Shared bases (`Action/ToCActions_Shared.h`): `ToCMainTankHoldAction::DragBossToAnchor` backs the
  tank 5 yd at a time toward an anchor while it tanks the boss and stands more than 12 yd from it;
  the boss follows, so it settles up to melee reach past that, possibly on a hazard. Where the spot
  matters, measure arrival on the boss (Anub'arak's pre-submerge drag). `ToCDirectAttackAction` is
  the base of every resolver action ([below](#targeting-is-direct-no-raid-icons)).
- Shared helpers (`Util/ToCHelpers_Shared.h`): `IsToCDpsBot` (not a tank by role or spec, not a
  healer) is the role every DPS resolver and its rule test; `GetToCHumanSkull` reads the human
  skull and `ToCPetOffPick` is the resolver triggers' pet clause
  ([below](#targeting-is-direct-no-raid-icons)).
- `AvoidCreatureClusterAction` is the legacy `FleePosition` dodge, capped at
  `Playerbots.FleeDistance` (5 yd), with no caller left. A new hazard uses `FindNearestPositionClearOfHazards` with a clearance past the trigger radius, per
  [pitfalls.md](../../engine/pitfalls.md).
- `IsBotInFrontalCone` and `CastClassTaunt` live in `src/Util/EncounterHelpers.{h,cpp}`; ToC keeps no
  copies.

## Targeting is direct, no raid icons

Every encounter picks each bot's target in code and attacks it ([ignis.md](../ulduar/ignis.md)).
Bots mark no kill target.

- **Resolver.** One function per encounter returns the bot's pick, or null outside its window or
  role, and notes it through `RaidObs::NoteDerived` (change-only, per bot). Its trigger fires while
  the pick exists and isn't `current target`, or the pet is off it (`ToCPetOffPick`); its action
  derives from `ToCDirectAttackAction` and calls `AttackDirect(pick)`.
- **Rule.** The encounter's DPS `OwnTargeting` predicate is `resolver(botAI) != nullptr`, so rule
  and resolver cover the same window by construction. This breaks the raid-wide
  [whole fight](../README.md#targeting-the-whole-fight) rule on purpose: no ToC resolver ends in a
  terminal fallback, so a wider rule would leave a bot with no target source. Where the resolver
  has nothing, `dps assist` takes over: Anub'arak's phase 2 with no scarab within 30 yd, and the
  1.5 s between the two worms' submerges.
- **Settling and pets.** `AttackDirect` commands the pet on every call, since no generic strategy
  wires `pet attack`, then returns false once the bot is on the pick so lower nodes run. The
  trigger's pet clause sends back a resummoned, revived or drawn-off pet at no tick's cost.
- **Pre-pull.** Every resolver branch needs an in-combat unit: `Attack()` has no out-of-combat guard
  and reaches 100 yd ([kologarn.md](../ulduar/kologarn.md)).
- **Human skull.** Each resolver's first branch is `GetToCHumanSkull`: the group skull on a unit of
  the encounter that is alive, in combat, selectable and attackable by the bot. Bots here place no
  skull, so it's a player's, unless a bot runs `mark rti`. `attack rti target` is wired into no
  generic strategy, so without this read a player's mark does nothing.
- **CC icons.** Faction Champions' CC icons and `rti cc` are the only icons bots still place or
  follow: `rti cc` is the generic class CC pipeline's only input (`RtiCcTrigger`,
  `RtiTriggers.cpp:25-36`; `CcTargetValue.cpp:29`) and never drives DPS target choice.
- **Legacy `rti`.** `rti` is saved per bot, so the strategy's first `OnTick` on map 649 resets the
  tags older builds set (`cross`, `square`, `triangle`, `diamond`) to `skull`
  (`ToCRestoreLegacyRti`). Once per strategy object, so a player's later `rti` order sticks until
  the strategy is rebuilt.
- **No icon clear at a pull.** Group icons live in memory only (`Group::SetTargetIcon` never writes
  the DB), so no icon from an older build survives a restart, and a clear could only remove a
  player's marks.

## Pre-pull staging

Where each boss turns hostile (`trial_of_the_crusader.h` `Locs[]`, `FactionChampionLoc[]`;
`instance_trial_of_the_crusader.cpp`):

| Arrival | Release point | Engage |
|---|---|---|
| Gormok | `Locs[1]` (563.9, 178.4), the gate front | forced: `AttackStart(SelectNearestTarget(200))` 10 s after his summon, 46 s from the first gossip, 11 s on a retry |
| Dreadscale (mobile), Acidmaw (stationary) | `Locs[2]` (575.7, 171.5), `Locs[3]` (549.9, 171.5) | forced, 17.5 s after Gormok dies; heroic on the 150 s timer |
| Icehowl | `Locs[1]` | forced, 15.5 s after the second worm dies |
| Jaraxxus | (563.7, 129.6), 10 yd south of `Locs[4]` | forced, 70.8 s from the gossip. A wipe respawns him on `Locs[4]`, attackable, with no forced attack and an empty `MoveInLineOfSight`: the retry is whoever attacks |
| Faction Champions | `FactionChampionLoc[10..19]`, x 520-536 for an Alliance raid, mirrored about `Locs[4]` x for Horde | proximity, from 4 s after the jump; the first engaged pulls all (`TYPE_FACTION_CHAMPIONS_START`) |
| Twins | Fjola `Locs[9]` (585.5, 170), Eydis `Locs[10]` (545.5, 170), 6 yd up | proximity, from 25.5 s after the gossip |
| Anub'arak | `Locs[17]` (785.9, 133.4) | attackable once a player is inside 80 yd, then proximity |

- **Forced** picks any attackable unit, totems and pets too. In the 2026-10-15/16 traces each boss
  turned hostile within 1-3 yd of its point, the raid waited 37-70 yd off, and Gormok, Dreadscale
  and Icehowl opened on non-tanks, Icehowl twice on a Stoneskin Totem.
- **Proximity** is `Creature::GetAggroRange`: `detection_range` 20 plus the level gap, so 23 yd from
  83 against 80, 3D centre to centre.
- **Barrett** (34816, at (559.2, 90.6)) drops `UNIT_NPC_FLAG_GOSSIP` at "We are ready!". It comes back
  at the encounter's done scene (2.5 s after the Beasts, the champions; 34.5 s after Jaraxxus) and on
  a wipe's `InstanceCleanup`, except a Jaraxxus wipe, which removes it. He is invisible from stage
  9. So a missing flag spans the encounter, walk-ins included, and the gap after a kill.
- **Floor**: players stood up to y 187.6 by the main gate (GO 195647, y 198.7) and 49-60 yd out on
  every other bearing. Anub'arak's room is navprobe-clean with `--nav 0x09` at 22 and 30 yd round
  `ANUBARAK_ROOM_CENTER` and at its tank side points.
- Players drop combat 1-3 s after a beast dies (a drink lands 1.6 s after the worms in a 2026-10-17
  25N trace), so the worms leave about 15 s to stage and Icehowl about 13.

**Window** (`Util/ToCHelpers_Staging`, read once per instance per ms), with nothing live:

- Opens at the gossip, not before, so bots stay with the player while they buff, eat or talk: Barrett
  present without his flag after it was seen at this stage. The latch keeps closed the gap after a
  kill, when the flag is still gone from the last gossip.
- Stage 3 also opens on Jaraxxus attackable and out of combat: a wipe's cleanup re-summons him with no
  gossip. Stage 9 opens with no Barrett test, who is invisible by then.
- The next beast by guid slot: Gormok while slot 4 is empty or alive, else the worms while slot 6 or 7
  is, else Icehowl. A dead beast keeps its slot until a wipe's cleanup clears it.
- Per bot: alive, out of combat, and in the arrival's room (the arena: within 80 yd 2D of
  `ARENA_CENTER`, z > 380; Anub'arak's: x > 661.6, z < 200) with its master, else the group leader,
  so a leader who walks out releases it (Naxx Thaddius). Combat ends it, so the engage releases
  everyone and a leftover snobold beats a stand.

**Stands**, dealt per instance on an arrival change and every second, so a dead or new bot settles
within one. Bots only, by guid: duty stands first, then healers (`IsHeal`), ranged (`IsRangedDps`)
and melee (the rest, off-duty tanks included), each role taking the least taken slot first in its own
order, so an overflow doubles up. Arena slots past 44 yd of the centre are dropped. A stand that moves
is floor-checked once with `ValidateFloorPoint`, kept if it lands within 3 yd, else the raw stand;
park within 1 yd, walk back past 2.5.

- **Forced arrivals**: the duty tank 3 yd from the release point, every other stand 8+, which covers
  a 1 yd park either side and a pet 3 yd off its owner (`static_assert`ed). Beasts duties come from
  the Beasts roster ([northrend-beasts.md](northrend-beasts.md#stage-model-and-tank-duties)), as
  pass 2 deals them with nothing engaged.
  - Gormok: roster[0] `tank` toward the centre, roster[1] `swap` 9 yd out; melee 9 yd to his sides;
    casters on 25/33/40 yd arcs 25-80° off the line to the centre: out of Stomp (clearance 24), off
    the drag.
  - Worms: roster[0] `mobile` 3 yd east of Dreadscale, so his Spew faces away from the raid,
    roster[1] `stationary` 3 yd west of Acidmaw; melee on Dreadscale's rear stands; casters on a
    10.5 yd lattice 19+ yd from Acidmaw (Sweep), 22.3-36 from Dreadscale and off his Spew cone, 10
    slots on 25-man, 8 on 10N; healers by the farther tank stand, ranged by Dreadscale.
  - Icehowl: roster[0] `tank` 3 yd toward the gate, so the raid stands behind him (guide: the tank's
    back to a wall); melee 11, healers 17.5, ranged 23 yd on the breath bearings, inside the spread's
    bands.
  - Jaraxxus, on his intro spot (stage 2) or on him (the stage 3 retry): the `IsMainTank` bot `tank`
    3 yd north, assist tanks 0 and 1 (`addtank0`/`addtank1`) 8 yd to his sides, melee 9 yd south,
    healers and ranged on a 3 yd lattice 20 yd north, the no-spread stack. A human main tank gets
    no stand, and no bot takes it.
- **Proximity arrivals**: nobody inside 28 yd (23 + 5) of a boss or landing slot, so the pull stays
  with the player.
  - Champions: a 10 yd lattice 28+ yd from all ten landing slots, 49 slots; healers and ranged take
    those farthest from the slots first, melee and tanks the nearest.
  - Twins: the Fjola tank 22.5 yd from Dark 1 toward her, the Eydis tank 22.5 yd from Light 1 toward
    Eydis, a lone tank (`both`) 22.5 yd from Dark 1 toward the centre; the raid on a 4 yd lattice
    16 yd from Dark 1, since it starts Dark. Every stand is 30+ yd from both twins and nearer the
    south portal of its colour. A bot due an essence walk gets no stand until it has it.
  - Anub'arak, from `GetAnubarakPrepullTanks`: `pickup` (the main tank, else side 0) on
    `GetAnubarakBossAnchor`, `side0`/`side1` on the side points; melee 8 yd west of
    `ANUBARAK_ROOM_CENTER`, casters on 22 and 30 yd rings west of it, all 40.9+ yd from him.

**Hold.** `toc staging stand` → `toc move to staging stand` (`ACTION_RAID`) walks at
`MOVEMENT_NORMAL`, latched, never attacks, and returns false once parked, so lower nodes buff and
heal. Rules (`ToCDefinition_Staging.cpp`):

- `toc staging hold` zeroes follow, formation, set behind, tank face, rear flank, move random,
  `return` and `return to stay position` (a player's `stay` too), and tank and dps assist: out of
  combat they pull on a skull, and Anub'arak turns attackable inside 80 yd.
- `toc staging rest guard` blocks food and drink while walking or on a forced arrival's duty stand
  (eating blinds 12-18 s). A parked bot elsewhere may eat and drink: a player-pulled wait has no time
  limit.
- `toc staging tank form guard` blocks `caster form` for tanks.

**Resistance.** Fire, Frost and Shadow Resistance Aura rank 5 (48947, 48945, 48943) and Aspect of the
Wild rank 4 (49071, nature) are `APPLY_AREA_AURA_RAID` + `MOD_RESISTANCE_EXCLUSIVE` 130: one holder
per school covers the raid. A spell of several schools resists by the lowest
(`Unit::GetResistance(SpellSchoolMask)`), so Fel Lightning (fire + nature) needs both. 130 against
a level 83 caster averages 20%.

| Encounter | Magic damage by school |
|---|---|
| Gormok | fire: Fire Bomb |
| Worms | Dreadscale all fire (Fire Spit, Molten Spew, Burning Bite, Spray, Bile); Acidmaw all nature |
| Icehowl | frost: Arctic Breath |
| Jaraxxus | fire: Fel Fireball, Legion Flame, Incinerate Flesh, Burning Inferno, Fel Inferno; fire + nature: Fel Lightning; shadow: Mistress' Kiss |
| Faction Champions | mostly physical; magic spread over shadow, frost, arcane, nature and fire |
| Twins | Light fire, Dark shadow |
| Anub'arak | frost: Penetrating Cold, Freezing Slash (physical + frost); nature: Leeching Swarm, scarab Acid-Drenched Mandibles; heroic shadow: Shadow Strike |

Sanctified and Swift Retribution work under any of the paladin's auras
(`spell_pal_sanctified_retribution_effect`); Improved Devotion and Improved Concentration only under
their own.

The `ResistAura` stem picks:

- **Schools**: Gormok fire; worms fire + nature; Icehowl frost; Jaraxxus fire + nature; champions
  none (mostly physical); Twins fire, then shadow (the raid starts Dark and takes Light's fire);
  Anub'arak frost + nature. The live encounter's set (Beasts by stage mask, unioned in a heroic
  overlap; an empty mask keeps the last set), else the staged arrival's, else none, so paladins go
  back to their own aura between pulls.
- **Paladins**: one bot holder per school, fire, frost, shadow in that order, re-elected every second
  and at once when the set changes. A school a human paladin holds on himself is skipped. Holders
  that still qualify are kept first; each empty school then takes the lowest of: own aura strategy
  already that school, another unelected paladin on the same aura strategy (so the raid keeps
  Devotion), cost (resistance or `bspeed` 0, `baoe` or none 1, `bcast` 2, `barmor` 3), guid. On
  Jaraxxus fire goes first to a paladin who knows Aura Mastery, and a kept fire holder without it
  gives way.
- **Hunter**: Aspect of the Wild for `GetNatureResistanceHunter` while nature is wanted; it hands the
  duty on by mana, through Viper.
- `toc resistance aura` → `toc raise resistance aura` (`ACTION_RAID + 1`, above the staging walk)
  casts through `DoSpecificAction`, outside the queue, never `ChangeStrategy`: `toc resistance aura
  hold` zeroes all eight paladin aura nodes on the holder, `toc nature aspect hold` the hunter's other
  six aspects. A paladin's own aura is read by its application
  ([pitfalls.md](../../engine/pitfalls.md#detection-that-never-fires)).

**Known gaps.**

- Totems and pets left near a release point aren't moved, and a forced attack takes them too.
- Humans hold no stand. One nearer a release point than its duty stand takes the forced attack.
- A stand is floor-checked from wherever its bot stands when dealt: one landing more than 3 yd off
  falls back to the raw stand.
- A heroic raid out of attempts keeps the window open: Barrett never shows his gossip again.
- Shaman resistance totems don't count toward a school; Aura Mastery is called only as a Jaraxxus
  cover; a lone hunter holding
  Aspect of the Wild loses Viper (`GetNatureResistanceHunter`'s last fallback).
- A holder already covered by another paladin's aura of its school fires its trigger every tick
  without casting (the class action reads that aura by name) and keeps its own aura meanwhile.

## What a trace answers

Boss notes are in each boss doc. Raid-wide, under `postmortem.py <file> --notes toc.`:

| Key | Shape | Says |
|---|---|---|
| `progress` | latch | The stage per the [instance script](#instance-script) table, per instance, restated at each trace's start and then noted on change, attributed to the bot whose tick read it |
| `stage` | per bot | Its stand, `<arrival>.<role> x,y,z` rounded to the yard, ` human` on a duty stand a human stood nearer the release than; `none` in a window with no stand. Change-only, and a trace opens at the engage, so for 2 s after an arrival closes any bot's tick notes every bot's last label again |
| `aura` | per paladin and hunter bot | Its resistance duty: `fire`, `frost`, `shadow`, `nature`, `none`. No reader section |

`tools/botobs/bosses/trial_of_the_crusader.py <file> --staging`, over a trace of any ToC encounter:
per release (a unit's first snapshot with a target; of the champions only the first), the victim,
the duty holder, ms until it targets a tank, its hits on non-tanks in the first 5 s, and each bot's
stand from its latest `stage` up to 2 s past the release, with its 2D distance from it at the
release, flagged past 3 yd. The banner says when the trace never wrote `stage`.

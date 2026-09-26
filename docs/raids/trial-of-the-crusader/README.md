# Trial of the Crusader (map 649)

Strategy key **`trialofthecrusader`**, one strategy for all five encounters. Cross-raid conventions
are in [../README.md](../README.md). Mechanics source:
`src/server/scripts/Northrend/CrusadersColiseum/TrialOfTheCrusader/`.

**`wotlk-toc` is Trial of the *Champion*** (the 5-man dungeon, map 650), **not the raid.**

| Boss | Doc | Code stems | Trace slug | Note-key prefix |
|---|---|---|---|---|
| Northrend Beasts | [northrend-beasts.md](northrend-beasts.md) | `Gormok`, `Jormungars`, `Icehowl`, `NorthrendBeasts` | `northrend-beasts` | `nb.` |
| Lord Jaraxxus | [lord-jaraxxus.md](lord-jaraxxus.md) | `Jaraxxus` | `lord-jaraxxus` | `jaraxxus.` |
| Faction Champions | [faction-champions.md](faction-champions.md) | `FactionChampions` | `faction-champions` | `fc.` |
| Twin Val'kyr | [twin-valkyr.md](twin-valkyr.md) | `TwinValkyr` | `val-kyr-twins` | `tv.` |
| Anub'arak | [anubarak.md](anubarak.md) | `Anubarak` | `anub-arak` | `anub.` |

Raid-wide notes use `toc.`. Bot-tractable (avoidance, positioning, kill order, interrupts): Beasts,
Jaraxxus, Faction Champions, Anub'arak P1/P3. Harder, needing new state tracking: the Val'kyr
essence system and Anub'arak spike-kiting.

## Instance script

`instance_trial_of_the_crusader.cpp` registers no bosses (`GetEncounterCount()` 0, no
`SetBossState`) and no object data. It exposes three things:

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

- **`IsEncounterInProgress()`**, a private `EncounterStatus`, true only while an alive non-GM player
  is in the map. It goes true before most pulls: from the end of the Beasts intro (stage 1) through
  every walk-in until the last beast dies, at the champions' release, 3.25 s before the twins turn
  aggressive, and from the Lich King scene until Anub'arak engages and then dies or evades. Only
  Jaraxxus sets it on engage. An evade (`TYPE_FAILED`) or an all-dead cleanup clears it.
- **`GetGuidData`**: Gormok (4), Dreadscale (6), Acidmaw (7), the twins by NPC entry (34497 Fjola,
  34496 Eydis), Anub'arak (13, also while submerged). Icehowl, Jaraxxus and the champions only by
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
| `northrend worms sweep frontal` | Sweep cast |
| `northrend worms afflicted by burning` | Burning Bite/Spray auras, which never exist ([northrend-beasts.md](northrend-beasts.md)) |
| `icehowl frothing rage` | Frothing Rage |
| `jaraxxus incinerate flesh on raid` | Incinerate Flesh |
| `jaraxxus fel fireball interruptible` | Fel Fireball cast |
| `jaraxxus nether power active` | Nether Power |
| `jaraxxus legion flame nearby` | Legion Flame |
| `twin valkyr needs base essence`, `… vortex requires essence`, `… touched requires essence`, `… shield requires essence` | essences, Vortex casts, Touch, shields |
| `twin valkyr pact interrupt duty` | Twin's Pact cast |
| `anubarak burrower should be focused` | Permafrost on the burrower |
| `anubarak penetrating cold on raid` | Penetrating Cold |
| Anub'arak burst row | Leeching Swarm, lust only in phase 3, latched per instance |

## Code layout

`src/Ai/Raid/ToC/` splits per stem: `Action/ToCActions_<Stem>`, `Trigger/ToCTriggers_<Stem>`,
`Multiplier/ToCMultipliers_<Stem>`, `Util/ToCHelpers_<Stem>`. `NorthrendBeasts` holds what the three
beasts share, `Shared` what several encounters use.

**Seams.** A boss gains a trigger, action or multiplier by editing only its own stem:

- trigger: class plus `creators[...]` entry in `ToC<Stem>TriggerContext`, node in
  `AddToC<Stem>TriggerNodes` (both `Trigger/ToCTriggers_<Stem>.h`);
- action: class plus entry in `ToC<Stem>ActionContext` (`Action/ToCActions_<Stem>.h`);
- multiplier: `AddToC<Stem>Multipliers` (`Multiplier/ToCMultipliers_<Stem>.h`);
- burst row: each encounter stem (`NorthrendBeasts`, `Jaraxxus`, `FactionChampions`, `TwinValkyr`,
  `Anubarak`) answers `ToC<Stem>BurstWindow` with a `ToCBurstWindow`. `ToCBurstWindowMultiplier`
  (`Multiplier/ToCMultipliers_Shared.{h,cpp}`) applies the live encounter's row to in-combat burst
  cooldowns, mana-return ones exempt: `allowLust` to Bloodlust/Heroism, `allowAll` to the rest.
  Every row ANDs with the shared tank-held-boss gate.

The root only dispatches: `ToCStrategy.cpp` calls every stem's node and multiplier list, and
`ToCActionContext.h`/`ToCTriggerContext.h` merge the stem contexts. The four registration sites name
only these root classes. **Dispatch order is behaviour**: equal relevances pop in insertion order.

**Encounter gate** (`Util/ToCEncounterGate.{h,cpp}`), read from the instance script once per
instance per ms. Off map 649 the gate is open and nothing is live.

- **Open** (`ToCEncounterGateOpen`, gates triggers): the stage's encounter is this one, pulled or
  not, so pre-pull prep (an essence before the twins) runs. Beasts at stage 2 and Jaraxxus at stage
  4 also stay open until the next encounter is live: the kill moves the stage while adds live on
  (snobolds riding players when Gormok dies last, the Mistress and Infernals Jaraxxus's portals and
  volcanoes summoned), and each trigger tests its own units.
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
  matters, measure arrival on the boss (Anub'arak's pre-submerge drag).
- `AvoidCreatureClusterAction` is the legacy `FleePosition` dodge, capped at
  `AiPlayerbot.FleeDistance` (5 yd), kept only for its one caller (slime pools). A
  new hazard uses `FindNearestPositionClearOfHazards` with a clearance past the trigger radius, per
  [pitfalls.md](../../engine/pitfalls.md).
- `IsBotInFrontalCone` and `CastClassTaunt` live in `src/Util/EncounterHelpers.{h,cpp}`; ToC keeps no
  copies.

## What a trace answers

Boss notes are in each boss doc. Raid-wide: `postmortem.py <file> --notes toc.`. `progress` is the
stage per the [instance script](#instance-script) table, per instance, restated at each trace's
start and then noted on change, attributed to the bot whose tick read it.

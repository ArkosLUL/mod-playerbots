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
  `Anubarak`) answers `ToC<Stem>BurstWindow` with a `ToCBurstWindow`
  (`Multiplier/ToCMultipliers_Shared.h`).

The root only dispatches: `ToCStrategy.cpp` calls every stem's node and multiplier list, and
`ToCActionContext.h`/`ToCTriggerContext.h` merge the stem contexts. The four registration sites name
only these root classes. **Dispatch order is behaviour**: equal relevances pop in insertion order.

**Constraints.**

- pblint reads a `creators["..."]` key only after a one-line `class X : public NamedObjectContext<…>`
  in the same file, so a stem context keeps its constructor inline in its header. A trigger's
  `: Trigger(botAI, "name")` stays on one line for the same reason.
- `src/Ai/Dungeon/TOC/TOC*.h` (Trial of the Champion) collide case-insensitively with raid `ToC*.h`.
  Hence the raid-unique umbrella headers `ToCRaidActions.h`, `ToCRaidTriggers.h`,
  `ToCRaidMultipliers.h`, and `BuildShared{Action,Trigger}Contexts.cpp` including the root contexts
  by path. A new header needs a name no dungeon header has, ignoring case.
- `Util/ToCData.h` holds only raid-wide values: map id, `ARENA_CENTER`, `ANUBARAK_PIT_CENTER`, and
  the `ToCNpcs`/`ToCSpells` mirrors. A new boss-specific id goes in the stem's
  `Util/ToCHelpers_<Stem>.h`.
- Shared bases (`Action/ToCActions_Shared.h`): `ToCMainTankHoldAction::DragBossToAnchor` backs the
  tank 5 yd at a time toward an anchor while it tanks the boss and stands more than 12 yd from it;
  the boss follows, so it settles up to melee reach past that.
- `AvoidCreatureClusterAction` is the legacy `FleePosition` dodge, capped at
  `AiPlayerbot.FleeDistance` (5 yd), kept only for its two callers (slime pools, Legion Flame). A
  new hazard uses `FindNearestPositionClearOfHazards` with a clearance past the trigger radius, per
  [pitfalls.md](../../engine/pitfalls.md).
- `IsBotInFrontalCone` and `CastClassTaunt` live in `src/Util/EncounterHelpers.{h,cpp}`; ToC keeps no
  copies.

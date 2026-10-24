# Sunwell Plateau (map 580)

Strategy key `sunwell`, files in `src/Ai/Raid/SWP/`, from the third-party fork
`brighton-chi/mod-playerbots@sunwell-strategies`. Cross-raid conventions are in
[README.md](README.md).

## The Eredar Twins tank gate has never worked

`SunwellPlateauResetEncounterStatesAction::Execute` gates the first assist tank's Alythess-step reset
on

```cpp
if (PlayerbotAI::IsTank && !AI_VALUE2(Unit*, "find target", "grand warlock alythess"))
```

`PlayerbotAI::IsTank` with no call parentheses takes the **function's address**, which is never null,
so the tank half of the condition is always true and every bot passes it. Nothing warns and nothing
fails to compile. The gate has been inert since the strategy landed, so any judgement of how the
Twins play is a judgement of the ungated behaviour.

## Target exclusions

`RaidSunwellStrategy` overrides `HasTargetExclusions()` / `AppendTargetExclusions(GuidSet&,
TargetValueExclusionType)`, the `Strategy` API for keeping units out of target values; BWL, Kara, MC,
SSC, Ulduar and the Magisters' Terrace and Underbog dungeon strategies use it too. What it costs per raid is in
[../engine/raid-mechanics-lessons.md](../engine/raid-mechanics-lessons.md#what-a-strategy-costs-per-raid).

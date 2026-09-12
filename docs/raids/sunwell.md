# Sunwell Plateau (map 580)

Strategy key `sunwell`. Cross-raid conventions are in [README.md](README.md).

The strategies came in from the third-party fork `brighton-chi/mod-playerbots@sunwell-strategies` —
31 new files under `src/Ai/Raid/SWP/**` plus one SQL update. Nothing about the merge itself is worth
keeping; two things from it are.

## The `"rs"` bug found during the merge

`GetInstanceStrategies()` was missing `"rs"` while `PlayerbotAI.cpp` still mapped map 724 to it — so
the Ruby Sanctum strategy was applied but **never removed on a zone change**. Fixed in the same edit
that added `"sunwell"`.

## The Eredar Twins tank gate has never worked

`src/Ai/Raid/SWP/Action/SWPActions.cpp:99` reads

```cpp
if (PlayerbotAI::IsTank && !AI_VALUE2(Unit*, "find target", "grand warlock alythess"))
```

`PlayerbotAI::IsTank` with no call parentheses takes the **function's address**, which is never null,
so the tank half of the condition is always true and every bot passes it. Nothing warns and nothing
fails to compile. The gate has been inert since the strategy landed, so any judgement of how the
Twins play is a judgement of the ungated behaviour.

## The instance-strategy key list

`GetInstanceStrategies()` is the authoritative list; any key missing from it can be applied but never
cleaned up. Current contents:

```
aq20, aq40, blacktemple, bwl, gruulslair, hyjal, icc, karazhan, magtheridon, moltencore, naxx,
onyxia, rs, ssc, sunwell, tbc-ac, tempestkeep, trialofthecrusader, ulduar, voa, wotlk-an, wotlk-cos,
wotlk-dtk, wotlk-eoe, wotlk-fos, wotlk-gd, wotlk-hol, wotlk-hor, wotlk-hos, wotlk-nex, wotlk-occ,
wotlk-ok, wotlk-os, wotlk-pos, wotlk-toc, wotlk-uk, wotlk-up, wotlk-vh, zulaman
```

`RaidSunwellStrategy` overrides `HasTargetExclusions()` / `AppendTargetExclusions(GuidSet&,
TargetValueExclusionType)` — a `Strategy` API worth knowing about, since nothing else in the tree
uses it.

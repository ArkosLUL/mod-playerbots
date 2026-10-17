# w8-prepull-staging — ToC: opening spots before each boss engages, and resistance auras

Wave 4e. It runs after wave 4d (`w5-anubarak-rest`, `w3b-fc-defence-rest`, `w2-jaraxxus-rest`,
`refine-beasts-2`) merges and branches from that merge: those lanes change the spots this one reads
(worm spread, breath bearings, the Anub'arak anchor, the champions' opening spread), so read their
merged code, not the line numbers here. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Boss docs: `docs/raids/trial-of-the-crusader/`. Line
numbers are as of `1846fc944`; core script paths are under
`src/server/scripts/Northrend/CrusadersColiseum/TrialOfTheCrusader/` at the server root.

Goal: each boss arrives at the same coordinates every pull, so bots wait on their spots before he
engages, and the tanks catch him at once. Paladins also raise the resistance aura that fits each
encounter.

## Facts

**Where each boss arrives and how it engages** (`trial_of_the_crusader.h:274-320` `Locs[]`,
`instance_trial_of_the_crusader.cpp`):

| Boss | Arrives at | Engages | Window |
|---|---|---|---|
| Gormok | gate front (563.9, 178.4) | forced attack on the nearest player within 200 yd | 11 s after `SCENE_004`; 46 s from the gossip on a first start, 11 s on a retry |
| Acidmaw (stationary) / Dreadscale (mobile) | (549.9, 171.5) / (575.7, 171.5) | forced, nearest | 17.5 s after Gormok's death; heroic on its timer |
| Icehowl | gate front | forced, nearest | 15.5 s after the worms die |
| Jaraxxus | (563.7, 129.6) | forced, nearest | 70.8 s from the gossip; after a wipe at the centre, attack only |
| Faction Champions | ten landing slots, west for an Alliance raid (x 520-537, y 111-137), mirrored for Horde | no forced attack: 23 yd proximity, and one engaged pulls all | released 4 s after the jump |
| Twins | Fjola (585.5, 170), Eydis (545.5, 170) | 23 yd proximity | attackable 25.5 s from the gossip |
| Anub'arak | (785.9, 133.4) | attackable once a player is inside 80 yd, then 23 yd proximity | no gossip, stage 9 |

- Barrett Ramsey (`NPC_BARRENT` 34816) loses his gossip flag when "We are ready!" is chosen and gets
  it back only after the encounter, the Beasts' walk-ins and Jaraxxus' retry (Barrett re-summons
  him) included.
- In the 2026-10-16 traces (`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/649_*`
  from `1792176436` on) each boss turns hostile within 1-3 yd of the same spot every pull; the raid
  waits 37-70 yd away; Gormok runs 32-36 yd before a tank reaches him (in 25N his first victim was a
  ranged bot); Dreadscale hit a melee DPS before any tank in both Beasts pulls. Jaraxxus, whose main
  tank already walks to him in the intro, had zero tank movement.
- The ToC strategy runs on both engines and its triggers are gated by stage only, so they fire out
  of combat. No helper says a scene is running, and stage 8 closes every ToC gate.
- Precedent, Naxx Thaddius staging: node `NaxxDefinition_Thaddius.cpp:20-33`; spot and safety
  `NaxxBossHelper.h:2491-2532`; `follow` zeroed by `ThaddiusPrepullMultiplier`
  (`NaxxMultipliers.cpp:124`); doc `docs/raids/naxxramas.md`, "Pre-pull staging". ToC's own:
  `jaraxxus intro main tank` (`ToCTriggers_Jaraxxus.cpp:28-35`, `ToCActions_Jaraxxus.cpp:158-169`,
  `ToCMultipliers_Jaraxxus.cpp:70-80`).
- What works against it: `follow` (a default action at 1.0) walks a parked bot back to its master;
  `FindNearestPositionClearOfHazards` searches only around the bot. Three ways to start a pull early
  (`docs/engine/pitfalls.md`, "Before the pull, and out of combat" and "A targeting mark never
  clears inside a pull"): a direct `Attack()` reaches 100 yd, `tank assist` runs out of combat, and
  Anub'arak is attackable inside 80 yd.

**Resistance auras.**

- Fire, Frost and Shadow Resistance Aura (rank 5: 48947, 48945, 48943) are `APPLY_AREA_AURA_RAID` +
  `MOD_RESISTANCE_EXCLUSIVE`: one holder per school covers the raid, a second adds nothing, auras of
  different schools from different paladins both apply. Shaman resistance totems and Aspect of the
  Wild share the exclusive type for their school.
- Schools (Spell.dbc `SchoolMask`, `modules/mod-spell-tweaks/data/dbc-reference/spell.reference.csv`
  at the server root):
  - fire: Gormok's Fire Bomb, Dreadscale's Burning Bile and Burning Spray, Jaraxxus' Fel Fireball,
    Legion Flame, Incinerate Flesh and Infernal Eruption (Fel Lightning is fire + nature), the
    Twins' Unleashed Light, Light Vortex and Touch of Light;
  - frost: Arctic Breath, Penetrating Cold, Freezing Slash (physical + frost);
  - shadow: the Twins' Dark counterparts, the burrowers' Shadow Strike;
  - nature, which no paladin aura covers: Acidic Spew, Paralytic Spray, Leeching Swarm;
  - physical: Massive Crash, Impale.
  - Corrected by the investigation: Molten Spew is fire, so Dreadscale is all fire and Acidmaw all
    nature. The full table and the multi-school rule are in the README's "Pre-pull staging".
- The generic path (MC, BWL, Ulduar, e.g. `UldDefinition_Ignis.cpp:49-54`): `BossFireResistanceTrigger`
  and its siblings (`src/Ai/Base/Trigger/BossAuraTriggers.cpp`) key on a boss name and pick the first
  alive paladin in group order, whatever its spec. `BossFireResistanceAction`
  (`src/Ai/Base/Actions/BossAuraActions.cpp`) adds `+rfire` to the combat engine and never removes
  it; `rfire`/`rfrost`/`rshadow` are siblings of `barmor`/`baoe`/`bcast`/`bspeed`
  (`PaladinAiObjectContext.cpp:46-57`), so that evicts the paladin's normal aura strategy for good.
  The hunter path avoids it: no strategy change, `BossNatureAspectHoldMultiplier`
  (`src/Ai/Base/Strategy/BossResistanceMultipliers.h`) holds the aura and reverts when the boss
  dies (rationale at `BossAuraActions.cpp:50-57`).

## Scope

1. **Window.** `ToCStagingEncounter(bot)` names the encounter about to start; it is non-None while
   none of its units is engaged, the bot is not in combat, and either Barrett is present without his
   gossip flag, or the stage is 9 and the bot is in Anub'arak's room (past the Web Door, x > 661.6).
   The next beast is the first one not yet dead. Decision (log it): staging starts at the gossip,
   not before, so bots stay with the player while they buff, eat or talk. Tightest case, a Beasts
   retry: 11 s against a walk of about 60 yd from Barrett, so about 9 s.
2. **Stands**, per encounter and role, built from the script's own points. The arena floor is a GO,
   so per the module `CLAUDE.md` they anchor on those points, are validated live with
   `ValidateFloorPoint`, and are checked against the traces' positions.
   - *Forced-attack bosses* (Gormok, the worms, Icehowl, Jaraxxus): each boss's duty tank stands
     strictly nearest its release point, on the spot the encounter holds it; every other bot a
     margin further out. So he walks straight onto his tank: Gormok's tank, the two worm holders,
     Icehowl's tank, Jaraxxus' main tank and add tanks. This takes over `jaraxxus intro main tank`.
   - *Proximity bosses* (Faction Champions, Twins, Anub'arak): nobody inside 23 yd plus a margin of a
     boss or a landing slot; the pull stays with the player, as today. Stands are the opening
     layout: champions, the crowd-AoE spread; Twins, the existing essence step, then each tank on
     its twin's side; Anub'arak, the main tank on the anchor, side tanks on their side points, the
     raid west of the centre, so once a player pulls he runs onto the anchor and there is no drag.
   - *Non-tanks* take each encounter's opening layout: Gormok, the ranged band out of Stomp; worms,
     the caster spread slots and melee rear spots on Dreadscale's arrival; Icehowl, breath bearings
     around his arrival point.
   - *Humans* can't be moved. A human nearer the release point than the bot tank shows in the probe
     and is not fought.
3. **Hold.** Park within about 1 yd; the action returns true while walking (latched), false once
   parked. A Block rule in `ToCDefinition_Staging.cpp` zeroes `follow`, the `stay` return and
   formation moves for a bot holding a stand; its predicate is the window plus a latched stand.
   Never `Attack()` or `tank assist` from a stand. The hold ends when the encounter goes live or the
   bot enters combat, so leftover snobolds win.
4. **Observability.** A `toc.stage` probe gives each bot's stand label, or `none`. A `--staging`
   view, in `postmortem.py` or per boss reader as the investigator decides: each bot's distance to
   its stand at the release, the boss's first victim, time to a tank, any non-tank hit in the first
   5 s.
5. **Resistance auras.** Paladins raise each encounter's aura from the staging window through the
   fight (`ToCEncounterGateOpen`), then return to their normal aura.
   - Schools per encounter from the facts above, the core script and the guide. Suggested: fire on
     Gormok, the worms and Jaraxxus; frost on Icehowl and Anub'arak; fire and shadow on the Twins
     when two paladins are free. Faction Champions from the 10/25 roster's damage mix. Log each
     choice.
   - One holder per school, re-picked on death; pick holders so the raid keeps its other auras where
     it can (Devotion from another paladin), and log the rule.
   - Cast directly and hold with a multiplier vetoing the holder's other aura nodes while the gate
     holds, as the hunter path does. Never `ChangeStrategy`, which outlives the encounter. Leave the
     generic `Base` triggers alone: other raids use them.
   - Optional, investigator decides: a hunter on Aspect of the Wild for the worms and Anub'arak, via
     `GetNatureResistanceHunter` (`src/Ai/Base/Util/GenericBuffUtils.h`).
   - A `toc.aura` probe per paladin: the aura held, or `none`.

## Owns

- a new `Staging` stem: `ToC{Actions,Triggers,Helpers}_Staging.*` and `ToCDefinition_Staging.cpp`;
- a new `ResistAura` stem in the same layout, or the auras folded into `Staging` if the investigator
  prefers;
- their registration in `ToCStrategy.cpp`;
- the `jaraxxus intro main tank` node and its multiplier, to retire (`Jaraxxus` stem, those lines
  only);
- a "Pre-pull staging" section in `docs/raids/trial-of-the-crusader/README.md`;
- the `--staging` reader section and its test.

Encounter spots come from each encounter's helpers, read-only: where a spot is not reachable from
outside its stem, add an accessor there and list it in the report. New `.cpp` files need a CMake
re-run: say so in the report.

## Task list

Status: done. Groups A-C implemented, the finisher integrated, checked, reviewed and committed. Paths are under
`src/Ai/Raid/ToC/` unless they start with `docs/` or `tools/`. Read first: the README's "Pre-pull
staging" (arrival points, engage kinds, Barrett, schools), "Code layout" (stems, seams, gate) and
`northrend-beasts.md` "Stage model and tank duties". Every group codes against the contracts
below; change one only through the finisher. No group touches a file another group owns.

### Contracts

Names (pblint resolves them; none leads with an encounter prefix, so `ToCGatedTrigger` never wraps
them):

| Kind | Name | Owner |
|---|---|---|
| trigger → action, `ACTION_RAID` | `toc staging stand` → `toc move to staging stand` | A |
| rules | `toc staging hold`, `toc staging rest guard`, `toc staging tank form guard` | A |
| trigger → action, `ACTION_RAID + 1` | `toc resistance aura` → `toc raise resistance aura` | B |
| rules | `toc resistance aura hold`, `toc nature aspect hold` | B |
| probe, per bot, on change | `toc.stage` | A |
| probe, per bot, on change | `toc.aura`: `fire`, `frost`, `shadow`, `nature` (hunter), `none`; paladin and hunter bots only | B |

`toc.stage` text: `<arrival>.<role> <x>,<y>,<z>` (the stand through `RaidObs::DescribeDerived`), a
duty tank's suffixed ` human` while a living human group member stands nearer that stand's release
point than the stand does (2D); `none` for a bot in a window with no stand. Arrivals and roles:
`gormok` tank, swap; `worms` mobile, stationary; `icehowl` tank; `jaraxxus` tank, addtank0, addtank1;
`champions`; `twins` fjola, eydis, both; `anubarak` pickup, side0, side1; every arrival also melee,
ranged, healer. Re-noted every read, and the last value stays once the window closes: a ToC session
opens at the engage, so a label cleared at the close would never reach the trace.

C++ (namespace `TrialOfTheCrusaderHelpers`, except `ToCEncounterGate.h`'s global names):

```cpp
// Util/ToCEncounterGate.h (A)
ToCEncounter ToCEncounterOfStage(uint32 stage);  // the existing EncounterOfStage
bool ToCRaidWideGateOpen(PlayerbotAI* botAI, uint32 encounterId);  // true
bool ToCRaidWideGateLive(PlayerbotAI* botAI, uint32 encounterId);  // false
inline constexpr EncounterGate ToCRaidWideGate{&ToCRaidWideGateOpen, &ToCRaidWideGateLive};

// Util/ToCHelpers_Staging.h (A)
enum class ToCStagingBoss : uint8 { None, Gormok, Worms, Icehowl, Jaraxxus, Champions, Twins, Anubarak };
ToCStagingBoss GetToCStagedArrival(PlayerbotAI* botAI);  // raid-wide half of the window, per instance per ms
ToCStagingBoss GetToCStagingBoss(PlayerbotAI* botAI);    // that, plus the bot's own half
ToCEncounter ToCStagingEncounter(PlayerbotAI* botAI);    // the encounter of GetToCStagingBoss
```

Each stem follows the README's seam shape: classes with a one-line `: Trigger(botAI, "name")`, a
context `ToC<Stem>TriggerContext`/`ToC<Stem>ActionContext` with inline creators in its header,
`AddToC<Stem>TriggerNodes(std::vector<TriggerNode*>&)`, and a Definition accessor
`EncounterDefinition const& ToC<Stem>Definition()` (gate `ToCRaidWideGate`, encounter id
`static_cast<uint32>(ToCEncounter::None)`, trace name `nullptr`, rules only). Triggers run every tick.
Per-instance state is a `static RaidInstanceState<State>` in the stem's helper `.cpp`.

### Group A: Staging stem

Files: new `Util/ToCHelpers_Staging.{h,cpp}`, `Trigger/ToCTriggers_Staging.{h,cpp}`,
`Action/ToCActions_Staging.{h,cpp}`, `Definition/ToCDefinition_Staging.cpp`; accessor additions only
in `Util/ToCEncounterGate.{h,cpp}`, `Util/ToCHelpers_NorthrendBeasts.{h,cpp}`,
`Util/ToCHelpers_Icehowl.{h,cpp}`, `Util/ToCHelpers_TwinValkyr.{h,cpp}`,
`Util/ToCHelpers_Anubarak.{h,cpp}`; the retirement in `Trigger/ToCTriggers_Jaraxxus.{h,cpp}`,
`Action/ToCActions_Jaraxxus.{h,cpp}`, `Multiplier/ToCMultipliers_Jaraxxus.{h,cpp}`,
`Util/ToCHelpers_Jaraxxus.{h,cpp}`.

1. **Accessors**, no behaviour change: `ToCEncounterOfStage` and `ToCRaidWideGate` (contract);
   `std::vector<Player*> GetBeastsTankRoster(Player* bot)` (the existing `GatherRoster`);
   `float IcehowlBreathStepDeg(Unit* anyUnitOnMap)` (the existing `BreathStepDeg`); move
   `TwinTankRole` to the Twins header and add `TwinTankRole GetTwinTankRole(Player* bot)`
   (`TankRoleOf` without its note); `struct AnubarakPrepullTanks { ObjectGuid pickup; ObjectGuid
   side[2]; }` and `AnubarakPrepullTanks GetAnubarakPrepullTanks(Player* bot)`, computed fresh with no
   latch from `StableMainTankGuid` and the side ranking `RefreshSideTanks` uses (factor that ranking
   out so both share it); pickup is the main tank while alive, else side 0.
2. **Window, raid-wide half** (`GetToCStagedArrival`), per instance per ms, on map 649 only:
   - Barrett (34816): guid cached per instance; while it doesn't resolve, a grid search for his
     entry within 250 yd of the reading bot at most every 2 s. Present: alive and `IsVisible()`.
   - Whenever he is present with `UNIT_NPC_FLAG_GOSSIP`, latch `ToCEncounterOfStage(stage)` as the
     encounter his gossip was seen for.
   - Stage 9: Anubarak while `ToCLiveEncounter` is None.
   - Else, with the stage's encounter E not None and `ToCLiveEncounter` None: open when Barrett is
     present without the flag and either the latch equals E, or the stage is 3 and `GetJaraxxus` is
     out of combat (a wipe's cleanup re-summons him attackable with no gossip). The latch keeps
     closed the 2.5-34.5 s after a kill while the flag is still gone from the last gossip.
   - Beasts arrival: Gormok while guid slot 4 is empty or resolves alive; else Worms while either of
     slots 6 and 7 is empty or resolves alive; else Icehowl. A dead or despawned beast's slot keeps
     its guid until a wipe's cleanup clears it.
3. **Window, the bot's half** (`GetToCStagingBoss`): the bot alive, in world, not in combat; its
   master, else the group leader, on the same instance and, like the bot, inside the arrival's room:
   the arena (within 80 yd 2D of `ARENA_CENTER`, z > 380) or, for Anub'arak, past the Web Door
   (x > 661.6, z < 200). A leader who leaves releases the hold (Naxx Thaddius precedent).
4. **Deal**, per instance, rebuilt every 1000 ms while an arrival is staged and at once when it
   changes, so a dead or new bot settles within a second. Bots only (humans can't be moved), alive,
   on the instance. Duty tanks first (layouts below), then healers (`IsHeal`), ranged (`IsRangedDps`),
   melee (everyone else, non-duty tanks included), each by guid ascending, taking its role's slots in
   list order; past the end the list repeats, two to a slot.
5. **Layouts.** Script points are mirrored as constants named for their `Locs[]` or
   `FactionChampionLoc[]` index: the script header isn't on the include path. `û(P→Q)` is the unit
   vector, `rot(v, θ)` turns v by θ degrees counter-clockwise. Z is the release point's: 394.5 on the
   arena, 142.2 in Anub'arak's room. Arena slots past 44 yd of `ARENA_CENTER`
   (`WORM_SPOT_FLOOR_RADIUS`) are dropped. On a forced arrival every non-duty stand is at least 8 yd
   from each of its release points (duty 3 plus `STAGING_RELEASE_MARGIN` 5, covering a 1 yd park
   either side and a pet's 3 yd); every layout below meets it, `static_assert` it.
   - **Gormok**, R = `Locs[1]` (563.93, 178.37), û = û(R→`ARENA_CENTER`): tank = Beasts roster[0] at
     R + 3û; swap = roster[1] at R + 9û; melee R + 9·rot(û, ±90°), alternating; healers then ranged at
     R + r·rot(û, ±θ) for r 25 with θ 44, 62, 80, r 33 with θ 35, 49, 63, r 40 with θ 25, 37, + before
     −: 16 slots at least 25 yd from him (Stomp clearance 24), 7.8 apart (Fire Bomb 8), off the drag
     line to the centre.
   - **Worms**, D = `Locs[2]` (575.74, 171.5), A = `Locs[3]` (549.93, 171.5), ŵ = û(A→D): mobile =
     roster[0] at D + 3ŵ; stationary = roster[1] at A − 3ŵ; melee D + 8.3·rot(−ŵ, ±54°)
     (`WORM_MELEE_STAND`, `WORM_MELEE_GROUP_DEG`), alternating; healers and ranged on a 10.5 yd hex
     lattice through (566.27, 157.5), kept at most 44 from `ARENA_CENTER`, at least 19 from A (Sweep
     clearance 18 + 1), 22.3-36 from D (past the caster spread's melee circle, inside the generic
     `reach spell` trigger) and more than `WormSpewHalfArc(botAI)` + `WORM_SPEW_CLEAR_PAD` off ŵ seen
     from D (his Spew faces his tank). Healers take slots by the larger distance to the two tank
     stands ascending, ranged by distance to D ascending. 10 slots on 25-man, 8 on 10N.
   - **Icehowl**, R = `Locs[1]`, n̂ = û(`ARENA_CENTER`→R), b̂ = −n̂, step s = `IcehowlBreathStepDeg`
     (18°, 36° on 10N), bearings θ = k·s for k = 0, 1, −1, 2, −2, …: tank = roster[0] at R + 3n̂, his
     back to the open floor (guide: the tank's back to a wall); melee R + 11·rot(b̂, θ) for |θ| ≤ 57°;
     healers R + 17.5·rot(b̂, θ) for |θ| ≤ 90°, nearest 60° first; ranged R + 23·rot(b̂, θ) for
     |θ| ≤ 72°. The radii sit in the breath spread's bands (`ICEHOWL_SPREAD_*`); each role deals
     round-robin over its bearings.
   - **Jaraxxus**, anchor P = (563.672974, 129.571) at stage 2, at stage 3 his live position (else
     `Locs[4]`), f̂ = (0, 1): tank = the bot `PlayerbotAI::IsMainTank` names, at P + 3f̂;
     addtank0/1 = `IsAssistTankOfIndex(member, 0/1, true)` at P + 8·rot(f̂, ±90°); melee
     P + 9·rot(−f̂, θ) for θ 0, 25, −25, 50, −50; healers then ranged on a 3 yd hex lattice within 6 yd
     of P + 20f̂, innermost first: the no-spread stack, in front of him.
   - **Champions**, slots S = `FactionChampionLoc[10..19]`, x mirrored to 2·`Locs[4]`.x − x unless
     `GetTeamIdInInstance()` is Alliance: a 10 yd hex lattice through `ARENA_CENTER`, kept at least
     28 yd (23 aggro + 5) from every S; healers, then ranged, take slots farthest from S's centroid
     first, melee and tanks nearest first. 49 slots either side.
   - **Twins**, F = `Locs[9]`, E = `Locs[10]`, Dark 1 = `Locs[13]`, Light 1 = `Locs[15]`, by
     `GetTwinTankRole`: fjola at Dark 1 + 22.5·û(Dark 1→F), eydis at Light 1 + 22.5·û(Light 1→E), both
     at Dark 1 + 22.5·û(Dark 1→`ARENA_CENTER`); everyone else on a 4 yd hex lattice within 8 yd of
     Dark 1 + 16·û(Dark 1→`ARENA_CENTER`), melee nearest `ARENA_CENTER` first. Every stand is 30+ yd
     from both twins and nearer the south portal of its wanted colour than the north one, which
     stands 8-9 yd from a twin. A bot with `TwinValkyrMustSwapEssence(botAI, false)` while
     `GetEssencePortal` finds its wanted colour has no stand that tick: the essence walk
     (`ACTION_RAID + 4`) goes first.
   - **Anub'arak**, O = `ANUBARAK_ROOM_CENTER`, roles from `GetAnubarakPrepullTanks`: pickup at
     `GetAnubarakBossAnchor` (O until a patch lands), side0/side1 at `GetAnubarakTankSidePoint`;
     melee O + 8·rot((−1, 0), θ) for θ 0, 40, −40; healers then ranged at O + 22·(cos φ, sin φ) for φ
     180, 160, 200, 140, 220, 120, 240, then O + 30·(cos φ, sin φ) for φ 170, 190, 150, 210, 130,
     230. All navprobe-clean with `--nav 0x09`, 40+ yd from `Locs[17]`.
6. **Per-bot read**, `ToCStagingRead GetToCStagingRead(PlayerbotAI*)` returning `{held, walk, duty,
   spot}`: inside the bot's window with a dealt stand. When the dealt stand moves past 0.5 yd, the
   spot is validated once with `ValidateFloorPoint` (kept where it lands within 3 yd, else the raw
   stand) and the park latch drops; then parked = parked ? dist ≤ 2.5 : dist ≤ 1.0 (2D), walk = not
   parked. The latch is split by edge and written where the trigger declines, so trigger and action
   agree within a tick. Notes `toc.stage`: the label and the stand, never the bot's distance. Pure
   reads for the rules: `IsToCStagingHeld` (window and a stand) and `IsToCStagingRestHeld` (held, and
   walking or a duty tank).
7. **Trigger and action.** `toc staging stand` fires on `walk`. `toc move to staging stand` derives
   from `NorthrendBeastsWalkAction` (`Action/ToCActions_NorthrendBeasts.h`): false once parked, so
   lower nodes buff, eat and drink; true without a new `MoveTo` while its walk to the spot is in
   flight; else `WalkTo(spot, MOVEMENT_NORMAL)`. Never `Attack()`.
8. **Rules** (`ToCDefinition_Staging.cpp`): `toc staging hold`, `IsToCStagingHeld`, families Follow,
   CombatFormationMove, SetBehind, TankFace, RearFlank, MoveRandom, TankAssist, DpsAssist, names
   `return to stay position`, `return`; `toc staging rest guard`, `IsToCStagingRestHeld`, `food`,
   `drink` (eating blinds a bot 12-18 s); `toc staging tank form guard`, `Role::Tank`,
   `IsToCStagingHeld`, `caster form` (a druid tank loses bear out of combat). Tank and dps assist go
   too: out of combat they pull on a skull, and Anub'arak turns attackable inside 80 yd.
9. **Retire `jaraxxus intro main tank`**: its trigger, node, action, creators and
   `JaraxxusIntroHoldMultiplier`; then `GetJaraxxusInIntro` and `JARAXXUS_INTRO_STAND` if nothing else
   calls them.

### Group B: ResistAura stem

Files: new `Util/ToCHelpers_ResistAura.{h,cpp}`, `Trigger/ToCTriggers_ResistAura.{h,cpp}`,
`Action/ToCActions_ResistAura.{h,cpp}`, `Definition/ToCDefinition_ResistAura.cpp`. The pattern is
Thorim's frost aura (`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp` `ElectThorimFrostAuraPaladin`,
`src/Ai/Raid/Uld/Definition/UldDefinition_Thorim.cpp` `thorim frost aura hold`), read-only. Never
`ChangeStrategy`; leave `src/Ai/Base` alone.

1. **Helper API**: `enum class ToCResistSchool : uint8 { None, Fire, Frost, Shadow, Nature };`,
   `ToCResistSchool GetToCResistDuty(PlayerbotAI*)` (notes `toc.aura`), `char const*
   ToCResistAuraName(ToCResistSchool)` (`fire resistance aura`, `frost resistance aura`,
   `shadow resistance aura`, `aspect of the wild`), `bool ToCResistAuraMissing(PlayerbotAI*)`, and
   the pure rule predicates `IsToCResistPaladinHolding`, `IsToCResistHunterHolding`.
2. **Wanted schools**, per instance per ms: while `ToCLiveEncounter` names one, its set (Beasts by
   `GetBeastsStageMask`, unioned in a heroic overlap), else `GetToCStagedArrival`'s, else none, so
   paladins return to their own aura between pulls. Sets: Gormok fire; worms fire, nature; Icehowl
   frost; Jaraxxus fire, nature (Fel Lightning resists by its lower school); champions none; Twins
   fire, then shadow; Anub'arak frost, nature.
3. **Paladin holders**, one per wanted school in the order fire, frost, shadow, re-elected at most
   every 1000 ms and at once when the set changes. Skip a school a living human paladin holds on
   himself (`GetAuraOfRankedSpell(rank 1, his guid)`; rank-1 ids in `src/Ai/Base/Trigger/BossAuraTriggers.h`).
   Candidates: living bot paladins on the instance knowing the aura (`spell id` value non-zero), not
   holding another school this round. Keep the current holder while it qualifies; else take the
   lowest of (its aura strategy already that school ? 0 : 1), (another candidate shares its aura
   strategy ? 0 : 1), cost, guid. Aura strategy is the first of `rfire`, `rfrost`, `rshadow`, `bspeed`,
   `baoe`, `bcast`, `barmor` in its combat engine; cost 0 for a resistance aura or `bspeed`, 1 for
   `baoe` or none, 2 for `bcast`, 3 for `barmor`: Sanctified and Swift Retribution work under any
   aura, Improved Devotion only under Devotion.
4. **Hunter**: nature duty for `ai::buff::GetNatureResistanceHunter(botAI, bot) == bot` while nature
   is wanted, with its own mana rotation.
5. **Trigger** `toc resistance aura` on `ToCResistAuraMissing`: a paladin by its own application
   (`bot->GetAuraOfRankedSpell(rank 1, bot->GetGUID())`: a weaker duplicate keeps its application with
   its effects stripped, and `HasAura` by name walks effects), a hunter by `HasAura("aspect of the
   wild", bot)`. **Action** `toc raise resistance aura`: `DoSpecificAction(ToCResistAuraName(duty),
   Event(), true)`, `isUseful` the same test.
6. **Rules** (`ToCDefinition_ResistAura.cpp`): `toc resistance aura hold`, `IsToCResistPaladinHolding`,
   names `devotion aura`, `retribution aura`, `concentration aura`, `crusader aura`, `sanctity aura`,
   `fire resistance aura`, `frost resistance aura`, `shadow resistance aura` (the slot is exclusive;
   the action casts outside the queue); `toc nature aspect hold`, `IsToCResistHunterHolding`,
   `aspect of the viper`, `aspect of the dragonhawk`, `aspect of the hawk`, `aspect of the monkey`,
   `aspect of the cheetah`, `aspect of the pack`.

### Group C: reader and tests

Files: new `tools/botobs/bosses/trial_of_the_crusader.py` and
`tools/botobs/tests/test_trial_of_the_crusader.py`; in `tools/botobs/tests/test_toc_naming.py`,
`RAID_WIDE_TRIGGERS` gains `toc staging stand` and `toc resistance aura`.

1. **Reader**, raid-wide over any ToC trace, in the `run_sections` shape of `bosses/lord_jaraxxus.py`.
   Banner: encounter, difficulty, and `toc.stage` missing when the trace never wrote it. `--staging`,
   per release:
   - Units: Gormok 34796; Dreadscale 34799 (`worms.mobile`), Acidmaw 35144 (`worms.stationary`);
     Icehowl 34797; Jaraxxus 34780; the first of the 28 champion entries (the enum in
     `src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.h`) to take a target; Fjola 34497, Eydis 34496;
     Anub'arak 34564.
   - Release: the first `snap` row where the unit's target column (index 7) is non-zero. Print the
     victim (name, roster role, human), the duty stand's holder with its ` human` suffix, the ms
     until the unit first targets a tank-role member (`never`), and its `dmg` rows on non-tank members
     in the 5 s from the release.
   - Per bot: its latest `toc.stage` note at or before release + 2 s whose label leads with the
     arrival, and its 2D distance (one decimal) from that stand in the last snapshot at or before the
     release (pre-roll rows have negative `t`), flagged past 3 yd; roster bots without one read
     `no stand`.
2. **Test**: synthetic v13 traces written to a temp dir as `test_toc_naming.py` does: a Gormok
   release with tank, healer and ranged bots and a human, notes before and after `t = 0`, a non-tank
   hit at 1 s; a release of both worms; a `none` label and a missing one. Assert every printed figure.

### Finisher

Integrate, run every lane check over the whole change, review it through the three lenses, fix,
document, commit.

1. Seams: `Trigger/ToCRaidTriggers.h` and `Action/ToCRaidActions.h` include the four new stem
   headers; `ToCTriggerContext.h` and `ToCActionContext.h` `Absorb` both stems' contexts;
   `Definition/ToCDefinitions.h` declares both definitions; `ToCStrategy.cpp` appends
   `AddToCStagingTriggerNodes` and `AddToCResistAuraTriggerNodes` at the end of `InitTriggers` and both
   definitions' `AddMultipliers` just before `ToCBurstWindowMultiplier`. A CMake re-run is needed.
2. Docs: the README "Pre-pull staging" gains what the code decides, "What a trace answers" gains
   `toc.stage` and `toc.aura` with the reader's usage; `lord-jaraxxus.md` drops the intro node from
   its ladder and Tactics; Known gaps go into the boss docs.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Window from Barrett's gossip: his flag gone after it was seen at this encounter's stage | brief Scope 1; `npc_announcer_toc10`, the done scenes | stage alone (bots leave the player while they buff, eat or talk) |
| Stage 3 also opens on Jaraxxus attackable and out of combat | `InstanceCleanup` re-summons him with no gossip | the latch alone (no retry staging after a cold start) |
| Anub'arak: stage 9, bot past the Web Door (x > 661.6) | brief; Barrett is invisible from stage 9 | Barrett |
| The bot's master, else the leader, must stand in the same room | Naxx Thaddius staging | none (bots stay parked after the player walks out) |
| Duty tank 3 yd from its release point, every other stand 8+ | `SelectNearestTarget(200)`; 1 yd park, pets ~3 yd off their owner | 5 / 10 yd |
| Beasts duties from the Beasts roster: Gormok and Icehowl roster[0], swap roster[1], mobile worm roster[0], stationary roster[1] | the stage model's pass 2 with nothing engaged, so the deal agrees at the engage | `IsMainTank` and assist indexes |
| Icehowl tank between him and the gate, raid on breath bearings behind him | guide: tank's back to a wall, half circle behind; known gap "no tank turns his back to the open floor" | tank toward the centre (his back to the wall, 3-4 bearings) |
| Gormok casters on 25-40 yd arcs to his sides | script Stomp 15 / 20 yd (clearance 24); guide ranged past 15, spread for Fire Bomb; the drag to `ARENA_CENTER` | a ring round the release, which the drag walks him into |
| Worm casters on a 10.5 yd lattice, 22.3-36 yd from Dreadscale, 19+ from Acidmaw, off the Spew cone; overflow doubles up | the worm rules (Sweep, melee circle, Spew, reach); guide "okay if you are in range of one other player" | 13 yd spacing (5 slots) |
| Jaraxxus casters stacked 20 yd in front, melee 9 behind, add tanks 8 to his sides | `lord-jaraxxus.md` no-spread decision; guide alternative positioning, "melee a few steps back" | spread round the room |
| Champions: every stand 28+ yd from all ten landing slots on a 10 yd lattice, healers and ranged farthest | script proximity 23 yd; w3b: no opening spread, a 25N Shout feared 13 stacked | only bots inside 28 yd move |
| Twins tank stands 22.5 yd from the south portal of their colour toward their twin, the raid by Dark 1 | portal spots; a walk to a north portal ends within 17 yd of Eydis | stands on the twin-to-centre line, nearer the north portal |
| Anub'arak: pickup on `GetAnubarakBossAnchor`, side tanks on the side points, the raid on 22 / 30 yd rings west of the centre | brief; navprobe `--nav 0x09` clean | stands by the door |
| The hold also blocks tank and dps assist | pitfalls: tank assist pulls on a skull out of combat; Anub'arak attackable inside 80 yd | follow, stay and formation only |
| Food and drink blocked while walking or for a duty tank, caster form for tanks | pitfalls: eating blinds 12-18 s; a druid tank loses bear out of combat | no block |
| `toc.stage` keeps its last value after the window closes | a session opens at the engage | `none` at the close (no label ever reaches a trace) |
| Schools: Gormok fire; worms fire + hunter nature; Icehowl frost; Jaraxxus fire + hunter nature; champions none; Twins fire, then shadow; Anub'arak frost + hunter nature | DBC `SchoolMask`; `GetResistance` takes the lowest school; champions mostly physical; raid starts Dark, so takes Light Surge (fire) | the brief's suggestion: no hunter, champions shadow |
| Holders bots only; aura already that school, then a duplicated aura, then Retribution < Concentration < Devotion, then guid; latched, re-picked on death | `spell_pal_sanctified_retribution_effect` (any aura); Improved Devotion needs Devotion | first paladin in group order |
| Auras from the staging window through the fight, nothing between pulls | brief | the whole stage (Fire Resistance from zone-in) |
| A human paladin holding a school's aura skips that school | one holder covers the raid | elect anyway |
| One raid-wide reader, `bosses/trial_of_the_crusader.py --staging` | the stands span five encounters | a section in each boss reader |
| Staging at `ACTION_RAID`, the aura at `ACTION_RAID + 1` | the aura is instant; the stand walk holds the tick while in flight | both at `ACTION_RAID` |
| Food and drink blocked only on forced-arrival duty stands (Gormok tank and swap, both worm holders, Icehowl tank, Jaraxxus tank and add tanks) and while walking | forced attacks hit at a fixed time; a player-pulled wait has no time limit | block on every tank stand |
| ` human` suffix checked only on forced duty stands, against their release point | proximity bosses never pick the nearest unit | every tank stand |
| Twins: melee take lattice slots nearest `ARENA_CENTER` first, healers and ranged the farthest | brief orders melee only; the groups share slots only once it overflows | one order for all |
| `toc.stage` names the floor-validated spot, not the raw stand | the reader measures from where the bot was sent | the raw stand |
| Every Anub'arak stand at Z 142.2, arena stands at 394.5 | brief | the accessors' 142.6 |
| Jaraxxus duties on bots only: `IsMainTank`, then `IsAssistTankOfIndex(0/1, true)`; a human main tank's stand stays empty | the fight's own assignment, so the stand and the hold agree | the next bot tank takes it |
| A bot's hold record is dropped whenever it reads outside its window or with no stand | coming back validates the spot again | keep it |
| Paladin election in two passes: every holder that still qualifies is kept first, then empty schools fill in fire, frost, shadow order | brief "keep the current holder while it qualifies" | one pass per school (a dead fire holder pulls the frost holder over) |
| "Another candidate shares its aura strategy" means any unelected bot paladin on the same strategy, whether or not it knows this school's aura; no strategy never counts | intent: keep Devotion from another paladin | only this school's candidates |
| Beasts live with an empty stage mask keeps the previous school set | the gate and the Beasts helper find Icehowl by separate searches; an empty set drops every holder for a tick | an empty set |
| No group: a bot counts only itself as a candidate | | |
| Human skip by `GetAuraOfRankedSpell(rank 1, his guid)` (any rank); candidates alive, in world, on the instance, `spell id` non-zero | brief | |
| `GetNatureResistanceHunter` may pick a human hunter, and then no bot holds nature | the generic pick, unchanged | bot hunters only |
| The raise keeps `DoSpecificAction`: a holder covered by another paladin's same-school aura doesn't cast | the class action reads that aura by name, so the holder keeps its own aura while the other's lasts | a direct `CastSpell` (a stripped duplicate that costs the raid Devotion) |
| `toc.stage`: for 2 s after an arrival closes, any bot's tick notes every bot's last label again | a note before a session opens is dropped and a ToC session opens at the engage, which also closes the window; a bot mid-cast or drinking runs no triggers | `RaidObs::MarkPull` at the window (the 30 s idle close ends a session inside the 70.8 s Jaraxxus intro) |
| Reader: a bot whose latest `toc.stage` at or before release + 2 s reads `none` shows `none`, not an older stand | `none` means in the window with no stand (the essence walk) | the brief's rule, which skips `none` |
| Reader duty stands: Gormok `tank`, Dreadscale `mobile`, Acidmaw `stationary`, Icehowl `tank`, Jaraxxus `tank`, Fjola `fjola`/`both`, Eydis `eydis`/`both`, Anub'arak `pickup`; champions none. The duty line names the role only where a release has two, the ` human` suffix always | the boss's first pick | add tanks and swap as duties |

## Carried over

- w6: `lord_jaraxxus.py`, `anub_arak.py`, `northrend_beasts.py` and `val_kyr_twins.py` banners list
  `toc.stage` and `toc.aura` as silent on a pull with no staging window (a heroic Beasts overlap, an
  older build) or no paladin or hunter bot; `faction_champions.py` already leaves `toc.` keys out.
- w6: `faction-champions.md`'s "opening Shout or Bladestorm" gap predates the staging lattice
  (ranged and healers start 10 yd apart); re-check on a trace with staging.
- CMake re-run: eight new TUs (`Util/ToCHelpers_{Staging,ResistAura}.cpp`,
  `Trigger/ToCTriggers_{Staging,ResistAura}.cpp`, `Action/ToCActions_{Staging,ResistAura}.cpp`,
  `Definition/ToCDefinition_{Staging,ResistAura}.cpp`).

## Known gaps

Moved into the README's "Pre-pull staging", `northrend-beasts.md` and `lord-jaraxxus.md`, with
these found in review: Gormok's closest caster stands are 7.1 yd apart (the slots above, not 7.8),
under Fire Bomb's 8; a stand is floor-checked from wherever its bot stands when dealt; a heroic raid
out of attempts keeps the window open; a holder covered by another paladin's same-school aura fires
its trigger every tick without casting; a human main tank leaves Jaraxxus' tank stand empty.

- A Beasts retry gives 11 s against an 85 yd walk from Barrett to Gormok's tank stand.
- Totems and pets left near a release point earlier aren't moved; forced attacks pick them too.
- A worm healer slot rarely reaches both tank stands within 30 yd.
- Humans hold no stand; shaman resistance totems don't count toward a school; no Aura Mastery calls.
- A lone hunter holding Aspect of the Wild loses Viper (`GetNatureResistanceHunter`'s last fallback).

## Blocked

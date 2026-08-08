# Auriaya — complete the Ulduar boss strategy

## Context

`docs/raids/ulduar.md:355` lists Auriaya as "entire fight unimplemented except fall-recovery". That
row is **stale**: commit `483086f3c` (after the doc's last edit `b94735a7d`) added a Sonic Screech
cone dodge, Seeping Feral Essence avoidance, skull marking and anti-fear. So this task is not a
green-field strategy — it is closing the real gaps in the existing one, then correcting the doc.

Auditing the current code against `src/server/scripts/Northrend/Ulduar/Ulduar/boss_auriaya.cpp`,
`creature_template` and `spell_cone` turned up four defects and three missing behaviours. The worst
is that **every Auriaya trigger resolves the boss through `"find target"`**, which
`docs/engine/pitfalls.md:80-83` documents as walking only *the bot's own threat list*. Any bot
parked on a Sanctum Sentry or the Feral Defender never resolves Auriaya, so its cone dodge, void-zone
dodge and anti-fear all silently switch off — precisely during the adds phases those bots are in.

Outcome: a complete, both-difficulty Auriaya strategy (Ulduar is 10/25-man only and every entry and
spell id is difficulty-shared, so difficulty needs no special handling), plus an accurate doc.

## Encounter facts established from source and DB

| Mechanic | Id | Truth |
|---|---|---|
| Sonic Screech | 64422 | `spell_cone.ConeDegrees = 120`. Frontal cone, damage split among those hit |
| Terrifying Screech | 64386 | AoE fear every 35s from 35s |
| Sentinel Blast | 64389 | **Not** in `spell_cone`; its SpellScript strips non-players → raid-wide, unavoidable |
| Guardian Swarm | 64396 | `DoCastVictim`, tank-only DoT; generic dispel covers it |
| Sanctum Sentry | 34014 | `unit_flags 0` (selectable). Strength of the Pack 64369 buffs the boss while alive; Savage Pounce 64666 fires **only** at 8–25 yd from its own victim; Rip Flesh 64375 on victim |
| Feral Defender | 34035 | Summoned at 60s. Random-aggro periodic 61906 → untankable. 8 Feral Essence stacks + itself = 9 lives; each "death" is a feign (1 HP, `UNIT_FLAG_NOT_SELECTABLE`) that respawns after ~35s |
| Seeping Feral Essence | 34098 | `unit_flags 0x2000000` — non-selectable stalker, so only `"nearest npcs"` / `FindNearestCreature` sees it |
| Enrage | 47008 | 10 min. No enrage awareness exists anywhere in the module; out of scope |

Auriaya's own entry is `NPC_AURIAYA = 33515` (core `ulduar.h:170`).

## Defects to fix in existing code

1. **Boss lookup** — all six `AI_VALUE2(Unit*, "find target", "auriaya")` sites in
   [UldTriggers_Auriaya.cpp](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Auriaya.cpp),
   [UldActions_Auriaya.cpp](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Auriaya.cpp) and
   [UldBossHelper.cpp:324](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L324) must go
   through `GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA)`.
2. **Cone angle** — [UldTriggers_Auriaya.cpp:43](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Auriaya.cpp#L43)
   passes `M_PI / 2.0f` (90°). `IsBotInFrontalCone` forwards to `HasInArc`, which takes the **full**
   arc, so bots between 45° and 60° off-centre stand in a cone they believe they dodged. Needs 120°.
3. **Skull mark is cosmetic** — `AuriayaMarkDpsTargetAction` sets the icon, but Auriaya has no
   `"attack rti target"` node, so nothing retargets. Kologarn
   ([UldStrategy.cpp:180-181](modules/mod-playerbots/src/Ai/Raid/Uld/UldStrategy.cpp#L180-L181)) and RS
   both pair the mark with one.
4. **Wrong comment** — [UldTriggers_Auriaya.cpp:42](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Auriaya.cpp#L42)
   calls Sentinel Blast a frontal cone. It is raid-wide.

Confirmed already correct, leave alone: `TooCloseToCreatureTrigger` (`FindNearestCreature`) and
`MoveAwayFromCreatureAction` (`"nearest npcs"`) both see the non-selectable essence stalker, and
`GetFirstLiveUnitByEntry` / `IsDownOrFeigning` already handle the Feral Defender's feign correctly.

## Behaviours to add

- **Focus order flips to Sanctum Sentry → Feral Defender → boss.** Sentries die permanently and
  drop the boss's Strength of the Pack buff; the Defender only ever feigns and costs a void zone per
  kill. Current code prefers the Defender.
- **Sanctum Sentry off-tank pickup.** Assist tank 0 taunts a loose sentry. Melee-range tanking is the
  default, which also denies Savage Pounce's 8–25 yd window for free.
- **Main tank steers Auriaya's facing** so the 120° cone points away from the raid.

## Implementation

### 1. `src/Ai/Raid/Uld/Util/UldScripts.h`

Add `NPC_AURIAYA = 33515` under an `// Auriaya` comment, matching the "ported from `ulduar.h`"
convention already used for the other bosses.

### 2. `src/Ai/Raid/Uld/Util/UldBossHelper.{h,cpp}`

Constants next to the existing `ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT`:

```cpp
// Sonic Screech (64422) is a 120-degree frontal cone per spell_cone; HasInArc takes the full arc.
constexpr float ULDUAR_AURIAYA_SONIC_SCREECH_CONE = 2.0f * static_cast<float>(M_PI) / 3.0f;
constexpr float ULDUAR_AURIAYA_SONIC_SCREECH_RANGE = 45.0f;
constexpr float ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS = 10.0f;   // DBC radius, confirm in-game
constexpr float ULDUAR_AURIAYA_FACING_TOLERANCE = 0.15f;         // rad; below this the tank holds still
constexpr float ULDUAR_AURIAYA_FACING_ARC_STEP = 0.125f;         // rad per tick, as Sindragosa
constexpr float ULDUAR_AURIAYA_FACING_MIN_RAID_DIST = 8.0f;      // raid this close makes the bearing noise
```

Helpers (one definition each, shared by trigger, action and multiplier — they hold separate
instances, per `docs/engine/pitfalls.md:132`):

- `Unit* GetAuriaya(PlayerbotAI* botAI)` → `GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA)`.
- `bool AuriayaEncounterActive(PlayerbotAI* botAI)` → `GetAuriaya(botAI) != nullptr`. Rewrite
  `AuriayaFearWindowActive` to call it. The window stays "boss alive" — the fear repeats every 35s,
  so there is no useful narrower window.
- `Unit* GetAuriayaFocusTarget(PlayerbotAI* botAI)` → `GetFirstAliveUnitByEntry(NPC_AURIAYA_SANCTUM_SENTRY)`,
  else `GetFirstLiveUnitByEntry(NPC_AURIAYA_FERAL_DEFENDER)` (live, not merely alive — the Defender
  feigns), else `nullptr`. Single source of truth for both the mark trigger and the mark action,
  which currently duplicate the selection.
- `bool UldCastClassTaunt(PlayerbotAI* botAI, Unit* target)` — the warrior/paladin/DK/druid switch
  currently inlined in `XT002PummellerTauntAction` and `UldActions_YoggSaron.cpp:603`. Mirrors ICC's
  `IccCastClassTaunt`. Use it for the new sentry taunt; retro-fitting the two existing call sites is
  optional and out of scope.

### 3. `src/Ai/Raid/Uld/Trigger/UldTriggers_Auriaya.{h,cpp}`

Existing triggers: swap in `GetAuriaya`, the cone constants, and `GetAuriayaFocusTarget`; fix the
Sentinel Blast comment (it is raid-wide, mentioned only so nobody re-adds a dodge for it).

New:

- `AuriayaAttackDpsTargetTrigger` — mirrors `IsSkullOnXT002Add`
  ([UldTriggers_XT002.cpp:42-56](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_XT002.cpp#L42-L56)):
  active when the encounter is up, the bot is not the main tank, and the group skull is on a live
  `NPC_AURIAYA_SANCTUM_SENTRY` or `NPC_AURIAYA_FERAL_DEFENDER` (`!IsDownOrFeigning`) that is not
  already the bot's target.
- `AuriayaSentryTauntTrigger` — active when the bot is assist tank 0
  (`IsAssistTankOfIndex(bot, 0, /*ignoreDeadPlayers*/ true` — the default `false` silently deletes
  the role when a tank dies, per `docs/engine/pitfalls.md:148`), a live sentry exists, and that
  sentry's victim is not this bot.
- `AuriayaTankFacingTrigger` — active when the bot is the main tank, Auriaya is alive **and her
  victim is this bot** (otherwise the facing is not ours to steer), the raid centroid is at least
  `ULDUAR_AURIAYA_FACING_MIN_RAID_DIST` from her, and the facing error exceeds
  `ULDUAR_AURIAYA_FACING_TOLERANCE`.

### 4. `src/Ai/Raid/Uld/Action/UldActions_Auriaya.{h,cpp}`

`AuriayaMarkDpsTargetAction` uses `GetAuriayaFocusTarget`; drop its duplicated selection. Keep the
existing `MarkTargetWithSkull` + `SetRtiTarget(botAI, "skull", …)` pair.

New:

- `AuriayaSentryTauntAction : Action` — `UldCastClassTaunt(botAI, GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA_SANCTUM_SENTRY))`.
- `AuriayaTankFacingAction : MovementAction` — arc-step the main tank around Auriaya until she faces
  away from the raid. Copy the shape of
  [ICCActions_SG.cpp:130-149](modules/mod-playerbots/src/Ai/Raid/ICC/Action/ICCActions_SG.cpp#L130-L149),
  which is the only shipped precedent for correcting a boss's facing:

  1. Centroid of alive group members on map 603, **excluding the main tank and assist tank 0**, so
     the tanks' own positions do not drag the target bearing. Mirror the static
     `ComputeGroupCentroid` in
     [ICCActions_LK.cpp:101](modules/mod-playerbots/src/Ai/Raid/ICC/Action/ICCActions_LK.cpp#L101).
  2. Desired orientation = bearing from centroid to Auriaya, i.e. cone pointing away from the raid.
     A tanked boss faces its victim, so the tank drives it by standing on that bearing.
  3. Signed error against `boss->GetOrientation()`, normalised to `(-π, π]`. Step one
     `ULDUAR_AURIAYA_FACING_ARC_STEP` around the boss at the bot's **current** radius — never a jump
     to the far side, which would drag the boss through the raid.
  4. `MoveTo(..., MovementPriority::MOVEMENT_FORCED)` with the same argument shape as Sindragosa.

  Two constraints this shape exists to satisfy: a moving bot cannot cast anything with a cast time
  (`docs/engine/pitfalls.md:28-33`), so the deadband in the trigger must let the tank settle; and raw
  ring geometry produces off-mesh points (`:49-60`), which the small step at melee radius keeps
  harmless.

### 5. Registration

`UldTriggerContext.h` and `UldActionContext.h`: `creators[…]` entry plus a static factory for each of
the three new triggers and two new actions, in the existing Auriaya blocks (`:111-114` / `:110-113`
and `:232-235` / `:228-231`). Names must match the `TriggerNode` / `NextAction` strings **exactly** —
an unregistered trigger is skipped with no warning. The other two wiring sites
(`BuildShared*Contexts.cpp`) and `PlayerbotAI.cpp` already carry Ulduar; nothing to add.

### 6. `src/Ai/Raid/Uld/UldStrategy.cpp` — Auriaya block at `:199-220`

| Node | Priority | Why |
|---|---|---|
| `auriaya sentry taunt` | `ACTION_RAID + 3` | An untanked sentry is the worst state; tank-only, so it competes with nothing else |
| `auriaya seeping essence` (existing) | `ACTION_RAID + 2` | unchanged |
| `auriaya anti fear` (existing) | `ACTION_RAID + 2` | unchanged |
| `auriaya sonic screech` (existing) | `ACTION_RAID + 1` | unchanged |
| `auriaya attack dps target` → `attack rti target` | `ACTION_RAID` | matches Kologarn; dodges must outrank retargeting |
| `auriaya mark dps target` (existing) | `ACTION_RAID` | unchanged |
| `auriaya tank facing` | `ACTION_RAID` | main-tank-only, so it never contends with the DPS nodes |
| `auriaya fall from floor` (existing) | `ACTION_RAID` | unchanged |

No new multiplier. Per the doc's threat-redirect table Auriaya is deliberately **not** vetoed (she is
main-tank-held all fight), and the burst-window table deliberately leaves her ungated.

### 7. `docs/raids/ulduar.md`

`CLAUDE.md` requires `/compact-docs-writer` **before** editing this doc — it is convention knowledge
reached indirectly from `docs/raids/README.md`. Invoke it at the start of the doc edit, not after.

- Delete the stale Auriaya row from the Sev-1 table at `:355`.
- Add an `## Auriaya` section carrying what this investigation cost: the 120° cone from `spell_cone`
  vs. the 90° the code assumed; Sentinel Blast being raid-wide rather than a cone; sentries before
  Defender and why; Savage Pounce's 8–25 yd gate making a melee-range tank the whole counter; the
  Defender's random aggro making it untankable and its feign meaning `GetFirstLiveUnitByEntry`, not
  `GetFirstAliveUnitByEntry`; and the `"find target"` threat-list trap that this change removes.
- Note the two remaining accepted gaps: no enrage-timer awareness (module-wide), and the Crazy Cat
  Lady achievement being incompatible with killing sentries — bots optimise for the kill, and per the
  raid's follower model they never chase achievements.

Also worth adding to `docs/engine/pitfalls.md` under "Names fail silently": nothing new here, but the
`"find target"` bullet at `:80-83` gains Auriaya as a second casualty alongside Thane/Zeliek.

## Verification

Static, before claiming done:

```bash
python apps/codestyle/codestyle-cpp.py
```

Name-registration sweep — every new string must appear on both sides:

```bash
grep -n "auriaya" src/Ai/Raid/Uld/UldStrategy.cpp
grep -n "auriaya" src/Ai/Raid/Uld/UldTriggerContext.h src/Ai/Raid/Uld/UldActionContext.h
grep -rn "find target\", \"auriaya" src/Ai/Raid/Uld/   # must return nothing afterwards
```

The module cannot be compiled headless in this environment, so the build is the user's step — do not
run one unless asked.

In-game, map 603, both 10 and 25-man (identical entries and spell ids, so one pass per difficulty is
confirmation, not separate behaviour):

1. Pull Auriaya. Confirm assist tank 0 taunts both Sanctum Sentries and holds them in melee, and that
   no Savage Pounce lands.
2. Confirm the skull sits on a sentry and that non-tanks actually **switch target** to it — this is
   the part that silently did nothing before.
3. Watch the main tank arc-step until Auriaya's back is to the raid, then stop. Confirm the tank
   still casts (a tank that never stops moving never casts).
4. At the Sonic Screech cast, confirm nobody outside the tanks takes damage — the old 90° assumption
   showed up as bots eating the cone while standing "clear".
5. At ~60s, confirm the skull moves to the Feral Defender once the sentries are dead, that bots move
   out of each Seeping Feral Essence pool, and that the skull does **not** stick to the Defender
   while it is feigning.
6. On Terrifying Screech, confirm Tremor Totem is down and Fear Ward is on the main tank.
```

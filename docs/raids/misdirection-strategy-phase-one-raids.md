# Threat redirect (Misdirection / Tricks of the Trade) in Ulduar, Obsidian Sanctum and Eye of Eternity

## Context

Hunter Misdirection and rogue Tricks of the Trade are wired two ways in this module:

1. **Generic class nodes**, which always redirect at the **group main tank**:
   - `"low tank threat"` → `misdirection on main tank` @ 27.0f — [GenericHunterStrategy.cpp:68](modules/mod-playerbots/src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp#L68)
   - `"misdirection on main tank and light aoe"` → same action @ 27.0f — [GenericHunterStrategy.cpp:71-72](modules/mod-playerbots/src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp#L71-L72)
   - rogue mirrors at `ACTION_HIGH + 7` — [DpsRogueStrategy.cpp:213-231](modules/mod-playerbots/src/Ai/Class/Rogue/Strategy/DpsRogueStrategy.cpp#L213-L231), [AssassinationRogueStrategy.cpp:197-215](modules/mod-playerbots/src/Ai/Class/Rogue/Strategy/AssassinationRogueStrategy.cpp#L197-L215)
2. **Boss-scoped actions** inside a raid strategy, for when the main tank is *not* the right threat
   sink, or when the moment that matters is not the pull.

Boss-scoped redirects exist across **every TBC raid** (BT, SSC, TK, SWP, Hyjal, ZA, Gruul, Mag) and on
**five Naxxramas bosses** (Kel'Thuzad, Thaddius, Four Horsemen, Gluth, Anub'Rekhan).

**No WotLK raid has one, and no WotLK raid has a veto either.** `Uld/` registers exactly one
multiplier ([UldStrategy.cpp:481](modules/mod-playerbots/src/Ai/Raid/Uld/UldStrategy.cpp#L481),
`AlgalonMultiplier`), `OS/` one (`SartharionMultiplier`), `EoE/` one (`MalygosMultiplier`), and none of
them `dynamic_cast`s to `CastMisdirectionOnMainTankAction` /
`CastTricksOfTheTradeOnMainTankAction`. So the generic main-tank redirect fires unconditionally —
including on the Ulduar encounters where the strategy deliberately puts a second tank, an add tank, or
a swap tank on the thing the DPS is hitting.

Meanwhile the multi-tank infrastructure is already in place: `GetGroupMainTank` /
`GetGroupAssistTank` ([RaidBossHelpers.h:27-28](modules/mod-playerbots/src/Ai/Raid/RaidBossHelpers.h#L27-L28)),
`PlayerbotAI::IsAssistTankOfIndex` (used across `UldActions_Thorim.cpp`, `UldActions_Razorscale.cpp`,
`UldActions_Kologarn.cpp`, `UldActions_Freya.cpp`, `UldActions_Mimiron.cpp`,
`UldTriggers_IronAssembly.cpp`, `UldTriggers_Algalon.cpp`, `UldTriggers_Auriaya.cpp`), and per-boss
swap actions (`IronAssemblyFusionPunchSwapAction`, `ThorimUnbalancingStrikeSwapAction`,
`AlgalonPhasePunchSwapAction`).

**Scope of this task** (confirmed with the user): Ulduar, Obsidian Sanctum and Eye of Eternity only.
Deliver the findings document **plus the veto slice** — no new boss-scoped redirect actions, and no
refactor of the working Naxx code.

## Rubric (inherited from the existing Naxx findings doc)

> The generic node already redirects to the main tank at the pull. A dedicated action only earns its
> keep when **the target is not the main tank**, or **the moment that matters is not the pull**.

The corollary matters just as much here: a veto only earns its keep where the main tank is *reliably*
wrong. Vetoing an encounter that is merely *imperfect* for the MT turns hunters and rogues into dead
weight — the exact criticism the Naxx doc levels at the old blanket veto.

### Two supporting facts established during investigation

- **No pull-window helper exists outside Naxx and RS.** Naxx expresses pull windows with
  `JustStartCombat()` / `PullWindowMs = 10000` backed by a per-boss combat-start timestamp
  ([NaxxBossHelper.h:2155](modules/mod-playerbots/src/Ai/Raid/Naxx/NaxxBossHelper.h#L2155)). A grep for
  `JustStartCombat|_combat_start_ms|encounterStart` over `src/Ai/Raid/` finds a combat clock only in
  `RS/` and `SWP/`. Ulduar, OS and EoE have none. If dedicated actions follow later, use BT's
  stateless `boss->GetHealthPct() > 95.0f` idiom
  ([BTTriggers.cpp:28](modules/mod-playerbots/src/Ai/Raid/BT/BTTriggers.cpp#L28)) rather than building
  a clock — it needs no new state and doubles as the fresh-spawn / fresh-phase test.
- **`GetFirstAliveUnitByEntry` beats `"find target"` for multi-tank detection.**
  `AI_VALUE2(Unit*, "find target", name)` only resolves creatures that already have *this* bot on their
  threat list — which is precisely wrong on a one-tank-per-boss encounter, where a bot parked on boss A
  never sees boss B. This is the documented Four Horsemen failure mode. Use entry-based
  `GetFirstAliveUnitByEntry(botAI, entry)` ([RaidBossHelpers.h:29](modules/mod-playerbots/src/Ai/Raid/RaidBossHelpers.h#L29))
  instead; Ulduar's entries are already enumerated in `UlduarIDs`
  ([UldBossHelper.h:25-180](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.h#L25-L180)), with
  the boss entries themselves coming from core `ulduar.h` via `UldScripts.h`.

## Findings

### Ulduar — veto now (the generic main-tank redirect is *actively wrong*)

| Boss | Why the main tank is the wrong sink | State to gate on |
|---|---|---|
| **Iron Assembly** (Steelbreaker / Runemaster Molgeim / Stormcaller Brundir) | Three bosses, one tank each; Fusion Punch then forces a swap. The MT is wrong for two of the three targets for the whole fight. | boss entries from core `ulduar.h`; `IronAssemblyFusionPunchSwapTrigger` already resolves the *active* tank ([UldTriggers_IronAssembly.cpp:102-115](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_IronAssembly.cpp#L102-L115)) |
| **Mimiron** | Four phases, each a different creature with a fresh threat table; phase 3 has two tankable units at once (VX-001 + Aerial Command Unit) split MT / AT0. | `NPC_LEVIATHAN_MKII` 33432, `NPC_VX001` 33651, `NPC_AERIAL_COMMAND_UNIT` 33670 ([UldBossHelper.h:97-99](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.h#L97-L99)); MT/AT0 split at [UldActions_Mimiron.cpp:245](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp#L245) |
| **Thorim** | The raid splits into arena and gauntlet squads with a tank each, so half the DPS is nowhere near the MT; Unbalancing Strike then forces a swap on the boss. | `IsAssistTankOfIndex(bot, 0)` branches at [UldActions_Thorim.cpp:97,139](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp#L97); `ThorimUnbalancingStrikeSwapTrigger` |
| **Algalon** | Phase Punch forces an MT↔AT0 swap on a stack timer; a redirect at the MT pins Algalon on him mid-swap. | `AlgalonPhasePunchSwapTrigger` already computes the active tank and its swap partner ([UldTriggers_Algalon.cpp:89-103](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Algalon.cpp#L89-L103)) |
| **Razorscale — flying phase only** | While she is airborne the MT holds nothing; the Dark Rune adds belong to the assist tanks. Grounded phase is single-tank and the generic node is *correct* there, so this must be phase-gated, not blanket. | `IsFlyingPhase()` / `IsGroundPhase()` ([UldBossHelper.h:380-381](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.h#L380-L381)); assist-tank loops at [UldActions_Razorscale.cpp:142,190,308,358,478](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Razorscale.cpp#L142) |

The phase-gated Razorscale entry has direct precedent: `NaxxThreatRedirectMultiplier` gates on Heigan's
`IsFastDance()` so the charges are held for the arena return rather than burned on his ledge
([NaxxMultipliers.cpp:561-600](modules/mod-playerbots/src/Ai/Raid/Naxx/NaxxMultipliers.cpp#L561-L600)).

### Ulduar — do *not* veto; candidates for a dedicated action later

The boss itself is MT-held for the whole fight, so the generic node is right for the primary target.
The gain would be redirecting the *adds* at the tank who already picks them up — real, but a veto here
costs more than it saves.

| Boss | Add-tank value |
|---|---|
| **Auriaya** | Sanctum Sentry pack at the pull, and the Feral Defender resurrects up to nine times — each respawn is a fresh threat table. MT/AT0 already used ([UldTriggers_Auriaya.cpp:39](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Auriaya.cpp#L39)). |
| **Freya** | Three Elders alive together in hard mode plus repeating add waves — Snaplasher, Storm Lasher, Detonating Lasher, Ancient Water Spirit, Ancient Conservator ([UldBossHelper.h:57-63](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.h#L57-L63)) — on assist tanks ([UldActions_Freya.cpp:134](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Freya.cpp#L134)). |
| **Kologarn** | Right Arm (`NPC_RIGHT_ARM` 32934) is a separate unit and Rubble (`NPC_RUBBLE` 33768) goes to AT0 ([UldActions_Kologarn.cpp:102,139,243](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Kologarn.cpp#L102)); the body never resets threat. |
| **Yogg-Saron** | Crusher Tentacles want a tank in phase 1 and the phase-3 body is effectively a fresh pull, but the only phase state is `YoggSaronPhase3PositioningTrigger` — thinner than the other candidates. |
| **Ignis** | Single tank on the boss; the Iron Constructs are the add-tank case. |

### Ulduar — no

| Boss | Why |
|---|---|
| **General Vezax** | One tank, no swap, no reset. The generic node is exactly right and wants to fire all fight. |
| **Hodir** | No meaningful tanking; nothing to redirect. |
| **Flame Leviathan** | Vehicle combat — no threat table, and neither spell is castable. |
| **XT-002 Deconstructor** | No strategy files exist at all (13 of Ulduar's 14 encounters are implemented), so there is nothing to hook. |

### Obsidian Sanctum — narrow veto, low value

The off-tank grabs **every** landed drake and parks them at `SARTHARION_OFFTANK_POSITION`, far from the
raid stack, so neither the drakes' frontal Shadow Breath nor boss-centred AoE crosses the raid
([OSActions.cpp:59-101](modules/mod-playerbots/src/Ai/Raid/OS/OSActions.cpp#L59-L101)). The MT holds
Sartharion himself at `SARTHARION_MAINTANK_POSITION`. So while the raid is burning a to-kill drake, a
redirect at the MT aims threat at a player standing on the boss — the opposite of what the off-tank
logic exists to achieve.

It is **not a wipe risk**, and the doc should say so plainly: `ObsidianSanctumHelpers::ForceThreat`
([OSShared.h:133-141](modules/mod-playerbots/src/Ai/Raid/OS/OSShared.h#L133-L141)) does
`AddThreat(bot, 1000000.0f, …)` followed by `FixateTarget(bot)`, and fixate overrides threat outright.
The redirect cannot actually move a drake. The real cost is a **wasted cooldown** — three Misdirection
charges or a 6s Tricks window burned on a threat table that cannot matter — plus MT threat pollution
that surfaces only if the fixate lapses.

Verdict: veto the redirect **only while the bot's current target is one of the three drakes**, so the
cooldown is saved for Sartharion, where the MT *is* the right sink. `IsDrakeEntry(entry)` already exists
([OSShared.h:28](modules/mod-playerbots/src/Ai/Raid/OS/OSShared.h#L28)).

### Eye of Eternity — nothing to do

Recorded explicitly so it does not get re-litigated:

- **Phase 1** — single tank on Malygos, no swap, no threat reset. The generic node is correct and wants
  to fire for the whole phase. Power Sparks are handled by DK Death Grip and focused damage, not tanking
  ([EoEStrategy.cpp:18-22](modules/mod-playerbots/src/Ai/Raid/EoE/EoEStrategy.cpp#L18-L22)).
- **Phase 2** — Malygos is airborne and `UNIT_FLAG_NON_ATTACKABLE`; the raid kills Nexus Lords and Scions
  of Eternity rather than parking them on a dedicated tank, so the MT is as good a sink as any.
- **Phase 3** — drake vehicle combat (`NPC_WYRMREST_SKYTALON`); neither Misdirection nor Tricks is
  castable, so the node is inert rather than harmful.

Phase state is available via `MalygosTrigger::getPhase(bot)`
([EoETriggers.cpp:17-42](modules/mod-playerbots/src/Ai/Raid/EoE/EoETriggers.cpp#L17-L42)) if this is ever
revisited. No veto, no dedicated action.

### Note on the shared base, for whoever implements actions later

`NaxxRedirectThreatAction` ([NaxxActions.h:28-40](modules/mod-playerbots/src/Ai/Raid/Naxx/Action/NaxxActions.h#L28-L40),
body [NaxxActions_Shared.cpp:12-108](modules/mod-playerbots/src/Ai/Raid/Naxx/Action/NaxxActions_Shared.cpp#L12-L108))
is the only redirect base that covers **hunters and rogues** — every BT/SSC/TK/SWP/Hyjal/ZA action is
hunter-only. Subclasses implement just `GetRedirectTank()` and `GetThreatDumpTarget()`; the base also
provides `GetTankHolding(Unit*)` and `GetRedirecterIndex()`.

Per the user's decision, it stays where it is: any future Ulduar action gets its **own copy** rather
than promoting it to a shared raid-level base. The known cost of that choice, for the record: the
`35079` Misdirection proc-aura id is already duplicated across nine per-raid helper headers
(`NaxxSpellIds.h:112`, `ZAHelpers.h:40`, `GruulHelpers.h:24`, `BTHelpers.h:86`, `HyjalHelpers.h:42`,
`MagHelpers.h:31`, `TKHelpers.h:48`, `SSCHelpers.h:64`, `SWPData.h:69`) and Ulduar would make ten.

## Implementation

### 1. Findings document

Write `docs/raids/ulduar/ulduar-os-eoe-threat-redirect-findings.md`, following the format of the
existing [docs/raids/naxxramas/naxx-threat-redirect-findings.md](modules/mod-playerbots/docs/raids/naxxramas/naxx-threat-redirect-findings.md):
context → the pattern and its templates → current state → per-boss verdict tables → what was
implemented → notes for whoever picks up the dedicated actions. Content is the Findings section above.

### 2. `UldThreatRedirectMultiplier`

- `src/Ai/Raid/Uld/UldMultipliers.h` — declare alongside `AlgalonMultiplier`, name `"uld threat redirect"`.
- `src/Ai/Raid/Uld/UldMultipliers.cpp` — `GetValue`:
  - early-return `1.0f` unless the action `dynamic_cast`s to `CastMisdirectionOnMainTankAction` or
    `CastTricksOfTheTradeOnMainTankAction` (**must not** cast to the shared `BuffOnMainTankAction` base —
    that would also kill paladin Beacon of Light, shaman Earth Shield and druid Thorns/Lifebloom on the
    tank, a bug the Naxx work had to undo);
  - return `0.0f` when `GetFirstAliveUnitByEntry` resolves any Iron Assembly member, any Mimiron phase
    unit, Thorim, or Algalon;
  - return `0.0f` for Razorscale **only** when `IsFlyingPhase()`;
  - otherwise `1.0f`.
  - Requires `#include "HunterActions.h"`, `"RogueActions.h"`, `"RaidBossHelpers.h"`, `"UldBossHelper.h"`.
- `src/Ai/Raid/Uld/UldStrategy.cpp` — `multipliers.push_back(new UldThreatRedirectMultiplier(botAI));`
  next to the existing `AlgalonMultiplier` push at line 481.

### 3. Obsidian Sanctum drake veto

Add the check to the existing `SartharionMultiplier::GetValue`
([OSMultipliers.h:12-19](modules/mod-playerbots/src/Ai/Raid/OS/OSMultipliers.h#L12-L19)) rather than
introducing a new class — OS is a single-encounter raid and the multiplier is already registered and
boss-gated ([OSStrategy.cpp:39](modules/mod-playerbots/src/Ai/Raid/OS/OSStrategy.cpp#L39)). Same
two-`dynamic_cast` guard, then `0.0f` when `AI_VALUE(Unit*, "current target")` is a unit whose entry
satisfies `ObsidianSanctumHelpers::IsDrakeEntry`.

### 4. Eye of Eternity

No code change. The doc records why.

No build-file changes anywhere: the module has **no `CMakeLists.txt`** — AzerothCore's module macro
globs `src/**/*.cpp`, so even new files need no registration. Heroic/25-man needs no special handling:
creature entries are identical across difficulties, and these vetoes read entries and tank roles, not
difficulty-specific spell ids.

## Verification

There is no unit-test harness for this module and it **cannot be compiled headless in this
environment** — verification is a build plus in-game observation. Do not report this as verified
without one.

1. **Build**: configure and build AzerothCore with `mod-playerbots` enabled; confirm no new warnings in
   the four touched files. Multipliers are `dynamic_cast`-based, so unlike creator-name strings a
   mistake here does surface at compile time.
2. **Ulduar (map 603), veto positive cases** — raid with a bot MT, a bot AT, a hunter bot and a rogue
   bot. On Iron Assembly, Mimiron, Thorim and Algalon expect **zero** `Misdirection` /
   `Tricks of the Trade` casts for the whole encounter.
3. **Razorscale phase gate** — no redirect casts while she is airborne; redirects resume the moment she
   is grounded. This is the one entry that can regress silently into a blanket veto.
4. **Ulduar control cases** — Vezax and Kologarn must still show the generic redirect firing, proving
   the veto is boss-scoped rather than a blanket kill.
5. **Non-redirect buffs on the tank still land** — with a paladin, shaman or druid bot in the raid,
   confirm Beacon of Light / Earth Shield / Thorns are still cast on the tank during the vetoed Ulduar
   encounters. This is the specific regression the narrow `dynamic_cast` prevents.
6. **Obsidian Sanctum (map 615)** — with drakes up, no redirect casts while a bot's current target is a
   drake; redirects fire normally once the raid switches to Sartharion.
7. **Eye of Eternity (map 616)** — unchanged behaviour in all three phases (control for "no code
   change").

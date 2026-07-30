# Threat redirect in Ulduar, Obsidian Sanctum and Eye of Eternity

## Context

Hunter Misdirection and rogue Tricks of the Trade reach a raid two ways:

1. **Generic class nodes**, which always redirect at the **group main tank**:
   - `"low tank threat"` → `misdirection on main tank` @ 27.0f (`src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp:68`)
   - `"misdirection on main tank and light aoe"` → same action @ 27.0f (`:71-72`)
   - rogue mirrors at `ACTION_HIGH + 7` (`src/Ai/Class/Rogue/Strategy/DpsRogueStrategy.cpp:213-231`,
     `AssassinationRogueStrategy.cpp:197-215`)
2. **Boss-scoped actions** inside a raid strategy, for when the main tank is *not* the right threat
   sink, or when the moment that matters is not the pull.

Boss-scoped redirects exist across every TBC raid (BT, SSC, TK, SWP, Hyjal, ZA, Gruul, Mag) and on
five Naxxramas bosses. Before this work **no WotLK raid had either an action or a veto**: `Uld/`
registered `AlgalonMultiplier`, `XT002BurstWindowMultiplier`, `XT002TargetGuardMultiplier` and
`UlduarBurstWindowMultiplier`, `OS/` one `SartharionMultiplier`, `EoE/` one `MalygosMultiplier` — and
only the XT-002 target guard touched the redirects. So the generic main-tank redirect fired
unconditionally, including on the Ulduar encounters where the strategy deliberately puts a second
tank, an add tank or a swap tank on the thing the DPS is hitting.

The multi-tank infrastructure was already there: `GetGroupMainTank` / `GetGroupAssistTank`
(`src/Ai/Raid/RaidBossHelpers.h:27-28`), `PlayerbotAI::IsAssistTankOfIndex` (used across
`UldActions_Thorim.cpp`, `UldActions_Razorscale.cpp`, `UldActions_Kologarn.cpp`,
`UldActions_Freya.cpp`, `UldActions_Mimiron.cpp`, `UldTriggers_IronAssembly.cpp`,
`UldTriggers_Algalon.cpp`, `UldTriggers_Auriaya.cpp`) and per-boss swap actions
(`IronAssemblyFusionPunchSwapAction`, `ThorimUnbalancingStrikeSwapAction`,
`AlgalonPhasePunchSwapAction`).

Scope: Ulduar, Obsidian Sanctum and Eye of Eternity, **veto slice only** — no new boss-scoped
redirect actions, no refactor of the working Naxx code. All paths are relative to
`modules/mod-playerbots/`.

## Rubric

Inherited from `docs/raids/naxxramas/naxx-threat-redirect-findings.md`:

> The generic node already redirects to the main tank at the pull. A dedicated action only earns its
> keep when **the target is not the main tank**, or **the moment that matters is not the pull**.

The corollary matters as much: a veto only earns its keep where the main tank is *reliably* wrong.
Vetoing an encounter that is merely *imperfect* for the main tank turns hunters and rogues into dead
weight — the exact criticism the Naxx doc levels at the old blanket veto.

### Two supporting facts

- **No pull-window helper exists outside Naxx and RS.** Naxx expresses pull windows with
  `JustStartCombat()` / `PullWindowMs = 10000` backed by a per-boss combat-start timestamp
  (`src/Ai/Raid/Naxx/NaxxBossHelper.h:2155`). A grep for `JustStartCombat|_combat_start_ms|encounterStart`
  over `src/Ai/Raid/` finds a combat clock only in `RS/` and `SWP/`. Ulduar, OS and EoE have none. If
  dedicated actions follow later, use BT's stateless `boss->GetHealthPct() > 95.0f` idiom
  (`src/Ai/Raid/BT/BTTriggers.cpp:28`) rather than building a clock — it needs no new state and
  doubles as the fresh-spawn / fresh-phase test.
- **`GetFirstAliveUnitByEntry` beats `"find target"` for multi-tank detection.**
  `AI_VALUE2(Unit*, "find target", name)` only resolves creatures that already have *this* bot on
  their threat list — precisely wrong on a one-tank-per-boss encounter, where a bot parked on boss A
  never sees boss B. This is the documented Four Horsemen failure mode. Use
  `GetFirstAliveUnitByEntry(botAI, entry)` (`src/Ai/Raid/RaidBossHelpers.h:29`); Ulduar's entries are
  enumerated in `UlduarIDs` (`src/Ai/Raid/Uld/Util/UldBossHelper.h:25-180`), with the boss entries
  themselves coming from core `ulduar.h` via `UldScripts.h`.

## Per-boss verdict

### Ulduar — veto (the generic main-tank redirect is *actively wrong*)

| Boss | Why the main tank is the wrong sink | State gated on |
|---|---|---|
| **Iron Assembly** (Steelbreaker / Runemaster Molgeim / Stormcaller Brundir) | Three bosses, one tank each; Fusion Punch then forces a swap. The main tank is wrong for two of the three targets for the whole fight. | boss entries from core `ulduar.h`; `IronAssemblyFusionPunchSwapTrigger` resolves the *active* tank (`src/Ai/Raid/Uld/Trigger/UldTriggers_IronAssembly.cpp:102-115`) |
| **Mimiron** | Four phases, each a different creature with a fresh threat table; phase 3 has two tankable units at once (VX-001 + Aerial Command Unit) split main tank / assist tank 0. | `NPC_LEVIATHAN_MKII` 33432, `NPC_VX001` 33651, `NPC_AERIAL_COMMAND_UNIT` 33670 (`UldBossHelper.h:97-99`); MT/AT0 split at `src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp:245` |
| **Thorim** | The raid splits into arena and gauntlet squads with a tank each, so half the DPS is nowhere near the main tank; Unbalancing Strike then forces a swap on the boss. | `IsAssistTankOfIndex(bot, 0)` branches at `src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp:97,139`; `ThorimUnbalancingStrikeSwapTrigger` |
| **Algalon** | Phase Punch forces a main tank ↔ assist tank 0 swap on a stack timer; a redirect at the main tank pins Algalon on him mid-swap. | `AlgalonPhasePunchSwapTrigger` computes the active tank and its swap partner (`src/Ai/Raid/Uld/Trigger/UldTriggers_Algalon.cpp:89-103`) |
| **Razorscale — airborne only** | While she is airborne the main tank holds nothing; the Dark Rune adds belong to the assist tanks. The ground phases are single-tank and the generic node is *correct* there, so this is phase-gated, not blanket. | boss Z against `RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD` (440.0f) |

The phase-gated Razorscale entry has direct precedent: `NaxxThreatRedirectMultiplier` gates on
Heigan's `IsFastDance()` so the charges are held for the arena return rather than burned on his ledge
(`src/Ai/Raid/Naxx/NaxxMultipliers.cpp:561-600`).

**XT-002** was already handled before this work: `XT002TargetGuardMultiplier`
(`src/Ai/Raid/Uld/UldMultipliers.cpp:82`) zeroes both generic redirects for the whole encounter,
because the main tank is the wrong sink while a Pummeller is out and `xt002 redirect threat action`
picks the tank that actually needs it. Left as it is.

### Ulduar — do *not* veto; candidates for a dedicated action later

The boss itself is main-tank-held for the whole fight, so the generic node is right for the primary
target. The gain would be redirecting the *adds* at the tank who already picks them up — real, but a
veto here costs more than it saves.

| Boss | Add-tank value |
|---|---|
| **Auriaya** | Sanctum Sentry pack at the pull, and the Feral Defender resurrects up to nine times — each respawn is a fresh threat table. MT/AT0 already used (`src/Ai/Raid/Uld/Trigger/UldTriggers_Auriaya.cpp:39`). |
| **Freya** | Three Elders alive together in hard mode plus repeating add waves — Snaplasher, Storm Lasher, Detonating Lasher, Ancient Water Spirit, Ancient Conservator (`UldBossHelper.h:57-63`) — on assist tanks (`src/Ai/Raid/Uld/Action/UldActions_Freya.cpp:134`). |
| **Kologarn** | Right Arm (`NPC_RIGHT_ARM` 32934) is a separate unit and Rubble (`NPC_RUBBLE` 33768) goes to AT0 (`src/Ai/Raid/Uld/Action/UldActions_Kologarn.cpp:102,139,243`); the body never resets threat. |
| **Yogg-Saron** | Crusher Tentacles want a tank in phase 1 and the phase-3 body is effectively a fresh pull, but the only phase state is `YoggSaronPhase3PositioningTrigger` — thinner than the other candidates. |
| **Ignis** | Single tank on the boss; the Iron Constructs are the add-tank case. |

### Ulduar — no

| Boss | Why |
|---|---|
| **General Vezax** | One tank, no swap, no reset. The generic node is exactly right and wants to fire all fight. |
| **Hodir** | No meaningful tanking; nothing to redirect. |
| **Flame Leviathan** | Vehicle combat — no threat table, and neither spell is castable. |

### Obsidian Sanctum — narrow veto, low value

The off-tank grabs **every** landed drake and parks them at `SARTHARION_OFFTANK_POSITION`, far from
the raid stack, so neither the drakes' frontal Shadow Breath nor boss-centred AoE crosses the raid
(`src/Ai/Raid/OS/OSActions.cpp:59-101`). The main tank holds Sartharion himself at
`SARTHARION_MAINTANK_POSITION`. So while the raid is burning a to-kill drake, a redirect at the main
tank aims threat at a player standing on the boss — the opposite of what the off-tank logic exists to
achieve.

It is **not a wipe risk**: `ObsidianSanctumHelpers::ForceThreat` (`src/Ai/Raid/OS/OSShared.h:133-141`)
does `AddThreat(bot, 1000000.0f, …)` followed by `FixateTarget(bot)`, and fixate overrides threat
outright, so the redirect cannot actually move a drake. The real cost is a **wasted cooldown** —
three Misdirection charges or a 6s Tricks window burned on a threat table that cannot matter — plus
main-tank threat pollution that surfaces only if the fixate lapses.

Verdict: veto **only while the bot's current target is one of the three drakes**, so the cooldown is
saved for Sartharion, where the main tank *is* the right sink.

### Eye of Eternity — nothing to do

Recorded explicitly so it does not get re-litigated:

- **Phase 1** — single tank on Malygos, no swap, no threat reset. The generic node is correct and
  wants to fire for the whole phase. Power Sparks are handled by DK Death Grip and focused damage,
  not tanking (`src/Ai/Raid/EoE/EoEStrategy.cpp:18-22`).
- **Phase 2** — Malygos is airborne and `UNIT_FLAG_NON_ATTACKABLE`; the raid kills Nexus Lords and
  Scions of Eternity rather than parking them on a dedicated tank, so the main tank is as good a sink
  as any.
- **Phase 3** — drake vehicle combat (`NPC_WYRMREST_SKYTALON`); neither Misdirection nor Tricks is
  castable, so the node is inert rather than harmful.

Phase state is available via `MalygosTrigger::getPhase(bot)` (`src/Ai/Raid/EoE/EoETriggers.cpp:17-42`)
if this is ever revisited.

## What was implemented

**`UldThreatRedirectMultiplier`** (`src/Ai/Raid/Uld/UldMultipliers.h`, body in `UldMultipliers.cpp`,
registered in `RaidUlduarStrategy::InitMultipliers`, `src/Ai/Raid/Uld/UldStrategy.cpp`). Returns
`0.0f` when any Iron Assembly member, any Mimiron phase unit, Thorim or Algalon is alive and within
perception, and when Razorscale is above the flying Z threshold. Otherwise `1.0f`.

Two details that are load-bearing:

- It `dynamic_cast`s to `CastMisdirectionOnMainTankAction` and `CastTricksOfTheTradeOnMainTankAction`
  **only** — never to their shared `BuffOnMainTankAction` base, which would also kill paladin Beacon
  of Light, shaman Earth Shield and druid Thorns/Lifebloom on the tank. That is a bug the Naxx work
  had to undo in `ThaddiusGenericMultiplier`.
- Razorscale's phase is read straight off the unit's Z rather than through
  `RazorscaleBossHelper::IsFlyingPhase()`, because the helper needs `UpdateBossAI()` first and that
  reassigns the group's main tank as a side effect. `UlduarBurstWindowMultiplier` already reads her
  the same way for the same reason. The temporary harpoon knockdowns count as grounded here, which is
  intended: she is tankable then, and a redirect during the knockdown is what puts her back on the
  main tank when she gets up.

**Obsidian Sanctum** — the drake check went into the existing `SartharionMultiplier::GetValue`
(`src/Ai/Raid/OS/OSMultipliers.cpp`) rather than a new class: OS is a single-encounter raid and that
multiplier is already registered and boss-gated (`src/Ai/Raid/OS/OSStrategy.cpp:39`). Same narrow
two-`dynamic_cast` guard, then `0.0f` when `AI_VALUE(Unit*, "current target")` satisfies
`ObsidianSanctumHelpers::IsDrakeEntry`.

**Eye of Eternity** — no code change.

No build-file changes: the module has no `CMakeLists.txt`, so AzerothCore's module macro globs
`src/**/*.cpp`. Heroic and 25-man need no special handling — creature entries are identical across
difficulties, and these vetoes read entries and tank roles, not difficulty-specific spell ids.

**Not verified by a build.** The module cannot be compiled in the environment this was written in.

## Notes for whoever adds the dedicated actions

`NaxxRedirectThreatAction` (`src/Ai/Raid/Naxx/Action/NaxxActions.h:28-40`, body
`NaxxActions_Shared.cpp:12-108`) is the only redirect base that covers **hunters and rogues** — every
BT/SSC/TK/SWP/Hyjal/ZA action is hunter-only. Subclasses implement just `GetRedirectTank()` and
`GetThreatDumpTarget()`; the base also provides `GetTankHolding(Unit*)` and `GetRedirecterIndex()`.

Per the user's decision it stays where it is: any future Ulduar action gets its **own copy** rather
than promoting it to a shared raid-level base. The known cost, for the record: the `35079`
Misdirection proc-aura id is already duplicated across nine per-raid helper headers
(`NaxxSpellIds.h:112`, `ZAHelpers.h:40`, `GruulHelpers.h:24`, `BTHelpers.h:86`, `HyjalHelpers.h:42`,
`MagHelpers.h:31`, `TKHelpers.h:48`, `SSCHelpers.h:64`, `SWPData.h:69`) and Ulduar would make ten.

Any new boss-scoped action needs the matching veto entry removed or narrowed, or the multiplier will
suppress the generic node while the new action covers the same window.

## Verification

There is no unit-test harness for this module and it cannot be compiled headless in the environment
this was written in — verification is a build plus in-game observation.

1. **Build** with `mod-playerbots` enabled; confirm no new warnings in the four touched files.
   Multipliers are `dynamic_cast`-based, so unlike creator-name strings a mistake surfaces at compile
   time.
2. **Ulduar (map 603), veto positive cases** — raid with a bot main tank, a bot assist tank, a hunter
   bot and a rogue bot. On Iron Assembly, Mimiron, Thorim and Algalon expect **zero** Misdirection /
   Tricks of the Trade casts for the whole encounter.
3. **Razorscale phase gate** — no redirect casts while she is airborne; redirects resume the moment
   she is grounded. This is the one entry that can regress silently into a blanket veto.
4. **Ulduar control cases** — Vezax and Kologarn must still show the generic redirect firing, proving
   the veto is boss-scoped rather than a blanket kill.
5. **Non-redirect buffs on the tank still land** — with a paladin, shaman or druid bot in the raid,
   confirm Beacon of Light / Earth Shield / Thorns are still cast on the tank during the vetoed
   Ulduar encounters. This is the specific regression the narrow `dynamic_cast` prevents.
6. **Obsidian Sanctum (map 615)** — with drakes up, no redirect casts while a bot's current target is
   a drake; redirects fire normally once the raid switches to Sartharion.
7. **Eye of Eternity (map 616)** — unchanged behaviour in all three phases (control for "no code
   change").

## Related documents

- `docs/raids/naxxramas/naxx-threat-redirect-findings.md` — the rubric and the shared action base.
- `docs/general/threat-redirect-trash-and-naxx-plan.md` — the trash-pack proactive redirect.

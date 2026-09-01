# Raid strategies — shared conventions

Per-raid encounter knowledge lives in the sibling files. This holds what applies across all of them.
Engine mechanics are in [../engine/action-selection.md](../engine/action-selection.md); the trap
catalogue every boss strategy keeps rediscovering is [../engine/pitfalls.md](../engine/pitfalls.md);
vehicles, movement stability, reach and per-raid cost are in
[../engine/raid-mechanics-lessons.md](../engine/raid-mechanics-lessons.md).

## File layout and wiring

A raid is a folder under `src/Ai/Raid/<Raid>/` mirroring `src/Ai/Raid/Gruul/`, the reference
implementation: `<X>Strategy`, `<X>Triggers` + `<X>TriggerContext.h`, `<X>Actions` +
`<X>ActionContext.h`, `<X>Multipliers`, `<X>Helpers`. Large raids split per boss under `Action/` and
`Trigger/` and keep a thin umbrella header (`UldActions.h`) that includes the parts, so contexts and
registration maps need no edits.

Four registration sites: `RaidStrategyContext.h` (key → strategy), `BuildSharedTriggerContexts.cpp`,
`BuildSharedActionContexts.cpp`, and `PlayerbotAI::ApplyInstanceStrategies` — both the
`allInstanceStrategies` list *and* the `case <mapId>:` arm. There is no `CMakeLists.txt`; new `.cpp`
files are globbed automatically.

Naming: strategy keys are bare lowercase (`"blacktemple"`); triggers and actions are lowercase,
space-separated and boss-prefixed; multipliers are `{BossName}{Purpose}Multiplier`.

**One live bug in this area**: `"rs"` is missing from `GetInstanceStrategies()` while map 724 still
maps to it, so the Ruby Sanctum strategy gets applied but never removed on a zone change.

## Priority conventions

| Band | Use |
|---|---|
| `ACTION_RAID` (60) | role assignments, target selection, positioning |
| `ACTION_RAID + N` | stagger within a boss — flat priorities leave ordering to vector insertion order |
| `ACTION_EMERGENCY + 6` (96) with `MOVEMENT_FORCED` | emergency hazard dodge (ZA precedent) |

Class dispel nodes sit **below** `ACTION_RAID` (mage 40, druid 57), so any raid positioning node
outranks them. A boss whose mechanic is a dispel race needs its own dispel action above 60 — Heigan's
sits at `ACTION_RAID + 5`.

## Threat redirect — the rubric

Quoted verbatim because three separate encounters re-derived it:

> The generic node already redirects to the main tank at the pull. A dedicated action only earns its
> keep when **the target is not the main tank**, or **the moment that matters is not the pull**.

And its corollary, which matters more:

> A veto only earns its keep where the main tank is *reliably* wrong. Vetoing an encounter that is
> merely *imperfect* for the MT turns hunters and rogues into dead weight.

Three timing flavours cover everything seen so far, all from the Black Temple pattern: **pull** (boss
above 95% health), **phase transition** (timer- or state-based), and **multi-tank assignment** (index
the living hunters/rogues and map each to a tank).

`RaidRedirectThreatAction` (`Raid/RaidRedirectThreat.{h,cpp}`) is the **only redirect base covering
hunters *and* rogues** — every BT/SSC/TK/SWP/Hyjal/ZA action is hunter-only. A rogue casts Tricks of
the Trade and stops there (it redirects everything for 6s); a hunter casts Misdirection then dumps
`steady shot` to spend the three charges. Subclasses implement only `GetRedirectTank()` and
`GetThreatDumpTarget()`; Naxx, OS, VoA and Hodir do.

Vetoes must `dynamic_cast` to the **concrete** actions (`CastMisdirectionOnMainTankAction`,
`CastTricksOfTheTradeOnMainTankAction`) — never the shared `BuffOnMainTankAction` base, which also
carries paladin Beacon, shaman Earth Shield and druid Thorns/Lifebloom. The Naxx work had to undo
exactly that bug.

Any new boss-scoped redirect action needs its matching veto entry removed or narrowed, or the two
fight each other.

Both presume threat sticks: **a mob that wipes its own threat table has no redirect and no
taunt answer.** Look for `DoResetThreatList` / `SelectTargetFromPlayerList` in the script first —
Freya's Detonating Lashers re-roll every 10s, so ten of them face one 30s cooldown and the answer is
geometry (a range leash), not threat. The same test bounds a taunt: allowlist the adds the encounter
**owns**, never the whole target ladder — taunting a rung borrowed for damage off another tank means
owning its positioning too.

## Movement-suppression multiplier

Eight encounters independently arrived at the same idiom: while a positioning action is active, zero
`ReachTargetAction`, `CastReachTargetSpellAction`, `CombatFormationMoveAction` and `FollowAction` so
the generic mover cannot drag the bot back.

**The failure mode this creates**: once `CombatFormationMoveAction` is disabled, the boss-specific
action is the *only* thing that can position those bots. If it silently fails — an off-navmesh
`MoveTo`, a `MoveInside` that never returns false — they stand still for the whole fight. Void Reaver
is the case study.

## Targeting-suppression multiplier

Same idiom one layer up: a boss that owns target selection must zero `DpsAssistAction` and
`TankAssistAction` **for the whole fight**, not only inside the phase that motivated it.

**The failure mode**: a `choose target` action conventionally returns `false` once the bot already
holds its pick, which `DoNextAction` treats as FAILED (see
[../engine/action-selection.md](../engine/action-selection.md)) — so every settled tick keeps
draining the queue down to `dps assist` at relevance 50. `GeneralFindTargetSmartStrategy`
(`Value/DpsTargetValue.cpp`) ranks by attack range then remaining lifetime with **no current-target
preference**, so a fresh low-health add always outranks a boss. The bot takes the add, the boss
action yanks it back next tick, and it flips forever without landing a cast — in-game it reads as
bots jiggling on the spot. Noth is the case study.

**Whole fight** also rules out the subtler gate: zeroing only while the encounter's own target lookup
returns non-null. That reopens the hole exactly where the encounter node has nothing to say — on
Freya's lasher waves generic assist took over and the off-tank towed ten detonating adds into the
raid stack. Suppressing unconditionally is safe only if the ladder ends in a **terminal fallback**
(the boss), so no role is left nodeless — Freya's main tank had none and held her only because it
pulled her.

## Boss helpers

`GenericBossHelper<BossAiType>` works only when the boss AI class is visible and exposes `events`.
It is **unusable** for Anub'rekhan, Gothik and Heigan (AI class file-local to its `.cpp`) and for any
`TaskScheduler`-driven script. Fall back to the plain-`AiObject` + timer-model shape used by
`GluthBossHelper` and `HeiganBossHelper`.

For a clock every bot in the instance must agree on, use `HeiganBossHelper::PhaseStateFor`: a static
mutex plus `unordered_map<instanceId, State>`, re-anchored on phase edges with a staleness guard.
Remember that trigger, action and multiplier each hold **separate** helper instances — anything they
must agree on needs a file-static, GUID-keyed map defined in exactly one `.cpp`.

Resolve bosses with `AI_VALUE2(Unit*, "find target", "<lowercase name>")` **only** when the bot is
reliably on that creature's threat list; otherwise use `GetFirstAliveUnitByEntry`
(`RaidBossHelpers.h:29`). A melee bot parked on Thane never resolves Zeliek. `SetInCombatWithZone()`
in a script's `JustEngagedWith` is what makes `"find target"` work from the pull on some bosses.

## Heroic and 10/25

There is no separate heroic strategy anywhere — difficulty is an instance property and one code path
serves both. What actually differs per mode:

- **Spell ids**, and the mapping is not uniform. Check `spelldifficulty_dbc` per spell: Heigan's
  25-man Decrepit Fever is 55011 while Eruption, Spell Disruption and Plague Cloud have no
  difficulty rows at all; on Kel'Thuzad only Frost Bolt has variants. Use
  `NaxxSpellIds::HasAnyAura(unit, {…})` rather than a raw id.
- **Add counts and target counts** — gate on `bot->GetRaidDifficulty()`.
- **Some mechanics exist only in 25-man** (Noth's Blink/Cripple).

Ulduar hard modes are the exception to all of this: they are a **raid choice, not the 10/25 flag**,
so the "heroic comes free" reasoning does not apply. See [ulduar/README.md](ulduar/README.md).

## Anti-fear (`RaidAntiFear.h`)

One shared component, next to `RaidBossHelpers.h`, replacing what used to be five hand-copied
per-boss implementations (BWL Nefarian, Kara Nightbane, TK Solarian, TK Kael'thas, Hyjal Archimonde).
`RaidAntiFearTrigger` / `RaidAntiFearAction` / `RaidAntiFearTotemGuardMultiplier` are abstract;
concrete subclasses supply only `FearWindowActive()`.

Behaviour: a **priest** casts Fear Ward (6346) on the group main tank, or the main healer if the tank
already has it; a **shaman** drops Tremor Totem, gated on it not already being out. Consumers are
Onyxia below 40% (Bellowing Roar 18431), Auriaya (Terrifying Screech 64386, fixed 35s cycle so the
totem stays up all fight), and Yogg-Saron P2/P3 (Malady of the Mind 63830/63881, Deafening Roar
64189).

**Two defects this design exists to avoid:**

1. **`ChangeStrategy("+tremor")` does not work.** `NoEarthTotemTrigger` picks the earth totem by
   first match in `{strength of earth, stoneskin, tremor, earthbind}`, and every shaman spec ships
   with one of the first two — so Tremor never wins. Both strategies also register a node on the same
   `"no earth totem"` trigger at priority 55, so the slot thrashes, and the strategy is never removed
   afterwards, leaking past the instance.
2. **The earth totem slot is exclusive.** Anything that drops Tremor must also suppress the competing
   earth-totem casts for as long as the fear window is open, or the shaman's own strategy re-drops
   Stoneskin on the next GCD. Hence the guard multiplier — the same shape as
   `IllidanStormrageUseEarthbindTotemMultiplier`.

Cast directly and suppress the competition; no `ChangeStrategy`, no persistent state, self-reverting
when the window closes.

**Explicitly not counterable — do not target:** Insane (63120/64464, AoE charm), Psychosis, Lunatic
Gaze and Induce Madness (sanity drains), and Chains of Kel'Thuzad (28410, mind control — no *fear*
counter applies, but a druid does Cyclone the charmed raider, see [naxxramas.md](naxxramas.md)).
Phase 1
WotLK content — Naxxramas, Obsidian Sanctum, Eye of Eternity, Vault of Archavon — has **zero** fear
mechanics; all 15 Naxx boss spell enums were checked, and Gluth's vanilla Terrifying Roar (29685)
does not exist in this build.

`SPELL_FEAR_WARD = 6346` is defined here, but still duplicated in `KaraHelpers.h`, `TKHelpers.h`,
`SSCHelpers.h` and `HyjalHelpers.h` — deliberately left alone to keep the change contained.

## Raid cheats are on by default

Stock config is `AiPlayerbot.BotCheats = "food,taxi,raid"`. A number of mechanics are "handled" only
because the raid cheat papers over them — which makes them (a) not real play and (b) a silent wipe on
any server that turns the cheat off. Treat every cheat-dependent mechanic as an open gap, not as
working behaviour.

## Recording new work

Durable encounter knowledge goes into the matching `<raid>.md` here, or into `<raid>/<boss>.md`
where the raid has outgrown one file — Ulduar has, and its per-boss files mirror
`src/Ai/Raid/Uld/Action/UldActions_<Boss>.cpp`. An in-flight plan lives in
[../plans/](../plans/) only while the work is unfinished; when it ships, fold its durable content
into the raid file and delete the plan.

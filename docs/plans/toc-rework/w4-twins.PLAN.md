# w4-twins — Twin Val'kyr

Wave 3. Rules, sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md).
Guide: `twin-valkyrs-strategy-guide-toc-25/`.

## Scope

1. **Essence on every difficulty.** `w0c-foundation` fixed the ids, which were 10N only before, so
   verify the whole path: `TwinValkyrEssenceActionBase::AcquireEssence` walks to a portal and calls
   `HandleGossipHelloOpcode`. The essence portals are friendly units; probe the choice
   (`tv.essence`).
2. **Colour matching.** Swap for Light/Dark Vortex, and for heroic Touch (Touch outranks Vortex).
3. **Twin's Pact.** Break Shield of Light/Darkness first, then interrupt the heal. Interrupt duty
   ranked by GUID, as on Vezax.
4. **Orbs** (Powering Up → Empowered). At minimum avoid wrong-colour orbs; the investigator decides
   whether bots collect the right colour.
5. **Tanks.**
   - Split Fjola and Eydis between two tanks with a redirect, and replace the blanket
     movement-veto multiplier.
   - Health is shared, so check the enrage timer per difficulty in the script.
6. **Lust row**, `tv.` probes, a `tools/botobs/bosses/val_kyr_twins.py` reader and test.
7. Verify every Twins node `w0c-foundation` made live in 25N/10H/25H.

## Owns

Stem `TwinValkyr`; `docs/raids/trial-of-the-crusader/twin-valkyr.md`; the reader and its test.

## Task list

Every mechanic, id, timer and radius these tasks use is verified in
`docs/raids/trial-of-the-crusader/twin-valkyr.md` (script, DBC CSVs, `acore_world`); read it first.
Live bugs this fixes: non-tanks and the main tank start Light and all DPS follow the skull on Fjola,
so they deal −50% (essence aura-state mod); tanks never swap for a Vortex (30-80k, lethal on 25H);
the Pact shield is ignored, so class kicks land on an interrupt-immune twin; the essence-swap
multiplier vetoes every `MovementAction`, `AttackAction` included; the assist tank never drags Eydis
in; no orb handling, no redirect, no taunt, no probes.

Scope 7 is verified statically: `HasLight/DarkEssence`, `HasLight/DarkTouch`, the Vortex reads and
`GetTwinCastingPact` remap through `GetSpellIdForDifficulty` (rows 405/388/464/441/402/390/467/466),
the portal gossip remaps through the `Spell` constructor, and `FindCurrentSpellBySpellId` walks every
current-spell slot (the Vortex is self-channelled, the Pact generic). The refine lane confirms on
25N/10H/25H traces with `--essence` and `--pact`.

**Contract** (group 1 writes it in `Util/ToCHelpers_TwinValkyr.h`, namespace
`TrialOfTheCrusaderHelpers`; group 2 calls it; group 3 reads the probe keys and node names):

```cpp
enum class TwinColour : uint8 { None, Light, Dark };
enum class TwinEssenceReason : uint8 { None, Base, Shield, Vortex, Touch };
struct TwinEssenceWant { TwinColour colour; TwinEssenceReason reason; };

Unit* GetFjola(PlayerbotAI*);            Unit* GetEydis(PlayerbotAI*);      // alive, else nullptr
TwinColour TwinColourOf(Unit* twin);     TwinColour EssenceOf(Unit* unit);
TwinColour ActiveVortexColour(PlayerbotAI*);   // cast start to channel end
Unit* GetTwinCastingPact(PlayerbotAI*);        // kept
Unit* GetShieldedTwin(PlayerbotAI*);           // holds Shield of Lights/Darkness
Player* GetTwinTank(Player* bot, Unit* twin);  bool IsTwinTank(Player* bot, Unit* twin);
TwinEssenceWant GetWantedEssence(PlayerbotAI*);
bool TwinValkyrMustSwapEssence(PlayerbotAI*, bool urgentOnly);  // urgent = Touch or Vortex
Creature* GetEssencePortal(Player* bot, TwinColour colour);
Unit* GetTwinDpsTarget(PlayerbotAI*);          char const* TwinRtiIcon(Unit* twin);  // skull | cross
char const* TwinReadyInterrupt(Player* bot, Unit* twin);
bool IsTwinPactInterrupter(PlayerbotAI*, Unit* twin);
bool TwinOrbThreatens(PlayerbotAI*, float clearance);
bool FindTwinOrbDodgeSpot(PlayerbotAI*, Position& spot);
```

Probe keys (all `RaidObs::NoteDerived` inside the helper that derives them, except the per-instance
`ObsValue`): `tv.essence` `<reason>:<colour>` (lowercase enum names), `tv.target`
`<fjola|eydis>:<pact|lone|colour>`, `tv.tank` (`fjola|eydis|both|none`, tanks only), `tv.shield`
(`ObsValue<TwinColour>`, raw 0/1/2), `tv.interrupt` `duty|standby|none` (every living bot while
a twin casts an unshielded Pact, `none` without a ready interrupt), `tv.orb` `wrong|splash|none`.

Node ladder, no ties (name → action @ relevance):

| Trigger | Action | Relevance |
|---|---|---|
| `twin valkyr pact interrupt duty` | `twin valkyr interrupt pact` | `ACTION_EMERGENCY + 7` |
| `twin valkyr touched requires essence` | `twin valkyr swap essence for touch` | `ACTION_EMERGENCY + 6` |
| `twin valkyr vortex requires essence` | `twin valkyr swap essence for vortex` | `ACTION_EMERGENCY + 5` |
| `twin valkyr orb incoming` | `twin valkyr dodge orb` | `ACTION_EMERGENCY + 4` |
| `twin valkyr shield requires essence` | `twin valkyr swap essence for shield` | `ACTION_RAID + 5` |
| `twin valkyr needs base essence` | `twin valkyr take base essence` | `ACTION_RAID + 4` |
| `twin valkyr engaged by main tank` | `twin valkyr main tank hold light twin` | `ACTION_RAID + 3` |
| `twin valkyr darkbane needs assist tank` | `twin valkyr assist tank hold dark twin` | `ACTION_RAID + 2` |
| `twin valkyr redirect threat` | `twin valkyr redirect threat` | `ACTION_RAID + 1` |
| `twin valkyr dps target` | `twin valkyr focus twin` | `ACTION_RAID` |

`needs initial essence`/`acquire initial essence` and `pact interruptible` are renamed or replaced
as above; nothing else keeps the old names.

### Group 1 — helpers and multipliers

Files: `src/Ai/Raid/ToC/Util/ToCHelpers_TwinValkyr.h`, `src/Ai/Raid/ToC/Util/ToCHelpers_TwinValkyr.cpp`,
`src/Ai/Raid/ToC/Multiplier/ToCMultipliers_TwinValkyr.h`,
`src/Ai/Raid/ToC/Multiplier/ToCMultipliers_TwinValkyr.cpp`.

1. **Constants** in the helper header (boss ids stay out of `ToCData.h`): `SPELL_LIGHT_SHIELD` 65858,
   `SPELL_DARK_SHIELD` 65874 (10N ids, remapped at each call); `NPC_CONCENTRATED_LIGHT` 34630,
   `NPC_CONCENTRATED_DARK` 34628; `TWIN_ORB_TRIGGER_RADIUS` 2.75, `TWIN_ORB_BLAST_RADIUS` 6.0,
   `TWIN_ORB_DODGE_CLEARANCE` 4.0 (trigger), `TWIN_ORB_DODGE_SEARCH_CLEARANCE` 5.0 (action, past the
   trigger's), `TWIN_ORB_DODGE_SEARCH_RADIUS` 12.0, `TWIN_ORB_HORIZON` 7.0 (one second of flight),
   `TWIN_ORB_SPLASH_ALLY_RADIUS` 7.0; the contract types.
2. **Instance scan.** A `RaidInstanceState<TwinValkyrState>` refreshed once per instance per
   `getMSTime()` ms (the `ToCEncounterGate` memo shape): Fjola and Eydis from
   `map->GetCreature(instance->GetGuidData(TOC_DATA_FJOLA|TOC_DATA_EYDIS))`, alive; the active Vortex
   (each twin's own Vortex id, remapped against her); the Pact twin (each twin's own Pact id); the
   shielded twin (her own shield id, remapped); `RaidObs::ObsValue<TwinColour> shield{"tv.shield"}`;
   and the orbs, one `GetCreatureListWithEntryInGrid({34630, 34628}, 100)` per refresh storing guid,
   colour, position and `GetMotionMaster()->GetDestination` (else the position). Cache guids and
   positions, never pointers. Contract readers `GetFjola`, `GetEydis`, `ActiveVortexColour`,
   `GetTwinCastingPact`, `GetShieldedTwin` answer from it. Delete `TwinValkyrEncounterActive` and
   `TwinValkyrLight/DarkVortexActive`; keep `HasLight/DarkEssence`, `HasAnyEssence`,
   `HasLight/DarkTouch`.
3. **Tanks.** Alive tanks ranked as `GetGroupMainTank` then `GetGroupAssistTank` do (main-tank
   flag, else the first tank; assists with assistants first), resolved once per ms in the state
   and admitting `IsTank(m) || IsTank(m, true)`. Fjola goes to the first, Eydis to the second, or
   to the first when only one lives. `IsTwinTank` probes `tv.tank` for a bot that is a tank
   (`IsTank(bot) || IsTank(bot, true)`).
4. **Colour rule**, `GetWantedEssence`, probing `tv.essence`. First match:
   1. a Touch on the bot: the Touch's colour, `Touch`;
   2. an active Vortex: its colour, `Vortex`;
   3. a non-tank, non-healer bot whose essence is the colour of a shielded twin casting her Pact:
      the other colour, `Shield`;
   4. `Base`: a tank takes the colour opposite its twin (Fjola's tank Dark, Eydis's Light, a tank
      holding both Dark); anyone else keeps the essence held, Dark when none.

   `None` when neither twin lives. `TwinValkyrMustSwapEssence` is `wanted.colour != EssenceOf(bot)`,
   restricted to `Touch`/`Vortex` when `urgentOnly`. `GetEssencePortal` is the nearest portal of the
   colour (Light 34568, Dark 34567), 2D, from the per-ms sweep that also takes the orbs.
5. **DPS target**, `GetTwinDpsTarget`, probing `tv.target`: the Pact twin while one casts; the
   living twin if one is missing; Fjola under a lone tank (`lone`); else Eydis for a Light bot,
   Fjola for Dark or none.
6. **Interrupts.** `TwinReadyInterrupt`: first of kick, pummel, shield bash, mind freeze (each
   `IsWithinMeleeRange`), counterspell (`IsWithinCombatRange` 30), wind shear (25), spell lock
   (a living pet, the bot within 30), off the bot's cooldown, affordable, and passing
   `botAI->CanCastSpell`; null for a bot with no `PlayerbotAI`. `IsTwinPactInterrupter`: the twin
   casts her Pact with no shield, this bot has a ready interrupt and is not
   `MustSwap(urgentOnly)`, and no alive group member on map 649 with a lower GUID meets the same
   test. Probe `tv.interrupt` for every bot in that window, `none` without a ready interrupt.
7. **Orbs.** A bot's hazards are points every yard along each orb's next `TWIN_ORB_HORIZON` of path
   (position toward destination) for: orbs not of its essence colour (`wrong`; with no essence,
   every orb), and orbs of its colour while a group member not holding that colour stands within
   `TWIN_ORB_SPLASH_ALLY_RADIUS` of it (`splash`). `TwinOrbThreatens` tests 2D distance to any
   hazard point against the clearance. `FindTwinOrbDodgeSpot` runs
   `FindNearestPositionClearOfHazards(bot, hazards, TWIN_ORB_DODGE_SEARCH_CLEARANCE,
   TWIN_ORB_DODGE_SEARCH_RADIUS)` and probes `tv.orb` with the rule of the nearest hazard, or
   `none` when it finds nothing.
8. **Multipliers**, each testing the action first, then `ToCEncounterIsLive(botAI, TwinValkyr)`,
   then lookups:
   - keep `TwinValkyrControlTankMovementMultiplier`, its victim test becoming `IsTwinTank`;
   - delete `TwinValkyrPrioritizeEssenceSwapMultiplier` and the `ToCActions_TwinValkyr.h` include;
   - new `TwinValkyrTauntGuardMultiplier`: `IsTauntAction` whose target (`action->GetTarget()`, else
     the current target) is a twin another living tank is assigned → 0;
   - new `TwinValkyrInterruptHoldMultiplier` (by action name): kick, pummel, shield bash,
     counterspell, wind shear, mind freeze, spell lock → 0 while `GetShieldedTwin`; silencing shot,
     strangulate, silence, hammer of justice, bash → 0 whenever the target is a twin
     (immunity set -286);
   - new `TwinValkyrRedirectGuardMultiplier`: `CastMisdirectionOnMainTankAction`,
     `CastTricksOfTheTradeOnMainTankAction`, `CastTricksOfTheTradeAction` → 0 (the lane's own
     redirect casts directly, outside the queue).
9. **Burst row** `ToCTwinValkyrBurstWindow`: `{true, GetShieldedTwin(botAI) != nullptr}`.

### Group 2 — triggers and actions

Files: `src/Ai/Raid/ToC/Trigger/ToCTriggers_TwinValkyr.h`,
`src/Ai/Raid/ToC/Trigger/ToCTriggers_TwinValkyr.cpp`, `src/Ai/Raid/ToC/Action/ToCActions_TwinValkyr.h`,
`src/Ai/Raid/ToC/Action/ToCActions_TwinValkyr.cpp`.

pblint reads a creator only after a one-line `class X : public NamedObjectContext<…>` in the same
header, and a trigger's `: Trigger(botAI, "name")` on one line. Every trigger name keeps the
`twin valkyr` prefix, so `ToCGatedTrigger` wraps it (open from stage 6, pre-pull included).

10. **Nodes and contexts** per the ladder above, pushed in `AddToCTwinValkyrTriggerNodes` in ladder
    order; creators in `ToCTwinValkyrTriggerContext` and `ToCTwinValkyrActionContext`.
11. **Tank holds.** `engaged by main tank`: `IsTwinTank(bot, fjola)`, Fjola attackable (no
    `UNIT_FLAG_NON_ATTACKABLE`). `darkbane needs assist tank`: tank of Eydis and not of Fjola.
    Both actions derive `ToCMainTankHoldAction`: mark the twin's icon (`MarkTargetWithSkull` Fjola,
    `MarkTargetWithCross` Eydis), `SetRtiTarget` to it, `Attack` it when it is not the current
    target, `CastClassTaunt` any assigned twin whose victim is not this bot (so a lone tank holds
    both), then `DragBossToAnchor(twin, ARENA_CENTER)`.
12. **Essence walks.** Each essence trigger fires when `GetWantedEssence` names its reason and the
    colour differs from `EssenceOf(bot)`; `needs base essence` also covers a bot with none. The four
    actions share `TwinValkyrEssenceActionBase::AcquireEssence(TwinColour)`, taking the colour from
    `GetWantedEssence`, never re-deriving it. Walk to `GetEssencePortal` at `MOVEMENT_FORCED`;
    for Touch, Vortex and Shield first `bot->InterruptNonMeleeSpells(true)` when
    `IsMovementPreventedByCasting()`; clear `last movement` once the bot has stood still 500 ms short
    of range (a stopped walk is otherwise refused as a duplicate for 5 s). The in-range gossip stays
    as is.
13. **Interrupt.** `pact interrupt duty`: `IsTwinPactInterrupter(botAI, GetTwinCastingPact(botAI))`.
    The action casts `TwinReadyInterrupt` on that twin.
14. **Orb dodge.** Trigger: not `TwinValkyrMustSwapEssence(botAI, true)` and
    `TwinOrbThreatens(botAI, TWIN_ORB_DODGE_CLEARANCE)`. Action: latch the spot as a member; keep it
    while `FindTwinOrbDodgeSpot`'s hazards still clear it, else re-find; `MoveTo` at
    `MOVEMENT_FORCED` and return true while walking; `InterruptNonMeleeSpells(true)` only when a
    hazard point lies within `TWIN_ORB_TRIGGER_RADIUS + 0.5` of the bot and the cast pins it.
15. **DPS target.** Trigger: not a tank, not a healer, the encounter live, and `GetTwinDpsTarget`
    differs from the bot's `rti target` or current target. Action: mark the twin's icon if unset,
    `SetRtiTarget(botAI, TwinRtiIcon(twin), twin)`, `Attack(twin)` when it is not the current target,
    else return false.
16. **Redirect.** Trigger: hunter or rogue, the encounter live, `GetTwinTank(bot,
    GetTwinDpsTarget(botAI))` alive and not the bot. `TwinValkyrRedirectThreatAction :
    RaidRedirectThreatAction`: `GetRedirectTank` that tank, `GetThreatDumpTarget` that twin.
17. Delete the old skull-for-everyone comments and the `pact interruptible` `Attack` action.

### Group 3 — reader, test and boss doc

Files: `tools/botobs/bosses/val_kyr_twins.py`, `tools/botobs/tests/test_val_kyr_twins.py`,
`docs/raids/trial-of-the-crusader/twin-valkyr.md`.

18. **Reader** `val_kyr_twins.py`, modelled on `general_vezax.py` (`run_sections`, validity banner,
    `silent_keys` for declared probes never written). Ids per difficulty from the boss doc and the
    README table; interrupts, taunts and lust matched by the `spell` record's name.
    - `--essence`: per bot, combat time with no essence, swaps per `tv.essence` reason; per Vortex
      (twin `cast` rows), who took unabsorbed Vortex damage (`dmg` 66048/67203-67205,
      66059/67155-67157 with `a` > 0), by role.
    - `--touch`: each Touch aura on a player, apply to removal, raid damage from Touch rows meanwhile.
    - `--pact`: each Pact cast with `tv.shield` up/down, time to break, interrupts that landed after
      it, outcome (the twin's `castingSpell` column ending before 15 s, or her health jumping),
      `tv.interrupt` duty, lust casts in the window.
    - `--orbs`: Unleashed hits taken versus absorbed per bot and role, `tv.orb` rule counts, peak
      Powering Up stacks, Empowered applications.
    - `--tanks`: per twin, share of combat time her victim was her `tv.tank` tank, taunts, median
      distance from `ARENA_CENTER` (`anchor("ARENA_CENTER")`) and between the twins.
    - `--targets`: per DPS bot, share of time on the twin the rule names (other colour than its
      essence aura, the Pact twin during a Pact), from `snap.u[7]`.
19. **Test** `test_val_kyr_twins.py`: a synthetic pull pinning each section's arithmetic, and the
    shared `fixtures/full-v12.ndjson` reading empty in every section without raising.
20. **Boss doc** (fold with `compact-docs-writer`): a strategy section with each decision below and
    its rationale, the ladder, the known gaps; "What a trace answers" with the six keys and the
    reader's sections, as `docs/raids/ulduar/mimiron.md` does.

### Integrate

Compile the three groups together. No registration site, no shared file and no new `.cpp` changes,
so no CMake re-run. Checks per the overview.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| DPS hit the twin of the other colour than their essence (Light → Eydis, Dark → Fjola), the Pact twin overriding; per-bot RTI on skull (Fjola) or cross (Eydis) | DBC 65811/65827: +50% against aura state 19 (Eydis) or 22 (Fjola), −50% against the other; guide: "one tank grabs Light Essence and tanks Eydis" | skull on Fjola for everyone, today's half damage while Light |
| Start colours: raid Dark, Fjola's tank Dark, Eydis's tank Light | guide, "The Pull" and "Tanks" | today's Light for non-tanks and the main tank |
| Everyone matches a Vortex, tanks and healers included; tanks then walk back to the colour opposite their twin | script/DBC: map-wide, 30-80k in 5 s; the tank that must swap is usually on the casting twin, who stands still; the restore keeps the +50% threat | guide: tanks stay and use cooldowns, lethal on 25H |
| Touch outranks Vortex | brief; in this core Touch hits the whole raid until the touched player swaps | Vortex first, the raid eats up to 6 more Touch ticks |
| Pact: every DPS on the shielded twin; DPS holding her colour swap while the shield holds | DBC ±50% and 175k/700k/300k/1.2M shields; guide offers "switch to Light" when DPS is short; a swap costs ~2.5 s each way (gossip reach ~18 yd from centre) | no swap, lust alone |
| Interrupt duty: lowest GUID among bots with a ready, in-range working interrupt that are not swapping urgently; class kicks held while shielded; silences and stuns zeroed on the twins | brief (Vezax); shield `MECHANIC_IMMUNITY` 26; immunity set -286 | the class interrupt nodes alone |
| Lust held until a twin raises her shield | guide: "worth it to use Bloodlust during this period" | lust at the pull |
| Orbs: dodge ones of another colour, and one's own colour with an other-colour ally within 7 yd; 4 yd trigger clearance, 5 yd search, 1 s of path; no stack, no soakers, no active collection | script: 2.75 yd trigger, 6 yd blast, 7-8 yd/s, 24/36 per big wave; pitfalls (a wide warning band dodges permanently: 6 yd over a longer horizon covers most of the 47 yd disk on a heroic wave); Empowered comes mostly from matching Vortex ticks | guide: tight stack plus 3-5 soakers catching orbs |
| Main tank on Fjola, first assist on Eydis, a lone tank holds both; both dragged to `ARENA_CENTER`; taunt back, taunt guard; hunters and rogues redirect to the tank of the twin their colour sends them to | brief; README rubric (target not the main tank); guide ("possible to one tank"); both twins centred keep melee target swaps short and every portal equidistant | main tank takes both |
| Essence walks at `MOVEMENT_FORCED`, clipping the bot's cast for Touch, Vortex and Shield; the blanket essence-swap multiplier deleted | pitfalls (blanket `MovementAction` veto kills `AttackAction`; a cast pins the feet; forced beats a same-priority lock) | keep the veto |
| Twins resolved through `GetGuidData`, one scan per instance per ms, orbs included | gate precedent; pitfalls (`GetFirstAliveUnitByEntry` deprecated and sight-capped; per-bot sweeps) | per-bot entry lookups |
| No new coordinate; `ARENA_CENTER` and the portals only | navprobe reads the arena floor (gameobject 195527) off mesh with no height, so no point there can be checked offline | derive anchors off the centre |
| Tank Vortex damage and Twin Spike get no defensive node | guide: tank damage is light; Power of the Twins is the peak | a Vezax-style defensive on Power of the Twins |
| Shield rung also sends a DPS bot with no essence straight to the other colour | otherwise two ~2.5 s walks: Dark by Base, then Light once Eydis is shielded | the task-4 rule literally |
| Orb scan runs from a living twin, 100 yd | one list serves every bot for that ms; a bot at the gate (77 yd from centre) would miss the far side of the 47 yd circle | scan from the bot |
| Spent orbs (display 11686) skipped | `spell_valkyr_ball_periodic_dummy_aura` swaps the model on explosion and despawns 1.5 s later; Unleashed is instant | dodge a harmless orb for 1.5 s |
| Spell lock also needs `!bot->HasSpellCooldown` and the bot within 30 yd | `PlayerbotAI::CanCastSpell` passes any known pet spell before its cooldown and range checks, and `PlayerbotAI::CastSpell` casts Spell Lock as the bot, so the cooldown lands on the warlock and range is measured from it | the pet's cooldown and range, which never read spent |
| Interrupt-duty peers filtered by `IsInMap(bot)` | same instance map, so same update thread | map id 649 alone |
| Interrupt hold matches `<spell>` and `<spell> on <x>` | "kick on enemy healer" and "hammer of justice on snare target" cast the same spell on a twin | exact names only |
| Taunt guard and silence hold take the action's target when it is a twin, else the current target | challenging shout/roar target the bot, righteous defense an ally, yet all hit the bot's target | `GetTarget()` alone |
| Arcane torrent not held | only `RacialsStrategy`'s low mana/energy and boost nodes cast it, for its resource; on an immune twin the silence is harmless | zero it on the twins, losing a resource cooldown all fight |
| Interrupt duty needs the kick's power cost met | `CanCastSpell` tests with `TRIGGERED_IGNORE_POWER_AND_REAGENT_COST`, so an empty rogue or a DK under 20 RP held duty and failed every cast | `CanCastSpell` alone |
| A bot in an unshielded Pact window with no ready interrupt writes `tv.interrupt` `none`; the reader cuts a bot's spans at its death | the key is change-only, so a `duty` from an earlier Pact read as current | leave the latch and filter in the reader only |
| Under a lone tank every DPS bot stays on Fjola outside a Pact (`tv.target` `lone`); the reader reads a `tv.tank` of `both` as that window | the twins obey taunt diminishing returns (`flags_extra` 0x80000, all eight entries), so a Dark lone tank taunting Eydis back from Light DPS goes immune by the 5th taunt | keep the colour rule and lose Eydis to the DPS |
| Tank list resolved once per ms, admitting `IsTank || IsTank(bySpec)`; the DPS trigger uses the same predicate | pitfalls (`IsTank` flickers); a one-tick drop made the main tank `both`, taunting Eydis off her tank | `GetGroupMainTank`/`GetGroupAssistTank` per call |
| Portals taken in the orb sweep, nearest by 2D | per-bot 200 yd grid sweeps on every essence-walk tick; the portals are four fixed summons of the twins | `GetNearestCreatureByEntry` per bot |
| An essence walk refused as a duplicate holds the tick while the bot moves | a lower node's feral charge or Charge leaps the bot back onto the twin; the forced lock only stops `MovementAction`s | yield the tick |
| `--orbs` prints the bots per `tv.orb` rule, not row counts | the key is change-only, so rows count rule switches; `move` rows count dodges | row counts |
| `TwinTankAssignment` dropped from the contract | nothing called it; `IsTwinTank` writes `tv.tank` | keep it |
| Eydis's tank trigger also needs Eydis attackable | the script releases both twins together; matches Fjola's trigger | tank of Eydis alone |
| Orb spot kept while 4 yd clear (the trigger's clearance, searched at 5) and the dodge ran within 1 s | a kept spot never re-fires the trigger, the yard is hysteresis; a later wave gets a fresh spot | re-find every tick |
| Orb dodge, Touch and Vortex walks clear a forced booking headed elsewhere; Shield and Base only a stalled one | forced does not outrank forced; the orb trigger stands down during urgent swaps | stalled bookings only |
| After a successful gossip the bot stops and clears `last movement` | gossip reaches ~14 yd from the portal; the rest of the walk would run under the forced lock | finish the walk |
| Focus action marks via `MarkTargetWithIcon`, moving an icon left on another unit | `rti target` is recalculated from the icon | mark only when the icon is unset |
| Reader: essence held before the trace opens read from the first aura row (a removal means held), else from which Surge hit the bot; Pact `healed` on a 5+ point shared-health jump within 2 s, `kicked` when the casting column drops over 1 s early; shield broken when `tv.shield` drops before the cast ends; `tv.interrupt` read only after the break; strip plus new essence within 250 ms is one swap; Empowered rows within 100 ms are one | aura rows start at session open and each Surge skips its colour's holders; creature auras and heals go unrecorded; `tv.interrupt` is change-only, so a dead bot's value carries over between Pacts | `?` for pre-trace essences; outcomes left unknown |

## Carried over

- Merge stage: README "Nodes keyed on these" Twins row (renamed nodes, `twin valkyr shield requires
  essence`, the essence-swap multiplier gone) and README constraints: the arena floor is gameobject
  195527, off mesh for navprobe, walked by straight shortcut.
- Merge stage, engine lesson for `pitfalls.md`: navprobe cannot see a gameobject floor, so an arena
  like ToC's reads off mesh with no height; the offline check is impossible there, not failed.
- Merge stage, engine lesson: a script's hand-rolled periodic can ignore mitigation entirely —
  `spell_valkyr_touch_aura` hits every player on the map (its `ExcludeTargetAuraSpell` is 0) and
  deals the pre-absorb amount.
- Merge stage: `tools/botobs/tests/test_toc_naming.py:63` samples the removed `twin valkyr pact
  interruptible`; swap in `twin valkyr pact interrupt duty`. It passes today (prefix only) and sits
  outside this lane's files.

## Known gaps

- No soakers or active orb collection: Empowered comes from matching Vortex and Touch absorption
  and orbs that happen to pass.
- A bot touched during the other colour's Vortex swaps for the Touch and eats the Vortex unless it
  can swap back in time.
- Human raid members hold no interrupt duty (no `PlayerbotAI` to ask).
- A lone tank can't truly hold both: Light bots deal half on Fjola, and an Eydis Pact still pulls
  her off.
- Deferred cleanup: each essence and orb trigger re-derives `GetWantedEssence` every tick. A
  per-bot, per-ms memo in `TwinValkyrState` must match its `tv.essence` probe and the peer loop in
  `IsTwinPactInterrupter`, which reads other bots' wants.

## Review

Open issues after re-review: none. Deferred: the optional half of conformance-6, the memo above,
because it is not a small change.

## Blocked

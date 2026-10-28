# Icecrown Citadel (map 631)

Strategy key `icc`, one strategy for every encounter. Cross-raid conventions are in
[../README.md](../README.md). Mechanics source: `src/server/scripts/Northrend/IcecrownCitadel/`. A
code comment calling the core bugged is checked against it; the boss doc gives the verdict, with issue
text when it holds. During an upstream merge ICC keeps ours; the merge report lists upstream's ICC
changes so the user can pick which to port. Tactics come from the guides in
[guide-sources.md](../../systems/guide-sources.md), converted into `build-icc-wt/_guides/`
(`text/<slug>.<tag>.md`, listed in `INDEX.md`).

## Code layout and registration

`src/Ai/Raid/ICC/` splits per encounter stem, in dispatch order: `Marrowgar`, `Deathwhisper`,
`FrostGiant` (Rotting Frost Giants), `Gunship`, `Saurfang`, `Dogs` (Stinky and Precious), `Festergut`,
`Rotface`, `Putricide`, `BloodPrinces` (council and trash), `BloodQueen`, `Svalna`, `Valithria`,
`Sindragosa` (boss and gauntlet), `LichKing`. A stem owns:

- `Action/ICCActions_<Stem>`, `Trigger/ICCTriggers_<Stem>`, `Multiplier/ICCMultipliers_<Stem>`;
- `Util/ICCHelpers_<Stem>`: its positions, constants, phase readers and other helpers;
- `Definition/ICCDefinition_<Stem>.cpp`: one `EncounterDefinition` with its rows and multipliers,
  the accessor declared in `Definition/ICCDefinitions.h`.

A file exists once it has content: `Marrowgar`, `FrostGiant` and `Svalna` have no multipliers.
Raid-wide parts:

- `Util/ICCHelpers_Shared`: helpers several encounters use (class taunt and CC, raid icons, heroic
  buffs, the cheat helpers).
- `Util/ICCData.h`: map id, the `CreatureIdsICC` and `SpellIdsICC` enums, and the Defile and
  Malleable Goo ids the spell listeners watch. A new encounter-specific id or position goes in the
  stem's helpers.
- `Util/ICCEncounterGate.h`: the boss state ids (`IccEncounterId`, mirroring the core's `DataTypes`)
  and `IccRaidWideGate`.
- `Util/ICCSeams.{h,cpp}`: the hooks table and the configs for the raid-wide helpers in
  `src/Ai/Raid/` (seams below).
- Raid-wide definitions, on `IccRaidWideGate` (open, never live), `ICC_BOSS_NONE` and no trace name:
  `RaidWide` (`Trigger/ICCTriggers_RaidWide`, `Action/ICCActions_RaidWide`,
  `Definition/ICCDefinition_RaidWide.cpp`: the opener and resistance aura rows, their holds, and the
  direct-targeting bases) and `Definition/ICCDefinition_HeroicBuffs.cpp` (the strip, [Cheats](#cheats)).
- `Util/ICCScripts.{h,cpp}`: per-instance state, the spell listeners, the state reset, the Blood
  Prince trash hook and the RaidObs name table, registered by `AddSC_IcecrownBotScripts` from
  `src/Script/Playerbots.cpp`.
- Umbrellas `Action/ICCActions.h`, `Trigger/ICCTriggers.h`, `Multiplier/ICCMultipliers.h` include
  every stem; `RS/RSMultipliers.cpp` includes `ICCActions.h`.

**Gates.** An encounter's definition carries its slug from the table below as trace name. A boss
definition takes `BossStateGate` with its `IccEncounterId` (Svalna 9): open unless that boss is done
or another is in progress, so pre-pull steps run; live while `IN_PROGRESS`. `Dogs` and `FrostGiant` (`ICC_BOSS_NONE`, 255) take `UnitGate<entries>` (`RaidEncounter.h`) on Stinky 37025 and Precious
37217, and on the giant's 38490 (10) and 38494 (25): open while one is alive on the bot's
map, live while it is in combat with a victim in the bot's group (a pet counts as its owner) or with
the bot. A per-instance guid cache, rescanned at most once a second, finds it, so one in an unloaded
grid leaves the gate closed. Their ticks open the trace through `IccMarkPull` while live. Each trigger
still tests its boss itself, mostly through `"find target"` (the bot's threat list), Sindragosa's
through a 200 yd `FindNearestCreature`, Marrowgar's by entry through a per-instance guid cache, the
giant's through its gate's live unit, the Gunship's through the instance's guid slots (2 raid ship, 40
enemy ship) and both ships' passenger sets; one firing while its gate is live names the trace
(`RaidObs::NamePull`). The strip, the heroic buffs and `IccMarkPull` run in definition ticks, which
run while the gate is open; `icc sindragosa group position` still runs its raid cheats and Nitro
Boosts inside `IsActive`.

Rows, rules, `e.Multiplier` and ticks run while open, `e.LiveMultiplier` only while live. Every
encounter multiplier is live. Festergut's and Rotface's state moves when Putricide reaches his balcony
point, about 7 s into the pull (~102 and ~104 yd at 15 yd/s). What acts before live:

| Encounter | Before live |
|---|---|
| Festergut | every row, rule and the tick from his engage, keyed on his combat; only the multiplier `icc festergut brake` waits for live |
| Rotface | every row and rule, keyed on him being in combat; the multipliers `icc rotface ooze splash` and `icc rotface self cure` wait for live |
| Gunship | for bots aboard the raid's ship: the definition tick, `icc gunship rocket pack setup` (no pack equipped, Zafod within 150 yd), `icc gunship cannon near` (a cannoneer with its pack), `icc in cannon`; `icc gunship tank defensive` is asked now but returns false until live; `icc gunship rocket jump`, every rule and the multiplier wait for live |
| Valithria | `icc valithria group` (she is within 100 yd), `icc valithria heal` (healers within 100 yd); live once the first archmage pulls the combat trigger |
| Sindragosa | `icc sindragosa group position` (she is within 200 yd), its raid cheats and Nitro Boosts included |

Nothing else: Marrowgar's nodes key on his boss state, the giant's and the dogs' on their gate being
live, any prince's engage pulls Valanar into the state in the same update, and the rest key on fight
auras, units or `"find target"`.

**Seams.** An encounter gains a trigger, action, multiplier, rule or tick in its stem files and
definition alone:

- trigger and action: classes carrying their name as `static constexpr char const* Name`, joined by
  an `e.Node<Trigger, Action>(priority)` row. Rows naming one trigger back to back share a node.
  pblint resolves a row through both `Name`s, so keep each on one line.
- multiplier: `e.LiveMultiplier<M>(families)`, `e.Multiplier<M>` only for one that acts before live;
  rules (`OwnMovement`, `OwnTargeting`, `Block`, `Exclusive`) and `Tick` likewise, rules after the
  stem's multipliers.
- direct targeting: a resolver `Unit* (PlayerbotAI*)` in the stem's helpers, `GetRaidHumanSkull` its
  first branch, every branch an in-combat unit, its pick probed there. A trigger class on
  `IccDirectTargetTrigger` and an action on `IccDirectAttackAction` pass their `Name` and the resolver;
  one row joins them, and `e.OwnTargeting(name, roles, &IccResolverPicks<&Resolver>, pickers)`
  follows the stem's multipliers. The action sends the pet first, the trigger also fires while the
  pet is off the pick. Marrowgar, Deathwhisper, the Gunship (`GsbTarget`), Saurfang (`GetDbsTarget`),
  Stinky and Precious (`GetDogsTarget`), Festergut (`GetFgTarget`) and Rotface (`GetRfTarget`) use
  it, with no icon writes or `rti` use. `RaidRestoreLegacyRti` stays unwired
  until no ICC code writes `rti`, and it never resets `star`. The Marrowgar tick resets a bot's
  `cross` or `star` `rti` to `skull` once per instance, and the Gunship tick a `star` while its gate
  is open; a bot that skips both rooms keeps what the old Marrowgar code set.
- hooks: a row in `HookRows()` (`Util/ICCSeams.cpp`), `{&Icc<Stem>Definition, burstWindow,
  openerBosses, resistSchools, fireWantsMastery}`, unused hooks null, the functions in the stem's
  helpers. Only the first row whose definition is live is asked, so none answers before live (the
  table above). `burstWindow` feeds `icc burst window` (`RaidBurstWindow.h`), which with no row lets
  every burst through. `openerBosses` feeds the opener (`RaidOpener.h`, `icc.opener`): a DPS bot holds
  its damage on a listed boss until a group tank has held it 2 s, or 5 s after it came into combat.
  `resistSchools` and `fireWantsMastery` feed the aura election (`RaidResistAura.h`, `icc.aura`): one
  bot paladin per wanted fire, frost or shadow school, the nature hunter's Aspect of the Wild, and the
  hold rules keep each holder's own auras or aspects off. With no hook the opener and aura triggers neither fire nor note.
  Rows: Marrowgar's opener (boss 36612), Deathwhisper's burst window (lust waits for phase 2),
  Saurfang's opener (boss 37813) and burst window (lust waits for Frenzy), Festergut's resist
  schools (shadow: Gaseous Blight, Blighted Spores, Pungent Blight).
- defensives: `GetRaidCover` and `GetRaidPersonalDefensive` (`RaidDefensive.h`) with
  `ICC_DEFENSIVE_SCOPE` (`icc.cover`, `icc.personal`) and a `RaidPredictedHit` the stem builds.
  Tank buttons go through `NextTankDefensive` (`RaidTankDefensive.h`) with a `Block` over
  `HeldTankDefensiveNames()` keeping the class nodes off them until a low-health valve, and
  `CallHumanTankDefensive` tells a human tank. Users:
  - Marrowgar: `icc lm tank defensive` (each Bone Slice window and under 40%, class buttons held to
    25%, `lm.defensive`, `lm.humancall`) and `icc lm storm cover` (`GetRaidCover` per 6 s of Bone
    Storm ticks, class cover nodes held over the lead);
  - Deathwhisper: `icc lady deathwhisper defensive` (Deformed Fanatic, unkicked Frostbolt, under 35%),
    plus `icc lady deathwhisper brake`, a `LiveMultiplier` holding damage dealers at 90% of the top
    tank's threat (frees at 80%);
  - Gunship: `icc gunship tank defensive` (`gsb.defensive`, `gsb.furywindow`, `gsb.humancall`);
  - Saurfang: `icc dbs tank defensive` (holder or Mark carrier at Blood Power peaks and through
    Frenzy, `dbs.humancall`) and `icc dbs mark defensive` (`GetRaidPersonalDefensive` on
    `icc.personal`);
  - Stinky and Precious: `icc dogs tank defensive` (a button in each Decimate cast, `dogs.defensive`,
    `dogs.humancall`) and `icc dogs decimate cover` (raid cover during the dog's 3 s self-stun after
    Decimate);
  - Festergut: `icc festergut tank defensive` (ladder at 2 and 3 Inhaled Blight, `fg.defensive`,
    `fg.humancall`), `icc festergut pungent cover` and `icc festergut pungent defensive` (raid cover
    and personal buttons on the Pungent Blight cast);
  - Rotface: `icc rotface tank defensive` (the kiter under a Big Ooze of 4+ stacks within 6 yd, any
    tank under 40%, class buttons held to 25%, `rf.defensive`, `rf.humancall`).
- opening positions: a `Mover` row in the definition whose trigger holds the stand while the gate is
  open and not live, its positions navprobed in the stem's helpers. No raid-wide staging.

**The root only dispatches.** `ICCStrategy.cpp` adds nodes and runs `OnTick` in
`IccEncounterDefinitions()` order, and adds multipliers in its own: Marrowgar, Deathwhisper,
FrostGiant, Saurfang, Dogs, Festergut, Rotface, Putricide, BloodPrinces, BloodQueen, Svalna,
Valithria, Sindragosa, LichKing, Gunship, RaidWide, then `icc burst window`. **Both orders are
behaviour**: equal relevances pop in insertion order, and the engine stops at the first multiplier
that zeroes an action, crediting it with the veto, while several ICC multipliers act inside
`GetValue`. `ICCActionContext.h` and `ICCTriggerContext.h` register every definition's rows. The
registration sites name only these root files: `RaidStrategyContext.h`,
`BuildSharedActionContexts.cpp`, `BuildSharedTriggerContexts.cpp`, and `PlayerbotAI.cpp` (map 631 →
`icc`, and the instance-strategy name list).

## Boss states and trace names

`instance_icecrown_citadel.cpp` calls `SetBossNumber(15)`; the ids are the core's `DATA_*` order in
`icecrown_citadel.h`. `AddSC_IcecrownBotScripts` registers each id's slug (`RaidObs::NameBossStates`).
A trace opened while that boss is `IN_PROGRESS` takes the slug outright, whether the boss state opens
it or an engage does: the core runs the engage hook after `JustEngagedWith` has queued the state. A
boss state renames a trace open under another name, except one `MarkPull` opened, which it closes
(`idle`) and replaces: a dogs trace in its 30 s idle window would otherwise absorb the Festergut
pull. Nothing renames a slug-named trace afterwards.

| Id | Encounter | `IN_PROGRESS` from (core) | Trace slug | Prefix |
|---|---|---|---|---|
| 0 | [Lord Marrowgar](lord-marrowgar.md) | his `JustEngagedWith` | `lord-marrowgar` | `lm.` |
| 1 | [Lady Deathwhisper](lady-deathwhisper.md) | her `JustEngagedWith` | `lady-deathwhisper` | `ldw.` |
| 2 | [Gunship Battle](icecrown-gunship-battle.md) | the enemy captain's `DoAction(ACTION_ENEMY_GUNSHIP_TALK)` at the battle start | `icecrown-gunship-battle` | `gsb.` |
| 3 | [Deathbringer Saurfang](deathbringer-saurfang.md) | his `JustEngagedWith` | `deathbringer-saurfang` | `dbs.` |
| 4 | [Festergut](festergut.md) | Putricide's `MovementInform` at his balcony point, seconds after the pull | `festergut` | `fg.` |
| 5 | [Rotface](rotface.md) | the same, at Rotface's point | `rotface` | `rf.` |
| 6 | [Professor Putricide](professor-putricide.md) | his `JustEngagedWith` | `professor-putricide` | `pp.` |
| 7 | [Blood Prince Council](blood-prince-council.md) | Valanar's `JustEngagedWith`; Keleseth and Taldaram pull him in | `blood-prince-council` | `bpc.` |
| 8 | [Blood-Queen Lana'thel](blood-queen-lana-thel.md) | her `JustEngagedWith` | `blood-queen-lana-thel` | `bql.` |
| 9 | [Sister Svalna](sister-svalna.md) | `BossAI::_JustEngagedWith` | `sister-svalna` | `ss.` |
| 10 | [Valithria Dreamwalker](valithria-dreamwalker.md) | her `JustEnteredCombat`, as the adds reach her | `valithria-dreamwalker` | `vd.` |
| 11 | [Sindragosa](sindragosa.md) | her `JustEngagedWith` | `sindragosa` | `sg.` |
| 12 | [The Lich King](the-lich-king.md) | his `JustEngagedWith` | `the-lich-king` | `lk.` |
| 13 | [Sindragosa gauntlet](sindragosa.md#sindragosa-gauntlet) | the gauntlet's `DoAction(ACTION_START_GAUNTLET)` | `sindragosa-gauntlet` | `sg.` |
| 14 | [Blood Prince trash](blood-prince-council.md#blood-prince-trash) | never: `NOT_STARTED`, then `DONE` once its four door guards die | `blood-prince-trash` | `bpc.` |
| — | [Stinky and Precious](stinky-and-precious.md) | no boss state | `stinky-and-precious` | `dogs.` |
| — | [Rotting Frost Giant](rotting-frost-giant.md) | no boss state | `rotting-frost-giant` | `rfg.` |

`icc.` is raid-wide, matches every ICC slug and belongs to none: a slug owning it would have its keys
read silent on every other ICC pull. An encounter with a phase publishes it as `<prefix>.phase`;
`tools/botobs/bosses/icecrown_citadel.py --phases` reads it as one raid-wide timeline, each step from
the first bot to note it. Festergut and Rotface engage before their state moves, so their traces open
on the engage, named by the DBC encounter credit, which matches the slug. The last two rows open
through `IccMarkPull` from their definitions' ticks while the gate is live, the Blood Prince trash
through a hook in `Util/ICCScripts.cpp` ([its section](blood-prince-council.md#blood-prince-trash)).
The dungeon bosses are the kill-credit creatures: Marrowgar, Deathwhisper, Saurfang, Festergut,
Rotface, Putricide, Valanar, the Blood-Queen, Sindragosa, the Lich King. Gunship and Valithria credit
by spell (72959, 72706), Svalna not at all.

## Cheats

Every cheat site runs through one of two `ICCHelpers_Shared` helpers and writes a `cheat` record
([observability.md](../../systems/observability.md)) keyed `<prefix>.<kind>`, with `tgt` the unit
acted on when it is not the bot:

- `IccRaidCheat(botAI, key, target)` returns `HasCheat(BotCheatMask::raid)` and records when true. It
  is asked last in each condition, so only a cheat that runs counts.
- `IccNoteCheat(bot, key, target)` only records. The `Playerbots.EnableICCBuffs` buffs keep that
  gate (shipped 1), act on heroic only, and reach human raiders too; the header carries it as
  `cfg.iccbuffs`.

`tools/botobs/bosses/icecrown_citadel.py --cheats` is the only view of `cheat` rows: `--bot` and
`--notes` skip them.

Class **gap**: kept until the encounter carries the guide's answer; one survives only where bots
can't carry that answer out at all, its reason in the boss doc. **Buffs**: the `EnableICCBuffs`
operator option, kept. A removed cheat leaves the table; its boss doc's Cheats section says what
replaced it.

| Key | Class | Gate | Sites |
|---|---|---|---|
| `pp.oozekill`, `pp.gascloudkill` | gap | raid | `IccPutricideVolatileOozeAction`, `IccPutricideGasCloudAction`: kill every extra add of the entry |
| `pp.unboundplague` | gap | raid | `IccPutricideAvoidMalleableGooAction::HandleUnboundPlague`: suicide with nobody near |
| `pp.freeooze` | gap | raid | `IccPutricideAbominationAction::TryRegurgitate`: Regurgitated Ooze with no energy cost; off, it casts at full cost |
| `vd.dreamteleport`, `vd.cloudteleport` | gap | raid | `IccValithriaDreamCloudAction`: onto a human in Dream State, onto the shared cloud |
| `sg.frostbeacon`, `sg.mysticbuffet`, `sg.magicbarrier`, `sg.nothreat` | gap | raid | `IccSindragosaGroupPositionTrigger`: main tank's P3 beacon strip, tanks' Mystic Buffet strip, Magic Barrier on tanks, 1% Threat on assist tanks |
| `sg.flares` | gap | raid | `IccSindragosaFrostBeaconAction::TryDropTombFlares`: smoke flares from `StoreNewItemInBestSlots` |
| `sg.tombstrip` | gap | raid | `IccSindragosaMysticBuffetAction`, `IccSindragosaFrostBombAction`: tomb aura strips |
| `lk.ragingspiritkill` | gap | raid | `IccLichKingWinterAction`: kills the fullest Raging Spirit past two |
| `lk.wickedspiritkill` | gap | raid | `HandleSpiritMarkingAndTargeting`: kills the star Wicked Spirit |
| `lk.hoj` | gap | raid | `HandleAssistTankAddManagement`: Hammer of Justice on Shamblings from any class |
| `lk.frostmourneescape` | gap | raid | `HandleTeleportationFixes`, normal: snaps a Harvest Soul victim out of Frostmourne on its first tick, skipping the Spirit Warden |
| `ldw.nothreat`, `ldw.spitefulfury` | buffs | iccbuffs | the Deathwhisper definition's tick while live |
| `pp.heroicbuff`, `pp.spitefulfury` | buffs | iccbuffs | the Putricide definition's tick while live, on every member |
| `sg.heroicbuff`, `sg.spitefulfury` | buffs | iccbuffs | the Sindragosa definition's tick while live, on every member |
| `lk.heroicbuff` | buffs | iccbuffs | `IccLichKingShadowTrapAction`, `IccLichKingWinterAction`, `IccLichKingAddsAction` (every member) |
| `lk.empoweredblood`, `lk.valkyrstun` | buffs | iccbuffs | `IccLichKingAddsAction`, `HandleValkyrAssignment` |

`*.heroicbuff` is `IccApplyHeroicBuffToMember` (Experienced, Aegis of Dalaran, Pain Suppression, 1%
Threat), recorded when it adds any, on members in the bot's own instance only. Experienced 71188 and
1% Threat 70115 never expire, so `IccHeroicBuffsDefinition`'s tick takes both off whenever none of the
fights handing them out (Deathwhisper, Putricide, Sindragosa, the Lich King) is `IN_PROGRESS`: each bot
off itself, and the first bot past a 1 s per-instance throttle off every human on the map. Every
raid-gated site keeps the cheat's exact behaviour while the raid cheat is
on; off, the site skips the cheat. The cleanup halves (Cyclone, Magic Barrier, 1% Threat removal) stay
ungated.

**The class taunt** (`IccCastClassTaunt`, ten bosses) casts the real taunt, at most once per 500 ms
per bot; a taunt on cooldown fails with no ranged pull, which would face the add. The 1M threat with
`FixateTarget` and the cooldown reset are gone, so:

- a caller taunting several units in a row reaches one per cooldown (8 s on every class): the gunship
  home tank's boarders, the council's princes and Dark Nuclei, the Lich King's add passes and Raging
  Spirits;
- a taunted unit holds the taunter for the 3 s aura, then real threat decides;
- a tank swap (Festergut, Stinky and Precious, Putricide, Saurfang, Sindragosa) waits out a cooldown
  spent elsewhere, while the stacked tank has already stopped.

Saurfang, Festergut, Rotface's Big Ooze and the gunship captain carry taunt diminishing returns
(`flags_extra` 0x80000). Saurfang's swap is one elected taunt per Rune of Blood with generic taunts
blocked, and his free tank taunts one Blood Beast only while the next swap stays covered. Festergut
and the dogs swap by one elected taunt, generic taunts blocked. Rotface's kiter taunts a Big Ooze
through one path, skipped at the immune level. The gunship's board tank taunts the captain.

Each boss doc says what its callers risk.

Documented, not wrapped: Valithria's floor snaps (`IccValithriaPortalAction`,
`IccValithriaHealAction`) skip fall damage through `NearTeleportTo`, which keeps combat; `IccLichKingAddsAction`'s Val'kyr edge-dive puts the
carry aura on itself and suicides at the bottom; the Lich King's 1 yd platform snap cancels knockbacks
mid-flight ([the-lich-king.md](the-lich-king.md#unstucks)). Not cheats: spell clicks and object use in range,
real item use, own-aura cancels, the spell listeners.

## Per-instance state

`IccInstanceState` (`Util/ICCScripts.h`) sits in a `RaidInstanceState` store: `IccState(id)` creates
it, and a has-state read (`ActiveGooPositions`, the reset) uses `Find`. Assignments and latches are
traced containers:
`lk.winterinbound`, `lk.mtinbound`, `lk.rangedinbound`, `lk.sharedslot` (-1 none), `sg.flaredblue`,
`sg.group`, `sg.tomb`, `sg.freedfallback`, `bql.curve`, `bql.airslot`, `bql.slot`, `vd.portalclaim`.
They are unordered, and no loop over one depends on its order. Saurfang keeps
`dbs.slot`, `dbs.kill`, `dbs.cc`, `dbs.markheal` and `dbs.protect` in its stem's own
`RaidInstanceState`, Festergut `fg.slot`, `fg.carrier`, `fg.leg`, `fg.sporespot` and `fg.brake`,
Rotface `rf.slot`, `rf.dispel`, `rf.kiter`, `rf.flood` and `rf.oozespot`; Deathwhisper's stem latches
`ldw.brake` (`ObsGuidSet`) and the frost giant's `rfg.apart` (`ObsGuidSet`) the same way. Unused:
the gunship's `gsbLastStarPos` and `gsbLastRocketPackUse`, Festergut's `fgSlotState`,
`festergutGooWaitUntil` and `SpreadSlotState`, Rotface's `rotfaceVileGasWaitUntil` and
`rfExplosionSlotMemory` (`rf.explosionslot`, silent).
Timestamps, throttles, caches, edge detectors (`sgLastPhase3`) and struct values stay bare;
`sgFlaredRedThisPhase`, an int set, is noted as `sg.flaredred` where it changes.

**Reset**: `IccBossStateResetScript` stamps the state while any boss is `IN_PROGRESS` and erases it 5 s
(`ICC_RESET_GRACE_MS`) after the last one leaves it, kill or wipe, never mid-fight;
`RaidInstanceStateMapScript` drops it with the map. It is keyed on boss state because Gunship, Valithria, Svalna, Keleseth and Taldaram are
not dungeon bosses, so a reset on "a dungeon boss left combat" let their latches outlive a wipe.

## Spell listeners

`Util/ICCScripts.cpp`, on the caster's map thread, so their probes satisfy RaidObs's threading rule.
Unpaced: each fires once per cast, and 25H goo hits two players at once. A note's `txt` is the spell
id.

- **Malleable Goo**: Putricide's 70852 and the balcony's 72296, neither with a spelldifficulty row.
  The missiles and impacts they trigger (72297 row 2197; 72295/72615/74280/74281 row 2211) carry
  Putricide as their unit target and never pass the player test. Each impact is noted as a 12 yd, 8
  s hazard circle; consumers size their own, Putricide's 8 yd, 6 s the widest, Festergut's 5 yd,
  8.5 s.
- **Vile Gas**: Putricide's balcony cast on Rotface heroic, 69240 remapped by the `Spell` constructor to
  73019 (10H) or 73020 (25H), so it is matched through `GetSpellIdForDifficulty`. Notes `rf.vilegas`
  on the victim; no Rotface code reads `rotfaceVileGas` any more.
- **Defile** 72762, a 2 s cast with no difficulty row, read at `OnSpellPrepare`. Notes `lk.defile` on
  the target and a 6 yd hazard circle for the cast time; the puddle that follows is a sampled creature.

## Trigger names

Every trigger name starts `icc `, mostly followed by its boss: `lm`, `lady deathwhisper`, `dbs`,
`festergut`, `rotface`, `putricide`, `bpc`, `bql`, `valithria`, `sindragosa`, `lich king`. Deathwhisper's are
`icc lady deathwhisper` dodge, defensive, frostbolt, dominate mind, taunt, redirect and target;
Marrowgar's include `icc lm tank defensive` and `icc lm storm cover`, Saurfang's `icc dbs tank
defensive` and `icc dbs mark defensive`. Festergut's are `icc festergut` goo, pungent defensive,
pungent cover, tank defensive, bloat swap, brake, target and stand; Rotface's `icc rotface` dodge,
tank defensive, dispel, ooze taunt, kite, carrier, target and stand. The others:
`icc dark reckoning` (Deathwhisper), `icc in cannon` and the `icc gunship` ones (Gunship), `icc
rotting frost giant plague`, `icc dogs` tank defensive, decimate cover, swap, yield and target
(Stinky and Precious), `icc valkyre spear` and `icc sister svalna` (Svalna). The raid-wide `icc opener swing`, `icc opener redirect` and `icc resistance
aura` belong to no encounter. Rows register classes under their `Name`, so two classes sharing one leave
one unbuilt.

## Known gaps

- Targeting is by raid icon outside Marrowgar, Deathwhisper, the giant, the Gunship, Saurfang,
  Stinky and Precious, Festergut and Rotface; the probes record every pick.
- Every gap cheat above: a mechanic it makes survivable is open, and a server with the raid cheat off
  loses it.
- The hooks answer only while a definition is live, so Festergut's aura hook, and any for Rotface or
  Valithria, sits out the seconds before their state moves.

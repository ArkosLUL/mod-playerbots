# Naxxramas threat redirect: which bosses want a Black Temple style misdirect action

## Context

Black Temple gives most of its bosses a **dedicated, boss-scoped threat-redirect action** instead of
relying on the generic class-level `misdirection on main tank` / `tricks of the trade on main tank`
node. Naxxramas has exactly one such action (Kel'Thuzad) plus a blanket veto multiplier that turns
hunters and rogues into dead weight on four encounters.

This document records which Naxxramas bosses would gain from the same treatment, which are already
served well enough by the generic path, and which must stay redirect-free. The Tier 1 bosses have
since been implemented — see "What was implemented" below.

All paths are relative to `modules/mod-playerbots/`.

## The Black Temple pattern

Four wiring pieces per boss:

| Piece | Example | Location |
|---|---|---|
| Trigger | `HighWarlordNajentusPullingBossTrigger` — hunter-only + `boss->GetHealthPct() > 95.0f` | `src/Ai/Raid/BT/BTTriggers.cpp:28` |
| Trigger registration | `creators["high warlord naj'entus pulling boss"]` | `src/Ai/Raid/BT/BTTriggerContext.h:23` |
| Action | `HighWarlordNajentusMisdirectBossToMainTankAction : AttackAction` | `src/Ai/Raid/BT/BTActions.cpp:93` |
| Strategy node | `ACTION_RAID + 2` | `src/Ai/Raid/BT/BTStrategy.cpp:19` |

Every BT redirect action has the same body: cast `misdirection` on the resolved tank, then — once the
`SPELL_MISDIRECTION` self-aura is up — fire `steady shot` into the boss to burn the charges. All of
them are **hunter-only**; there is no rogue Tricks of the Trade anywhere under `src/Ai/Raid/`.

Three timing flavours exist, and they cover everything Naxxramas needs:

- **Pull** — boss above 95% health: Naj'entus, Teron Gorefiend, Mother Shahraz, Illidari Council
  (`BTTriggers.cpp:28`, `:185`, `:395`, `:449`).
- **Phase transition** — timer-based (`SupremusPullingBossOrChangingPhaseTrigger` is active for the
  first 10s of every 60s phase cycle, `BTTriggers.cpp:118`; Gurtogg's window restarts whenever Fel
  Rage drops, `BTActions.cpp:931`) or state-based
  (`ReliquaryOfSoulsMisdirectBossToMainTankAction` re-fires while the *current* Essence is above 95%
  health, i.e. once per fresh spawn, `BTActions.cpp:953`).
- **Multi-tank assignment** — `SupremusMisdirectBossToMainTankAction` (`BTActions.cpp:276`) indexes the
  first three living hunters in the group and maps hunter[0]→main tank, hunter[1]→assist tank 0,
  hunter[2]→assist tank 1. `IllidariCouncilMisdirectBossesToTanksAction` (`BTActions.cpp:1329`) does a
  fixed 4-way index→(boss, tank) map. `IllidanStormrageMisdirectToTankAction` (`BTActions.cpp:1754`)
  picks its target from the encounter phase.

Two supporting idioms are worth copying:

- `IllidariCouncilControlMisdirectionMultiplier` (`src/Ai/Raid/BT/BTMultipliers.cpp:465`) zeroes the
  **generic** `CastMisdirectionOnMainTankAction` so it cannot compete with the boss-scoped action.
- `IllidariCouncilWaitForDpsMultiplier` (`BTMultipliers.cpp:526`) and
  `IllidanStormrageWaitForDpsMultiplier` (`:730`) explicitly return `1.0f` for their redirect action so
  the DPS-hold window never suppresses it.

Tanks are resolved through the shared helpers `GetGroupMainTank` / `GetGroupAssistTank`
(`src/Ai/Raid/RaidBossHelpers.h:27-28`).

## Current Naxxramas state

- `KelthuzadMisdirectBossToMainTankAction` (`src/Ai/Raid/Naxx/Action/NaxxActions_Kelthuzad.cpp:418`,
  node at `src/Ai/Raid/Naxx/NaxxStrategy.cpp:48`, `ACTION_RAID + 3`) is the only BT-style action.
  Hunter-only, and already phase-aware: phase 2 dumps into the boss, phase 1 into the current add.
- `NaxxThreatRedirectMultiplier` (`src/Ai/Raid/Naxx/NaxxMultipliers.cpp:547`, registered
  `NaxxStrategy.cpp:226`) zeroes **both** generic redirect actions whenever Gluth, Instructor
  Razuvious, Gothik or any of the four Horsemen are on the bot's threat list.
- Loatheb, Razuvious and the Four Horsemen additionally force the global `"neglect threat"` value
  (`NaxxMultipliers.cpp:127`, `:272`, `:474`), so bots there ignore threat entirely.
- Four bosses are **fully commented out** — strategy nodes, contexts, actions and helpers alike: Noth
  (`NaxxStrategy.cpp:199`, helper `src/Ai/Raid/Naxx/NaxxBossHelper.h:1152`), Heigan
  (`NaxxStrategy.cpp:31`), Gothik (`:96`), Patchwerk (`:105`, plus all of
  `src/Ai/Raid/Naxx/Action/NaxxActions_Patchwerk.cpp`).
- Helper state a redirect action could lean on today: `ThaddiusBossHelper::IsPhasePet /
  IsPhaseTransition / IsPhaseThaddius / IsMainTankEngagedOnPets / IsOffTankEngagedOnPets /
  IsAssignedToPrimarySide` (`NaxxBossHelper.h:1419-1591`); `SapphironBossHelper::IsPhaseGround /
  IsPhaseFlight / JustLanded` (`:831-840`); `GluthBossHelper::BeforeDecimate / JustStartCombat`
  (`:1080`, `:1108`); `FourhorsemanBossHelper::IsAttracter` (`:1287`).

## Per-boss verdict

The generic node already redirects to the main tank at the pull. A dedicated action only earns its
keep when **the target is not the main tank**, or **the moment that matters is not the pull**.

### Tier 1 — the generic path is actively wrong (implemented)

| Boss | Why | Flavour |
|---|---|---|
| **Thaddius** | Thaddius activates after both pets die with a **fresh threat table** — nobody owns him. Textbook Reliquary of Souls case, and `IsPhaseTransition()` already detects the moment. The pet phase separately wants two redirects (MT on Stalagg, OT on Feugen), which a main-tank-only node cannot express; `IsMainTankEngagedOnPets` / `IsOffTankEngagedOnPets` already resolve both. | phase transition + assignment |
| **Four Horsemen** | Four bosses, one tank each. The generic redirect is vetoed today, so hunters and rogues contribute nothing at the pull — the worst threat moment of the fight. Wants Illidari-Council-style index→(horseman, tank) mapping, and must stay **pull-only**, because the attractor rotation afterwards makes any redirect wrong. Caveat: no per-horseman tank map exists yet. The kill order at `NaxxActions_FourHorsemen.cpp:31-76` and `IsAttracter` are the only assignment state, so the map has to be built from scratch. | assignment, pull-only |
| **Gluth** | Vetoed today because of the MT/OT Mortal Wound swap (`src/Ai/Raid/Naxx/NaxxTriggers.cpp:229`, `NaxxMultipliers.cpp:519`). A redirect at whoever *currently* holds Gluth beats nothing at all, and the Decimate zombie wave wants a redirect at the zombie off-tank. The target is resolvable from `gluth->GetVictim()` plus `GluthBossHelper::BeforeDecimate()`. | current-tank aware |
| **Patchwerk** | Pure threat race: no CC, a hard enrage, and Hateful Strike punishes a sloppy threat order. **No dedicated action** in the end: one tank, no swap and no reset means the generic main-tank node is already the right behaviour, and every second of the fight wants it, not just the pull. | generic node, unchanged |

### Tier 2 — real but smaller; the value is the *add* tank, not the boss

| Boss | Why |
|---|---|
| **Anub'Rekhan** | The boss pull is fine on the generic node. The gain is redirecting the Crypt Guard spawns (at the pull and on each Locust Swarm) at the assist tank that already picks them up (`NaxxActions_Anubrekhan.cpp:43-60`). |
| **Grand Widow Faerlina** | The Worshipper/Follower pack at the pull wants a redirect at the add tank rather than the boss main tank. Note that add tanking is not modelled today — only the assist-tank Worshipper sacrifice is. |
| **Maexxna** | Spiderling waves every 40s are picked up by the off tank (`NaxxActions_Maexxna.cpp:54`). The boss itself never resets threat. |
| **Grobbulus** | Single tank, stable threat, main tank kites the cloud. Pull-only, and the generic node already covers it. |
| **Sapphiron** | Threat survives the air phase — he lands back on the tank — so only the pull matters, and the generic node handles that. Lowest value in the instance. |

### Tier 3 — would benefit, but blocked

| Boss | Why | Blocker |
|---|---|---|
| **Noth the Plaguebringer** | The single best phase-transition case in Naxxramas: Blink wipes threat and every balcony return is a fresh pull. The commented-out helper even has `IsBalconyPhase()` and `IsBlinkWindow()` ready (`NaxxBossHelper.h:1198-1199`). | whole strategy and helper commented out |
| **Gothik the Harvester** | Only the moment he lands and the two sides merge; irrelevant before that. | strategy commented out, and currently vetoed |

**Heigan the Unclean** has since been unblocked: the strategy is live again and `HeiganBossHelper`
(`src/Ai/Raid/Naxx/NaxxBossHelper.h`) exposes `IsFastDance()`. `NaxxThreatRedirectMultiplier` now
zeroes both generic redirects while Heigan is `REACT_PASSIVE` on his ledge, so the charges are still
available for the arena return, which is the moment the tank has to rebuild through. A dedicated
BT-style action was not added — the generic `low tank threat` node already fires on that return.

### No

| Boss | Why |
|---|---|
| **Instructor Razuvious** | Mind-controlled Understudies tank him, and the multiplier already zeroes every player taunt (`NaxxMultipliers.cpp:273-279`). Any player-targeted redirect rips him off the Understudy and wipes the raid. Keep the veto. |
| **Loatheb** | Single tank, no threat reset, no add wave, and `"neglect threat"` is already forced. The generic node is enough. |
| **Kel'Thuzad** | Already done, and already phase-aware. |

## What was implemented

All four Tier 1 bosses, plus the two multiplier fixes. Tier 2 and Tier 3 are untouched.

**Shared base** — `NaxxRedirectThreatAction` (`src/Ai/Raid/Naxx/Action/NaxxActions.h`, body in
`NaxxActions_Shared.cpp`). Covers hunters *and* rogues: a rogue casts Tricks of the Trade on the tank
and stops there, since Tricks redirects everything for the next 6s; a hunter casts Misdirection and
then dumps `steady shot` into a chosen target to spend the three charges. Subclasses implement only
`GetRedirectTank()` and `GetThreatDumpTarget()`. Two protected helpers are shared: `GetTankHolding()`
(which group tank a unit currently attacks) and `GetRedirecterIndex()` (this bot's position among the
group's living hunters/rogues, for the one-tank-per-boss assignments).

| Boss | Tank | Dump target | Trigger window | Node |
|---|---|---|---|---|
| Thaddius | pet phase: tank already on our own pet, else main/assist by `IsAssignedToPrimarySide`; after that the main tank | our assigned pet during the pet phase, nothing afterwards | assigned pet above 95%, or boss above 95% (covers transition and wake-up) | `ACTION_RAID + 3` |
| Four Horsemen | redirecter index even → main tank, odd → assist tank 0 | main tank ↔ Thane Korth'azz, assist tank ↔ Baron Rivendare / Highlord Mograine | `IsEncounterUp()` + `JustStartCombat()`, 10s, attractors excluded | `ACTION_RAID + 4` |
| Gluth | inside the Decimate window *and* already on a zombie chow → assist tank 1 (zombie tank), else whoever holds the boss right now, else main tank | the zombie chow when the zombie tank was picked, else the boss | `JustStartCombat()` or `InDecimateWindow()` | `ACTION_RAID + 2` |

Once Thaddius is up there is no dump shot: Polarity Shift can land at any moment and a bot standing
still to finish a Steady Shot dies to it, so the action only applies the buff and lets the rotation
spend the charges. Gluth resolves tank and dump target from one assignment, so a bot that never
switched to a zombie keeps redirecting boss threat at the boss tank.

Only the two melee horsemen get a redirect: Zeliek and Blaumeux belong to the attractor rotation, and
the window closes after 10s because the rotation makes every later redirect land on the wrong player.
Attractors are excluded outright — they have a corner to reach, and the node outranks the rotation.

Patchwerk has no boss-scoped node. The generic main-tank redirect is already exactly right there (one
tank, no swap, no reset), and a boss-scoped window would only have taken it away for most of the fight.

**Multipliers.** `NaxxThreatRedirectMultiplier` gained `stalagg` and `feugen`: during the pet phase
there is one tank per pet, so the generic main-tank node is wrong for one side. Everything else keeps
the generic redirect. That multiplier already only zeroed the two generic actions, so the Gluth and
Four Horsemen entries stayed as they were. `ThaddiusGenericMultiplier` no longer zeroes the whole
`BuffOnMainTankAction` base during the pet phase, which had also been killing Beacon of Light, Earth
Shield and Thorns on the tank.

**Helpers.** `ThaddiusBossHelper::GetBoss()`, `FourhorsemanBossHelper::JustStartCombat()` /
`IsEncounterUp()` and `GluthBossHelper::InDecimateWindow()` were added. `IsEncounterUp()` exists
because `UpdateBossAI()` needs Zeliek, and `"find target"` only sees creatures that already have this
bot on their threat list — a melee bot parked on Thane never resolves him.

**Not verified by a build.** The module cannot be compiled in the environment this was written in.

## Notes for whoever picks up Tier 2 or Tier 3

- **Wiring cost per boss**: `NaxxActions.h`, `NaxxActionContext.h`, `NaxxTriggers.h` / `.cpp`,
  `NaxxTriggerContext.h`, `NaxxStrategy.cpp` — five sites, and no CMake changes, since every boss
  `.cpp` already exists. Creator name strings are the usual failure mode: a typo there fails silently
  at runtime (trigger or action not found, node never fires), not at compile time.
- Tier 2 is about add tanks, so the new action's `GetRedirectTank()` wants `GetTankHolding()` on the
  add rather than a tank index.
- Razuvious and Gothik keep the blanket veto in `NaxxThreatRedirectMultiplier` either way.

## Related documents

- `docs/general/threat-redirect-trash-and-naxx-plan.md` — the trash-pack proactive redirect and the
  original Naxx veto list.
- `docs/raids/naxxramas/kelthuzad-strategy-fixes-plan.md`, `gluth-strategy-fixes-plan.md`,
  `thaddius-strategy-fixes-plan.md`, `four-horsemen-mark-swap-plan.md`.

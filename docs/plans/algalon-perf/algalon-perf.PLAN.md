# Algalon strategy: performance work that changes no behaviour

## Context

The bottlenecks and hot paths of the Algalon bot strategy, and the optimizations that leave what the
bots do untouched. Nothing here is implemented. Each item is independent and can ship alone.

Basis: a full read of the Algalon sources (`src/Ai/Raid/Uld/**/*Algalon*`, ~7,500 lines), the engine
and helper code they run on, and probe tallies from the 22 Algalon traces in
`env/dist/logs/botobs/603_7_algalon*.ndjson` (server root). **No profiler run exists** (no `pmon` dump
on disk), so every magnitude below is an estimate from source, not a measurement. Each proposed
change was then handed to a second, independent read whose only job was to construct a case where
behaviour differs; none was found, and one half-change was dropped on its finding (see "turned
down"). Line numbers are as of `4125d4e54` (2026-10-10).

**Headline: Algalon's own files hold no single large bottleneck.** The design already does the
expensive things once per raid or rarely: one room scan per instance every 250 ms (`ScanRoom`), every
bot-side helper reads that cached scan, no trigger or multiplier does a grid sweep, half the triggers
are paced, and all seven hand-written multipliers cache their per-action classification. What is left
is two real clusters (items 1-2), a worst-case cap (3), a diffuse lookup tax (4) and trivia (5-6). The
largest costs paid *during* an Algalon pull are most likely two shared Ulduar multipliers that
already have open tickets (last section but one).

Suggested order: 1, 2, 3, 5, 6, then measure before deciding on 4.

## How cost arises here

- A raid bot led by a real player runs an engine pass every 100-300 ms (`ReactDelay` 100 plus a
  0-200 ms per-bot stagger); none while a cast is preparing.
- Each pass (`Engine::DoNextAction`, `src/Bot/Engine/Engine.cpp:187-338`): triggers are checked, then
  actions are popped, and **every multiplier runs on every popped action that passed `isUseful()`**
  — dozens per pass.
- Algalon contributes 24 triggers, 26 multipliers (7 hand-written, 19 rules) and one tick function
  (`src/Ai/Raid/Uld/Definition/UldDefinition_Algalon.cpp`). Twelve triggers run every pass: the Big
  Bang, smash, soaker, tank-heal, tank-defensive, explosion and pickup ones. The other twelve are
  paced 250 ms to 2 s (`Trigger/UldTriggers_Algalon.h`).
- The gate is *open*, not *live*: all of it also runs everywhere in Ulduar between pulls, where each
  piece exits after one or two lookups.
- Expensive primitives, dearest first: `CheckCollisionAndGetValidCoords` (navmesh raycast);
  `PlayerbotAI::CanCastSpell` (allocates a `Spell`, runs `CheckCast`); `IsWithinLOSInMap` (VMAP +
  dynamic-tree raycast); name-based `PlayerbotAI::HasAura` (walks all 317 aura types); a 25-member
  group walk; `FindState` (process-wide mutex + hash, `UldEncounter_Algalon.cpp:122-128`).

## The list

All paths are under `src/Ai/Raid/Uld/`; bare line numbers are in `Util/UldEncounter_Algalon.cpp`.

| # | What | Who pays today | Saving (estimate) | Diff |
|---|---|---|---|---|
| 1 | Tank-cut election pays range, sight and a full cast check before the cheap rejections | the holder and every paladin and priest, each pass while the holder is hurt | most of askers × candidates × (raycast + cast check) | ~40 lines |
| 2 | Tank-heal pick repeats its sight test | 1-2 tank healers, every pass, all fight | 10-14 raycasts a pass down to 0-4 | ~15 lines |
| 3 | Dodge / hole-exit fallback searches redo collision checks | a dodging bot whose first search fails (17 of 1,816 dodges) | caps the worst case at 1-2 sweeps instead of 3-4 | ~10 lines |
| 4 | Helpers re-fetch the encounter state | every bot, per popped action | a few mutex+hash lookups per action | ~150 lines |
| 5 | Four cheap-test-first reorders | see below | small | ~15 lines |
| 6 | Tick builds the tank order three times | once per instance per 250 ms | small | ~10 lines |

### 1. Tank-cut election: cheap rejections first

`GetAlgalonTankCut` (3486-3618) runs every pass for the holder and every paladin and priest. Once the
holder is under 60 %, or under 80 % inside the raid-hit window (1 s before to 5 s after each star death
and meteor) with no cut running, each asker walks the whole provider list to learn whether it is the
first able one. That is askers × candidates per pass (about 9 × 8 with four paladins and four
priests), and the newest trace shows 19 cuts in a 6:18 pull, so the windows are frequent.

Every candidate goes through `ExternalReady` (2379-2395): range test, sight raycast, then
`CanCastSpell`. Only after that do two cheap tests turn most candidates down:
- **On cooldown.** `CanCastSpell` notices it at `src/Bot/PlayerbotAI.cpp:3394`, after the raycast.
- **Reserved for the Big Bang soak.** With no disperser latched, `usable` (3516-3517) rejects any
  spell whose cooldown outlasts the time to the hit, after the raycast *and* the full cast check.
  Nothing is cast, so it repeats every pass.

Changes:
- Split `ExternalReady` into its cheap half (provider AI, `CanStayOut`, spell id, `CanPay`, plus a new
  `provider->HasSpellCooldown(spellId)` rejection) and its dear half (range, sight, `CanCastSpell`).
- In `GetAlgalonTankCut`, apply `usable` to the id from the cheap half, and run the dear half only
  if it passes. `SacrificeReady` (2398-2406) splits the same way. The two calls in
  `ExplosionCoverProvider` (3680, 3698) have no reserve test and simply run both halves.
- In the loops at 3589-3601 and 3603-3615, test the member's class before `IsBot` /
  `PlayerbotAI::IsHeal` (each a hash lookup plus `dynamic_cast`, 25 members, two passes each).

Identical because every test involved only reads, so the conjunction may be reordered.
`CanCastSpell` returns false on the same cooldown test, and its only earlier `true` is for a spell
the caster's pet owns, which none of these are. `AlgalonCooldownMs` (3620-3635) only reads:
`Player::ApplySpellMod` changes nothing without a spell in flight (core
`src/server/game/Entities/Player/Player.cpp:10242-10245`). One debug log line
(`PlayerbotAI.cpp:3396`) is no longer emitted on this path.

### 2. Tank-heal pick: one sight test per tank instead of seven

`GetAlgalonTankHealPick` (3282-3362). `TankHealReachable` (754-758) ends in `IsWithinLOSInMap`. The
branch chain calls `reachable(holder)` up to four times and `reachable(partner)` up to three: **7
raycasts** when both tanks are topped off (branch `full`, 637 of 1,475 recorded branch changes), 5 on
a top-off, 3 on an urgent heal. A tank healer asks twice every pass
(`AlgalonTankHealCriticalTrigger`, `AlgalonTankHealTrigger`), a paladin once more a second
(`AlgalonTankBeaconTrigger`), and `AlgalonTankHealAction::Execute` asks again.

Change: remember each `reachable` answer in a local the first time it is asked, and in every
conjunction test `below(...)` before `reachable(...)`. Topped-off tanks then cost no raycast, and any
other case at most two.

Identical because both lambdas only read, nothing moves inside one call, and every use is already
null-guarded. The `algalon.tankheal` trace row depends only on the branch taken.

### 3. Dodge and hole exit: share collision answers across the fallback searches

`FindAlgalonClearSpot` (`Action/UldActions_Algalon.cpp:71-93`) sweeps up to 12 rings of 16 points and
runs `CheckCollisionAndGetValidCoords` on each point clear of the hazards.
`AlgalonCosmicSmashAction::Execute` (247-283) chains up to four searches and
`AlgalonLeaveBlackHoleAction` (285-310) up to three. Recorded dodges: 1,799 settled by the first
search, 17 fell through (`min` 14, `cross` 2, `none` 1). The exit has no branch probe.

Change: one `HazardSweepCache` (`src/Util/EncounterHelpers.h:76-91`) per `Execute`, passed to every
search in the chain, exactly as `Action/UldActions_Freya.cpp:188-207` does.

Identical because the cache is keyed on ring and angle for one origin and one grid, holds only the
collision answer, and the bot cannot move inside one `Execute`. No saving in the 99 % case; it bounds
the spike when a boxed-in bot falls through every tier (up to ~190 navmesh raycasts per search).

### 4. One state lookup per entry point (optional, do last)

`FindState` is a mutex lock plus a hash lookup and almost every helper opens with one. The paced
triggers already keep this low on the trigger side; what repeats is per popped action:
- `AlgalonHideSlackMs` (3971-4030): five `FindState`, and `AlgalonHoldsConstellation` twice. During
  the 8 s cast it runs twice per pass in triggers, once per popped spell action
  (`AlgalonHideCastMultiplier`) and once per popped movement action (rule `algalon big bang hide`).
- `AlgalonSmashHoldsGround` (4751-4758): two `FindState` and a `GET_PLAYERBOT_AI` on every movement
  action of every bot (rule `algalon smash hold`).
- `AlgalonMayDamage` → `AlgalonYieldsBoss` → `AlgalonShouldPickUp` / `AlgalonShouldSwap` (4178-4237,
  4944-4972): every damaging action; about eight lookups for the swap tanks and the handler.
- `DeriveSlot` / `RawSlot` (2005-2045, 2174-2215): about ten, once a second per bot.

Change: internal overloads in the anonymous namespace that take the `AlgalonEncounterState` already
fetched; the public functions become thin wrappers.

**Only where the player passed down is the calling bot.** `IsAlgalonSwapTank(other)` looks up the
*other* player's instance, so it answers false for a pair member who left the map. The calls about
the holder at 1966, 4196, 4210 and 4233 keep their own lookup, and the tick's calls for another
player at 928-930 stay on the public functions.

Largest diff, smallest gain per line. Worth it only if a measurement after items 1-3 says so.

### 5. Cheap test first

- `AlgalonSoakerPrepTrigger::IsActive` (`Trigger/UldTriggers_Algalon.cpp:61-62`): test
  `PlayerbotAI::IsHeal(bot)` before `AlgalonHideSlackMs(bot)`. Today every unphased bot computes the
  slack each pass of the cast and only healers use it.
- `AlgalonYieldsBoss` (4227-4230): test swap-pair / handler membership before
  `GET_PLAYERBOT_AI(bot)`. This is the path the damage guard takes for every damaging action.
- `AlgalonHandlerKiting` (`Definition/UldDefinition_Algalon.cpp:61`): replace
  `GetAlgalonHandler(botAI) != bot`, a walk over the group on every movement action of a tank, with
  a guid comparison: handler only when the state exists, `bot->GetGroup()` is non-null and
  `state->handler.Get() == bot->GetGUID()`. The group test must stay: a handler kicked from the raid
  is "not handler" today.
- `GetAlgalonExplosionPersonal` (3845-3883): resolve the ten button ids once per call, not once in
  each of its two loops. Each is a string-keyed `"spell id"` lookup, paid every pass by every bot
  that would not survive the star about to die.

All four reorder or reuse side-effect-free reads.

### 6. Tick: build the tank order once

`OrderedTanks` (557-571) is four group walks with a role lookup per member. One tick builds it three
times: twice inside `EnsureSwapPair` (605 or 622, and 619) and once in `ElectHandler` (1186). Build it
once in `AlgalonTickEncounterState` and pass it down. Nothing the tick runs in between changes roles,
flags or who is alive.

## Looked at and turned down

Would change behaviour — do not re-propose:
- Fetching the slot anchor once per dodge `Execute` instead of once per search. `RawSlot` reads the
  clock (Big Bang due within 15 s, Nitro Boosts cooldown), so a fallback search that starts on the
  far side of such an edge would prefer a different point for one tick.
- Reading `state->engaged` / `state->phase` in place of the live `AlgalonEngaged`: up to 250 ms stale
  at the pull and at a wipe.
- A ms-stamped memo of `AlgalonHideSlackMs` or the hide role across the multiplier pass: an action
  that returned false may already have used Nitro Boosts or latched a shelter.
- Sharing the tank-cut election between bots, or moving it into the 250 ms tick: bots act one after
  another and the claim stamps change between askers.
- An early return for non-handlers in the constellation triggers:
  `GetAlgalonHandlerConstellation` (4288-4372) erases a stale kite hole on the way, and every bot's
  shelter pick reads that.
- A longer `checkInterval` on any Algalon trigger: it delays the reaction.
- Id-based aura tests in place of the name-based `PlayerbotAI::HasAura` (1-5 a pass for the tank
  healers, one a second for the aura paladin): the name match covers every rank and any same-named
  proc aura, so an id list is only equal once checked against the DBC. A separate, non-strict item
  if wanted.

Identical but not worth the code:
- A trigger-pass memo (`EncounterTriggerPassId`) of the tank-heal pick or of
  `GetAlgalonHandlerConstellation`: after item 2 it saves at most two raycasts for two bots; the
  handler is one bot.
- Allocation churn in `ScanRoom` / `LogStarHealth` and the other tick internals: 4 Hz per instance.
- Per-action name caches in `AlgalonLeaveFieldMultiplier` / `AlgalonWalkGuardMultiplier`: the names
  involved fit the small-string buffer.

## Paid on every Algalon pull, but not in Algalon's files

Both are open already in
[../ulduar-perf-followups/ulduar-perf-followups.PLAN.md](../ulduar-perf-followups/ulduar-perf-followups.PLAN.md)
and are not re-planned here. They probably outweigh everything above for the bots they hit, because
each sweep is a 100 yd grid visit.

- **Item 4 there, `UldThreatRedirectMultiplier`** (`Multiplier/UldMultipliers_Shared.cpp:48-81`):
  `PB_NPC_ALGALON` is last of six entries, so each `misdirection on main tank` candidate costs five
  full sweeps that miss and one that hits. On Algalon the action is then vetoed (386 recorded vetoes
  in the 22 pulls), so Misdirection stays ready and the hunter asks again each pass its trigger
  fires. Tricks is zeroed earlier by Algalon's own `algalon class tricks` rule and never gets here.
- **Item 5 there, `UlduarBurstWindowMultiplier`** (same file, 85-220): per burst-cooldown action, one
  sweep, three grid searches and a threat-list name walk, to answer "allow" every time on Algalon.
  Trinket, tinker and racial actions are popped constantly (7,746 recorded vetoes by the generic
  `hold burst until tank engaged` alone, which runs ahead of it). It also copies the action name for
  every popped action.

## Verification

- **Item 2** — equivalence harness, the method recorded in the follow-ups plan (none is committed;
  `tools/nativetest` is std-only): old body from `git show HEAD:<file>`, new body from the tree, both
  compiled together with a counting stub for `TankHealReachable`. Enumerate holder and partner
  present or absent, reachable or not, each in the health bands the chain tests (50 / 70 / 85 / 95 %,
  and the 15 % margin), the healer's own band, and the boss on or off the pair. Assert the same pick
  and branch and never more raycasts. Red check: break the memo for a null partner and confirm
  mismatches appear.
- **Item 1** — same harness shape for the provider order: stub the cheap and dear halves with
  counters, enumerate per candidate {no spell, on cooldown, reserved, out of range, castable} and
  assert the same provider and spell id.
- **Items 3-6** — by reading against the callee each one leans on (the sweep at
  `src/Util/EncounterHelpers.cpp:458-562` for 3).
- **Every change** — `~/.claude/scripts/pb-syntax-check.sh <changed .cpp>`,
  `tools/pblint/pblint.py <paths>`, `python apps/codestyle/codestyle-cpp.py`.
- **A pull before and after**, `.playerbots pmon toggle`, `pmon reset` at the pull, `pmon` after. Read
  ratios, not times: triggers `algalon tank defensive`, `algalon tank heal`,
  `algalon tank heal critical` and `algalon explosion cover` against the rest. pmon now times each
  multiplier by name (`Engine.cpp:248-251`); `docs/systems/observability.md:582` still says it does
  not and wants correcting when this closes.
- **Trace** — same probe keys as before, with `algalon.tankheal`, `algalon.tankdef` and
  `algalon.dodge` spread like the 22-pull baseline: dodge `clear` 1,799 / `min` 14 / `cross` 2 /
  `none` 1; tankheal `holder` 693 / `full` 637 / `self` 82 / `offtank` 39 / `range` 24.

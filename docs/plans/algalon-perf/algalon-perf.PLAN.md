# Algalon strategy: performance work that changes no behaviour

## Context

Six items came out of a read of the Algalon sources (`src/Ai/Raid/Uld/**/*Algalon*`), the engine
under them and the 22 traces in `env/dist/logs/botobs/603_7_algalon*.ndjson`. **Nothing was measured**:
no `pmon` dump exists, so sizes are estimates from source. Items 1-3, 5 and 6 are in; 4 is open.

What they established, and what must not be re-proposed, is in
[algalon.md](../../raids/ulduar/algalon.md#what-it-costs) and
[raid-mechanics-lessons](../../engine/raid-mechanics-lessons.md).

## Shipped 2026-10-10, live check still owed

1. Tank-cut election: cooldown and reserve before range, sight and the cast check; class before
   role lookups in the two group loops.
2. Tank-heal pick: health before sight, one sight test per tank.
3. Dodge and hole exit: one `HazardSweepCache` per `Execute`.
5. Cheap test first in `AlgalonSoakerPrepTrigger`, `AlgalonYieldsBoss`, `AlgalonHandlerKiting`
   (`IsAlgalonHandler`), and the ten button ids resolved once in `GetAlgalonExplosionPersonal`.
6. `OrderedTanks` built once per tick.

Checked: syntax check (7 TUs), pblint, codestyle, and old body against new body compiled together
with counting stubs — pick 3,456,000 states, election 21,000,000 random raid states, 0 mismatches,
never more raycasts or cast checks (election: 58 % fewer of each in that mix). Both red checks bit
(a memo that merges the two tanks: 6,840 mismatches; a `CanCastSpell` stub ignoring the cooldown:
72,020). The harness was not committed.

**Not run:** a pull. Owed: `.playerbots pmon toggle`, `pmon reset` at the pull, `pmon` after, reading
the ratio of triggers `algalon tank defensive`, `algalon tank heal`, `algalon tank heal critical` and
`algalon explosion cover` to the rest; and a trace whose `algalon.tankheal`, `algalon.tankdef` and
`algalon.dodge` spread like the 22-pull baseline: dodge `clear` 1,799 / `min` 14 / `cross` 2 /
`none` 1; tankheal `holder` 693 / `full` 637 / `self` 82 / `offtank` 39 / `range` 24.

## Open: 4. One state lookup per entry point

Do it only if that measurement says so: largest diff, smallest gain per line.

`FindState` (`Util/UldEncounter_Algalon.cpp`) is a process-wide mutex plus a hash lookup, and almost
every helper opens with one. The paced triggers keep it low on the trigger side; what repeats is per
popped action:
- `AlgalonHideSlackMs`: five `FindState`, and `AlgalonHoldsConstellation` twice. During the 8 s cast
  it runs twice per pass in triggers, once per popped spell action (`AlgalonHideCastMultiplier`) and
  once per popped movement action (rule `algalon big bang hide`).
- `AlgalonSmashHoldsGround`: two `FindState` and a `GET_PLAYERBOT_AI` on every movement action of
  every bot (rule `algalon smash hold`).
- `AlgalonMayDamage` → `AlgalonYieldsBoss` → `AlgalonShouldPickUp` / `AlgalonShouldSwap`: every
  damaging action; about eight lookups for the swap tanks and the handler.
- `DeriveSlot` / `RawSlot`: about ten, once a second per bot.

Change: internal overloads in the anonymous namespace that take the `AlgalonEncounterState` already
fetched; the public functions become thin wrappers.

**Only where the player passed down is the calling bot.** `IsAlgalonSwapTank(other)` looks up the
*other* player's instance, so it answers false for a pair member who left the map: every call about
the boss's holder keeps its own lookup, and so do the tick's calls for another player in
`NoteTankRoles`.

Identical but not worth the code: a trigger-pass memo (`EncounterTriggerPassId`) of the tank-heal
pick or of `GetAlgalonHandlerConstellation`; allocation churn in `ScanRoom` / `LogStarHealth` (4 Hz
per instance); per-action name caches in `AlgalonLeaveFieldMultiplier` / `AlgalonWalkGuardMultiplier`.

## Paid on every Algalon pull, but not in Algalon's files

Open as items 4 and 5 of
[../ulduar-perf-followups/ulduar-perf-followups.PLAN.md](../ulduar-perf-followups/ulduar-perf-followups.PLAN.md);
each sweep is a 100 yd grid visit, so they probably outweigh everything here for the bots they hit.

- `UldThreatRedirectMultiplier`: `PB_NPC_ALGALON` is last of six entries, so each
  `misdirection on main tank` candidate costs five sweeps that miss and one that hits. The action is
  then vetoed (386 recorded vetoes in the 22 pulls), so Misdirection stays ready and the hunter asks
  again each pass its trigger fires. Tricks is zeroed earlier by `algalon class tricks`.
- `UlduarBurstWindowMultiplier`: per burst-cooldown action, one sweep, three grid searches and a
  threat-list name walk, to answer "allow" every time on Algalon; trinket, tinker and racial actions
  are popped constantly. It also copies the action name for every popped action.

# c5-toc-group-walks

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the
**Wave 3** section bind this lane.

Behaviour change on `Custom`, branch `tocpr/c5-toc-group-walks`: every group walk that a trigger's
`IsActive` or a multiplier reaches uncached gets an O(1) per-tick cost, for upstream's O(1) trigger
rule. The sites (function ← trigger, who reaches it) are listed under "Group-walk audit" in the
Decisions for review of `c1-toc-base.PLAN.md` (Jaraxxus, Twins), `c2-toc-anubarak.PLAN.md`
(Anub'arak) and `c3-toc-fc-beasts.PLAN.md` (Faction Champions, Jormungars). Their line numbers are
at Custom `1039c9045`; find each by function name.

Shape, per site:
- Default: a per-group, per-ms memo of the walk's group-wide result in the encounter's per-instance
  state, shared by every bot, like `GetJaraxxusPinnedCasters` with `JaraxxusState::pinned`
  (`Util/ToCHelpers_Jaraxxus.cpp`): store guids, never `Player*`/`Unit*`, resolve them per call, and
  a groupless bot's result never serves another bot. A per-bot answer (which healer, which hunter)
  stays per bot, computed from the memo.
- Where a cheap test (aura, cooldown, class, role) already decides the answer for most bots, test it
  before the walk instead, if that alone makes the cost O(1) for every bot outside the narrow window.
- Sites over the same walk share one memo.

The only allowed behaviour difference is the memo's: a member's change between two bots' ticks in
one ms shows one tick later. Picks, order and tie-breaks stay the same; a site that can't keep that
stays as it is, logged under Decisions for review with the reason.

Out of scope:
- `HumanInsideRelease` (`Util/ToCHelpers_Staging.cpp`): only `NoteStage`'s trace label reaches it,
  and `p3` deletes it with the trace.
- Walks reached only from actions or chain opens, or already behind a memo, latch or roster interval.
- The shared raid files (`src/Ai/Raid/Raid*.{h,cpp}`): wave 2 has ported them to the PR stack, so
  keep every edit under `src/Ai/Raid/ToC/`.

Also delete unused code (user, 2026-11-18), each referenced only by its own declaration and
definition: `AvoidCreatureClusterAction` with `FleeFromCreatureCluster` (`Action/ToCActions_Shared.*`),
`GetCreatureClusterCenter` and `GetNearestCreatureByEntry` (`Util/ToCHelpers_Shared.*`),
`ANUBARAK_PIT_CENTER` (`Util/ToCData.*`), `SPELL_MASSIVE_CRASH_*` (`Util/ToCData.h`); drop includes
left unused and doc mentions. Re-check with `git grep` before deleting. Duplicated helpers stay.

The lead may split by encounter into at most 3 implementers (Jaraxxus + Twins, Anub'arak,
FC + Jormungars); a memo helper shared across encounters stays with the finisher. The finisher also
greps `src/Ai/Raid/ToC` for `ForEachGroupMember`, `GetFirstMember`, `AliveMembersOnMap` and
`GroupOnMap` to confirm no trigger or multiplier still reaches an uncached walk, and records each new
memo in its boss doc under `docs/raids/trial-of-the-crusader/` (`lord-jaraxxus.md` describes the
pinned-cast memo). `tools/nativetest/` tests of touched code keep passing.

## Task list

Not split: 19 sites, each a small memo, and one shared helper. Lead implements alone.

1. [x] `Util/ToCHelpers_Shared.{h,cpp}`: `ToCGroupMemo<T>` (per group per ms, optional guid key, groupless
   never shared, `Get` const over mutable fields) and `ToCGroupGuid`. Delete `GetNearestCreatureByEntry`,
   `GetCreatureClusterCenter`, `<list>`.
2. [x] Jaraxxus (`ToCHelpers_Jaraxxus.cpp`, `JaraxxusState`): `IncinerateFleshTargetFor` (target guid),
   `JaraxxusIsNetherPowerRemover` (any non-healing remover), `JaraxxusIsFelFireballInterrupter` (lowest
   guid with an interrupt ready, keyed by boss), `AnyLivingAssistTankOne` (bool); `PinnedMemo` onto the helper.
3. [x] Twins (`ToCHelpers_TwinValkyr.cpp`, `TwinValkyrState`): `OtherColourAllyNear` (living members' guid,
   position, essence), `IsTwinPactInterrupter` (lowest guid eligible, keyed by twin), `LustReady` (bool).
4. [x] Anub'arak (`ToCHelpers_Anubarak.cpp`, `AnubarakState`): `GetSpikeTarget` (marked guid);
   `GetAnubarakBattleResTarget` (druid pick keyed by pickup, healer corpse pick); cold carriers shared by
   `GetAnubarakPenetratingColdHealTarget`, `AnubarakAuraMasteryDue`, `AnubarakDivinePleaDue`; tank players
   shared by `lowestTank`, `GetAnubarakTankRescue`, `AnubarakDivinePleaDue`; `HealsOnTheWay` (caster, target
   pairs); `tierPick` (members some healer can rank under `SWARM_HEAL_TIERS`); healer bots by guid shared by
   `BehindStand` and `GetAnubarakHealerStand`; `GetAnubarakRedirect` (hunter bots, first rogue bot).
5. [x] FC: `CounterspellMage` (`FactionChampionsState`, keyed by target, returns a guid);
   `FactionChampionsChased`/`FactionChampionsKite` read living healers on the map (`BallState`) instead of
   `GroupOnMap`.
6. [x] Jormungars: `HunterIndex` (living hunters in group order, `JormungarsState`).
7. [x] Unused code: `AvoidCreatureClusterAction`/`FleeFromCreatureCluster`, `ANUBARAK_PIT_CENTER`,
   `SPELL_MASSIVE_CRASH_*`; drop the includes left unused.
8. [x] Final grep for `ForEachGroupMember`, `GetFirstMember`, `AliveMembersOnMap`, `GroupOnMap`; docs: each
   memo in its boss doc, `ToCGroupMemo` and the deletions in `trial-of-the-crusader/README.md`.

## Decisions for review

- **One helper, `ToCGroupMemo<T>`** (`Util/ToCHelpers_Shared.h`, ToC code, not the shared raid files):
  same rule as c1's `PinnedMemo`, which now uses it unchanged. `Get` is const over mutable fields, so states
  passed as const keep their signatures. A refresh rebuilds the answer in place; no fill reads its own memo.
- **Interrupter picks** (Fel Fireball, Twin Pact, Counterspell) memo the lowest eligible guid, keyed by the
  boss, twin or kill target; a bot is on duty when it is eligible itself and nobody lower is. The fill tests
  members in guid order up to the first eligible one, so which members' `"spell id"`, `CanCastSpell` and
  `WantedEssence` get read moves; a bot turning ready between two ticks in one ms can make two claimants for
  that ms. Both are the brief's allowed memo difference.
- **Twins splash test** memos each living member's guid, position and essence; the per-bot answer is a
  distance scan over that list, no unit lookups.
- **FC Nitro run and kite hop** memo the living healers on the map, all `NearHealer` and `HealerGap` read;
  their per-spot search still loops those healers. The cheap tests (Nitro ready, focused, chasers) already ran
  before the walk.
- **Anub'arak swarm heal** (`tierPick`) loops only the members `SwarmHealTier` can rank under
  `SWARM_HEAL_TIERS` for any healer: carriers, tanks under the top-off, non-tanks under
  `ANUBARAK_SWARM_HOT_HP` with no HoT. That predicate must follow `SwarmHealTier` (code comment says so).
  `lowestTank`, `GetAnubarakTankRescue` and `AnubarakDivinePleaDue` loop the tank list (and Plea the cold
  carriers too); loop bodies unchanged, group order kept.
- **Off a ToC instance** `GetSpikeTarget`, the cold heal target, Aura Mastery, Divine Plea, the tank rescue
  and `CounterspellMage` answer null or false instead of walking: unreachable, every caller runs on map 649.
- **Unused code deleted** after a `git grep`: `AvoidCreatureClusterAction`/`FleeFromCreatureCluster`,
  `GetNearestCreatureByEntry`, `GetCreatureClusterCenter`, `ANUBARAK_PIT_CENTER`, `SPELL_MASSIVE_CRASH_*`,
  `<list>` and `MovementActions.h` with them. README's `AvoidCreatureClusterAction` bullet went whole:
  `pitfalls.md` already holds the `FleePosition` cap and the clearance rule. `docs/plans/icc-rebuild/
  r1-foundation.PLAN.md` still says to leave these in ToC; left alone, it's another effort's brief and the
  instruction is moot.
- Final grep: every remaining `ForEachGroupMember`/`GetFirstMember`/`AliveMembersOnMap`/`GroupOnMap` site
  is a memo fill, behind a per-instance memo, latch or roster interval, action-only (Jaraxxus carrier spot,
  Twins `CriticalAllyNear`), or trace-only (Jaraxxus `addTank` prune, `HumanInsideRelease`).

## Known gaps

- Every memo is keyed by group, so two groups in one instance recompute on every switch (as the sphere and
  Shadow Strike duty memos already do).
- Duplicated helpers (`TryClassTaunt`, segment-distance helpers, threat caps, `NorthrendBeastsWalkAction`'s
  home) stay, by the user's call.

## Blocked

None.

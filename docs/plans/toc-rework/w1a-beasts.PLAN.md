# w1a-beasts — stage model, Gormok, Icehowl

Wave 3. Rules, sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md). The
code layout and gate are in `docs/raids/trial-of-the-crusader/README.md`. Guide:
`beasts-of-northrend-master-strategy-guide-toc-25/`.

## Scope

1. **Stage model.**
   - Which beast is active, and tank assignment across all three stages.
   - Heroic runs on timers, not kills (worms at 150 s, Icehowl at 340 s, enrage at 520 s; verify in
     the instance script), so beasts overlap. One tank model must cover that, for 10- and 25-man
     tank counts.
   - `w1b-jormungars` builds on this model, so expose it from the `NorthrendBeasts` stem.
2. **Gormok.**
   - Impale tank swap (today at 3 stacks, `GORMOK_IMPALE_SWAP_STACKS`; check the Impale id per
     difficulty).
   - Snobolds: Snobolled players, Snobold Vassals. The guide has a Snobold-to-melee tactic.
   - Fire Bomb ground fire, and Staggering Stomp (interrupting cast).
   - `RaidTankDefensive` if the swap fails at high Impale stacks.
3. **Icehowl.**
   - Rebuild the charge: today it uses `FleePosition` plus a multiplier vetoing every
     `MovementAction`, which also vetoes `AttackAction`. Use hazard-clear stands from
     `FindNearestPositionClearOfHazards` and forced dodges, and settle two forced dodges in a
     multiplier.
   - Probe the charge corridor with `NoteHazard`, since it has no world object.
   - Arctic Breath and Whirl.
   - The wall-crash stun as a burn window.
   - A `RaidTankDefensive` for Frothing Rage after a missed charge.
4. **Lust row.** Today lust fires on Gormok's pull and is wasted. Pick the window per the guide and
   the script.
5. **Observability.** `nb.` probes (stage, tank assignment, charge decisions), a
   `tools/botobs/bosses/northrend_beasts.py` reader, and a synthetic-pull test.
6. Verify every Beasts node that `w0c-foundation` made live in 25N/10H/25H.

## Owns

Stems `Gormok`, `Icehowl`, `NorthrendBeasts`; `docs/raids/trial-of-the-crusader/northrend-beasts.md`;
the reader and its test. Not the `Jormungars` stem, except the stage-model interface it consumes.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked

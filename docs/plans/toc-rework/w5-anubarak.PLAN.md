# w5-anubarak — Anub'arak

Wave 3. Rules, sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md).
Guide: `anubarak-master-strategy-guide-toc-25/`.

## Scope

1. **Transition.** The Lich King scene (stage 8) and the floor collapse.
   - `IsEncounterInProgress()` is true from that scene until Anub'arak dies or the raid wipes, so the
     gate must stay closed until the pull.
   - Handle landing in the pit and regrouping.
2. **Phase 1.**
   - Nerubian Burrowers: tank them on Permafrost in heroic; Shadow Strike interrupts.
   - Scarabs.
   - Frost Sphere → Permafrost economy.
   - Penetrating Cold targets.
3. **Phase 2 (submerged).**
   - Kite the Pursuing Spike into Permafrost with hazard-clear stands, replacing the blanket
     movement veto.
   - Scarabs.
   - The spike (34660) is boss-flagged and engages under the name "Anub'arak".
4. **Phase 3.** Leeching Swarm with `RaidTankDefensive`; burrowers keep coming in heroic.
5. **Threat.** On emerge he resets threat and picks a random target (`boss_anubarak_trial.cpp`), so
   add a redirect or taunt.
6. **Lust row.** `w0c-foundation` ported the Leeching Swarm delay unchanged; re-tune it now.
7. **Observability.** `anub.` probes (phase, spike target, sphere use), a
   `tools/botobs/bosses/anub_arak.py` reader and test.
8. Verify every Anub'arak node `w0c-foundation` made live in 25N/10H/25H.

## Owns

Stem `Anubarak`; `docs/raids/trial-of-the-crusader/anubarak.md`; the reader and its test.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked

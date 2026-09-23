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

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked

# Aspect of the Wild — one holder at a time

In flight only for the live check below. Everything else has shipped and is written into
[../../raids/ulduar/thorim.md](../../raids/ulduar/thorim.md),
[../../classes/hunter.md](../../classes/hunter.md) and [../../raids/README.md](../../raids/README.md).

## What changed

`GetNatureResistanceHunter` (`src/Ai/Base/Util/GenericBuffUtils.cpp`) gained a third outcome. A
candidate below the Viper entry threshold that still wears Aspect of the Wild is recorded as
`staleHolder`, and when one exists the helper returns `nullptr` instead of drafting a replacement, so
nobody casts and nobody is pinned. Both callers compare the result against their own bot, so `nullptr`
already reads as "not me" — no call-site changes.

Before the change, a drained holder was skipped by the eligibility `continue` before the `auraHolder`
check, so it became invisible to the search and a healthy hunter was drafted alongside it. That is
what produced two simultaneous Aspect of the Wild sources on
`603_4_elder-stonebark_1789241915` — guid 115360 with 24 recipients and guid 5141 with 22 — while
5141's Dragonhawk sat vetoed at `rel 0.00` for nothing, since 49071 is `MOD_RESISTANCE_EXCLUSIVE`.

## What is still unverified

Only a live pull can show it. Needs a nature boss: Kologarn, Freya, Thorim, or VoA's Emalon and
Archavon. Elder Stonebark also works — the `freya nature resistance` node resolves Freya by grid sweep
from her Conservatory.

Against the newest trace for that boss:

1. **One holder at any instant.** Group `"e":"aura","sp":49071` records by their `"s"` field. Two
   distinct sources overlapping in time is the defect returning.
2. **No pointless pin.** A hunter that is not the holder must show no `rel 0.00` on any
   `aspect of the *` action.
3. **Handoff still happens.** Over a long fight the `"s"` should change hands at least once rather than
   one hunter holding throughout, unless only one hunter was ever above 30% mana.
4. `tools/botobs/postmortem.py <file> --verify` exits zero.

Baseline to beat is `603_4_elder-stonebark_1789241915`: two sources, and the non-holder vetoed.

## Known gap this does not close

`Aq40UseResistanceBuffsAction` still leaks `+rnature` (and `+rshadow` for priests) to any hunter that
leaves AQ40 after Viscidus or Princess Huhuran, which blocks Aspect of the Viper outright for the rest
of the session. Recorded in [../../raids/README.md](../../raids/README.md); deliberately out of scope
here. A hunter carrying it will still run dry on long fights — the change above only stops it costing a
*second* hunter its aspect.

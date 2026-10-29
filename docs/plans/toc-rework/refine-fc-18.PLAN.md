# refine-fc-18: grip note, CC flicker, Ret crawl reach, Shamanistic Rage reserve

Wave 4ao, run in session on the user's call (2026-10-29). Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4z `6814c755b`.

## Why

25H kill 2026-10-29 (`649_2_caiphus-the-stern_1793302299`), 3 deaths: Totemist at 1:06 (the iced Ret's Hammer of
Justice, then 25.8k in 220 ms) and 1:47 (no Shamanistic Rage: the Enh rotation spent it at full health at 1:11),
Agony at 1:54 (Warrior and hunter 28k in one tick). Kiting, CC and defensives all fired; the gaps are in Known
gaps.

## Changes

- **Grip note** (bug): `FactionChampionsNoteChainGrip` read the chain through `OpenChain`, whose advance saw the
  grip's own cooldown and skipped the link, so `NoteGrip` never ran. It now reads `ChainFor`. Native test
  `GripNotedBeforeItsCooldownIsRead`.
- **CC flicker** (bug): `UpdateCcDuties` dealt a recast at 2 s left that the keep check, the caster's own CC
  past cast time + 0.4 s, released the next refresh. The deal now skips that caster's spell too.
- **Ret crawl reach**: `CrawlReach` 17 yd for a Retribution champion (`KITE_CRAWL_REACH_RET`), 10 for the rest.
- **Shamanistic Rage reserve**: `faction champions personal reserve` zeroes the class `shamanistic rage` all
  fight; the personal defensive casts it by id. Cost: Enh mana return and the T10 2-piece buff.

## Targets (next 25H)

No `fc.linkskip` at a grip's ms and no "skipping my grip" whisper; no `fc.cc` duty under 0.3 s; fewer Hammer of
Justice 66007 auras and a smaller iced Ret share in `--damage`; Shamanistic Rage only with a `toc.personal` note.

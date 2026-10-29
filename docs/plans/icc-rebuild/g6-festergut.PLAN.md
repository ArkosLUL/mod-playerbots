# g6-festergut — close Festergut's and Stinky and Precious's gaps

Gap wave `3cd-gaps`, after `3g`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap lanes"
section included. Builds on the merged `r6-festergut` lane and its review: read
[r6-festergut.PLAN.md](r6-festergut.PLAN.md) first. Its decisions stand unless closing a gap needs a
change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/festergut.md`,
`docs/raids/icecrown-citadel/stinky-and-precious.md` and r6's brief.

- The resist-hook bullet is stale: his `HookRows()` row is in `src/Ai/Raid/ICC/Util/ICCSeams.cpp`
  (`be0531146`). Confirm and drop it.
- Precious's zombies: `build-icc-wt/_guides/text/stinky-and-precious.blog.md` and the core script decide
  who holds, spreads or kills them.

## Owns

Stems `Festergut` and `Dogs` and their definitions, both docs, the readers `festergut.py` and
`stinky_and_precious.py` and their tests, this brief.

## Task list

One lead, no split: every change sits in the two stems, their readers and docs.

Facts the tasks rest on (core, DBC, DB as of this lane):

- No 631 trace on disk. The user built `623638b99` clean on 2026-10-29.
- Festergut's resist row is in `HookRows()` (`ICCSeams.cpp:33`): the hook bullet is stale.
- Floor: navprobe rings at 2-44 yd (25 radii, 72 headings, `--nav 0x09`) round his spawn all settle at
  360.385; sight to his spawn is clear out to 42 yd (one heading blind at 44).
- Divine Shield 642 has `SPELL_ATTR1_IMMUNITY_PURGES_EFFECT` and all-school immunity; Gastric Bloat
  (school 40, no `SPELL_ATTR0_NO_IMMUNITIES`) is purged on apply. Mortal Wound carries
  `SPELL_ATTR0_NO_IMMUNITIES`, so nothing removes it. School immunity calls `EvaluateSuppressed`, so a
  bubble kept up turns Festergut onto the next on his threat list.
- Awaken Plagued Zombies 71159 is set to infinite duration (`SpellInfoCorrections`): zombies live until
  killed or Precious dies. Zombie 38104/38105: 63k/101k health, 3.2-4.4k/5.5-7.6k a swing before
  armour, Infected Wound 71158 → 71157 on every swing (+100 physical damage taken a stack, 99, 60 s).
  Precious 3.15M (10) / 8.82M (25): a wave of 11 is 22% (10) or 13% (25) of her health.
- Swings every 2 s before armour, (base + attack power / 14) × damage modifier × 2 (`CalculateMinMaxDamage`):
  Stinky 12.5-17.4k (10) / 26.4-36.7k (25), Precious 24.9-34.7k / 52.9-73.5k; Festergut 35.2-48.5k (10N)
  to 81.9-112.8k (25H) before Inhaled Blight.
- Inhaled Blight's scale aura grows his combat reach 20% a stack (8 → 12.8 at three), and his chase stops at
  both reaches plus 0.5 off a tank (`ChaseMovementGenerator`).

Tasks:

1. Festergut drag: a bot holder walks him back once he stands 3.5 yd off his spawn outside a spore
   wave, to his chase stop distance past his spawn on the far side from him, until he is within 2.5 yd (`fg.drag`,
   stand `drag`).
2. Spore stack shift: goo fouling the close stack, or a far stack the flips can't move, sends that group
   onto one spot clear of every impact for the rest of the wave, within 6.75 yd of the player who can't
   walk (the holder, or the player it rings) so the burst still takes him; failing that, the nearest
   clear spot (`fg.closeshift`, `fg.farshift`).
3. A third tank holding him swaps stands with the tank-spot tank carrying the most Gastric Bloat.
4. Melee fill counts players in ranged and healer specs as standing out.
5. One bot tank: a bot paladin holder at 9 Gastric Bloat with no taunter casts Divine Shield and
   cancels it (Wowhead); a human paladin holder is called to. Other classes recorded.
6. Dogs: threat brake on a tank not holding the dog (90%/80%, `dogs.brake`), folded into the yield.
7. Dogs, one tank: a damage-dealing death knight or paladin guards the zombies (`dogs.guard`): its pick
   is the loose zombie, and it taunts one off a healer. The swap recorded as impossible.
8. Readers and tests for the new keys; both boss docs; healer walks closed by analysis.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Drag past 3.5 yd, to both combat reaches plus 0.5 past his spawn (10-14.8 yd), done within 2.5 yd, never in a spore wave | core chase stop distance, widened by Inhaled Blight's scale (review: a fixed 9 yd left him short from one stack, unmoved from two); his legs release at 3.5; a holder walking off the close stack loses the tanks' stack | Legs derived from where he stands |
| Shifted stack is one spot, no ring | a 2.5 yd ring can't fit between a 7.5 yd goo clearance and a fixed holder 8 yd away; Vile Gas is held 20 s past every spore, so a tight stack costs nothing | Members dodge one by one and wait the impact out |
| Shift kept for the rest of the wave unless goo fouls it again | a walk back can land on the burst | Return once the impact ages out |
| A shift that can't keep the holder in still moves the rest | three of four keep their stack | Leave the group to dodge one by one |
| Third tank takes the relieved tank's spot | his facing and the close stack both follow the tank spots | Third tank on the close anchor (inside goo reach of both spot tanks) |
| Human ranged and healers count toward Vile Gas's quota by spec, not by where they stand | a position count flaps melee in and out as humans move | Position past 14.5 yd |
| Paladin bubble cleared at 9 at once when no other tank lives, else 6 s after the 9th stack | Wowhead: Divine Shield removes Gastric Bloat; the next stack lands 15-17.5 s after; suppression turns him while it runs, so it is cancelled at once (it still outlasts the cancel until the holder's next threat, see Known gaps) | Bubble kept up (he turns to the raid) |
| No damage-dealer swap on Festergut | his stacks need 100 s to drop; 35-113k swings every 2 s before armour on a damage dealer | — |
| No damage-dealer swap on the dogs | Decimate leaves the taker at 15% under 12.5-73.5k swings within the 15 s the stacks need | — |
| Dogs brake 90%/80%, folded into the yield | Festergut's and Putricide's brakes | A separate node |
| Zombie guard only with fewer than two tanks alive, death knight first, then paladin | their taunts work in any presence or seal; plate | Hunters or warlocks with pets |
| Guard taunts a zombie on a healer; the free tank still doesn't | the guard has no swap to save its taunt for | No guard taunt |
| Healer walks closed by analysis | the heal pick adds 30 to a member past 38.5 yd, so a healer walks only for one 30 points lower than all in reach; slot 1 reaches every stand, slots 2 and 3 all but two each, and each covers the other's | Block heal reach on slots |

Tasks 1-8 done; healer walks and the resist-hook bullet closed in the doc.

Review: fixed the drag distance (bug), the shift's keep reach, which let a walking fixed player leave the
burst, the human bubble call missing the Avenging Wrath marker, the swing figures and the explosion
comment (minor); recorded the bubble's leftover suppression.

## Carried over

- Merge stage, ICC README: trigger names (Festergut adds `bloat clear`, the dogs `zombie taunt`); the
  per-instance state list (Festergut adds `fg.closeshift`, `fg.farshift`, `fg.drag`; Stinky and Precious
  keep `dogs.brake` and `dogs.guard`); the class-taunt paragraph (the dogs' zombie guard taunts zombies off
  healers, a free tank never does).
- r14: unchanged from r6 (`fgSlotState`, `festergutGooWaitUntil`, `SpreadSlotState`, the 12 yd goo note,
  one taunt helper, one walk base); the Festergut and Dogs stems now each keep their own `IsTankPlayer`,
  `IsDutyBot`, `MembersOnMap`, `TauntName`, `TauntSpellId`, `TauntUsable` copies.
- No CMake re-run: no file added.

## Known gaps

- No 631 trace from a build with this stem; no `-Werror` pass. Built clean at `623638b99`.
- Festergut: a goo within 5 yd of the holder still hits him; a shift that can't keep him in leaves him
  out of the burst; one bot tank that is no paladin, with no human able, explodes at the 10th stack; the
  drag rests on the core's chase stop distance (`fg.drag` spans); humans hold no duty.
- Festergut, from review: a cancelled bubble leaves the holder suppressed on his threat list until its next
  hit or Consecration tick (only `AddThreat` brings it back), and his once-a-second victim check in that
  gap can hand him to the next player for a swing. No player action closes it: Hand of Reckoning fails on
  its own victim. `--tanks` target runs round each bubble show how often.
- Dogs: one tank carries Mortal Wound to ten stacks; a guard at 15% after Decimate can die to its
  zombies; humans hold no duty.

## Blocked

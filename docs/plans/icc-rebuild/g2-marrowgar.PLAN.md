# g2-marrowgar — close Lord Marrowgar's gaps

Gap wave `3ab-gaps`, after `3b`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap lanes"
section included. Builds on the merged `r2-marrowgar` lane and its review: read
[r2-marrowgar.PLAN.md](r2-marrowgar.PLAN.md) first. Its decisions stand unless closing a gap needs a
change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/lord-marrowgar.md` and in r2's brief, plus
the two every gap lane does (tank defensives, line of sight).

- Tank defensives: the doc names his arrival after Bone Storm; check the script for every other
  predictable tank spike, Bone Slice included.
- Line of sight: every stand r2 placed (the inner arc, the bait ring, the storm stack and anchors)
  against him, and the healer stands against the tanks.

## Owns

Stem `Marrowgar` files and its definition, the doc, the reader and `tools/botobs/tests/test_lord_marrowgar.py`,
this brief.

## Task list

One lead, no split: every change sits in the stem's helpers and state.

1. [x] Line of sight: navprobe `los` and `ring --los-from` over the bait ring, inner slots, tank spot,
   stack ring and anchors with him up to 8 yd off his spot; the bait ring stays at 10 slots.
2. [x] Tank defensives (convention 43): Bone Slice window model from the core timers, the main tank and
   off-tanks spend `NextTankDefensive` (physical only) at the window and under 40%, class buttons held
   off until then, a call to a human tank.
3. [x] Bone Storm raid cover: `GetRaidCover` on 6 s windows of the storm's ticks, class cover nodes held
   over the lead-in.
4. [x] Heroic storm spikes: storm picks stay inside the bot's reach from its stand.
5. [x] Dodge walk tested against the other lines (`accept` callback, `SegmentCrossesCircle`).
6. [x] Berserk: reader margin; Hand of Protection on Impaled: core check (closed, nothing to wire).
7. [x] Probes `lm.defensive`, `icc.cover`; reader `--defensives`, `--covers` and berserk margin; tests; doc.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Bait slots stay 10 | navprobe `ring --los-from` and `los` over 49 boss positions to 8 yd off his spot and a 2 yd grid to 10.5 yd: all ten on the floor and in sight of him, the tank spot and the inner arc; only the headings past them (−100°, +120°) fall behind a wall | Cap at 8 (it only bites with a ninth hunter, who then stands inside him) |
| Tank defensive: each main tank and off-tank spends the weakest physical button once per Bone Slice window (1 s before to 6 s after) and again under 40%; the class buttons wait until 25% | Core: Bone Slice resuming is the only predictable tank spike; convention 43; Obsidian Sanctum and Anub'arak | A button at every storm's end (his arrival is plain swings) |
| Windows come from phase edges plus the core's 10 s, never from a Bone Slice cast | Convention 23, `EVENT_ENABLE_BONE_SLICE` | Read the cast (an on-next-swing spell) |
| A human tank is called at the window and under 40% | `RaidHumanTankCall`, convention 43 | No call (humans hold no duty) |
| Raid cover: `GetRaidCover` per 6 s of ticks, the tick taken at 28 yd, offered 3 s ahead; Anti-Magic Zone dealt out, personal buttons not wired | WT "rotate defensive cooldowns", Wowhead Divine Guardian; core tick formula; shared `RaidDefensive` stays as it is | Each member's own distance (needs a shared change); a personal pair as Jaraxxus has |
| In a storm a bot takes only a spike within 10 yd (melee) or 28 yd (ranged) of itself | A spike pick freed the bot's stand, so reach walked it into his lanes | Walk the stack-to-spike axis to range (lines unpredictable); leave it to a dedicated bot |
| No Hand of Protection on Impaled | Core: Impaled (69065) has `SPELL_ATTR0_NO_IMMUNITIES`, `IsImmunedToDamage` returns false for it | Wire it as Wowhead says |
| The dodge walk must cross no other patch within 5 yd; four attempts, the last two (`cross`) ignore that | `EncounterHelpers` accept callback, `SegmentCrossesCircle` | Keep the straight step |
| No Berserk pacing | Wowhead calls it very lenient (Warcraft Tavern is silent); core 10 min (26662) | Burst pacing |

## Carried over

- `ICCData.h` keeps the unused `NPC_SPIKE*_10H/_25N/_25H`; r14 can drop them (from r2).
- `IccLmWalkAction` copies ToC's walk latch; r14 can hoist one raid-wide base (from r2).
- `RaidDefensive.cpp` deals Anti-Magic Zone to a physical hit; the lane drops it at its call. A school
  filter in the shared deal would serve every user.
- The ICC README's defensives seam bullet gains Marrowgar as its first user (`icc lm storm cover`, the
  cover hold, `icc lm tank defensive`); the merge stage applies it.

## Known gaps

- No 631 trace; never built (no new `.cpp`, no CMake re-run); no `-Werror` pass.
- Humans hold no duty: one standing far out draws his charges.
- A heroic spike on an anchor holder is 34 yd from the stack, past a stack bot's reach, so nobody shoots it in
  the storm: that player lives on the healers until it ends.
- The first Bone Storm tick lands as the aura does (69076 has `SPELL_ATTR5_EXTRA_INITIAL_PERIOD`), ahead of
  every cover window, which starts at the second tick.
- The cover's damage is the tick at one flat 28 yd, not each member's distance; `--covers` against the
  deaths in a storm says whether it deals late. No personal defensive for a storm.
- No bot tank has held a Bone Slice window live: whether the button lands inside the first slices waits
  for `--defensives`.
- Storm timing comes from the first tick that reads the aura, so a window start can sit 0.3 s late; the
  3 s offer and 1.5 s lead cover it.
- Gameobject collision isn't in navprobe's sight.

## Blocked

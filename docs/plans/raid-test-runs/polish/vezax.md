# Polish log: General Vezax (25N hard mode)

Trace slug `vezax` (files `general-vezax`), raid `uld`, scenario `uld-vezax` (Faceless Horrors and
Twilight packs despawned; main tank DK, else warrior, else bear). Hard mode is the Saronite Animus: a kill
counts only if it was summoned and killed (`general_vezax.py --vapors`: `vezax.hardmode` reaches
`animus`, never `lost`). Reader `tools/botobs/bosses/general_vezax.py`.

## Round 1 (2026-11-22, build 02925e260)

**Data.** Window 8, one valid pull per pool (`raidtest.py report --boss vezax --raid uld --head 02925e260
--pulls 10`). Pools by roster prefix (Uldx/Uldy/Uldz):

| Pool | Run | Trace | Main tank | Result |
|---|---|---|---|---|
| uld1 | `tr-20261122-120446-26` | `603_26_general-vezax_1795349141` | DK | wipe 4:44, 32 deaths |
| uld2 | `tr-20261122-120526-27` | `603_27_general-vezax_1795349181` | warrior | kill 4:08, 1 death, **not hard mode** |
| uld3 | `tr-20261122-120405-25` | `603_25_general-vezax_1795349102` | DK | wipe 4:57, 30 deaths |

Window 7 smoke (`--head 61011f323`): uld1 `tr-20261122-101341-30` (`603_30_general-vezax_1795342477`),
wipe 5:02, 32 deaths. Analysis scripts: session scratchpad `vezax-r1/py/`.

**Stop rules.** Not polished: two wipes, and the kill lost hard mode at 2:30.7 (a vapor died, no Animus,
Barrier never cast). First round.

**Deaths by mechanic.**

| Pull | Deaths |
|---|---|
| w8 uld1 | 4 ranged to Vezax melee 2:51-3:12 (a warlock pulled him at 2:48 and he walked into the camp); 1 Shadow Crash 3:37; collapse 4:00-4:39, 27: DK main tank to Mark leech 4:00, then 9 Animus melee, 9 Vezax melee, 6 Profound Darkness, 1 Mark, 1 Shadow Crash |
| w8 uld2 | 1 ranged to Vezax melee 1:29 (a warlock pulled him at 1:23) |
| w8 uld3 | collapse 4:26-4:52, 30: 19 Profound Darkness, 6 Vezax melee (DK main tank first), 4 Animus melee, 1 Mark |
| w7 uld1 | fury warrior to Vezax melee 0:26 (opener); 1 Shadow Crash 3:37; collapse 4:24-4:57, 30: 15 Profound Darkness, 7 Vezax melee, 6 Animus melee (both tanks), 1 Mark, 1 Shadow Crash |

**Worst: the Animus-phase collapse (85 of 95 deaths). A DPS race the 226 rosters lose, not a bot bug.**
The Animus (12,236,737 health) spawns at 3:08 with Vezax at 21.4-26.7%. On it the raid does 118k/s
(uld3) and 125k/s (w7) with Heroism at +0.2 s, against 134k/s on Vezax the minute before; uld1 did 80k/s
after its threat losses and lost its main tank at 4:00. Healer effective healing peaks at 17-31k/s while
Profound Darkness (749 every 2 s, +10% shadow taken per stack) climbs; at 37-40 stacks (~75 s) it
outruns them and the raid dies in one or two ticks (w7 4:30.52-4:30.54: 6 deaths), with the Animus at
21.8-26.8% (uld1 49%). It needs ~100 s at that rate. Vezax meanwhile regains 5-10% from Mark heal-back
(Barrier only cuts damage taken), so 26-36% of him would remain under 40+ stacks. Contributors:

- ranged are held outside fields all through it (`vezax hold cast outside field multiplier`, 277 vetoes in
  75 s on uld3) and still hold 33-91% mana at +70 s;
- roster DKs deal 2.2-3.5k/s against 6.5-8k for other melee (the talent bug), about 8k/s;
- most melee do ~10% less on the Animus than on Vezax, Heroism included (front lane, no Backstab
  or Shred, debuffs and DoTs reapplied).

Classification: gear, plus a tactic question (Q1, Q2). Not changed.

**Picked: dps take Vezax off the main tank (bug; 6 deaths, healer cooldowns and mana, uld1's camp).**

1. *Opener, all four pulls: rogues' Tricks of the Trade on the fury warrior.* uld1: Uldxaf and Uldxat
   cast 57934 on Uldxay at 0:00.24 and 0:00.25 (redirects live 0:00.67, 0:00.99); Uldxay was Vezax's
   victim from 0:01.2 to 0:31.8, the DK main tank's from 0:33.8, after both rogues' next Tricks went to
   it at 0:31.2. Same in uld2 (warrior 0:03.3-0:38.8), uld3 (0:03.0-0:36.1) and w7 (0:03.3, warrior
   dead 0:26.7). Healers spent Pain Suppression, Guardian Spirit, Lay on Hands and Hand of Sacrifice on
   him. Off-tank Righteous Defense held Vezax 1.2 s (uld1 0:04.3-0:05.5); uld2's warrior main tank logged
   `taunt` IMPOSSIBLE throughout. Cause: `TricksOfTheTradeTargetValue::Calculate`
   (`src/Ai/Class/Rogue/RogueValues.cpp:41-44`) took the highest-AP melee dps although both tanks stood
   beside the rogues at the drop: `TankNeedsRedirect` (`:15-30`) is false at the pull, since its opener
   branch never runs in raids and a rogue that hasn't hit yet has no target or no threat. Known shared
   pitfall (`docs/engine/pitfalls.md`, Mimiron).
2. *Mid-fight: an affliction warlock in a Shadow Crash field passes 130% of the main tank.* uld1 Uldxak
   took Vezax at 2:48.4 from the DK main tank (in a field since 2:42.9; DK 9.9 yd from Vezax); uld2
   Uldyal at 1:23.7 from the warrior (in a field since 1:11.8). Nothing held dps under the tank on Vezax:
   the threat multiplier only covered the Animus (`UldMultipliers_Vezax.cpp:78-96`), and no bot runs
   the `threat` strategy.

The DK main tank's ~25 of 71 talents matter here (uld1's loss), but uld2's warrior lost Vezax the same
way, so the fix doesn't depend on them.

**Second (independent, small): area damage killed a vapor in the melee (bug; the only kill lost hard
mode).** uld2's 2:00.2 vapor landed within Whirlwind reach of the melee ball (Vezax at (1850.4, 91.0)):
Uldyay's Whirlwind at 2:14.17 and 2:22.45 (Deep Wounds on it), Uldyan's Divine Storm at 2:24.97 (Holy
Vengeance on it), with Death and Decay and Pestilence on Vezax, Consecration, Fire Nova and an
Explosive Trap around; `vezax.hardmode` lost at 2:30.7. Cause: 63081 is `TARGET_DEST_CASTER_RADIUS` at
45 yd with no randomised distance (`Spell.cpp:1735-1772`), but `MovePositionToFirstCollision` lands it on
the first navmesh edge in the way, or on the caster when that raycast fails (`Map.cpp:3464-3489`).
Before the stop line nothing held area damage. (Starfall killed the second vapor at 2:37, after hard
mode was already lost and its hold lifted.)

**Fix** (Vezax files only: `UldDefinition_Vezax.cpp`, `UldMultipliers_Vezax.{h,cpp}`,
`UldEncounter_Vezax.{h,cpp}`):

- `vezax threat multiplier` (was the Animus-only one): a non-tank, non-healer bot's damage on Vezax or the
  Animus holds once its threat reaches 90% of the main tank's on that boss's list. On Vezax it stands
  down while the main tank isn't on his list. Auto-attack already swinging continues.
- `vezax tricks on tank multiplier`: the generic Tricks of the Trade fires only when its target is a tank,
  for the whole encounter.
- Rule `vezax vapor area hold`: while hard mode is pending, 47 area spells by name hold for a bot with a
  live vapor within 15 yd of it or of its current target (review added Corpse Explosion, Fire Elemental
  Totem, Frost Nova, Shadowflame, Thunderstorm).

Docs: `docs/raids/ulduar/vezax.md`, plus a Vezax data point on the shared Tricks pitfall in
`docs/engine/pitfalls.md` (doc only, no shared code).

10/25 and normal/hard: the threat and Tricks guards key on the main tank and the encounter only; the
vapor hold needs hard mode pending, so normal mode never sees it.

**Next window should show.**

- No 57934 on a non-tank on Vezax; Vezax's victim a tank from the opener on. `postmortem.py --threat
  33271` melee and ranged shares well under the 10-15% / 2-12% now, and no non-tank killed by his melee
  before the Animus.
- `vezax threat multiplier` veto rows mostly on warlocks and the fury warrior, with pre-spawn raid damage
  not far under 129-135k/s and Vezax still at ~21-27% at the spawn. A large drop means the cap throttles
  the raid to the weak DK tanks.
- `vezax vapor area hold` vetoes only with a vapor in the melee; `--vapors` shows no area cast at a vapor
  and `vezax.hardmode` never `lost` before the Animus.
- The Animus collapse stays (gear); judge this round by the opener and the threat pulls.

**Shadowfiend (build 02925e260).** Healer priests cast it at 29-51% mana (w7: 80%), and their mana 15 s
later was at most a point higher in all five healer casts: under Aura of Despair it returns nothing whatever the gate.
Harmless here, recorded in `docs/raids/ulduar/vezax.md`.

**Questions (user).**

- *Q1. Vezax hard mode at ilvl 226 is a gear check the rosters fail* (Animus ~100 s against ~75 s of
  survival, then 26-36% of Vezax under 40+ stacks). Options: (a) keep polishing mechanics on the 226
  rosters with the stop rule waived, as for Algalon; (b) polish at 25n with `UlduarVezaxHardMode` off,
  which first needs a normal-mode path (with it off nothing kills a vapor, so the Animus still spawns and
  the raid stands on the Barrier); (c) raise Vezax's roster gear. *Recommended: (a)*, and re-measure
  after the DK talent fix.
- *Q2. Lift the ranged cast hold outside fields while the Animus lives?* A tactic change: ranged die with
  33-91% mana, and the raid lives ~80 s past the spawn either way. *Recommended: yes*; worth roughly 10%
  on the Animus, not enough alone.
- *Q3. Shared: fix `TricksOfTheTradeTargetValue` in class code* (`RogueValues.cpp:15-44`, the open
  pitfall since Mimiron): give the tank Tricks at the pull and never hand it to a dps while threat is
  still being built.
  *Recommended: yes, as its own lane*; every boss with a rogue gets pulled again, and the Vezax and
  Mimiron guards can then go.

**User answers (2026-11-22):** (1) keep polishing on the 226 rosters, the stop rule waived as for
Algalon; (2) yes, lift the ranged "no casting outside a field" hold while the Animus lives; (3) no
shared Tricks of the Trade fix, so the Vezax-only Tricks guard stays.

## Round 2 (2026-11-22, branch from `eda15422d`)

**Data.** No new pull: window 9 did not pull Vezax after round 1 merged (`0f303c478`); window 10 will. So
`raidtest.py report --boss vezax --raid uld --head 02925e260 --pulls 10` still shows window 8's three.
No stop rule, verdict or failure ranking this round (stop rule waived by answer 1; round 1 unjudged).
Deaths by mechanic: none new, round 1's table stands.

**Picked: answer 2, the field cast hold on the Animus (user-decided tactic).** Baseline re-measured on
the round 1 traces with the new `general_vezax.py --animus` caster table (spawn to the collapse, the
first sample under 4/5 of the sampled roster alive):

| Pull | Window | Animus | Raid / ranged | Ranged casts/min | Outside-field casts | Field share | Ranged mana at the collapse | Holds per ranged |
|---|---|---|---|---|---|---|---|---|
| w8 uld3 | 3:08.5-4:28.6, 80 s | 100→22.9%, 0.96 %/s, kill ~104 s | 118k / 56k/s | 15.7-28.5 | 0-4 per bot | 49-68% | 0-93, median 47 | 19-44 |
| w7 uld1 | 3:08.6-4:28.6, 80 s | 100→19.2%, 1.01 %/s, kill ~99 s | 124k / 67k/s | 13.5-24.0 | 1-5 | 49-69% | 0-100, median 42 | 16-43 |
| w8 uld1 | 3:08.8-4:03.4, 55 s (main tank dead 4:00) | 100→57.8%, 0.77 %/s | 95k / 39k/s | 16.5-24.2 | 0-3 | 45-62% | 8-81 | 12-29 |

Cause: `VezaxHoldCastOutsideFieldMultiplier::GetValue` (`UldMultipliers_Vezax.cpp:61`) zeroed every ranged
damage action outside a 63277 field for the whole encounter, Animus included.

**Fix** (`UldMultipliers_Vezax.{h,cpp}` only): the hold returns 1 for a bot whose current target is the
live Animus (entry 33524). Keyed on the target, not the Barrier: in the first 2-6 s after the spawn ranged
still sit on Vezax behind the Barrier (1% damage) and stay held there. Healers were never under this hold
(the multiplier exempts `IsHeal`), and no other Vezax rule holds a healer's casts, so nothing changes for
them. 10/25 and normal/hard: the Animus only exists in hard mode, so normal mode keeps the hold as before.
Reader: `general_vezax.py --animus` gains the pace and caster table above (`animus_casters`, tests in
`tools/botobs/tests/test_general_vezax.py`). Docs: `docs/raids/ulduar/vezax.md` (field hold, the trace,
known gaps).

Expected size: outside a field a caster loses +100% damage and +100% cast speed, so a second of casting
there does about a quarter of an in-field one. With ranged out of a field 31-51% of the phase that is
roughly +10k/s, ~8-10% on the Animus: ~0.96-1.01 → ~1.05-1.1 %/s, a kill in ~90-95 s, still past the
~80 s collapse. Not enough alone, as round 1 said. uld2's only Animus pull, from a build before window 7
(`603_7_general-vezax_1795299665`), came closest: 143k/s, 1.15 %/s to its collapse at +84 s, the Animus
dead at +94.5 s with 9 of 25 alive, then a wipe on Vezax at 5:32.

**Next window should show** (`general_vezax.py --animus`, pulls that keep the main tank to the collapse):

- ranged `outside` casts on the Animus up from 0-5 per bot to a share near their out-of-field time
  (31-51%), casts/min up from a 14-29 range, and `held` at n/0, n being holds on Vezax behind the
  Barrier before the switch;
- ranged damage on the Animus above 56-67k/s and raid above 118-124k/s; the Animus pace above
  0.96-1.01 %/s, a projected kill under ~99-104 s;
- ranged mana at the collapse well under the median 42-47%; a ranged at 0 well before the collapse
  means full-price casting outran the pool and the gain fell short;
- Animus victim still a tank (`time on target`) and `vezax threat multiplier` vetoes on ranged on the
  Animus: more casting pushes them toward the 90% cap, and a ranged pull of the taunt-immune Animus
  would cost more than the gain.
- Round 1's checks still stand (no 57934 on a non-tank, no non-tank killed by his melee before the
  Animus, no area cast at a vapor).

**Observation, not chased.** Elemental shamans' mana climbed in the Animus phase in all three pulls
(67→81, 70→93, 73→100) while every other caster's fell: something still returns mana under Aura of
Despair. Unchecked; the doc's "immune to every mana return" may have an exception.

**Questions.** None new.

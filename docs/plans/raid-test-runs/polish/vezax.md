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

## Round 3 (2026-11-22, build 697052475)

**Data.** Window 10, one valid pull per pool (`raidtest.py report --boss vezax --raid uld --head
697052475 --pulls 10`), all hard mode on, all wipes:

| Pool | Run | Trace | Main tank | Result |
|---|---|---|---|---|
| uld1 | `tr-20261122-165438-14` | `603_16_general-vezax_1795366526` | DK Uldxam | wipe 1:28, 30 deaths, no Animus (Berserk 0:29.6) |
| uld2 | `tr-20261122-170121-17` | `603_19_general-vezax_1795366925` | warrior Uldyaa | Animus killed at 4:25.5, wipe on Vezax at 26%, DC `no_progress` 16:36, 23 deaths |
| uld3 | `tr-20261122-165659-15` | `603_17_general-vezax_1795366664` | bear Uldzaz | wipe 5:30, 31 deaths, Animus at 3.6% at the collapse |

Analysis scripts: session scratchpad `vezax-r3/py/`.

**Deaths by mechanic.**

| Pull | Deaths |
|---|---|
| uld1 | all 30 after Berserk 26662 at 0:29.6: 28 Vezax melee (hits of 66-274k), 2 Shadow Crash |
| uld2 | after the Animus kill: 18 Vezax melee (main tank 4:30.9 under Surge of Darkness with healers at 0-26% mana, then ranged as he roamed, the off-tank, the melee), 2 Shadow Crash, 2 Mark, 1 own Shadow Word: Death backlash (4:26) |
| uld3 | collapse 4:28-4:40: 14 Profound Darkness, 5 Animus melee; then 8 Vezax melee, 2 Mark, 2 Searing Flames (re-raised bots among them) |

**The 1:28 wipe (uld1).** The first Mark (0:19.97) went to the DK main tank: 6 of 25 stood past the Mark
floor, short of the 9 the core wants (`boss_general_vezax.cpp:248-258`). His mark node sent him to the
nearest southern spot, 26 yd south of Vezax (`TryGetVezaxMarkSpot`, `UldEncounter_Vezax.cpp:428-454`),
which is measured from Vezax and moves with him: the tank's moves ran from (1852.8, 63.1) at 0:20.3 to
(1852.8, -8.8) at 0:30.0 and Vezax followed from y 89.1 to y 17.2. `UpdateAI` casts Berserk the moment
he leaves x 1720-1940, y 20-210 (`boss_general_vezax.cpp:188-189`): Berserk 64112/26662 at 0:29.636,
first death 0:30.8, all 30 dead by 1:24.

**The 16:36 stall (uld2).** The Animus died at 4:25.5 (the first Animus kill since window 7). The raid then
collapsed under its Profound Darkness stacks with dry healers; with the main tank dead at 4:30.9 Vezax
roamed after ranged to (1880.7, 179.1), ~100 yd north of his anchor and still inside the berserk box.
The camp is boss-relative, so slots and strafes followed him past the hall's north edge: at 6:22-6:24
four bots (Uldyaj, Uldyaf, Uldyae, Uldyaq) took Shadow Crash dodge moves to (1898.3, 210.5), the move
z dropping 342.2 → 339.0 → 334.1, and fell to (1861.1, 170.7, -27.7) under the map. Vezax followed to z
226.6, could not reach them, and regenerated from 25.2% to 100% between 6:42 and 6:55; the 10-minute
Berserk at 10:12 changed nothing, and DC's `no_progress` ended the pull. Two re-raised bots (Uldyab,
Uldyak) stood on the floor, out of his reach as well. No death came from it; it cost ~10 minutes of
pool time.

**Verdict on round 2 (ranged cast hold lifted on the Animus): it worked, beyond the estimate.**
`--animus`, spawn to kill or collapse:

| Pull | Animus | Raid / ranged | Ranged casts/min | Outside-field casts | Held | Ranged mana at the stop |
|---|---|---|---|---|---|---|
| uld2 | 3:08.5-4:25.5, killed in 77 s, 1.30 %/s | 160k / 89k/s | 20-43 | 7-14 per bot | 0/0 | 7-54, median 15 |
| uld3 | 3:08.5-4:30.6 collapse, 3.6% left, 1.17 %/s (kill ~85 s) | 145k / 69k/s | 23-34 | 10-19 | 0/0 | 0-82, median 22 |
| round 2 baseline | 0.96-1.01 %/s, kill ~99-104 s | 118-124k / 56-67k/s | 14-29 | 0-5 | 16-44 | median 42-47 |

Pace +16-29% against the ~10% estimated; uld3 was ~3 s short. One ranged (Uldzaq) hit 0 mana at 3:56,
34 s before the collapse; the rest ran out within ~8 s of it. Animus victim a tank 98.7% (uld2) and 88.3%
(uld3, the rest after the tank died at 4:29.5). Round 1 checks hold: Vezax's victim a tank 98.9-100%
before the Animus (a warlock pet held him 2 s at the uld2 opener), every Tricks and Misdirection on a
tank, `vezax threat multiplier` vetoes 3-111 a pull, no vapor hit, hard mode held in both pulls that got
there, Vezax at 20.3% and 14.7% at the spawn.

**Failure ranking.**

1. *Profound Darkness race and the collapse after it: 54 deaths (uld3 31, uld2 23).* Gear, as in round 1;
   the user waived the stop rule. uld2 killed the Animus and still lost: Vezax went 20.3% → 26.9% on Mark
   heal-back during the Barrier, and the raid could not finish 26% under ~40 stacks with dry healers.
   Not changed.
2. *Berserk from a marked main tank: 30 deaths (uld1).* Bug, fixed below.
3. Shadow Word: Death backlash under the stacks: 1 (class code, not chased).

**Picked: the marked main tank drags Vezax out of his room (bug).** The doc already has the tank hold the
boss "whatever happens", and the dodge leaves him out for the same reason (`VezaxDodgesShadowCrash`), but
`VezaxMarkOfTheFacelessTrigger` had no such test. Same hazard for anyone else he is hitting: uld2's
re-raised tank was marked as his victim at 6:33.

**Fix** (`UldTriggers_Vezax.cpp` only): `VezaxMarkOfTheFacelessTrigger` stands down for the bot Vezax is
hitting (`GetVictim() == bot`), so it holds where it stands; melee and the off-tank still break off it.
10/25 and normal/hard: the Mark and the chase exist in all four. Bots only, so the same with a real
player in the raid.

**Reach audit row (taken as a bug): confirmed, not moved.**

- *Core.* `2026_11_06_01.sql:2515` sets display 28548 to CombatReach 12.6 (base 8), template scale 1
  (`creature_template_model` 33271), `Creature.cpp:3609` writes reach × scale, and `GetDistance`
  subtracts both sizes (`Object.cpp:1304-1308`): his "15 yd" is 29.1 yd of real 3D distance.
- *Traces.* 37 marks with 23+ alive in windows 7, 8 and 10 (7 pulls): 7 went to melee or the main tank,
  with 6, 7, 2, 7, 9, 9 living players past 29.1 at the snapshot before (and 0 once Vezax was pulled into
  the camp), against 12-14 past the old 24.5 in all six. Every mark with 10+ past 29.1 went to someone
  past it but one, at 28.7 yd in a snapshot 0.3 s old. The 0:20 mark had 2-7 past in 4 of 7 pulls. A 13-14 bot camp fills rows 0-3; rows 2-3 (5-6 bots)
  are past, row 1 is 0.3-0.7 yd past when exactly on its slot, row 0 inside. At 10-man the camp fills rows
  0-1, so nearly every mark falls inside.
- *Why not moved.* Vezax settles 7.1-11.5 yd north of his anchor, between the main tank on it and the camp
  (he meets the tank on its way in). Past 41.5 yd 2D `PartyMemberToHeal` scores a member as 30 points
  healthier (`PartyMemberToHeal.cpp:88-95`, `GetDistance2d` > 38.5), so the tank is picked there only
  when ~30 points lower than everyone else, and past 43 yd 3D (heal range 40 plus both reaches,
  `Spell::CheckRange`) no heal lands. Replayed on where both really stood 0:15-3:00 in six pulls: a
  slot at 27-29.25 is within 41.5 of the main tank 86-100% of samples, 31.5 0-100%, 33.75 0-72%, 36+
  0-18%; every camp bot as it stood plus 4.5 yd cuts the share of time *any* healer is within 41.5 of
  him from 43-93% to 10-42%. So no radius clears 29.1 with the 2.4 yd deadband and keeps the healers on
  the tank: where the camp goes is a positions call (Q1). The victim guard above removes the outcome that
  made the floor lethal.

**Reader.** `general_vezax.py`: band floor and ceiling on reach 12.6 (29.1 / 42.6); `--mark` gains a
`far` column, the living past the floor at each mark against the 9 or 4 the core wants (test added).
Docs: `docs/raids/ulduar/vezax.md` (band, crash skip 17.1, the victim guard, the off-floor gap); the
camp constant's comment in `UldEncounter_Vezax.h`.

**Next window should show.**

- No mark node move by Vezax's victim: `--mark` branch for a marked main tank reads `unchanged`, no
  `vezax mark of the faceless action` move from him, Vezax within ~12 yd of his anchor through every
  mark window, no Berserk before 10:00.
- Marked main tank windows: melee break moves, and a leech no worse than a marked melee's (6-16 ticks).
- `far` under 9 still common (the rows did not move): marks on melee or tanks at roughly 1 in 5.
- Round 2's Animus figures hold (1.17-1.30 %/s, ranged outside casts 7-19, held 0).

**Questions (user).**

- *Q1. The camp's front rows sit inside the Mark floor since the reach update; moving them out loses the
  healers' reach to the main tank. Which layout?* (a) Healers fill first, front row at 30.5 (1.4 yd past
  the floor), ranged dps behind to 39.5: replayed reach to the tank 78-100% in 5 of 6 pulls, 0% where
  Vezax settled 11.5 yd north; every camp bot past the floor, so 13-14 (25-man) and 5-6 (10-man) far.
  (b) The main tank stands on the camp side, ~10 yd north of the anchor, so Vezax never leaves his spawn
  and every row reaches the tank: the camp can move out freely, but the melee end up south behind him,
  where the southern mark spots are, and those and the Animus melee lanes need rework. (c) Shift every
  row 4.5 yd as the audit says: healers lose the tank most of the time. (d) Keep the rows: with this
  round's guard a near-pool mark costs a leech, not a wipe. *Recommended: (a)*, the smallest change that
  clears the floor and keeps the tank healed in the common settle; (b) if the north-most settle keeps
  starving him.
- *Q2. Guard the camp against the off-floor walk?* Check each strafe and slot against the navmesh
  (`CheckCollisionAndGetValidCoords`, as the hazard search already does) and fall back to the search.
  Only reachable after a collapse, so it saves pool time, not kills. *Recommended: yes, small, in a later
  round.*

**Candidate points for Q1 (a)**, for navprobe before any such change: rows 30.5 / 32.75 / 35 / 37.25 /
39.5, files and strafe as today, mark arcs at 35 yd ±65.5°, z 342.4. The widest strafed slot is 44.5 yd
from him, inside the 45 yd gate. For Vezax at his north-most settle (1851.4, 92.9):

```
row 0: (1853.9,123.4) (1856.9,123.4) (1848.9,123.4) (1845.9,123.4) | strafed (1868.9,123.4) (1871.9,123.4) (1833.9,123.4) (1830.9,123.4)
row 1: (1853.9,125.7) (1856.9,125.7) (1848.9,125.6) (1845.9,125.6) | strafed (1868.9,125.7) (1871.9,125.7) (1833.9,125.6) (1830.9,125.6)
row 2: (1853.9,127.9) (1856.9,127.9) (1848.9,127.9) (1845.9,127.9) | strafed (1868.9,127.9) (1871.9,127.9) (1833.9,127.9) (1830.9,127.9)
row 3: (1853.9,130.2) (1856.9,130.2) (1848.9,130.1) (1845.9,130.1) | strafed (1868.9,130.2) (1871.9,130.2) (1833.9,130.1) (1830.9,130.1)
row 4: (1853.9,132.4) (1856.9,132.4) (1848.9,132.4) (1845.9,132.4) | strafed (1868.9,132.4) (1871.9,132.4) (1833.9,132.4) (1830.9,132.4)
mark arcs: (1883.2,107.4) (1819.6,107.4)
```

For Vezax at his west-most settle (1847.8, 90.0) the same points shifted by (-3.6, -2.9): row 0 strafed
out to (1827.3,120.5) and (1868.3,120.5), row 4 to (1827.3,129.5) and (1868.3,129.5), mark arcs
(1879.6,104.5) (1816.0,104.5).

**User answers (2026-11-22):** Q1: the Mark should only reach ranged, so move the camp: option (a),
healers in front at 30.5 yd, ranged behind out to 39.5 yd. The candidate points above navprobe on the floor
(orchestrator: both settles' outer strafed slots, row 4 and the mark arcs read z 341.6-342.4, dz under 0.9).
Q2 (dodge and camp points off the walkable floor) is a bug: fix it in the same round.

## Round 4 (2026-11-22, branch from `b21cda8ee`)

**Data.** Window 11 at 25n (`raidtest.py report --boss vezax --raid uld --head d2874f536 --pulls 10`) and
window 12's 10n check (`--head ebe746ad1 --pulls 10 --diff 10n`), one valid pull per pool, all kills with
hard mode held (`--vapors`: `animus` at 3:08.2-3:08.5, never `lost`). Analysis scripts: session scratchpad
`vezax-r4/`.

| Window | Pool | Run | Trace | Result |
|---|---|---|---|---|
| 11 (25n) | uld1 | `tr-20261122-190009-16` | `603_16_general-vezax_1795374055` | kill 5:52, 2 deaths |
| 11 (25n) | uld2 | `tr-20261122-190132-17` | `603_17_general-vezax_1795374146` | kill 6:02, 1 death |
| 11 (25n) | uld3 | `tr-20261122-190132-18` | `603_18_general-vezax_1795374151` | kill 4:59, 2 deaths |
| 12 (10n) | uld1 | `tr-20261122-195816-11` | `603_11_general-vezax_1795377531` | kill 6:40, 1 death |
| 12 (10n) | uld2 | `tr-20261122-195654-10` | `603_10_general-vezax_1795377450` | kill 5:34, 2 deaths |
| 12 (10n) | uld3 | `tr-20261122-195856-12` | `603_12_general-vezax_1795377572` | kill 7:36, 1 death |

**Stop rules.** Polished at 25n (window 11, kills on all three pools, 2/1/2 deaths) and the 10n check
passed (window 12, 1/2/1). No failure is picked this round: it builds the user's answers to round 3's Q1
and Q2.

**Deaths by mechanic.**

| Pull | Deaths |
|---|---|
| w11 uld1 | 2 melee to Profound Darkness (4:22, 4:28) |
| w11 uld2 | 1 ranged to Shadow Crash (4:07) |
| w11 uld3 | 1 healer (3:57), 1 melee (4:39) to Shadow Crash |
| w12 uld1 | 1 healer to Shadow Crash (5:20) |
| w12 uld2 | 2 ranged to Shadow Crash (4:17, 5:20) |
| w12 uld3 | 1 ranged to Shadow Crash (3:56) |

**Verdict on round 3 (a marked main tank holds where it stands): it worked.** Window 12 marked the main
tank four times (10n uld1 3:12, uld2 1:00 and 3:12, uld3 2:20; none in window 11): `--mark` branch
`unchanged` in all four, no Berserk (26662/64112) in any of the six pulls, Vezax 9.5-13 yd from the anchor
(`--band`). The two 3:12 marks, in the Animus phase with 1 of 4 far, leeched 21 and 23 ticks (+1.3 and
+2.0 %/s on him), worse than a marked melee's 6-16: at 10-man the old rows left the whole raid inside the
floor, which this round's rows end.

**Change 1, user answer Q1 (a): the Mark only reaches ranged and healers.**
`ULDUAR_VEZAX_CAMP_RADIUS` 31.5 → 35 (`UldEncounter_Vezax.h`): rows 30.5 / 32.75 / 35 / 37.25 / 39.5, mark
arcs at 35 yd ±65.5°, exactly the navprobed candidate points above. `EnsureVezaxSlotAssignments`
(`UldEncounter_Vezax.cpp:283` before) hands out slots in two passes, healers first, so 4 healers take row
0 at 25-man (2 at 10-man) and ranged dps fill behind. 13-14 camp bots fill rows 0-3 at 25-man; only row
4's strafed slots (44.5) pass reach spell's 42.6 ceiling, row 3's widest strafe is 42.5.

`--mark` far replay on the window 11-12 traces (`vezax-r4/mark_far_replay.py`): *now* as recorded; *stood*
moves each living camp bot by its new slot minus its old one, keeping how far it stood off its slot;
*slot* puts it exactly on its new slot. Vezax and everyone else stay where they stood.

| Pull | Marks | Far now (under the minimum) | Far, new rows as stood | Far, new rows on slot |
|---|---|---|---|---|
| w11 uld1 | 8 | 10-14/9 (0) | 12-14 (0) | 14 |
| w11 uld2 | 9 | 10-14/9 (0) | 10-14 (0) | 13-14 |
| w11 uld3 | 7 | 4-13/9 (1, 0:20 on a melee) | 11-13 (0) | 13 |
| w12 uld1 | 10 | 0-5/4 (8) | 4-5 (0) | 4-5 |
| w12 uld2 | 8 | 1-5/4 (7) | 4-5 (0) | 4-5 |
| w12 uld3 | 11 | 0-5/4 (9) | 3-5 (1, 4:32) | 4-5 |

Today 2 of 24 window 11 marks and 15 of 29 window 12 marks went to melee or a tank. Healer reach replay
(`vezax-r4/heal_reach_replay.py`, 0:15-3:00, within 41.5 yd 2D of the main tank): w11 uld1 and uld2 100%
→ 100%, w11 uld3 19-54% per healer (any healer 66%) → 74-100% (any 100%), 10n 80-100% → 77-100% (any
84-100%), Vezax settling 9.5-9.9 yd north, except 9.9 yd south in w11 uld1 and uld2 (the tank between him
and the camp).

**Change 2, user answer Q2 (bug): dodge and camp points off the walkable floor.** In window 10 uld2
(`603_19_general-vezax_1795366925`) Uldyaf, Uldyaj and Uldyaq stood at (1881-1885, 181-184, 342.2) when
dodge moves sent them to (1895.95-1901.6, 190.4-210.6) at 6:22.5-6:24.8; over the next 3 s their xy moved
under 2 yd while z fell 342.2 → 321.9, then they landed at (1861.1, 170.7, -27.7). The strafe
(`TryGetVezaxDodgeSpot`, `UldEncounter_Vezax.cpp:728-768` before), the slot (`TryGetVezaxSlotPosition`
`:259`) and the mark spots (`TryGetVezaxMarkSpot` `:370`) are raw boss-relative geometry at his Z, handed
to `MoveTo` with no floor test. The collision check alone would not have caught it either: for a Player
source `PathGenerator::BuildPolyPath` takes the "can fly" shortcut whenever the start or end has no poly
nearby (`PathGenerator.cpp:277-288`), and `Map::CheckCollisionAndGetValidCoords` then skips the raycast,
settles Z on the ground below (`Map.cpp:3503-3550`) and returns true.

Fix (`UldEncounter_Vezax.{h,cpp}`, `UldActions_Vezax.cpp`): `VezaxPlaceOnFloor` runs the hazard sweep's
`CheckCollisionAndGetValidCoords` from the bot and takes its answer (a point past the navmesh edge comes
back pulled onto it), refusing a failed call or an answer more than `ULDUAR_VEZAX_FLOOR_STEP` (5 yd) above
or below the bot. The position node checks its slot before every walk (not while parked) and holds when
refused or already within tolerance of the pulled-back point; the strafe is checked, and a refused strafe
or one pulled into the blast falls back to the sweep; the side mark spot is checked and a refused one
holds; a marked melee takes the nearest southern spot that passes. The dodge sweep and the melee mark
break pass the sweep an accept callback taking only ground within 5 yd of the bot. New probe
`vezax.floor` (`on`/`edge`/`off`). Docs: `docs/raids/ulduar/vezax.md` (camp section, mark arc, crash
fallback, trace, known gap removed) and a pitfalls entry on the player shortcut in
`docs/engine/pitfalls.md` (doc only).

10/25 and normal/hard: the rows, fill order and floor check key on nothing that differs. Real players take
slots by spec as before, so the same with or without one in the raid.

**Next window should show.**

- `--band`: no camp bot's `<29.1` share near the 44-68% rows 0-1 read in windows 11-12; medians 30.5-37.5.
- `--mark`: far 13-14/9 at 25n and 4-5/4 at 10n, every mark on `heal` or `ranged` (`L`/`R`, branch
  `side`) until camp deaths take the pool under 9 / 4; no main tank mark, so no 21-23 tick leech on him.
- `vezax.slot`: healers on 0, 1, 10, 11 (25n) or 0, 10 (10n).
- No main tank death before the Animus and `--animus` near window 11 (healers still reach him); the
  heal replay predicts a drop only where Vezax settles past ~10.5 yd north.
- Ranged casts/min on the Animus near window 11's (review replay, scratchpad `vezax-r4-review/`): the
  camp still follows Vezax while the Animus lives, and the new rows put ranged dps past 38.5 yd of the
  Animus (a 30 yd spell's reach, combat reach 7) in 4-40% of samples against 0-9% today, past 44.5 (36
  yd talented) in 0-6%. The formation vetoes reach spell, so a caster short of its range talent stands
  out of range there.
- `postmortem.py --probes vezax.floor`: only `on` while he stands near his spawn. An `edge` there is a
  navmesh hole a straight check clips at, stranding the bot short of its slot: report it. No bot ever
  below z 330.
- Shadow Crash: the front rank's flight time grows 2.7 → 3.1 s; crash deaths no higher than windows
  11-12's 1-2 a pull.

**Questions.** None new.

## Round 5 (2026-11-23, build 1fa3c332c)

**Data.** Window 14 at 25n hard mode (`raidtest.py report --boss vezax --raid uld --head 1fa3c332c
--pulls 10`), one valid pull per pool, compared with window 11 (round 3 code, `--head d2874f536`, the
three kills in round 4's table). Hard mode held in all three (`--vapors`: `animus` at 3:08.2-3:08.6, no
vapor hit). Analysis scripts: session scratchpad `vezax-r5/`.

| Pool | Run | Trace | Result |
|---|---|---|---|
| uld1 | `tr-20261123-102505-17` | `603_17_general-vezax_1795429549` | wipe 4:52, 30 deaths, Animus at 16.2% |
| uld2 | `tr-20261123-102505-18` | `603_18_general-vezax_1795429556` | wipe 5:04, 30 deaths, Animus at 10.4% |
| uld3 | `tr-20261123-102445-16` | `603_16_general-vezax_1795429526` | wipe 5:00, 30 deaths, Animus at 7.5% |

**Stop rules.** Not polished: three wipes. Round 4 built answers, not a picked failure, so no repeat count.

**Deaths by mechanic** (last hit; re-raised bots die twice).

| Pull | Before the collapse | Collapse |
|---|---|---|
| uld1 | 1 Shadow Crash (3:37) | from 4:22 (main tank to Animus melee): 11 Profound Darkness, 7 Animus melee, 9 Vezax melee, 1 Shadow Crash, 1 Mark |
| uld2 | none | from 4:04: 18 Profound Darkness, 4 Animus melee, 6 Vezax melee, 1 Shadow Crash, 1 Mark |
| uld3 | 1 Shadow Crash (3:27) | from 4:23 (main tank to Animus melee): 10 Profound Darkness, 6 Animus melee, 10 Vezax melee, 3 Shadow Crash |

**Verdict on round 4 (camp past the Mark floor, floor checks): both did what they targeted, and the camp
move lost the Animus race.** Marks: every one before the collapse went to a healer or ranged, far 13-14/9
(window 11: 2 of 24 on melee or a tank). `--band` medians 31.0-37.3, `<29.1` 1-11% per bot. No death
before the Animus, crash deaths 0-1 a pull before the collapse as in windows 11-12, no bot below z 340.9.
`vezax.floor` read `edge` six times, four on marked bots' arc spots (1:01-1:02, 3:53) and two elsewhere
(3:24, 4:11), no death near any. But the Animus, killed in all three window 11 pulls, survived all three:

| `--animus` | Window 11 | Window 14 |
|---|---|---|
| Pace | 1.24-1.35 %/s, killed in 74-80 s | 1.02-1.12 %/s, 7.5-18.1% left at the collapse (76-80 s) |
| Raid / ranged | 153-165k / 82-85k/s | 126-138k / 59-64k/s |
| Ranged median distance to the Animus | 23.2-34.0 | 31.7-40.5 |
| Ranged time past reach spell's 37 yd (`past`) | 0-28%, mostly under 12% | 10-93% |
| Reach spell walks per ranged (`walks`) | 0-4 | 0-28 |
| Ranged moving share | 28-53% | 32-70% |

Melee lost 3-6k/s, the rest is ranged. Vezax at the spawn (14-25% against 17-22%), healer mana, the Mark
leech (276-657k against 402-476k) and the field share on the Animus (36-76% against 39-69%) did not
move.

**Picked: ranged dps out of reach of the Animus, the camp and reach spell deadlocked (bug, 88 of 90
deaths: the whole collapse in all three pulls).** The Animus stands ~8.5 yd off Vezax's side (median
offset ±6.8-8.3 across, −4.8 to +5.0 along the camp bearing) and its reach is 7, so reach spell's ceiling
on it is 28.5 + 1.5 + 7 = 37 yd (its walks ended at a median 37.4). The camp is
boss-relative (`TryGetVezaxSlotPosition`, `UldEncounter_Vezax.cpp:302` before), so round 4's rows put
ranged dps at 32.75-37.25 yd from Vezax, 40-47 from the Animus. Reach spell (`ACTION_HIGH`,
`ReachTargetAction::isUseful`, `ReachTargetActions.cpp:15-33`) is kept by the formation rule
(`UldDefinition_Vezax.cpp:47`), and the position node (`UldActions_Vezax.cpp:256-291`) walks the bot back
once it is 2.4 yd off its slot. uld2 Uldyah (arcane mage): position to its slot 43.1 yd from the Animus
at 3:11.7, reach spell in to 37.4 at 3:12.3, back to the slot 3:13.6, in at 3:14.6, out 3:16.3, in 3:17.3,
every ~2.5 s, casting on every other leg; `flip.move.by:reach spell <-> vezax raid position` read
2.9 / 24.9 / 4.1 a minute in window 14 and 0 in window 11. The camp section already forbids that deadlock
(its ceiling), but round 4 checked it against Vezax's reach only; round 4's review flagged the risk at
4-40% of samples.

A bug, not a position call: the strategy intends ranged on the Animus to cast at it, and the camp's own
ceiling rule forbids the deadlock. The fix stops each slot where reach spell already ends the walk, the
user's rows and fill order stand, and nobody comes nearer Vezax than the healers' row, so the Mark still
reaches only ranged and healers. None of reach spell's 320 ranged walks in window 14 ended under the 29.1
floor from Vezax (medians 32.1-36.0), so the Mark floor and the Animus ceiling do not conflict in any
traced geometry.

**Fix** (`UldEncounter_Vezax.cpp` only): `PullVezaxSlotIntoAnimusReach` (`:168`): for a non-healer slotted
bot whose current target is the live Animus, a slot farther than `GetRange("spell")` plus both live
combat reaches minus two slot tolerances (~35 yd) from it slides along the line to the Animus to that
distance, stopping where the line would cross row 0's radius (30.5) from Vezax. `TryGetVezaxSlot` (`:466`)
applies it, and the dodge strafes from the pulled slot (`:862`). New probe `vezax.reach`
(`off`/`in`/`pulled`/`floor`). Reader: `--animus` gains `past` (living time past 37 yd of the Animus) and
`walks` (accepted reach spell moves) per caster and the `vezax.reach` values written after the spawn
(tests added). Docs: `docs/raids/ulduar/vezax.md` (camp ceiling bullet, trace section, the race gap
updated). Shared code: none.

Replay on the six pulls' Vezax and Animus positions (`vezax-r5/clamp_replay.py`, round 4's slots): ranged
dps slots past 37.4 yd of the Animus 0-25% (window 11 geometry) and 18-100% (window 14) → 0-3%; slots
moved a median 0-4.7 yd, at most 7; the floor stop ends 0-6% of slot samples, 83% in uld2, where the
Animus sat south-east, and those still land inside the ceiling. Pulled slots of one file bunch along the
line to the Animus (nearest neighbour 2.3 → 0.5-0.6 yd median in the three pulls where it acts most); a
crash already covers a whole group and the leech the whole camp, so that costs nothing new.

10/25 and normal/hard: the Animus exists only in hard mode, so normal mode never pulls a slot; at 10-man
ranged dps fill rows 0-1 and are mostly inside the ceiling already. Healers and the main tank are left
alone, and the pull keys on the bot's own target, so the same with or without a real player in the raid.

**Next window should show** (`general_vezax.py --animus`, pulls that keep the main tank to the collapse):

- ranged `past` under ~12% and `walks` 0-4, as in window 11; `vezax.reach` written as `pulled` or
  `floor` by most ranged dps; `flip.move.by:reach spell <-> vezax raid position` near 0;
- ranged 80k/s+ and raid 150k/s+ on the Animus, pace 1.24 %/s or more, the Animus dead before ~80 s;
- the Mark still only on healers and ranged, far at least 9 at the 3:52 mark inside the Animus phase
  (pulled slots never nearer than 30.5 yd);
- crash deaths no higher than 1-2 a pull; a dodge from a pulled slot reads `vezax.dodge = strafe`.

If ranged still sit past the ceiling, check `vezax.reach = floor` against where the Animus stood: the
floor stop is the one case the pull leaves out of reach.

**Observations, not chased.** Healers carry the Animus as their target and take reach spell walks off row
0 too (0-4 in window 11, 0-12 in window 14, uld2 Uldyac 90% past it); their slots stay as the user chose.
The `edge` floor reads on marked bots' arc spots say the 35 yd arc is clipped more than 1 yd at some
settles; the marked bot still moves and nothing died near one.

**Questions.** None new. The fix holds the round 3 answer; if the user reads the Animus-phase pull as a
new position, the alternative is ranged dps fixed on their rows and the race lost.

## Round 6 (2026-11-23, build 523b2f60f)

**Data.** Window 15 at 25n hard mode (`raidtest.py report --boss vezax --raid uld --head 523b2f60f
--pulls 10`), one valid pull per pool, hard mode held in all three (`animus` at 3:08.4, no vapor hit),
against window 11 (`--head d2874f536`) and window 14 (round 4's table). Analysis scripts: session
scratchpad `vezax-r6/`.

| Pool | Run | Trace | Result |
|---|---|---|---|
| uld1 | `tr-20261123-121740-1` | `603_1_general-vezax_1795436349` | kill 5:43, 0 deaths, the Animus dead in 76 s |
| uld2 | `tr-20261123-121740-2` | `603_2_general-vezax_1795436352` | wipe 4:55, 30 deaths, the Animus at 24.2% |
| uld3 | `tr-20261123-121740-3` | `603_3_general-vezax_1795436349` | wipe 6:25, 30 deaths, the Animus at 3.4% at the collapse |

**Stop rules.** Not polished: two wipes. Round 5 picked ranged out of the Animus's reach and uld3 shows
it again (second round; a third stops).

**Deaths by mechanic** (last hit; re-raised bots die twice).

| Pull | Before the collapse | Collapse |
|---|---|---|
| uld1 | none | none |
| uld2 | 2 ranged to Animus melee (3:23, 3:25) and a healer to Profound Darkness after its hits (3:28), all with the Animus loose | from 3:58: 19 Profound Darkness, 2 Animus melee, 2 Vezax melee, 2 Mark, 2 Shadow Crash |
| uld3 | none | from 4:34 (main tank to Vezax melee on the first big Profound Darkness wave): 13 Profound Darkness, 10 Vezax melee, 4 Shadow Crash, 2 Animus melee, 1 Mark |

**Verdict on round 5 (slots pulled into the Animus's reach): it worked where the Animus stood beside or
ahead of him, and failed where it stood behind him**, the case its own floor stop leaves out.

| `--animus`, `geometry.py` | uld1 | uld2 | uld3 | window 11 |
|---|---|---|---|---|
| Animus from Vezax along the camp bearing | −2.4 | +5.3 | **−9.6** (he settled 14.1 yd north of the anchor) | −1.9 to +4.9 |
| Ranged `past` / `walks` | 6-33% / 0-5 | 8-40% / 0-8 | **73-91% / 22-34** | 0-28% / 0-4 |
| `vezax.reach` written | pulled 10, floor 3 | floor 10, pulled 9 | **floor 9 of 9** | - |
| `flip.move.by:reach spell <-> vezax raid position` /min (window 14) | 0.18 (2.92) | 1.03 (24.86) | **36.96** (4.07) | 0 |
| Pace, raid / ranged | 1.31 %/s, 161k / 89k | 0.87 %/s, 108k / 48k (loose) | 1.12 %/s, 138k / 61k | 1.24-1.35, 153-165k / 82-85k |
| Mark far at 3:52 | 14/9 | 12/9 | 3/9 | |

Crash deaths before the collapse 0 in all three. In uld3 the line to the Animus crosses row 0's radius
first (`PullVezaxSlotIntoAnimusReach`, `UldEncounter_Vezax.cpp:197-211`), so every ranged slot stopped
there, 40 yd from it; reach spell walked them to ~37 and the position node back, ranged median 28.2-29.5
yd from Vezax, inside the Mark floor anyway.

**Ranking.** uld2 and uld3 tie at 30.

1. *uld2: the Animus loose from +7 s (bug, fixed below).* The bear off-tank Uldyab landed the first hit
   at 3:08.77 and was its victim from 3:10.05; the warrior main tank Uldyaa, on the anchor 3.4 yd from it,
   opened with Sunder, Rend and Shockwave IMPOSSIBLE and never got it. Every redirect (Misdirection
   3:08.71, 3:09.31, Tricks 3:08.76, 3:09.28) went to Uldyaa (`VezaxAnimusRedirectAction`,
   `UldActions_Vezax.cpp:137-145`), and `vezax threat multiplier` capped dps at 90% of the main tank's
   threat (`UldMultipliers_Vezax.cpp:87-97` before), so above the bear's: 15 vetoes at +5-10 s and 23 at
   +10-15 s (uld1, main tank holding: none before +60 s). A resto shaman's heals (34k in 7.5 s) took it
   at 3:15.5 and the bear retook it at 3:16.7; demonology warlock Uldyav, vetoed at 3:17.19, cast Soul
   Fire at 3:17.46 and took it from 36 yd at 3:19.77 while Uldyaa stood 9.1-9.9 yd off it. It hopped
   through the camp (Uldyah, Uldyap dead), Heroism at +0 went on that, a healer died at 3:28, and
   Uldyaa held it only from 3:45.0. Deaths ran from +50 s, the collapse at +68 s with 41% of it left.
2. *uld3: the Animus behind Vezax (position question, Q1).* At a 14.1 yd north settle it stands 9.6 yd
   behind him from the camp; no slot both clears the Mark floor and reaches it without new positions.
   The same settle put row 0 44-48 yd from the main tank (healers within 41.5 of him 2-5% of the minute
   before the spawn), round 3's predicted cost of the user's rows.

**Also fixed, from round 5's review: healers' reach spell on the Animus (bug).** Healers switch to it
(`VezaxSaroniteAnimusTrigger`, `UldTriggers_Vezax.cpp:112-135`), and the formation rule keeps reach
spell for every ranged role (`UldDefinition_Vezax.cpp:47-48`), so a healer past 37 yd of it walked in and
the position node walked it back: uld3's four healers 12-28 walks, 89-98% of the phase past (84 healer
reach spell moves against 62 position moves), window 14 uld2 Uldyac 12 walks.

**Fix** (Vezax files only):

- `VezaxThreatMultiplier` (`UldMultipliers_Vezax.cpp:76-100`): the cap is 90% of the boss's victim's
  threat when the victim is a tank, else the main tank's as before (with the Vezax stand-down). With the
  main tank holding, as in 8 of the 9 Animus pulls since window 11, nothing changes.
- Rule `vezax healer animus reach hold` (`UldDefinition_Vezax.cpp:52`): reach spell holds for a healer
  whose target is the live Animus (`VezaxTargetingAnimus`, `UldEncounter_Vezax.cpp:776`, also used by
  the field cast hold). Slots unchanged.

Docs: `docs/raids/ulduar/vezax.md` (threat paragraph and Animus bullet, camp ceiling bullet, the
north-settle gap). 10/25 and normal/hard: the threat cap keys on whoever the boss hits, tanks recognised
by spec for a real player too; the Animus exists only in hard mode. Shared code: none.

**Next window should show.**

- Healers: `walks` 0 in `--animus`, `vezax healer animus reach hold` vetoes on healers only
  (`postmortem.py --vetoes`), the reach spell/position flip no longer fed by healers. In a north settle
  the walks were carrying healers toward the main tank (2.2k/s healing on him in uld3's phase against
  1.2k/s the minute before): a main tank death before the collapse in such a pull is this hold's cost.
- An off-tank holding the Animus: `vezax threat multiplier` vetoes on dps under its threat, the Animus on
  a tank ~98% until the collapse, no non-tank victim in its first 60 s.
- uld1-like geometry unchanged: pace 1.24 %/s or more, ranged `past` under ~33%, `walks` 0-5.
- A north settle still loses the race until Q1 is answered: `vezax.reach = floor` on every ranged dps.

**Observations, not chased.** A resto shaman's heal threat outran a bear's at +7 s from 36 yd; the
threat change does not touch healers. The warrior main tank's first Animus GCDs read IMPOSSIBLE, so the
off-tank beat him to it.

**Questions (user).**

- *Q1. When he settles 10+ yd north of the anchor the Animus stands behind him from the camp: which
  constraint gives?* (a) Ranged dps cross the Mark floor only where the floor stop would leave their slot
  past the Animus's ceiling, going to reach spell's own end point (~25 yd from him at uld3's settle). The
  floor already fails there: ranged stood a median 28.2-29.5 yd from him and far was 3/9 at the 3:52
  mark. Cost: the one Animus-phase mark (~+44 s) is drawn from the whole raid, ~44% melee or a tank.
  Gain: uld1's ranged did 89k/s on it against uld3's 61k/s, ~18% of its health over 80 s, and uld3 was
  3.4% short. (b) The main tank stands ~10 yd north of the anchor (round 3's (b)): he stays on his spawn,
  the Animus by the tank sits on the camp side, and row 0 reaches the tank in every settle; needs the
  south mark spots, the Animus melee lanes and `set behind` reworked, and navprobe. (c) Ranged arc round
  at the floor radius to where it meets the Animus's reach (~70° off the camp bearing): keeps the floor,
  but each group piles on one point ~5 yd from its own mark arc spot, so the arc must move too. (d) Keep:
  the race is lost in that settle (1 of 9 Animus pulls since window 11). *Recommended: (a)*, small and
  acting only where the floor is already lost to the flip; (b) if north settles keep starving the main
  tank of heals.

**Review.** Both causes hold. uld2 snapshots (`snap.u[7]`, spawn 3:08.43): the Animus's victim was a
fury warrior at +0.1 s, the bear from +1.1 s, the resto shaman at +7.1 s, the bear at +8.1 s, Uldyav at
+11.3 s, then three more ranged until Uldyaa at +36.6 s; all four redirects went to Uldyaa at +0.3-0.9 s;
`vezax threat multiplier` rows 15 at +5-10 s and 23 at +10-15 s, the first at +7.8 s (+8.8 s on ranged).
In uld1 and uld3 the main tank was the victim of Vezax and of the Animus from +1.1 s, so the cap there is
as before. uld3 healers: 84 reach spell moves (12-28 each) against 63 position moves. No size, mode or
player branch: a real tank counts as the holder by spec. `pb-check` passes.

- Review fix: the findings doc's "+5 s" first cap was the bin start; now +9 s, and the cap sentence
  reworded.
- Left: redirects still go to the main tank while the off-tank holds the Animus, so they raise his
  threat and not the cap dps are held under; who they feed follows who tanks it, a tactic. While a
  non-tank holds it the cap falls back to the main tank's, which can sit above the holding tank's; the
  bear retook it in 1 s in uld2.

## Round 7 (2026-11-23, build 8bc2a82d3)

**Data.** Window 17 at 25n hard mode (`raidtest.py report --boss vezax --raid uld --head 8bc2a82d3
--pulls 10`), one valid pull per pool, hard mode held in all three (`animus` at 3:08.4-3:08.5, no vapor
hit), against window 15 (round 5, `--head 523b2f60f`) and window 11's kills (`--head d2874f536`).
Analysis scripts: session scratchpad `vezax-r7/` (`compare.py` runs the table below over windows 11-17).

| Pool | Run | Trace | Result |
|---|---|---|---|
| uld1 | `tr-20261123-143850-1` | `603_1_general-vezax_1795444796` | wipe 5:01, 31 deaths, the Animus at 23.8% at the collapse |
| uld2 | `tr-20261123-143850-2` | `603_2_general-vezax_1795444791` | wipe 4:54, 30 deaths, the Animus at 21.7% |
| uld3 | `tr-20261123-143850-3` | `603_3_general-vezax_1795444798` | wipe 5:00, 30 deaths, the Animus at 19.0% |

**Stop rules.** Not polished: three wipes. Round 6 fixed two new failures, so no repeat count.

**Deaths by mechanic** (last hit; re-raised bots die twice).

| Pull | Before the collapse | Collapse |
|---|---|---|
| uld1 | 3 Mark leech and 1 Profound Darkness at 3:58-3:59 (the 3:52 mark, below) | from 4:20: 18 Profound Darkness, 5 Vezax melee, 3 Mark, 1 Animus melee |
| uld2 | 1 Shadow Crash (3:37), 1 ranged to Animus melee with it loose (3:45) | from 4:04: 18 Profound Darkness, 4 Animus melee, 4 Mark, 2 Vezax melee |
| uld3 | none | from 4:17.96 (main tank to Vezax melee): 11 Profound Darkness, 9 Vezax melee, 9 Animus melee, 1 Mark |

**Verdict on round 6 (threat cap on the holder, healers' reach spell held on the Animus).**

- *Threat cap on the holder: never exercised.* No tank but the main tank was the victim of Vezax or the
  Animus in any pull (`vezax-r7/holder.py`), so the cap ran exactly as round 5's. Not a cause.
- *Healer hold: did what it targeted, and its named cost showed up.* Healer `walks` 0 in all three,
  `vezax healer animus reach hold` vetoes on healers only (20/19/24, all reach spell). uld3 settled
  14.0 yd north as in window 15 (Animus 9.5 behind him from the camp), and the main tank got 2.4k/s of
  healing over the first 69 s against 3.6k/s in window 15 (`vezax-r7/mtheals.py`; one healer landed
  none on him, another 28 heals against 64), died at +69.5 s against +85.9 s, and the collapse came 8 s
  earlier. Total healer output fell too (11.4k/s against 12.8k/s), so the walks it stopped were not
  costing heals. In uld1 and uld2 his healing did not drop (3.3k and 2.4k/s against 3.0k and 2.3k/s).
- uld1-like geometry did not occur; uld3's `vezax.reach` read `floor` for 9 of 9 ranged dps.

**What regressed.** Window 15's kill was the one normal settle in that window; window 17 had none.

| `compare.py`, Animus phase | Window 11 kills | w15 uld1 / uld2 / uld3 | w17 uld1 / uld2 / uld3 |
|---|---|---|---|
| Pace, %/s | 1.24-1.35 | 1.31 / 0.87 / 1.12 | 1.00 / 0.98 / 1.04 |
| Raid / ranged, k/s | 153-165 / 82-85 | 161/89, 108/48, 138/61 | 124/57, 121/51, 129/53 |
| Vezax from the anchor, median | 9.5-9.9 | 9.9 / 8.9 / 14.1 | 12.6 / 15.7 / 14.0 |
| Animus along the camp bearing | −1.9 to +4.9 | −2.4 / +5.3 / −9.6 | −8.0 then +0.6 / −6.2 then −6.7 / −9.5 |
| Ranged `past`, median | 0-6% | 19 / 22 / 82% | 69 / 48 / 79% |
| Healers to the main tank, 2D median | 23-40 | 36-40 / 36-40 / 43-45 | 41-43 / 41-46 / 44-45 |
| Healer `walks` | 6-10 | 4 / 2 / 84 | 0 / 0 / 0 |
| Main tank death | none | - / +86 / +86 s | +88 / +80 / +70 s |
| Non-tank Animus victim | 1% | 1 / 39 / 1% | 1 / 29 / 11% (after the main tank died) |

- *North settles in all three.* uld1 left the opener 12.6 yd north (window 15: 7.7, then 9.9), the
  Animus 8 yd behind him for the first 30 s (ranged `past` 97-100%), then beside him; uld3 14.0 as in
  window 15. The opener walk sets this: the tank runs from the drop through Vezax to the anchor and he
  stops 7.7-14 yd north of it. Not a round 6 effect; the pending Q1.
- *uld2: the main tank chased the loose Animus off the anchor (bug, fixed below).* A shadow priest
  (Uldyao) took it at 3:23.57 (+15 s) with the warrior main tank Uldyaa holding: `vezax threat
  multiplier` held him from 3:17.88, and his Vampiric Touch, Shadow Word: Pain and Devouring Plague in a
  field then did 90.8k in 5.5 s (16.5k/s, `snap.u[11]`) with no cast of his. Uldyaa ran `reach melee` from
  3:24.5 and `charge` at 3:26.6 to (1857.2, 101.4), 21 yd north, back past Vezax at 3:32, and Vezax went
  from 9.9 yd north of the anchor to 15.7 by 3:39 and stayed there: ranged `past` 24% → 55-62%, healers
  within 41.5 of the main tank 68-100% → 0-15%. Window 15's uld2 chase reached 37 yd and dragged Vezax to
  18.8 before he came back to 8.4. The findings doc already says the main tank stays on the anchor and the
  victim brings the Animus to him; `reach melee` and charges were never held for him (the formation keeps
  Reach for the main tank).
- *uld3: the healer hold's cost*, above.
- *uld1's 3:52 mark* killed three of the marked bot's group (Uldxas, Uldxav, Uldxab at 3:58-3:59): the
  crash at 3:54.4 strafed group R west along the marked R bot's own arc, and it took until 3:58.5 to get
  17 yd clear. Window 15's same mark: 30 ticks, no deaths. Not chased.

**Picked: round 6's healer hold (regression) and the main tank's chase (independent, small).**

**Fix** (Vezax files only):

- Rule `vezax healer animus reach hold` removed (`UldDefinition_Vezax.cpp`): healers' reach spell walks
  onto the Animus come back, the only thing that carries heals to the main tank in a north settle.
- Rule `vezax main tank anchor hold` (`UldDefinition_Vezax.cpp:52`, predicate `VezaxAnimusOffAnchor`,
  `UldEncounter_Vezax.cpp:782`): for the main tank whose target is the live Animus standing farther from
  the anchor than his melee range plus `ULDUAR_VEZAX_TANK_SLOT_TOLERANCE`, `reach melee` and the
  `Charge` family hold. The Animus spawns by the anchor and the bring-back walks its victim to within
  3 yd of him, so it comes back inside that range and he can step to it.

Docs: `docs/raids/ulduar/vezax.md` (Animus ceiling bullet, threat bullet with the DoT leak, bring-back
bullet, settle range, north settle named). 10/25 and normal/hard: the Animus exists only in hard mode;
the rule keys on the main tank's own target, a real player tanking runs no rule. Shared code: none.

**Next window should show.**

- `vezax main tank anchor hold` vetoes on the main tank only, only with the Animus loose; while alive the
  main tank within 3 yd of the anchor through the Animus phase; a loose Animus leaves Vezax's settle where
  it was.
- In a north settle, healer `walks` back to window 15's 12-28 each and the main tank healed ~3.5k/s, alive
  at the collapse; in a normal settle walks stay near 0-4.
- Pace in a normal settle 1.24 %/s or more, as window 15's uld1; a north settle still loses the race
  until Q1 is answered.

**Next candidate (bug): DoTs leak past the threat cap.** The cap zeroes new casts only; DoTs already on
the Animus keep ticking (uld2 above: +40% of the main tank's threat in 5.7 s). Hold DoT application
where its remaining ticks would carry the bot past the cap, or count them toward it. Only uld2 (roster B,
warrior main tank) has lost the Animus early, in windows 15 and 17.

**Questions (user).** Q1 (north settle, queued with recommendation (a)) gets window 17's data: all
three pulls had one, two from the opener (12.6 and 14.0 yd), against one in window 15. With the healer
walks back, healers cross the Mark floor in a north settle as before round 6; Q1 (b) (main tank ~10 yd
north of the anchor) would end both the ranged and the healer reach problem.

**Review.** Both causes hold. w17 uld2 (`603_2_general-vezax_1795444791`, `vezax-r7-review/chase.py`,
`vezaxdrift.py`): Uldyaa passed 1.9 yd from Vezax's centre going out (3:25.8) and 2.4 coming back
(3:32.8), his victim throughout; Vezax moved only after the second, 9.9 → 15.7 yd north over 3:32.7-3:39.2.
Under the hold only the first reach steps fire (the Animus 7.8-12 yd from the anchor), ending 3-7 yd north
of it, and the returned Animus stood 8.4 yd from him (w15 uld2: 8.1), inside his 9.8 yd melee range.
`mtheals.py` re-run: main tank healed 3576/s (w15 uld3, 132 heals) against 2368/s (w17 uld3, 74) over
69 s. No size, mode or player branch: a real player runs no rule, and one the Animus hits walks it in
as the bot victims do. `pb-check` passes.

- Review fix: Intervene escaped the hold. It is a `CastProtectSpellAction` (`protect party member` at
  `ACTION_EMERGENCY`: a non-tank under 30% whose attacker is within 30 yd of it), not `Family::Charge`;
  in window 14 uld2 (`603_18_general-vezax_1795429556`, `vezax-r7-review/intervene.py`) Uldyaa
  intervened onto the Animus's victim Uldyav at 4:33.6, from 14.7 to 36.1 yd off the anchor, right
  after a charge. `intervene` joins the hold's names; findings doc updated.
- Left: the hold measures the Animus from the anchor, not from the main tank, who may park up to 6 yd
  off it (twice his tolerance), so a returned Animus just out of his melee range on his far side from the
  anchor gets no step from him. Not seen: returns stood 8.1-8.4 yd from him with him on the anchor.

## Round 8 (2026-11-23, build 99188e0f1)

**Data.** Window 19 at 25n hard mode (`raidtest.py report --boss vezax --raid uld --head 99188e0f1
--pulls 10`), one valid pull per pool, hard mode held in all three (`animus` at 3:08.39-3:08.50, no vapor
hit). Analysis scripts: session scratchpad `vezax-r8/` (`compare.py` over windows 11-19, `settle.py`,
`mtheals.py`, `mtanchor.py`, `crash.py`, `crashpaths.py`, `replay.py`, `swd.py`).

| Pool | Run | Trace | Result |
|---|---|---|---|
| uld1 | `tr-20261123-163804-1` | `603_1_general-vezax_1795451940` | kill 6:25, 3 deaths, the Animus dead in 79.5 s |
| uld2 | `tr-20261123-163804-2` | `603_2_general-vezax_1795451942` | wipe 5:10, 30 deaths, the Animus at 12.3% at the collapse |
| uld3 | `tr-20261123-163804-3` | `603_3_general-vezax_1795451946` | kill 5:31, 1 death, the Animus dead in 77.9 s |

**Stop rules.** Not polished, one pool short: uld1 and uld3 are hard-mode kills with 3 and 1 deaths,
uld2 wiped. Round 7 picked new failures, so no repeat count.

**Settle per pull** (Vezax from the anchor, `settle.py`, `mtanchor.py`): uld1 9.3 from the opener, 9.0-10.1
through the Animus phase (normal); uld2 12.8 from the opener, 12.4-12.8 to the collapse (north); uld3
7.7, then 9.9 (normal).

**Deaths by mechanic** (last hit; re-raised bots die twice).

| Pull | Deaths |
|---|---|
| uld1 | 3 Shadow Crash in the Animus phase: Uldxaj and Uldxaq at 3:47.6, the marked Uldxas at 3:57.5 |
| uld2 | collapse from 4:20.8 (main tank to Animus melee at +72.3 s): 16 Profound Darkness, 8 Vezax melee, 6 Animus melee |
| uld3 | 1 own Shadow Word: Death backlash (4:26.6, 20,220, 0.2 s after the Animus died) |

**Verdict on round 7 (main tank anchor hold, healer walks back).**

- *Anchor hold: did what it targeted.* Vetoes 0 / 9 / 0. uld2's nine fell at 4:22.4-4:44.5 on the
  re-raised main tank, with the Animus loose on ranged 13.6-21.6 yd off the anchor after his first death;
  none while he held it. Alive, every main tank stood on the anchor (0.0 yd) through the phase, and Vezax
  kept his settle.
- *Healer walks: back, but they did not feed the main tank in the north settle.* uld2's healers walked
  16/6/6/9 times (normal settles: 9 and 3 in all), stood a median 42 yd off him, and he got 2.25k/s from
  everyone (1.5k/s from healers) over the first 69 s, against 2.4k/s in w17 uld2 and 3.6k/s for the bear
  in w15 uld3; he died at +72.3 s, 8 s before the collapse.
- *Pace:* normal settles 1.26 and 1.28 %/s, dead in 79.5 and 77.9 s (window 11: 1.24-1.35); uld2's north
  settle 1.10 %/s with ranged `past` 51-81% and `vezax.reach = floor` on 10 bots.
- *DoT leak past the threat cap (round 7's candidate): not borne out.* uld2's warrior main tank held the
  Animus from +1.0 s to his death (tank 84.8%, every non-tank sample after it); `vezax threat multiplier`
  vetoed 198 actions in the phase (ranged 129, melee 69; uld1 13, uld3 1) and the cap held. Not built.

**Ranking.**

1. *uld2's collapse, 30 deaths: a north settle from the opener* (Q1, pending, not built). Every north
   settle in windows 15-19 lost the race (w15 uld3, w17 all three, w19 uld2: 12.6-15.7 yd, four from the
   opener walk, one from the chase round 7 held), every normal settle with the main tank holding the
   Animus killed it (w15 uld1, w19 uld1, w19 uld3: 76-80 s).
2. *Walks into a landing Shadow Crash, 3 deaths (uld1): bug, fixed below.*
3. *Shadow Word: Death backlash, 1 death (uld3): bug, small and independent, fixed below.*

**Picked: walks into a landing Shadow Crash.** The crash cast at 3:44.39 on healer Uldxac (L, row 0)
landed at 3:47.49 on (1864.6, 119.8). Uldxaq's and Uldxaj's slots, pulled toward the Animus to the
south-west, sat 4.3 and 5.5 yd west of the impact, past their group's inner file, so the 15 yd L strafe
east ended 10.7 and 10.2 yd from it. The dodge stops issuing past 10 yd (`UldActions_Vezax.cpp:92`), and
reach spell toward the Animus walked them back to 8.7 and 8.2 (3:46.84, 3:47.21, 3:47.41): dead at 9.84
and 9.95 yd from the centre. Uldxas, marked at 3:52.68 and on his way to his arc spot when the 3:54.39
crash took his position, dodged to ~12 yd, then his mark action, which releases the dodge's walk once
the bot is past 10 yd (`:58-62`), walked him back across the impact toward the spot 23.5 yd beyond it
(3:56.35, 3:56.83): dead at 9.86 yd. The core hits a player within the radius of a creature's DEST area
spell 3D from its centre, no reach added (`Spell.cpp:9507-9524`). Position and soak declined only a
destination inside the blast (`UldActions_Vezax.cpp:176-180`, `:265-272` before), never a path through
it; reach spell and the mark walk not at all.

Across windows 11-19 (`crashpaths.py`, `replay.py`) such walks killed 7 from the Animus spawn on, before
any collapse: w11 uld2 Uldyah (a position walk through it, that kill's only death), w11 uld3 Uldzae and Uldzax
(mark walks, both of that kill's deaths), w17 uld2 Uldyaj (reach spell to 9 yd of it before his dodge),
and w19 uld1's three. 0-13 such walks per Animus phase, most landing clear.

**Fix** (Vezax files only):

- `VezaxWalkMeetsShadowCrash` (`UldEncounter_Vezax.cpp:635`): while a crash flies, a walk that ends within
  `ULDUAR_VEZAX_SHADOW_CRASH_WALK_CLEARANCE` (11, the dodge trigger's own reach, which now uses it) of the
  impact, or passes within it nearer than the bot stands, waits for the landing. The position, soak and
  mark actions decline on it, the first two in place of their destination-only test.
- Rule `vezax shadow crash walk hold` (`UldDefinition_Vezax.cpp:55`, predicate
  `VezaxReachSpellMeetsShadowCrash`): reach spell holds when its walk, measured to where it would stop
  (spell range plus both reaches from the target), meets the crash.
- Rule `vezax shadow word: death hold` (`UldDefinition_Vezax.cpp:57`, `VezaxCarriesProfoundDarkness`):
  Shadow Word: Death holds while the bot carries Profound Darkness. `swd.py`: 5 backlashes in windows
  11-19, all at 4:23-4:44 just after the Animus died, 12.8-21.9k on ~22k-health shadow priests, one death.
- Reader: `--crash` gains `in`, the bots that took an accepted walk other than the dodge toward the landing
  impact by the code's own test, and a summary of the movers and the hit bots that took one (test added).
  w19 uld1 reads 11 walks (reach spell 5, position 4, mark 2) and 3 of 3 hits on such bots.

Replay on the walks as issued (`replay.py`): the hold stops 0-13 a pull in the Animus phase, each for at
most the rest of a ~3 s flight, 1-28 s of bot time a pull against ~14 casters over 80 s, and it stops a
walk for each of the 7 deaths. Docs: `docs/raids/ulduar/vezax.md` (Shadow Crash walk hold, Shadow Word:
Death hold, `--crash` reader, the healer-walk bullet, the north settle tally).

10/25 and normal/hard: crashes come in every mode; Profound Darkness only in hard mode, so the Shadow Word:
Death hold never fires in normal. Bots only, keyed on the crash and the bot's own walk or aura, so the same
with or without a real player. Shared code: none.

**Next window should show.**

- `--crash`: `in` 0 for reach spell, position, soak and mark walks (the melee spot and bring-back walks are
  not gated, and the dodge is not counted); no hit on a bot that walked toward the impact; no Animus-phase
  crash death in a pull that keeps its main tank.
- `postmortem.py --vetoes`: `vezax shadow crash walk hold -> reach spell` only inside crash flights, a few
  to ~15 a pull; `vezax shadow word: death hold` only on shadow priests, only after the spawn. No 32409
  damage after the spawn.
- Normal settles still at 1.24 %/s or more.
- A north settle still loses until Q1 is answered.

**Observation, not chased.** The warrior main tank (roster B) capped the dps on the Animus far more than
the DK and the bear (198 threat vetoes in the phase against 13 and 1).

**Questions (user).** Q1 (north settle) stays queued with recommendation (a). Window 19 adds a fifth
north settle of five lost: uld2, 12.8 yd from the opener, the Animus at 12.3%, ranged 51-81% past its
ceiling, healers 42 yd from the main tank. Until it is answered a window passes only when all three pools
happen to settle normally.

**Review.** Both causes hold. w19 uld1 (`vezax-r8/crash.py`): Uldxaq's and Uldxaj's strafes ended 10.7 and
10.2 yd from the 3:47.49 impact, reach spell moves at 3:46.84-3:47.41 took them to 8.7 and 8.2, and the
samples around the hit put them 9.6-10.0 yd 2D from it; Uldxas's dodge ran 3:54.47-3:56.31 through repeated `wait`s to 9.9 yd, then mark
moves at 3:56.35, 3:56.83 and 3:57.42 alternated with dodges at the 10 yd edge until he died at 9.9. The
core's DEST check (`Spell.cpp:9507-9524`) is 3D with no reach for a creature caster, and players carry
63420 (aura records, 180 s) with the 32409 backlash shadow. No size, mode or player branch. `pb-check`
passes.

- Review fix: a declined walk is not a wait. A failed action hands the tick to the next one
  (`Engine.cpp:299-319`), so a marked bot whose mark walk met the crash took the next mover instead:
  position, soak, the Animus melee spot or reach. Replayed on windows 11-19
  (`vezax-r8-review/held_mark_fallthrough.py`), 9 mark walks are held; in 5 the slot walk passes the
  test and would have walked the marked bot 15-27 yd back toward its group's slot with 1.7-2.0 s of
  flight left, 2 were melee or a tank with no slot. Rule `vezax mark walk wait` (`OwnMovement`, attacks
  kept, the dodge the only mover passed, predicate `VezaxMarkWalkMeetsShadowCrash`) now holds every
  walk of a marked bot that is not his victim while its mark walk meets the crash; the mark action's
  own decline went with it. Next window: its vetoes only on marked bots inside crash flights, no
  position, soak, melee spot or reach move from a marked bot inside one.
- Review fix: the new reader docstring, the docstring addition on `crashes` and the fixture comment in
  the test went (comment rule).
- Left: the strafe still counts as clear at 10 yd (`TryGetVezaxDodgeSpot`), so a pulled slot ends
  10.2-10.7 yd out with no margin; the hold keeps such a bot there, and a stricter test sends more of
  the camp to the scattered search. A walk already under way when a crash is cast keeps going; only the
  dodge at 10 yd stops it. Pre-existing: the position node walks a marked bot back toward its slot each
  time it stands within 3 yd of its mark spot (10-13 position moves from marked bots per w19 pull), cut
  off by the mark node after ~3 yd.

**User answer (2026-11-23):** Q1: yes, option (a). In a north settle a ranged dps bot whose pulled slot
would stay out of the Animus's reach goes to where reach spell would end its walk toward it, crossing the
Mark floor; only in that case and only while it targets the Animus; healers keep their slots.

## Round 9 (2026-11-23, build 90400b39c)

**Data.** Window 21 at 25n hard mode (`raidtest.py report --boss vezax --raid uld --head 90400b39c
--pulls 10`), one valid pull per pool, hard mode held in all three (`animus` at 3:08.52-3:08.58, no vapor
hit). Analysis scripts: session scratchpad `vezax-r9/` (`compare.py` over windows 11-21, `settle.py`,
`shift.py`, `outside.py`, `lastmove.py`, `fields.py`, `holds.py`, `flights.py`, `replay_cross.py`).

| Pool | Run | Trace | Result |
|---|---|---|---|
| uld1 | `tr-20261123-183252-1` | `603_1_general-vezax_1795458835` | kill 5:43, 0 deaths, the Animus dead in 81 s |
| uld2 | `tr-20261123-183252-2` | `603_2_general-vezax_1795458836` | kill 5:37, 0 deaths, the Animus dead in 82 s |
| uld3 | `tr-20261123-183252-3` | `603_3_general-vezax_1795458839` | wipe 4:37, 29 deaths, the Animus at 71.6% at the collapse (3:30.8) |

**Stop rules.** Not polished, one pool short as in window 19: uld1 and uld2 are hard-mode kills with 0
deaths, uld3 wiped. Round 8 picked crash walk-ins; this round's failure is new, so no repeat count.

**Settle per pull** (Vezax from the anchor, `settle.py`, `shift.py`): all three normal before the spawn,
9.9 yd (uld1 (-3.9, 9.1), uld2 (0, 9.9), uld3 (3.4, 9.3) from the anchor). At the spawn he swings round
the main tank on the anchor in every pull, 3:14.5-3:19.8: uld1 and uld2 sideways to (7.4, 5.6) and
(7.5, 6.5), then (-8.7, 4.8) and (-9.6, 2.6) by 3:45; uld3 to (9.5, -3.0), 12.3 yd straight away from the
camp, with the Animus at (4.7, 4.4), 7.1 yd ahead of him toward the camp. No north settle.

**Deaths by mechanic** (last hit; re-raised bots die twice).

| Pull | Deaths |
|---|---|
| uld1 | none |
| uld2 | none |
| uld3 | 23 Shadow Crash: 5 camp at 3:28.86, 6 at 3:38.95, 3 at 3:49.06 (two of them re-raised), then with the camp gone 6 melee at 4:05.09 and 2 melee and the main tank at 4:15.67; 1 Profound Darkness (Uldzak 3:30.58, hit by the 3:28 crash); 2 Animus melee, 3 Vezax melee from 4:17 |

**Verdict on round 8 (crash walk hold, Shadow Word: Death hold): both did what they targeted.**

- `--crash` `in`: 0 / 0 / 2, the two in uld3 from `vezax animus melee spot action` (not gated by design);
  0 of 32 hits on a bot that took a walk toward the impact. uld1 and uld2 took no crash hit at all (31 and
  30 crashes).
- `vezax shadow crash walk hold`: 2 / 4 / 7 vetoes, all on reach spell, every one inside a crash flight
  (`flights.py`); `vezax mark walk wait`: 0 in all three (no marked bot's walk met a crash).
- No Shadow Crash death in a pull that kept its main tank through a dodging camp; uld3's 23 came from the
  gate lockout below, with the walk test holding (`in` 0 for gated walks).
- `vezax shadow word: death hold`: 2 / 1 / 0 vetoes, only shadow priests (Uldxao, Uldyao), only after the
  spawn (4:28-5:42); no 32409 damage in any pull (window 19: 5 backlashes, 1 death).
- Pace in the normal settles 1.24 and 1.22 %/s (window 11: 1.24-1.35); uld3 1.29 %/s until the collapse.

**Picked: the camp shut out of the formation gate (bug, 29 of 29 deaths).** `VezaxFormationActive`
(`UldEncounter_Vezax.cpp:246-266` before) wants a bot within 45 yd 2D of Vezax; outside it the slot
(`VezaxRaidPositionTrigger`, `UldTriggers_Vezax.cpp:256`) and the dodge (`VezaxShadowCrashDodgeTrigger`,
`:71-83`) never fire. As he swung away (`shift.py`), every camp bot's last move before it went
`outside` was a soak walk (`vezax shadow crash soak action`, forced) to the 3:03.02 crash's field at the
old camp spot, 46.9-48.7 yd from him (`lastmove.py`; Uldzar 3:17.62 to Uldzaq 3:22.34), and
`vezax.formation` read `outside` for 12 of 13 at 3:19.1-3:23.7. Reach spell on the Animus (stop 37 yd
from it) then stacked them on one point, (1851.54, 122.74), 37.4 yd from it and 45.6 from him, and
nothing walked them back: 203 bot-s shut out while alive (`outside.py`), against 0-28 in every other pull
of windows 11-21 (brief knockbacks; w14 uld1's 14 bots ~2 s each at 3:16.6, just after the spawn). The
crash cast at 3:24.12 on Uldzad landed on that point at 3:28.68 and hit the 12 shut out, none of
them dodging; Uldzap, the one camp bot still inside the gate, strafed at 3:25.40 and lived, then
soaked out at 3:33.99 and died at 3:38.95. The crashes at 3:38.77 and 3:48.86 landed on the same point and took the rest, and
with no ranged left past his 17.1 yd skip they then went to the melee with ~0.4 s of flight (dodges
issued 4:04.67-4:04.89 for 4:05.09).

The doc's own rule says the gate holds only while it contains every stop a generic mover can leave a
bot at; the Animus phase breaks that twice over (his swing, and reach spell measured from the Animus).

**Fix** (Vezax files only):

- `ReadVezaxFormation` (`UldEncounter_Vezax.cpp:229`): a bot inside the gate in combat joins
  `formationJoined` (`ObsGuidSet`, probe `vezax.joined`), and while he stays in combat a joined bot past
  45 yd reads `kept`, which `VezaxFormationActive` counts as open. The height band still shuts it; the
  per-bot reset (`ResetVezaxEncounterState`) and the tracker's instance reset clear it. Bots that never
  entered keep today's 45 yd gate, so the doc's wall concern holds: a joined bot stands on the hall
  floor, and its slot, strafe and mark spots are floor-checked from there.
- User answer Q1 (a): `PullVezaxSlotIntoAnimusReach` (`:168`) no longer stops at row 0's radius; a
  ranged dps targeting the live Animus always goes the full step to two slot tolerances inside reach
  spell's stop on it. `vezax.reach` reads `cross` where the slot ends inside row 0's radius (was `floor`
  where the radius stopped it). The floor stop only ever acted when it left the slot out of reach, so
  "only in that case" is exactly where it used to fire; healers return before the pull as before.

Replay (`replay_cross.py`, windows 15-21, Vezax and the Animus where they stood, slots exact): ranged dps
slots past the Animus's 37 yd ceiling 1-100% (north settles 32-100%) → 0% in every pull; crossed slots a
median 25-30 yd from him, under 20 only in w17 uld3 and w19 uld2 at 4:21-4:25 with the main tank dead
and the Animus loose. Mark far pool at in-phase marks: unchanged (13-14) in normal settles, 13-14 → 4 in
north settles (w15 uld3, w17 all three, w19 uld2), so those marks draw from the whole raid.

Reader: `--band` gains `kept` (share of the pull the latch held the gate) and `out s` (living time shut
out after joining), and its `on` and `--field`'s gate count `kept` as open (`formation_spans`, test
added). Docs: `docs/raids/ulduar/vezax.md` (gate latch paragraph, Animus ceiling bullet, floor bullet,
trace keys, `--band`, north settle gap).

10/25 and normal/hard: the gate and its latch run in every mode; the Animus pull only in hard mode.
Bots only, keyed on the bot's own position and target, so the same with or without a real player.
Shared code: none.

**Next window should show.**

- `--band` `out s` 0 for every camp bot (a death or the height band aside); `vezax.formation = kept`
  rows where he swings away at the spawn or a crash knocks a bot back, followed by `vezax raid position
  action` walks back to the slot; `vezax.joined = 1` for every slotted bot early in the pull.
- `--crash`: no Animus-phase crash with 0 dodgers in both blocks while the camp lives; Animus-phase crash
  deaths 0-1 a pull.
- In a north settle: `vezax.reach = cross` on most ranged dps, ranged `past` under ~33% and `walks` 0-5
  as in normal settles, `flip.move.by:reach spell <-> vezax raid position` near 0, pace 1.22 %/s or more.
  Its cost: in-phase marks with far 4/9 and on melee or a tank about 4 times in 9; healers still 42+ yd
  from the main tank there (the doc's north settle gap).
- Normal settles unchanged: 1.22-1.28 %/s, 0-3 deaths.

**Questions.** None new. The north settle's healer reach to the main tank stays open (Q1 (b) was the
alternative that would also fix it).

**Review.** Both causes hold. uld3 (`vezax-r9-review/lockout.py`): Vezax went from (+3.4, +9.3) off the
anchor at 3:14 to (+9.5, −3.0) at 3:20; each of the 12 shut-out bots' last move before `outside` was a
soak walk to the field at (1850.7, 125.7), 48.7 yd from where he stopped, most then reach spell to
(1851.5, 122.7), 45.6 from him and 37.4 from the Animus; crash-killed camp bots 5 / 6 / 3 at
3:28.9 / 3:38.9 / 3:49.1, 12 distinct. `--band` `out s` sums to 203 s. `replay_cross.py` re-run on
w15 uld3, w17 uld3, w21 uld3: past the ceiling 100 / 100 / 3% → 0%, far at in-phase marks 13 → 4 in
the two north settles. No size, mode or player branch, no shared code, no comments. `pb-check` passes.

- Review fix: Uldzap dodged the 3:28.68 crash (`--crash` 1 of 13), so "0 of 13 dodging" above is now the
  12 shut out none dodging; the findings doc's "the one bot still inside dodged and lived" now says a
  soak walk took it out too (dead at 3:38.95, one of the 12). One overlong doc line rewrapped.
- Left: crossed slots go two slot tolerances (2.4 yd) past reach spell's own stop, the margin round 5's
  pull keeps so a parked bot stays inside the ceiling. With the Animus loose after the main tank died a
  crossed slot can sit at his feet (w17 uld3 replay: down to 4.2 yd at 4:21-4:25, 105 of 2740 samples
  under 20); collapse only.

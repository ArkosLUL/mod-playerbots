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

## Round 10 (2026-11-23, build b9dd70fcc)

**Data.** Window 22a at 25n hard mode (`raidtest.py report --boss vezax --raid uld --head b9dd70fcc
--pulls 10`), one valid pull per pool, hard mode held in all three (`animus` at 3:08.3-3:08.8, no vapor
hit), against window 21 (round 8, `--head 90400b39c`) and window 19 (`--head 99188e0f1`). Only Vezax r9
and Faction Champions changed between the two builds. Analysis scripts: session scratchpad `vezax-r10/`
(`compare.py` over windows 11-22, `settle.py`, `geomgroups.py`, `pastwhy.py`, `pastwhy2.py`, `perbot.py`,
`threatvetoes.py`, `markdetail.py`, `markedges.py`, `markedmovers.py`, `edges.py`, `strafe_replay.py`,
`replay_cross.py`, `magemana.py`).

| Pool | Run | Trace | Result |
|---|---|---|---|
| uld1 | `tr-20261123-194527-1` | `603_1_general-vezax_1795463232` | wipe 4:42, 31 deaths, the Animus at 34.2% at the collapse (4:10.1) |
| uld2 | `tr-20261123-194527-2` | `603_2_general-vezax_1795463202` | kill 5:24, 0 deaths, the Animus dead in 75 s |
| uld3 | `tr-20261123-194527-3` | `603_3_general-vezax_1795463228` | wipe 5:00, 30 deaths, the Animus at 7.4% at the collapse (4:32.5) |

**Stop rules.** Not polished: one kill. Both failures picked below are new causes. The symptom of the
second, ranged dps past the Animus's ceiling, was round 5's pick (slots) and round 9's built answer (north
settles); if the far group still sits past it next window, that is the third round on it and the lane
stops there.

**Settle per pull** (Vezax from the anchor, `settle.py`, `geomgroups.py`): uld1 7.7 then 9.9 yd north
(normal), swung east to (+9.9, +0.2) at the spawn, the Animus 7.1 yd west of him; uld2 8.3 then 9.9
(normal), swung west to (−3.8, +9.1), the Animus 7.5 east; uld3 10.2 north, swung east to (+6.5, +7.8),
the Animus 9.0 west and 2.6 behind him. No north settle in the Animus phase.

**Deaths by mechanic** (last hit; re-raised bots die twice).

| Pull | Before the collapse | Collapse |
|---|---|---|
| uld1 | 2 ranged to Mark leech at 1:08.8-1:09.8 (the 1:00.5 mark, 75 ticks); 4 to Mark leech at 3:57.4-3:58.3 (the 3:52.8 mark, 32 ticks: Uldxaq, Uldxae, and Uldxau and Uldxaw re-raised from the first) | from 4:06.6 (DK main tank to Profound Darkness at +57.8 s): 10 Animus melee, 7 Profound Darkness, 6 Vezax melee, 2 Mark |
| uld2 | none | none |
| uld3 | none | from 4:23.5 (bear main tank to Vezax melee at +75.2 s): 12 Profound Darkness, 9 Vezax melee, 5 Animus melee, 4 Mark (the 4:32.6 mark on a melee, far 2/9) |

**Verdict on round 9 (formation latch, ranged crossing the Mark floor): both did what they were built
for, and neither caused a wipe.**

- *Latch.* `--band` `out s` 0.0 for every camp bot in all three (w21 uld3: 203 bot-s); `kept` 0% but
  Uldxaw 2%; `vezax.formation = kept` 4 rows in uld1 (Uldxag 3:24.5 and 3:25.2, Uldxaw 3:38.1, the dead
  main tank 4:31), none in uld2 and uld3; `vezax.joined = 1` for every bot from the first second. Both
  east swings kept the camp inside 45 yd this time, so the latch had little to hold, and it kept nobody
  in a formation it should have left.
- *Cross.* `vezax.reach = cross` from 7 / 10 / 8 bots. Replayed on round 9's slots (`replay_cross.py`):
  crossed 6 / 85 / 46% of ranged slot samples in the phase, a median 29.2-29.9 yd from him (min 24.4;
  row 0 is 30.5, the floor 29.1). Each is where the old floor stop left the slot farther from the Animus
  than the pull's own target, two slot tolerances inside its 37 yd ceiling, as built; past the ceiling
  itself the floor stop would have left only 1-2%, so in normal settles a cross mostly moves a slot
  under 2.4 yd. No cost seen: far 13-14/9 at every in-phase mark (3:12, 3:52), none of them on a melee
  or a tank before the collapse, one Animus-phase crash hit before it in three pulls (Uldxaw 3:37.4,
  lived), none on a crossed bot. uld2 crossed 85% of the time and killed it fastest since window 11.
  **Keeping the Q1 answer cost no kill here.**
- *Pace* 1.07 / 1.33 / 1.10 %/s (window 21's normal settles 1.22-1.24). Ranged `past` 19 / 25 / 22% of
  the phase, all off-slot (dodge, reach spell, position and soak walks); no pulled slot itself past the
  ceiling (`pastwhy.py`).
- *Threat vetoes* rose (uld1 100, uld3 157 in the phase, window 21 0-32), 93 and 141 of them from the
  10 s bin of the main tank's death on (+57.8 and +75.2 s): a consequence, not a cause.

**Ranking.**

1. *uld1, 31: a marked healer stranded inside its group by the floor check (bug).* The 1:00.5 mark on
   Uldxac (L, row 0) sent it toward its arc spot (1884.6, 103.6), but from 1:00.9 the straight-line
   check from where it stood clipped the spot to (1861.1, 114.6), 9 yd away (`vezax.floor = edge`
   1:00.9-1:05.6). There `TryGetVezaxMarkSpot` (`UldEncounter_Vezax.cpp:478` before) clipped to the same
   point, the trigger read it arrived (`UldTriggers_Vezax.cpp:53`), and the soak walked it back into the
   camp's field twice (1:03.2, 1:05.3); it left only after a dodge at 1:06.3 moved it past the clip. Group
   L stayed within
   7-15 yd: 75 ticks (w21 uld1 21, w19 uld1 21), Uldxau and Uldxaw dead, Vezax healed at +0.543 %/s
   for the window (−0.21 in w19/w21), and the fire mage Uldxaq spent three Mana Shields on the leech and
   hit 0 at 2:07. Healers Uldxab and Uldxae stood at 31/30% at 2:00 (w19/w21 51-66 / 50-52), Uldxab dry
   from 3:03; Vezax was 31.8% at the spawn (26.4-28.4); in the Animus phase ranged did 65k/s (81-93k),
   two of them dry (the fire mage at 0%, the re-raised Uldxaw at 6%), the 3:52 mark killed four of the
   weakened group (32 ticks; the
   3:54 crash strafed group L along the marked bot's arc), and the main tank died at +57.8 s.
   `edges.py`: every `edge` read in windows 14-22 clipped at (1860.0-1861.7, 113.6-114.7), from west or
   east, whatever his settle: a small navmesh hole the pathed walk goes round. `markedges.py`: 84 marks
   before the spawn, those with an edge held over 1.9 s (w17 uld2 0:20 and 1:00, w21 uld2 0:20, w22 uld1
   1:00) leeched 41-75 ticks against a median 11 (max 33) for the 73 with none; soak walks of marked bots
   came 8 from edge windows, 2 from the rest (`markedmovers.py`).
2. *uld3, 30: the far group's strafes leave the Animus's reach (bug).* The race ended 7.4% short at
   1.10 %/s (w19 uld3, same roster, 1.28). Group L, on his far side from the Animus, spent 30-44% of the
   phase past its 37 yd ceiling against 1-9% for R; all 39 of L's accepted dodges ended past it, 2 of
   R's 29 (reader's new `dodges` column). L's ranged did 39k/s against 53k/s in w19 uld3; the 14k/s is
   ~0.12 %/s, a kill at ~82 s before the 84 s collapse. Cause: `TryGetVezaxDodgeSpot` (`:939` before)
   strafes 15 yd across the camp from the slot round 5 pulled toward the Animus, so a group whose
   strafe points away from it leaves its reach, and reach spell and the position node walk it back
   (`pastwhy2.py`: the group with its strafe past the ceiling spends 19-44% of the phase past it in every
   normal settle since window 15, kills included, the other 5-13%). Windows 15-22 (`strafe_replay.py`):
   416 of 597 ranged dodges in the phase ended past the ceiling. The findings doc already had the strafe
   pulled with the slot into the ceiling; only its start was.
3. Profound Darkness collapse after the main tank's death: the end of both wipes, not a cause.

**Picked: both, independent.** Two separate helpers, one per pull.

**Fix** (`UldEncounter_Vezax.cpp` only):

- Mark spots are floor-checked along the line from Vezax (`CheckVezaxFloor` takes an origin, `:134`; the
  side and south spots pass his position, `:527`, `:557`); the slot and the strafe still check from the
  bot. The spot no longer moves with the bot, so the trigger never reads a clip as arrival, and the walk
  (pathed by `MoveTo`) goes round the hole. Lines from him to the arc spots (±65.5° off the camp
  bearing) and the south spots never cross the hole for any settle within ~25 yd of the anchor (the L arc's
  line first meets it with him ~25 yd north-west, the R arc's and the south ones' past 32); the hall's north
  edge still pulls a spot back as before.
- A ranged dps targeting the live Animus whose strafe ends farther from it than the slot pull's reach gets
  the strafe pulled the same way (`TryGetVezaxStrafeInAnimusReach`, `:213`, used first at `:989`), kept
  only if it lands on the floor and 11 yd (`ULDUAR_VEZAX_SHADOW_CRASH_WALK_CLEARANCE`) off the impact;
  else the plain strafe and its 10 yd test as before. New `vezax.dodge = pulled`. The pull's geometry is
  shared with the slot pull (`PullVezaxSpotIntoAnimusReach`, `:174`); `vezax.reach` is written only for
  the slot as before. Replay: 373 of the 416 get a spot 11+ yd clear (27 at 10-11 and 16 inside 10 keep
  the plain strafe), 24.5 yd from him at least, median 28.2, 7.5 yd from the plain strafe (max 11.5).
  This crosses the Mark floor for the seconds of a dodge, in the same case the user's Q1 answer lets a
  slot cross it (out of the Animus's reach otherwise).

Reader: `--animus` gains `dodges` (accepted dodges ending past the Animus's ceiling, of all) per caster
and prints `vezax.dodge` values after the spawn (test added). Docs: `docs/raids/ulduar/vezax.md` (floor
paragraph with the hole, Shadow Crash section, trace keys, `--animus`).

10/25 and normal/hard: the mark spots and their check run in every mode; the strafe pull only on a live
Animus, so hard mode only, and it keys on distances, not raid size. Bots only, keyed
on the bot's own target and position, so the same with or without a real player. Shared code: none.

**Next window should show.**

- `--mark`: no `vezax.floor = edge` on a marked bot held past a second, no mark window over ~35 ticks
  before the spawn, no soak walk by a marked bot.
- `--animus`: `dodges` near 0 past for both groups (they were 39/39 against 2/29 in uld3);
  `vezax.dodge = pulled` written by most ranged dps after the spawn; ranged `past` under ~15% for the
  group on his far side from the Animus too; pace 1.24 %/s or more in a normal settle whichever way he
  swings.
- `--crash`: no hit on a bot whose dodge read `pulled`; Animus-phase crash deaths 0-1 a pull.
- Cost to watch: pulled strafes stand 24.5-37 yd from him (median 28.2, 54% inside the 29.1 floor), so an
  in-phase mark cast during a far-group dodge sees up to ~5 fewer far; `far` under 9 at 3:12 or 3:52, or a mark on a melee or tank there,
  is this fix's cost.

**Questions.** None new. Q1 (a) cost no kill in this window (uld2 crossed 85% of its samples and killed
fastest); the north settle's healer reach to the main tank stays open as before.

**Review.** Both causes hold. w22 uld1 Uldxac (`vezax-r10-review/markclip.py`): mark move to (1884.63,
103.63) at 1:00.685, `edge` and a move to (1861.06, 114.64) at 1:00.912, soak walks back to the field at
1:03.237 and 1:05.291, back on the arc spot only after the 1:06.254 dodge; `--mark` reads 75 ticks,
+0.543 %/s. w22 uld3 `--animus`: L's five ranged dps 39 of 39 dodges past the ceiling, R's four 2 of 29.
`strafe_replay.py` re-run on windows 15-22: 373 of 416 pulled 11+ yd clear, from him 24.5-37 (median
28.2). No size, mode or player branch, no shared code, no comments. `pb-check` passes.

- Review fix: a marked bot no longer gets the strafe pull (`TryGetVezaxStrafeInAnimusReach`). Its dodge
  is taken from its slot, so the pull drags it toward the Animus and the camp with its leech:
  `vezax-r10-review/marked_dodges.py` finds 7 marked ranged dodges in Animus phases of windows 15-22,
  4 the pull would move, bots alive within 16.5 yd of the spot 2 → 6, 4 → 5 and 2 → 2.
- Review fix: the hole crossing starts at ~25 yd from the anchor, not 30 (`vezax-r10-review/holelines.py`,
  hole box (1860-1862, 113.5-115)), and pulled strafes stand 24.5-37 yd from him, about half inside the
  floor, not 24.5-28 all inside; the findings doc said both, and "always the group on his far side",
  which holds only in a normal settle (both groups in a north one).
- Left: the dodge picks pulled or plain strafe afresh every tick with no hysteresis, so a pulled spot
  hovering at 11 yd off the impact while the Animus drifts can flip the bot between two points up to
  11.5 yd apart. The 10-11 band held 27 of 416 replayed dodges, and the Animus stands still on the main
  tank. A marked bot's dodge itself still heads for its group's strafe (pre-existing).

## Round 11 (2026-11-23, build e440e6f17)

**Data.** Window 23 at 25n hard mode (`raidtest.py report --boss vezax --raid uld --head e440e6f17
--pulls 10`), one valid pull per pool, hard mode held in all three (`animus` at 3:08.3, no vapor hit),
against window 21 (round 8, `--head 90400b39c`) and window 22a (round 9, `--head b9dd70fcc`). Only Vezax
r10 changed for Vezax between the builds. Analysis scripts: session scratchpad `vezax-r11/` (`cmp3.py`
the table below, `perbot.py`, `surge.py`, `mtdeath.py`, `healcasts.py`, `dodgecost.py`, `movers.py`,
`markcrash.py`, `markreplay.py`, `exitreplay.py`, `latetotal.py`).

| Pool | Run | Trace | Result |
|---|---|---|---|
| uld1 | `tr-20261123-210611-8` | `603_8_general-vezax_1795468054` | wipe 4:50, 29 deaths, the Animus at 23.7% at the collapse (4:16.0) |
| uld2 | `tr-20261123-210631-9` | `603_9_general-vezax_1795468062` | wipe 5:02, 29 deaths, the Animus at 11.8% at the collapse (4:28.5) |
| uld3 | `tr-20261123-210551-7` | `603_7_general-vezax_1795467993` | wipe 6:11, 31 deaths, the Animus at 1.9% at the collapse (4:32.3) |

**Stop rules.** Not polished: no kill. Round 10's far-group failure (ranged dodges past the Animus's
ceiling) is gone (below), so its lineage stops nothing; this round's picks are new. The race itself is
the waived gear check.

**Deaths by mechanic** (last hit; re-raised bots die twice).

| Pull | Before the collapse | Collapse |
|---|---|---|
| uld1 | none | from 4:08.0 (DK main tank to Vezax melee at +59.5 s): 10 Profound Darkness, 10 Vezax melee, 7 Animus melee, 1 Mark, 1 Shadow Crash |
| uld2 | 1 Mark leech (3:57.2, the 3:52 mark) | from 4:18.4 (a healer at +69.9 s): 20 Profound Darkness, 4 Animus melee, 3 Shadow Crash, 1 Vezax melee |
| uld3 | 1 Shadow Crash (healer Uldzae 3:47.5, a strafe across the impact) | from 4:24.4: 14 Profound Darkness, 11 Vezax melee, 3 Mark, 2 Animus melee |

**Verdict on round 10 (mark spots from Vezax, pulled dodges): both did what they targeted.**

- *Mark spots.* Marks before the spawn leeched 15-27 ticks (window 22a uld1's stranded one 75); one
  `edge` read on a marked bot (uld2 1:00.5 Uldyaf, a declined position walk, on its spot by 1:05.5, 17
  ticks); no soak walk by a marked bot (windows 21-22a: 2).
- *Pulled dodges.* `vezax.dodge = pulled` from 10/10/9 bots; ranged dodges ending past the ceiling 0-1
  per bot (window 22a uld3 group L 39 of 39); ranged `past` 4-15 / 1-24 / 1-17% (uld3's L group 1-17%
  against 30-44%); reach spell walks 0.6-1.6 per ranged bot-minute against 2.2-3.3. A pulled dodge costs
  what a strafe does (`dodgecost.py`): walk 14.0-14.8 yd against 15.0, field again 2.6-3.2 s after the
  landing (strafes in windows 21-22a 2.6-3.2), 4 casts from the cast to 6 s after the landing (3-4).
- *Crashes.* No hit on a bot whose dodge read `pulled`; Animus-phase crash deaths before the collapse
  0/0/1, the one a healer (never pulled).
- *Cost watch.* Far 14/14/13 at 3:12 and 3:52; no in-phase mark on melee or a tank before a collapse.

**Windows 21, 22a and 23 pull by pull** (`cmp3.py`; settle is Vezax from the anchor; main tank healing
all sources, before the spawn over its last minute, in the phase to his death or the stop).

| Pull | Settle; Animus off him | Animus | Raid / ranged k/s | Healer mana spawn → stop | MT healed / taken k/s | Mark ticks pre / phase | Crash hits pre / phase | MT death |
|---|---|---|---|---|---|---|---|---|
| w21 uld1 | 9.9; across +9.1 | kill 81 s, 1.24 %/s | 153 / 84 | 20-56 → 0-9 | 1.1, 2.8 / 3.0 | 72 / 8, 16 | 0 / 0 | - |
| w21 uld2 | 9.9; +8.9 | kill 82 s, 1.22 | 150 / 75 | 48-69 → 0-46 | 1.0, 2.4 / 2.9 | 82 / 21 | 0 / 0 | - |
| w21 uld3 | 9.9, swung away; −4.7 | lockout, 71.6% at 22 s | 158 / 89 | 42-81 → 35-78 | 1.2, 2.5 / 2.4 | 52 / - | 0 / 12 | +66.9 |
| w22 uld1 | 9.9; −7.1 | 34.2% at 61 s, 1.07 | 133 / 65 | 0-74 → 0-49 | 1.6, 2.5 / 3.1 | 126 / 32 | 0 / 1 | +57.8 |
| w22 uld2 | 9.9; +7.5 | kill 75 s, 1.33 | 165 / 89 | 47-69 → 0-46 | 1.3, 2.1 / 2.5 | 74 / 19 | 0 / 0 | - |
| w22 uld3 | 10.2; −9.0 | 7.4% at 84 s, 1.10 | 136 / 62 | 46-81 → 0-43 | 1.6, 3.6 / 4.0 | 47 / 19, 26 | 0 / 0 | +75.2 |
| w23 uld1 | 9.6; −2.8 | 23.7% at 67 s, 1.13 | 139 / 73 | 38-67 → 1-47 | 1.9, 2.8 / 3.5 | 62 / 7, 32 | 0 / 0 | +59.5 |
| w23 uld2 | 9.9; +8.9 | 11.8% at 80 s, 1.10 | 137 / 61 | 37-70 → 3-15 | 0.7, 2.3 / 2.6 | 76 / 19, 41 | 0 / 0 | +92.1 |
| w23 uld3 | 10.6; +8.5 | 1.9% at 84 s, 1.17 | 144 / 70 | 41-73 → 0-46 | 1.6, 2.6 / 3.1 | 49 / 25, 21 | 0 / 1 | +82.1 |

All nine settled normally. The pace to a collapse reads low, so `surge.py` reads the Animus at +60 s,
everyone alive: uld1 28.3 / 34.8 / 28.2%, uld2 33.5 / 19.2 / 33.7%, uld3 40.5 (the lockout) / 29.5 /
27.1% (windows 21 / 22a / 23). Window 23 kept window 21's two kills' pace to the minute and lost after it:

- uld1: the main tank died at +59.5 s (window 21: alive). In the 14.5 s before, healers landed 11.4k on
  him against 25.4k in window 21 (Beacon of Light on him there), and spent the rest on group L and the
  healers, drained by the 3:52 mark (32 ticks, 133k; window 21's 16). He took 3.5k/s against 3.0k/s.
- uld2: healers dry by +70 s (Uldyae 70 → 0), deaths from +69.9 s; the Animus went 33.7 → 11.8% over
  +60-80 s against window 21's 33.5 → 2.3%. The 3:52 mark leeched 41 ticks, 162k (window 21: 21).
- uld3: a healer killed at +39 s by a strafe across the impact (below); 1.9% short at the collapse.

**Which round helps, which hurts.** Neither hurts by any measure here.

- Round 9's latch: `out s` 0 for every camp bot in all six pulls since (window 21 uld3: 203 bot-s and
  its 29 deaths). Helps, at no cost.
- Round 9's crossing (Q1): `cross` from 7/10/8 and 9/5/1 bots; far 13-14/9 at every in-phase mark
  before a collapse in six pulls. Window 22a uld2 crossed 85% of its samples and killed fastest since
  window 11. No cost found; the Animus pace at +60 s does not separate the windows.
- Round 10's mark spots: marks before the spawn 62/76/49 ticks a pull (window 22a 126/74/47, window 21
  72/82/52). Helps.
- Round 10's pulled dodges: uld3 at +60 s 29.5 → 27.1%, 7.4% → 1.9% short; elsewhere neutral (ranged
  travel 190-222 yd per bot-minute against 169-235, same walk and field time per dodge).

**A change from the Q1 answer did not cost these kills.**

**Ranking** (89 deaths).

1. *The collapse, 87.* The waived gear race; window 23 lost it by survival after +60 s (above).
2. *The 3:52 mark × crash, 1 death (uld2) and 32/41/21 ticks: a plan question (Q1).* The crash cast
   1.3-1.8 s after every Animus-phase mark makes the marked bot's group strafe the way its arc runs
   (`markdetail.py` uld2: at 3:55.8-3:56.8 group R's four ranged and two healers 3.4-5.9 yd from Uldyah,
   the marked bot, which itself dodged to its group's strafe spot). `markcrash.py`: marks whose bot dodged
   a crash averaged 28 ticks in windows 11-23 (21 windows), against 13-16 for the rest; the 3:52 one
   killed 4, 4 and 1 in w17 uld1, w22 uld1 and w23 uld2.
3. *A strafe across the impact, 1 death (uld3): bug, fixed below.*

**Picked: the late strafe.** Crash cast at 3:44.17 on healer Uldzac, landing at 3:47.36 on (1843.8,
115.8) (`crash.py`). Uldzae (R, slot 10, row 0 inner file) stood 10.4 yd east of it, 14 yd east of its
slot, with a position walk toward that slot (4.1 yd from the impact) issued at 3:42.94, before the cast.
It carried the bot inside 10 yd, and the dodge at 3:45.17 sent it to group R's strafe 15 yd west of the
slot, 18.9 yd beyond the impact: 9.6 yd from the centre at 3:45.19, 0.9 at 3:46.51, dead at 6.8 past it
at 3:47.53. `TryGetVezaxDodgeSpot` (`UldEncounter_Vezax.cpp:971` before) checks only where the strafe
ends (10 yd off the impact), never whether the walk there leaves the blast before the landing.
`exitreplay.py` over windows 11-23: 12 of 7249 strafe or pulled dodges could not leave the blast in the
flight left at 7 yd/s, 11 of them across the impact (most from 9-11 yd, clear only after 18-20), and 10 of
the 12 bots were hit, 6 dead, all but one in the Animus phase; 10 hits came from the other 7237.

**Fix** (`UldEncounter_Vezax.cpp` only): `VezaxDodgeClearsInTime` (`:691`) walks the bot's path to a
strafe or pulled strafe (`VezaxBlastExitWalk`, `:664`: distance along it until it leaves the 10 yd blast
for good) at its run speed against the flight left (`crashCastMs`, the stored window less its 500 ms
slack). A strafe that cannot make it falls to the search (12 yd clear, nearest first), noted
`vezax.dodge = late`; with no clear spot it keeps the strafe. For the edge bots that died the search's
spot is 2-4 yd away. Reader: `--crash` gains `late`, bots whose accepted dodge could not leave the blast
by the landing at 7 yd/s, and a summary of how many were hit (test added; windows 11-23: 32 such dodges of
all kinds, 22 hit). Docs: `docs/raids/ulduar/vezax.md` (Shadow Crash, trace keys, `--crash`, the mark ×
crash gap).

10/25 and normal/hard: crashes come in every mode and the test keys on the bot's own position, speed and
the stored flight. Bots only, so the same with or without a real player. Shared code: none.

**Next window should show.**

- `vezax.dodge = late` on a few bots a pull at most (replay: 12 in 39 pulls), each followed by a dodge
  of a few yards; `--crash` `late` 0 on strafes (a search can still be late when the bot is deep in);
  no Animus-phase crash death on a camp bot before a collapse.
- Everything else as window 23: `strafe`/`pulled` the bulk of dodges, ranged `past` under ~15%,
  marks before the spawn under ~30 ticks.

**Questions (user).**

- *Q1. A crash cast 1.3-1.8 s after the Animus-phase mark sends the marked bot's group along its arc.
  Change who moves?* The strafe is fixed per group and runs the way that group's arc does, so the group
  follows the marked bot for 3-4 s: 21-41 ticks on the 3:52 mark in window 23, the leech drawing the
  healers off the main tank in uld1 before his death and draining uld2's healers. (a) For a crash cast
  inside a fresh mark, the marked bot walks straight to its arc instead of joining its group's strafe,
  and its group strafes the other way where that strafe clears the impact, else the search. Replayed on
  the 21 such windows of windows 11-23 (the group's own walks mirrored, the impact test not applied):
  modelled leech targets 34.8 → 23.5 a window, window 23's 3:52 marks 37/43/26 → 23/27/15. Cost: that
  group lands toward the other group's side for one crash, its field walk back unmeasured. (b) The
  marked bot's group takes the search clear of both the impact and the marked bot (the melee break's 18
  yd), scattering it for one crash. (c) Keep: ~12 extra ticks a window (~50k raid damage), Vezax +0.82
  %/s over such Animus-phase windows against +0.24 for those without a crash.
  *Recommended: (a)*; the arc walk alone saves about 1 tick in 7.

**Review.** The cause holds. w23 uld3 Uldzae (`vezax-r11-review/uldzae.py`): crash cast 3:44.171,
landing 3:47.361 at (1843.8, 115.8). The bot stood still 10.4 yd east channeling Hymn of Hope (64901,
3:42.672-3:45.066), just outside the cast-time interrupt's 10 yd, so the 3:42.937 position walk moved
it only once the channel ended; inside 10 yd, the 3:45.167 dodge sent it to (1824.9, 114.4), 18.9 yd
past the impact, through 0.9 yd at 3:46.512, dead 6.8 yd past at 3:47.524. `exitreplay.py` re-run on
windows 11-23: 12 of 7249 strafe or pulled dodges late (11 crossing), 10 of them hit; the other 7237 took
10 hits. On that bot the fix's search walks ~4 yd out. No size, mode or player branch, no shared code, no
comments. `pb-check` passes.

- Review fix: a late dodge with no clear spot noted `vezax.dodge = strafe` even when it kept the pulled
  strafe; it now notes the kind it keeps.
- Review fix: `--crash`'s summary counted bots and called them dodges.
- Left: a late pulled strafe falls to the plain strafe before the search when that one is in time (the
  existing pulled → plain cascade), so that dodge can end past the Animus's ceiling. `--crash` `late`
  cannot tell strafes from searches; reading "late 0 on strafes" needs the `vezax.dodge` note at the
  move, as `exitreplay.py` does.

## Round 12 (2026-11-24, build baf47824b)

**Data.** Window 24 at 25n hard mode (`raidtest.py report --boss vezax --raid uld --head baf47824b
--pulls 10`), one valid pull per pool, hard mode held in all three (`animus` at 3:08.3-3:08.5, no vapor
hit), against windows 21-23 (`cmp3.py`). Analysis scripts: session scratchpad `vezax-r12/` (`cmp3.py`,
`surge.py`, `mtwindow.py`, `palheals.py`, `beacon.py`, `beaconhist.py`, `beaconwhy.py`, `copies.py`,
`copies2.py`, `tankbuffs.py`, `leak.py`, `dealt24.py`, `threatvetoes.py`, `markcrash.py`,
`exitreplay.py`, `latenotes.py`, `vezvictim.py`).

| Pool | Run | Trace | Result |
|---|---|---|---|
| uld1 | `tr-20261123-222105-10` | `603_10_general-vezax_1795472506` | kill 5:35, 0 deaths, the Animus dead in 73.5 s |
| uld2 | `tr-20261123-222226-11` | `603_11_general-vezax_1795472587` | wipe 4:52, 30 deaths, the Animus at 25.5% at the collapse (4:20.5) |
| uld3 | `tr-20261123-222246-12` | `603_12_general-vezax_1795472608` | kill 5:32, 0 deaths, the Animus dead in 79 s |

**Stop rules.** Not polished: uld2 wiped. Round 11's failure (late strafes) is gone (below); this round's
pick is new, so no repeat count.

**Deaths by mechanic** (last hit; re-raised bots die twice).

| Pull | Before the main tank's death | From it |
|---|---|---|
| uld1 | none | - |
| uld2 | none | from 4:01.6 (warrior main tank to Animus melee at +53.1 s): 11 Profound Darkness, 10 Vezax melee, 7 Animus melee (him included), 2 Shadow Crash (the re-raised main tank 4:31.4, a re-raised melee 4:47.5) |
| uld3 | none | - |

**Verdict on round 11 (a late crash dodge takes the search): it did what it targeted.**

- `exitreplay.py`: 1 of 930 strafe or pulled dodges could not leave the blast in time (uld2 Uldyae
  4:16.9, 2.1 s needed against 2.1 left), not hit; no strafe or pulled dodge hit at all (windows 11-23:
  12 of 7249 late, 10 hit).
- `vezax.dodge = late` 1 / 3 / 2 bots; the four with a matched move walked 3.5-10 yd to 12-12.7 yd
  from the impact, and none of the six was hit. More than the replay's 12 in 39 pulls, all harmless.
- No crash hit on a camp bot before a collapse in any pull; uld2's two crash deaths came 11-27 s into
  its collapse.
- Everything else as window 23: strafe and pulled the bulk of dodges, ranged `past` 1-12% (uld3 one
  ranged at 18%). One pre-spawn mark over ~30 ticks: uld1 0:20 (53, below).

**Animus and Vezax per pull** (`cmp3.py`, `surge.py`; settle is Vezax from the anchor, across the
Animus off him; main tank healing all sources, spawn to his death or the stop).

| Pull | Settle; across | Vezax at spawn | Animus +30 / +45 / +60 s | Animus | Raid / ranged k/s | MT healed / taken k/s | Beacon on the MT in the phase | Vezax |
|---|---|---|---|---|---|---|---|---|
| uld1 (DK) | 9.9; −7.0 | 19.6% | 60.9 / 37.9 / 17.1 | kill 73.5 s, 1.36 %/s | 167 / 96 | 3.5 / 3.5 | 100%, 87k copied | dead 5:35.2 |
| uld2 (warrior) | 9.9; +8.1 | 21.6% | 66.1 / 50.4 / 35.3 | 39.9% at his death (+53.1), 25.5% at the collapse (+72) | 128 / 58 | 3.1 / 4.0 | 0%, 96k copied onto the bear | 21.4% floor, wipe 4:47.7 |
| uld3 (bear) | 9.9; −7.2 | 24.4% | 61.2 / 42.0 / 27.9 | kill 79 s, 1.27 %/s | 156 / 77 | 2.7 / 3.0 | no holy paladin | dead 5:31.9 |

All three settled normally. uld2 ran window 21 uld2's kill pace to +45 s (67.8 / 51.2) and lost it with
its main tank.

**Why the uld2 main tank died at 4:01.6.** The warrior Uldyaa held Vezax all pull and the Animus from
+1.0 s, on the anchor; he took 4.0k/s in the phase, the top of windows 21-24's 2.4-4.0, and got
3.1k/s.

1. *Beacon of Light sat on the idle bear off-tank Uldyab from 0:16.8 to his death* (`beacon.py`): the
   holy paladin Uldyac refreshed it there at 1:47.0 and 3:17.3 with Uldyaa at 80%. In the phase its
   copies cast 95.9k (1.81k/s) onto a full-health bear, 8.5k effective; on uld1's DK main tank they
   landed 87k effective (1.2k/s, 34% of his healing). Sacred Shield went the same way (bear 5, him 2;
   uld1 him 7).
2. *The 3:52.5 mark on healer Uldyaf (L) and a crash on its group 1.6 s later*: 40 ticks, 25 of them at
   3:54-3:56; Uldyac and Uldyad fell to 38% and healed themselves and the ranged. From 3:52 to 4:01.5 he
   took 69.7k and got 38.5k (`mtwindow.py`); the paladin's Lay on Hands at 3:58.6 went on a ranged
   (23.7k, copied onto the bear) with him at 27-37%. Uldyaf was at 3-7% mana.
3. *Earlier, the Animus loose 12 s on the shadow priest Uldyao* (3:32.7-3:44.7): held by `vezax threat
   multiplier` from 3:19.7, his DoTs in a field still did 205k in 13 s (15.8k/s, `dealt24.py`) and
   passed 130% of the warrior's threat. The bring-back walked it in at 31% health and Uldyaa retook it.
   The same bot did it in window 17 uld2 (round 7's DoT leak candidate); not in windows 19, 22a, 23.

**What followed.** Both bosses loose: Vezax killed Uldyap 4:06.0, Uldyav 4:07.8 and Uldyaw 4:09.7, the
Animus Uldyal 4:03.2 and Uldyao 4:06.1; the bear took the Animus at 4:07.9 and Vezax at 4:23.7. The
collapse came at 4:20.5 with the Animus at 25.5%, a second Heroism at 4:30.7, Profound Darkness killed
11 at 4:20-4:36, wipe at 4:47.7.

**Beacon placement since window 11** (`beaconhist.py`, rosters A and B, the two with a holy paladin; C
has none). On the main tank 80%+ of the phase in 5 of 18 pulls: 4 kills, his death before +80 s in 1
(w19 uld2, a north settle). Under 60% in 13: 4 kills, his death before +80 s in 5 (w14 uld1, w17 uld2,
w22a uld1, w23 uld1, w24 uld2). `beaconwhy.py`: the main tank on the anchor stood past the paladin's
`HealDistance` 26-54% of the pull (windows 21, 23, 24); the off-tank 0-39%.

**Ranking** (30 deaths, all uld2, all from its main tank's death).

1. *Beacon of Light and Sacred Shield on the idle off-tank: bug, fixed below.*
2. *The 3:52 mark × crash: plan question (Q1, queued from round 11).* Below.
3. *DoTs carrying a held dps past the threat cap on the Animus: bug, not small* (estimate a DoT's
   remaining threat, or hold its application near the cap). Candidate for a later round.

**Picked: tank buffs on the idle off-tank.** The strategy has the main tank hold Vezax and the Animus on
the anchor for the whole fight; the class pick meant to beacon "the one actually taking damage"
(`PaladinTankToBeaconValue::Calculate`, `src/Ai/Class/Paladin/Value/PaladinTankToBeaconValue.cpp:24`)
drops a tank past `HealDistance` (`:44`), so from row 0 only the off-tank, standing nearer the camp,
qualified at most refreshes, and its 75-point swap margin (`:17`, `:65`) kept the beacon there once it
held it. Beacon reaches 60 yd and Sacred Shield 40 (`spell.reference.csv` range indexes 14 and 5).

**Fix** (Vezax files only):

- `VezaxTankBuffHolder` (`UldEncounter_Vezax.cpp:761`): for a holy paladin bot while he fights, the
  living main tank, else the tank he is hitting. `VezaxTankBuffTarget` (`:785`) returns him when he lacks
  the buff (Beacon: the bot's own), stands inside the spell's range and line of sight, and
  `CanCastSpell` passes.
- Nodes `vezax tank beacon` (`ACTION_CRITICAL_HEAL + 7`) and `vezax tank sacred shield` (`+ 2`), where
  the class nodes sit (`UldDefinition_Vezax.cpp:46-61`; `VezaxTankBuffTrigger`/`Action`).
- Rule `vezax tank buffs on the holder` (`:63`): the class `beacon of light on tank`, `… on main tank`,
  `sacred shield on tank` and `… on main tank` hold for a healer while the holder exists.

Reader: `general_vezax.py --tank` (main tank = the tank he hit longest; Beacon share on him and on
another tank before the Animus and after it, copies landing on him to his first death, Sacred Shield
casts by target, the rule's vetoes; tests added). Docs: `docs/raids/ulduar/vezax.md` (camp section,
trace section, the mark × crash gap). 10/25 and normal/hard: the main tank holds Vezax in every mode
and no branch keys on size or the Animus. A real main tank is the group's flagged one like a bot; a
real paladin runs no rule. Shared code: none.

**Next window should show** (uld1 and uld2; uld3 has no holy paladin).

- `--tank`: Beacon on the main tank ~100% before and after the spawn, copies landing on him ~1.2k/s in
  the phase (uld2 had 0 of 1.8k/s cast); Sacred Shield casts on him only; Beacon casts ~4-5 a pull
  (w23 uld2 flipped 11 times).
- `postmortem.py --vetoes`: `vezax tank buffs on the holder` only on the holy paladin, on the class
  beacon and Sacred Shield actions.
- No main tank death before the Animus dies in a normal settle; in uld2 his healing at or above the
  ~4k/s he takes.

**Q1 (round 11) against window 24: it still matters.** Animus-phase 3:52 marks: uld1 Uldxag (R) 49
ticks, the crash cast +1.5 s on its own group (impact on Uldxab), the marked bot dodging, Vezax
+1.20 %/s; uld2 Uldyaf (L) 40 ticks, crash +1.6 s on its own group, +0.86 %/s, and the leech drew the
healers off the main tank 9 s before his death (above); uld3 Uldzae (R) 22 ticks, the crash on the
other group, the marked bot not dodging, +0.60 %/s. `markcrash.py` over windows 11-24: Animus-phase
marks whose bot dodged a crash 17, median 32 ticks (mean 29.1), Vezax +0.76 %/s; with a crash but no
dodge by the marked bot 38, median 7 (12.3), +0.06; none 12, median 17 (15.2), +0.23. A variant at
uld1 0:20: the crash +2.6 s aimed at the marked bot itself, whose dodge headed for its group's strafe:
53 ticks, 38 on its own group.

**Questions (user).**

- *Q1 (from round 11, unchanged). A crash cast 1.3-1.8 s after a fresh mark sends the marked bot's group
  along its arc: change who moves?* (a) the marked bot walks straight to its arc and its group strafes
  the other way where that clears the impact, else the search; (b) the group takes the search clear of
  the impact and the marked bot; (c) keep. *Recommended: (a)*; window 24's two own-group cases leeched
  49 and 40 ticks and the second cost uld2 its main tank.
- *Q2 (shared, optional). Fix `PaladinTankToBeaconValue` in class code* so the beacon follows the tank
  taking damage within Beacon's 60 yd rather than `HealDistance`? The Vezax nodes cover this boss.
  *Recommended: yes, as its own lane*: every boss with a holy paladin and a tank parked past 38.5 yd
  gets pulled again.

**Review.** The cause holds (`vezax-r12/beaconwhy.py` re-run, `vezax-r12-review/phasecopies.py`). w24
uld2: the class pick moved Beacon to the bear at 0:16.8 with Uldyaa 42.3 yd from the paladin, past
`HealDistance` and both reaches; at the 1:47.0 and 3:17.3 refreshes he stood inside it (41.4, 37.7 yd)
and the swap margin kept the bear. In the phase its copies cast 95.9k on the bear, 8.5k effective, while
he took 3.99k/s and got 3.07k/s. `--tank` on windows 23-24: Beacon on another tank 92-99% after the spawn
in w23 uld1, w23 uld2 and w24 uld2, on him 100% in w24 uld1. Not a plan change: the class value means
Beacon for the tank taking damage (`PaladinTankToBeaconValue.h:15`), and XT-002 does the same. No size,
mode or player branch, no shared code, no comments. `pb-check` passes.

- Review fix: `--tank`'s Beacon shares stop at his first death like its copies; its output and the
  trace section now say so.

**User answers (2026-11-24):** Q1: yes, (a): when a Shadow Crash lands right after the Animus-phase
Mark of the Faceless, the marked bot walks straight to its spot and its group strafes the other way.
Q2: **no**, no shared `PaladinTankToBeaconValue` fix: round 12's Vezax-only Beacon holder stays and
shared paladin code is not touched.

## Round 13 (2026-11-24, branch from `6486414fc`)

**Data.** No new pull: round 12 merged (`1433ef25c`) after window 24, so `raidtest.py report --boss
vezax --raid uld --head baf47824b --pulls 10` still shows window 24's three (round 12's table: uld1
kill 5:35 and uld3 kill 5:32 with 0 deaths, uld2 wipe 4:52 with 30). No stop rule, verdict or ranking
this round: it builds the answer to Q1, and round 12 is unjudged. Deaths by mechanic: round 12's
table stands. Analysis scripts: session scratchpad `vezax-r13/` (`replay.py`, `cases.py`).

**Built: user answer Q1 (a), the crash after the Animus-phase mark.** Round 11's write-up holds on the
new reader (`--markcrash`, below) over windows 11-24: 28 Animus-phase marks had a crash cast inside
them, a median 1.5 s in (1.3-4.8), and leeched a median 29 ticks, 22 on the marked bot's own block,
Vezax +0.77 %/s over the window; the 107 pre-spawn ones, crash 2.3-8.9 s in, 14 and 11, −0.26 %/s.
Window 24's three 3:52 marks:

| Pull | Marked | Crash on | Ticks / on its block | Marked bot's dodge | Blockmates with / away |
|---|---|---|---|---|---|
| uld1 | Uldxag (R) | R, +1.5 s | 49 / 34 | off its spot (the group strafe) | 6 / 0 |
| uld2 | Uldyaf (L) | L, +1.6 s | 40 / 27 | off its spot | 6 / 0 |
| uld3 | Uldzae (R) | L, +1.5 s | 22 / 15 | none | 4 / 0 |

**Fix** (`UldEncounter_Vezax.{h,cpp}` only):

- `VezaxEncounterState::crashMarked`: at each crash's cast `VezaxNoteShadowCrash` (`:659`) records the
  slotted camp member carrying 63276, not Vezax's victim, while he carries the Barrier
  (`VezaxMarkedCamper`, `:638`); the next crash overwrites it.
- `TryGetVezaxDodgeSpot` (`:1104`): the recorded bot dodges to its mark spot (`TryGetVezaxMarkedDodgeSpot`,
  `:1077`, the side arc spot from `TryGetVezaxMarkSpot`) when that stands 11 yd
  (`ULDUAR_VEZAX_SHADOW_CRASH_WALK_CLEARANCE`) off the impact and `VezaxDodgeClearsInTime` passes,
  `vezax.dodge = mark`; else the old cascade. Its blockmates (`VezaxStrafesAwayFromMark`, `:1093`) take
  the strafe the other way, pulled into the Animus's reach as before (`pulled away`) or plain (`away`),
  each kept only 11 yd off the impact (the plain strafe's own test is 10) and in time; else the search
  (`search`, `late`), never the old strafe.
- Recorded at the cast, so a blockmate's direction holds for the flight; the dodge re-picks its spot
  every tick and would otherwise turn if the marked bot died mid-flight.

Replay (`replay.py`, windows 11-24, positions as traced, a blockmate's reversed strafe taken as its
traced strafe mirrored 30 yd across): the marked bot's walk to its spot clears 11 yd and leaves the
blast in time in 15 of 15 dodges; blockmates' reversed strafe in 81 of 131, the other 50 mostly where
the crash was aimed at the other block, whose impact the reversed strafe would cross, so they search;
6 already went the other way (a crash aimed at the marked bot itself). Round 11's leech model put
both halves together at about a third fewer leech targets a window.

Reader: `general_vezax.py --markcrash` (`mark_crashes`, `:616`): per crash cast inside a camp member's
mark, its phase (`pre`/`animus`/`post`), the cast's offset, the block it was aimed at (`it` for the
marked bot), the leech ticks, those on the marked bot's block and those from the cast on, the marked
bot's first dodge in the flight (`spot` within 6 yd of its arc spot, else `off spot`), and its
blockmates' first dodges toward its arc side or the other way; tests added. `--animus` already prints
the new `vezax.dodge` values. Docs: `docs/raids/ulduar/vezax.md` (Shadow Crash section, trace keys,
`--markcrash`; the known gap now covers the pre-spawn and post-kill cases only).

10/25 and normal/hard: the record needs the Barrier, so only hard mode's Animus phase sees it, and
nothing keys on raid size (a 10-man camp has two blocks in rows 0-1). The record takes any slotted camp
member, a real player too, so a block turns away from a marked human as from a bot; the marked bot's
own dodge runs for bots only, as every dodge does. Shared code: none.

**Next window should show** (`--markcrash`, the 3:52 marks):

- the marked bot `spot` where it dodged, `vezax.dodge = mark` on it, its dodge row mostly `dup` (it is
  already walking there); blockmates `away`/`pulled away` when the crash hits its own block (the replay
  sent 7 of 71 there to the search, which can read `with`), a few yards `with` by the search when it
  hits the other;
- leech on the marked bot's block from the cast on well under window 24's 34 / 27 / 15, the window
  under ~20 ticks, Vezax's heal-back under +0.5 %/s;
- `--crash`: no hit on a blockmate that turned, `late` still 0-1 a pull;
- cost to watch: the turned block lands past the other block, ~12-18 yd from the new field as a
  strafe does, but among the other block's dodgers; a crash hit, a field share drop or a healer out
  of reach of the main tank right after the 3:52 mark is this change's cost;
- pre-spawn and post-kill cases unchanged (blockmates `with`, the marked bot `off spot`).

**Questions (user).**

- *Q3. Turn the block in every phase too?* Before the spawn the crash comes 2.3-8.9 s into a mark
  (median 5.6), mostly once the marked bot is clear: those 107 cases leeched a median 14 ticks, but
  the 9 whose marked bot dodged the crash a median 24, and one aimed at the marked bot itself (w24
  uld1 0:20) 53, 38 on its block. (a) Extend the turn to any crash cast inside a mark; (b) only to
  crashes aimed at the marked bot or its block in the first ~4 s; (c) keep the Animus phase only.
  *Recommended: (c) until window 25 reads the Animus-phase turn, then (b) if it works.*

**Review.** The cause holds (`vezax-r13-review/follow.py`; the lane's `replay.py` and `cases.py`
re-run: 28 Animus-phase cases, median 29 / 22 ticks, 15 of 15 marked walks and 81 of 131 reversed
strafes clear). w24 uld1: no leech tick on group R before the 3:54.3 crash; from Uldxag's strafe it
stood 1-8 yd from its blockmates to 3:57.8 at six ticks a second on them, 10-17 yd only at 3:58.8;
uld2 likewise, six a second on its block to 3:57.5. The fix is the user's (a) as worded. No size, mode or player branch: the
record needs the Barrier and takes a real player in a slot like a bot; no shared code, no comments.
`pb-check` passes.

- Review fix: the marked bot's dodge now goes to its mark walk's own point, and `MoveTo` refuses an
  exact repeat for 5 s even to a bot something stopped (`docs/engine/pitfalls.md`), so a mark walk
  stopped short left it in the blast where the old strafe, another point, was sent. In all 15 traced
  marked dodges the mark walk was 0.46-1.54 s old and still moving, so the dodge reads `dup`.
  `VezaxShadowCrashDodgeAction` now clears `last movement` for any dodger standing still 500 ms past
  its last move (`ULDUAR_VEZAX_DODGE_STALL_MS`), as Hodir and Algalon do.
- Review fix: `--markcrash` read that `dup` row as `no dodge`; it counts it now (test added).
- Review fix: the doc called the recorded member a camp bot; a real player in a slot is recorded too.
- Review fix: next-window line, own-block blockmates `away` but not all (7 of 71 searched).
- Left: the mark action's own walk has the same 5 s stall; older than this round.

## Round 14 (2026-11-24, build 3ce456519)

**Data.** Window 25 at 25n hard mode (`raidtest.py report --boss vezax --raid uld --head 3ce456519
--pulls 10`), one valid pull per pool, hard mode held in all three (`animus` at 3:08.3-3:08.5, no vapor
hit), against window 24 (`--head baf47824b`). Analysis scripts: session scratchpad `vezax-r14/`
(`cmp3.py`, `surge.py`, `animusbins.py`, `vezline.py`, `window.py`, `rows.py`, `campat.py`,
`leechwho.py`, `far.py`, `markpick.py`, `surgemark.py`, `markstall.py`, `stonestop.py`, `markdelay.py`,
`meleemarks.py`, `afterarrive.py`, `q3.py`; pull list in `pulls.py`).

| Pool | Run | Trace | Result |
|---|---|---|---|
| uld1 | `tr-20261124-102318-9` | `603_9_general-vezax_1795515840` | kill 6:07, 4 deaths, the Animus dead in 84.3 s |
| uld2 | `tr-20261124-102137-7` | `603_7_general-vezax_1795515738` | kill 5:17, 0 deaths, the Animus dead in 71.0 s |
| uld3 | `tr-20261124-102217-8` | `603_8_general-vezax_1795515779` | wipe 5:18, 30 deaths, the Animus dead at +87.3 s, after the collapse |

**Stop rules.** Not polished: uld3 wiped and uld1's kill had 4 deaths. The pick below is new (round 13's
review left it as a note), so no repeat count.

**Deaths by mechanic** (last hit; re-raised bots die twice).

| Pull | Deaths |
|---|---|
| uld1 | 4 Mark leech on melee at 4:34.0-4:35.1 (the 4:32.5 mark on frost DK Uldxah): Uldxat, Uldxai, Uldxai again after Reincarnation, Uldxax |
| uld2 | none |
| uld3 | collapse from 4:28.4 (+80.0 s): 13 Profound Darkness to 4:34.4, 9 Vezax melee (bear main tank 4:34.5, then 8 as he roamed after ranged), 7 Searing Flames (5:01.6), 1 Shadow Crash (the last tank, 5:18.3) |

**Animus and Vezax per pull** (`cmp3.py`, `surge.py`, `vezline.py`; settle is Vezax from the anchor).

| Pull | Settle | Vezax at spawn | Animus +30 / +45 / +60 / +80 s | Animus | Raid / ranged / melee k/s | Vezax at its death | End |
|---|---|---|---|---|---|---|---|
| uld1 (DK) | 12.9 (north) | 19.8% | 66.3 / 48.2 / 31.2 / 5.8 | dead +84.3 s, 1.19 %/s | 147 / 75 / 68 | 23.4%, 25 alive | Vezax dead 6:07.6 |
| uld2 (warrior) | 9.9 | 19.2% | 60.1 / 41.2 / 19.8 / - | dead +71.0 s, 1.41 %/s | 172 / 100 / 65 | 25.3%, 25 alive | Vezax dead 5:16.7 |
| uld3 (bear) | 9.9 | 20.7% | 64.2 / 45.0 / 29.3 / 5.7 | dead +87.3 s, 1.17 %/s to the collapse | 146 / 69 / 72 | 24.3%, 15 alive | wipe 5:18.3, Vezax 22.9% |

Tanks held the Animus from +1.0 s (96.9-98.8%), Heroism at +0.0-0.4 s, Surge of Darkness at +13 and
+76 s in all three. uld1 is window 25's one north settle and still killed it.

**Verdict on round 12 (Beacon of Light and Sacred Shield on the main tank): it worked.** `--tank`: Beacon
on him 100% before the spawn and 96% (uld1) / 100% (uld2) after, copies 159k and 50k landed on him,
Sacred Shield on him only (5, 6), 4 Beacon casts a pull; `vezax tank buffs on the holder` vetoed only
the holy paladin's class `beacon of light on tank` and `sacred shield on tank` (69, 10). Both main tanks
lived through the phase, healed 3.0 and 2.6k/s against 2.9 and 2.7k/s taken (window 24 uld2: Beacon 0%
on him, dead at +53 s). uld3 has no holy paladin.

**Verdict on round 13 (a crash after the Animus-phase mark turns the block): it worked, short of the
target in uld2.** `--markcrash` 3:52 marks:

| Pull | Marked | Crash | Ticks / on its block / after the cast | Marked walk | Blockmates with / away | Vezax %/s | Window 24 |
|---|---|---|---|---|---|---|---|
| uld1 | Uldxad (heal R) | R, +1.5 s | 15 / 9 / 9 | spot | 0 / 6 | +0.41 | 49 / 34, +1.20 |
| uld2 | Uldyae (heal R) | R, +1.6 s | 24 / 12 / 12 | spot | 0 / 6 | +0.64 | 40 / 27, +0.86 |
| uld3 | Uldzas (melee) | - | 16 | south | - | +0.43 | 22, a healer |

`vezax.dodge = mark` 1 bot and `pulled away`/`away` 6 in each; no crash hit in either phase, so the
turned block cost nothing. uld2's 24 (`leechwho.py`, `campat.py`): the marked healer finished a Chain
Heal and walked only at +0.84 s, with all seven R members stacked on one field centre, so the ticks at
+2.3 s (13 bots, both blocks) and +3.2 s (R's six strafing away, 15.5-18.7 yd) came before it was
16.5 yd clear. Marks starting a walk over 0.4 s late (`markdelay.py`, windows 11-25): 26 of 210, early
ticks median 12-17 against 11-15; not chased.

**Why uld3 lost the race (`animusbins.py`).** The same roster killed it in 79 s in window 24 at 1.27 %/s
(156k: 76 ranged, 75 melee); here 1.17 %/s (146k: 69, 72), and the Animus died at +87.3 s, 7 s after
the first Profound Darkness deaths. Every dps targeted it 93-100% of the phase. The ranged gap (−7.7k/s)
sits in block R: fire mage Uldzaq 4.3k/s against 8.5 (dry at 4:12.5), elemental Uldzan 6.5 against 8.4,
hunters Uldzak 3.5 / 5.1 and Uldzau 3.3 / 3.8 (pets not counted); R's ranged stood in a field 42-56% of
the phase against 62-70% for L, and 5 of the 7 phase crashes landed on L, so R had fewer fields of its
own. Melee lost 3.4k/s, the cat druid
Uldzas most (4.8 against 7.1, marked at 3:52 with far 7-9 as crossed and pulled R slots stood inside
the floor: Q1 (a)'s accepted cost). Healers (no holy paladin in roster C) hit bottom first: Uldzaf
under 5% at 4:09, Uldzae 0 at 4:26.5. **Searing Flames missed no kick in the race**: 19 kicked before
3:00, none cast while the Barrier stood; after the Animus died at 4:35.7 Vezax (24.3%, tankless from
4:34.5) roamed north after ranged, and of five casts two were kicked, then Uldzaj's Wind Shear failed
15 yd from him and the kickers were dead for the last two: they landed at 4:59.6, 5:07.6 and 5:15.6,
the first killing 7. The waived gear race, lost by ~7 s; not changed.

**Why the uld1 melee died at 4:34 (`window.py`, `rows.py`, `far.py`, `surgemark.py`).**

1. *Surge of Darkness opened the mark to melee.* 62662 carries `MOD_SCALE` +35%
   (`spell.reference.csv`: auras 33, 79, 61), so his combat reach grows to 17.0 and the floor to 33.5
   yd for 10 s. At the 4:32.30 pick 14 stood past 29.1 but 8 past 33.5, so the core drew from everyone
   inside it (`boss_general_vezax.cpp:239-261`) and took frost DK Uldxah, 12.4 yd from him. Hard mode
   delays the mark and surge timers 12 s at the sixth vapor, so the 4:32 mark lands in the 4:24 surge
   in every pull (normal mode: 4:20 and 6:20): 24 of 30 such marks in windows 11-25 went inside 33.5,
   11 to melee, 3 to a tank, against 3 of 200 outside a surge, each with 9-10 far by the snapshot.
2. *The first tick was unavoidable.* Under 42 Profound Darkness stacks the melee beside Uldxah were at
   9-32% when it landed; the break sent them east and west at 4:32.6-4:32.7, but the tick cast at
   4:33.51 (landing 4:34.03) caught three 5-7 yd out (7-10 had Uldxah walked) and killed Uldxat and
   Uldxai.
3. *The marked walk stalled (bug).* Uldxah's mark walk south (to (1846.2, 67.0), 32.6 yd) went out at
   4:32.522; at 4:32.745 its own `healthstone` (FAILED) and `healing potion` ran, and
   `UseItemAction::UseItem` (`UseItemAction.cpp:337-342`) stops a moving bot for anything but a potion
   and idles its AI a global cooldown. It stood at (1860.0, 94.8) from 4:33.0, 1.6 yd out, and every
   mark walk to 4:37.5 read `dup` (30 of them): `MoveTo` refuses an exact repeat for 5 s
   ([pitfalls](../../../engine/pitfalls.md#movement-that-silently-no-ops)). The 4:34.51 tick killed
   Uldxax (15.6%, 13.7 yd east) and Uldxai after Reincarnation (standing on its stale break booking,
   `wait`). Walked at 7 yd/s, Uldxah stands 22.8 and 19.9 yd from them at that tick: 2 of the 4
   deaths.

`markstall.py` (windows 11-25): 10 of 223 marks with a walk stalled over 0.5 s short of the spot, those
leeching a median 29.5 ticks against 13; `stonestop.py`: 70 healthstones tried on a walking bot, 58
stopped it, nearly all in collapses (soak 20, melee spot 9, bring-back 9, reach 13, position 5, mark
1, dodge 1).

**Ranking.** 1. uld3's collapse, 30 (the waived race, above). 2. uld1's leech, 4: the surge floor (a
position question, Q4) and the stalled walk (bug, 2 of them). Nothing else died.

**Picked: the stalled escape walk (bug).** The strategy sends a marked bot to its spot, a breaking melee
clear of it and a dodger out of the blast; a healthstone stopped the walk and the repeat guard kept it
stopped.

**Fix** (Vezax files only):

- Rule `vezax escape healthstone hold` (`UldDefinition_Vezax.cpp:80`, predicate `VezaxEscapeUnderway`,
  `UldEncounter_Vezax.cpp:780`): `healthstone` holds while the bot is moving and is marked (not his
  victim), breaking off a marked ally (the break trigger's own test) or within 11 yd of a pending crash
  impact. A potion is usable on the move, but the veto pushes the node's `healing potion` fallback at
  relevance 0.003, so it waits out the walk too.
- `VezaxMarkOfTheFacelessAction` and `VezaxMarkOfTheFacelessBreakAction` call `ReleaseStalledWalk`
  (the dodge's `ReleaseStalledDodge`, renamed; `ULDUAR_VEZAX_WALK_STALL_MS` 500): a bot standing still
  500 ms past its last move gets `last movement` cleared, so the walk is re-sent instead of `dup` or
  `wait`.
- Reader: `--mark` counts `far` past 33.5 for a mark inside Surge of Darkness and prints `surge`
  (`surge_starts`, `in_surge`; test added); uld1's 4:32 mark now reads 8/9 `yes`.

Docs: `docs/raids/ulduar/vezax.md` (Surge row, floor bullet, escape paragraph, trace keys, two known
gaps). 10/25 and normal/hard: the mark, break and crash run in every mode and nothing keys on size or
the Animus. Bots only (a real player runs no rule or action), so the same with or without one. Shared
code: none.

**Next window should show.**

- No `healthstone` act on a bot moving marked, breaking or inside a crash's 11 yd (`stonestop.py`: 0
  for mark, break and dodge walks); `vezax escape healthstone hold` vetoes only there.
- `markstall.py`: no marked bot standing still over 0.5 s short of its spot with `dup` re-issues; a
  marked melee at 4:32 reaches its south spot in ~4-5 s and the leech after its first 2 s spares the
  melee.
- `--mark`: the 4:32 mark `surge yes` with far under 9 in most pulls (unchanged; Q4).
- Round 12 and 13's checks hold: Beacon on the main tank, the 3:52 turn `spot` / `away`.

**Q3 judged on window 25 (`q3.py`).** Ticks on the marked bot's block from the crash's cast on:

| Window 25 | Cases | After-cast ticks on the block |
|---|---|---|
| pre-spawn | 12 (crash +2.5-8.8 s) | 14: uld1 0/0/0/0, uld2 7/0/6/0, uld3 1/0/0/0 |
| post-kill | 3 (+4.5-7.7 s) | 0 |
| Animus phase, turned | 2 (+1.5-1.6 s) | 9, 12 |

uld2's 7 came from a crash on the other block (the marked block never dodged, the walk-out time) and
its 6 from one aimed at the marked bot at +5.7 s; (b)'s subset (own block or the marked bot inside
~4 s) held 2 cases, 0 and 1. Windows 11-25: (b)'s subset is the 0:20 mark (crash +2.3-2.6 s), 15
cases, mean 9 (w24 uld1's 32 the top); the other 130 pre-spawn and post-kill cases a mean 1.6; the 16
unturned Animus-phase ones a median 24. *Recommended: (c)*, keep the turn to the Animus phase.

**Questions (user).**

- *Q3 (from round 13).* Turn the block for a crash after a mark outside the Animus phase too? (a) any
  crash inside a mark; (b) only crashes aimed at the marked bot or its block in the first ~4 s; (c)
  keep it to the Animus phase. *Recommended: (c)*: outside it the block took 0-7 ticks from the cast on
  in window 25 (14 over 15 cases), against 9-12 with the turn and a median 24 without it in the Animus
  phase.
- *Q4. Surge of Darkness moves the Mark floor from 29.1 to 33.5 yd for 10 s, and hard mode's 4:32 mark
  always falls in one: rows 0-1 are inside, so the mark draws from the whole raid (24 of 30 such marks
  in windows 11-25, 11 on melee; uld1's 4 deaths). Change the camp for it?* (a) Ranged dps step 4.4 yd
  out from their slots for the surge, healers stay: only outside the Animus phase, since crossed slots
  must stay inside the Animus's 37 yd ceiling, so it misses most 4:32 marks. (b) The whole camp steps
  out: row 0 then stands 44+ yd from the main tank while Surge doubles his melee. (c) Keep: with this
  round's fix a melee mark costs the first tick on the melee next to it. *Recommended: (c).*

**Review.** The cause holds (`vezax-r14-review/walkstall.py`, `gemstop.py`): w25 uld1 Uldxah's mark
walk went out at 4:32.522, its `healthstone` FAILED at 4:32.745 with the bot moving, and every mark
walk read `dup` until the 4:37.426 re-issue. In the 45 newest traces a healthstone stopped a mark or
dodge walk short four times (31.1, 10.4, 3.2, 1.2 yd). No size, mode or player branch, no shared code,
no comments. `pb-check` passes.

- Review fix: Surge of Darkness has a 3 s cast and his events wait it out, so its scale holds from the
  cast's end; `--mark` took it from the cast start (`surge_starts` adds the row's `ct`; test added).
  Window 25 reads the same.
- Review fix: a marked bot that is his victim but not the main tank skipped the crash test in
  `VezaxEscapeUnderway`; it falls through to it now.
- Review fix: the fix list said potions stay; the veto takes the potion fallback down with it.
- Left: `mana sapphire` (relevance 90 on `high mana`, no cooldown test in `UseManaGemAction`) stops a
  walking mage the same way, but in those traces it never left a mark or dodge walk short (17 with
  the mage still after it, all on its spot); not held.

**User answers (2026-11-24):** Q3: (c), the crash turn stays Animus-phase only. Q4: (c), the camp stays
as is for Surge of Darkness. Neither is built.

## Round 15 (2026-11-24, build e834a2708)

**Data.** Window 26 at 25n hard mode (`raidtest.py report --boss vezax --raid uld --head e834a2708
--pulls 10`), one valid pull per pool, hard mode held in all three (`animus` at 3:08.4-3:08.5, no vapor
hit), against window 25 (`--head 3ce456519`) and windows 11-25 for the surge and interrupt tallies.
Analysis scripts: session scratchpad `vezax-r15/` (`timeline.py`, `mtdeath.py`, `healers.py`, `after.py`,
`acts.py`, `ondamage.py`, `kicks.py`, `surges.py`, `bigcds.py`, `replay.py`, `sfkicks.py`, `kickfails.py`,
`walkstall.py`, `stonestop.py`, `markstall.py`, `vetoes.py`).

| Pool | Run | Trace | Result |
|---|---|---|---|
| uld1 | `tr-20261124-120707-5` | `603_5_general-vezax_1795522069` | kill 5:39, 0 deaths, the Animus dead in 79.2 s |
| uld2 | `tr-20261124-120647-4` | `603_4_general-vezax_1795522049` | kill 5:00, 0 deaths, the Animus dead in 72.4 s |
| uld3 | `tr-20261124-120748-6` | `603_6_general-vezax_1795522109` | wipe 6:13, 31 deaths, the Animus dead in 79.2 s and the main tank 5 s later |

**Stop rules.** Not polished: uld3 wiped. Round 14's pick did not recur and this round's are new, so no
repeat count.

**Roster note.** uld3's main tank Uldzaz is class 6, a blood DK (Rune Tap, Vampiric Blood, Icebound
Fortitude, a ghoul), not the bear rounds 3 and 12-14 named; roster C's off-tank is the protection
paladin Uldzaa (class 2), as roster A's is.

**Deaths by mechanic** (last hit; re-raised bots die twice).

| Pull | Deaths |
|---|---|
| uld1 | none |
| uld2 | none |
| uld3 | all from the main tank's death at 4:32.7: 19 Vezax melee (the main tank in the late surge, then 7 ranged, 9 melee and the off-tank twice as he went down the threat list), 10 Searing Flames (one cast, 5:09.65), 1 Shadow Crash (the soulstoned main tank, 4:39.9), 1 Mark leech (a healer, 4:43.4) |

**Animus and Vezax per pull** (`timeline.py`; settle is Vezax from the anchor before the spawn).

| Pull | Settle | Vezax at spawn | Animus +30 / +45 / +60 / +75 s | Animus | Raid / ranged / melee k/s | Vezax at its death | End |
|---|---|---|---|---|---|---|---|
| uld1 (DK) | 9.9 | 21.3% | 63.9 / 45.3 / 27.3 / 8.7 | dead +79.2 s, 1.26 %/s | 155 / 87 / 65 | 25.8% | Vezax dead 5:39.0 |
| uld2 (warrior) | 9.9 | 14.8% | 59.4 / 38.6 / 17.9 / - | dead +72.4 s, 1.38 %/s | 169 / 93 / 70 | 18.3% | Vezax dead 5:00.0 |
| uld3 (DK) | 9.9 | 21.5% | 60.6 / 40.8 / 22.4 / 6.1 | dead +79.2 s, 1.26 %/s | 156 / 78 / 74 | 24.5% | wipe 6:13.3, Vezax 23.15% |

All three settled normally and won the race; uld3 lost after it.

**Verdict on round 14 (healthstone held on Mark, break and dodge walks; a stalled walk re-sent): it
worked.**

- `walkstall.py` (round 14 review's): 1,251 escape walks (1,042 dodge, 133 break, 76 mark), none
  stopped by a healthstone or mana gem (4 in round 14's 45 traces). The 13 it flags as standing short
  are dodges that yielded clear of the blast at 4:27-4:30 and one mark in uld3's last seconds.
- `markstall.py`: 24 marks, none with its walk standing over 0.5 s short of the spot (windows 11-25: 10
  of 223), median 12 ticks.
- `stonestop.py`: 2 healthstones tried on walking bots, both outside an escape (reach melee, soak) in
  uld3's collapse; `vezax escape healthstone hold` vetoed nothing, since none came up during an escape.
- The 4:32 surge marks: uld1 on melee Uldxal (south, far 6/9), 15 ticks, no death (window 25 uld1: 4
  melee dead); uld2 on healer Uldyad, 5 ticks; uld3 on melee Uldzao just after its main tank died, 40
  ticks (12 in the first 4 s) with Vezax loose.
- Rounds 12 and 13 hold: Beacon on the main tank 99-100% before and after the spawn (uld1, uld2; uld3
  has no holy paladin); the 3:52 crash turned the marked bot's block away in all three (with/away 0/6,
  0/6, 0/5), 12-17 ticks.

**Why uld3 wiped: the main tank died in the late Surge of Darkness, 5 s after the Animus.** The DK
Uldzaz (42,004 health) held Vezax and the Animus on the anchor from +1.0 s. The Animus died at 4:27.79
with Vezax at 24.5%, and the surge cast at 4:24.25 doubled Vezax's melee over 4:27.2-4:37.2.

- *Damage taken* (`mtdeath.py`), 4:13.5 to his death at 4:32.74: 83.1k, Vezax melee 45.3k (16.4k at
  4:29.1 and 14.2k at 4:32.7 in the surge, 7-8k a hit before it), Animus melee 20.6k, Profound Darkness
  17.2k (39 stacks at his death).
- *Healing*: 49.7k in that span, 45.3k his own (Death Strike, Rune Tap, Vampiric Blood, healthstone,
  potion). Roster C has no holy paladin: from 4:08 the four healers healed 292k and put 13.1k on him,
  standing a median 40-44 yd off (`healers.py`). uld1's holy paladin alone put 49.1k on its DK over the
  same span, Beacon on him.
- *Mana*: healers at the spawn 58 / 49 / 48 / 31% (uld1 48 / 74 / 68 / 46), at 4:28.5 6 / 21 / 2 / 0;
  the resto druid dry from 4:03.
- *Cooldowns*: Icebound Fortitude at 3:24.2 by the surge node on the phase's first surge, at 100%
  health, so on cooldown until 5:24; Rune Tap 3:56.1 and 4:31.4; Vampiric Blood, healthstone and potion
  at 4:19.0-4:20.2 with him at 14-28% under the Animus; the surge node FAILED at 4:27.4 with nothing
  left. Pain Suppression (4:16.6) and Guardian Spirit (4:17.0) both went on the disc priest Uldzac.

**Across windows 11-26** (`surges.py`, `bigcds.py`, `replay.py`). Hard mode's surges come at +13 and
+76 s of the Animus. Every DK and warrior main tank spent Icebound Fortitude or Shield Wall (2 min) on
the first, at 51-100% health, or on a class low-health node at +3 to +11 s (5 pulls), and met the late
one without it: 8 of the 24 that reached it died inside it (DK w15 uld3, w17 uld1, w23 uld3, w25 uld3,
w26 uld3; warrior w14, w15, w17 uld2), the rest bottomed at 17-71%. Replaying the first surge without
the cooldown (damage taken divided by its cut, heals as landed) puts his low at a median ~34%, 10 of 32
under 25% and 4 under 0. Which surge gets it is a trade, not a broken intent: a plan question (Q5).

**Who should have picked Vezax up: nobody could.** He is taunt-immune here (immunity set −427, −287's
`ATTACK_ME` and `MOD_TAUNT` plus spell haste, `2026_07_05_00.sql`), so only threat moves him, and
`VezaxThreatMultiplier` (`UldMultipliers_Vezax.cpp:88-97`) drops its cap once the main tank is dead
(`GetGroupMainTank` returns null), so the dps were uncapped. The off-tank Uldzaa, at 2-4% mana since 0:23,
stood in melee (13.2 yd) at 4:32, but the 4:32.65 mark went to melee Uldzao (the surge floor, Q4's
accepted cost) and its break walked Uldzaa to 22-32 yd; back in reach it cast Righteous Defense at
4:36.4 and every 8 s after, 7 casts to no effect. Its damage on Vezax over the pull was 114.5k against
1.06-2.07M for every dps (`ondamage.py`, `dealt` while targeting him). The main tank came back on a
soulstone at 4:33.1 at 14% with no threat; his Death Grip did nothing, his slot walked him toward the
anchor while Vezax chased ranged north, a crash aimed at the off-tank killed him at 4:39.9, and Rebirth
brought him back at 17-18%, 15-52 yd from Vezax, until Searing Flames. No change proposed: the main
tank has to live.

**What followed** (`after.py`). Vezax took the top-threat ranged and walked 9.9 → 47 yd from the anchor
by 4:39, killing Uldzap 4:34.7, Uldzal 4:36.5, Uldzaq 4:40.9 and Uldzar 4:42.8; the 4:32 mark's leech
among the melee killed healer Uldzad at 4:43.4 and put him from 23.7 to 29.3%; the melee went one by one
from 4:44.6. The Searing Flames cast at 5:07.63 landed on 15 and killed 10 (below). The off-tank held him
from 5:38 to its death at 5:58.8, second Heroism at 5:40.6, wipe 6:13.3 with Vezax at 23.15%.

**Searing Flames with the duty on a bot that could not kick (bug, 10 deaths).** `vezax.interrupter` sat
on Uldzad (resto shaman, the lowest GUID with a kick) from 0:10. Reincarnated at 4:43.7 with 0 mana, it
still read ready, so at 5:07.72 the duty was its: Wind Shear (8% base mana) FAILED 31 yd from him, and
no other bot kicked though five melee with Kick, Pummel or Mind Freeze stood 4-8 yd from him.
`VezaxReadyInterrupt` (`UldEncounter_Vezax.cpp:1235` before) asked `CanCastSpell` alone, which probes
with `TRIGGERED_IGNORE_POWER_AND_REAGENT_COST` and passes `SPELL_FAILED_OUT_OF_RANGE` and
`NOT_SHAPESHIFT`; [casting](../../../engine/action-selection.md#casting) already says a duty pick must
test cost and form itself. Windows 11-26 (`kickfails.py`): 9 duty kicks FAILED, 5 by shamans at 0-1%
mana, 2 by rogues short of Kick's 25 energy (4% and 20%), 1 by an elemental shaman 61 yd off, 1
unexplained; 4 casts landed in all, the other 3 in w25 uld3 with no kicker alive in reach.

**Ranking** (31 deaths, all uld3).

1. *The main tank's death in the late surge and the loose Vezax after it: 31*, 10 of them the Searing
   Flames below. Causes: no long cooldown left for the late surge (Q5), roster C's healers dry and 40-44
   yd off (gear, waived), and nobody able to take a taunt-immune Vezax back (no change proposed).
2. *The Searing Flames duty on a bot that could not kick: 10 of those 31.* Bug, small, independent.

**Picked: the interrupt duty (bug); the late surge goes to the user (Q5).**

**Fix** (`UldEncounter_Vezax.cpp` only): `VezaxKickInReach` (`:1237`) counts a kick as ready only when
it is off cooldown, the bot's stance allows it (`CheckShapeshift`), the bot can pay its cost and Vezax is
in its range (melee range for a melee kick, else combat range at the spell's own); `CanCastSpell` still
follows. `VezaxReadyInterrupt` (`:1255`) uses it for the bot's own kick and for the GUID-ranked duty
pick, so a dry, short or distant bot passes the duty on, and an arms warrior's Pummel outside Berserker
Stance no longer wins it. Docs: `docs/raids/ulduar/vezax.md` (duty paragraph; known gaps for the late
surge and a main tank death).

10/25 and normal/hard: Searing Flames comes in every mode (every 8 s at 25, 15 s at 10, never under the
Barrier) and nothing keys on size or the Animus. A real player is never picked (no bot AI), as before.
Shared code: none.

**Next window should show.**

- `--boss`: Searing Flames landing only with no kicker alive in reach; `kickfails.py`: no
  `vezax searing flames interrupt action` FAILED with the bot dry, short of energy or out of range.
- `vezax.interrupter` moving off a shaman healer once it runs dry (a melee kicker holding it late in a
  long pull).
- Round 14's checks still holding: no escape walk stopped by an item, no mark walk standing short.
- Until Q5 is answered, the late surge unchanged: the main tank without Icebound Fortitude or Shield
  Wall at +79 s (`bigcds.py`).

**Questions (user).**

- *Q5. Hard mode's late Surge of Darkness (+76 s of the Animus, healers at 0-35% mana, ~38 Profound
  Darkness stacks) found every DK and warrior main tank without Icebound Fortitude or Shield Wall: both
  come back after 2 min and went on the first surge at +13 s. 8 of 24 died in it, uld3's wipe among
  them. Which surge gets the cooldown?* (a) The late one: hold it, surge node and class nodes alike, from
  the spawn until the late surge, except under 25% health; replayed, the first surge then takes him to a
  median ~34%, 10 of 32 under 25%, where the hold lets go and the late one is bare again. (b) Keep it on
  the first, and add a 1-minute button to every surge (Vampiric Blood for a DK, which the surge node
  lacks) plus a priest's Pain Suppression or Guardian Spirit on him for the late one, held for it: in
  uld3 both went on a disc priest 10 s before his death. (c) Keep: the late surge follows the healers'
  mana, and uld1's DK lived through it on a holy paladin's healing. *Recommended: (b)*: without its
  cooldown the first surge is about as deadly as the late one, and the external cooldown is what dry
  healers can still give at the late one.

**Review.** The cause holds (`603_6_general-vezax_1795522109`): `vezax.interrupter` read 1 on Uldzad from
0:09.98 and never changed, its interrupt action FAILED at 5:07.72 with the snapshot at 0% mana 31.1 yd
from him, and the cast's 10 deaths came at 5:09.65, while Uldzai and Uldzam (Mind Freeze, 48-55% runic
power), Uldzat (Kick, full energy) and Uldzaj (Wind Shear, 93% mana) stood 4.2-8.2 yd from him. Every kick
in the list is off the global cooldown (`StartRecoveryCategory` 0), so the `CanCastSpell` tail does not
flip the pick each GCD. No size, mode or player branch, no shared code, no comments. `pb-check` passes.

- Review fix: a melee kick's range read `IsWithinMeleeRange` with no extra, but `Spell::CheckRange` adds
  the kick's 5 yd less two `MIN_MELEE_REACH` (less one while both units move): 1 yd short standing, 3 yd
  short with leeway, so a melee chasing him after a main tank death read out of reach with its kick in
  range. It now mirrors the core.
- Review fix: the duty paragraph in `docs/raids/ulduar/vezax.md` named only cost and range as what
  `CanCastSpell` skips (stance too), and stated the test rosters' GUID order as general.

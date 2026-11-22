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

# g8-putricide — close Professor Putricide's gaps

Gap wave `3cd-gaps`, after `3g`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap lanes"
section included. Builds on the merged `r8-putricide` lane and its review: read
[r8-putricide.PLAN.md](r8-putricide.PLAN.md) first. Its decisions stand unless closing a gap needs a
change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/professor-putricide.md` and in r8's
brief.

- One bot tank: nobody drives the abomination. The guides and the vehicle's spells decide who else
  can drive; the Gap lanes' one-bot-tank rule applies to holding him.

## Owns

Stem `Putricide` and its definition, the doc, the reader `professor_putricide.py` and its test, this
brief.

## Task list

One lead, no split: every change sits in the stem's one state, scan and election
(`ICCHelpers_Putricide.cpp`), with thin row and rule edits.

Facts the tasks rest on (core, DBC, DB as of this lane):

- No 631 trace on disk. The user built `623638b99` clean on 2026-10-29.
- The ooze and the cloud carry immunity set -362 (SNARE, ROOT, STUN and more): no player slow lands; Regurgitated
  Ooze's slow (aura 33) has no mechanic.
- The abomination is a minion of its driver (vehicle 591): `ExitVehicle` dismisses it, so the table can be
  drunk again. Health 630k (10N), 945k (25N), 441k (10H, 25H). While it lives, 70402/72511/72512/72513 hits
  every player but its driver for 1950-2050/2925-3075/2925-3075/4388-4612 every 2 s (SRC_AREA_ALLY, 50000 yd,
  players only).
- His swings: (177.07 + AP 805 / 14 × attack time) × DamageModifier 87/176.4/118/243.4, ×1.5 base max, every
  1.5 s; Mutated Strength (P3) +50% damage and attack speed: 10N 30.6-42.2k a second before armor.
- Mutated Plague lasts 180 s per stack refresh and is instant; taunt DR runs full, 65%, 42%, 27%, immune, and
  resets 15 s after the last taunt aura ends (`Unit::GetDiminishing`).
- The Unbound Plague searcher 70917 ticks every 500 ms (first jump 1 s in); 70920 searches 3 yd.
- Ooze Eruption 70492: 10 yd round the ooze; its search 70459 5 yd round the ooze.
- The goo listener keeps an impact until the next cast evicts it at 9 s old, so impacts outlive 9 s between
  casts (25-30 s apart).

Tasks:

1. [x] One bot tank: a bot damage dealer drives when no bot tank is free (melee, Death Knights and paladins
   last of them, then ranged, never a healer); the driver hold covers every role and passes the plague walk;
   the driver holds no slot.
2. [x] A riding tank exits (`pp.abom exit`) when he is off every tank and no other tank stands on foot, and a
   tank driver on foot is dropped then; the exit dismisses the minion, so the table serves a new driver.
3. [x] No tank alive: a bot Death Knight or paladin damage dealer picks him up (`pp.swap pickup`) and keeps him.
4. [x] Sharer survival (`pp.sharehp`): two of his swings at his own min/max damage, after the sharer's armor,
   damage taken auras and its ready button's cut, must leave 25%; the button goes up from 2.5 s, the taunt
   within 1.2 s of the application; without the button up by then it must survive bare or drops out.
5. [x] Hold ceiling: a hold ends at 3 stacks once it has taken one, when a taker has fewer and DR is under
   level 3.
6. [x] Reverse pass: an anchored carrier (his holder, which keeps its drag stand, the rooted ooze victim, a
   rider) has its receiver walk onto it (`pp.plague receive`); stackers keep 6 yd off a plagued ooze victim on
   the ooze's side, clamped to the probed disk.
7. [x] Goo flight timed from where he stood when the impact first showed; impacts kept 16 s.
8. [x] Rings held from his first arrival on the spot (`pp.rings`).
9. [x] Closed by analysis in the doc: kite slows, heal reach to a far kiter, the abomination's raid hit and
   hazards, sharer classes.
10. [x] Reader `--abom` exit, `--plague` share models and receivers, `--spread` rings; tests; doc. All 646 slots
    and the drink stand re-probed (floor, sight, path).

Review fixes: a holder carrying the plague kept no tank stand, and its duty hold would have fought the stand
hold, so it couldn't drag him off a puddle; the plagued-victim stack spot wasn't normalized with the bot on the
victim and could leave the probed disk.

Independent review fixes: the back taunt waited out the sharer's 3 s taunt aura (any taunt aura blocked the
election), so a sharer took three or four P3 swings against the model's two; now only an aura from someone he
isn't hitting blocks it. A Death Knight or paladin driver on foot elected guard kept walking to the table with
him: a driver he hits is dropped, and guards rank the driver last. A riding carrier kept eating while its pass
was due, parking inside a puddle its receiver's dodge won't enter.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| With no bot tank free a damage dealer drives: melee (Mutated Slash replaces its swings), Death Knights and paladins last of them, then ranged, never a healer | Gap lanes' one-tank rule; the guides' off-tank duty needs no taunt; a free Death Knight or paladin can pick him up | Nobody drives, so puddles fill the room by P3 |
| Drive although the abomination hits every player but its driver for 1950-2050/2925-3075/2925-3075/4388-4612 every 2 s | Every guide drives it; a puddle hits 4.4-7k a second and grows to 27.5 yd | No abomination |
| A riding tank exits while he is off every tank and no other tank stands on foot; a tank driver on foot is dropped then, kept while he is passive at his table | Vehicle 591's summon is a minion of its driver, so `ExitVehicle` dismisses it and the table serves a new driver | A damage dealer holds him while the tank rides |
| No tank alive: a bot Death Knight or paladin damage dealer takes him, Death Knight then healthiest, and keeps him | Gap lanes' rule (a damage dealer with a class taunt); Dark Command and Hand of Reckoning need no form or stance | He roams the healers |
| Sharer survival model: two swings at his min/max average, `CalcArmorReducedDamage`, damage taken auras, the button's own MOD_DAMAGE_PERCENT_TAKEN cut, 25% left (`RAID_DEFENSIVE_FLOOR_PCT`) | A dead sharer heals him 300k-2.1M a stack; P3 swings run 30.6-42.2k a second before armor on 10N, about twice that on 25-man | The old gate, 80% health with a button ready |
| Share taunt moves to 1.2 s before the application, the button from 2.5 s | Wowhead: taunt 1 s before; the window is the lead plus a scan and the tank's taunt back, 1.8 s, two P3 swings | Taunt at 2.5 s (three swings) |
| Sharers stay Death Knights and Retribution paladins | Taunt needs Defensive Stance (and Shield Wall a shield), Growl Bear Form; encounter code changes no form or stance | Wowhead's fury warriors and ferals |
| Hold ceiling at 3 after one application, a taker with fewer stacks, DR under level 3 | WT: never 4, delay 3; script: 3+3 costs 31% less a tick than 4+2; a 10 s hold leaves the next taunt at 65% duration | r8's holds of 2 only, reaching 4 on the third hold |
| An anchored carrier's receiver walks onto it; his holder keeps dragging him meanwhile | 70920 searches 3 yd plus reach; a holder walking would drag him onto the receiver | The carrier waits (rooted) or walks him (holder) |
| Stackers keep 6 yd off a plagued ooze victim, on the ooze's side | Ooze Eruption hits 10 yd round the ooze, which erupts once within 5 yd of its victim (70459) | Stack at 3.5 yd and catch the plague |
| Goo timed from his position at the first scan after the cast, impacts kept 16 s | Scans run every 200 ms; the listener keeps an impact until the next cast evicts it at 9 s old | Timed from where he stands now, dropped at 9 s |
| Rings hold from his first arrival within 10 yd of the spot | He is pulled at his table 51 yd from the centre | Opening positions round the table |
| No kite slow but Regurgitated Ooze, no Hand of Freedom duty | Immunity set -362 (snare, root) on the ooze and the cloud; nothing slows the kiter | Player snares |
| Healers keep heal reach rather than a slot following the kiter | Several healers on a 12 yd ring; one walks when the kiter passes 40 yd | Rotface's lone-healer slot |
| The driver ignores hazards | 630k/945k/441k/441k health; eating means standing in the puddle | Abomination dodges |
| Review: the tank takes a sharer's stack back inside the sharer's taunt aura | `ThreatManager::TauntUpdate`: the latest taunt wins; Wowhead's tank taunts back 2 s after the sharer | Wait out its 3 s aura and model three or four P3 swings |
| Review: a riding carrier stops eating while its pass is due, slashing its target instead | Eating parks it inside a puddle; the receiver's dodge won't enter one | The receiver stands in the puddle for the pass |

## Carried over

- Merge stage, ICC README: the Malleable Goo listener bullet says Putricide times each landing from the cast
  time and keeps it 9 s; now from where he stood at the first scan after it, kept 16 s. The per-instance state
  paragraph's Putricide list gains `pp.rings` (`ObsValue`). The taunt paragraph: "a taunted unit holds the
  taunter for the 3 s aura" holds against threat only, a later taunt takes it at once
  (`ThreatManager::TauntUpdate`); Putricide's swap is "held for two Mutated Plague applications, or to 3
  stacks after one while taunt DR is under level 3".
- Merge stage, `icc-rebuild.PLAN.md` Status: the r8 row's "one driver (one bot tank: nobody drinks)" and "hold
  counts applications (back at 2 holds to 4)" are closed here.
- r14, unchanged from r8: `ppBloatLastDir` and `BloatDir` in `ICCScripts.h`, `NPC_GROWING_OOZE_PUDDLE`,
  `NPC_CHOKING_GAS_BOMB`, `NPC_MALLEABLE_OOZE_STALKER` and `SPELL_GROW_AURA` in `ICCData.h`; hoist the walk base
  (`IccPutricideWalkAction`) and `IsTankPlayer`/`IsDutyBot`/`MembersOnMap` shared with other stems.
- Rebuild needed, no CMake re-run (no file added or removed).

## Known gaps

- Those in the boss doc.
- No 631 trace; the user built `623638b99` clean on 2026-10-29; no `-Werror` pass of this lane's code.
- The sharer model counts no crushing blows, heals or avoidance, and likely passes every 25-man sharer over:
  `pp.sharehp` against the health sharers really kept settles it.
- A tank that exits the abomination picks him up only once within 30 yd of him.
- A riding carrier slashing a target that stands in a puddle still draws its receiver into it.

## Blocked

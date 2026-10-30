# g10-blood-queen — close Blood-Queen Lana'thel's gaps

Gap wave `3efg-gaps`, after `3cd-gaps`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap
lanes" section included. Builds on the merged `r10-blood-queen` lane and its review: read
[r10-blood-queen.PLAN.md](r10-blood-queen.PLAN.md) first. Its decisions stand unless closing a gap
needs a change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/blood-queen-lana-thel.md` and in r10's
brief.

- Mass Dispel after Incite Terror: `RaidAntiFear` (`src/Ai/Raid/RaidAntiFear.h`) is shared, so the
  answer is ICC-local or reported.
- One bot tank: nobody stands on the main tank for Blood Mirror. The Gap lanes' one-bot-tank rule
  applies.

## Owns

Stem `BloodQueen` and its definition, the doc, the reader `blood_queen_lana_thel.py` and its test,
this brief.

## Task list

One lead, no split: every task edits the stem's one helper file and state.

1. One bot tank (`AssignTanks`): with one tank player in the raid, human or bot, the sturdiest damage
   dealer takes the mirror role (`bql.tank` 2): plate melee (DK, warrior, paladin), then other melee,
   then ranged, most health, latched. It stands on the main tank under the tank hold (now any role),
   skips groups, nodes and CC, and spends `NextPhysicalDefensive` under 40% and in the last minute.
2. Mind control: a vampire no bot can control for 2 s joins `bql.kill`; non-vampire damage dealers
   resolve it (`bql.target` `kill`), drop their stands until it dies, and `icc bql splash` holds only
   round controlled ones.
3. Incite Terror: priests ward themselves (ICC-local overrides of the anti-fear trigger and action),
   then `icc bql mass dispel` casts on the feared player with the most feared within 15 yd, at least 3,
   within 30 yd and in sight, priests splitting clusters (`bql.massdispel`).
4. Bite and Pact walks detour round shadows (`GetBqlWalkVia`, `bql.detour`).
5. Pact with a human member still away 3 s in: bots walk to him (`bql.pact` `human`).
6. Three navprobed dais-edge ground nodes: 30 ground, 45 in all.
7. Reader and tests for the new keys and values (`--cc` kills, new `--fear`, `--spread` detours), boss
   doc, this brief.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| One tank player in the raid: the sturdiest damage dealer is the mirror (plate melee, then other melee, then ranged; most health; latched), physical defensives under 40% and in the last minute | Gap lanes' one-bot-tank rule; WT, Wowhead: the off-tank soaks Blood Mirror in stamina gear; script: Blood Mirror links her victim's nearest player; she has `NO_PARRY_HASTEN`, so a melee mirror in front only loses its own parried swings | nobody: Blood Mirror lands on whoever stands nearest |
| No damage-dealer mirror while a second tank player lives, human included | Humans decision; two players racing for "nearest" move the link | a damage-dealer mirror beside a human off-tank |
| A mind-controlled vampire no bot can control for 2 s is killed by the non-vampire damage dealers, vampires staying on her; area damage holds only round controlled ones | Wowhead: mind controlled until killed; script: 70924/73015 give it +300%/+900% health and +100% damage, so control stays first | let it run free (r10) |
| Priests ward themselves before Incite Terror and Mass Dispel the largest feared cluster (3+ within 15 yd, 30 yd range, in sight), priests on separate clusters | WT, Wowhead: priests Fear Ward themselves, then Mass Dispel; script: she is passive through the fear, so a feared main tank costs nothing | Fear Ward on the main tank (shared `RaidAntiFear`) and no Mass Dispel |
| Bite and Pact walks go through the nearest point whose legs miss every shadow by 4.5 yd, at most 8 yd longer than straight; straight otherwise | script: a shadow hits within 4 yd every second; pitfalls: a straight walk can cross another hazard | straight walks (r10) |
| A human Pact member still over 4 yd off the spot 3 s after the Pact: the bots walk to him and stop within 2 yd | Humans decision (bots adapt, he holds no duty); script: the Pact ends once all members stand within 5 yd of each other | bots wait at the spot (r10) |
| Three ground nodes on the dais edge, 33-35 yd north of the centre where no lane runs: 20 nodes in caster reach, 14 in heal reach | navprobe floor, path and sight; the 25-man shortage past 17 | overflow onto the air nodes by the Pact spot |
| Pact members still hurt nodes along their walk | WT, Wowhead: members cross the raid to a pre-picked spot; a 7.5 yd spread has no 10 yd corridor | route round the lattice: a longer walk, more Pact ticks raid-wide |
| The boss doc's Blood Mirror row now says the link takes each hit in its own school, armour skipped | core `Unit::DealDamage`, `SPELL_AURA_SHARE_DAMAGE_PCT`: `CalcAbsorbResist` with the hit's school mask; the guides say shadow | keep the guides' "shadow" |
| Review: a damage-dealer mirror is held off her until the first vampire and bitten after the other damage dealers, like a tank mirror | script: her first bite takes the top threat but her victim, the linked player included; a frenzied mirror walks off the main tank and the link goes to whoever is nearest | treat it as any damage dealer: it hits her from the pull, and a vampire mirror heals 10% of its damage |

All tasks done.

## Carried over

- Merge stage, ICC README: trigger names gain `icc bql mass dispel`; the Blood-Queen's state list gains
  `bql.kill` (`ObsGuidSet`) and `bql.massdispel`; its defensives line: `icc bql tank defensive` also
  covers a damage-dealer mirror through `NextPhysicalDefensive`; the taunt paragraph: with one tank
  player the Blood-Queen's mirror is a damage dealer; the splash line: `icc bql splash` holds only round
  a vampire kept under control.
- `docs/raids/README.md` "Anti-fear": for the Blood-Queen, priests knowing Mass Dispel ward themselves
  (ICC-local overrides of the trigger and action) and Mass Dispel the feared after Incite Terror.
- r14: the r10 items stand.

## Known gaps

In the boss doc's Known gaps. No 631 trace; the user built `623638b99` clean on 2026-10-29, and this
lane's changes are unbuilt. The review added two, both left open: Fear Ward's 3 min cooldown wards only
the first Incite Terror (a split of the priests is the alternative), and percent cuts miss the mirrored
share, so the mirror's buttons cut only Delirious Slash and direct hits.

## Blocked

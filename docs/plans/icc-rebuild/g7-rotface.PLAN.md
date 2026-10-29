# g7-rotface — close Rotface's gaps

Gap wave `3cd-gaps`, after `3g`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap lanes"
section included. Builds on the merged `r7-rotface` lane and its review: read
[r7-rotface.PLAN.md](r7-rotface.PLAN.md) first. Its decisions stand unless closing a gap needs a
change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/rotface.md` and in r7's brief.

- One bot tank: nobody kites the Big Ooze. The Gap lanes' one-bot-tank rule applies.

## Owns

Stem `Rotface` and its definition, the doc, the reader `rotface.py` and its test, this brief.

## Task list

One lead, no split: every change lands in the stem's one state, hazard model and tick
(`ICCHelpers_Rotface.cpp`), with thin trigger/action rows.

1. One bot tank: the kite duty falls to a Death Knight or paladin damage dealer. It never face-tanks
   or holds a Big Ooze, taunts from range (a 20 yd stand-off walk), waits 30 degrees ahead of the ooze
   spot, lets the ooze to 9 yd while it carries a Little Ooze, spends physical defensives. Kite rules
   and the kiter taunt block for every role.
2. Wade: with no dry leg either way, the towing kiter walks a wet leg; floods leave its dodge list
   (its legs own them) and a wet kiter walks a leg whatever the ooze's distance.
3. Freedom duty (`rf.freedom`): a towing, slowed kiter with its ooze within 20 yd gets its own Hand
   of Freedom, Every Man for Himself or Escape Artist, else the nearest paladin bot in 30 yd and sight;
   a wading Death Knight kiter casts Anti-Magic Shell.
4. Burst: everyone, kiter and carriers included, keeps 12.5 yd from a Big Ooze casting its explosion.
5. Little Ooze hazard (`little`, 10 yd) for all but carriers and the kiter.
6. Gassed kiter: Vile Gas on it with its Big Ooze within 20 yd opens the defensive window and the
   human call.
7. Cure walk (`rf.curewalk`): with no curer in reach, the nearest free bot curer walks to 25 yd of the
   carrier, stand released.
8. Lone bot healer: its inner slot follows the kiter.
9. Cleansing Totem: a shaman bot with one down replaces it (`icc rotface totem`).
10. Kite cast cut: the kiter drops a cast once its ooze is within 10 yd or it stands wet.
11. Navprobe the disk the new spots use (rings to 41 yd, sight of his spawn); reader and tests; doc.

All done. Review fixes: a damage dealer kiter keeps the duty against a free main tank (his victim
blinking handed it over for one refresh); `wade` reads only while the kiter stands wet; a towing damage
dealer kiter is cured at once. Independent review: the election no longer dereferences a null pick when no
bot can kite (worldserver crash); the kiter's pet attacks him, not the kited ooze it would pull and die
to; a Death Knight kiter's Anti-Magic Shell waits for a ticking flood, not its warning.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| One bot tank: a DK, then a paladin damage dealer kites, lowest guid | Gap lanes' rule; Dark Command and Hand of Reckoning need no stance or form | Warrior or cat (Taunt and Growl need Defensive Stance or Bear Form), a hunter (no taunt), nobody |
| A damage dealer kiter never stands for a Big Ooze | 25-man Big Ooze averages 54k a swing before armor (DamageModifier 152.5, level 82 base 173 + AP), 31k on 10 | Same duty as a tank |
| Wade a flood only with no dry leg either way | Warcraft Tavern and Wowhead: cross it, freed by Hand of Freedom or Anti-Magic Shell; a held 3-stack ooze hits a tank ~25k a second, a flood 4.4-6.7k | Hold and tank it at the edge (old); reverse past it (drags the aura onto the outer ring) |
| Floods are no dodge hazard for a towing kiter | The dodge pulled it out of a leg the kite then walked again | Dodge and kite both own floods |
| Freedom: kiter's own first, then the nearest paladin bot; DK kiter AMS while wading | DBC 1044 (root/snare immunity, 30 yd), 48707 (aura 267 school immunity, 75% absorb); Wowhead | Generic paladin self-freedom only |
| Everyone keeps 12.5 yd from a bursting Big Ooze | Missiles fly 5 yd/s from it, so a mark 12.5 yd out lands 2.5 s after the cast, time for the 8.5 yd step | Dodge each mark only: the kiter's lands in ~1 s |
| Gassed kiter: defensive window with its ooze within 20 yd | 69248 confuses 6 s, the ooze closes 24 yd in that | No answer (old) |
| Others step 10 yd off a Little Ooze | 69751 radius 10; guides: move it out of the raid | Route carriers round the raid |
| Curer walk to 25 yd of a ready carrier with no curer in reach; holder, kiter and carriers never walk | Cures reach 30 yd; Wowhead's cure at the merge | Let the infection run its 12 s (old) |
| Lone bot healer: inner slot nearest the kiter, re-picked past 30 yd | Heal reach 40 yd; an inner slot within 30 degrees of the kiter is 26.3 yd from it | Fixed slot, far side out of reach |
| Cleansing Totem replaced by Mana Spring, else Healing Stream | It strips a disease every 3 s in 30 yd | Block Call of the Elements, which leaves one already down |
| Kite cast cut at 10 yd or wet; instant facing left alone | The spline rewrites orientation each update (`Unit::UpdateSplinePosition`); `PlayerbotAI::CastSpell` refuses a cast-time spell while moving | Block the kiter's rotation |
| No threat redirect onto the kiter | The pickup taunt recovers it; Misdirection's shots on him would hand the kiter his threat; `--oozes` settles | Misdirection on each pickup |
| A towing kiter keeps 32.5 yd in its last dodge | A puddle or mark on the kiter costs less than the aura on the outer ring | Drop the keep-out last |
| Damage dealer kiter waits 30 degrees off the ooze spot, so a new Big Ooze opens on a carrier there and takes one swing before the taunt (reviewer) | Script: a new Big Ooze attacks the nearest player (`SelectNearestPlayer`); 25-man swings average 54k before armor | Wait on the spot in plate and take the opening swing itself |

## Carried over

- For `r14-closeout`, unchanged from r7: `IccInstanceState::rotfaceVileGasWaitUntil` and
  `rfExplosionSlotMemory` (`rf.explosionslot`, silent) in `Util/ICCScripts.h`; `NPC_PUDDLE`,
  `NPC_SMALL_OOZE`, `NPC_BIG_OOZE`, `SPELL_OOZE_FLOOD`, `SPELL_OOZE_FLOOD_VISUAL`, `SPELL_SLIME_SPRAY`,
  `SPELL_UNSTABLE_OOZE_EXPLOSION` in `ICCData.h`; `IccRotfaceWalkAction`, a copy of the stand-walk base,
  now under the cure walk too.
- For the merge stage (ICC README): Rotface's trigger names gain `freedom`, `totem` and `cure walk`; its
  defensives line gains the damage dealer kiter's physical buttons and the gassed kiter; its traced state
  gains `rf.freedom`.

## Known gaps

See `rotface.md`, "Known gaps": heal threat on the kiter (worse for a damage dealer), Sticky Ooze's slow
cleared by no freedom, no walk to free the kiter, a wade with no leg left stands in the flood, a damage
dealer kiter's pick-up walk trading steps with its flood dodge (reviewer), a carrier's
walk across the raid, Call of the Elements re-dropping a Cleansing Totem, overflow stands, the gassed
kiter caught, no kiter without a Death Knight or paladin, humans hold no duty, no 631 trace.

## Blocked

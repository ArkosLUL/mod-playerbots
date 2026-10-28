# g3-deathwhisper — close Lady Deathwhisper's and the Rotting Frost Giant's gaps

Gap wave `3ab-gaps`, after `3b`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap lanes"
section included. Builds on the merged `r3-deathwhisper` lane and its review: read
[r3-deathwhisper.PLAN.md](r3-deathwhisper.PLAN.md) first. Its decisions stand unless closing a gap
needs a change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/lady-deathwhisper.md`,
`docs/raids/icecrown-citadel/rotting-frost-giant.md` and r3's brief, plus the two every gap lane does
(tank defensives, line of sight).

- Tank defensives: her phase 2 melee and the heroic Touch stacks, the giant's hits on its tank; check
  the scripts for every predictable tank spike.
- Line of sight: r3 left no fixed coordinates. Probe what decides play: the tank sides she is tanked
  from against the adds' doors and the healers, and the giant's tank spot if a closed gap adds one.
- The Cyclone recast gap: the doc records why a bot can't recast early. Another class's instant CC
  landing at the fade is one way to close it; any other that holds the victim works.

## Owns

Stems `Deathwhisper` and `FrostGiant` and their definitions, both docs, both readers and their tests,
this brief.

## Task list

One implementer, no split: the Deathwhisper helpers and the giant helpers are the only shared seams.

Done, in this order:

1. CC chain (`ICCHelpers_Deathwhisper`): a victim that already had a control aura this domination gets an
   instant CC first (Hammer of Justice, Repentance, Blind), and the duty is handed out during the last
   second of a Cyclone so the follow-up lands one tick after the fade. A druid in cat form or Tree of Life
   (a shaman in Ghost Wolf) drops the form and casts.
2. Tank defensives: `NextTankDefensive` on a Deformed Fanatic in melee, on a Frostbolt no bot can kick, and
   under 35% health; the tank class buttons held off the fight; a human tank called.
3. Threat brake: a damage dealer holds fire on her at 90% of the top tank's threat, frees at 80%.
4. Non-lethal dodge spots must see the bot's target.
5. Giant: a pair of bot non-tanks walks out to a spot clear of everyone so nobody else dodges; a tank is a
   partner only when no one else is left.
6. Probes `ldw.defensive`, `ldw.brake`, `rfg.apart`; readers show the CC chain with its free time, the
   defensives and brake, and the pairs that walked out; synthetic-pull tests.
7. Docs: LOS facts, mechanics rows (Necrotic Strike, Shadow Cleave, Vampiric Might, Frost Fever, Darnavan
   added), gaps closed or recorded.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| A victim that already had a control aura takes an instant CC first; the duty goes out in the Cyclone's last second | `CanCastSpell` refuses a Cycloned target, so a Cyclone recast starts at the fade; the brief's own suggestion | Cyclone recast, 1.5-2 s free |
| A druid or shaman in a form drops it to cast, ranked just behind one who needn't | Guides: Cyclone from every druid spec; `SpellInfo::CheckShapeshift`; `RemoveShapeshift` ends only druid and shaman forms | leave them out (r3) |
| Deformed Fanatic: a defensive on the tank it is on, no bounce | The Tavern base guide bounces it; Wowhead kites it or face-tanks it on a major defensive; the add walks 4 yd/s, so a kite drags the tank into the raid | taunt ping-pong, Distracting Shot |
| Tank windows: Deformed in melee, a Frostbolt no kick is ready for (last 1.1 s), under 35%; class buttons held all fight | Convention 43, `OSDefinition_Sartharion` hold | one button per Frostbolt |
| No window on her own swing or the Touch stacks, which the brief named | A steady hit every 2 s (about 11k on 10N, 28k on 25H before armor) is healed, Touch costs threat only, and the 35% valve covers a bad stretch | a window on 25H phase 2 |
| The brake runs on every difficulty at 90%/80% of the top tank's threat | ToC Anub'arak's cap; one code path | heroic only |
| No Divine Shield | A shielded victim is immune and she drops it; untauntable on heroic she takes the next threat; the tanks already trade her by stacks | bubble at 4-5 stacks with a second tank in reach |
| The slowed shade victim is not a gap | The shade lives 7 s and is rooted for 2, so it chases 5 s at 5.25 yd/s; a 4.9 yd/s victim gives up 1.75 yd | Hand of Freedom |
| Heroic blast clearance stays at 10 yd | The victim outruns the shade; earlier clearing walks the raid every 12 s | clear while a shade is in the blast radius |
| Deathchill Bolt and Curse of Torpor stay generic | Melee and hunters take adherents first and kick through the generic interrupt on their target; druid (57) and mage (40) dispels exist | a kick and a dispel duty |
| Non-lethal dodge spots must see the bot's target | A spot behind a torch post drops the target | no sight test |
| No torch-post stands | The posts are static and navprobe sees them, but the old decision stands: nothing in the stem is fixed | navprobed stands at (-625.0, 2221.1) and (-625.0, 2201.7) |
| Giant: a pair of bot non-tanks walks out to a spot found by sweep; with a tank or a player the others dodge as before | The blog's pairs more than 10 yd apart; a melee carrier no longer displaces the melee | everyone near steps out |
| Giant: a tank is a partner only when nobody else is left | A carrier with no one in reach dies | let it die |
| Giant: no tank defensive | Stomp 12k and Arctic Breath 4 x 4k are not spikes | a button on the breath |

## Carried over

Nothing from the earlier lanes; their decisions stand.

## Known gaps

Also in the boss docs:

- No Deformed Fanatic bounce; no Divine Shield; no torch-post stands.
- The brake holds spells and swings, not an auto attack already running, and needs a tank with threat:
  at phase 2's open nothing holds a ranged bot back but the redirects and the buffs.
- No instant CC in the raid leaves the 1.5-2 s Cyclone recast gap; a holder mid-cast at the fade waits out
  its cast; one Hammer of Justice (1 min) covers one victim a round.
- A defensive per unkicked Frostbolt, and the next one each time a button ends under a Deformed Fanatic,
  spends long cooldowns; tank health is unmeasured. The defensive hold keys on `Role::Tank` and the window
  on the shared tank test, which also counts a spec tank on a DPS strategy.
- Darnavan is outside the kill order.
- Giant: a pair with a tank or a player still makes the others dodge; a carrier with every other player
  carrying dies; only a pair's spot is tested, not its walk.
- No 631 trace, never built, no `-Werror` pass.

## Blocked

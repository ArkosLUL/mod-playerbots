# AQ40 `+rnature` leak — closed, pending a live pull

In flight only for the checks below. Everything durable has shipped into
[../../raids/README.md](../../raids/README.md), [../../classes/hunter.md](../../classes/hunter.md) and
[../../engine/action-selection.md](../../engine/action-selection.md).

## What changed

`Aq40UseResistanceBuffsAction` set `+rnature` on hunters fighting Viscidus or Princess Huhuran and only
removed it again while the `aq40` instance strategy was loaded, so leaving AQ40 stranded the strategy on
the bot. `rnature` is a sibling of `bdps`, so it evicted the combat Dragonhawk node, and
`HunterAspectOfTheViperTrigger` bails out whenever it is set — the hunter could not use Aspect of the
Viper again, in any later raid.

AQ40 now uses the same per-boss nodes every other nature boss has: `BossNatureResistanceTrigger` +
`BossNatureResistanceAction` in `RaidAq40Strategy::InitTriggers`, and `BossNatureAspectHoldMultiplier`
in a new `InitMultipliers`, for `"viscidus"` and `"princess huhuran"`. The action and its
unconditional trigger are gone.

The priest branch went with them. Every priest already gets `rshadow` in the non-combat engine from
`AiFactory::AddDefaultNonCombatStrategies`, and the branch guarded on not having it, so it never fired
for a default priest — zero rows in the DB carried `+rshadow` in a combat list. What it would have done
is load a `NonCombatStrategy` into the combat engine permanently.

`data/sql/playerbots/updates/2026_09_13_00_playerbots_clear_leaked_rnature.sql` swaps `+rnature` back to
`+bdps` in stored strategy lists. One row matched here (guid 115360, Nightwarrior).

**Behaviour change:** the old gate was `boss->IsInCombat()`; the new one is `FindBossByName` + alive +
hostile, so hunters take Aspect of the Wild when the boss comes into range rather than at engage. Same
timing Kologarn, Freya, Thorim, Emalon and Archavon already have.

## What is still unverified

Needs an AQ40 pull of Viscidus or Princess Huhuran, then any later boss in the same session.

1. **The node fires.** `viscidus nature resistance action` (or `princess huhuran …`) at `rel 60.00`.
2. **One holder.** Group `"e":"aura","sp":49071` records by `"s"`; two overlapping sources is a defect.
3. **No pointless pin.** A hunter that is not the holder shows no `rel 0.00` on any `aspect of the *`.
4. **Nothing sticks.** After leaving AQ40, `SELECT value FROM playerbots_db_store WHERE guid=<hunter>
   AND \`key\`='co'` has no `+rnature`, and the hunter casts Aspect of the Viper on the next boss.
5. `tools/botobs/postmortem.py <file> --verify` exits zero.

Before-picture is `603_4_general-vezax_1789302545`: Wild at `rel 20.00` from the `rnature` node on a
boss with no nature node at all, zero Viper casts, 70.3% of a 363 s fight under 5% mana.

## Notes for whoever runs the migration

Apply it with the worldserver down. A bot that is online when the row changes writes its in-memory
strategy list back over it on logout.

## Known gap this does not close

`BossFireResistanceAction`, `BossFrostResistanceAction` and `BossShadowResistanceAction` still add
`+rfire`/`+rfrost`/`+rshadow` for paladins and never remove them, live at Razorscale, Ignis, Freya,
Thorim, Mimiron, Sara, Yogg-Saron, Koralon and Toravon. Same defect shape; fixing it needs a per-boss
hold multiplier for the paladin aura slot.

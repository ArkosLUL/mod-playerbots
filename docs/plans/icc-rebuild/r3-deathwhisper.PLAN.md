# r3-deathwhisper — Lady Deathwhisper, Rotting Frost Giants

Wave 3a, parallel with `r2-marrowgar`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounters

- **Lady Deathwhisper:** slug `lady-deathwhisper`, prefix `ldw.`, boss id 1, stem `Deathwhisper`
  (with `icc dark reckoning`). Core `boss_lady_deathwhisper.cpp`. Doc `lady-deathwhisper.md`, reader
  `lady_deathwhisper.py`.
- **Rotting Frost Giant:** slug `rotting-frost-giant`, prefix `rfg.`, no boss id (the `r1` unit
  gate), stem `FrostGiant`. Core `icecrown_citadel.cpp`. Doc `rotting-frost-giant.md`, reader
  `rotting_frost_giant.py`.
- Guides in the cache:
  - Deathwhisper: Warcraft Tavern 25 and 10, instant-carry's raid overview, Wowhead if saved;
  - giant: the dontstandinthefire blog.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 1011 lines (`ICCActions_LDW.cpp`). The Mana Barrier aura sets the phase
  (`IccLdwBarrierUp`).
- The skull goes on the add nearest her within 25 yd, and never leaves a stale non-add.
  `HandleAddTargeting` has no caller.
- Hand-rolled flee vectors (`fleeX`, 12 hits).
- `IccLadyDeathwhisperMultiplier` always vetoes Flee, BlinkBack, Follow and Formation. With a Shade on
  the bot it vetoes everything but the shade action.
- Cheats (`IccAddsLadyDeathwhisperAction`):
  - `ldw.cyclone`: a Cyclone aura on a Dominate Mind victim, bot or human;
  - `ldw.insignificance`: the tank strips Touch of Insignificance.
  - Buffs: `ldw.nothreat`, `ldw.spitefulfury` (`EnableICCBuffs`, keep).
- Giant (`IccRottingFrostGiantTankPositionAction`): cheat `rfg.deathplague` strips Death Plague. The
  spread duty is commented out. The doc has core-bug issue text: the plague pick should also skip
  72865.
- Positions: 2 for Deathwhisper, 1 for Dark Reckoning, 1 for the giant, none navprobed.

## Guide answers to build on

- **Dominate Mind** takes 3 non-tanks for 12 s (the script: 1/1/3 on 25N/10H/25H, none on 10N). The
  raid stops cleaving, and a druid Cyclones each victim (it doesn't break on damage); any CC works.
  Victims can be humans.
- **Touch of Insignificance:** each stack cuts threat 20%. The guide says she can't be taunted, so tanks
  keep threat neck and neck with Misdirection and Tricks of the Trade, and Divine Shield drops the
  stacks. Check against the script whether taunts work on normal.
- **Adds:** a tank at each side's torches on the upper platform, physical and magic DPS split for the
  immune adds.
- **Heroic:** adds keep spawning in phase 2, Shades explode in an area, she can't be taunted.
- **Setup:** 2-3 tanks, 4-6 healers.
- **Death Plague:** pairs more than 10 yd apart. The plague hits after 15 s for 8k and spreads to
  everyone within 8 yd; with nobody to spread to, it kills its host. The giant also knocks back 20 yd.

## Focus

- A CC duty on each Dominate Mind victim, with ranked casters and Cyclone first.
- Threat parity and redirects on Touch.
- Add kill order by immunity, an interrupt duty for Frostbolt, the Death and Decay and Shade dodges,
  and heroic phase 2 adds.
- The giant's pair spread duty.

## Owns

Stems `Deathwhisper` and `FrostGiant` and their definitions, both docs, both readers and their tests,
this brief.

## Task list

Done, one implementer (the work shares one helper per stem, so no split):

1. Deathwhisper helpers (`Util/ICCHelpers_Deathwhisper.{h,cpp}`, new `.cpp`): per-instance scan of
   adds, shades and Death and Decay; resolver by damage school, tanks by side; CC duty `ldw.cc`;
   Frostbolt interrupt duty; normal taunt swap; redirect tank; hazard model; Dark Reckoning; lust
   window; tick with the heroic buffs and reset.
2. Deathwhisper triggers, actions, splash-hold multiplier and definition rewritten: one forced dodge,
   rules instead of the old blanket veto, no icon writes, no flee vectors, no fixed positions.
3. Giant helpers (`Util/ICCHelpers_FrostGiant.{h,cpp}`, new `.cpp`): pair duty `rfg.partner`; node
   renamed `icc rotting frost giant plague`; the cheat and the commented-out block gone.
4. Readers: `lady_deathwhisper.py` `--targets`, `--cc`, `--kicks`, `--dodges` (no `--skull`);
   `rotting_frost_giant.py` `--partners`; tests for both.
5. Boss docs with mechanics tables.
6. Review (`iccr/r3-deathwhisper-review`): form check on CC and kick picks, a timed-out CC duty
   passes on, the giant's band hold and per-ms carrier memo, partners walk to player carriers.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Tanks own a side each by her y (main tank the lower door) and meet the adds there, no stand at the torches | Warcraft Tavern 25 stands them at the torches to break casters' sight, which bots can't plan | navprobed torch stands |
| No opening positions | The guides stand the raid behind her, inside her 23 yd aggro radius | a pre-pull stand that would pull her |
| No phase 2 drag | Wowhead's heroic drag to each wave's side; the free tank takes the add instead | drag her between sides |
| Kill order by school: physical on adherents, casters on fanatics, each skipping what it can't hurt; casters skip a shrouded adherent | instant-carry, the immunity auras 71234/71235, Shroud 70768 | one shared order |
| Dominate Mind CC ranked Cyclone, Hammer of Justice, Hex, Polymorph, Repentance, Fear, Blind; tanks hold no duty | guides: Cyclone first, any CC works; the two that don't break first | Cyclone only |
| Area damage held within 15 yd of a victim not under Cyclone | guides: stop cleaving | hold raid-wide for 12 s |
| Normal swap at 3 Touch stacks, by a tank carrying none; heroic: both tanks keep hitting her, redirects onto her tank | Wowhead, Warcraft Tavern; script: heroic taunt immunity all fight (DB), DR flag on all four entries | swap at 5 stacks |
| Generic tank taunts blocked during her fight | taunt DR on her; the encounter's own taunt takes loose adds | generic taunts on |
| Lust held to phase 2 on every difficulty | Wowhead (heroic only) | normal free |
| One forced dodge for pools, martyrdom, shade and heroic blast, at `ACTION_EMERGENCY + 6` | conventions 14, 19 | one node per hazard |
| Dark Reckoning by hazard-clear spot; a marked tank or player stays and the rest clear it | old spot 106 yd from both priests against 56 yd of walk in 8 s | the fixed spot |
| Giant pairs form round each carrier, cured partner first, never a tank | blog's pairs; script: a cured partner ends the chain | standing pairs |
| Giant tank position dropped | used only by commented-out code, no guide basis | navprobe it |
| Review: CC and kick picks skip a spell the bot's form blocks, so cat-form and Tree of Life druids hold no Dominate Mind duty; `CanCastSpell` passes `NOT_SHAPESHIFT` and the direct cast then fails | `SpellInfo::CheckShapeshift`; Cyclone excludes Tree of Life and every non-stance form | cancel cat form and Cyclone, as the guide's "any druid" implies |
| Review: the normal swap needs a taunter free of stacks, which last 30 s, so after the first swap it lands at 4-5 stacks, about every 30 s | Wowhead's "taunt every 30 seconds"; 71204 duration and stacking | let a tank with 1 stack taunt one at 3+ |
| Review: a tank carrier gets a bot partner, and the partner walks to a tank or player carrier | Humans hold no duty; a tank keeps the giant where it is | the player walks, with nothing to tell them who |

## Carried over

- From `r0a-restructure`: the giant's "waiting for core fix" comment and `TODO` block. Done: both
  removed with the position they used.

## Known gaps

Also in the boss docs:

- Deformed Fanatic face-tanked, not bounced or kited.
- No DPS threat brake on heroic phase 2.
- No Divine Shield to drop Touch stacks.
- Deathchill Bolt and Curse of Torpor left to the generic interrupt and dispel nodes.
- A shade victim slowed by Frostbolt Volley is slower than the shade.
- A Cycloned victim is free about 1.5-2 s before the recast lands: `CanCastSpell` refuses an immune
  target. Closing it needs a cast that skips that check, or an instant CC from another class at the
  fade.
- Giant: melee dodging a carrier lose uptime; a carrier with no possible partner dies.
- No 631 trace on disk; nothing measured live.

## Blocked

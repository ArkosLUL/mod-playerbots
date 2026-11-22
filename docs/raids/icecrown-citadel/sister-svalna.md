# Sister Svalna

Boss id 9, trace slug `sister-svalna`, prefix `ss.` ([README](README.md)). Code: stem `Svalna`
(`src/Ai/Raid/ICC/*/ICC*_Svalna.*`), two definitions: `Svalna`, nodes `icc valkyre spear` and `icc
sister svalna` interrupt, tank defensive, hurl, taunt, target; `SvalnaGauntlet` (below). Core:
`boss_sister_svalna`, the captains, Crok and `npc_frostwing_vrykul` in `icecrown_citadel.cpp`. Not
boss-flagged and no kill credit, so only the boss-state table names her trace.

## Mechanics

The raised captains' Flash Heal 71782/71783, Smite 71778/71779, Power Word: Shield 71780/71781 and
Machine Gun 71788/71789 have spelldifficulty rows; nothing else here does.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Gauntlet | Crok 37129 walks the hall with four captains; at waypoints 1 and 2 Svalna kills one with Caress of Death (a 4 s Impaling Spear through her immunity, unavoidable); once the Ymirjar are dead she raises the dead captains (Revive Champion, 2 at most) as 37491/37493/37494/37495 in their guid slots 31-34, who charge the nearest player, then lands | Ranged on the Deathbringers, melee on Frostbinders, AoE the summons; tank the raised captains | [Gauntlet](#svalna-gauntlet); captains tanked and killed (below) |
| Engage | `IN_PROGRESS` from `BossAI::_JustEngagedWith`; Crok tanks her from the engage | Crok tanks her safely | No opening positions; bot tanks take the captains |
| Impaling Spear | 15-20 s in, then every 20-25 s: Aether Shield 71463 on her (45 s, immune to every school, a heal every 2 s), then Impaling Spear 71443 (0.5 s) on a random player: stunned, a vehicle for spear 38248, 10% of max health a second until freed | Pull the spear out, throw it back | One elected puller; one holder throws |
| Remove Spear 71462 | The spear's spell click, any raid member: a 1 s cast, frees the victim, puts Infernal Spear 50307 in the clicker's bags | — | Nearest damage dealer bot, else healer, else a tank holding nothing (`ss.spear`) |
| Hurl Spear 71466 | The item's spell, 25 yd, 10k, through immunities (`SPELL_ATTR0_NO_IMMUNITIES`); refuses a shapeshifted thrower; breaks the shield on hit | — | The holder nearest her throws while she is shielded, walking to 18 yd past 24 (`ss.hurl`); others keep theirs for the next shield |
| Raised captains | Arnath: Flash Heal (2 s), Power Word: Shield, Smite, 25-man Dominate Mind 14515 (2 s, 20 yd, 15 s charm, magic, every 28-37 s from 22-27 s); Brandon: Divine Shield under 20%, Hammer of Betrayal; Grondel: Charge, Mortal Strike, Conflagration; Rupert: Fel Iron Bomb, Machine Gun, Rocket Launch. Fear-immune only. Her death kills them | Tank them; kill her or them first, either way | Each to the nearest free bot tank (`ss.tank`), taunted back when lost; DPS on Arnath, Rupert, Grondel, Brandon (not under Divine Shield), then her unshielded (`ss.target`); Arnath's Dominate Mind and Flash Heal kicked (`ss.interrupt`); tank valve (`ss.defensive`) |

## Duties

- **Live** while Svalna is alive within 100 yd and in combat or a raised captain fights; the
  captains charge before she lands, so this covers the seconds before her state moves.
- **Spear puller**: per spear, the nearest bot not impaled, charmed (25-man Dominate Mind) or in a
  vehicle, not locked to one ability (Bladestorm makes the click a no-op) and not a tank holding an
  add; damage dealers rank first, then healers, then free tanks. It drops its cast (the victim loses
  10% a second), walks forced to within 5 yd and clicks. The claim lapses once the spear is gone or
  the puller fails any of these.
- **Thrower**: elected among uncharmed bot holders by distance to her; drops form and cast before
  using the item.
- **Kicker**: per Arnath cast of Dominate Mind or Flash Heal, the nearest bot not casting, impaled,
  charmed or pulling a spear with a kick or silence ready, in reach and sight; one kicker per caster,
  elected every 200 ms in the tick.
- **Tank valve**: a bot tank a captain hits spends `NextTankDefensive` (physical) under 40%; the class
  nodes keep theirs to 25%; a human tank in that state is called (`ss.humancall`).
- **Targets**: a player's skull, a tank's own captain, the captains in kill order (a tank skips one
  another tank holds), then her while unshielded.

## Decisions

- **One puller per spear**, damage dealers first: everyone running for it (the old answer) pulled
  tanks off the captains and healers off the victim.
- **The throw waits for the shield**: Hurl Spear's 10k matters little, the shield's immunity and heal
  a lot, and the next shield comes with its own spear.
- **Kill order** Arnath first: the blog leaves the order open; he heals the others and, on 25-man,
  casts Dominate Mind, and putting damage dealers on him puts their interrupts on his casts.
- **Dominate Mind is kicked, not cleared**: its 2 s cast is the window; a charm that lands runs its
  15 s with no CC duty.
- **Tank valve** at 40%: no captain hit is predictable enough for a timed button.

## Svalna gauntlet

Definition `SvalnaGauntlet`, `ICC_BOSS_NONE`, `UnitGate` on Battle-Maiden 37132, Deathbringer 38125,
Frostbinder 37127, Huntress 37134, Warlord 37133, Corrupted Ymirjar 38184 and Warhawk 38154, trace
`svalna-gauntlet` opened by `IccMarkPull` from its tick while live; her state replaces it once she
engages. Keys `ss.gauntlet.*`; nodes `icc sister svalna gauntlet` dodge, interrupt, tank defensive,
taunt, target, which `ICC_PREFIXES` files under `sister-svalna` until it gets the gauntlet's row. Its
nodes but the dodge sit below Svalna's: her captains rise while Corrupted Ymirjar may still fight.

The hall's spawns span x 4340-4372: packs at y 2624-2643 (two Huntresses, a Warlord, two Battle-Maidens), y
2570-2608 (two Deathbringer and Frostbinder pairs, two Huntresses, two Battle-Maidens) and on the
upper platform at y 2510-2528 (two Huntresses, two Battle-Maidens, three Warlords). navprobe `--nav
0x09` settles the lower hall at 351.10 and the platform at 358.43; the platform's east edge (x 4368,
y 2520) is off the mesh. No stand is fixed: dodge spots come from the collision-checked sweep.

Empowered Shadow Bolt 69528/70281, the orb's blast 71286/71292 and Arctic Chill's tick 71271/71829 have
spelldifficulty rows; nothing else here does.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Pull | Crok pauses at waypoints 0-2 until the Ymirjar homed past y 2600/2550/2515 die; the casters keep 16 yd (the Frostbinder hovers in place) | Draw packs to Crok | Kill order by role (`ss.gauntlet.target`): ranged Deathbringer, melee Frostbinder first, then the other caster, Huntress, Battle-Maiden, Warlord, summons, the current pick kept while nothing of a better tier fights; tanks take the melee adds they hold, else the nearest no other tank holds, never a caster; area damage generic |
| Melee Ymirjar | Warlord, Battle-Maiden, Corrupted Ymirjar melee their victim | Tank them | One elected rescue taunt per melee add on a non-tank (`ss.gauntlet.taunt`), generic taunts free; tank valve (`ss.gauntlet.defensive`) |
| Empowered Shadow Bolt 69528/70281, Summon Undead 69516 | Deathbringer: a 2 s bolt every 2.5 s; from 5 s, every 45 s, a 2 s cast raising five Corrupted Ymirjar (71302), with no prevention type, so no kick or silence stops it and a stun does; its AI stops on silence | Ranged on Deathbringers | One elected stunner per Summon Undead (Hammer of Justice, Concussion Blow, Bash), then one elected kicker per bolt, kicks then silences (`ss.gauntlet.interrupt`) |
| Whirlwind 41056 | Warlord, every 6 s: a 2 s cast, self-channelled so it sits in the channel slot, then 41057 a weapon hit 8 yd round him | — | Non-tanks within 8.75 yd step to 10.5 yd, forced, cutting a cast, melee keeping their target within 3 yd past its combat reach where they can (`ss.gauntlet.dodge whirlwind`); reach and charge toward a target inside it held |
| Frozen Orb 71274 | Frostbinder, from 15-25 s every 40-50 s: a 10 s self-channel on its nearest player within 30 yd, its aura on that player (`SpellInfoCorrections` retargets it); as it runs out an orb (71285, 5 yd/s) flies to where the holder stood, and 71286 deals 15k frost (25-man 20k) within 6 yd. No prevention type: only a stun breaks the channel, and then nothing flies | — | One elected stunner per channel (`ss.gauntlet.interrupt orb`); the others within 6.75 yd of the holder step to 8.5 yd in its last 3 s, then everyone leaves the landing spot until the orb lands (`ss.gauntlet.dodge orb`), forced, cutting a cast; a channel last seen within 0.6 s of its end counts as launched, one gone sooner as broken; reach and charge toward a target inside either circle held |
| Arctic Chill 71270 | Frostbinder aura: 71271, 5k frost (25-man 7k) a second within 10 yd | Melee on Frostbinders anyway | None |
| Huntress | Ice Trap, Rapid Shot channel, Volley (a rain round a point 10-35 yd off); 25-man Summon Warhawk 38154 | — | Generic; Volley's area left to avoid aoe |
| Twisted Winds 71306 | Frostbinder swaps places and threat with a creature | — | None |

Decisions: the blog's role split decides the opening tiers; area damage stays with the generic pickers,
the packs standing bunched and a Deathbringer raising five; no gauntlet opening positions, since Crok's
escort and the player's pulls start each pack; Arctic Chill is taken, the blog putting melee on
Frostbinders; the orb dodge splits in two because a holder can't leave a circle round itself.

## Cheats

None.

## Known gaps

- A landed Dominate Mind runs its 15 s with no CC duty.
- Gauntlet: Arctic Chill, Ice Trap, Volley and Twisted Winds have no encounter answer; the Warhawk is
  left to the generic AI.
- Gauntlet: with no Hammer of Justice, Concussion Blow or Bash ready in reach, Summon Undead lands and
  a Frozen Orb flies. A holder within a few yards of its Frostbinder then takes the blast: the orb lands
  in under a second, too soon to clear 6 yd.
- Humans hold no duty.
- No ICC trace yet carries these probes, so nothing here is measured.

## Targets for the next pulls

25N first: `--spears` shows every impale ending within 4 s and no victim dying on a spear, and a Hurl
Spear inside each shield; `--duties` has one puller per spear, `captain` before `svalna` for every
damage dealer and an `ss.interrupt` span on each Dominate Mind; `--shield` holds each shield under 10
s. Gauntlet: `svalna_gauntlet.py --kicks` has a stunner on most Summon Undead and Frozen Orb casts,
`--dodges` a dodge per Whirlwind and launched orb, `--targets` `deathbringer` first for ranged and
`frostbinder` for melee. 25H: the same.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `ss.shield` | per bot within 30 yd of her | `up` or `down`. No trace records a creature's aura, so this is the shield's only record |
| `ss.spear` | traced map, puller → spear | The spear it was elected for; `0` once it lapses |
| `ss.hurl` | per holder | `throw`, `walk`, `wait` (another holder throws), `hold` (unshielded) |
| `ss.interrupt` | per bot | `dominate`, `heal` while elected to kick Arnath, `none` |
| `ss.tank` | traced map, tank → captain | The raised captain it holds; `0` once it lapses |
| `ss.target` | per tank and damage dealer | `skull`, `owned`, `captain`, `svalna`, `none` |
| `ss.defensive`, `ss.humancall` | per bot tank; per call | The ladder's cast name, `idle`; the button a human tank was told |
| `ss.gauntlet.target` | per tank and damage dealer | `skull`; tanks `rescue`, `held`, `none`; damage dealers the kind, `none` |
| `ss.gauntlet.taunt` | per bot tank | The kind of melee add its rescue taunt is due on, `none` |
| `ss.gauntlet.interrupt` | per bot | `summon`, `orb` while elected to stun a Summon Undead or a Frozen Orb channel, `bolt` while elected to kick, `none` |
| `ss.gauntlet.dodge` | per bot | `whirlwind`, `orb`, `blocked`, `none` |
| `ss.gauntlet.defensive`, `ss.gauntlet.humancall` | per bot tank; per call | As `ss.defensive` |

Each launched Frozen Orb is a 6 yd hazard circle (71286, 25-man 71292) at its landing spot for its flight.

`tools/botobs/bosses/sister_svalna.py`; its banner names declared keys the pull never wrote, leaving
out `ss.gauntlet.*`:

- `--shield`: each shield and how long it held, from the first bot to note each edge;
- `--spears`: each impale (aura 71443 on the victim) and the Hurl Spear casts;
- `--duties`: spear pullers, throw rules, kicks, captain tanks and targets per bot;
- `--defensives`: tank buttons and human tank calls;
- `--cheats`: uses in the pull, none of them hers.

`tools/botobs/bosses/svalna_gauntlet.py`, its banner reading the `ss.gauntlet.*` keys off the source:
`--packs` (each kind seen and dead), `--targets`, `--taunts`, `--kicks`, `--dodges`, `--defensives`,
`--cheats`.

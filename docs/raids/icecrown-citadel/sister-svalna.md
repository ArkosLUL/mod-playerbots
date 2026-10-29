# Sister Svalna

Boss id 9, trace slug `sister-svalna`, prefix `ss.` ([README](README.md)). Code: stem `Svalna`
(`src/Ai/Raid/ICC/*/ICC*_Svalna.*`); nodes `icc valkyre spear` and `icc sister svalna` hurl, taunt, target.
Core: `boss_sister_svalna` and the captains in `icecrown_citadel.cpp`. Not boss-flagged and no kill
credit, so only the boss-state table names her trace.

## Mechanics

No ability here has a spelldifficulty row: one id on every difficulty.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Gauntlet | Crok 37129 walks the hall with four captains; at waypoints 1 and 2 Svalna kills one with Caress of Death (a 4 s Impaling Spear through her immunity, unavoidable); once the Ymirjar are dead she raises the dead captains (Revive Champion, 2 at most) as 37491/37493/37494/37495 in their guid slots 31-34, who charge the nearest player, then lands | Ranged on the flying Deathbringers, melee on Frostbinders, AoE the summons; tank the raised captains | Ymirjar on the generic AI; captains tanked and killed (below) |
| Engage | `IN_PROGRESS` from `BossAI::_JustEngagedWith`; Crok tanks her from the engage | Crok tanks her safely | No opening positions; bot tanks take the captains |
| Impaling Spear | 15-20 s in, then every 20-25 s: Aether Shield 71463 on her (45 s, immune to every school, a heal every 2 s), then Impaling Spear 71443 (0.5 s) on a random player: stunned, a vehicle for spear 38248, 10% of max health a second until freed | Pull the spear out, throw it back | One elected puller; one holder throws |
| Remove Spear 71462 | The spear's spell click, any raid member: a 1 s cast, frees the victim, puts Infernal Spear 50307 in the clicker's bags | — | Nearest damage dealer bot, else healer, else a tank holding nothing (`ss.spear`) |
| Hurl Spear 71466 | The item's spell, 25 yd, 10k, through immunities (`SPELL_ATTR0_NO_IMMUNITIES`); refuses a shapeshifted thrower; breaks the shield on hit | — | The holder nearest her throws while she is shielded, walking to 18 yd past 24 (`ss.hurl`); others keep theirs for the next shield |
| Raised captains | Arnath: Flash Heal (2 s, kickable), Power Word: Shield, Smite, 25-man Dominate Mind; Brandon: Divine Shield under 20%, Hammer of Betrayal; Grondel: Charge, Mortal Strike, Conflagration; Rupert: Fel Iron Bomb, Machine Gun, Rocket Launch. Her death kills them | Tank them; kill her or them first, either way | Each to the nearest free bot tank (`ss.tank`), taunted back when lost; DPS on Arnath, Rupert, Grondel, Brandon (not under Divine Shield), then her unshielded (`ss.target`) |

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
- **Targets**: a player's skull, a tank's own captain, the captains in kill order (a tank skips one
  another tank holds), then her while unshielded.

## Decisions

- **One puller per spear**, damage dealers first: everyone running for it (the old answer) pulled
  tanks off the captains and healers off the victim.
- **The throw waits for the shield**: Hurl Spear's 10k matters little, the shield's immunity and heal
  a lot, and the next shield comes with its own spear.
- **Kill order** Arnath first: the blog leaves the order open; he heals the others and, on 25-man,
  casts Dominate Mind, and putting damage dealers on him puts their interrupts on his Flash Heal.

## Cheats

None.

## Known gaps

- The Ymirjar gauntlet runs on the generic AI, and no trace names it: the boss state stays
  `NOT_STARTED` until she engages.
- 25-man Dominate Mind is left to the generic AI; killing Arnath first is the only answer.
- No tank defensive ladder: no boss hits a bot tank.
- No ICC trace yet carries these probes, so nothing here is measured.

## Targets for the next pulls

25N first: `--spears` shows every impale ending within 4 s and no victim dying on a spear, and a Hurl
Spear inside each shield; `--duties` has one puller per spear and `captain` before `svalna` for every
damage dealer; `--shield` holds each shield under 10 s. 25H: the same.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `ss.shield` | per bot within 30 yd of her | `up` or `down`. No trace records a creature's aura, so this is the shield's only record |
| `ss.spear` | traced map, puller → spear | The spear it was elected for; `0` once it lapses |
| `ss.hurl` | per holder | `throw`, `walk`, `wait` (another holder throws), `hold` (unshielded) |
| `ss.tank` | traced map, tank → captain | The raised captain it holds; `0` once it lapses |
| `ss.target` | per tank and damage dealer | `skull`, `owned`, `captain`, `svalna`, `none` |

`tools/botobs/bosses/sister_svalna.py`; its banner names declared keys the pull never wrote:

- `--shield`: each shield and how long it held, from the first bot to note each edge;
- `--spears`: each impale (aura 71443 on the victim) and the Hurl Spear casts;
- `--duties`: spear pullers, throw rules, captain tanks and targets per bot;
- `--cheats`: uses in the pull, none of them hers.

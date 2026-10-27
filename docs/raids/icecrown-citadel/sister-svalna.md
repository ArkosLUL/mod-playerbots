# Sister Svalna

Boss id 9, trace slug `sister-svalna`, prefix `ss.` ([README](README.md)). Code: stem
`Svalna` (`src/Ai/Raid/ICC/*/ICC*_Svalna.*`). Mechanics: `boss_sister_svalna` in
`icecrown_citadel.cpp`. Not boss-flagged and no kill credit, so only the
boss-state table names her trace.

## Mechanics

- `IN_PROGRESS` from `BossAI::_JustEngagedWith`, once Crok's run up the hall brings her down.
- **Impaling Spear** every 20-25 s, first at 15-20 s: Aether Shield 71463 on herself (45 s, immune to
  every school), then Impaling Spear 71443 on a random player. It makes the victim a vehicle that an
  Impaling Spear 38248 rides until someone clicks it: Remove Spear 71462 frees the victim and hands
  the clicker an Infernal Spear 50307, whose Hurl Spear 71466 on her breaks the shield.
- None of these has a spelldifficulty row.

## Tactics

- `icc valkyre spear` (`ACTION_EMERGENCY + 5`): every bot within 100 yd of a spear walks to it and
  clicks it.
- `icc sister svalna` (`ACTION_RAID + 5`): a bot within 30 yd that holds the Infernal Spear throws it
  while she is shielded.

She has no phase. `IccSvalnaShielded` reads Aether Shield for the trigger and the throw, and
publishes `ss.shield`.

## Cheats

None.

## Known gaps

- Every bot within 100 yd runs for a spear at emergency priority, tanks and healers included, until
  the first click despawns it.
- No ICC trace yet carries `ss.shield`, so nothing here is measured.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `ss.shield` | per bot within 30 yd of her | `up` or `down`. No trace records a creature's aura, so this is the shield's only record |

`tools/botobs/bosses/sister_svalna.py`; its banner names declared keys the pull never wrote:

- `--shield`: each shield and how long it held, from the first bot to note each edge;
- `--spears`: each impale (aura 71443 on the victim) and the Hurl Spear casts;
- `--cheats`: uses in the pull, none of them hers.

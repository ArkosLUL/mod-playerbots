# Twin Val'kyr

Raid-wide facts, the code layout and the essence, Vortex, Touch and Twin's Pact ids by difficulty:
[README.md](README.md). Mechanics: `boss_twin_valkyr.cpp`, the DBC CSVs and `SpellInfoCorrections.cpp`.
Figures run 10N / 25N / 10H / 25H.

Other ids, Light / Dark where paired:

| Spell | 10N | 25N | 10H | 25H |
|---|---|---|---|---|
| Surge damage | 65767 / 65769 | 67274 / 67265 | 67275 / 67266 | 67276 / 67267 |
| Vortex damage | 66048 / 66059 | 67203 / 67155 | 67204 / 67156 | 67205 / 67157 |
| Shield of Lights / Darkness | 65858 / 65874 | 67259 / 67256 | 67260 / 67257 | 67261 / 67258 |
| Unleashed Light / Dark | 65795 / 65808 | 67238 / 67172 | 67239 / 67173 | 67240 / 67174 |
| Empowered Light / Darkness | 65748 / 65724 | 67216 / 67213 | 67217 / 67214 | 67218 / 67215 |
| Powering Up | 67590 | 67602 | 67603 | 67604 |

Fjola Lightbane (34497, Light) runs the fight: the specials, the berserk, and the shared health pool
(each update she takes Eydis's health loss onto herself and copies the result back). Eydis Darkbane
(34496, Dark). Either death kills both. Both fly, CombatReach 9, immunity set -286: silence, stun,
fear, root, snare, knockback and more, but not interrupt or taunt. They turn aggressive at
(585.5, 170) and (545.5, 170), 3.25 s after `IsEncounterInProgress`. Their events advance only with
a victim, so every timer below counts from the pull.

## Colours

Light spells are fire (school 4), Dark ones shadow (32).

- **Essence**: absorbs 1,000,000 of its school, +20% run speed, and through its linked Effect 2
  (65811 Light / 65827 Dark) **+50% damage against the other-colour twin and −50% against her
  sister** (`MOD_DAMAGE_DONE_VERSUS_AURASTATE`; aura state 19 is Eydis, 22 Fjola). A Light bot on
  Fjola deals half. Death-persistent; taking one strips the other.
- **Surge**, each twin from the pull: every 2 s, map-wide, 1,500 / 2,500 / 2,500 / 4,500 to every
  player without her colour's essence (`ExcludeTargetAuraSpell`, set per difficulty in
  `SpellInfoCorrections`). Everyone takes exactly one; no essence takes both.
- **Vortex**: 8 s cast (6 s heroic), then a 5 s channel ticking every second map-wide (radius index
  28) on anyone without the caster's essence: 5,850-6,150 / 8,288-8,712 / 9,263-9,737 /
  15,600-16,400. Wrong colour is 30-80k in 5 s, lethal on 25H, tanks included. Not interruptible
  (`PreventionType` 0). Self-channelled, so `FindCurrentSpellBySpellId` sees it from cast start to
  channel end.
- **Powering Up** (67590 row): each 1,000 an essence absorbs is a stack; 100 become Empowered
  Light/Darkness for 20 s (+100% damage against the other-colour twin, 20% mana). Matching Vortex
  ticks give 5-16 stacks each, matching orbs and Touch ticks the rest. The stacks do nothing else.

## Essence portals

Light 34568 at (541.0, 117.3) and (586.2, 162.1), Dark 34567 at (586.1, 117.5) and (541.6, 161.9):
31.8 yd from `ARENA_CENTER`, opposite colours 45 yd apart along an edge. Friendly, gossip flag,
CombatReach 7, summoned and despawned with the twins. `npc_essence_of_twin::OnGossipHello` strips
the other essence and the Touch of the portal's colour, then casts the essence, remapped by the
`Spell` constructor, so a bot's `HandleGossipHelloOpcode` works on every difficulty.
`GetNPCIfCanInteractWith` allows `INTERACTION_DISTANCE` 5.5 **plus both combat reaches**: about
14 yd from a portal, 18 yd from the centre.

## Specials

Every 45 s from 45 s, one of four, none repeating until all four have run: Light Vortex (Fjola),
Dark Vortex (Eydis), Light Pact (Fjola), Dark Pact (Eydis).

**Twin's Pact.** The caster takes Shield of Lights/Darkness (16 s), then casts Twin's Pact (15 s,
interruptible). The shield absorbs 175k / 700k / 300k / 1.2M and carries `MECHANIC_IMMUNITY` to
interrupt (26), so every kick lands for nothing until the absorb is spent and the aura drops. Pact
heals 20% (normal) or 50% (heroic); `SpellInfoCorrections` strips its sister-heal effect and shared
health carries it over. The sister gets Power of the Twins and dual wield for 15 s, hitting her tank
harder. Interrupts that work: kick, pummel, shield bash, counterspell, wind shear, mind freeze,
spell lock; silences and stuns do nothing.

**Touch**, heroic only. Each twin, first at 10-25 s, then 45-50 s after one lands or 10 s after
finding no target; Fjola pushes hers to 15 s past each special. It targets a random player holding
the other colour's essence who is neither twin's victim. `spell_valkyr_touch_aura` deals 2,925-3,075
(10H) or 5,850-6,150 (25H) every 2 s for 20 s to the touched player only, after absorbs: an essence of
the Touch's colour absorbs it all (school absorb, fire for Light, shadow for Dark). Only gossip at a
portal of its colour ends it, even for a holder of that colour: it's an instant 20 yd/s missile, so
one picked on a Light holder can land after the swap back. Before core `512343e24` (upstream
2026-11-02) every tick hit the whole raid.

## Orbs

Concentrated Light 34630 and Darkness 34628: hostile, not selectable, reach 0. Waves at 10-15 s,
then +8, +8 and +15 s, repeating: 4, 4, 24 (normal) or 6, 6, 36 (heroic), half each colour. They
spawn on a 47 yd circle round `ARENA_CENTER`, 1.5 yd up, and fly chords to random points on it at
7-8 yd/s (flight or run speed, unmeasured), despawning at an endpoint two times in three. Every
85 ms (100101) an orb takes the nearest **player** within 2.75 yd 2D, never a pet, and explodes:
Unleashed Light/Dark, 6 yd, 7,800-8,200 / 7,800-8,200 / 8,775-9,225 / 14,625-15,375, absorbed by
the matching essence. An orb its own colour sets off still hits the other colour within 6 yd. Fjola
schedules the waves and runs no event while casting, so one due in her Light Vortex spawns as the
channel ends (25H 2026-11-16: due 1:31.2, spawned 1:41.2).

## Tanks, health, berserk

- Twin Spike every 7-10 s from 5-8 s: 100% weapon damage (125% heroic), physical, +20% damage taken
  for 15 s. Otherwise tank damage is light; Power of the Twins is the peak.
- Taunts diminish (`flags_extra` 0x80000 on all eight entries): 100 / 65 / 42 / 27% duration, then
  immune, threat effect included, until 15 s pass untaunted. Class taunts recharge in 8 s, so
  taunting on cooldown reaches the immune 5th.
- Berserk 64238 (+900% damage, +150% haste) at 10 min normal, 6 min heroic.
- HealthModifier 435 / 2000 / 600 / 2800, rescaled by `mod-dungeon-scale`: read `mhp` off a trace.

## Wipe and kill

An evade sets `TYPE_FAILED`, and `InstanceCleanup` despawns both twins with their portals and orbs
and strips every essence, Empowered and Touch (`DoAction(-1)`). Barrett's gossip summons fresh twins,
so each attempt restarts every timer. The kill strips the same auras.

## Floor

No point on the arena floor can be verified offline ([README.md](README.md#arena-floor)); anchor
only on the script's own points, `ARENA_CENTER` and the portals. The entrance tunnel's floor is the
same GO (navprobe `point` reads no mesh there), so its stands sit on cells players stood on across 66
ToC traces: x 561-565 for y 79-87, x 557-566 at y 88-89, z a flat 395.2. The portcullis GO 195648 at
(563.5, 78.2) closes when the twins are summoned. The tunnel width is unmeasured: the prebuilt
navprobe (2026-09-05) predates `los`.

## Strategy

The Wowhead heroic "door strategy", adapted: the raid stands in the entrance tunnel, 51+ yd from
`ARENA_CENTER`, past every orb (they never leave the 47 yd circle), so nobody dodges and no goalie
has anything to catch. Everyone holds Dark and tunnels Fjola; Eydis's tank holds Light.

**Door stands**, `GetTwinDoorStand`, dealt by guid over the group's bots (alive or dead, a tank spec
off the tank list last), so staging and the fight agree. Fjola's tank (560.5, 88.2), Eydis's
(565.5, 88.2), 5 yd apart for cleave; a lone tank (563.5, 88.2). Melee at y 87 and 85.5, healers
84 and 82.5, ranged 81 and 79.8, each row at x 563.25 / 561.5 / 565; past six a role starts over,
stacking costs nothing here. Hunters take the back row from x 565 in. The twins hover ~6 yd over
their tank, so melee reach them from ~10 yd 2D (3D 11.8), and no shot casts inside that 11.8: the
ranged rows sit only 12-14 yd from a twin parked on the arena side of her tank. A
twin moves again only once her target moved past ~9.1 yd 3D (`CONTACT_DISTANCE` plus both reaches
less her hover, ~6.9 yd 2D): she parks that short of a standing target on the side she came from,
overtakes a tank walking her in and stops right over it, and pulled into the tunnel stays there. They face the raid and melee eat parries: turning them round would put the tanks in
the orb field.

**Colours**, `GetWantedEssence`, first match:

1. **Vortex** running, bot not matching it: a tank **holds** its colour (walking would drag its twin
   off the door), and on normal so does the raid through a Light Vortex if a bot paladin running its
   own Fire Resistance Aura had Aura Mastery ready at the cast start (latched then). Otherwise the
   Vortex's colour. Outranks a Touch: the Touch's other colour would eat the whole Vortex, up to
   80k. A non-tank holding it walks on to wait 10 yd from the portal it swaps at next, its Touch's
   colour or else its base one, toward its stand (`twin valkyr walk to next portal`), so that swap
   lands as the channel ends: a held Touch ticks every 2 s from then. Heroic can't hold: 25H lands ~54k through the measured 20% resist and 13% absorbs, ~35k
   even with Aura Mastery and Divine Guardian, against 25-33k health.
2. **Touch** on the bot: its colour, re-taken if already held (during a Vortex of that colour too).
3. **Base**: Eydis's tank Light, everyone else Dark, even after a Touch or the Light Vortex left it
   Light. Eydis's Touch only picks a Light holder no twin has as victim, so it finds nobody; the
   2026-10-25 25H kill's rule "keep what it holds" bounced 3 Touch pairs.

No colour swap for a Pact.

| Special | Normal | Heroic |
|---|---|---|
| Light Vortex | nobody moves; raid covers and personal defensives | raid swaps Light and back; Fjola's tank holds Dark on a defensive |
| Dark Vortex | raid immune; Eydis's tank holds Light on a defensive | same |
| Fjola's Pact | DPS already on her at +50%, kick | same |
| Eydis's Pact | ignored, DPS stay on Fjola | DPS switch to her at −50%, lust, kick |

**Essence walks** go to the portal of the colour with the shortest walk there and on to the bot's
door stand (from Light 1 the nearest Dark portal is Dark 2, 80 yd from the door), at
`MOVEMENT_FORCED`, and gossip in reach. Heroic Light Vortex: Light 1 (~40 yd from the stack, inside
the 6 s cast with little to spare), on to Dark 1 during the channel while no wave spawns, swap as it
ends, back (from Dark 1's spot ranged reach both twins at 26-29 yd, healers the tanks). Touch and
Vortex walks first clip a cast that pins the feet; base walks, the one to the next portal included,
only a channel with over 1.5 s left (a warlock re-channels Drain Soul on low twins). A walk stopped 500 ms short of
range clears `last movement`, which otherwise refuses the repeat as a duplicate for 5 s; one still
walking holds the tick, or a lower node's charge leaps the bot back onto the twin. Touch and
Vortex walks and the orb dodge also clear a booking headed elsewhere: forced doesn't outrank forced.
Once the gossip lands the essence the bot stops rather than finish the walk under the lock.

**Orb waits** (`TwinValkyrWalkWait`, `tv.wait`). Out in the arena bots stand a yard or two apart, so
any blast reaches the bot. Each essence walk, the next-portal walk and the door stand predict the
closest pass of every harmful orb (straight to its destination at its spline speed) over 1.5 s; the
bot stops when walking on passes inside 6.5 yd and closer than standing (`orb`), and in gossip reach
skips a swap whose new colour would bring a harmful orb closer (`swap`). A Vortex walk waits only
while it still ends 1.5 s before the cast (Nitro's test; never once it channels); every stretch of
waiting caps at 3 s. A waiting healer, or a bot inside the dodge's trigger, hands the tick on to its
heals or the dodge. A healer on a base or door-stand walk also stops (`heal`) for a group member under
35% within 40 yd while standing is orb-safe: the walks outrank every heal node.
`TwinValkyrBoostEssenceWalk` spends Nitro Boosts (3 min, so about one per pull) on a walk over 15 yd,
mid-pull only: a Vortex walk when it would end under 1.5 s before the cast does (heroic's 40 yd in 6
s; normal's 35-38 yd in 8 s doesn't), a Touch or base walk only once the pull's Light Vortex has
started, which on normal means the trip back to Dark and the stand; never the base walk to the next
portal, which has the channel. No
essence-swap multiplier: a blanket `MovementAction` veto also vetoes `AttackAction`
([pitfalls.md](../../engine/pitfalls.md)).

**Door hold.** `twin valkyr door hold` blocks reach (heal reach too), set behind, rear flank,
formation, follow, flee, run away, move random, charge, blink and disengage for every bot with a
door stand once a twin is in combat, tanks included, except a tank fetching its twin, and a twin
tank from the summon: reach melee on her would send it 40 yd out and back. Both it and the door
stand wait for the pull: the gate opens at the Faction Champions kill, so bots would stand in the
tunnel and stop following from then. Essence walks and the orb dodge are the
encounter's own movers. `twin valkyr door stand` walks a non-tank back past 2.5 yd off and parks it
within 1 yd (straight walk, the tunnel has no navmesh), yielding to any essence walk.

**Targets**, `GetTwinDpsTarget`: a player's skull on either twin, then the Pact twin (heroic either,
normal Fjola's only: Eydis's heals 20%, not worth −50% damage), then the twin of the other colour
than the bot's essence (Light → Eydis, Dark or none → Fjola). A melee bot skips a twin more than
12 yd off her door spot, falling back to her sister (rule `door`). Under a lone tank all DPS stay on
Fjola outside a Pact: Light DPS would pull Eydis, and diminishing taunts can't bring her back. A
shared mark would halve every bot of one colour, so each DPS bot attacks its own pick
directly ([README.md](README.md#targeting-is-direct-no-raid-icons)). `twin valkyr dps target guard`
zeroes `dps assist` on the node's own test: `IsToCDpsBot`, fight live, a pick.

**Tanks.** Alive tanks ranked main, then assists with assistants first, each passing `IsTank` or
`IsTank(bySpec)` (the strategy read flickers): Fjola to the first, Eydis to the second, both to a
lone one (a human on the list keeps his twin; the user tanks Eydis). The twins turn aggressive and
attackable ~9.5 s after the summon (`EVENT_VALKYRIES_ATTACK`), idle ~85 yd from the door, and fly at
10.5 yd/s (`speed_run` 1.5). A bot tank waits on its door spot, its hold neither attacking nor
taunting before the pull: an attack already running would make the `attack` command a duplicate the
bot refuses. The player pulls (proximity 23 yd; the engaged twin takes the puller, her sister her
nearest player) and runs into the tunnel, or tells a bot tank to `attack` (or `tank attack`) either
twin: that leaves her as its `pull target`, and every bot tank then fetches its own twin, so each
sister takes her own tank. The tank taunts its twin once she's in range and taunts back any assigned
twin on someone else. Mid-pull only a twin on someone else, more than 30 yd off her door spot and no
yard closer for 2 s sends her tank out. A fetch walks to 25 yd of her for the taunt, the door hold
off so reach melee can finish. Then it walks her back by a lead 5 yd out of its spot toward `ARENA_CENTER`
(0.6 yd inside the orb circle), waits up to 3 s for her to stop, and steps back to the spot
(`DragBossToAnchor` stops 12 yd short), backing up while she's on the far side: she stays 5 yd out,
clear of the hunters. Both tanks hold their colour through every Vortex
(above) on the Vortex defensive, or on heals with none ready. The first 15 s cost each tank 4-5k
damage per second, too much for the drag the kill used. `twin valkyr tank target guard` zeroes `tank assist` while its twin is attackable. Multipliers:
`TwinValkyrTauntGuardMultiplier` zeroes a taunt on a twin another living tank owns,
`TwinValkyrControlTankMovementMultiplier` zeroes formation moves for a tank holding its twin.
Hunters and rogues redirect to their target twin's tank, casting outside the queue, so
`TwinValkyrRedirectGuardMultiplier` zeroes the class Misdirection and Tricks actions and the
opener's redirect. A hunter waits for its twin in Auto Shot range: Misdirection's 4 s transfer
starts on the next attack, Kill Command included. Twin Spike
gets no defensive node.

**Vortex defensives.** `GetTwinVortexHit`, for a bot whose want is a hold: the shared
`RaidPredictedHit` from the channel's first tick (a period in), 4 s, the remapped tick's DBC average
times the ticks left, the Vortex's school. `twin valkyr vortex defensive` casts
`GetRaidPersonalDefensive`'s pick. On a normal Light Vortex hold, `twin valkyr vortex raid cover`
casts `GetRaidCover`'s deal (Aura Mastery, Divine Sacrifice with Divine Guardian, Anti-Magic Zone,
Divine Hymn, Tranquility) and `twin valkyr raid cover hold` blocks those class nodes during the
lead-in. Fjola's tank on 25H faces ~64k after Fire Resistance Aura.

**Twin's Pact.** Every DPS goes to the Pact twin unless a player's skull is on her sister, normal's
Eydis aside.
`TwinValkyrInterruptHoldMultiplier` zeroes the seven working interrupts while any twin is shielded,
and silencing shot, strangulate, silence, hammer of justice and bash on a twin always. Arcane
torrent stays: only the racial nodes cast it, for its resource. Once the shield breaks, the duty
falls to the lowest GUID among bots on the map
with a ready, in-range working interrupt that are not swapping for a Touch or Vortex, as on Vezax.
Ready also means off cooldown and affordable: `CanCastSpell` skips power, and a pet spell's
cooldown; the warlock casts Spell Lock itself, so its cooldown and range are the warlock's.

**Lust and burst cooldowns** (`TwinValkyrLustWindowOpen`, latched per pull, reset when neither twin
is in combat). Heroic: Eydis's shield goes up, or the 3rd special starts with no Eydis Pact among
them (a count of specials, a Vortex, shield or Pact appearing). Normal: both twins on a listed tank
within 12 yd of their door spots, or 20 s into the pull. A DPS bot holds every gated burst cooldown
until lust lands on it (Bloodlust or Heroism up, or Sated or Exhaustion gained this pull); one Sated
from before the pull, or with no living shaman left with lust off cooldown, waits only for the
window; otherwise the window plus 10 s at most, for a lust that never comes. Healers hold only Power
Infusion (`twin valkyr power infusion hold`, both its nodes, since the burst row's flag covers every
role), tanks nothing, Shadowfiend stays free. Heroic cooldowns can wait to ~2:15, so 2-3 min ones
lose a use.

**Orbs.** A bot dodges orbs of another colour (every orb without an essence) and of its own colour
while a group member without it stands within 7 yd, testing points a yard apart along each orb's
next 7 yd (a second) of flight: trigger at 4 yd, search clear by 5 yd within 12 yd, clipping a
pinning cast only with a point within 3.25 yd. A found spot is kept while it stays 4 yd clear (so
it can't re-fire the trigger) and the dodge ran within 1 s (so no later wave reuses it). A wider
band or longer horizon dodges permanently: 6 yd over more path covers most of the 47 yd disk on a
heroic wave ([pitfalls.md](../../engine/pitfalls.md)). It now only fires for bots out in the arena
(portal walkers, a fetching tank); nothing reaches the tunnel. No goalies or orb collection:
Empowered comes from matching Vortex ticks (the Dark Vortex gives the Dark raid ~80 stacks). The
dodge yields only to a Vortex swap with no time left to wait.

**Lookups.** One scan per instance per ms (`RaidInstanceState`) resolves the twins through
`GetGuidData`, reads the Vortex, the Pact, the shield, the tank list and the pull's state (specials,
lust window, each twin's approach to her door), and takes every orb and portal in one 100 yd sweep
from a twin; `GetFirstAliveUnitByEntry` is deprecated and sight-capped, and per-bot sweeps and group
walks multiply. The door deal refreshes once a second.

| Trigger | Action | Relevance |
|---|---|---|
| `twin valkyr pact interrupt duty` | `twin valkyr interrupt pact` | `ACTION_EMERGENCY + 8` |
| `twin valkyr vortex requires essence` | `twin valkyr swap essence for vortex` | `ACTION_EMERGENCY + 7` |
| `twin valkyr touched requires essence` | `twin valkyr swap essence for touch` | `ACTION_EMERGENCY + 6` |
| `twin valkyr vortex raid cover` | `twin valkyr cast vortex raid cover` | `ACTION_EMERGENCY + 5` |
| `twin valkyr vortex defensive` | `twin valkyr cast vortex defensive` | `ACTION_EMERGENCY + 4` |
| `twin valkyr orb incoming` | `twin valkyr dodge orb` | `ACTION_EMERGENCY + 3` |
| `twin valkyr next portal due` | `twin valkyr walk to next portal` | `ACTION_EMERGENCY + 2` |
| `twin valkyr needs base essence` | `twin valkyr take base essence` | `ACTION_RAID + 5` |
| `twin valkyr engaged by main tank` | `twin valkyr main tank hold light twin` | `ACTION_RAID + 4` |
| `twin valkyr darkbane needs assist tank` | `twin valkyr assist tank hold dark twin` | `ACTION_RAID + 3` |
| `twin valkyr redirect threat` | `twin valkyr redirect threat` | `ACTION_RAID + 2` |
| `twin valkyr door stand` | `twin valkyr move to door stand` | `ACTION_RAID + 1` |
| `twin valkyr dps target` | `twin valkyr focus twin` | `ACTION_RAID` |

### Known gaps

- 25N 2026-10-28 (trace `649_3_fjola-lightbane_1793222782`): 2:22, 0 deaths, the raid in the tunnel
  87% of living time, 2 Unleashed hits there. Its Light Vortex swap took Light 2.3-5.1 s into the 8 s
  cast and kept bots off their stands 17.8-23.5 s from the cast (77-108 yd); nobody used Nitro then.
- 25H 2026-10-29 (trace `649_2_val-kyr-twins_1793302950`, 4 bot healers): 3:21, 11 deaths. Against
  the old centre stack (2026-10-25, 2:43, 56 Unleashed hits, 3 Eydis Touches per Light Vortex): 0
  Unleashed in the tunnel, one Eydis Touch, tanks alive through each Vortex, no bot Vortex tick (13
  Nitro uses), lust inside Eydis's Pact; the kill missed the 3:00-3:15 target and DPS cooldowns
  against the lust window went unchecked. 8 died at 3:14, after the Light Vortex: that Touch's 3 ticks, then the ~55 yd walk
  Light 1, Dark 1, stand with healers moving at the raid's low point (32% average), and one Light
  orb going off where bots still walking out on Light crossed bots walking home on Dark, its blast
  killing 4. The raid still takes Light 1 and Dark 1 around the channel, by the user's choice.
- 25H 2026-11-02 (traces `649_54_val-kyr-twins_1793657909` and `_1793658373`): a wipe with the twins
  at 3%, then a 3:21 kill with 1 death. The wipe: Eydis's Touch, picked on Assasin while Light after a
  Fjola Touch, landed after his swap back to Dark and was never re-taken (16 s, 1.09M) before the
  Light Vortex walk. Hunters did 5-8k against 10-16k: Fjola, fetched and walked in, stopped over her
  tank, 10-11 yd 3D from every hunter stand.
- 25H 2026-11-04/08/10 (traces `649_3_fjola-lightbane_1793819637`, `649_1_val-kyr-twins_1794146392`,
  `649_2_val-kyr-twins_1794263217`): kills at 2:59.9, 3:16.8 and 3:11.1 with 0, 3 and 2 deaths, every
  one on the Light Vortex walk (11-04 had none). 11-10: the column ran out through a 36-orb wave
  spawned 1 s before the cast, the dodge off, and two Light blasts (8, 6 hit) killed two healers.
  11-08: 14 bots swapped to Dark at one spot by Dark 1 beside a Light orb (14 hit); no direct heal on
  the 6 s walk home; Tree and Assasin flipped between dodge and door stand into an 8-hit blast.
- 25H 2026-11-16 (trace `649_1_val-kyr-twins_1794829396`, orb waits live): 2:45.8, 0 deaths, no
  unabsorbed blast. Bots stood a median 8.1 s at Light 1 through the channel with 1-3 orbs up, then
  walked to Dark 1 through the 36-orb wave spawned at its end: Dark 4.6 s and the tunnel 7.0 s after
  it (11-10, no wave: 2.7 and 5.6 s). Both twins first took Deathsong's ghoul, in 11-10 too.
- A tank's pull from ~85 yd backs up facing her the whole way in, ~13 s at backpedal speed with her
  in melee, the first ~4.5 s past healer range.
- The dodge clears 5 yd against a 6 yd blast, and stacked bots all pick the same nearest clear spot.
- A twin a heal-threat pull parks deep in the tunnel stays there: re-parking her needs ~10 yd into
  the arena.
- Each Fjola Touch ticks once: the 25-27 yd walk to Light 1's reach takes 3.2-4.0 s, the first
  tick lands at 2 s.
- 25H Surges dealt 33k/s after resists (6.66M over 3:21); 4 healers held the raid at 76-81% between
  specials, so any Touch tick eats into it.
- A human paladin on Fire Resistance Aura leaves no bot fire holder (`ResistAura` skips his school),
  so the normal raid hold through a Light Vortex never latches and the raid swaps (25N 2026-10-28).
- Humans hold no stand and no interrupt duty: there is no `PlayerbotAI` to ask. A human Eydis tank
  who keeps her in the arena leaves melee on Fjola only.
- A tank with no defensive ready holds through a Vortex anyway, on heals.
- The heroic Light Vortex walk to Light 1 barely fits the 6 s cast; a late bot without Nitro Boosts
  eats a tick or two.
- A lone tank can't truly hold both (**Targets**): Light bots deal half on Fjola, and an Eydis
  Pact still pulls her off.
- Deferred cleanup: each essence and orb trigger re-derives `GetWantedEssence` every tick. A
  per-bot, per-ms memo in `TwinValkyrState` must match its `tv.essence` probe and the peer loop in
  `IsTwinPactInterrupter`, which reads other bots' wants.

## What a trace answers

Position, verdicts and movement come from the raid-agnostic streams. The Twins rows, under
`--notes tv.`, each per bot on change unless marked:

| Key | Says |
|---|---|
| `essence` | `<reason>:<colour>` wanted: `vortex`, `hold` (keeps its colour through a Vortex), `touch`, `base`, or `none` with neither twin alive |
| `target` | `<fjola\|eydis>:<rule>`, the twin the DPS rule names and why: `skull` (a player's), `pact`, `lone` (a lone tank), `door` (a melee bot's sister fallback), or the essence held (`light`, `dark`, `none`); `none` with no twin in reach |
| `door` | `<role>:<x>,<y>`, the door stand: role `tank`, `melee`, `ranged`, `heal`; `none` without one |
| `nitro` | `vortex`, `touch` or `base`, the walk a Nitro Boosts use went on; per use |
| `tank` | `fjola`, `eydis`, `both` or `none`, tanks only |
| `shield` | The shielded twin, `0` none, `1` Fjola, `2` Eydis. Per instance |
| `interrupt` | `duty`, `standby` or `none` (no ready interrupt), written by every living bot while a twin casts an unshielded Pact; a dead bot's goes stale, so the reader cuts it at the death |
| `orb` | The rule of the hazard point nearest the bot when it found a dodge spot, `wrong` or `splash`, or `none` when no spot was found |
| `wait` | Why a walk stands still: `orb`, `swap`, `heal`, or `none` |

`tools/botobs/bosses/val_kyr_twins.py` reads the rest: per bot, time without an essence and swaps
under the `tv.essence` reason held when the essence landed, and per Vortex, how many held its colour
and who took unabsorbed ticks, by role, split into bots holding on purpose and missed swaps, and for
the bots that swapped onto its colour the median time from the channel's end to their next swap
(negative: early) and to the tunnel (`--essence`); each Touch's hold, what ended it and the raid
damage meanwhile (`--touch`); each Pact's shield up and down off `tv.shield`, cast length, outcome,
kicks wasted on the shield and after the break, duty and lust (`--pact`); Unleashed hits taken or
absorbed, peak Powering Up and Empowered per bot, each blast with unabsorbed damage (hits of one
spell within 50 ms, its bots hit and absorbed, tunnel or arena), and which bots dodged under each
`tv.orb` rule (`--orbs`); each twin's time on her `tv.tank` tank, taunts, distance from `ARENA_CENTER` and between
the twins, her first victim from the engage and when her own tank took her (`--tanks`); each DPS bot's time on the twin the rule names: its `tv.target` twin while
that reads `skull` or `door`, Fjola while a `tv.tank` reads `both`, no Eydis Pact on normal
(`--targets`); per bot, living time within 2.5 yd of its `tv.door` stand and 51+ yd out, time out
of the tunnel by `tv.essence` reason, Unleashed hits taken in the tunnel and out, and `tv.nitro`
uses by walk (`--door`). In a trace the summon opened, time shares count from the engage (`pull` row
`src: rename`).

Still invisible, and what stands in:

- **An essence taken before the trace opens** has no apply row. A bot's first removal names the
  colour it held; a bot that never swapped is placed by the Surges it took, since each skips holders
  of its colour's essence; one neither Surge touched reads `?`.
- **The shields and the Pact heal**: creature auras and heals are never recorded, so `tv.shield` is
  the shield and a jump in shared health the heal. A Pact the casting column drops more than 1 s
  early with no jump reads `kicked`.
- **Touch rows** carry the full amount; their `ab` is shield spent, not damage prevented.
- **Vortexes and Pacts** come from cast rows, so a twin RaidObs never watched drops out of both; the
  banner names her.
- **A lone human tank** writes no `tv.tank`, so `--targets` scores its DPS by colour.

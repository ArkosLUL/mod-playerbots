# Mimiron polish log (25-man hard mode, Firefighter)

Reader: `tools/botobs/bosses/mimiron.py <trace>` (`--phases`, `--heal`, `--walk`, `--fire`, `--p3`, ...).
Scenario `uld-mimiron`: the tank presses the Big Red Button as the pull's start, which arms hard mode;
a wipe resets it. Hard mode is real whenever fire spawns (Flames 64566 ticks from the first seconds),
so every pull below was a hard-mode pull. A kill counts only in hard mode.

## Round 1 (build 02925e260, DC 5443dbc)

**Data:** window 8, one valid pull per pool (`raidtest.py report --boss mimiron --raid uld --head
02925e260 --pulls 10`), plus window 7's smoke wipe on uld2 (`--head 61011f323`). Traces
`env/dist/logs/botobs/603_<n>_mimiron_<epoch>.ndjson`. No DC veto inside any pull. Comparison:
three older human-led wipes on the same map (`603_26_mimiron_1795128319`, `_1795182524`,
`_1795186335`), which reached phase 4.

| Pool | Run | Trace | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261122-121130-29` | `603_29_…_1795349556` | wipe 10:41 in P3 at Berserk (10:13), ACU ~15% | 32: P1 Proximity Mine 2; handover fire 1; P2 Heat Wave/Rapid Burst 7; P3 Bomb Bot 10, Plasma Ball 7 (all after Berserk), Water Spray 3, Assault Bot 2 |
| uld2 | `tr-20261122-121150-30` | `603_30_…_1795349578` | wipe 9:24 in P3 | 35: P1 Plasma Blast tank 1 then ranged 4, Shock Blast 1, fire 1; handover fire 2; P2 Boomer XP-500 trash 2, Rapid Burst 1, fire 1, Frost Bomb 1; P3 Plasma Ball 7, Bomb Bot 5, Water Spray 4, Assault Bot 3, Junk Bot 2 |
| uld3 | `tr-20261122-121109-28` | `603_28_…_1795349533` | wipe 6:53 in P2 | 32: handover fire 7 (melee); P2 Heat Wave/Rapid Burst 18, fire 6, Laser Barrage 1 |
| uld2 (w7) | `tr-20261122-101642-31` | `603_31_…_1795342659` | wipe 7:10, raid dead in P2 | 31: P1 Plasma Blast tank 2 (soulstoned once); P2 Heat Wave/Rapid Burst 21, fire 2; P3 Junk Bot 4, Bomb Bot 1, Plasma Ball 1 |

All four, 130 deaths: P2 Heat Wave/Rapid Burst 47, fire 20 (10 in the 1→2 handover, 9 in P2, 1 in
P1), P3 Bomb Bot 16, ACU Plasma Ball 15, P3 adds 11, Water Spray 7, P1 Plasma Blast 7 (3 on the tank,
4 on ranged after it), mines 2, Boomer XP-500 2, Shock Blast, Laser Barrage and Frost Bomb 1 each.

**What wipes the raid.** Two pulls die in P2 (uld3, w7 uld2), two in P3 (uld1 at Berserk, uld2 by
attrition); none reached P4. P2 is a mana wall here: it ran 136-258 s against 80-89 s in the human-led
pulls (raid output on VX-001 roughly half), and in both P2 wipes three of four healers were under 10%
mana about 100 s into it, then the raid fell to Heat Wave within ~30 s (w7 uld2 4:35-4:58: 15 deaths).
Healers enter P2 low partly because of the handover: across it one human-led raid's healers went
46/55/67/59 → 66/66/63/65 (drinking), while in the bot-only pulls 10 of 16 healers lost 5-22 points
(uld3 56/57/64/52 → 69/37/54/32, uld2 58/60/45/64 → 42/55/35/66, w7 uld2 63/47/47/44 → 41/50/36/55,
uld1 48/68/75/59 → 59/55/60/66), entering P2 at 32-69%. The roster DKs' missing talents (~25 of 71) shave raid damage, which lengthens P2 and
let uld1 reach Berserk in P3; not worked around.

**Failure picked: bots stand in fire through the phase handovers.** With no player in the raid
nothing is followed (the handover's mover is `follow`), so the raid stands where P1 ended for ~48 s,
in the field P1 laid. Bots then sit in burning nodes: runs of 20-90 consecutive Flames ticks with 0-1
dodges (uld3 Uldzas 90, Uldzab 66, Uldzam 58; uld2 Uldyaq 44; uld1 Uldxag 31), against runs of a few
ticks in combat. Two causes:

1. **Nodes inside the west door's model are invisible to the hazard read.** `ReadMimironFirefighterHazards`,
   `ReadMimironMarkers` and `IsMimironSpotMineSafe` scanned `"nearest npcs"`, and
   `NearestUnitsValue::Calculate` keeps only units passing `IsWithinLOSInMap`. uld3: ten melee and the
   off-tank stacked on (2688.2, 2558.3) from 1:44; Uldzay was ticked by node 4294968963 at 2.3 yd for
   34 s and by 4294968977 at 2.2 yd from 2:14, fell to 26-35% health, and over the ~10 passes before
   its death at 2:17.25 wrote no `mimiron.flee` note (its last `flames` notes are from the 0:13 pull;
   the trigger's `cov` row for the non-combat engine: 157 checks, 2 fires). By the trigger code
   (`UldTriggers_Mimiron.cpp:271-328`) two nodes in 3.5 yd, or one at under 60%, must fire; the only
   per-unit filter left is sight. Meanwhile `mimiron drink guard` (same read, 15 yd) kept vetoing
   their `eat`, so farther nodes were visible, and Uldzax 0.7 yd off saw the same nodes and dodged at
   2:15.3. Standing bots under 55% health ticked by fire drew no flee note within 1.2 s in 102 of 109
   samples inside the alcove (x < 2695), against 10 of 20 out of combat and 9 of 20 in combat in the
   room. The closed west door, `GO_MIMIRON_DOOR_1` 194776 at
   (2677.9, 2569.83), `DOOR_TYPE_ROOM` so closed while the boss is in progress, has display 8684
   (`UL_GNOMEWING_DOOR_01.MDX`) with a GameObjectDisplayInfo box of x -6.1..14.5, y -17.9..18.1: world
   x 2671.8-2692.4, y 2552.0-2587.9, the whole alcove including the phase 1 tank spot (2691.6,
   2568.5). The uld2 pair that burned at (2677.9, 2559.7) stood on the door line. The door as the
   blocker is by elimination; not navprobed (no Docker in this lane).
2. **The parked 80% rule never trips in a handover.** Outside the door box the trigger did see the
   node, but with nothing to heal four idle healers top a parked bot within a second: uld2 Uldyaq sat
   44 ticks in node 4294969055 at 76-100%, Uldyav 22 ticks at 88-100%, and both dodged only once P2
   started (Uldyaq at 3:38, 1.3 s after its first target). The 80% line (`ULDUAR_MIMIRON_FLAMES_PARKED_DODGE_HEALTH_PCT`)
   exists to stop exactly this ("healers held parked bots above 60, so one node never ended"); the
   health gate's premise is uptime lost by leaving, and a handover has nothing to attack.

**Classification:** bug. The strategy means bots to leave fire, handovers included (the drink guard and
fire hold guard exist for the handover); they don't.

**Fix** (Mimiron files only, any size and difficulty):

- `FindMimironFloorUnits` (`UldEncounter_Mimiron.cpp`): a grid search by entry over `SightDistance`
  with no sight test, now used by the fire/bomb/fire-bot read, the mine and rocket marker read and
  `IsMimironSpotMineSafe`. Normal mode reads mines and rockets the same way.
- `MimironDodgeFlamesTrigger`: after the existing gates, a standing bot with no mech attackable
  (`GetMimironRingFocus` null) leaves any burning node, at any health. Walking bots keep the 60% gate
  (review), so a human-led raid's followers crossing the field aren't thrown off the walk and held
  from `follow` for 2.5 s per node.
- `MimironProximityMineAction` (review): `FindNearestCreature`, the trigger's own lookup, instead of
  `"nearest npcs"`. The trigger saw alcove mines the action couldn't, so melee there never dodged
  them in a handover.

Not changed: `GetMimironBombBotChasing` (Bomb Bots are selectable and live in P3, away from the door).

Seen in review, not picked: w7 uld2 Uldyaj stood in two nodes for 7 s casting Revive Pet (8.8 s),
which no trigger interrupts; the handover rule should move it before the cast starts.

**Not picked.**

- P2 Heat Wave/Rapid Burst attrition (47) is the mana wall above: the handover fix is the part a bug
  fix reaches; the rest is raid output (gear, DK talents) and the plan question Q1.
- P3 Bomb Bots (16, 10 in uld1): ranged a Bomb Bot chases stay and shoot by design and die when it
  lands; worth its own round once raids reach P3 reliably.
- P1 Plasma Blast on roster B's warrior main tank (uld2, both windows, 1:16): Last Stand took window 1,
  Shield Wall window 2, and window 3 went to a paladin's Hand of Sacrifice (30%), which the claim then
  held against Pain Suppression or Guardian Spirit; the tank died, and in window 8 the MK II then
  killed four ranged with Plasma Blast and one with Shock Blast before the off-tank had it. Candidate
  for round 2: the healer external order (Guardian Spirit or Pain Suppression ahead of Hand of
  Sacrifice) and the off-tank pickup after a main tank death.
- uld2 P2: a Frost Bomb flee ran two melee from the west stairs (x 2665) 25 yd further west into the
  corridor, where Boomer XP-500 trash (34192, 72 in the trace, roaming 100+ yd west of the room
  centre) blew them up. A DC `preDespawn` of them would remove it, if wanted.

**Next window should show** (`mimiron.py --fire`, `--phases`; trace scripts in the lane scratchpad):

- No fire deaths in the handovers (window 8: 10 in three of four pulls), handover Flames damage per
  pull far below window 8's (uld3 Z2+H1 1.75M, uld2 283k, uld1 206k), and no run of 10+ consecutive
  ticks on a standing bot with no `flames` flee note in Z or H spans.
- Inside the door box (x 2672-2692, y 2552-2588) standing bots ticked by fire under 55% health draw a
  `flames` flee note within ~1 s, as in the room.
- More `flames+N` flee notes in Z/H spans, and no new A-B-A between the flames dodge and anything else
  there (nothing else moves bots in a handover).
- No handover mine hits on melee in the door box without a `mine` flee note.
- Healer mana at P2 start no lower than window 8's; it may not rise, since the drink guard still vetoes
  drinks within 15 yd of a node (Q1).

**Questions for the user.**

- **Q1 (plan change): a bot-only handover position.** With no player in the raid, the 1→2 handover
  leaves the raid in the phase 1 field, melee in the west alcove, so healers can't drink (15 yd drink
  guard) and P2 opens from the west in a line. Should a bot-only raid walk itself to clean ground for
  the handover and walk in over the last ~10 s, as human leaders do (stand ~45 yd out, then the
  centre)? Recommendation: yes, for the 1→2 handover only, keyed on no real player in the raid like
  Yogg-Saron's phase 1 start; it targets the mana wall that ends two of four pulls.
- **Q2: the DK talent fix** (open on the plan's roster row) matters here: P2 length is the mana wall
  and uld1 lost to Berserk with the ACU at ~15%. Recommendation: fix provisioning before judging P2
  and P3 output on these rosters.

**User answer (2026-11-22):** yes. A bot-only raid gets its own phase 1 to 2 handover position:
out to clean ground about 45 yd from the centre, back in over the last ~10 s, triggered by no real
player in the raid (like Yogg-Saron's walk-in).

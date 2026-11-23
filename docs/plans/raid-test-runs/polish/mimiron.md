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
player in the raid (like Yogg-Saron's walk-in). Round 3 drops that trigger under the user's one code
path decision: every Firefighter raid holds the band.

## Round 2 (Custom eda15422d, round 1 merged as e939553d1)

**Data:** nothing new. Round 1 is merged but no window has pulled it yet (window 10 will), so this
round skips the stop rules, the verdict on round 1 and the failure ranking, and builds the user's
answer to Q1. Read for the shape: the three human-led wipes (`603_26_mimiron_1795128319`,
`_1795182524`, `_1795186335`) and window 8's bot-only fields (`603_28/29/30_…`, `603_31_…` from
window 7). Trace scripts in the lane scratchpad.

- **Human leaders** stood 43-47 yd west (bearing 165-200° off the room centre) through the handover,
  after a detour to 55-63 yd at +10 s in two of three; set off 35-40 s in; stood 23-36 yd out (median)
  at phase 2 and 8 yd five seconds later. Healers drank at +2 to +12 s (1-7 drinks a handover).
- **Bot-only fire at the end of phase 1:** 3-25 live nodes, all at 147-217° and 18-79 yd from the
  centre. Scoring 24 bands (below) on those fields picks 120-150° or 225-255°, with 1-6 of 25 slots
  inside the drink guard's 15 yd at the pick, still 0-6 when re-scored at +17 s and +34 s. Those fields
  never grew toward a band: the raid stood in them, and a chain stops while a player is within 4 yd of
  its head.
- **Script clock** (`boss_mimiron.cpp`): MK II beaten at 0 (NOT_SELECTABLE, damage zeroed under 15000),
  VX-001 summoned at the room centre at 17 s, Mimiron into VX-001 seat 0 at 35 s and seat 1 at 44 s,
  `SetData(1, 2)` at 47.75 s. The pull opens with a NOT_SELECTABLE full-health MK II for ~13 s; the 2→3
  handover summons the Aerial Command Unit 5 s after VX-001 is beaten.

**Built** (Mimiron files and reader only; any size, Firefighter only):

- `MimironHandoverStep`, per instance, refreshed by `TickMimironObs` while nothing is attackable (trace
  latch `mimiron.handover`). Needs Firefighter, the boss in progress, no real player of the group on the
  instance (`MimironBotLedRaid`, removed in round 3), no Aerial Command Unit, and a
  NOT_SELECTABLE MK II under `ULDUAR_MIMIRON_SPENT_HEALTH_PCT` (10). Then **Hold** with no VX-001,
  **Late** with a fresh unselectable VX-001 not riding, **WalkIn** once Mimiron sits in its seat 0 or 1.
  Anything else, None.
- **Hold band**, picked once when the step leaves None (`mimiron.hold`, "<deg> deg <n> fire"): of 24
  bearings 15° apart, fewest burning nodes within 20 yd of the band's middle (40 yd out), ties nearest
  the phase 1 tank spot so the handover's seeds land beside the phase 1 field. Every living member gets
  an `hmhold` slot: rows at 44, 40 and 36 yd from the centre, outer row first, 4 yd apart along a row
  (25-man: 9/9/7, about ±21° at the outer row; 10-man 4/4/2).
- **Walk in:** `DeriveMimironSpreadSlot` hands out the usual phase 2 slots on the staging VX-001
  (`hmwedge` east for ranged, `p2melee` west of VX-001 for melee and tanks), 12.75 s before phase 2.
- **`mimiron handover drink hold`** vetoes `drink`/`eat` in Hold off the bot's slot, and in Late and
  WalkIn: a food-cheat drink stops the bot thinking for up to 20 s, and the walk in starts 18 s after
  VX-001's summon. Off the slot (review): the leg under way fails the formation action every tick, and
  the engine hands that tick to `drink`.
- Reader `mimiron.py --handover` (with tests): step times, the band, raid distance from the centre at
  phase 1 end, +10 s, walk in and phase 2, bots on a phase 2 slot at phase 2, healer mana, drinks,
  Flames and deaths. A phase latch blip splitting phase 1 (P1, Z, P1b) still finds the handover (review).

Player-led raids kept `follow` until round 3. Normal mode is unchanged.

**navprobe for the orchestrator:** the band spans 36-44 yd from the room centre at any bearing. The
findings doc has the floor flat and on mesh to 45 yd on all 16 headings (the retired lap plan, commit
`ab43383d3`: 24/24 at 35, 40 and 44, three east headings off at 48), but nothing between headings.
Check: `navprobe --map 603 --nav 0x09 ring 2744.65 2569.46 364.32 <r> 72` for r = 36, 40 and 44. A
failing arc means a lower `ULDUAR_MIMIRON_HOLD_OUTER_RADIUS` or skipping those bearings in the pick.

**Next window should show** (`mimiron.py --handover`, `--fire`, `--bomb`, `--heal`, `--flips`):

- In every pull reaching phase 2: `mimiron.handover` 1, 2, 3 once each, the walk in about 12.7 s
  before phase 2, one `mimiron.hold` row.
- Raid median ~40 yd from the centre at +10 s and at the walk in, melee out of the alcove; at phase 2
  most living bots on a phase 2 slot (aim: 80%+).
- No handover fire deaths (window 8: 10 in three of four pulls) and handover Flames far under window 8's
  (uld3 1.75M, uld2 283k, uld1 206k).
- Handover drinks (window 8: none) and healer mana at phase 2 at or above phase 1's end (window 8: 10 of
  16 healers lost 5-22 points).
- Phase 2's opening Rapid Burst no worse than window 8 (human walk ins: 324-399k in 30 s).
- No A-B-A between `mimiron arc spread action` and the fire dodge in the Z and H spans.

Watch: the first Frost Bomb's cleared count (`--bomb`); a band north or south of the field may split
the fire in two, and one bomb clears only 30 yd. Drinks started just before VX-001's summon may walk
in up to ~2 s late. Phase 1 chains keep spreading until VX-001 dies, 1.22 yd/s toward the nearest
player: on window 8's fields the nearest slot sits 13-15 yd from a node, so the band's near edge can
burn within ~10 s. If `--fire` shows hold-slot dodges dominating Z and H, drop the tie-break toward the
phase 1 tank spot for the band farthest from the fire.

**Questions for the user:** none new. Q2 (DK talents) is DC lane S, merged.

## Round 3 (Custom 60f6769dc, round 2 merged there)

**Data:** nothing new. Rounds 1 and 2 are merged but no window has pulled them; the newest Mimiron
traces on disk are still window 8's (`603_28/29/30_…`) and window 7's `603_31_…`. Stop rules, verdict
and failure ranking skipped. This round carries out the user's one code path decision (2026-11-22, the
effort's decisions row): a strategy runs the same code with or without a real player in the raid, and a
real player only changes who does a job he is doing himself.

**Changed** (Mimiron files and reader only):

- `MimironBotLedRaid` removed: the handover step (`ReadMimironHandoverStep`, was
  `ReadMimironBotLedHandoverStep`) runs in every Firefighter raid, so a player-led raid holds the band,
  walks in and keeps the drink hold too. `GetMimironBotLedHandoverStep` renamed `GetMimironHandoverStep`.
  RaidObs keys unchanged (`mimiron.handover`, `mimiron.hold`, `mimiron.slot` `hmhold`).
- `mimiron handover follow hold` (new block): zeroes `follow` while a step is on and the bot has a slot.
  With a player leading, `follow` takes every tick the formation lets go (on the slot it stands down,
  mid-leg it fails), so the bot would walk to the player and back: the A-B-A that phase 1's camp needed
  sight fixes for. Only `follow`, only until phase 2 (the step goes None once VX-001 is selectable or on a
  wipe).
- Real players: nothing in the handover moves another unit, and a player keeps his `hmhold` index, which
  just stays empty wherever he stands, so the band's layout is the same code with or without him.
- Reader `mimiron.py --handover`: positions, slot counts and healer mana count bots only (a human is never
  moved by the handover); steps print as `steps:`, and a pull without them as "not Firefighter, or a build
  without the handover hold" (review: window 7 and 8's bot-only traces predate round 2 too). Test with a
  human healer standing on the room centre.
- navprobe (orchestrator, 2026-11-22): `navprobe --map 603 --nav 0x09 ring 2744.65 2569.46 364.32 <r> 72`
  gave 72/72 on mesh at r = 36, 40 and 44, so the band's floor is checked every 5°; header and findings
  doc say so.

Bot-only pulls are unchanged by this round: their step was already on, and `follow` never runs there
(window 8: 276-326 `follow` rows a pull, all `USELESS`; the human-led `603_26_…_1795128319` had 585 `OK`).

**Next window should show:** round 2's list (the window still judges rounds 1 and 2); this round's
block can't show in a bot-only pull. A player-led pull, when one comes, should show `mimiron.handover`
1, 2, 3, the bots on the band and then on their phase 2 slots, the player wherever he chose, no `follow`
row with verdict `OK` from a bot between step 1 and phase 2, and no `follow`/`mimiron arc spread action`
A-B-A in the Z and H spans (`--flips`).

**Questions for the user:** none.

## Round 4 (build 697052475, rounds 1-3 built)

**Data:** `raidtest.py report --boss mimiron --raid uld --head 697052475 --pulls 10 --runs
tr-20261122-164334-10,tr-20261122-164414-11,tr-20261122-164434-12,tr-20261122-164836-13`. No DC veto
inside any pull. Trace scripts in the lane scratchpad (`mim4/`).

| Pool | Run | Trace | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `…-164334-10` | `603_12_…_1795365867` | wipe 8:44 in P3, ACU ~74% | 32: P1 Proximity Mine 2; P2 Flames 8 (3 melee at 3:26-3:28 crossing fire in a Frost Bomb escape under Heat Wave, 5 at 4:03-4:10 after a barrage), Heat Wave 1; P3 Bomb Bot 10, Water Spray 6, Assault Bot 3, Junk Bot 2 |
| uld3 | `…-164414-11` | `603_13_…_1795365908` | wipe 10:30 in P3 at Berserk, ACU ~16% | 32: P2 Flames 1; P3 Bomb Bot 12, Water Spray 7, Junk Bot 2, Assault Bot 1, Berserk Plasma Ball 9 |
| uld2 | `…-164836-13` | `603_15_…_1795366169` | wipe 10:30 in P4 at Berserk, parts at 20-23% | 31: P1 Plasma Blast on the main tank 1; P4 Rocket Strike 2, Hand Pulse 2; Berserk 26 |
| uld2 | `…-164434-12` | `603_14_…_1795365875` | voided: "kill" 2:42, 2 deaths | normal mode, see below |

Three valid pulls, 95 deaths: Berserk 35, Bomb Bot 22, Water Spray 13, Flames 9 (all P2), Assault and Junk
Bots 4 each, P4 Rocket Strike and Hand Pulse 2 each, mines 2, Heat Wave 1, Plasma Blast 1. First phase 4
of a bot-only pull (uld2, 8:24).

**Verdict on rounds 1-3 (handover):** held. Every pull ran `mimiron.handover` 1, 2, 3 once, walk in
12.9-13.2 s before P2, one hold band (240, 240, 150 deg, 0 fire). Raid median 36-40 yd out at +10 s and
the walk in, 18 yd at P2; on a P2 slot at P2 22, 24 and 19 of 25. Handover fire deaths 0 (window 8: 10
in three of four pulls), handover Flames 14k/106k/22k (window 8: 206k-1.75M). Healer mana across it:
3 of 12 healers lost 5-15 points (window 8: 10 of 16 lost 5-22). The mana wall is gone: P2 ran 103, 78
and 83 s (window 8: 136-258 s, human 80-89 s) with 9, 0 and 1 deaths (window 8: two pulls wiped in P2).
Still no handover drink in any pull. Round 3's player-led path can't show in bot-only pulls.

**Voided uld2 run `tr-20261122-164434-12` (DC side, for the orchestrator).** Not a hard-mode pull and not
a real kill:

1. The pool arrived straight from its Iron Assembly kill (`tr-…-163606-9`, instance 11) unrefreshed: the
   first snapshot (t 168 ms) has all 25 stacked on the drop (2717.0, 2570.7) at 20-100% health and 0-99%
   mana, no pre-roll snapshot at all, and DC's record shows `start: engaged`, `startStep: object` never
   reached, status only "preparing pull 1". The engage came ~3 s after the run start.
2. What engaged him: the moonkin Uldyas had cast Starfall (53201) at Iron Assembly 1 s before that kill
   (`603_11_stormcaller-brundir_1795365440`, t 365206). AzerothCore keeps a positive aura's remaining time
   across logout (`PlayerStorage.cpp` `_LoadAuras`, only negative or `AURA_EXPIRES_OFFLINE` auras tick
   offline), so it came back live 70 s later, which only a logout between the runs explains (a 10 s aura);
   at t 2 ms its stars (53195, 53198) hit Mimiron, whose seat on
   the MK II at (2744.65, 2569.46) is 27.7 yd from the drop, inside Starfall's 30 yd.
3. So no button: normal mode. Phase 1 opened 7.7 s in (normal script: talk, sit at 6 s, +2 s; hard mode
   13 s), no Flames 64566 and no Emergency Mode in the trace.
4. The "kill": the raid also damaged Mimiron himself (33350, 1,742,400 hp): 100 → 83% in the first 20 s
   (Starfall), then 82 → 0% across 2:01-2:42 in P2 (pets, Lightning Bolt, Corruption, Devastate and procs
   targeting him; 120 casts at him in the first 10 s, ~560 in 1:50-2:40). His death set `BOSS_MIMIRON` DONE
   (`BossAI` death), so RaidObs closed `kill` and DC's `bossState(10)==3` predicate called success. In the
   ten hard-mode traces he never leaves 100% (at most 7 casts at him, in two human-led pulls). Why he is hittable only
   here is not traced; the likely difference is that the Starfall put him into combat before
   `JustEngagedWith` made him NOT_SELECTABLE.

For DC: strip carried-over auras (or at least periodic damage ones such as Starfall, Hurricane, Volley,
Blizzard, Death and Decay) at spawn or before the teleport, or keep the drop over 30 yd from his seat;
and a refresh-before-teleport would also have stopped the low-health arrival. The runner's void was right.

**Failure picked: the Magnetic Core loop stops once the Aerial Command Unit is at or below 25%.**
Berserk (35) is the largest bucket, and phase 3 is where the time goes: 244 s+ (uld1), 375 s (uld3), 196 s
(uld2). Airborne the unit loses 0.04-0.18%/s (grounded 0.42-1.77%/s), so after two landings it is all
cores. In uld3 the second landing ended at 6:17 with the unit at 28%, 23% by 6:29; the third Assault Bot
died at 6:39 and its carrier Uldzay wrote `mimiron.corestep no-corpse` at 6:40, then three more corpses
(7:40, 9:19, 9:37) went unlooted (`no-corpse` again from the next carriers at 9:19 and 9:52). The unit sat at
16-23% for 211 s until Berserk (16% at 10:00), the 15% fire bot sweep never came (fire bots 8.8 alive on average, 20 at
most), and 24 of uld3's 32 deaths fell in that stretch (Bomb Bot 7, Water Spray 6, Junk Bot 2, Berserk 9).
uld2 hit the same gate: its third Assault Bot died at 6:44 with the unit at ~10%, `no-corpse` at 6:45, and
the unit took until 7:52 to die airborne; phase 4 then hit Berserk with the parts at 20-23%, ~65 s short.

Root cause: `MimironMagneticCoreAction::Execute` (`UldActions_Mimiron.cpp:1565`) gated the fetch on
`!acuLow` (`GetHealthPct() <= ULDUAR_MIMIRON_CORE_HOLD_RELEASE_PCT`) whether or not a core was in hand. The
trigger still fired on the corpse, the action skipped collecting and fell through to `if (!core)` →
`no-corpse`. The 25% line is one of the hold's escapes ("where the phase ends before another core
matters"), written for a core in hand; at 0.04-0.18%/s airborne a bot raid's phase does not end.

**Classification:** bug. The strategy means every corpse to become a landing (the core is the phase's only
killable window, the hold ends once the next Assault Bot is up); the 25% gate was meant to spend a held
core, not to stop fetching one.

**Fix** (Mimiron files only, every size and mode, with or without a player):
`MimironMagneticCoreAction::Execute` skips the fetch only when the core in hand is due now
(`core && (expiring || acuLow || (chaining && ready))`); with empty bags the carrier fetches at any unit
health. Unchanged above 25%, and a held core at or under 25% still goes down at once. Header comment and
findings doc say the escapes end the hold, never the fetch. Review: the action's own comment still gave
the old reason for the 25% line (the phase ends there) and now gives the new one (one landing takes
about what's left). The 3→4 handover is untouched: the beaten unit is NOT_SELECTABLE, so
`GetUldUnitByEntry` misses it and the trigger is off.

**Next window should show** (`mimiron.py --p3`, `--notes mimiron.corestep`):

- With the unit at or under 25% airborne and an Assault Bot corpse up, `loot` then `use` from the carrier,
  never `no-corpse` (uld3 3, uld2 1 this window), and a third landing in pulls that reach 25%.
- Phase 3 shorter in those pulls: the stretch at or under 25% airborne ~30-60 s, not 211 s (uld3) or 68 s
  (uld2); a uld3-like pull reaching the 15% fire bot sweep and phase 4.
- Nothing different above 25% (core steps, landings and their cadence as this window).

**Not picked.**

- Bomb Bots (22, uld1 10, uld3 12; uld2 0). In uld2 ranged cast at Bomb Bots 2.8 times a second of
  bomb-alive time and killed all eight; in uld1 and uld3 0.7, and from 6:51 on uld3's got 0-3 casts each.
  Ranged with the bomb as target spent GCDs on `corruption on attacker`, `shadow word: pain on attacker`
  and the like (rel 15-19.5, above the nukes at 0.5-5) on other units, and dodged fire bot lanes. Part of
  it is phase 3 length (fewer living ranged, bigger fire bot pile), which this fix shortens; the "on
  attacker" DoTs during a Bomb Bot are the candidate for round 5.
- Water Spray (13) with 7.5-8.8 fire bots alive on average (15-20 at most) in uld1 and uld3: the findings
  doc's own cull warning ("nine to fifteen alive ... would say a cull was right"). uld3's pile came from the
  missing 15% sweep (above). uld1 never got the unit under 74%, so its pile was the kept-bot plan itself; a
  plan question if it persists after this fix.
- uld1 P2 Flames (8): melee crossing fire in the 3:26 Frost Bomb escape under Heat Wave, and four ranged
  and a healer after the 4:06 barrage.
- Phase 4 core: in uld2 the carrier looted a phase 3 corpse at phase 4 start (8:24), held it, and walked it
  toward the unit from 9:19 to Berserk, a melee off the rendezvous. The core is a phase 3 tool; a small
  `IsMimironPhase3` gate on `MimironMagneticCoreTrigger` would stop it.
- Handover drinks still 0.

**Questions for the user:** none.

## Round 5 (build d2874f536, round 4 built)

**Data:** `raidtest.py report --boss mimiron --raid uld --head d2874f536 --pulls 10`: one valid pull per
pool, no DC veto inside any. Trace scripts in the lane scratchpad (`mim5/`).

| Pool | Run | Trace | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261122-184807-13` | `603_13_…_1795373329` | **kill** 10:09, parts down 9:51.9 | 9: P1 Proximity Mine 1; P2 Flames 1; H2 Heat Wave 1, Flames 1; P3 Water Spray 1; P4 Hand Pulse 2, Plasma Ball 1, Shock Blast 1 |
| uld2 | `tr-20261122-184907-14` | `603_14_…_1795373390` | wipe 10:25 in P4 at Berserk | 30: P1 Flames 1 (0:17); P2 Rapid Burst/Heat Wave 3 (melee, 3:57); P3 Water Spray 2; P4 Berserk 24 (10:01-10:20) |
| uld3 | `tr-20261122-184928-15` | `603_15_…_1795373410` | wipe 10:06 in P4 at Berserk | 31: P3 Water Spray 8, Bomb Bot 7, Flames 2; P4 Hand Pulse 7, Plasma Ball 4, Assault Bot 1 (8:56-10:00), Berserk 2 |

70 deaths: Berserk 26, phase 4 attrition before it 16, Water Spray 11, Bomb Bot 7, Flames 5, P2/H2 Heat
Wave and Rapid Burst 4, mine 1. Deaths count revives: uld3's Uldzad died three times in phase 3.

**The uld1 kill was hard mode.** Mimiron cast Summon Flames (Initial) 64563 at 7.1 s and every 30 s
after, an event scheduled only with `_hardmode`, which only the button's `SetData(0, 7)` sets; phase 1
opened at 13.2 s (the hard-mode intro); Flames 64566 ticked from 15 s (348k in phase 1); VX-001 cast
Emergency Mode 64582 at phase 2 start and every phase 3 bot cast it; the clock is the 10-minute one (uld2
and uld3 cast Berserk 64238 at 10:00.1-10:00.3), and uld1's parts went down at 9:51.9, before it.

**Stop rules:** one kill of three, 9 deaths in it: not polished. No failure picked three times.

**Verdict on round 4 (core fetch at or under 25%): held.** `no-corpse` 0 in all three (window 9: uld3 3,
uld2 1); every corpse at or under 25% was looted and used (uld1 6:32 → 6:34, uld2 7:59 → 8:05, uld3 7:55 →
8:01). Airborne at or under 25%: 15.5, 18.5 and 54 s (window 9: uld2 68, uld3 211 s). All three reached phase 4
(window 9: one); phase 3 ran 143, 208 and 237 s in uld1-3 (window 9: 244+, 196, 375). Above 25% as before: landings
53-59 s apart, the core held 3-22 s for `hold-bot-up`.

**What still wipes is the clock, and phase 3 sets it.** With phase 4 at ~150 s (uld1: 153 s), the
10-minute Berserk leaves phase 3 ~157 s; uld1 used 143 and killed 8 s short of it, uld2 (208 s, plus a
115 s phase 1 against 85-86) reached phase 4 with 66 s left, uld3 (237 s) with 71. A landing comes every
~54 s (the core goes down as the next Assault Bot spawns, and each landing delays the add timers 25 s),
so phase 3 is landings × 54 s, and the count is set by what a landing takes: uld1 35.2/34.7/6.8 points,
uld2 15.3/24.8/21.8/10.7, uld3 27.0/25.6/21.9/6.0. Airborne the unit takes almost nothing (0.12-0.24%/s).

- uld3's landings lost their ranged: alive at its four landings 7.7, 5.0, 3.1, 1.8 (ranged on the floor
  2.89M against 3.87M in uld1 and 4.70M in uld2). 15 of its 17 phase 3 deaths were Water Spray (8) and Bomb
  Bots (7), 11 deaths in the first 85 s. Its phase 4 attrition (12) is that raid, two healers short,
  in a 71 s phase 4 Berserk ended anyway.
- uld2's first landing took 15.3 points: its six melee moved 72% of it, 69 `mimiron fire bot action` moves
  against 86 `reach melee` (spray lanes under the landed unit), 0.47M against 1.67M from uld1's eight.

**Failure picked: bots stand in a Water Spray line their escape cannot leave.** 11 deaths, 8 in the pull
that lost its phase 3; the same lanes drive the uld2 churn above.

1. **The escape dead-ends where lanes cross.** 14 of 26 hits came within 5 s of an `unsafe ... spray11`
   note from the victim, 6 of the 11 deaths (uld3 Uldzao and Uldzat 4:50.7, Uldzad 4:56.4, Uldzae 7:22.7;
   uld2 Uldyah 6:41.9, Uldyat 7:16.4): the fan refused all eleven bearings for spray, then the unfiltered
   fallback found its straight sidestep in a lane too, and the bot stood. `MimironFireBotAction::Execute`
   (`UldActions_Mimiron.cpp:910-917`) stepped to 5.0 yd off the first lane, near side only, and `FleeFan`
   refused any spot within 4.5 yd of any lane or, for casters and healers in 25-man, 17 yd of any fire bot
   (`:242`, `IsMimironSpotFireBotSafe`). Fire bots arrive three at a time and walk to nearby flames, each
   with a live and a predicted lane: at 4:50.2 in uld3 melee Uldzao stood in three lanes of two fire bots
   9.3 and 12.3 yd off, and every spot 4 yd around it lay in one to four. Replay (lanes rebuilt from the
   snapshot before each of the 677 `spray` notes, 18 yd lanes): where the bot stood in a lane, near side
   then far side without the siren screen cleared every `unsafe` note in uld2 and uld3 and three of four
   in uld1. Each half alone falls short (uld3, 16 yd lanes): the siren screen dropped, 1 of 20 `unsafe`
   notes; the far side with the screen kept, 18 of 20. The screen alone blocked most casters' `fallback`
   notes (uld3 21 of 24).
2. **The lane was short.** Water Spray is radius 15 (`EffectRadiusIndex` 18) and the cone test adds the
   victim's 1.5 combat reach, 16.5 yd; hits measured from where the fire bot stood after spraying reach 16.3
   yd, against `ULDUAR_MIMIRON_FIREBOT_SPRAY_LENGTH` 16. uld3 Uldzad (4:45.2, 16.3 yd) drew no `spray` note.

The other four deaths were bots already moving under the lock (`spray locked`: uld1 Uldxau, uld3 Uldzal,
and uld3 Uldzad and Uldzaq at 5:18, revived seconds before).

**Classification:** bug. The strategy means the spray escape to leave the line (the sidestep to 5.0, the
ground-fire screen already dropped for it); it stood in it.

**Fix** (Mimiron files only, every size and mode, with or without a player):

- `MimironFireBotAction`: the near side first, then across the lane to the far side (5.0 + |side| yd),
  the unfiltered fallback only after the far side.
- The spray escape's fans skip the siren screen (`sirenScreen` on `MoveTowardClearOfMines`/`FleeFan`, on
  for every other caller): a silence costs a cast, the spray kills.
- `ULDUAR_MIMIRON_FIREBOT_SPRAY_LENGTH` 16 → 18, everywhere a lane is tested (escape, trigger, formation
  spots, the reader's lane test).

**Also fixed (round 4's review): a real player carrying a core.** `GetMimironCoreCarrier` picked only bots,
so a player looting and using cores raced the bot carrier for corpses. Now a living real player on the map
who holds a core, or has held one since the last handover (`MimironFightState::playerCoreCarrier`, reset
with the fight state at each handover and wipe), is the carrier, and every bot's core trigger stands down,
one holding a banked core too; with no such player the bot pick is unchanged. Bot-only pulls cannot show it.

**Next window should show** (`mimiron.py --p3`; `spray.py`, `sim.py`, `p3time.py`, `landings.py` in
`mim5/`):

- Water Spray deaths well under 11 (uld3 8); no hit within 5 s of the victim's `spray unsafe` note (14 of
  26 this window); `spray none` followed by a far-side `ok` where lanes cross.
- No hit at 16-18 yd from a bot with no `spray` note.
- Phase 3 ranged alive at the landings near full (uld3 7.7 → 1.8), and fewer landings needed.
- Deafening Siren on phase 3 casters and healers not much up (window 10: ranged 76/118/102, healers
  10/40/46 applications), since the escape may now land within 17 yd of a fire bot.
- No new A-B-A from the far-side crossing: phase 3 `fire bot <-> reach melee` 148/230/163, `arc spread <->
  fire bot` 39/127/106, `fire bot <-> reach spell` 63/77/63 this window.
- Carrier: bot-only pulls unchanged (`mimiron.carrier` Uldxay, Uldyay, Uldzay). A player-led pull (needs
  the user): a player who loots a core shows as `mimiron.carrier`, and no bot writes `corestep` while he
  lives.

**Not picked.**

- Bomb Bots (7, all uld3): nine bombs, 7-27 casts each, four exploded at 25-59%; round 4's candidate (the
  "on attacker" DoTs while a Bomb Bot is up) stands.
- The spray step-out against `reach melee` under a landed unit (uld2's first landing above): a predicted
  lane counts from the moment a fire bot picks its flame, however far it still has to walk. Timing it is
  a change to the lane model; next if the churn outlives this fix.
- uld2's 115 s phase 1 (30 s of the clock), not traced. The fire bot pile reached 6.0 average and 11 at
  most in uld3, near the findings doc's cull line (nine to fifteen); a plan question if spray deaths
  persist after this fix.
- Flames 5, phase 2 4, uld1's phase 4 4.

**Questions for the user:** none.

## Round 6 (build a55056c40, round 5 built)

**Data:** `raidtest.py report --boss mimiron --raid uld --head a55056c40 --pulls 10`: one valid pull per
pool, no DC veto inside any. Scripts in the lane scratchpad (`mim6/`: `p1.py`, `p1melee.py`,
`melee_ev.py`, `pos.py`).

| Pool | Run | Trace | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261122-212514-14` | `603_14_…_1795382759` | wipe 10:18 in P4 at Berserk | 33: P1 Flames 2; P3 Bomb Bot 1, Plasma Ball 1, Water Spray 1; P4 Plasma Ball 2, Hand Pulse 2; Berserk 24 |
| uld2 | `tr-20261122-212635-15` | `603_15_…_1795382838` | wipe 10:19 in P4 at Berserk | 30: H1 Flames 2 (one bot twice); P2 Rocket Strike 1; P3 Bomb Bot 1; P4 Plasma Ball 4, Flames 2, mine 1; Berserk 19 |
| uld3 | `tr-20261122-212434-13` | `603_13_…_1795382716` | wipe 10:29 in P4 at Berserk | 32: P2 Rapid Burst 2; H3 Bomb Bot 1; P4 Hand Pulse 4 (melee), Plasma Ball 1, revived bot 2; Berserk 22 |

95 deaths: Berserk 65, phase 4 before it 18, everything else 12. Water Spray 1.

**Stop rules:** no kill: not polished. No failure picked three times (phase 1 is new).

**Verdict on round 5 (spray crossing, 18 yd lanes): held.** Water Spray hits on the roster 5 (window 10:
26), deaths 1 (11), `spray unsafe` notes 0 (20), hits within 5 s of one 0 (14). Escapes: `ok` 170/103/67,
`none` 18/17/10, `locked` 95/63/42. Phase 3 ran 151, 151 and 130 s (window 10: 143, 208, 237) with 3, 1
and 0 deaths (3, 2, 17). The carrier change had no player-led pull to show it.

**The clock, phase by phase** (Berserk 64238 at 10:00; the transitions Z1, Z2, H1-H3 cost ~117 s
fixed):

| Pull | P1 | P2 | P3 | P4 reached | P4 before Berserk |
|---|---|---|---|---|---|
| window 10 uld1 kill | 86.0 | 92.0 | 143.2 | 7:18.4 | needed 153.4 |
| uld1 | **115.2** | 91.3 | 151.3 | 7:55.6 | 124 |
| uld2 | **136.8** | 90.4 | 150.7 | 8:15.3 | 105 |
| uld3 | **127.4** | 86.2 | 129.6 | 7:41.7 | 138 |

Phase 3 now fits; phase 1 lost the clock. With the kill's 86 s phase 1, phase 4 would have had 153, 156
and 179 s. Phase 1 across the last 14 traces is bimodal: 84-98 s or 112-154 s.

**Failure picked: after a Shock Blast escape, melee stand out of sight with no target for the rest of
phase 1.** Raid damage on the MK II runs the same to ~1:04 in fast and slow pulls (42-44% left); then
the slow ones lose half their rate. Melee dealt 3.45/3.54/3.59M in phase 1 against 5.43/5.67/5.75M in
three fast pulls; their aim at the MK II was 48/53/40% against 96-100%. Per 10 s (`p1melee.py`), melee
in uld1 had no target from 1:10 to 1:59 at 24-28 yd, uld3 none from 1:10 to phase end (2:20), uld2 77%
none from 1:10; round 5's slow uld2 (`603_14_…_1795373390`, 115 s) shows the same.

- uld1 1:04.322: nine melee run the Shock Blast escape (`MimironShockBlastAction`,
  `UldActions_Mimiron.cpp:338`) west from the MK II at (2686.4, 2569.7) to x 2671.8-2675.3, Z 365.4-365.9,
  the raised alcove floor; at 1:04.429 every one fires `drop target` (`invalid target`, which needs
  `AttackersValue::IsValidTarget`: possible target and `IsWithinLOSInMap`, `AttackersValue.cpp:249`).
- 1:10-1:50: `mimiron set dps priority action` FAILED 31 times; `mimiron.dpsrule` stays `mkii`, so
  `ResolveTarget` picks the MK II and `Attack` refuses it at its sight check (`AttackAction.cpp:149`).
  `mimiron target guard` vetoes `dps assist`; `reach melee` and `mimiron approach target action` need a
  current target. The bots fall to non-combat actions (`loot roll`, `suggest dungeon`) and stand.
- The MK II moved to (2698.5, 2560.6) at ~1:14, 22-35 yd from them, and nothing brought them back.

navprobe was not run (no Docker in this lane); sight is read from the server's own verdict above.

**Classification:** bug. The strategy means melee to hit the MK II after every Shock Blast; they stood.

**Fix** (Mimiron files only, every size and mode, with or without a player): `MimironSetDpsPriorityAction`
walks a melee bot that cannot see its resolved target to a standable spot in melee range on its side,
over a mine-safe leg (`GetMimironSightApproach`, the bearing fan `GetMimironTargetApproach` already
uses, without its fire-only gate), noted `mimiron.close sight`. Shock Blast (`ACTION_RAID + 5.5`) still
holds the tick through the cast, and the spot is refused inside the circle.
Review: the walk sat outside the guards that hold `mimiron approach target action` (uld1's alcove melee
dodged flames at 1:17 and 1:19, so it could walk them back through the fire), so it now yields to a
formation slot, a laser barrage and the frost bomb, rocket and fire holds; their predicates moved to
`UldEncounter_Mimiron`.

**Next window should show:**

- Phase 1 at 84-98 s in every pull (this window 115/137/127).
- Melee aim at the MK II in phase 1 near 100% (`p1.py`; 48/53/40%) and melee phase 1 damage near 5.5M.
- `mimiron.close sight` notes after Shock Blast casts, followed by `mkii` attacks within a few seconds,
  and no melee run of non-combat actions in phase 1.
- Phase 4 reached by ~7:20 with ~150 s before Berserk.

**Not picked.**

- Phase 4 Plasma Ball and Hand Pulse attrition before Berserk (18): it is ~150 s long and the kill
  only needs the clock.
- Bomb Bots 3, Flames 6.

**Questions for the user:** none.

## Round 7 (build 1fa3c332c, round 6 and the shared PickDefensive fix built)

**Data:** window 14, `raidtest.py report --boss mimiron --raid uld --head 1fa3c332c --pulls 10`: one valid
pull per pool, no DC veto inside any. Scripts in the lane scratchpad (`mim7/`: `regen.py`, `flat.py`,
`leash.py`, `track.py`, `shockesc.py`, `holder.py`, `landtgt.py`; round 6's `mim6/p1.py`, `p1melee.py`).

| Pool | Run | Trace | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261123-103146-19` | `603_19_…_1795429957` | **reset** 3:23.8 in P1, MK II at 31.5% | 4: Napalm Shell 1 (ranged 1:45); Flames 2 (off-tank 2:41, rezzed, 2:43); Proximity Mine 1 (melee 3:01) |
| uld2 | `tr-20261123-103207-21` | `603_21_…_1795429968` | wipe 10:39 at Berserk, P4 opened 10:28.7 | 31: P1 mines 2 (melee 0:45), Flames 1, Plasma Blast on the main tank 1 (1:15); P2 Heat Wave 3 and Rapid Burst 3 (ranged 3:08-3:53, melee 4:50-4:52), Frost Bomb 1 (melee 4:45); P3 Bomb Bot 1; Berserk 19 |
| uld3 | `tr-20261123-103146-20` | `603_20_…_1795429958` | wipe 10:29 at Berserk | 31: P2 Heat Wave 1, Rapid Burst 4 (3:35-3:57), Flames 2 (4:03, 4:10); P3 Bomb Bot 2; P4 Shock Blast 1 (9:21); Berserk 21 (from 10:03) |

66 deaths: Berserk 40, P2 Heat Wave/Rapid Burst 11, Flames 5, Bomb Bot 3, mines 3, Plasma Blast, Napalm
Shell, Frost Bomb and Shock Blast 1 each. Water Spray hit 5 and 4 times, no death.

**Stop rules:** no kill: not polished. No failure picked three times.

**uld1's reset was the fight, not the harness.** RaidObs closes `reset` when the boss state goes back to
NOT_STARTED, which only Mimiron's evade does (`BossAI::_Reset`); 24 of 25 were alive and out of combat at
teardown, DC logged no stall (`stallAtEnd` empty) and files every pull that ends without a kill as a wipe.
At 3:18.9 both tanks' Shock Blast escapes ran west up the corridor past the closed door ((2662.4, 2558.6, 368.7)
and (2650.6, 2577.3, 372.4), the main tank 80.2 yd from Mimiron's spawn (2742.53, 2560.99)), the MK II
chased the main tank into the doorway after its cast, and the encounter reset 4.9 s after the escape. The
one evade in the script is Mimiron's 80 yd from his spawn (`boss_mimiron::UpdateAI`); the last sample,
220 ms before the close, had him at 74.9 yd and closing, farther than in any of the other 19 Mimiron
traces (56.8-66.6). The core never evades a raid creature on an unreachable target (below), so the leash
is the only fit, but the crossing itself is not sampled.

**Verdict on round 6 (melee walk back into sight): held.** Melee aim at the MK II in phase 1 97% and 85%
in uld2 and uld3 (window 13: 48/53/40%), melee phase 1 damage 5.28M and 5.25M (3.45-3.59M),
`mimiron.close sight` 4, 10 and 7 a pull. Phase 1 93.5 s (uld3) and 104.2 s (uld2) against 115-137 s;
uld1's 190 s is the bug below. Left: uld3 melee had no target 51-67% of 1:00-1:20, after the second Shock
Blast. Phase 4 still came late, now from phase 3.

**The clock** (Berserk at 10:00; window 10's kill: P1 86, P2 92, P3 143, P4 at 7:18):

| Pull | P1 | Z2+H1 | P2 | H2 | P3 | H3 | P4 reached | before Berserk |
|---|---|---|---|---|---|---|---|---|
| uld1 | 190.3, reset | | | | | | | |
| uld2 | 104.2 | 48.3 | **128.9** | 24.3 | **277.6** | 32.1 | 10:28.7 | -29 |
| uld3 | 93.5 | 48.0 | 97.7 | 24.1 | **198.5** | 31.9 | 8:27.2 | 93 |

uld2 entered phase 3 with 6 ranged and 5 melee (11 dead in P1 and P2) and needed five landings of 9-19
points. uld3's two first landings took 30 and 35 points; at 13.7% in the third the 15% fire bot sweep
took the ranged (5 of 5 on fire bots from 7:09.8, ACU 20.5 → 7.8), and the last 6.5 points waited 51 s
for a fourth landing. uld2's fourth landing lost its last ~2 s the same way at 14.3%.

**Failure picked: the MK II flags its tank unreachable, so it is immune and heals.** Berserk is the
biggest bucket and uld1 never got past phase 1. Its MK II went 69.4 → 100% at 0:47.6 and 57.6 → 90.8 →
100% at 1:53.6-1:55.5 (`--unreach`), +33.3 points per step, and its health stood still under the raid's
fire 0:40.6-1:04 and 1:46.6-2:11.

- Core (`Creature.cpp`, `Combat/CombatManager`, `UnitAI::EvadeTimerExpired`): a failed chase path sets
  `SetCannotReachTarget(victim)`; `IsEvadingAttacks` then evades every hit (`SPELL_MISS_EVADE`,
  `MELEE_HIT_EVADE`), and 5 s into the 10 s timer `NpcRegenHPIfTargetIsUnreachable` (1, no override)
  heals max/3 each 2 s regen tick; in a raid the timer restarts instead of evading. It clears only on a
  chase spline that dispatches, a chase that arrives, or the creature stopping to cast: both spans ended
  at the MK II's next Shock Blast (1:04.3, 2:11.6).
- Both opened as the main tank walked back to his spot (2691.6, 2568.5) through the MK II after a west
  escape into the door box ((2678.1, 2572.1) at 0:34.5, (2680.5, 2558.1) at 1:41.7): at 0:40.6 he stood
  0.4 yd from it, at 1:46.6 3 yd. Back on the spot within its reach and sight, the chase had nothing to
  re-path, so the flag held.
- That west escape is every pull's: the holder's first Shock Blast escape goes west (straight away from
  an MK II east of him) in all 20 Mimiron traces, r 61-68 yd from the room centre; the flag followed in
  2 of uld1's 4, none elsewhere. No other trace shows a rise or a frozen MK II.

**Classification:** bug. The strategy holds the MK II on the tank spot for the raid to kill; its own escape
geometry hands the MK II an unreachable tank, and once a leash-crossing reset.

**Fix** (Mimiron files only, every size and mode, with or without a player):

- **Holder screen.** In phase 1 (`MimironPhase1Active`) every flee fan refuses the MK II's victim a spot
  farther from the room centre than the tank spot + `ULDUAR_MIMIRON_HOLDER_RADIUS_SLACK` (7: 60.1 yd) or
  than it stands (`GetMimironHolderRadius`, `IsMimironSpotHolderSafe`; the unfiltered fallback too),
  counted as `hold` in `mimiron.flee`. 7 keeps the shock escape's ±90° bearings (56 yd); replayed on the
  20 traces it refuses 1-7 holder legs a pull (uld1 13), every one into the alcove. It also keeps Mimiron
  within ~69 yd of his spawn.
- **Step out.** `mimiron unreachable trigger` / `mimiron unreachable step action` (`ACTION_RAID + 2`):
  while the MK II's flag names the bot (`GetMimironUnreachableMech`, the AQ40 Twin Emperors precedent), it
  walks to both reaches + `ULDUAR_MIMIRON_UNREACH_STEP_MARGIN` (4) off the MK II toward the room centre
  (toward the bot if the MK II stands within that of the centre), fire screened then not (`unreach`, `unreach
  fire`), so the chase re-paths; the formation walks it back. Only the flagged bot moves; a real player
  flagged is left alone.
- **Probe and reader.** `mimiron.unreach` (whom the flag names, per instance); `mimiron.py --unreach` lists
  its spans with the MK II's health and every rise over 3 points off the Self Repair floor, with tests.
  Findings doc: "An MK II that cannot reach its tank is immune and heals".

navprobe not run (no Docker in this lane). The orchestrator may check the ±90-112° escape points from the
spot, e.g. (2691.6, 2549.4), (2691.6, 2587.6), (2699.8, 2548.6), (2699.8, 2588.4): if the north and south
ones are off mesh, the fan falls to the next bearing, then `shock fire`, then `shock anchor`.

**Next window should show:**

- No MK II health rise in phase 1 (`--unreach`; window 14: 3 in uld1), any `mimiron.unreach` span on the
  holder ≤ ~3 s with `unreach ok` steps, and no `reset`.
- The holder's Shock Blast escapes ≤ 60.1 yd from the room centre, north or south (`hold` counts in
  `shock` notes; `holder.py` 0 legs past it), and `shock anchor` rare.
- Mimiron ≤ ~70 yd from his spawn (`leash.py`; window 14 max 74.9).
- Phase 1 84-104 s in every pull; melee sight losses after the first Shock Blast no more than now.

**Not picked.**

- The 15% fire bot sweep firing mid-landing (Q3), a target order change.
- Phase 2 Heat Wave and Rapid Burst (11, both full pulls; uld2's P2 129 s): next round's candidate.
- Flames 5, Bomb Bots 3, mines 3.

**Questions for the user.**

- **Q3 (plan change): hold the 15% fire bot sweep while the ACU is on the floor.** At 15% every kept fire
  bot joins the ranged kill list above the ACU; when that line falls inside a landing the ranged leave the
  one window the ACU takes damage (grounded 0.87-1.30%/s, airborne 0.11-0.12%/s). uld3 lost its third
  landing to it and waited 51 s for a fourth; uld2 lost the end of its fourth. Recommendation: yes, sweep only
  while the ACU is airborne (or once it lifts), keeping "killed before phase 4".

**Review.**

- uld1's reset is not the leash: Mimiron was 74.8 yd from his spawn (2742.53, 2560.99, 364.40) at
  3:23.552 and gaining ~2 yd/s, the MK II still chasing the tank, so 80 yd was out of reach by the
  3:23.772 close. Nor is the leash the only evade: the MK II's own `EnterEvadeMode` takes Mimiron with
  it, and the core calls it from `Creature::SelectVictim` when it finds no acceptable victim. Cause
  open; the holder screen still keeps the holder out of the corridor it happened in.
- The first west escape holds in 19 of 20 traces; `603_14_…_1795365875` (normal mode) tanks at the centre.
- One other rise exists: `603_15_…_1795373410`, P4 45.3 → 78.6 at 10:01.6, the wipe. The flag reaches
  phase 4, where the step action also runs.
- The flag's two spans ran 24-25 s each (0:40.6-1:04.3, 1:46.6-2:11.6).

**User answer (2026-11-23), Q3:** why does the sweep exist at all? Emergency Fire Bots put out fire
patches, so killing them earlier only lets the fire build faster. The sweep's only job is to keep fire
bots out of phase 4, and that was never measured there. Approved: drop the 15% sweep
(`ULDUAR_MIMIRON_FIREBOT_CLEANUP_PCT` and what hangs off it) for one window and measure phase 4 fire
against the spray.

## Round 8 (build 523b2f60f, round 7 built)

**Data:** window 15, `raidtest.py report --boss mimiron --raid uld --head 523b2f60f --pulls 10`: one valid
pull per pool, no DC veto inside any. Baselines for the new view: windows 10, 13 and 14 traces. Scripts in
the lane scratchpad (`mim8/`: `late.py`, `p2deaths.py`, `stuck.py`, `legs.py`, `movers.py`, `where.py`,
`botlog.py`, `burstcone.py`; round 7's `mim7/leash.py`, `holder.py`; round 6's `mim6/p1.py`).

| Pool | Run | Trace | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261123-124209-14` | `603_14_…_1795437770` | **kill** 9:39, parts down 9:22.4, hard mode | 10: P3 Bomb Bot 1, Flames 1 (the same ranged, rezzed); P4 Hand Pulse 5, Proximity Mine 3 |
| uld2 | `tr-20261123-124048-13` | `603_13_…_1795437690` | wipe 10:32 in P3 at Berserk | 31: P1 Flames 1, Shock Blast 1 (a ranged); P2 9 (Heat Wave 3, Flames 3, Rapid Burst 3), 7 of them bots that never walked in (below); P3 Bomb Bot 4, Water Spray 2 (one bot twice), Assault Bot 1; Berserk 13 |
| uld3 | `tr-20261123-124350-15` | `603_15_…_1795437871` | wipe 10:27 in P4 at Berserk | 32: P3 Water Spray 3, Bomb Bot 2, ACU Plasma Ball 1; P4 Flames 2 (one melee twice), mine 1, Hand Pulse 1; Berserk 22 |

73 deaths: Berserk 35, P2 9, Bomb Bot 7, Hand Pulse 6, Water Spray 5, mines 4, Flames 4 outside P2, Shock Blast,
Assault Bot and ACU Plasma Ball 1 each. The uld1 kill was hard mode: Summon Flames Initial at 7.1 s,
Flames from 14 s, Emergency Mode at phase 2, no Berserk cast.

**Stop rules:** one kill of three (10 deaths): not polished. No failure picked three times.

**Verdict on round 7 (holder screen, unreachable step): held.** No MK II health rise in phase 1 and no
`mimiron.unreach` span at all (window 14: three rises, 24-25 s spans), no reset. Holder Shock Blast legs
2/4/2, none past 60.1 yd (`holder.py`); `shock` notes with `hold` refusals 2/3/2, every escape `ok` but one
`shock fire ok` and one `none` (uld2), `shock anchor` only ever `locked`. Mimiron at most 61.0-65.1 yd from his spawn (window 14:
74.9), and that only as the beaten MK II parks at (2795, 2598). Phase 1 84.9, 89.4 and 85.9 s (window 14:
93.5-104.2, uld1's 190 s reset). Melee aimed at the MK II 100% of phase 1 in all three, 5.35-5.92M melee
damage, no `close sight` walk needed.

**The clock** (Berserk at 10:00):

| Pull | P1 | Z2+H1 | P2 | H2 | P3 | H3 | P4 reached | before Berserk |
|---|---|---|---|---|---|---|---|---|
| uld1 | 84.9 | 48.1 | 81.5 | 24.4 | 142.9 | 32.2 | 7:07.3 | 173, kill at 9:22 (P4 135 s) |
| uld2 | 89.4 | 48.3 | **136.4** | 24.3 | **320.6**, unfinished | | | -, Berserk in P3 |
| uld3 | 85.9 | 48.0 | 80.5 | 24.4 | **222.4** | 32.1 | 8:26.4 | 94 |

uld1's landings took 36.5 and 39.7 points, uld3's 23.1/17.8/19.5/16.1, uld2's 15.1/17.4/6.8 with Assault Bots
alive 98 s median (uld1 39 s) after its phase 2 losses.

**Built first: the user's Q3 answer.** Fire bots stay kept through phase 3, the phase 4 handover and phase 4:

- `GetMimironKeptFireBots` keeps every live fire bot while the hard-mode encounter is in progress near the
  room (no ACU, phase 4 or 15% test); `ULDUAR_MIMIRON_FIREBOT_CLEANUP_PCT` removed.
- `MimironSetDpsPriorityAction::BuildPriorityList` has no fire bot entry left (the sweep, the melee cull
  behind the Assault Bot, the ranged "loose one" behind the mech), and `MimironSetDpsPriorityTrigger` no
  longer fires in the phase 4 handover for a fire bot in the room, which was the handover cull (window 15:
  237 and 49 bot-s aimed at fire bots in H3). The spray and siren dodge, the target exclusions and the
  splash guard stay and now reach phase 4 too.
- Reader `mimiron.py --firebots`, from phase 3 on, per phase: fire bots alive at open and close, mean, max,
  gone; Flames, Water Spray and Deafening Siren (applications and raid-seconds silenced) on the raid; aim
  at fire bots; splash guard veto rows; spray or siren steps.

Baseline (fire bots never reached phase 4 in windows 10-15): P4 Flames 0.3-2.9k/s (w15 2.8 and 2.9, w14
1.4-1.7, w13 1.4-2.3, w10 0.3-1.5), P4 spray, siren and splash holds 0; P3 Flames 1.1-8.2k/s (w15 8.2/1.1/4.2), P3
Water Spray 2/7/9 hits in window 15.

**Failure picked: bots left in the phase 1 fire through the handover walk-in die in phase 2's opening.**
uld2's phase 2 opened with six deaths in 5 s (2:42.7-2:47.9, five bots, one twice) and lost the pull
with them: phase 2 ran 136 s, phase 3 321 s. All five, and a sixth that died at 4:40, had stood 51-55 yd
west of the room centre at phase 2's start, round (2691, 2575) in the phase 1 field, while 16 of 24 were
on their slots (`stuck.py`).

- Uldyap: hold slot substituted to its own spot at 1:56 (`substitute r12`), `approach none` at 2:13.6, then
  its walk-in slot `hmwedge 5/13` (2758, 2581), 65 yd east, at 2:17.84, and no `mimiron arc spread action`
  move, accepted or refused, until a Frost Bomb flee at 2:38.7 (`botlog.py`). The phase 1 field lay
  across every leg: straight 12.1 yd of burn in two overlapping nodes, the best one-waypoint detour 9.9 yd
  (`legs.py`, on the snapshot's 40-node cap, so if anything more).
- Cause: `GetMimironSlotApproaches` (`UldEncounter_Mimiron.cpp:713` at `5c73b742d`) offers a straight walk,
  a `cross` of at most 4 yd and one burn (`:838`) or a detour whose two legs miss all fire (`:847-859`);
  else nothing, and `MimironArcSpreadTrigger` stands down on an empty answer
  (`UldTriggers_Mimiron.cpp:125-127`). The hold was written for a bot near its slot; 40-70 yd out, a walk-in
  across the phase 1 field rarely has a clean leg.
- Healers then chased them: from 2:31 to 2:43 all four swung between `reach party member to heal` (targets
  50+ yd off) and their wedge slots, 165 and 167 refused moves, healing 28-42k a 2.5 s bucket.
- Every window shows it: 1-8 bots 30-57 yd out at phase 2 in all eight pulls of windows 13-15, and 16 of
  the 26 phase 2 deaths there were theirs (w15 uld2 7 of 9, w14 uld3 4 of 7, w14 uld2 3 of 7, w13 uld3 2
  of 2).

**Classification:** bug. The handover's walk-in (Q1) means every bot on its phase 2 slot at phase 2; these
never start walking.

**Fix** (Mimiron files only, every size and mode, with or without a player): `GetMimironSlotApproaches`, when
nothing clean is left, the goal is over `ULDUAR_MIMIRON_SLOT_STRANDED_DISTANCE` (30) away and the bot is at
60% or more, offers the least-burn of the straight leg and the standable detour waypoints, mines still
refused (both legs mine-safe), noted `stranded N` (yd of burn). Near its slot a bot keeps holding. A
walking bot keeps the 60% fire gate, so the crossing isn't thrown back by its first tick.

**Also fixed, small and independent: the Rapid Burst screen saw no cone.** Since core `29aadbcb2`
(2026-09-21) VX-001 parks a Burst Target (34211) where a random player stands and casts 63382 on it
(`boss_mimiron.cpp:1422-1427`, `:1533-1553`); `ReadMimironRapidBurstWindow` looked for a raid member
carrying 63382 (`UldEncounter_Mimiron.cpp:1712` at `5c73b742d`), and no roster aura row for 63382 exists in
any window 13-15 trace. So the fan, slot approaches, target approach and arc spread never refused a
live cone: 33 accepted legs ended in one in window 15 (arc spread 20, flames dodge 13) and 171 ticks,
306k, hit bots that walked in after the cast (10.5% of the window's Rapid Burst damage; uld3 205k). Now the
cone runs from VX-001 to the Burst Target holding VX-001's 63382, else VX-001's own aura and facing. The
reader's `--burst` took the cast target as carrier, so it kept 8 of window 14 uld2's 25 casts and called
every carrier ranged; it now names the roster member nearest the Burst Target at the cast (16 of uld2's
casts in window 15 then had the target sampled).

**Next window should show:**

- Fire bots (`--firebots`): alive at P4's open and through it (0 in every pull so far); P4 Flames per second
  against 0.3-2.9k/s; P4 Water Spray hits and deaths, siren raid-seconds and splash guard holds, all 0 so
  far; P3 the same against window 15; no raid aim at fire bots in H3 or P4. Watch phase 4's length and
  part damage: the splash guard now holds AoE on the stack while a fire bot is within 12 yd of it.
- Stranded: every living bot within ~30 yd of the centre at phase 2's start (`stuck.py`; 1-8 out in
  windows 13-15), `stranded N` notes in Late and WalkIn, Flames in the handover above window 15's 8-31k but
  no handover death, no early phase 2 deaths among the walk-in's stragglers, and no healer `reach party
  member to heal` A-B-A at phase 2's start.
- Rapid Burst: `burst` refusals in `flee` notes, walked-in hits (`burstcone.py`) well under window 15's
  171 ticks, and `--burst` covering most casts.
- Round 7's checks stay: no MK II rise, no reset, phase 1 84-104 s.

**Not picked.**

- Bomb Bots (7) and Hand Pulse (6, five in uld1's phase 4 kill).
- Water Spray (5, in uld2 and uld3) with the sweep still on; the next window reads it with fire bots kept.
- uld3's phase 3 (222 s): landings of 16-23 points against uld1's 37-40, the clock's remaining lever.
- Healers leaving the wedge to reach a far bot (`reach party member to heal` against `mimiron arc spread
  action`): only with stranded bots here; if it outlives the fix, a Mimiron hold on the reach while on a
  slot.

**Questions for the user:** none. Q3's measurement is the next window's.

**Review.**

- `stranded` also ran in phase 1: the phase 1 test in `GetMimironSlotApproaches` guards only the
  substitute ring, against the findings doc's "phase 1 keeps holding". Window 15 had ranged over 30 yd off
  their `p1stack` slot with fire down for 174-511 samples a pull (~240 ms each). Now skipped while
  `MimironPhase1Active`.
- Its 60% gate was looser than the 75% a 4 yd `cross` needs: one Flames tick (3.6-4.1k in uld2, 15-19% of a
  caster) took a 60-75% walker under the walking dodge's 60%, which threw it off the leg. Now the crossing's
  75%; uld2's six stragglers stood at 100% at the walk in.
- It still runs in Hold, phase 2, phase 3 and phase 4, not only in Late and WalkIn. Window 15 had bots over
  30 yd off their slot with fire down for 315-1084 `p3wedge` and 601-928 phase 4 `hmwedge` samples a pull.
  Next window, add: `stranded` notes by phase, and Flames and deaths on a bot within 5 s of its note.
- The Rapid Burst read is skipped while VX-001 rides the chassis: phase 4 casts Hand Pulse, and the read is
  a 100 yd grid search per bot pass.
- Left: a `stranded` waypoint can end within 30 yd of the slot with a burning second leg, and the bot then
  holds on the clean waypoint. On uld2's six stragglers every least-burn waypoint's second leg was clean.
- Checked: uld2's stragglers (six bots 51-54 yd out at phase 2, five dead 2:42.7-2:47.9, Uldyap's notes as
  cited, no arc spread act between its slot and phase 2, so an empty answer by elimination); Rapid Burst
  casts target Burst Targets and no roster aura row for 63382 exists in windows 13-15; 63382 puts effect 0
  on the target and effect 1 on VX-001 (Spell.dbc).

## Round 9 (build 2d7853f0e, round 8 built)

**Data:** window 16, `raidtest.py report --boss mimiron --raid uld --head 2d7853f0e --pulls 10`: one valid pull
per pool, no DC veto inside any. Baselines: windows 14 and 15 (`603_19/20/21_…_17954299xx`, `603_13/14/15_…_17954376xx`
/ `_1795437871`). Scripts in the lane scratchpad (`mim9/`: `p4.py`, `p4rate.py`, `p4spread.py`, `hand.py`,
`notes.py`, `bombs.py`, `bombaim.py`, `bombtrack.py`, `bomblegs.py`, `bomblegs2.py`, `collateral.py`,
`blastgeom.py`, `project.py`; round 8's `mim8/stuck.py`, `burstcone.py`, `botlog.py`).

| Pool | Run | Trace | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261123-135204-11` | `603_11_…_1795441975` | **kill**, parts down 9:52.4, hard mode | 10: P3 Water Spray 2 (melee, one spray); P4 Water Spray 2, ACU Plasma Ball 1, Shock Blast 1, Hand Pulse 4 (9:09-9:22) |
| uld2 | `tr-20261123-135204-12` | `603_12_…_1795441979` | wipe 8:56 in P3, raid dead | 31: P1 Flames 1 (hunter casting Revive Pet on the drop, 0:15.9), Shock Blast 1; Z2 Flames 1; P2 Laser Barrage 1, Rapid Burst 1, Flames 3 (melee after a Frost Bomb escape), Rocket Strike 1, Frost Bomb 1; P3 Bomb Bot 10, Assault Bot 6, Junk Bot 3, Water Spray 1, ACU Plasma Ball 1 |
| uld3 | `tr-20261123-135043-10` | `603_10_…_1795441886` | wipe 10:02 in P4, parts 28-33% | 31: P1 Flames 1 (after the MK II's opening swing, 14.7k on a ranged at 0:14); P2 Rapid Burst 4 (healers 3, one revived and dead again); P3 ACU Plasma Ball 1, Water Spray 1; P4 Hand Pulse 10 (six melee 7:41-7:44), ACU Plasma Ball 8 (ranged, 7:54-9:57), mines 2, MK II melee 1, Laser Barrage 1, Water Spray 1; Berserk 1 |

72 deaths: Hand Pulse 14, ACU Plasma Ball 11, Bomb Bot 10, Water Spray 7, Flames 6, Assault Bot 6, Rapid Burst 5,
Junk Bot 3, mines, Shock Blast and Laser Barrage 2 each, MK II melee, Rocket Strike, Frost Bomb and Berserk 1 each.
The uld1 kill was hard mode: Summon Flames Initial 7.1 s, Flames from 16 s, Emergency Mode at phase 2, no Berserk
cast (uld3's at 10:00.2).

**Stop rules:** one kill of three (10 deaths): not polished. No failure picked three times (Bomb Bots are new).

**The clock** (Berserk at 10:00):

| Pull | P1 | Z2+H1 | P2 | H2 | P3 | H3 | P4 reached | P4 |
|---|---|---|---|---|---|---|---|---|
| uld1 | 86.6 | 48.4 | 81.2 | 24.1 | 146.9 | 32.0 | 7:12.3 | 160.0 s, parts down 7.6 s before Berserk |
| uld2 | 100.2 | 48.2 | **126.3** | 24.1 | **224.4**, raid dead | | | |
| uld3 | 82.7 | 48.3 | 84.4 | 24.3 | 154.9 | 31.9 | 7:19.7 | 160 s left, raid dead by 10:00 |

uld2 entered phase 3 with 16 alive (6 ranged) and landed the unit once (5.5 points); uld3 entered phase 4 with 21 alive but two
healers, both wipes losing the raid in phase 2 first.

**Verdict on round 8.**

- **Fire bots kept (the user's experiment): helped in phase 3, hurt in phase 4.** `--firebots` against windows 14-15:

  | | P3 Flames k/s | P3 spray hits/dead | P3 siren raid-s | P4 fire bots open/mean | P4 Flames k/s | P4 spray hits/dead | P4 siren raid-s |
  |---|---|---|---|---|---|---|---|
  | w16 uld1 / uld2 / uld3 | 3.3 / 1.3 / 2.7 | 3/2, 4/1, 2/1 | 1017 / 456 / 746 | 4/2.6, -, 7/7.0 | 1.1, -, 0.3 | 9/2, -, 1/1 | 361, -, 199 |
  | w15 uld1 / uld2 / uld3 | 8.2 / 1.1 / 4.2 | 2/0, 7/2, 9/3 | 353 / 752 / 1190 | 0 | 2.8, -, 2.9 | 0 | 0 |
  | w14 uld2 / uld3 | 2.2 / 2.3 | 5/0, 4/0 | 791 / 410 | 0 | 1.4 (11 s), 1.7 | 0 | 0 |

  Aim at fire bots 0 bot-s in every phase (w14-15: P3 1-156, H3 49-237). Phase 3 stayed inside the earlier range on
  every count. Phase 4 Flames fell to 179k and 47k from 211-374k with no Flames death (w15 uld3: 2), but the kept
  bots sprayed 10 times for 3 deaths, silenced 560 raid-s, held splash 144 times, and ranged cast 39% and 36% of phase
  4 against 40-66% in nine earlier phase 4s (`p4rate.py`). The comparable pull, uld1 with 23+ alive in both
  windows, dealt 92.9k/s there against 108.7k/s and needed 160 s against 135 s for the same ~14.8M; it killed with
  7.6 s to spare. The fire they put out is healable, the time is not: plainly, keeping them into phase 4 hurt.
  Question Q4.
- **Stranded walk-in: held.** Bots over 30 yd out at phase 2: 1, 2, 0 (windows 13-15: 1-8), all three on a detour
  leg; on a phase 2 slot at phase 2 24/25, 20/23, 24/25 (w15 uld2 16/24); no phase 2 opening deaths; `stranded`
  notes in Z2 and H1 1/9, 4/20, 1/5, no death within 5 s of one. Handover Flames 35k/90k/9k (w15 8-31k), one
  handover death (uld2 Uldyat, Z2, 14 ticks after a fire dodge `arrived`, not stranded).
- **Rapid Burst read: held.** `burst` refusals in phase 2 `flee` notes 50/136/54 (none possible before), legs into a
  live cone 0/1/1 (w15: 10/4/19), walked-in ticks 91 for 158k (w15 171 for 306k), `--burst` 17/17, 17/17, 19/19
  casts with a carrier.
- **Round 7's checks:** no MK II rise, no `unreach` span, no reset, phase 1 82.7-100.2 s.

**Ranking.** Hand Pulse (14) and the ACU's Plasma Ball (11) are phase 4's, and both follow from raids reaching it
short: uld3 had two healers. Hand Pulse hit 22-33% of the raid a cast (`p4spread.py`), as in windows 10-15, the floor
the `p4melee` layout was measured to give, so not a bug. The ACU has no tank in phase 4 (`boss_mimiron.cpp`: `case
4` resets threat, then `DoSpellAttackIfReady(SPELL_PLASMA_BALL_P2)` on its victim), so its 65647 rides the top-threat
ranged: a plan change, Q5. Next, Bomb Bots (10): a bug.

**Failure picked: the Bomb Bot step-out runs 30 yd, along the chase.** `MimironBombBotAction` was the generic
`MoveAwayFromCreatureAction` (`UldActions_Mimiron.h:248` at `8bc2a82d3`; `MovementActions.cpp:3036`: eight compass
bearings, 3-30 yd, keep the spot farthest from the bomb, no hazard screen), fired by any bot within 8 yd of the bomb
(`UldTriggers_Mimiron.cpp:204`).

- Every leg is the full 30 yd: 628 of 628 in eight pulls of windows 14-16 (`bomblegs.py`), 52-157 a pull, 316 of them
  melee. 370 of 587 bystander legs began over 8 yd from the bot the bomb was chasing (`bomblegs2.py`), where the blast
  never reaches: it goes off on contact with that target, the bomb then 0.4-5.1 yd off it (median 2.5), hitting
  bystanders 0.4-5.7 yd from it (`blastgeom.py`, 60 blasts).
- Bots near the chase run its way. uld2 5:29-5:36 (`bombtrack.py`, `botlog.py`): the first bomb's target went Uldyac,
  Uldyap, Uldyaq in 3 s (threat); Uldyap (5:32.5), Uldyae (5:32.2) and Uldyaq itself (5:31.9, just before it was
  chased) all ran `mimiron bomb bot action` to (2779, 2548) within 2 yd, the bomb 2.2-3.4 yd behind the two bystanders
  all the way, and blew all three (Uldyae healer and Uldyap dead). In windows 14-16, 7 of 21 collateral hits came off
  this leg, 6 off the spray/siren fan, which had no blast screen either (`collateral.py`).
- The legs also cost the shooters: ranged on the bomb were moving 23-75% of each uld2 bomb's life (`bombaim.py`), the
  step a top mover, and uld2's bombs died once in 12 (uld3 4 of 6).

**Classification:** bug. The strategy means bystanders to step out of the blast ("everyone else inside the blast
steps out"); they sprint 30 yd from the bomb, mostly when out of it, and beside its victim when in it.

**Fix** (Mimiron files and reader only, every size and mode, with or without a player):

- `GetMimironMarkers` reads Bomb Bots with their chase target; `GetMimironBombBotBlast` gives the bot's blast: chased,
  or the nearest centre (the chase target, else the bomb) within a radius.
- `MimironBombBotTrigger`: a bystander within `ULDUAR_MIMIRON_BOMB_BOT_BLAST_CLEARANCE` (8) of a centre; a chased
  healer, melee or tank with the bomb within 8 yd as before; a chased ranged DPS never.
- `MimironBombBotAction` is a `MimironFleeAction`: a bystander steps through the bearing fan onto the clearance + 1
  (`bombbot`, unfiltered fallback), a chased bot kites 30 yd through it (`bombbot kite`, no fallback); both screened
  like every Mimiron flee.
- A blast centre fails `IsMimironSpotStandable` and the fan (new `bombbot` refusal count in `mimiron.flee`) for
  everyone but its victim; `mimiron bomb bot guard` holds `reach melee`, `reach spell`, heal reach, `set behind`,
  `follow` and the approach within the clearance + 4, as the rocket guard does.
- Reader `mimiron.py --bombbots` with tests: each bomb's life and end, chased and collateral victims (dead), step-out
  legs and median yards, `bombbot` outcomes, other dodges' blast refusals. Findings doc: "The blast goes off round the
  victim", the corrected Bomb Bot damage (63009 was the mine's), and the fire bot measurement.

Replayed on windows 14-16, 217 of the 587 bystander legs would fire, at 3.6 yd median (p90 7.2) instead of 30.

**Next window should show** (`--bombbots`, `--walk`, `--flips`):

- Bomb Bot step-outs a few yards (median well under 10) and fewer (~15-50 a pull against 52-157), `bombbot ok`
  notes, few `none`.
- Collateral blast victims near 0 (windows 14-16: 21 hits; w16 3 deaths), no bystander landing beside the chased bot.
- More Bomb Bots killed before contact (w16 1 of 12, 2 of 5, 4 of 6) and Bomb Bot deaths under 10; phase 3 moving
  shares down (w16 melee 46/51/45%, ranged 40/53/37%).
- No new A-B-A between `mimiron bomb bot action` and `reach melee` or `mimiron arc spread action`; `mimiron bomb bot
  guard` veto rows during bombs.

**Not picked.**

- uld2's hunter Uldyaj died casting Revive Pet on the drop in three nodes (0:15.9), as in window 7: no trigger moves a
  bot out of a cast it started there.
- uld3's three phase 2 healer deaths (Rapid Burst under Heat Wave at 56-60% raid health) and uld2's melee burning
  after the 3:29 Frost Bomb escape: phase 2 still cuts the raid both wipes lose later.
- Water Spray 7: mostly bots mid-move (`spray locked`) or walked in by other legs.
- The MK II's opening swing (uld3 0:14, 14.7k on a ranged on the drop), the findings doc's open item.
- A chased tank still kites 30 yd and drags the add pile; the kite was kept as it was.

**Questions for the user.**

- **Q4 (plan change, kill order): kill the fire bots in the phase 4 handover.** Kept into phase 4 they cost uld1 25 s
  of a clock it beat by 7.6 s, three spray deaths and 560 raid-s of silence, to save ~0.2-0.3M of healable fire.
  Recommendation: keep them through phase 3 (no sweep), and cull them in H3, 32 s in which nothing else is
  attackable (4-7 bots, ~63k each, a few seconds of raid output), so phase 4 starts without them.
- **Q5 (plan change, tank): an ACU tank in phase 4.** Its Plasma Ball (65647, 8-11k every ~4 s) rides the
  top-threat ranged: uld3 lost eight to it, uld1 one, and the findings doc has it open. Recommendation: the off-tank
  takes the ACU in phase 4 while the main tank keeps the MK II.

**Shared code:** none.

**Review.**

- The blast is round the bomb, not the victim: 63801 is `TARGET_SRC_CASTER` + `TARGET_UNIT_SRC_AREA_ENEMY`,
  5 yd (~6.5 with a player's combat reach), and the core casts it once the victim is within 3 yd
  (`SMART_EVENT_RANGE` 0-3, core update `2026_08_21_00`; the findings doc still said melee contact). Hit
  bystanders sat 1.2-6.2 yd from the bomb, as tight as the 0.4-5.7 from the victim, so the traces don't
  separate the two and the DBC decides. The victim-centred disc left the bomb's side open: a step-out there
  ended ~6.4 yd from a median detonation. Now the clearance runs from the line victim to
  `ULDUAR_MIMIRON_BOMB_BOT_DETONATION_REACH` (4) yd toward the bomb, and the step goes off that line's nearest
  point. Replayed on windows 14-16 with the step decided 0.5/1/2 s before each blast, step-outs landing within
  6.5 yd of where the bomb went off fell from 2/4/5 (of 59/49/66) to 0/2/3 (of 61/56/84).
- The ~15-50 step-outs a pull only replayed legs that fired under the old trigger. Every bot that comes within
  the clearance of a victim during a bomb's life gives 52-95 a pull (the plain disc 47-74), so expect about as
  many legs as before, a few yards each instead of 30.
- Bomb Bot deaths: 7 of uld2's 10 were the chased bot itself (windows 14-16: 15 chased against 7 collateral; chased
  ranged DPS 11 of 29 dead, chased healers 3 of 3). The step-out only reaches the collateral ones, so read the
  next window's `--bombbots` as chased against collateral, not as one total under 10.
- A bot a bomb chased ignored every other bomb's blast, even while it stood still (ranged, or its bomb over 8 yd
  off). Now only the kite skips them. Two bombs overlapped once in windows 14-16 (`603_20`, 5.5 s, one bomb
  living 20.7 s); the rest lived 0-11 s against one every 15 s.

## Round 10 (build c01caf9cc, round 9 built)

**Data:** window 18, `raidtest.py report --boss mimiron --raid uld --head c01caf9cc --pulls 10`: one valid pull
per pool, no DC veto inside any. Baseline: window 16 (`603_10/11/12_…_17954419xx`). Scripts in the lane scratchpad
(`mim10/`: `spraybar.py`, `deathacts.py`, `bombflips.py`, `p4flips.py`, `parts.py`; round 5's `mim5/spray.py`).

| Pool | Run | Trace | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261123-154723-7` | `603_7_…_1795448890` | **kill** 10:18.8, hard mode, parts down 9:52.0 / 9:56.6 / 10:06.8, under Berserk (10:00.2) | 11: P3 Water Spray 1; P4 Water Spray 4 (two inside a barrage window), mine 1, ACU Plasma Ball 1; P3b Water Spray 1 (the healer revived 0.9 s before, same lane); after Berserk Frost Bomb 1, ACU Plasma Ball 2 |
| uld2 | `tr-20261123-154743-9` | `603_9_…_1795448905` | wipe 10:22.5 in P4, parts 1.2/1.9/4.8% at 10:01 | 31: P1 mine 1; P3 Water Spray 3 (one healer twice); P4 ACU Plasma Ball 3, Hand Pulse 3, Flames 1; Berserk 20 |
| uld3 | `tr-20261123-154723-8` | `603_8_…_1795448895` | wipe 10:19.9 in P4, parts 23-28% at Berserk | 32: P2 Frost Bomb 1, Rapid Burst 2 (the same healer, revived, and a ranged), Heat Wave 1; P3 Water Spray 1, Bomb Bot 1 (chased ranged); P4 Water Spray 4 (two healers and two ranged, 8:03-8:08, all inside a barrage window), mine 1 (a healer, 8:06), ACU Plasma Ball 5, Hand Pulse 11 (8:26-9:52); Berserk 5 |

74 deaths: Berserk 28; before it Water Spray 14, Hand Pulse 14, ACU Plasma Ball 9, phase 2 4, mines 3, Flames 1,
Bomb Bot 1. Hard mode in all three: Summon Flames (Initial) at 7.1 s, Flames from 13-14 s, Emergency Mode at phase 2.

**Stop rules:** one kill of three, 11 deaths in it: not polished. No failure picked three times (round 5's spray
pick was the phase 3 lane crossing; this one is the barrage window).

**Verdict on round 9 (Bomb Bot step-out): held.** `--bombbots` against windows 14-16:

- Step-outs 116/75/104 legs at median 2.9/4.2/4.9 yd (before: every leg 30 yd); `bombbot` notes ok 85/59/91,
  locked 96/72/99, unsafe 6/3/1, fallback 4/4/1.
- Blast victims chased 3/2/2 (1 dead, uld3's ranged Uldzah), collateral 1/1/2 (none dead); Bomb Bot deaths 1
  (window 16: 10, 7 of them chased; windows 14-16: 21 collateral hits).
- Bombs killed or never reaching anyone 2 of 5, 3 of 5, 3 of 6 (window 16: 1 of 12, 2 of 5, 4 of 6).
- A-B-A on the step: with `reach melee` 26/8/12 (window 16: 10/4/12), the arc spread 3/3/10 (7/17/12), the fire
  bot step 1/1/3 (10/12/16); 30/17/25 in all against 28/34/41. uld1's `reach melee` pairs rose, with no death
  after them. `mimiron bomb bot guard` veto rows 0/0/11.
- Phase 3 moving: melee 47/43/52% (window 16: 46/51/45), ranged 33/32/30% (40/53/37).

**The clock** (Berserk at 10:00):

| Pull | P1 | Z2+H1 | P2 | H2 | P3 | H3 | P4 reached | P4 |
|---|---|---|---|---|---|---|---|---|
| uld1 | 89.2 | 48.3 | 89.5 | 24.3 | 144.3 | 32.0 | 7:20.7 | 166 s to the last part, 7 s past Berserk |
| uld2 | 98.9 | 48.3 | 86.7 | 24.2 | 147.2 | 31.9 | 7:30.3 | 150 s to Berserk, parts 1-5% short |
| uld3 | 84.4 | 48.0 | 89.9 | 24.4 | 160.0 | 32.1 | 7:32.0 | 148 s to Berserk, 21 dead in it |

Phases 1-3 now fit (P4 by 7:20-7:32 in all three); phase 4 needs ~165-170 s and gets ~150. Phase 4 is where the
clock and the raid go: kept fire bots (Q4), the ACU's Plasma Ball on ranged (Q5) and Hand Pulse.

**Ranking.** Water Spray and Hand Pulse 14 each. uld3's 11 Hand Pulse deaths followed its healer loss at 8:03-8:08
(spray took two healers and two ranged, a mine a third healer), and round 9 measured the pulse's 22-33% spread as
the `p4melee` layout's floor; ACU Plasma Ball is Q5. Water Spray is a bug: 9 of its 14
deaths came in phase 4, 7 of them inside a barrage window with no dodge at all.

**Failure picked: the spray dodge is off for the whole Laser Barrage window.** In windows 16 and 18, 25 of the 32
phase 4 Water Spray hits fell inside a barrage window (Spinning Up's cast plus 14.5 s, three a phase 4), 23 with no
`spray` flee note from the victim in the 3 s before, 9 dead; the 7 hits outside the windows all had notes
(`spraybar.py`).

- `MimironFireBotTrigger::IsActive` (`UldTriggers_Mimiron.cpp:287` at `c01caf9cc`) returned false while
  `MimironP3Wx2LaserBarrageTrigger` was active, for every bot, Spinning Up included. The barrage dodge returns
  false for a bot `clear` of the band or holding at its edge (`hold`, under a yard of arc left), so those bots got
  no dodge at all. uld1 7:58.4: one spray hit 11 of the melee stack (8 noted `hold 120`, 3 `clear 165`), and the
  death record of Uldxaf shows `mimiron p3wx2 laser barrage action` FAILED every tick for 4 s and no fire bot
  action queued (`deathacts.py`). uld3 8:08.5: Uldzal and Uldzad the same, `hold 135`.
- The barrage step's ring spots ignored spray lanes: their test was fire, bomb and rockets only
  (`UldActions_Mimiron.cpp:506`, `:535`, `:601` at `c01caf9cc`). uld3 8:03-8:06: the step walked healers Uldzae and
  Uldzac and ranged Uldzaq and Uldzal to one spot round (2759-2763, 2567-2569) on `ahead` steps; three died to
  spray within 5 s and the fourth to a mine. uld1 9:53: healer Uldxad on a `trailing` step, killed twice.
- The design meant otherwise: the fire dodge already "keeps running during a barrage for every bot the cone does
  not own", and the spray fan refuses the band at landing (`FleeFan`, `IsMimironSpotBarrageSafe` with the leg's
  travel), the fallback too. The gate predates fire bots reaching phase 4 (round 8 keeps them there).

**Classification:** bug. The strategy means every bot to leave a spray lane, phase 4 included (round 8: "the spray
and siren dodge ... now reach phase 4"); during each barrage they stood in it.

**Fix** (Mimiron files and reader only, every size and mode, with or without a player):

- `MimironFireBotTrigger` no longer stands down for the barrage. Rank does it: the barrage dodge (`+ 7`) takes the
  tick whenever it moves the bot, and the spray and siren fans keep refusing the band at landing.
- `MimironP3Wx2LaserBarrageAction`: a ring spot is clean only out of every spray lane at
  `ULDUAR_MIMIRON_FIREBOT_SPRAY_STAND_HALF_WIDTH` too (`isClean`, used by the step's radius pick, the clean-bearing
  probe and `stepin`), and the walk past an unclean edge runs when lanes exist, not only flames. With no clean
  radius the step still takes the plain one.
- Reader `--firebots`: per phase, the Water Spray hits inside a barrage window, their deaths, how many victims wrote
  no `spray` note in the 3 s before, and spray or siren steps there (tests). Findings doc: "The spray and siren
  dodge never defers to the barrage" and the clean test.

Normal mode and phase 2 are untouched: no fire bots, so no lanes and no fire bot trigger.

**Next window should show** (`--firebots`, `--spin`, `--flips`; `mim10/p4flips.py`):

- Phase 4 `in barrage windows`: Water Spray hits few and `no spray note` near 0 (this window uld1 13 hits / 2 dead /
  12 silent, P3b 1/1/1, uld3 4/4/4; window 16: 6/1/6 and 1/1/0), spray or siren steps there above 0 (0/0/0 now,
  see the review).
- No beam hits still (`--spin` beam ticks 0 in all three now), no barrage death.
- No new P4 A-B-A between `mimiron p3wx2 laser barrage action` and `mimiron fire bot action` (none now); arc spread
  ↔ barrage 22/12/11 and arc spread ↔ fire bot 186/71/87 about as now.
- Phase 4 Water Spray deaths well under this window's 9.

**Not picked.**

- Phase 4 Hand Pulse (14) and the ACU's Plasma Ball (9): Q5 and the layout's floor, above.
- Phase 3 Water Spray 5: uld2's three were `spray none`/`unsafe` with all eleven bearings refused for spray (two
  fire bots crossing near the healer), a dense-lane case, not the barrage.
- Phase 4 mines on barrage legs (19 hits, 2 dead): the step ranks mine-clear radii first but never refuses one.
- Phase 2: uld3 only (4).

**Questions for the user:** none new. Q4 (cull the fire bots in the phase 3 to 4 handover) and Q5 (an ACU tank in
phase 4) stand, not built. This window adds to Q4: kept fire bots sprayed 22 times in phase 4 for 9 deaths and
silenced 361/137/242 raid-s there, and uld1 and uld2 missed the clock by 7 and ~20 s. And to Q5: the ACU's Plasma
Ball killed 9 before Berserk, 7 of them ranged.

**Shared code:** none.

**Review.**

- The reader's barrage window ran 14.5 s from Spinning Up's cast, but the cone's `haz` `sweep` rows stop at
  +13.8-14.0 s (`603_7`: 470.6-484.4 s for the cast at 470.5). All 14/11 spray or siren steps it counted inside a
  window in uld1/uld2 fell at +14.1-14.3 s: the fire bot trigger firing the moment the barrage released it, not a
  dodge during the cone. A window now ends at its last `sweep` row (the 14.5 s only without them, test). Hits,
  deaths and silent counts are unchanged; steps inside are 0 in all six pulls of windows 16 and 18.
- Root cause checked: `603_7` 7:58.4, 11 hits from one spray, 8 victims on `hold 120`, 3 on `clear 165`, none
  with a fire bot `act` row in the 5 s before; Uldxaf's death record shows the barrage action `FAILED` every
  tick from 474.1 s to 478.3 s and no `mimiron fire bot action`. `603_8` 8:03.3/8:03.9 on `ahead` steps,
  8:08.5 two on `hold 135`. Window 16's `603_11`: 5 of its 6 hits on `clear` bots fresh off `refuge` or
  `ahead` steps, 1 on a `trailing` step.
- Left: a `hold` bot whose last move was a barrage step is `locked` for that step's expected time
  (`IsWaitingForLastMove`, FORCED against FORCED), so its spray flee can log `spray locked` and stay. The
  reader counts that as a note, not silent; if hits stay up next window, read the notes before the hits.

## Round 11 (build 99188e0f1, round 10 built)

**Data:** window 19, `raidtest.py report --boss mimiron --raid uld --head 99188e0f1 --pulls 10`: one valid pull
per pool, no DC veto inside any. Baselines: windows 16 and 18 (`603_10/11/12_…_17954419xx`,
`603_7/8/9_…_17954489xx`). Scripts in the lane scratchpad (`mim11/`: `deathmech.py`, `p2shape.py`, `p2rate.py`,
`healers.py`, `movers.py`, `barrage_scene.py`, `barrage_alt.py` (`--fire` for flames); round 10's `p4flips.py`,
`parts.py`; round 8's `stuck.py`).

| Pool | Run | Trace | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261123-165428-9` | `603_9_…_1795452910` | **kill** 10:03.5, hard mode, parts down 9:42.5 / 9:45.6 / 9:49.5, no Berserk cast | 8: P2 Rapid Burst 1 (ranged walking in); P3 Bomb Bot 2; P4 Frost Bomb 2 (melee on frost bomb legs, 7:29, 9:13), ACU Plasma Ball 1, mine 1 (ranged on a barrage leg, 7:50), Water Spray 1 |
| uld2 | `tr-20261123-165328-8` | `603_8_…_1795452850` | wipe 5:41.7 in P2, VX-001 at 12% | 31: P1 Flames 2 (both hunters casting Revive Pet on the drop, 0:15.9); P2 Heat Wave 11, Rapid Burst 12, Flames 6 (a holy paladin at 3:01; five bots at 2-15% on barrage legs, 4:18-4:28) |
| uld3 | `tr-20261123-165307-7` | `603_7_…_1795452829` | wipe 10:14 in P4 at Berserk (10:00.2), parts 15/15/19% | 31: P3 Water Spray 1, Bomb Bot 1; P4 mines 6 (4 ranged and healers on barrage legs 7:52.1-7:53.3, 2 melee on frost bomb legs 9:11-9:12), Water Spray 4, Hand Pulse 3, ACU Plasma Ball 2, Laser Barrage 1; Berserk 13 (Frost Bomb 3 at 10:01.7, two on barrage legs) |

70 deaths: P2 Heat Wave/Rapid Burst 24 (uld2 23), Berserk 13, Flames 8, mines 7, Water Spray 6, Bomb Bot 3, ACU
Plasma Ball 3, Hand Pulse 3, Frost Bomb 2 (3 more under Berserk), Laser Barrage 1. Hard mode in all three:
Summon Flames (Initial) at 7.1-7.2 s, Flames from 9.1-14.0 s, Emergency Mode at phase 2.

**Stop rules:** one kill of three, 8 deaths in it: not polished. No failure picked three times (round 10's was the
spray dodge deferring to the barrage; this one is the barrage step's own legs).

**Verdict on round 10 (spray dodge during the barrage): held.** `--firebots` phase 4 `in barrage windows`, uld1 /
uld3 (uld2 had no phase 4):

- Water Spray hits 0 / 3, dead 0 / 3, `no spray note` 0 / 0 (window 18: uld1 13 + P3b 1 hits, 2 + 1 dead, 12 + 1
  silent; uld3 4 / 4 / 4); spray or siren steps inside 0 / 9 (0 in every earlier pull).
- uld3's three (9:56.3, melee) were pinned, not silent: `spray none` / `unsafe` with 16 mine and 4-8 cone refusals and
  `frostbomb unsafe` (mine 6, cone 14), a fusing Frost Bomb, the cone, a fresh mine ring and a lane at once, 4 s
  before Berserk.
- Beam ticks 0 / 1: uld3 Uldzag (8:02.4, one tick at 94%), on an arc spread leg 0.5 s after `clear 105`, the cone's
  last ticks. Barrage ↔ fire bot A-B-A 0 / 3 (window 18 none); arc spread ↔ barrage 25 / 17 (22/12/11), arc spread ↔
  fire bot 97 / 113 (186/71/87).
- Phase 4 Water Spray deaths 5 (window 18: 9).

**The clock** (Berserk at 10:00):

| Pull | P1 | Z2+H1 | P2 | H2 | P3 | H3 | P4 reached | P4 |
|---|---|---|---|---|---|---|---|---|
| uld1 | 89.1 | 48.1 | 89.8 | 24.2 | 139.3 | 32.0 | 7:15.9 | parts down by 9:49.5, 10 s before Berserk |
| uld2 | 101.7 | 48.2 | **178.7**, raid dead | | | | | |
| uld3 | 85.8 | 48.2 | 90.5 | 24.3 | 145.1 | 32.2 | 7:19.2 | 161 s, parts 15-19% at Berserk |

Phases 1-3 fit where phase 2 holds (phase 4 by 7:16-7:19, window 18 7:20-7:32). uld3 lost 7 by 8:02, 5 of them in its
first phase 4 barrage window (7:48.9), and took the parts down at 0.21%/s against uld1's 0.33%/s.

**Why uld2 died at 5:46.**

1. Two DPS short from 0:15.9. Both hunters (Uldyaj, Uldyau) started Revive Pet (982, 8.8 s cast) at 0:09.8 on the
   drop, among the Summon Flames (Initial) nodes; once those lit, three overlapping nodes killed them in 2 s. A bot
   mid-cast runs no engine pass (`PlayerbotAI.cpp:340-418`, preparing cast → yield), so no fire dodge, not a single
   flee note. Same death in windows 7 and 16, always roster B. Uldyau was revived and died at 4:07.6, casting it again.
2. Phase 2 opened in fire: 28 nodes within 30 yd of the centre at its first sample (uld1 8, uld3 11), P2 Flames 599k
   (437k, 206k). One melee (Uldyak) stood 48 yd out at phase 2 on a `stranded` walk-in (12.9 yd of burn); healers went
   after it (`reach party member to heal` 70% of healer samples at 2:50), and healing in phase 2's second 10 s ran 26k/s
   against 39-43k/s in uld1 and uld3, under 47k/s of intake.
3. 3:01.4, 18 s in: the holy paladin Uldyac died in fire. Its flames rungs 3, 5 and 7 found nothing (7-10 bearings
   refused for fire), the 10 yd rung fell back unscreened onto another node (Flames 1.5 yd off at death), and the leg
   locked its next dodges (`flames locked` for 1.4 s) down to 8%.
4. Three healers for the rest of phase 2. The 3:13 barrage swept the healer wedge (healers moving 70-100% for 10 s,
   the dodge working); their mana went 54% (3:25), 26% (3:45), 15% (3:55), 2% (4:26). From 3:40 Heat Wave and Rapid
   Burst took the raid: four melee waiting out the 3:33 Frost Bomb's fuse 28-35 yd out, then two healers and two
   ranged at 3:54-3:55. The 4:16 barrage walked five bots at 2-15% through fire (4:18-4:28). VX-001 stood at 46% 52 s
   into phase 2 (uld1 36% at 54 s, uld3 38% at 58 s) and at 12% at the wipe.

**Ranking.** uld2's 23 Heat Wave and Rapid Burst deaths follow from the healer loss, uld3's 13 Berserk deaths from its
first phase 4 barrage. Bug-shaped, the barrage step's legs killed 10 before Berserk (mines 5: uld3 4, uld1 1; Flames
5: uld2's, at 2-15%) and 2 under it (Frost Bomb). Mine deaths on barrage legs: 2 in window 16, 2 in window 18, 5
now, all ranged or healers.

**Failure picked: the Laser Barrage step walks ranged over phase 4 mines.**

- Phase 4's Proximity Mines (65347, landing ~1.6 s later) fall on a 15.0 yd circle round the chassis: at 7:52 in uld3
  ten at exactly 15.0 from VX-001, plus nine of the previous cast at 9.5-25.4, eight of them 19-25 yd out (the chassis
  had moved). The phase 4 ranged ring is 13-24 yd from VX-001, so it runs through both.
- `stepRing` in `MimironP3Wx2LaserBarrageAction::Execute` (`UldActions_Mimiron.cpp:520-571` at `99188e0f1`) scored the
  radius shifts at one heading, the 40° arc, and with none mine-clear took the best crossing one, or with no clean
  endpoint the plain step with no mine test at all.
- uld3 7:50.9-7:53.6 (`barrage_scene.py`): ranged legs of 41-45° at r 15-18 passed 3-5 mines each (Uldzau 3, Uldzad 3,
  Uldzar 4, Uldzah 5, Uldzaq 4 then 2). Uldzah walked from 80% into two blasts (10.5k + 9.3k), Uldzar from 52%, Uldzau
  took two after Plasma Ball and Hand Pulse, Uldzad one, reincarnated at 20%.
- Replay (`barrage_alt.py`, the seven phase 4s of windows 16-19; rockets and spray lanes not modelled, every live
  flame counted burning): 132 of 715 phase 4 ring steps passed a live mine (2.5 yd off the leg or 3.5 off the end),
  76 of them ranged; at the step's heading every clean radius crossed one in 105, none was clean in 22. A 30°, 20° or
  10° arc over the same radii had a clean, mine-clear leg for 43 of the 76 ranged and 24 of the 56 melee; 5° steps
  from 35° to 5° reach 47, ranged out to 28 yd 6 more.

**Classification:** bug. The step ranks a mine-clear leg above every other (findings doc) but searched one arc, on the
premise that mines land inside 15 yd and healthy bots take two or three blasts; in phase 4 both are wrong.

**Fix** (Mimiron files only, every size and mode, with or without a player): `stepRing` picks over the 40° arc as
before; when no radius there gives a clean, mine-clear leg and a mine is on the floor, it tries
`ULDUAR_MIMIRON_BARRAGE_SHORT_STEPS` (30°, 20°, 10°) and takes the first arc with one, else the full step as before.
The next tick plans the rest. Phases 2 and 3 have no mines, so their steps are unchanged. Findings doc: "Phase 4
mines sit on the ranged ring, so a mined step shortens its arc", replacing the wrong premise.

**Next window should show** (`--mines`, `--spin`, `--firebots`; `mim11/barrage_alt.py`, `deathmech.py`):

- Phase 4 barrage legs passing a mine well under 17 of 86 (uld1) and 16 of 110 (uld3) (windows 16-18: 6-18 a pull),
  barrage-leg mine hits under 9 and 8 a pull, and no mine death on a barrage leg (5 now).
- 10-30° barrage legs where mines lie, with no beam hit (`--spin` beam ticks 0) and no barrage death from them.
- Phase 4 deaths in the first barrage window near uld1's 1 (uld3 5).
- Phase 2 barrage legs as before (`--walk` P2 barrage shares, `--fire` P2).
- Round 10's: no silent spray hit in a barrage window.

**Not picked.**

- uld2's holy paladin through the 10 yd flames fallback (1 death, the turn of that pull): the findings doc's open item,
  bots pinned between fire with every bearing refused.
- Hunters casting Revive Pet on the drop (2; windows 7, 16, 19, roster B): their pets are dead at a run's first pull,
  which is DC's pre-pull refresh (orchestrator). Strategy side, a Mimiron hold on `revive pet` in Firefighter would end
  the 8.8 s freeze, at the cost of the pet.
- Frost Bomb blast 2 (uld1 melee on frost bomb legs) and mines on frost bomb legs 2 (uld3 melee).
- uld3's beam death on an arc spread leg at the cone's tail, 1.
- Water Spray 6, Bomb Bot 3, ACU Plasma Ball 3 (Q5), Hand Pulse 3.

**Questions for the user:** none new; Q4 (cull the fire bots in the 3→4 handover) and Q5 (the off-tank tanks the ACU in
phase 4) stand, not built. This window adds to Q4: fire bots 1.9 and 1.7 alive on average in phase 4, Water Spray 1
and 4 hits there (5 dead), Deafening Siren 256 and 143 raid-s; to Q5: the ACU's Plasma Ball killed 3 ranged before
Berserk.

**Shared code:** none.

**Review.** A short arc searched the full ±8 yd radius shifts, so a 10° step could take an 8 yd shift and turn 8°/s
against the sweep's 10.64°/s; the simulation's "40° down to 10°" cleared constant-radius steps only. Replaying the
pick over windows 16-19 (`rv_mim11/short_pick.py` in the scratchpad), 4 of ~80 short picks for `inside` or `ahead`
bots turned below the sweep and slower than their full step. Short arcs now cap the shift at the arc's chord (12.7°/s
or more over 14-24 yd), keeping 40 of the 76 ranged and 22 of the 56 melee crossings (43 and 24 uncapped), and the
short search runs only when the move can issue. The next-window expectations above stand.

**User answers (2026-11-23).**

- **Q4: yes.** Keep the fire bots through phase 3 (no sweep) and kill them in the phase 3 to 4 handover, when nothing
  else can be attacked.
- **Q5: yes.** The off-tank tanks the ACU in phase 4 while the main tank keeps the MK II.

## Round 12 (build 90400b39c, round 11 built)

**Data:** window 21, `raidtest.py report --boss mimiron --raid uld --head 90400b39c --pulls 10`: one valid pull per
pool, no DC veto inside any. Baselines: windows 16-19. Scripts in the lane scratchpad (`mim12/`: `barrage_mines.py`,
`mine_scene.py`, `acu_p4.py`, `h3_units.py`, `opening.py`, `fallback.py`; round 11's `deathmech.py`,
`barrage_alt.py`).

| Pool | Run | Trace | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261123-185658-11` | `603_11_…_1795460261` | **kill** 9:25.7, hard mode, no Berserk cast | 2: P2 Rapid Burst 2 (one melee on an arc spread leg, revived, again) |
| uld2 | `tr-20261123-185919-12` | `603_12_…_1795460403` | wipe 10:09.7, P4 opened 9:56.7 | 30: P1 Flames 2 (both hunters casting Revive Pet on the drop, 0:14.9), Shock Blast 2 (0:38.3, one of them revived); P3 Bomb Bot 4, Water Spray 3, ACU Plasma Ball 1; Berserk 18 |
| uld3 | `tr-20261123-185618-10` | `603_10_…_1795460221` | wipe 9:32.1 in P3, raid dead | 31: P1 Shock Blast 1; P2 Rapid Burst 6, Heat Wave 3 (a healer twice, a ranged twice); P3 Bomb Bot 7, ACU Plasma Ball 4, Water Spray 3, Junk Bot 4, Assault Bot 1, Flames 2 |

63 deaths: Berserk 18, Bomb Bot 11 (9 of them the chased bot), phase 2 Rapid Burst and Heat Wave 11, Water Spray 6, phase
3 Plasma Ball 5, Junk and Assault Bots 5, Flames 4, Shock Blast 3. Hard mode in all three (Flames from the opening,
Emergency Mode at phase 2).

**Stop rules:** one kill of three (2 deaths): not polished. No failure picked three times.

**Verdict on round 11 (short barrage steps over mines): held for its target, on one phase 4.** Only uld1 reached a real
phase 4 (uld2's lasted 12.9 s into Berserk, uld3 died in phase 3). There no ranged or healer barrage leg passed a live
mine (0 of 24; window 19: uld1 17 of 63, uld3 11 of 53), none took a mine hit (9 and 8) and no mine killed anyone on a
barrage leg (5). Melee legs did: 12 of 64, all in the first barrage, where the chassis had moved the 8 yd melee ring
onto two mines of the 7:22 cast and one stack step at 7:29.9 took 9 hits (83k), no death; the ring's radius shifts
(5-8 yd) have no room round them. Beam ticks 0 in all three barrage windows, first phase 4 barrage window deaths 0
(window 19 uld3: 5), phase 2 barrage legs 47 (46). Round 10's check stays: no Water Spray hit in a phase 4 barrage window.

**The clock** (Berserk at 10:00):

| Pull | P1 | Z2+H1 | P2 | H2 | P3 | H3 | P4 reached | P4 |
|---|---|---|---|---|---|---|---|---|
| uld1 | 84.3 | 48.3 | 84.9 | 24.2 | 131.9 | 32.0 | 6:58.7 | 130.1 s, then a 7 s Self Repair round; kill 9:25.7 |
| uld2 | 101.3 | 48.1 | 103.3 | 24.3 | **274.2** | 32.3 | 9:56.7 | 12.9 s, Berserk 10:00.2 |
| uld3 | 95.8 | 48.0 | 95.1 | 24.3 | **295.6**, raid dead | | | |

uld1 is the fastest phase 4 entry yet (window 19: 7:16-7:19). The two wipes lost phase 3: landings of 20/20/8/20/6 points
(uld2) and 12/9/8 (uld3, nine dead in phase 2 first) against uld1's 33/39/5, with fire bots at 4.9 and 7.9 alive on
average and Bomb Bots reaching their target 8 of 11 and 13 of 14 times.

**Built: the user's Q4 (fire bots killed in the phase 3 to 4 handover).** Every window since round 8 had 3-7 kept fire
bots through H3 with no aim at them (window 21: uld1 4, uld2 3) and into phase 4.

- `GetMimironKeptFireBots`' 250 ms fold splits the live ones: kept as before, except while a spent, unselectable ACU
  stands in the room (`NOT_SELECTABLE`, under `ULDUAR_MIMIRON_SPENT_HEALTH_PCT`; traced at 0.18-0.19% through every H3),
  when they go to the new `IsMimironFireBotCullActive` instead. Phases 3 and 4 are unchanged; a survivor into phase 4 is
  kept again.
- `mimiron target guard` also runs while the cull is on, and `MimironSetDpsPriorityTrigger` for non-healers;
  `BuildPriorityList` puts the fire bots after the leftover Bomb, Assault and Junk Bots (uld2 entered H3 with 4 Junk
  Bots and an Assault Bot). The splash guard and the strategy's target exclusions read the kept set, so they lift in H3.

**Built: the user's Q5 (the off-tank tanks the ACU in phase 4).** Untanked, the ACU's Plasma Ball (65647, single
target, range 50,000) rode its top-threat ranged: in eight phase 4s of windows 16-21 its target was a ranged 72-99% of
samples, 29-37 balls on ranged a phase 4, while the off-tank aimed at VX-001 88-99% from his `p4melee` slot.

- `GetMimironPhase4AcuTankFocus`: the first living assist tank (`IsAssistTankOfIndex(bot, 0, true)`, which counts real
  players, so a real off-tank does the job himself) gets the ACU while it is attackable, else the melee focus.
  `MimironPhase4FocusTrigger`/`Action` attack it; `mimiron phase 4 acu tank target guard` (`Role::Tank`) keeps `tank
  assist` off his target; his class `lose aggro` taunt (Growl, Dark Command, Righteous Defense) takes it back. No tank
  ever ran `p4hold`, so he never holds.
- **What moves: nothing.** The main tank keeps `p4tank` and its fire drag, the off-tank his `p4melee` slot on the melee
  arc. The three parts share one point, the ACU's combat reach is 0 (model 28979), so its 5 yd melee range lies within
  the slot's 5 yd tolerance, and the ball has no splash. Only the ball moves, onto him.

**Fixed: a bot mid-cast in fire (round 11's candidate; evidence holds).** uld2's hunters Uldyaj and Uldyau began Revive
Pet (982, 8.8 s) at 0:07.1-0:07.2 on the drop, took six Flames ticks each from 0:13.9 and died at 0:14.9 with no flee
note, while every other bot there dodged (`flames+3 ok`); windows 7, 16, 19 and 21, always roster B. `PlayerbotAI`
yields before the engine while a cast prepares (`PlayerbotAI.cpp:340-418`), so the dodge trigger never runs. Now the
encounter tick (`MimironEncounterTick`, on `MimironTick`'s 250 ms scan) runs `MimironDodgeFlamesTrigger` for every bot
casting with more than `ULDUAR_MIMIRON_FLAMES_CAST_FINISH_MS` (1 s) left and calls `RequestSpellInterrupt` where it
fires, noted `mimiron.castcut`; the bot's next update interrupts and the dodge runs. Real players are skipped.

**Fixed: the flames ladder's fallback landing in fire (round 11's candidate; evidence holds).** 47 `flames+10 fallback`
notes in window 21; 15 of the 32 legs they issued ended within 3.5 yd of a live node (uld2's H1: 6 of 6), and 15 notes
issued no leg at all (uld3's healer Uldzac wrote 12 in 8 s at 1:13-1:21, taking Flames throughout). For a
position flee without `allowFire` (flames, Bomb Bot, siren), `FleeFan`'s fallback now scores every fan bearing at the
full hop, collision-clamped, past the same hard screens (shock, bomb, rocket, barrage, spray lane, holder) and the
backtrack and progress tests: fewest burning nodes, then off mines, then farthest from a node up to 5 yd, then
straightest; it tries them in that order until one issues and notes the bearing taken. `unsafe` now means no bearing
passes. Unit flees (Frost Bomb, rocket, shock) and fire-allowed ones keep the straight fallback.

Reader: `--acutank` (phase 4: the ACU's target by off-tank, main tank or role, the off-tank's aim, Plasma Ball hits and
deaths by victim) and `--fire` (fallbacks and how many legs ended in a burn, casts cut), with tests. Findings doc: the
fire bot cull, the ACU tank, the cast cut, the least-burn fallback, the melee ring's mines.

**Next window should show** (`--firebots`, `--acutank`, `--fire`, `--mines`; `mim12/acu_p4.py`, `fallback.py`):

- H3: aim at fire bots above 0 bot-s (0 in every pull since round 8), 0 fire bots alive at H3's close and at phase 4's
  open, phase 4 Water Spray and siren near 0 (window 19: 1-4 spray hits, 143-256 raid-s), phase 4 shorter for the
  splash it gets back (uld1 130 s).
- Phase 4: the ACU on the off-tank most of the phase (window 21 uld1: 1%, ranged 98%), Plasma Ball on ranged near 0
  (30), the off-tank aiming at the ACU most samples, no ranged death to Plasma Ball (windows 18-19: 9 and 3), and no
  off-tank death to it.
- Opening: `castcut` notes for Revive Pet when fire lands on a reviving hunter, no hunter dead to Flames at 0:14-0:16
  (roster B).
- Fallback legs ending in a burn near 0 (15 of 32), `fallback` notes with a bearing, none repeating without a leg.
- Round 11's: no ranged or healer barrage leg past a mine.

**Not picked.**

- Phase 3 length (uld2 274 s, uld3 296 s) and its Bomb Bots (11; 9 chased victims, bombs reaching their target 21 of
  25): next round's candidate once the cull and the ACU tank are judged.
- Phase 2 Rapid Burst and Heat Wave (uld3 9, a healer and a ranged twice each).
- Melee barrage legs over mines (above): the ring's 5-8 yd shifts leave no room; a short arc cannot clear a mine on
  the ring itself.
- Shock Blast 3, the uld1 melee's two Rapid Burst deaths on an arc spread leg.

**Questions for the user:** none.

**Shared code:** none (Mimiron files and its reader; `MimironTick` now returns whether it scanned, Mimiron-only).

**Review.**

- The least-burn fallback also ran for the Bomb Bot and siren flees, whose hop reaches their clear radius
  (`clearRadius`, 9 and 19 yd) on the straight bearing only. A side bearing at that hop stopped short of the
  siren's 19 yd, or ran along a Bomb Bot's blast line and stayed inside the blast the fan had refused,
  whenever it burned less than the straight leg. Window 21 had 4 `bombbot fallback` and 1 `siren fallback`
  notes, each with 7-11 bearings refused for fire, so the case is live. Radius flees keep the straight leg
  now; only the flames dodge ranks bearings.
- The ranking ignored the fan's two lethal screens, so a flames fallback could trade a straight leg clear
  of a Bomb Bot blast or Rapid Burst for a side one inside it. Bearings clear of both rank first now, ahead
  of burning nodes.
- Checked and holding: the cited evidence (`603_12` Uldyau's Revive Pet from 0:07.1 with 8.86 s to go,
  Flames kill at 0:14.9; 47 `flames+10 fallback` notes, 15 of 32 legs in a burn; uld1's ACU on ranged 98%,
  the off-tank on VX-001 88%). The cull's `NOT_SELECTABLE` key cannot fire in phase 4, where a downed part
  only turns `NON_ATTACKABLE`, and Self Repair lifts the ACU only at phase 4's start, so it holds all of H3.
  One code path: the cut skips real players, `IsAssistTankOfIndex` counts them.
- Left: a bot whose dodge cannot issue (every bearing `unsafe`) gets each cast with over 1 s left cut on
  every 250 ms scan while it stands in fire, a healer too. The reader folds cuts under 1 s apart into one,
  so read the raw `castcut` notes: a run on one bot with no `flames` leg after it means the cut should wait
  for the last dodge to have issued.

## Round 13 (build a88ea2ade, round 12 built)

**Data:** window 22, `raidtest.py report --boss mimiron --raid uld --head a88ea2ade --pulls 10`: one valid pull per
pool, no DC veto inside any. Baselines: windows 16-21. Scripts in the lane scratchpad (`mim13/`: `p3line.py`,
`coreuse.py`, `carrier.py`, `p4pos.py`, `p4dps.py`, `p4bombs.py`, `bombscene.py`, `bombcross.py`, `bombcharge.py`,
`walkins.py`, `acupre.py`, `castcut.py`, `revtime.py`, `pets.py`, `hm.py`; round 10's `parts.py`, round 11's
`deathmech.py`).

| Pool | Run | Trace | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261123-200230-1` | `603_1_…_1795464213` | **kill** 9:45.6, hard mode, no Berserk cast | 7: P3 Bomb Bot 3 (two ranged in one blast at 4:51.4, one of them again at 5:09.2); P4 Frost Bomb 2 (a rogue's Killing Spree at 7:21.3 and a warrior's Intercept at 8:04.9 into a live bomb, 0.9 and 2.1 s before its blast), Rocket Strike 1 (ranged), Hand Pulse 1 (melee 9:04.2) |
| uld2 | `tr-20261123-200230-2` | `603_2_…_1795464213` | wipe 10:18 in P4 at Berserk (10:00.2), parts 21-26% | 31: P2 Rapid Burst 2 (a healer, a melee); P3 Water Spray 3 (a ranged twice, a melee), ACU Plasma Ball 1, Bomb Bot 1; Berserk 24 |
| uld3 | `tr-20261123-200230-3` | `603_3_…_1795464217` | wipe 10:12 in P4 at Berserk (10:00.2), parts 10/10/14% | 31: P2 Rapid Burst 1, Flames 2 (that melee revived, and a healer after a dodge arrived); P3 Water Spray 2, Bomb Bot 1; P4 Hand Pulse 7 (8:05-9:26, six within 15 s of a Frost Bomb); Berserk 18 |

69 deaths: Berserk 42; before it Hand Pulse 8, Bomb Bot 5, Water Spray 5, P2 Rapid Burst 3, Flames 2, Frost Bomb 2,
Rocket Strike 1, P3 ACU Plasma Ball 1. Hard mode in all three: Summon Flames (Initial) at 7.1-7.2 s, Flames from
13.2-15.9 s, Emergency Mode at phase 2.

**Stop rules:** one kill of three, 7 deaths in it: not polished. No failure picked three times (the Frost Bomb is new).

**Verdict on round 12.**

- **Fire bot cull in the 3→4 handover (Q4): held.** H3 aim at fire bots 52/72/97 bot-s (0 since round 8), fire bots
  4/7/4 at H3's open and 0 at its close and at phase 4's open in all three (window 21: 3-7 into phase 4); phase 4
  Water Spray 0 hits, siren 0 raid-s, splash holds 0 (window 19: 1-4 hits, 143-256 raid-s); no aim at a fire bot in
  phase 3. uld1's phase 4 ran 140.8 s at 105k/s with 22.7 alive (window 21's kill: 130.1 s, 113k/s, 25 alive).
- **The off-tank on the ACU (Q5): held, with a leak.** Before Berserk the ACU targeted the off-tank 99/58/81%
  (window 21: 1%), Plasma Ball hit ranged 0/7/6 times (29-37 a phase 4 before), no death to it before Berserk
  (windows 18-19: 9 and 3), the off-tank aimed at the ACU 99-100%. uld2's ranged Uldyap took it back for 18 s
  (8:57-9:15) and for 2-5 s four more times; the off-tank there is the roster B bear.
- **Cast cut in fire: held.** No hunter died at the opening (roster B lost one or both in windows 7, 16, 19 and 21); 66 `castcut` notes,
  10 without a flames leg within 1.5 s. Side effect: roster B's hunters, petless from the pull, lived and kept
  starting Revive Pet: 46 starts, none finished, 175-176 s each (29% of the fight, 25-43% of phases 1-2), the fire
  chains walking to the still caster and the cut ending it. Fixed below.
- **Fire dodge fallback: held.** 7/10/14 `flames` fallbacks, all with a bearing, 3 of 31 legs ending in a burn (window
  21: 15 of 32, 47 notes with 15 issuing no leg); `unsafe` 1 (55).
- Round 11's: no mine kill on a barrage leg; barrage-leg mine hits 0/2/1.

**The clock** (Berserk at 10:00):

| Pull | P1 | Z2+H1 | P2 | H2 | P3 | H3 | P4 reached | P4 |
|---|---|---|---|---|---|---|---|---|
| uld1 | 90.8 | 48.3 | 81.8 | 24.2 | 138.6 | 32.0 | 7:08.8 | 140.8 s at 105k/s, then 13 s of Self Repair rounds; kill 9:45.6 |
| uld2 | 84.9 | 48.1 | 96.8 | 24.3 | **201.1** | 32.3 | 8:20.6 | 99.6 s to Berserk at 86k/s, parts 21-26% |
| uld3 | 83.3 | 48.0 | 93.3 | 23.9 | 137.0 | 31.8 | 7:10.7 | 169.5 s to Berserk at **69k/s**, parts 10-14% |

- **uld2 lost phase 3.** Its first core went down at 5:08.1 while the ACU chased a ranged: the core landed under the
  unit, which was 14 yd away when the core's 12 yd pull-down (64436, 3 s after the use) went out, so the first landing
  came at 5:36.4, ~25 s late, with every add timer behind it (1 miss in 62 uses over windows 14-22). Its landings then
  took 22.8/29.5/27.6 points (uld1 27.5/47.3/7.2, uld3 32.9/31.1/16.7) with three healers and both hunters casting
  Revive Pet 15% of the phase and 28-43% of phase 2 (uld2's phase 2 ran 97 s, 15 s over uld1).
- **uld3 lost phase 4.** It arrived 2 s behind uld1 but dealt 69k/s against 105k/s: 20 alive at the open, then the
  7:59 and 8:58 Frost Bombs split it (below), six dead to Hand Pulse inside those two windows and 1.04M lost there.

**Failure picked: in phase 4 the formation walks bots into the live Frost Bomb.** The 20 s from a phase 4 bomb's spawn
were 39% of twelve phase 4s (windows 16-22) and held 57 of their 97 deaths before Berserk, Hand Pulse 15.8k/s against
8.6k/s outside; the 13 bombs that moved the stack 20 yd or more cost a median 0.49M of raid damage each (up to
0.99M).

- The stack follows the main tank's escape, so slots round VX-001 end up across the bomb. uld3 7:59 (`bombscene.py`):
  bomb 13 yd from VX-001; Uldzaa fled, then his `p4tank` arc spread leg walked him back toward his spot
  17.5 yd from the bomb (8:01.3), the escape fired again (`frostbomb ok +113`, shock refusals) and took him to (2699,
  2574), 46 yd off centre; VX-001 followed to 43 yd. Melee ran the other way: Uldzag's escape to (2752, 2593), then
  `mimiron arc spread action` to its `p4melee` slot across the blast, the escape at 30 yd, the arc spread again, every
  ~280 ms for the whole fuse (11 escape legs). Ranged sat 55-63 yd from VX-001, healers 26-53.
- uld3 8:58: the stack went one way to 34 yd off centre, healers the other to 63-72 yd from it, and three melee walking
  back on arc spread legs died to three Hand Pulses at 9:04.2-9:07.8 at 6-16% health with no healer in range.
- `--bomb`'s new walk-in count gives 40/35/90 non-escape legs a pull into a live phase 4 bomb (full phase 4s of windows 16-21: 19-72;
  phase 2 0-4 in every pull), nearly all `mimiron arc spread action`; escape legs 136/84/144 a phase 4 (phase 2
  6-54). The main tank took 12-22 escape legs a bomb (Uldxaa 22 and 12, Uldzaa 13 and 12).
- Code: `MimironArcSpreadAction::Execute` (`UldActions_Mimiron.cpp:815` at `a88ea2ade`) sends the main tank to his
  anchor slot with no hazard test, and `MimironArcSpreadTrigger` skips the approach for him
  (`UldTriggers_Mimiron.cpp:125`); `GetMimironSlotApproaches` (`UldEncounter_Mimiron.cpp:930-948`) tests a leg against
  fire and mines but not a bomb, whose radius only the endpoints are screened for (`IsMimironSpotBombSafe`, 32 yd).
  In phase 2 VX-001 never moves, so slots stay on the bot's side and the gap never showed.
- Also: the charges and Killing Spree. `mimiron frost bomb guard` holds the walking movers within 38 yd of a live
  bomb, and `mimiron charge guard` holds charges only for a bot already inside one, so a gap-closer from outside lands
  in it: 49 in windows 16-22 (32 in phase 2), 7 killed by the blast, both of uld1's Frost Bomb deaths this window.

**Classification:** bug. The findings doc's frost bomb guard exists because "the generic movers walk bots back onto the
edge, and the flee ping-pongs there for the whole fuse"; the formation and the gap-closers do exactly that.

**Fix** (Mimiron files and reader only, every size and mode, with or without a player):

- `IsMimironLegBombSafe`: a leg is refused when it runs deeper inside `ULDUAR_MIMIRON_FROST_BOMB_STAND_RADIUS` (32)
  of a live bomb than the bot stands, so walking out passes. `GetMimironSlotApproaches` applies it to the direct leg,
  `cross`, both legs of every detour, the over-mines walk and `stranded`; with nothing left it notes `mimiron.approach
  none bomb` and the arc spread stands down until the blast. The main tank's anchor walk takes the same leg test in
  `MimironArcSpreadTrigger`, so he holds where his escape left him, the MK II and the stack with him; his spot stays
  exempt from every other screen.
- `mimiron frost bomb guard` also holds `Family::Charge` (Charge, Intercept, Feral Charge) and `killing spree`.

**Fixed, small and independent: `mimiron revive pet hold`** blocks `revive pet` while the hard-mode encounter is
engaged (`MimironFireFight`). Roster B's hunters have no pet in any Mimiron pull of windows 18-22 and started Revive
Pet 18+18, 1+13, 1+2 and 20+26 times; none finished. Before the engage they still try.

Reader: `--bomb` adds per bomb the walk-ins (accepted legs, any mover but the bomb's own escape, that ran deeper
inside 32 yd than the bot stood) by mover and how far VX-001 moved over the fuse, with a test. Findings doc: "In
phase 4 the formation walked bots through the fuse", the guard's new rows, the anchor's one exception, "A dead pet
stays dead under Firefighter", the `none bomb` note.

**Next window should show** (`--bomb`, `--acutank`, `--firebots`; `mim13/p4bombs.py`, `bombcharge.py`, `revtime.py`):

- Phase 4 `walks into the live bomb` near 0 and no `mimiron arc spread action` among them (40/35/90), escape legs far
  under 136/84/144 a phase 4, the main tank 1-3 escape legs a bomb (12-22), `none bomb` notes during fuses.
- VX-001 moved over a fuse at most ~25 yd (31 twice this window), no stack past ~35 yd off centre after a bomb (43).
- In the 20 s after a phase 4 bomb: Hand Pulse near the outside rate (15.8k/s against 8.6k/s), deaths well under
  this window's 6 (uld3), and a median loss well under 0.49M for a bomb that moves the stack.
- No gap-closer into a live bomb (`bombcharge.py`; 49 in windows 16-22) and no Frost Bomb death.
- Roster B hunters: no Revive Pet after the engage, `mimiron revive pet hold` veto rows, their Auto Shot and Steady
  Shot back (uld2: 175 s each on Revive Pet), uld2's phase 2 near 82-90 s.
- Round 12's checks hold: fire bots 0 at phase 4's open, the ACU on the off-tank, no opening hunter death.

**Not picked.**

- uld2's missed core (1 in 62 uses): a use while the ACU flies to a ranged target, which a stationary-unit gate on
  `IsMimironCoreUseReady` would catch at the cost of a later use; next if it repeats.
- The ACU leaking to ranged in uld2 (31% of its phase 4 before Berserk): the bear off-tank lost it for 18 s.
- Bomb Bots 5 (uld1's blast took two ranged at once), Water Spray 5, phase 2 Rapid Burst 3 and Flames 2.
- The raid still splits round a phase 4 bomb on the first escape: everyone flees straight away from it while the
  stack goes with the main tank. If deaths in bomb windows stay up once the walk-ins are gone, a plan question:
  bias every escape toward the main tank's side of the bomb.

**Questions for the user:** none.

**Shared code:** none (Mimiron files and its reader).

**Review.**

- Checked and holding: `--bomb` reproduces the phase 4 walk-ins 40/35/90, 140 of them `mimiron arc spread action`
  and 59 of those the main tank's (Uldxaa 23, Uldyaa 20, Uldzaa 16). uld3's 7:59 bomb: Uldzaa's arc spread leg ran from
  34 yd to his spot 17.3 yd off the bomb and alternated with the escape every ~270 ms from 8:05.8 to 8:09.0, the leg
  the trigger now refuses. uld1's Killing Spree (37 → 26 yd) and Intercept (34 → 24) began inside the guard's 38 yd,
  so `Family::Charge` and `killing spree` cover both deaths. uld2's hunters cast Revive Pet 46 times, `castcut 982`
  cut it, and no pet of theirs appears in the trace. Normal mode and 10-man run the same code (no bomb, so
  `hazards.bombs` is empty); a real player changes nothing. The pet hold keys on the config toggle like every
  Firefighter gate, so a normal-mode pull with the toggle on also holds Revive Pet (findings doc, "Firefighter").
- The next window's `walks into the live bomb` cannot reach 0: the fix leaves `combat formation move` (19 in
  window 22, 34 → 29-32 yd, the unstacker the guard leaves alone) and reach legs starting past the guard's 38 yd (6:
  `reach spell` 40 → 24, `reach party member to heal` 43 → 28, `reach melee`, the core carrier). Judge the fix by
  `mimiron arc spread action` walk-ins near 0 and the main tank's 1-3 escapes a bomb.
- `none bomb` cannot be counted: `NoteDerived` writes a `mimiron.approach` value only when it differs from the bot's
  last one, and a direct leg writes none, so most bots log it once a pull. The main tank's anchor hold writes no
  note at all. Read the hold from the walk-ins.
- Removed the fixture comment the round added to `test_mimiron.py` (no-comments rule).

## Round 14 (build 02de71ee6, round 13 built)

**Data:** window 24a, `raidtest.py report --boss mimiron --raid uld --head 02de71ee6 --pulls 10`: one valid pull per
pool, no DC veto inside any. Baselines: windows 16-22 (`mim13/traces.txt`). Scripts in the lane scratchpad (`mim14/`:
`mtesc.py`, `p2storm.py`, `bursts.py`, `burstcasts.py`, `p2cone.py`, `offslot.py`, `postbarrage.py`, `rhscene.py`,
`approach_replay.py`, `replay_batch.py`, `subst_replay.py`, `bombleg_replay.py`, `wedgefire.py`, `pulses.py`,
`healrange.py`, `track.py`, `revcasts.py`, `clock.py`, `alive.py`; round 13's `bombcharge.py`, `p4bombs.py`,
`walkins.py`, `revtime.py`, `hm.py`).

| Pool | Run | Trace | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261123-213557-1` | `603_1_…_1795469817` | wipe 6:19.3 in P3, raid dead | 32: P1 Napalm Shell 1; P2 Rapid Burst 9, Heat Wave 4, Flames 4 (three ranged in 0.1 s at 3:26.5, at 4-36% walking fire dodges); P3 (11 alive, one healer) Bomb Bot 4, Junk Bot 4, Assault Bot 3, ACU Plasma Ball 2, Water Spray 1 |
| uld2 | `tr-20261123-213557-2` | `603_2_…_1795469819` | **kill** 10:16.3, hard mode | 5: Flames 2 (two ranged at 6-8% walking the second barrage dodge on P2's last second, 3:58.6-3:59.6); P3 Bomb Bot 1; P4 Hand Pulse 2 (a battle-rezzed ranged at 36%, a melee after three straight pulses on the melee) |
| uld3 | `tr-20261123-213557-3` | `603_3_…_1795469820` | **kill** 9:34.5, hard mode, no Berserk cast | 5: P2 Heat Wave 1, Rapid Burst 1 (one ranged, revived between); P4 Hand Pulse 3 (two melee on one Shock Blast escape point at 8:51.4, one of them again at 8:58.8) |

42 deaths: phase 2 lost on healing 21 (uld1 17, uld2 2, uld3 2) and uld1's phase 3 after it 14, P4 Hand Pulse 5, Bomb
Bot 1, Napalm Shell 1. Hard mode in all three: Summon Flames (Initial) at 7.1 s, Flames from 8.2-16.2 s, Emergency Mode
at phase 2. The first window with two Mimiron kills.

**Stop rules:** two kills of three, 5 deaths in each: not polished. No failure picked three times (the phase 2 pile is
new).

**Verdict on round 13.**

- **Walks into a live phase 4 bomb: held.** Non-escape walk-ins 0/2/10 (uld2) and 6/1 (uld3) a bomb, against 40/35/90 a
  pull; `mimiron arc spread action` 1 and 1 (window 22: 140, 59 of them the main tank's). The 10 at uld2's 9:54.8 bomb
  are all the Laser Barrage dodge's plain-step fallback; the rest `reach party member to heal` 3 and `combat formation
  move` 4, as the review expected. Gap-closers into a live bomb 1 (an Intercept from 40 yd, outside the guard's 38,
  landing at 29; lived) against 10 in window 22; no Frost Bomb death (2).
- **The main tank's anchor hold: held.** 1/1/1 and 1/2 escape legs a phase 4 bomb (12-22).
- **VX-001 over a fuse: mostly held.** 4/34/0 and 12/22 yd (window 22: 14/31/6, 10/23, 1/31/23). uld2's 9:09.6 bomb, 13
  yd off VX-001, took the stack 21 → 42 yd off centre on the main tank's single escape and cost 0.94M.
- **Bomb windows:** no death in the 20 s after a phase 4 bomb (window 22: 6); all five phase 4 deaths fell outside them.
  Hand Pulse there still 19.7k/s against 12.6k/s outside.
- **Revive Pet: held while engaged, not in the handovers.** No start in P1, P2 or P4 and 35 hold vetoes, but roster B's
  hunters started it 7 times in Z1 (2, cut by fire), Z2 (2), H1 (1) and H2 (2): 33 s (Uldyau) and 59 s (Uldyaj) on it
  against 175 s each; none finished. `MimironFireFight` needs a construct in combat, and none is in a handover. Fixed
  below. uld2's phase 2 ran 91.3 s (96.8).
- Round 12's hold: fire bots 0 at phase 4's open in both (the H3 cull took 7 and 4), the ACU on the off-tank 78% in both,
  Plasma Ball on ranged 3 and 4, no hunter death at the opening.

**The clock** (Berserk at 10:00):

| Pull | P1 | Z2+H1 | P2 | H2 | P3 | H3 | P4 reached | P4 |
|---|---|---|---|---|---|---|---|---|
| uld1 | 84.9 | 48.2 | **119.9**, 17 dead | 24.2 | 88.7, raid dead (11 alive, one healer, at its open) | | | |
| uld2 | 84.3 | 47.9 | 91.3 | 24.4 | 149.5 | 32.0 | 7:22.9 | 155.3 s at 95k/s, 22 alive; Berserk cast at 10:00.2 in the Self Repair rounds (P4 ended 9:58.2), hit nobody; kill 10:16.3 |
| uld3 | 84.3 | 48.2 | 92.2 | 24.4 | 120.4 | 32.2 | **6:55.1** | 138.8 s at 105k/s, 25 alive; kill 9:34.5 |

uld3's is the fastest phase 4 entry yet (window 21's uld1: 6:58.7). uld2 had 4 s to spare: phase 3 ran 29 s over uld3's.

**Why uld1 wiped at 6:23: the ranged and healers piled in one Rapid Burst cone after the first barrage.**

- At Spinning Up (2:56.8) the wedge re-aimed 0° → 176°, onto the phase 1 field west (14 of 32 nodes inside it; window
  22-24a re-aimed to 132-176° in five of six phase 2s). The ranged and healers, standing at 300-60°, ran the `ahead`
  exit clockwise and stopped on the first clean bearing past the band edge: 12 of 14 at 222-244° by 3:04.8, most within
  ±2° in two rows (18 and 24 yd).
- Nothing spread them. At 3:12 (beams off) 8 of their 14 slots burned and 13 of 14 direct legs crossed 2-12 nodes
  (`approach_replay.py`); `GetMimironSlotApproaches` returns nothing under `ULDUAR_MIMIRON_SLOT_STRANDED_DISTANCE` (30)
  when fire refuses every leg, and writes no note. The three bots past 30 yd took `stranded` least-burn legs into the
  fire and the barrage `refuge` walked them back every ~0.5 s: 56 `arc spread <-> p3wx2 laser barrage` flips (uld2 5,
  uld3 3).
- The Frost Bomb (3:14.6, 31 yd from the pile) sent nine of them to one point (2731-2735, 2550-2554), and from 3:17 to
  the blast they wrote `none bomb`. The bomb rule held none of them alone: at 3:19 no off-slot ranged or healer had a
  fire-clean leg (`bombleg_replay.py`), so round 13 is not implicated; `none bomb` is written whenever the direct leg
  fails the bomb test, fire or not. The fire dodges at 3:22.5-3:23.6 then moved the pile as one, to (2723-2726,
  2546-2549).
- Cone-mates of a ranged or healer (others inside its 60° cone off VX-001, 2 s samples) ran 8.0-9.6 from 3:04.8 to
  3:13 and 6.9-8.3 to 3:24, against a phase 2 mean of 3.5-5.0 in every other pull of windows 19-24a. The first burst after the beams (3:11.4) hit 12 ranged and healers
  plus a tank for 100k, the 3:24.0 one 11 of them for 100k; Heat Waves at 3:21.5 and 3:26.5 then killed three ranged in
  fire at 3:26.5 and the healers fell to the next bursts (3:27.1, 3:29.6, 3:38.4, 3:40.7). Phase 2 ran 120 s with 17
  dead, and phase 3 opened with 11 alive and one healer.
- uld2 had the same re-aim (174°) with 11 nodes in it, but only 2 of 14 ranged and healers had no clean leg and they
  were on their slots by 3:08: 0 phase 2 deaths before its last second.

**Classification: plan change.** Every rule did what it intends: the barrage forces the wedge into the 117-126° safe
sector, the slot approach never walks a bot through fire, the refuge stops a bot on the first clean bearing. Getting the
ranged off a burning wedge needs a new position rule: Q6.

**Fixed, small and independent: `mimiron revive pet hold` covers the whole fight.** It now keys on
`MimironFirefighterLive` (the toggle and `BossStateGateLive(ULD_BOSS_MIMIRON)`, boss in progress) instead of
`MimironFireFight` (a construct in combat), so the opening and every handover hold too. One code path: the block runs in
bot engines only, 10 and 25 alike; as before, a normal-mode pull with the toggle on holds it as well.

Findings doc: the pet hold's new scope, the burning re-aimed wedge as an open item under "A burning band edge", and the
`none bomb` caveat in the trace key table.

**Next window should show** (`mim14/revcasts.py`, `mtesc.py`, `postbarrage.py`, `p2cone.py`; `--bomb`):

- No `revive pet` cast by roster B's hunters from the pull to its end, Z and H spans included (window 24a: 7), and
  `mimiron revive pet hold` vetoes in those spans.
- Round 13's checks still: phase 4 `mimiron arc spread action` walk-ins near 0, the main tank 1-2 escape legs a bomb, no
  Frost Bomb death.
- The phase 2 pile is untouched until Q6 is answered: expect cone-mates after a barrage of 4-6, and a pile like uld1's
  whenever the re-aimed wedge burns.

**Not picked.**

- P4 Hand Pulse 5, both kills: streaks of three pulses on the melee sector. uld3's two melee stood on one Shock Blast
  escape point 20 yd out, (2766, 2589), healers 29-46 yd off, and took 7-8k a pulse against 3.1k of healing between
  them; uld2's melee had healers 26-56 yd off and 1.6k of healing in its last 5 s. Next candidate: healer reach to the melee after a phase 4
  Shock Blast escape.
- uld2's 9:09.6 bomb moving the stack 34 yd; Bomb Bot 1; uld1's phase 3 deaths (11 alive at its open).

**Questions for the user.**

- **Q6 (plan change): where ranged stand when the barrage re-aims the wedge onto fire.** The re-aimed wedge stays put
  for the rest of phase 2, usually on the phase 1 field west. When its slots burn, the ranged and healers pile at the
  band edge in one burst cone and every dodge moves the pile, which lost uld1 its phase 2 (17 dead). Options: (a) once
  the beams stop, re-aim the wedge to the 120° whose slots hold the fewest burning nodes, ties nearest the current
  centreline, the handover band's pick: uld1 would have taken 285-300° (0 nodes, 52-67° from the pile); (b) keep the
  wedge and give a bot with no approach the nearest clean spot on its own bearing slice. Recommendation: (a). It reuses
  a tested pick and walks the pile ~20 yd, against a walk inside the post-barrage storm (Heat Wave, Rocket Strike and
  the Frost Bomb within 0.5-4 s) and fire chains drawn toward a second sector.

**Shared code:** none (Mimiron definition, findings doc, this log).

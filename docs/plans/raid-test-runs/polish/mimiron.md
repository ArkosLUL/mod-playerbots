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

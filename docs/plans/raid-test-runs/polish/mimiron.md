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

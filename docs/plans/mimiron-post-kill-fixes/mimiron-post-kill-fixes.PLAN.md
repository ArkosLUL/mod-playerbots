# Mimiron: fix what the 2026-08-31 kill trace shows

## Context

Mimiron died on 2026-08-31 (`env/dist/logs/botobs/603_1_mimiron_1788202403.ndjson`, kill at 7:52) but
18 members died on the way. Three of those are the human tank Dragon eating Shock Blast, with zero
`act` and zero `move` records against its guid — no bot AI was attached, so they are the player's own
and nothing here changes them. The other 15 are bots:

| Killer | Bot deaths | Cause |
|---|---|---|
| P3Wx2 Laser Barrage 63293 | **12** | the 4 s telegraph is invisible to the strategy |
| Bomb Bot 63801 | 2 | the DPS list and the sidestep each defer to the other |
| Plasma Blast 64529 | 1 | warrior held aggro off the human tank 23 s into the pull |

Separately, the user reported ranged DPS unable to reach the Aerial Command Unit while a Magnetic Core
had it grounded. The trace confirms it and gives a different cause than the one suspected.

This is the first Mimiron pull traced through RaidObs (commit `d0d0ccd7d`), and every finding below
came out of the note, hazard and cast streams rather than from watching. No rerun was needed.

---

## B1 — The Laser Barrage telegraph never fires (12 deaths)

**What the trace says.** VX-001 casts Spinning Up at 156.74, 351.31 and 411.50; the barrage lights at
160.90, 355.41 and 415.60 — 4.16 s later, every time. But the first `mimiron.barrage` note and the
first `haz` sweep row in each window land *after* the beams are already live, and `live` is `0.0` on
all 112 hazard rows. Elemena's first dodge note is at 356.08; it dies at 356.12.

**Why.** [UldBossHelper.cpp:3321](../../../src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L3321) and
[UldTriggers_Mimiron.cpp:93](../../../src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.cpp#L93) both test
`vx001->HasAura(SPELL_SPINNING_UP)`. **Spinning Up leaves no aura on VX-001.** Spell 63414 is
`ImplicitTargetA_1 = 38`, and the condition rows live in `acore_world` (DB update `2026_08_22_02`, all
four `RELEASED`) send effect 0 to the DB Target 33576 and effect 1 to the MK II 33432; effect 2 has no
effect at all. On VX-001 it exists only as a **channel** — `AttributesEx` bit `0x4`, DurationIndex 35
= 4000 ms — which is exactly why
[boss_mimiron.cpp:1450](../../../../../src/server/scripts/Northrend/Ulduar/Ulduar/boss_mimiron.cpp#L1450)
polls `FindCurrentSpellBySpellId` rather than `HasAura`.

The whole predictive model — `untilLive`, the ignition-lead extrapolation, the frozen-band
`sweepRate = 0` branch in the dodge — is already built, already correct, and unreachable. Reaction
time is ~0.1 s instead of 4.1 s.

Checked against the three window-2 victims' own recorded `cw` and radius, using the existing 40°
stepped legs at 7 yd/s: Shadow needs 1.4 s, Totemist 3.0 s, Elemena 3.2 s. All three fit inside 4.1 s.
**The dodge does not need changing — only waking up.**

**Fix.** One new helper, so the trigger and the window cannot drift apart:

```cpp
// Seconds until the Laser Barrage ignites, or -1 when VX-001 is not spinning up. Spinning Up is a 4 s
// channel and leaves no aura on VX-001: 63414's effect 0 lands on the DB Target and effect 1 on the
// MK II, so HasAura never answers true and the whole telegraph reads as nothing at all.
float GetMimironSpinningUpSeconds(Unit* vx001);
```

- Body: `FindCurrentSpellBySpellId(SPELL_SPINNING_UP)`, then `GetCastTimeRemaining() / 1000.0f`
  clamped at 0 — the channel timer is decremented before the tick that ends it, and a negative
  `untilLive` would predict the ignition backwards. `Spell.h` is already included in
  `UldBossHelper.cpp`; the trigger only needs the declaration it already gets from `UldBossHelper.h`.
- `GetMimironBarrageWindow`: take `untilLive` from it when it answers `>= 0`, else the existing aura
  branch, else return invalid.
- `MimironP3Wx2LaserBarrageTrigger::IsActive`: same test in place of `HasAura(SPELL_SPINNING_UP)`.
  This also extends `MimironLethalWindowActive` — and so the gap-closer veto and the arc spread
  standing down — across the windup, which is what those were written for.
- `IsMimironSpotBarrageSafe`: use `rate = 0` while `untilLive > 0`, matching the dodge's own
  `sweepRate`. `sweep` already spans the entire 10 s fire during the windup, so growing it again
  double-counts by up to 33° and refuses bearings that are clear.

---

## B2 — The phase-3 ACU tank has no anchor, so the core grounds off centre (user-reported)

**What the trace says.** `mimiron.core` brackets two grounded windows (249.07–269.24 and
274.42–289.52). The ACU sat at (2732.9, 2557.5) from 244.2 to 289 — 16.8 yd off
`ULDUAR_MIMIRON_ROOM_CENTER`. Sampling every raid member's distance to it across that span:
**7 to 12 of 25 beyond `spellDistance` (28.5 yd) in every sample**, the same names throughout —
Stormweaver, Fel, Hellflame, Trueshot, Nightwarrior (ranged DPS) and Tree, Prayer, Elemena, Holylight
(healers), out to 42 yd.

**The core lands under the ACU, not under whoever places it.** 64444 is `SPELL_EFFECT_SUMMON` of
34068 with `ImplicitTargetA_1 = 46` (`TARGET_DEST_NEARBY_ENTRY`) and a condition row in `acore_world`
requiring the Aerial Command Unit (33670/34109); `spell_mimiron_magnetic_core_summon::ModDest` only
drops Z to the floor. The trace confirms it twice: both Magnetic Core NPCs spawned at **d = 0.00** from
the ACU's x/y, and the ACU descended vertically — z 379.31 → 378.93 → 364.31 with x/y unchanged.
Justice used the first core standing 12 yd away and the ACU did not shift. So the carrier's position
cannot move the landing spot.

**What does set it: the tank.** The ACU hovers directly over its aggro target — `target=Bulwark` for
all of phase 3, `d = 0.0` whenever Bulwark stands still, including 20 s parked on the room centre. The
chain that broke it:

1. `236.41` — `mimiron bomb bot action` sends Bulwark to (2765.9, 2590.4), 29.7 yd off centre.
2. The ACU flies after it.
3. `240.31` onward — Bulwark runs `reach melee` back at the ACU: (2753.4, 2577.9) → (2746.2, 2570.7)
   → (2742.6, 2567.1) → (2739.1, 2563.6) → (2732.9, 2557.5).
4. They converge 16.8 yd off centre on the **opposite** side and both stop — Bulwark's move records
   read `r=dup` from 245.56 to 249.08, then nothing.
5. Forty-five seconds and two Magnetic Cores later, the ACU is still there.

**Why nothing pulled it back.** `DeriveMimironSpreadSlot` has a `p1tank` branch and a `p4tank` branch
and **no phase-3 tank branch**: with the ACU as focus a melee bot falls through to `p3wedge`, which
returns false for anything not ranged. The ACU tank therefore holds no slot and only ever chases. That
is the same failure `p1tank`'s own comment already describes for the MK II — *"Nothing else brings the
MK II back … over a five minute phase that walks the fight round the room until half the raid is out of
casting range"* — one phase later, with a Bomb Bot sidestep instead of a Shock Blast flee as the
displacement.

**Fix.** Add the missing branch, mirroring `p1tank`, before the wedge:

```cpp
// Phase 3 tank spot, and the reason is the Magnetic Core rather than the tanking. The unit hovers
// directly over whoever holds it and the core summons under the unit, so wherever the tank is standing
// when the core lands is where the raid spends the next 20 s. Left to chase, the two converge wherever
// a Bomb Bot sidestep happened to leave them: one kill grounded it 16.8 yd off centre and put 7 to 12
// of 25 past casting range for both windows.
if (PlayerbotAI::IsMainTank(bot) && focus->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
{
    branch = "p3tank";
    out = ULDUAR_MIMIRON_ROOM_CENTER;
    return true;
}
```

The wedge, its rows, spacing, min radius and staging-point centreline all stay exactly as they are —
they are correct once the ACU is where they assume it is. One bot moves instead of ten, and the fix is
at the cause. It also closes a blind spot in the trace: the ACU tank currently emits no `mimiron.slot`
note at all, so "has no slot" and "refused its slot" read identically; with the branch it emits
`p3tank` and the next pull says which.

Ordering note: this must sit **above** the `staging && !IsRanged` melee branch's sibling checks in the
same way `p1tank` does — after the staging melee branch, before `p3wedge` — or a tank in a handover
loses its staging slot.

---

## B3 — Bomb Bot: two rules that each defer to the other (2 deaths)

**What the trace says.** One Bomb Bot, 66406 HP, chased down from 95 % to 18 % and detonated at 240.5,
killing Power at 2.48 yd and Agony at 2.42 yd — both from full health, 22960 and 22918 damage (63801,
base points 23562, blast radius 5 yd). Neither has a `mimiron bomb bot action` in its act stream;
`avoid aoe` came back USELESS. `mimiron.dpsrule` shows both on `bombbot` at 236.2/236.8 and switched
off to `assaultbot` by 238.3 — they stopped shooting it as it closed.

**Why.** `MimironBombBotTrigger::IsActive` stands ranged DPS down whenever a Bomb Bot is within spell
range, on the premise that they shoot it instead. `MimironSetDpsPriorityAction::IsAllowedTarget` drops
the Bomb Bot once it is *closer than* `ULDUAR_MIMIRON_BOMB_BOT_RADIUS` (8 yd), on the premise that the
sidestep owns the bot there. Inside 8 yd a ranged DPS therefore does neither.

**Fix** (per the answered scope question — split on the chase target):

```cpp
// The Bomb Bot chasing this bot, if any. It runs 8.0 yd/s against a player's 7.0 and detonates on
// contact, so the one it is after cannot leave and has to shoot it down; anyone else inside the blast
// can step out and should. Both the DPS list and the sidestep read this, because each used to defer
// to the other and a ranged bot inside 8 yd did neither.
Unit* GetMimironBombBotChasing(PlayerbotAI* botAI, Player* bot);
```

Walks `nearest npcs` for `NPC_BOMB_BOT` whose `ServerFacade::instance().GetChaseTarget` is this bot —
the same lookup `GetMimironBombBotApproach` already does. Then:

- `IsAllowedTarget`: allow the Bomb Bot inside the radius when it is the one chasing this bot.
- `MimironBombBotTrigger`: stand ranged DPS down only while chased, replacing the
  `FindNearestCreature(..., spellDistance)` test that was swallowing bystanders.

---

## B4 — Phase 4 target flapping

`GetMimironPhase4Focus` picks `highest(allowed)` on raw `GetHealthPct()`. The trace shows MK II and
VX-001 tracking within 0.2 % of each other for the whole of phase 4 at ~0.85 %/s, crossing constantly:
**1380 of 1564 `mimiron.dpsrule` notes are a `vx001` ↔ `mkii` flip**, roughly five per bot per second,
each one resetting a swing or a cast and re-triggering `set behind`. No death traces to it directly;
Mighty and Justice died mid-`set behind` at 6:01 and 6:03 while the beams were up.

**Fix.** Compare a health *band* rather than the raw percent, tie-broken by the fixed entry iteration
order:

```cpp
// Health band for the phase 4 focus. Twenty-five bots burn two parts down within a tenth of a percent
// of each other, so a bare "highest health" comparison hands every bot a new target several times a
// second. Two percent is about 2.4 s of raid damage, and five bands still fit inside the hold margin.
constexpr float ULDUAR_MIMIRON_PHASE4_FOCUS_BAND_PCT = 2.0f;
```

Derived from state alone, so every bot still reaches the same answer — which the existing comment
requires, since the tank node, the DPS node and the pets all share this helper. The `levelled` and
`aboveFloor` tests keep comparing raw percent against `ULDUAR_MIMIRON_PHASE4_HOLD_PCT`: that is a
one-way floor in a burn and does not flap.

---

## B5 — `mimiron.barrage` probe noise

The note carries `cw` in whole degrees and the radius, so the text changes every tick and
`NoteDerived`'s change test never suppresses anything: 2300 notes, ~3.8 per bot per second across only
29 s of live barrage. Bucket `cw` to 15° and drop the radius — `snap.u` already samples the bot's
position four times a second and the hazard rows carry the apex, so the radius is derivable. Keeps the
band information, collapses the stream by roughly a factor of ten. Signature loses its `radius`
parameter.

`mimiron.carrier` needs nothing: it writes a bare guid, and `postmortem.py`'s `note_text` joins that to
a name on read.

---

## Not fixed, and why

- **The three Shock Blast deaths.** All on Dragon, zero `act` and zero `move` records — a human player,
  not the strategy.
- **The Plasma Blast death at 0:23.** Angry (warrior) took five MK II melee swings alongside two Plasma
  Blasts, so it was holding aggro off the human tank at the pull. A threat problem on the human's side,
  not a positioning one.
- **The flee fan's `fallback` rate.** 260 `shock fallback` and 77 `rocket fallback`, but the counters
  show the filters refusing at most 4 of 11 bearings and `cone0` throughout — the rest is `MoveTo`
  refusing while the bot's own `MOVEMENT_FORCED` leg is still in flight, which is the gate working. It
  mislabels a contested tick as a refusal; worth relabelling one day, not a defect.

## Files touched

- `src/Ai/Raid/Uld/Util/UldBossHelper.cpp` / `.h` — B1 helper and window, B1 `IsMimironSpotBarrageSafe`,
  B2 `p3tank` branch, B3 chase helper, B4 band constant and comparison
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.cpp` — B1 trigger, B3 sidestep trigger
- `src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp` / `.h` — B3 `IsAllowedTarget`, B5 note
- `docs/raids/ulduar.md` — the findings, per the raid-findings convention. Run `/compact-docs-writer`
  before editing it, up front rather than as cleanup.
- `docs/plans/mimiron-post-kill-fixes/mimiron-post-kill-fixes.PLAN.md` — mirror of this plan.

No new node, trigger or strategy wiring. No schema bump. The cheats-free constraint stands: no
`HasCheat`, no `TeleportTo`, no `->Kill(` anywhere in the Mimiron files.

## Verification

The module cannot be compiled from this checkout — the worldserver build is the only compile path, so
a rebuild has to come first and nothing below can run without it.

Static, before handing it over: brace and paren balance against HEAD with comments and string literals
stripped; every new helper declared and defined; no stale call to the old `GetMimironPhase3Slot` or
`NoteBarrageDecision` signatures; the cheat grep clean.

Then one normal pull and one Firefighter pull, read with `postmortem.py`:

1. **B1 is the headline.** `--notes mimiron.barrage` must show decisions starting ~4 s *before* the
   first `P3Wx2 Laser Barrage 63293` damage record, and `haz` rows carrying `live` counting 4.1 → 0
   with `sp` reading `Spinning Up 63414` for those rows. If `live` is still `0.0` throughout, the
   channel test is not matching and nothing else in B1 matters.
2. **Barrage deaths.** Zero, or a bot that started its dodge on time and still lost — which would be a
   step-size finding, not this one.
3. **B2.** `mimiron.slot` shows `p3tank` for the ACU tank through phase 3, the ACU's sampled position
   stays within a few yards of `ULDUAR_MIMIRON_ROOM_CENTER`, and no roster member is past 28.5 yd of
   it during a `mimiron.core` grounded window. Sample `snap.u` against the ACU the way this analysis
   did. The Bomb Bot sidestep will still displace the tank — what matters is that it comes back.
4. **B3.** A Bomb Bot detonation with no death, and `mimiron.dpsrule` holding `bombbot` all the way in
   for the chased bot while a bystander inside 8 yd shows a `mimiron bomb bot action` move record.
5. **B4.** `mimiron.dpsrule` in phase 4 down from ~1380 flips to tens, and the parts still reaching
   `ULDUAR_MIMIRON_PHASE4_HOLD_PCT` together rather than one being pushed under early.
6. **B5.** `mimiron.barrage` note count down by roughly a factor of ten, branch changes still legible.
7. **No behaviour regression elsewhere.** Compare a `--track` of two or three bots against this trace:
   B2 and B4 move bots on purpose, B1 moves them earlier; anything else that moved is a bug.
8. Cheat grep still clean.

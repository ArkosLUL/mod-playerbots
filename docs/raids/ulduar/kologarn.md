# Kologarn

**No raid target icons.** Skull means "everyone DPS this" and Moon is the CC channel, so a per-role
split built on them leaks into the generic engine. Targets are picked in code per role, the SWP
Eredar Twins model — which *requires* `KologarnDisableAutomaticTargetingMultiplier`, because
`DpsTargetValue::Calculate` falls back to a smart-target strategy when no icon is set and is
therefore **never null**: `NotDpsTargetActiveTrigger` stays true and `dps assist` retakes the target
on alternating ticks. It zeroes `DpsAssistAction`, `TankAssistAction` and
`CastDebuffSpellOnAttackerAction` — the last is what stops DoTs landing on whatever the bot drifted
onto.

| Role | Target |
|---|---|
| Body tank (`kologarn->GetVictim()`) | the body, held in melee |
| Off-tank (the other of MT / AT0) | rubble while any live; else the right arm, but only inside 30 yd taunt range of the body |
| Melee DPS | right arm while it lives, else the body |
| Ranged DPS (`IsRangedDps`, excludes healers) | rubble while any live, else as melee |

Rubble duty is **derived, never stored** — the off-tank is whoever is not holding the body — so it
follows the taunt swap on its own.

**Nothing engages before the boss does.** Every Kologarn trigger resolves him through
`GetFirstAliveUnitByEntry`, a pure `AiPlayerbot.SightDistance` proximity scan — 100 yd, no line of
sight — that answers "is he nearby", never "is he engaged", and `AttackAction::Attack()` has no
out-of-combat guard, so the raid used to pull itself from up to 100 yd out. `KologarnEncounterActive`
is `kologarn && kologarn->IsInCombat()`, the VoA `EmalonEncounterActive` shape, and gates the triggers
and the targeting multiplier alike. Resolving by entry rather than through `find target` — which walks
only the bot's own threatened-by-me list and therefore cannot fire before engagement — is deliberate:
a tank parked on the body never has the arms on its threat list, so the per-role focus split
collapses without it.

## Facts that contradict the retail guides

| | |
|---|---|
| Arm respawn | **50s**, not 60 |
| Crunch Armor | **63355** (−20%, 4 stacks, 45s). **64002 is never applied here**, so the old cheat checking it was dead code |
| Stone Grip | 62166 / 63981, **1 target**, caster's victim stripped → **the body tank is exempt** |
| Focused Eyebeam | 3 most distant players via `NonTankTargetSelector`, then **exactly one** at random → one runner, never the tank. Eye lives **10s**, chases at **5.5 yd/s** against a player's 7.0; beam is a **3 yd** AoE |
| Rubble | 8.0 yd/s — **faster than players, so held, not kited**. SmartAI: Rumble 63818 (10) / **Stone Nova 63978 (25, 10 yd, ~5550 + knockback)** |
| Shockwave (63783) | **200 yd**, nothing narrows it |
| Petrifying Breath | fires only when the body's victim is out of melee **and** `SelectNearbyTarget` finds nobody close — any body in melee suppresses it |

That tank exemption from both Grip and Eyebeam is what stops "body tanked at all times" and "run from
the eyebeam" ever conflicting.

Boss at `(1797.15, -24.40, 448.74)`, `o≈π`: **entrance is -X**, arms split along **Y**. The walkway
runs from the Shattered Walkway Door (x 1740.84) to the broken span at x 1782, past which
`boss_kologarn_pit_kill_bunny` instakills inside x 1782–1832 / y -56…8 / z 400–439.

## Mechanics

**Tank swap** at 2 Crunch Armor stacks, incoming tank holding **strictly fewer** — not an absolute
cap, which deadlocks both tanks at 2 and stops swapping for good, since 45s duration against a 14s
Smash timer never lets stacks clear. Equal stacks correctly means hold.

**Rubble** are held **laterally**, toward the dead arm's side, ~18 yd off the raid — never backward:
-X is the eyebeam escape lane. Ranged AoE falls out of the role table plus the engine's `"aoe count"`
thresholds, so no AoE action exists.

**Focused Eyebeam** is a real run, not a teleport, re-stepped each tick: -X toward the entrance, then
along the walkway, then back toward the boss. Turning that corner matters — the eye outlives the ~5s
of -X runway. Bystanders `FleePosition` off it. None of it needs the raid cheat.

**Petrifying Breath** gets its own `ACTION_EMERGENCY` guard sending the nearest tank, then nearest
melee, into melee range whenever the body is uncovered. The swap handover is already covered by the
swap action's `Attack`; the case that wipes is the MT dying while the off-tank sits 18 yd out.

**Stone Grip** victims are stunned passengers, so `KologarnMultiplier` zeroes their movement — orders
only fight the ride. Freeing them needs no code: DPS already focus the arm.

Healers need nothing boss-specific: `PartyMemberToHeal` does not filter vehicle passengers, so
gripped victims are already picked up. Do **not** reach for `"focus heal targets"` — it is an
*exclusive* filter and would starve the rest of the raid.

Nature resistance is wanted, since Shockwave and Petrifying Breath are both Nature. Only the first
alive hunter raises it, and Aspect of the Wild 49071 is `APPLY_AREA_AURA_RAID` +
`MOD_RESISTANCE_EXCLUSIVE`, so a second adds nothing. Limit: 30 yd radius.

## Not implemented, deliberately

- **Left arm** — dies to incidental cleave. Focusing it doubles rubble spawns and risks a
  both-arms-down Stone Shout window when the two 50s respawn timers drift into phase.
- **Shockwave** — 200 yd hits the platform wherever anyone stands: a healing check, not a dodge.
- **Raid formation** — no mechanic needs it, and fixed offsets on a narrow walkway over an instakill
  pit is where bots fall in.
- **Fall-from-floor teleport kept** — a pathing workaround, not a mechanic: a bot under the walkway
  is in the kill box and dies within a second, so there is no walk-back to attempt.


# Algalon — Big Bang Shadow Priest tactic

## Request
Only one bot should "tank" Big Bang: a Shadow Priest, mitigating via **Dispersion** (90% damage reduction).

## Current behavior (as of investigation)
- `AlgalonBigBangTrigger::IsActive()` — `UldTriggers.cpp:2165`. Fires for **every** bot while Algalon channels
  `SPELL_ALGALON_BIG_BANG` (64443), unless the bot already has `SPELL_ALGALON_BLACK_HOLE_DAMAGE` (62169 = safe/phased).
- `AlgalonBigBangHideAction::Execute()` — `UldActions.cpp:3062`. Every bot runs into the nearest
  `PB_NPC_BLACK_HOLE` (32953) / `PB_NPC_WORM_HOLE` (34099) to gain the safe-phase aura.
- Wired at `UldStrategy.cpp:324` — `TriggerNode("algalon big bang trigger", { NextAction("algalon big bang hide action", ACTION_EMERGENCY + 1) })`.

## Building blocks available
- `dispersion` action (`CastDispersionAction`, self-cast) — `PriestActions.h:175`. Registered in
  `PriestAiObjectContext.cpp:202`. Called via `botAI->DoSpecificAction("dispersion", ...)`.
- Shadow-spec check: `AiFactory::GetPlayerSpecTab(bot) == PRIEST_TAB_SHADOW`.
- Encounter gate already in place: `find target "algalon the observer"` alive check used by every Algalon trigger.

## Mechanic note
Big Bang is raid-wide lethal shadow damage to anyone NOT phased into a hole. A single Dispersing
Shadow Priest survives its own damage but does NOT protect the rest of the raid — so the design hinges
on what the *rest* of the raid should do (see open question).

## Open question
How should the raid split during Big Bang?
- (A) Shadow Priest Disperses in place; **everyone else still hides** in a hole (change = SP is exempted from hide, added Dispersion).
- (B) Only the Shadow Priest reacts at all (Disperse); no one hides. (Others would take lethal damage — likely not intended.)

## Plan (pending answer)
1. Add `SPELL_DISPERSION` id + Shadow-Priest predicate to `UldBossHelper.h`.
2. New `AlgalonBigBangDispersionTrigger` / `...DispersionAction` (or extend existing) — fires only for a designated Shadow Priest, casts `dispersion`.
3. Under option (A): exclude the designated Shadow Priest from `AlgalonBigBangHideAction` (add SP-exempt check to the hide trigger/action).
4. Wire trigger+action in `UldStrategy.cpp`, `UldActionContext.h`, `UldTriggerContext.h`, `UldTriggers.h`, `UldActions.h`.
5. Normal + heroic: Big Bang spell id 64443 is difficulty-agnostic; Dispersion is a bot spell — no difficulty split needed.

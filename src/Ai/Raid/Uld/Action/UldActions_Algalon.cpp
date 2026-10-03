#include "UldActions_Algalon.h"

#include <cmath>
#include <vector>

#include "AiObjectContext.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Position.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "Timer.h"
#include "UldData.h"
#include "UldEncounter_Algalon.h"
#include "Unit.h"

using namespace EncounterHelpers;

//
// Algalon the Observer
//

namespace
{
// A walk accepted while a cast holds the feet books the movement slot and goes nowhere until the cast
// ends. Only the runs that have to land in time call this.
void BreakCastPinningTheFeet(PlayerbotAI* botAI, Player* bot)
{
    if (bot->IsMovementPreventedByCasting())
        botAI->RequestSpellInterrupt();
}

// MoveTo refuses a point it already issued for MaxWaitForMove, moving or not, so a walk stopped short
// (a mana gem's StopMoving, Disengage) is never issued again unless the booking is dropped.
void ReleaseStalledWalk(PlayerbotAI* botAI, Player* bot)
{
    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    if (!bot->isMoving() && last.msTime && getMSTimeDiff(last.msTime, getMSTime()) > ULDUAR_ALGALON_STALL_MS)
        last.clear();
}

// Nearest ground clear of every marker and hole, near the bot's own slot when it has one. With
// `pathClear` the walk there must not clip another hole's field either: that phases the bot into the
// void Dark Matter, which is still better than a meteor or staying in the field, so callers fall back.
Position FindAlgalonClearSpot(Player* bot, float markerClearance, bool pathClear)
{
    Position anchor;
    Position const* prefer = GetAlgalonSlotAnchor(bot, anchor) ? &anchor : nullptr;
    auto const reachable = [bot, pathClear](float x, float y)
    { return AlgalonSpotInRoom(x, y) && (!pathClear || !AlgalonPathCrossesHole(bot, x, y)); };
    return FindNearestPositionClearOfHazards(bot, GetAlgalonDodgeHazards(bot, markerClearance),
                                             ULDUAR_ALGALON_DODGE_SEARCH_RADIUS, 2.0f, static_cast<float>(M_PI) / 8.0f,
                                             prefer, reachable);
}
}  // namespace

bool AlgalonBigBangSoakAction::Execute(Event /*event*/)
{
    // Dispersion's 90% survives it outright; the tank's own armour and a physical defensive do the rest.
    if (AlgalonCanDisperse(bot))
    {
        if (bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            bot->InterruptSpell(CURRENT_CHANNELED_SPELL);

        return botAI->CastSpell("dispersion", bot);
    }

    uint32 const defensive = NextTankDefensive(botAI, bot, "algalon.defensive", true);
    return defensive && botAI->CastSpell(defensive, bot);
}

bool AlgalonBigBangExternalAction::Execute(Event /*event*/)
{
    Player* soaker = GetAlgalonBigBangSoaker(botAI);
    if (!soaker)
        return false;

    if (!soaker->HasAura(SPELL_ALGALON_PAIN_SUPPRESSION) && botAI->CanCastSpell("pain suppression", soaker))
        return botAI->CastSpell("pain suppression", soaker);

    if (!soaker->HasAura(SPELL_ALGALON_GUARDIAN_SPIRIT) && botAI->CanCastSpell("guardian spirit", soaker))
        return botAI->CastSpell("guardian spirit", soaker);

    return false;
}

bool AlgalonBigBangHideAction::Execute(Event /*event*/)
{
    Position shelter;
    if (!GetAlgalonShelter(bot, shelter))
        return false;

    // Anywhere inside the field phases the bot on its next pulse.
    if (bot->GetExactDist2d(shelter) <= ULDUAR_ALGALON_SHELTER_RADIUS - 1.5f)
        return false;

    BreakCastPinningTheFeet(botAI, bot);
    ReleaseStalledWalk(botAI, bot);

    if (MoveTo(bot->GetMapId(), shelter.GetPositionX(), shelter.GetPositionY(), shelter.GetPositionZ(), false,
               false, false, false, MovementPriority::MOVEMENT_FORCED, true))
    {
        return true;
    }

    // MoveTo answers Duplicate for the rest of the run. Returning false there hands the tick to a heal
    // whose cast then pins the feet.
    return bot->isMoving();
}

bool AlgalonCosmicSmashAction::Execute(Event /*event*/)
{
    // FleePosition clamps to AiPlayerbot.FleeDistance (5 yd), inside the doubled damage band.
    char const* branch = "clear";
    Position clear = FindAlgalonClearSpot(bot, ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE, true);
    if (clear == Position())
    {
        branch = "min";
        clear = FindAlgalonClearSpot(bot, ULDUAR_ALGALON_COSMIC_SMASH_MIN_CLEARANCE, true);
    }

    if (clear == Position())
    {
        branch = "cross";
        clear = FindAlgalonClearSpot(bot, ULDUAR_ALGALON_COSMIC_SMASH_MIN_CLEARANCE, false);
    }

    if (clear == Position())
        branch = "none";

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "algalon.dodge", branch);

    if (clear == Position())
        return false;

    BreakCastPinningTheFeet(botAI, bot);
    ReleaseStalledWalk(botAI, bot);
    return MoveTo(bot->GetMapId(), clear.GetPositionX(), clear.GetPositionY(), clear.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_FORCED, true);
}

bool AlgalonLeaveBlackHoleAction::Execute(Event /*event*/)
{
    Position clear = FindAlgalonClearSpot(bot, ULDUAR_ALGALON_COSMIC_SMASH_TRIGGER_RADIUS, true);
    if (clear == Position())
        clear = FindAlgalonClearSpot(bot, ULDUAR_ALGALON_COSMIC_SMASH_TRIGGER_RADIUS, false);

    if (clear == Position())
        return false;

    BreakCastPinningTheFeet(botAI, bot);
    ReleaseStalledWalk(botAI, bot);
    return MoveTo(bot->GetMapId(), clear.GetPositionX(), clear.GetPositionY(), clear.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_FORCED, true);
}

bool AlgalonYieldBossAction::Execute(Event /*event*/)
{
    Unit* boss = GetAlgalon(botAI);
    if (!boss)
        return false;

    if (bot->GetVictim() == boss)
        bot->AttackStop();

    if (AI_VALUE(Unit*, "current target") == boss)
        context->GetValue<Unit*>("current target")->Set(nullptr);

    return true;
}

bool AlgalonTakeBossAction::Execute(Event /*event*/)
{
    Unit* boss = GetAlgalon(botAI);
    if (!boss)
        return false;

    if (boss->GetVictim() != bot && CastClassTaunt(botAI, boss))
        return true;

    // Taunt on cooldown or out of its 30 yd: walk in and hit him.
    if (AI_VALUE(Unit*, "current target") != boss)
        return Attack(boss);

    return false;
}

bool AlgalonConstellationTauntAction::Execute(Event /*event*/)
{
    Unit* constellation = GetAlgalonHandlerConstellation(bot);
    if (!constellation)
        return false;

    if (CastClassTaunt(botAI, constellation))
        return true;

    // Hits hold it until the taunt is back.
    if (AI_VALUE(Unit*, "current target") != constellation)
        return Attack(constellation);

    return false;
}

bool AlgalonDarkMatterTankAction::Execute(Event /*event*/)
{
    Unit* darkMatter = GetAlgalonLooseDarkMatter(bot);
    if (!darkMatter)
        return false;

    if (CastClassTaunt(botAI, darkMatter))
        return true;

    if (AI_VALUE(Unit*, "current target") != darkMatter)
        return Attack(darkMatter);

    return false;
}

bool AlgalonConstellationKiteAction::Execute(Event /*event*/)
{
    Unit* constellation = GetAlgalonHandlerConstellation(bot);
    Position spot;
    if (!constellation || !GetAlgalonKiteSpot(bot, constellation, spot))
    {
        _spotReached = false;
        return false;
    }

    float const distance = bot->GetExactDist2d(spot);
    if (_spotReached && distance > ULDUAR_ALGALON_SLOT_TOLERANCE * 2.0f)
        _spotReached = false;

    // Once parked the constellation walks itself into the hole, and the tick goes back to everything else.
    if (_spotReached || distance <= ULDUAR_ALGALON_SLOT_TOLERANCE)
    {
        _spotReached = true;
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "algalon.kite", "parked");
        return false;
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "algalon.kite", "hole");

    ReleaseStalledWalk(botAI, bot);
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_FORCED, true);
}

bool AlgalonStarTeamAction::Execute(Event /*event*/)
{
    Unit* star = GetAlgalonFocusStar(botAI);
    return star && Attack(star);
}

bool AlgalonStarMarkAction::Execute(Event /*event*/)
{
    Unit* star = GetAlgalonFocusStar(botAI);
    return star && MarkTargetWithStar(bot, star);
}

bool AlgalonRaidPositionAction::Execute(Event /*event*/)
{
    Position slot;
    bool waypoint = false;
    if (!TryGetAlgalonSlot(bot, slot, &waypoint))
    {
        _slotReached = false;
        return false;
    }

    float const distance = bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY());

    // A waypoint round a hole is never somewhere to hold: the next check hands out the next leg.
    if (waypoint)
    {
        _slotReached = false;
        if (distance <= ULDUAR_ALGALON_SLOT_TOLERANCE)
            return false;

        return MoveTo(bot->GetMapId(), slot.GetPositionX(), slot.GetPositionY(), slot.GetPositionZ(), false, false,
                      false, true, MovementPriority::MOVEMENT_COMBAT, true);
    }

    // Reach then hold, with a deadband. Re-issuing on every yard of drift restarts the spline, and a
    // moving bot can't start a cast.
    if (_slotReached && distance > ULDUAR_ALGALON_SLOT_TOLERANCE * 2.0f)
        _slotReached = false;

    if (_slotReached || distance <= ULDUAR_ALGALON_SLOT_TOLERANCE)
    {
        _slotReached = true;
        return false;
    }

    return MoveTo(bot->GetMapId(), slot.GetPositionX(), slot.GetPositionY(), slot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

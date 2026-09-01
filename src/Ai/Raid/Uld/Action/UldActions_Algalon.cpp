#include "UldActions_Algalon.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <cmath>
#include <vector>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "UldData.h"
#include "UldEncounter_Algalon.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

using namespace EncounterHelpers;

//
// Algalon the Observer
//

bool AlgalonResetEncounterStateAction::Execute(Event /*event*/)
{
    // One bot drops the whole instance entry; everyone else only lets go of its own latches, so a bot
    // that wandered out of the room cannot wipe state a raid that is still fighting depends on.
    ResetAlgalonEncounterState(bot, IsMechanicTrackerBot(bot, ULDUAR_MAP_ID));
    return true;
}

bool AlgalonBigBangHideAction::Execute(Event /*event*/)
{
    Unit* shelter = GetAlgalonShelter(bot);
    if (!shelter)
        return false;

    // Anywhere inside the field is enough, and stopping short of the centre saves a second of travel
    // out of an eight second cast.
    if (bot->GetExactDist2d(shelter) <= ULDUAR_ALGALON_SHELTER_RADIUS - 1.0f)
        return false;

    return MoveTo(shelter->GetMapId(), shelter->GetPositionX(), shelter->GetPositionY(),
                  shelter->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_FORCED,
                  true, false);
}

bool AlgalonBigBangSoakAction::Execute(Event /*event*/)
{
    // Big Bang cannot be immuned - Divine Shield does not stop it - only mitigated. Dispersion's 90%
    // reduction survives it outright; Guardian Spirit pays for it with its death save.
    if (botAI->CanCastSpell("dispersion", bot))
        return botAI->CastSpell("dispersion", bot);

    if (botAI->CanCastSpell("guardian spirit", bot))
        return botAI->CastSpell("guardian spirit", bot);

    // Nothing left in the raid's pocket. Standing still and taking it is deliberate: the spell needs
    // to find at least one target or Algalon ascends and evades, and a corpse beats a reset.
    return false;
}

bool AlgalonCosmicSmashAction::Execute(Event /*event*/)
{
    Unit* marker = GetAlgalonCosmicSmashMarker(bot);
    if (!marker)
        return false;

    // FleePosition would clamp this to AiPlayerbot.FleeDistance (5 yd), which lands inside the
    // double-damage band rather than clear of it.
    std::vector<Position> const impact = {marker->GetPosition()};
    Position const clear = FindNearestPositionClearOfHazards(bot, impact, ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE,
                                                            ULDUAR_ALGALON_COSMIC_SMASH_SEARCH_RADIUS);
    if (clear == Position())
        return false;

    return MoveTo(bot->GetMapId(), clear.GetPositionX(), clear.GetPositionY(), clear.GetPositionZ(), false,
                  false, false, false, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool AlgalonLeaveBlackHoleAction::Execute(Event /*event*/)
{
    std::vector<Position> holes;
    for (Unit* shelter : CollectAlgalonShelters(botAI))
        if (bot->GetExactDist2d(shelter) <= ULDUAR_ALGALON_SLOT_DISPLACE_RADIUS)
            holes.push_back(shelter->GetPosition());

    if (holes.empty())
        return false;

    Position const clear = FindNearestPositionClearOfHazards(
        bot, holes, ULDUAR_ALGALON_SHELTER_RADIUS + ULDUAR_ALGALON_SLOT_TOLERANCE,
        ULDUAR_ALGALON_SLOT_DISPLACE_RADIUS);
    if (clear == Position())
        return false;

    return MoveTo(bot->GetMapId(), clear.GetPositionX(), clear.GetPositionY(), clear.GetPositionZ(), false,
                  false, false, false, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool AlgalonPhasePunchSwapAction::Execute(Event /*event*/)
{
    Unit* boss = GetAlgalon(botAI);
    if (!boss)
        return false;

    if (AI_VALUE(Unit*, "current target") != boss)
        return Attack(boss);

    if (boss->GetVictim() != bot)
        return CastClassTaunt(botAI, boss);

    return false;
}

bool AlgalonConstellationTauntAction::Execute(Event /*event*/)
{
    Unit* constellation = GetAlgalonConstellationOnBossTank(botAI);
    if (!constellation)
        return false;

    if (CastClassTaunt(botAI, constellation))
        return true;

    // Between taunts, hold it with threat. Killing it is not the point - 20x base health against a
    // six minute enrage is not a fight anyone wins - but the hits are what keep it following once the
    // forced-attack window ends.
    if (AI_VALUE(Unit*, "current target") != constellation)
        return Attack(constellation);

    return false;
}

bool AlgalonConstellationKiteAction::Execute(Event /*event*/)
{
    Unit* constellation = GetAlgalonKiteTarget(bot);
    if (!constellation)
    {
        _spotReached = false;
        return false;
    }

    Unit* hole = GetAlgalonKiteHole(bot, constellation);
    if (!hole)
    {
        // No hole to spend, or the last one is being kept for Big Bang. Lead the constellation away
        // from the raid instead so its Arcane Barrage keeps landing on one bot.
        _spotReached = false;

        Player* crowd = GetNearestPlayerInRadius(bot, ULDUAR_ALGALON_KITE_CROWD_RADIUS);
        if (!crowd)
            return false;

        float const awayAngle = Position::NormalizeOrientation(crowd->GetAngle(bot));
        float const x = bot->GetPositionX() + std::cos(awayAngle) * ULDUAR_ALGALON_KITE_LEAD_DISTANCE;
        float const y = bot->GetPositionY() + std::sin(awayAngle) * ULDUAR_ALGALON_KITE_LEAD_DISTANCE;

        return MoveTo(bot->GetMapId(), x, y, bot->GetPositionZ(), false, false, false, true,
                      MovementPriority::MOVEMENT_COMBAT);
    }

    // Park past the hole on the side away from the constellation, so the chase drags it through the
    // phase field while the kiter stays outside. The spot converges as the constellation closes in,
    // because the bearing it is approached from swings round to the kiter's own.
    float const farAngle = Position::NormalizeOrientation(hole->GetAngle(constellation) + static_cast<float>(M_PI));
    float const x = hole->GetPositionX() + std::cos(farAngle) * ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET;
    float const y = hole->GetPositionY() + std::sin(farAngle) * ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET;

    float const distance = bot->GetExactDist2d(x, y);
    if (_spotReached && distance > ULDUAR_ALGALON_SLOT_TOLERANCE * 2.0f)
        _spotReached = false;

    // Yield once parked. The constellation walks itself into the hole from here, and everything below
    // this node - instants, heals, the class interrupts - gets its tick back.
    if (_spotReached || distance <= ULDUAR_ALGALON_SLOT_TOLERANCE)
    {
        _spotReached = true;
        return false;
    }

    return MoveTo(hole->GetMapId(), x, y, hole->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_FORCED, true, false);
}

bool AlgalonCollapsingStarFocusAction::Execute(Event /*event*/)
{
    Unit* star = GetAlgalonFocusStar(botAI);
    if (!star)
        return false;

    MarkTargetWithSkull(bot, star);
    SetRtiTarget(botAI, "skull", star);
    return true;
}

bool AlgalonDarkMatterTankAction::Execute(Event /*event*/)
{
    Unit* darkMatter = GetFirstAliveUnitByEntry(botAI, PB_NPC_UNLEASHED_DARK_MATTER);
    if (!darkMatter)
        return false;

    if (darkMatter->GetVictim() != bot && CastClassTaunt(botAI, darkMatter))
        return true;

    if (AI_VALUE(Unit*, "current target") != darkMatter)
        return Attack(darkMatter);

    return false;
}

bool AlgalonDarkMatterMarkAction::Execute(Event /*event*/)
{
    Unit* darkMatter = GetFirstAliveUnitByEntry(botAI, PB_NPC_UNLEASHED_DARK_MATTER);
    if (!darkMatter)
        return false;

    MarkTargetWithSkull(bot, darkMatter);
    SetRtiTarget(botAI, "skull", darkMatter);
    return true;
}

bool AlgalonRaidPositionAction::Execute(Event /*event*/)
{
    Position slot;
    if (!TryGetAlgalonSlot(bot, slot))
    {
        _slotReached = false;
        return false;
    }

    // A meteor is inbound on the slot. Standing aside costs a few seconds of formation; walking back
    // under it costs 41437.
    if (AlgalonCosmicSmashMarkerNear(botAI, slot, ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE))
    {
        _slotReached = false;
        return false;
    }

    float const distance = bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY());

    // Reach then hold, with a deadband. Re-issuing a move on every yard of drift restarts the spline,
    // and a moving bot cannot start a cast - it slides on the spot and never casts.
    if (_slotReached && distance > ULDUAR_ALGALON_SLOT_TOLERANCE * 2.0f)
        _slotReached = false;

    if (_slotReached || distance <= ULDUAR_ALGALON_SLOT_TOLERANCE)
    {
        _slotReached = true;
        return false;
    }

    return MoveTo(bot->GetMapId(), slot.GetPositionX(), slot.GetPositionY(), slot.GetPositionZ(), false,
                  false, false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

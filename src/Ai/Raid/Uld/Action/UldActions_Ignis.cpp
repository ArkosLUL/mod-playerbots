#include "UldActions_Ignis.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <cmath>

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
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

bool IgnisConstructTankAction::isUseful()
{
    IgnisConstructTankTrigger ignisConstructTankTrigger(botAI);
    return ignisConstructTankTrigger.IsActive();
}

bool IgnisConstructTankAction::Execute(Event event)
{
    Unit* construct = GetIgnisDrivenConstruct(botAI, bot);
    if (!construct)
        return false;

    if (AI_VALUE(Unit*, "current target") != construct)
        return Attack(construct);

    // Molten resets the construct's threat table, so aggro is re-checked before every leg of the walk
    // or the construct stops following halfway to the water.
    if (construct->GetVictim() != bot)
        return botAI->DoSpecificAction("taunt spell", event, true);

    if (IsIgnisConstructMolten(construct))
    {
        Position const& pool = GetIgnisNearestWaterPool(construct);

        // Brittle comes off a once-a-second poll on the construct, so hold still once it is in range
        // rather than walking it back out again.
        if (construct->GetExactDist2d(&pool) <= ULDUAR_IGNIS_WATER_BRITTLE_RADIUS)
            return false;

        return MoveTo(bot->GetMapId(), pool.GetPositionX(), pool.GetPositionY(), pool.GetPositionZ(), false, false,
                      false, true, MovementPriority::MOVEMENT_FORCED, true, false);
    }

    // Scorch only lights a patch away from the water, so until one is up there is nowhere to take the
    // construct - hold it where it is instead of dragging it around the room.
    Unit* scorchedGround = GetIgnisNearestScorchedGround(botAI, construct);
    if (!scorchedGround)
        return false;

    if (bot->GetExactDist2d(scorchedGround) <= ULDUAR_IGNIS_SCORCHED_GROUND_PARK_DISTANCE)
        return false;

    return MoveTo(bot->GetMapId(), scorchedGround->GetPositionX(), scorchedGround->GetPositionY(),
                  scorchedGround->GetPositionZ(), false, false, false, true, MovementPriority::MOVEMENT_FORCED, true,
                  false);
}

bool IgnisBrittleConstructMarkAction::isUseful()
{
    IgnisBrittleConstructMarkTrigger ignisBrittleConstructMarkTrigger(botAI);
    return ignisBrittleConstructMarkTrigger.IsActive();
}

bool IgnisBrittleConstructMarkAction::Execute(Event /*event*/)
{
    Unit* target = GetIgnisBrittleConstruct(botAI);
    if (!target)
        target = GetIgnis(botAI);

    if (!target)
        return false;

    MarkTargetWithSkull(bot, target);
    SetRtiTarget(botAI, "skull", target);

    return true;
}

bool IgnisMoltenConstructAvoidAction::isUseful()
{
    IgnisMoltenConstructAvoidTrigger ignisMoltenConstructAvoidTrigger(botAI);
    return ignisMoltenConstructAvoidTrigger.IsActive();
}

bool IgnisMoltenConstructAvoidAction::Execute(Event /*event*/)
{
    // Keyed on the Molten aura rather than the creature entry, so the construct the tank is still
    // walking through the fire does not scatter the raid.
    Unit* molten = GetIgnisNearestMoltenConstruct(botAI, bot);
    if (!molten)
        return false;

    return FleePosition(molten->GetPosition(), ULDUAR_IGNIS_MOLTEN_AVOID_RADIUS);
}

bool IgnisSlagPotHealAction::isUseful()
{
    IgnisSlagPotHealTrigger ignisSlagPotHealTrigger(botAI);
    return ignisSlagPotHealTrigger.IsActive();
}

bool IgnisSlagPotHealAction::Execute(Event /*event*/)
{
    Player* victim = GetIgnisSlagPotVictim(botAI);
    if (!victim)
        return false;

    // The pot ticks for ten seconds and the victim cannot be moved or shielded out of it, so the only
    // answer is raw throughput.
    static std::vector<std::string> const directHeals = {
        "greater heal",         "flash heal",       "penance",   // priest
        "healing touch",        "nourish",          "regrowth",  // druid
        "holy light",           "flash of light",   "holy shock",  // paladin
        "greater healing wave", "healing wave",     "riptide",  // shaman
    };

    for (std::string const& heal : directHeals)
    {
        if (botAI->CanCastSpell(heal, victim))
            return botAI->CastSpell(heal, victim);
    }

    return false;
}

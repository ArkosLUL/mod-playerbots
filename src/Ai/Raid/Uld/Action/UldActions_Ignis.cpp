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
#include "Spell.h"
#include "UldEncounter_Ignis.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <TankAssistStrategy.h>

bool IgnisScorchedGroundAction::isUseful()
{
    IgnisScorchedGroundTrigger ignisScorchedGroundTrigger(botAI);
    return ignisScorchedGroundTrigger.IsActive();
}

bool IgnisScorchedGroundAction::Execute(Event /*event*/)
{
    Unit* patch = GetIgnisNearestScorchedGround(botAI, bot);
    if (!patch)
        return false;

    return FleePosition(patch->GetPosition(), ULDUAR_IGNIS_SCORCHED_GROUND_AVOID_RADIUS);
}

bool IgnisMainTankPositionAction::isUseful()
{
    IgnisMainTankPositionTrigger ignisMainTankPositionTrigger(botAI);
    return ignisMainTankPositionTrigger.IsActive();
}

bool IgnisMainTankPositionAction::Execute(Event /*event*/)
{
    Unit* boss = GetIgnis(botAI);
    if (!boss)
        return false;

    Position const spot = GetIgnisMainTankPosition(bot, boss);
    float const distance = bot->GetExactDist2d(&spot);

    if (_spotReached && distance > ULDUAR_IGNIS_TANK_SPOT_TOLERANCE * 2.0f)
        _spotReached = false;

    if (_spotReached || distance <= ULDUAR_IGNIS_TANK_SPOT_TOLERANCE)
    {
        _spotReached = true;
        return false;
    }

    if (!bot->IsWithinMeleeRange(boss) && !IsIgnisScorchWindow(boss))
        return false;

    Position const leg = GetIgnisMainTankLeg(bot, boss, spot);

    return MoveTo(bot->GetMapId(), leg.GetPositionX(), leg.GetPositionY(), leg.GetPositionZ(), false, false, false,
                  true, MovementPriority::MOVEMENT_COMBAT);
}

bool IgnisConstructTankAction::isUseful()
{
    IgnisConstructTankTrigger ignisConstructTankTrigger(botAI);
    return ignisConstructTankTrigger.IsActive();
}

bool IgnisConstructTankAction::Execute(Event event)
{
    int8 const tankIndex = GetIgnisConstructTankIndex(bot);
    if (tankIndex < 0)
        return false;

    Unit* construct = GetIgnisDrivenConstruct(botAI, bot);
    if (!construct)
        return false;

    if (AI_VALUE(Unit*, "current target") != construct)
        return Attack(construct);

    if (construct->GetVictim() != bot)
    {
        if (botAI->DoSpecificAction("taunt spell", event, true))
            return true;

        return MoveTo(bot->GetMapId(), construct->GetPositionX(), construct->GetPositionY(),
                      construct->GetPositionZ(), false, false, false, true,
                      MovementPriority::MOVEMENT_FORCED, true, false);
    }

    if (IsIgnisConstructMolten(construct))
    {
        Position const& pool = GetIgnisAssignedWaterPool(tankIndex);

        if (construct->GetExactDist2d(&pool) <= ULDUAR_IGNIS_WATER_BRITTLE_RADIUS)
            return false;

        Position const spot = GetIgnisWaterApproach(bot, pool);

        return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                      false, true, MovementPriority::MOVEMENT_FORCED, true, false);
    }

    Unit* scorchedGround = GetIgnisAssignedScorchedGround(botAI, construct, tankIndex);
    if (!scorchedGround)
        return false;

    if (bot->GetExactDist2d(scorchedGround) <= ULDUAR_IGNIS_SCORCHED_GROUND_PARK_DISTANCE)
        return false;

    return MoveTo(bot->GetMapId(), scorchedGround->GetPositionX(), scorchedGround->GetPositionY(),
                  scorchedGround->GetPositionZ(), false, false, false, true, MovementPriority::MOVEMENT_FORCED, true,
                  false);
}

bool IgnisAttackBrittleConstructAction::isUseful()
{
    IgnisAttackBrittleConstructTrigger ignisAttackBrittleConstructTrigger(botAI);
    return ignisAttackBrittleConstructTrigger.IsActive();
}

bool IgnisAttackBrittleConstructAction::Execute(Event /*event*/)
{
    Unit* construct = GetIgnisBrittleConstruct(botAI);
    if (!construct)
        return false;

    if (AI_VALUE(Unit*, "current target") != construct)
        return Attack(construct);

    static std::unordered_map<uint8, std::vector<std::string>> const burstSpells = {
        {CLASS_MAGE, {"pyroblast", "frostbolt"}},
        {CLASS_HUNTER, {"chimera shot", "aimed shot"}},
        {CLASS_WARLOCK, {"chaos bolt", "shadow bolt"}},
        {CLASS_PRIEST, {"mind blast"}},
        {CLASS_DRUID, {"starfire"}},
        {CLASS_SHAMAN, {"lava burst"}},
    };

    auto const spells = burstSpells.find(bot->getClass());
    if (spells == burstSpells.end())
        return false;

    for (std::string const& spell : spells->second)
    {
        if (botAI->CanCastSpell(spell, construct))
            return botAI->CastSpell(spell, construct);
    }

    return false;
}

bool IgnisAttackBossAction::isUseful()
{
    IgnisAttackBossTrigger ignisAttackBossTrigger(botAI);
    return ignisAttackBossTrigger.IsActive();
}

bool IgnisAttackBossAction::Execute(Event /*event*/)
{
    Unit* boss = GetIgnis(botAI);
    if (!boss)
        return false;

    if (AI_VALUE(Unit*, "current target") != boss)
        return Attack(boss);

    return false;
}

bool IgnisMoltenConstructAvoidAction::isUseful()
{
    IgnisMoltenConstructAvoidTrigger ignisMoltenConstructAvoidTrigger(botAI);
    return ignisMoltenConstructAvoidTrigger.IsActive();
}

bool IgnisMoltenConstructAvoidAction::Execute(Event /*event*/)
{
    Unit* molten = GetIgnisNearestMoltenConstruct(botAI, bot);
    if (!molten)
        return false;

    return FleePosition(molten->GetPosition(), ULDUAR_IGNIS_MOLTEN_AVOID_RADIUS);
}

bool IgnisFlameJetsHoldCastAction::isUseful()
{
    IgnisFlameJetsTrigger ignisFlameJetsTrigger(botAI);
    return ignisFlameJetsTrigger.IsActive();
}

bool IgnisFlameJetsHoldCastAction::Execute(Event /*event*/)
{
    Unit* boss = GetIgnis(botAI);
    if (!boss)
        return false;

    Spell* jets = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    Spell* own = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!jets || !own)
        return false;

    if (own->GetCastTimeRemaining() < jets->GetCastTimeRemaining())
        return false;

    bot->CastStop();

    return true;
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

    static std::vector<std::string> const directHeals = {
        "greater heal",         "flash heal",       "penance",
        "healing touch",        "nourish",          "regrowth",
        "holy light",           "flash of light",   "holy shock",
        "greater healing wave", "healing wave",     "riptide",
    };

    for (std::string const& heal : directHeals)
    {
        if (botAI->CanCastSpell(heal, victim))
            return botAI->CastSpell(heal, victim);
    }

    return false;
}

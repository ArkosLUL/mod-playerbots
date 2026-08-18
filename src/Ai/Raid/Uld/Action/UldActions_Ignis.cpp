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
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
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
    // Same grid lookup the trigger uses. Going through "nearest npcs" here instead would leave the
    // trigger hot on a patch the action cannot see, and the bot would stand in it burning.
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

    Position const spot = GetIgnisMainTankPosition(botAI, bot);
    float const distance = bot->GetExactDist2d(&spot);

    // Reach then hold. Without the latch the bot re-issues a move on every drift inside the
    // tolerance, and a bot that is sliding on the spot never lands a swing.
    if (_spotReached && distance > ULDUAR_IGNIS_TANK_SPOT_TOLERANCE * 2.0f)
        _spotReached = false;

    if (_spotReached || distance <= ULDUAR_IGNIS_TANK_SPOT_TOLERANCE)
    {
        _spotReached = true;
        return false;
    }

    // Ignis runs at 10 yd/s against a player's 7, so he stays glued to a tank moving at full speed
    // and the walk needs no throttling. If he has fallen out of melee range anyway then something
    // went wrong with his path, and walking further only drops him for good. Scorch is the
    // exception: he is rooted for those three seconds and cannot follow anyone.
    if (!bot->IsWithinMeleeRange(boss) && !IsIgnisScorchWindow(boss))
        return false;

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT);
}

bool IgnisConstructTankAction::isUseful()
{
    IgnisConstructTankTrigger ignisConstructTankTrigger(botAI);
    return ignisConstructTankTrigger.IsActive();
}

bool IgnisConstructTankAction::Execute(Event event)
{
    int8 const tankIndex = GetIgnisConstructTankIndex(botAI, bot);
    if (tankIndex < 0)
        return false;

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
        // Fixed by tank index rather than by whichever pool is nearer: the anchor sits almost exactly
        // between the two, so nearest-pool always resolves the same way and both constructs would
        // shatter in one spot.
        Position const& pool = GetIgnisAssignedWaterPool(tankIndex);

        // Brittle comes off a once-a-second poll on the construct, so hold still once it is in range
        // rather than walking it back out again.
        if (construct->GetExactDist2d(&pool) <= ULDUAR_IGNIS_WATER_BRITTLE_RADIUS)
            return false;

        return MoveTo(bot->GetMapId(), pool.GetPositionX(), pool.GetPositionY(), pool.GetPositionZ(), false, false,
                      false, true, MovementPriority::MOVEMENT_FORCED, true, false);
    }

    // Scorch only lights a patch away from the water, so until one is up there is nowhere to take the
    // construct - hold it where it is instead of dragging it around the room.
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

    // Shatter needs one hit of 5000 (10-man) / 3000 (25-man) inside the 15 s window, which rotation
    // filler often will not reach on its own. Falls through to the rotation when none of these is
    // available rather than standing still waiting for a cooldown.
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
    // Keyed on the Molten aura rather than the creature entry, so the construct the tank is still
    // walking through the fire does not scatter the raid.
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

    // A cast that still lands first is worth finishing - only the ones the knockback will eat are
    // thrown away here, and stopping those frees the mana and the six seconds that follow.
    if (own->GetCastTimeRemaining() < jets->GetCastTimeRemaining())
        return false;

    botAI->InterruptSpell();

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

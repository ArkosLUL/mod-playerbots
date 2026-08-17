#include "UldTriggers_Freya.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>

bool FreyaNearNatureBombTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // Tanks eat the bomb. Stepping out would drag Freya toward the raid, or lift the Conservator off
    // the spore the melee are sheltering on, and ~6k every 18s is cheaper than either.
    if (botAI->IsTank(bot))
        return false;

    return bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS) != nullptr;
}

bool FreyaSetDpsPriorityTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    return PlayerbotAI::IsDps(bot);
}

bool FreyaTankAddsTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // The ladder ends at Freya, so these two always have something - no need to gather the wave here.
    return PlayerbotAI::IsMainTank(bot) || PlayerbotAI::IsAssistTankOfIndex(bot, 0, true);
}

bool FreyaRedirectThreatTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");

    return boss && boss->IsAlive();
}

bool FreyaAvoidDetonatingLasherTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // Tanks eat the blast. They have the health for it, and a tank running mid-wave gives up whatever
    // it is holding.
    if (botAI->IsTank(bot))
        return false;

    if (bot->GetHealth() >= ULDUAR_FREYA_DETONATE_FLEE_HEALTH)
        return false;

    Creature* lasher = bot->FindNearestCreature(NPC_DETONATING_LASHER, ULDUAR_FREYA_DETONATE_RADIUS);
    if (!lasher || !lasher->IsAlive())
        return false;

    // The bot killing it is inside 15 yd by definition and cannot do its job anywhere else, so it stays.
    return AI_VALUE(Unit*, "current target") != lasher;
}

bool FreyaMoveToHealingSporeTrigger::IsActive()
{
    // Check for the Freya boss
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // Conservator's Grip is a 50000 yd pacify-silence, so melee need a spore just as much as ranged.
    // Tanks are excluded for two different reasons: the add tank already ends up inside the aura by
    // walking the Conservator onto a spore, and moving it here as well would oscillate it between that
    // spore and the one nearest itself. The main tank is out because Freya is never repositioned.
    if (botAI->IsTank(bot))
        return false;

    // By entry, not "find target": that value walks only this bot's threat list, and a pacified bot that
    // has not hit the Conservator yet is exactly the bot that needs a spore.
    if (!GetFirstAliveUnitByEntry(botAI, NPC_ANCIENT_CONSERVATOR))
        return false;

    // The pheromone aura is the thing that matters, and it is exact - a bot can be inside 6 yd of a
    // spore that is still growing and not have it yet.
    if (bot->HasAura(SPELL_POTENT_PHEROMONES))
        return false;

    for (const ObjectGuid& guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == NPC_HEALTHY_SPORE)
            return true;
    }

    return false;
}

bool FreyaBreakIronRootsTrigger::IsActive()
{
    if (!IsFreyaHardModeActive(botAI))
        return false;

    // Trapped by either the Ironbranch or the Freya-cast Iron Roots (each leaves its own DoT).
    return bot->HasAura(SPELL_IRON_ROOTS_DAMAGE) || bot->HasAura(SPELL_IRON_ROOTS_FREYA_DAMAGE);
}

bool FreyaDodgeUnstableSunBeamTrigger::IsActive()
{
    if (!IsFreyaHardModeActive(botAI))
        return false;

    // The beam stalkers are non-selectable, so they never show up in attack-target lists - scan the
    // raw nearby-npc list instead.
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_FREYA_SUN_BEAM && unit->GetEntry() != NPC_FREYA_UNSTABLE_SUN_BEAM)
            continue;

        if (bot->GetExactDist2d(unit) < ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS)
            return true;
    }

    return false;
}

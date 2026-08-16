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
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Find the nearest Nature Bomb
    GameObject* target = bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, 12.0f);
    return target != nullptr;
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

    if (!botAI->IsTank(bot))
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    return GetFreyaTankTarget(botAI, state) != nullptr;
}

bool FreyaAvoidDetonatingLasherTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    Map* map = bot->GetMap();
    if (!map || !map->IsRaid())
        return false;

    // Detonate's maximum roll. Above this the blast cannot kill, so the bot stays and keeps hitting.
    uint32 const healthThreshold = map->Is25ManRaid() ? 7200 : 4900;
    if (bot->GetHealth() >= healthThreshold)
        return false;

    Creature* lasher = bot->FindNearestCreature(NPC_DETONATING_LASHER, ULDUAR_FREYA_DETONATE_RADIUS);

    return lasher && lasher->IsAlive();
}

bool FreyaMoveToHealingSporeTrigger::IsActive()
{
    // Check for the Freya boss
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // Conservator's Grip is a 50000 yd pacify-silence, so melee need a spore just as much as ranged.
    // Tanks stay put: walking one to a spore drags the Conservator into the raid.
    if (botAI->IsTank(bot))
        return false;

    Unit* conservatory = AI_VALUE2(Unit*, "find target", "ancient conservator");
    if (!conservatory || !conservatory->IsAlive())
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

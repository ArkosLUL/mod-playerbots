#include "UldHardMode.h"

#include "AiObjectContext.h"
#include "InstanceScript.h"
#include "Map.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "Unit.h"

bool IsVezaxHardModeActive(PlayerbotAI* /*botAI*/) { return sPlayerbotAIConfig.ulduarVezaxHardMode; }

bool IsIronAssemblyHardModeActive(PlayerbotAI* /*botAI*/) { return sPlayerbotAIConfig.ulduarIronAssemblyHardMode; }

bool IsSteelbreakerEmpowered(PlayerbotAI* botAI)
{
    if (!IsIronAssemblyHardModeActive(botAI))
        return false;

    // Steelbreaker last alive: the other two dead means two Supercharges pushed him to phase 3.
    return GetFirstAliveUnitByEntry(botAI, NPC_STEELBREAKER) != nullptr &&
           GetFirstAliveUnitByEntry(botAI, NPC_MOLGEIM) == nullptr &&
           GetFirstAliveUnitByEntry(botAI, NPC_BRUNDIR) == nullptr;
}

Unit* GetIronAssemblyNextKillTarget(PlayerbotAI* botAI)
{
    if (Unit* brundir = GetFirstAliveUnitByEntry(botAI, NPC_BRUNDIR))
        return brundir;

    if (Unit* molgeim = GetFirstAliveUnitByEntry(botAI, NPC_MOLGEIM))
        return molgeim;

    return GetFirstAliveUnitByEntry(botAI, NPC_STEELBREAKER);
}

uint32 FlameLeviathanActiveTowerMask(PlayerbotAI* /*botAI*/)
{
    return sPlayerbotAIConfig.ulduarFlameLeviathanHardMode ? FL_TOWER_ALL : 0;
}

Unit* GetFlameLeviathanNearestTowerHazard(PlayerbotAI* botAI, Unit* from, uint32 towerMask, float radius)
{
    if (!from)
        return nullptr;

    Unit* nearest = nullptr;
    float best = radius;

    // The hazard markers are non-selectable trigger creatures, so they never appear in the
    // attack-target lists ("possible targets"). Scan the raw nearby-npc list, which includes
    // every non-player unit regardless of selectable/attackable flags.
    auto const& npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get();
    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        uint32 entry = unit->GetEntry();
        bool const isHazard = ((towerMask & FL_TOWER_STORM) && entry == NPC_FL_THORIM_HAMMER_TARGET) ||
                              ((towerMask & FL_TOWER_FLAMES) && entry == NPC_FL_MIMIRONS_INFERNO_TARGET) ||
                              ((towerMask & FL_TOWER_FROST) && entry == NPC_FL_HODIRS_FURY_TARGET);
        if (!isHazard)
            continue;

        float const dist = from->GetExactDist2d(unit);
        if (dist < best)
        {
            best = dist;
            nearest = unit;
        }
    }

    return nearest;
}

bool IsThorimHardModeActive(PlayerbotAI* /*botAI*/) { return sPlayerbotAIConfig.ulduarThorimHardMode; }

bool IsFreyaHardModeActive(PlayerbotAI* /*botAI*/) { return sPlayerbotAIConfig.ulduarFreyaHardMode; }

bool IsHodirHardModeActive(PlayerbotAI* /*botAI*/) { return sPlayerbotAIConfig.ulduarHodirHardMode; }

bool IsMimironHardModeActive(PlayerbotAI* /*botAI*/) { return sPlayerbotAIConfig.ulduarMimironHardMode; }

bool IsXT002HardModeActive(PlayerbotAI* /*botAI*/) { return sPlayerbotAIConfig.ulduarXT002HardMode; }

bool IsXT002HeartbreakActive(PlayerbotAI* botAI)
{
    Unit* xt002 = GetXT002(botAI);
    return xt002 != nullptr && xt002->HasAura(SPELL_XT002_HEARTBREAK);
}

bool IsYoggSaronHardModeActive(PlayerbotAI* /*botAI*/) { return sPlayerbotAIConfig.ulduarYoggSaronHardMode; }

bool YoggThorimKeeperActive(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    Map* map = bot->GetMap();
    if (!map || !map->IsDungeon())
        return false;

    InstanceScript* instance = ((InstanceMap*)map)->GetInstanceScript();
    if (!instance)
        return false;

    // Same source the boss script reads: the freed-Keeper bitmask the raid set from the pre-pull
    // gossips. Which Keepers are up is a raid choice made in-instance, so it cannot come from config.
    return (instance->GetPersistentData(PERSISTENT_DATA_WATCHERS_MASK) & (1u << KEEPER_THORIM)) != 0;
}

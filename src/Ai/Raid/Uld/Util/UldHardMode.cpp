#include "UldHardMode.h"

#include "AiObjectContext.h"
#include "InstanceScript.h"
#include "Map.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "UldEncounter_FlameLeviathan.h"
#include "UldEncounter_XT002.h"
#include "UldScripts.h"
#include "Unit.h"

using namespace EncounterHelpers;

bool IsVezaxHardModeActive(PlayerbotAI* /*botAI*/) { return sPlayerbotAIConfig.ulduarVezaxHardMode; }

bool IsIronAssemblyHardModeActive(PlayerbotAI* /*botAI*/) { return sPlayerbotAIConfig.ulduarIronAssemblyHardMode; }

bool IsSteelbreakerEmpowered(PlayerbotAI* /*botAI*/, Unit* steelbreaker, Unit* molgeim, Unit* brundir)
{
    return steelbreaker != nullptr && molgeim == nullptr && brundir == nullptr;
}

bool IsSteelbreakerEmpowered(PlayerbotAI* botAI)
{
    // No hard-mode gate: this is a phase check, and Steelbreaker reaching phase 3 is a fact about the
    // fight rather than a raid setting. Gating it meant a raid that got there without the option set
    // lost the tank swap and chain-died to Meltdown.
    return IsSteelbreakerEmpowered(botAI, GetFirstAliveUnitByEntry(botAI, NPC_STEELBREAKER),
                                   GetFirstAliveUnitByEntry(botAI, NPC_MOLGEIM),
                                   GetFirstAliveUnitByEntry(botAI, NPC_BRUNDIR));
}

Unit* GetIronAssemblyNextKillTarget(PlayerbotAI* botAI)
{
    Unit* steelbreaker = GetFirstAliveUnitByEntry(botAI, NPC_STEELBREAKER);
    Unit* molgeim = GetFirstAliveUnitByEntry(botAI, NPC_MOLGEIM);
    Unit* brundir = GetFirstAliveUnitByEntry(botAI, NPC_BRUNDIR);

    // The kill order is the only thing the hard-mode option changes. Hard mode saves Steelbreaker for
    // last so he reaches phase 3; otherwise he dies first, which takes Fusion Punch off the tank and
    // means Static Disruption and Overwhelming Power never happen at all. Molgeim is second in both
    // orders, so Rune of Summoning and its untauntable Lightning Elementals are never reached.
    if (IsIronAssemblyHardModeActive(botAI))
    {
        if (brundir)
            return brundir;
        if (molgeim)
            return molgeim;

        return steelbreaker;
    }

    if (steelbreaker)
        return steelbreaker;
    if (molgeim)
        return molgeim;

    return brundir;
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

bool IsMimironHardModeActive(PlayerbotAI* /*botAI*/) { return sPlayerbotAIConfig.ulduarMimironHardMode; }

bool IsXT002HardModeActive(PlayerbotAI* /*botAI*/) { return sPlayerbotAIConfig.ulduarXT002HardMode; }

bool IsXT002HeartbreakActive(Player* bot, Unit* xt002)
{
    return bot != nullptr && xt002 != nullptr && xt002->HasAura(GetXT002HeartbreakSpellId(bot));
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

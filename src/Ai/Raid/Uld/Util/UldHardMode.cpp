#include "UldHardMode.h"

#include "AiObjectContext.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "UldBossHelper.h"
#include "Unit.h"

bool IsVezaxHardModeActive(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.ulduarVezaxHardMode)
        return false;

    return GetFirstAliveUnitByEntry(botAI, NPC_VEZAX_SARONITE_ANIMUS) != nullptr;
}

bool IsIronAssemblyHardModeActive(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.ulduarIronAssemblyHardMode)
        return false;

    // A kill order only matters while the council is still being whittled down.
    return GetFirstAliveUnitByEntry(botAI, NPC_STEELBREAKER) != nullptr ||
           GetFirstAliveUnitByEntry(botAI, NPC_MOLGEIM) != nullptr ||
           GetFirstAliveUnitByEntry(botAI, NPC_BRUNDIR) != nullptr;
}

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

uint32 FlameLeviathanActiveTowerMask(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.ulduarFlameLeviathanHardMode)
        return 0;

    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_FLAME_LEVIATHAN);
    if (!boss)
        return 0;

    uint32 mask = 0;
    if (boss->HasAura(SPELL_FL_TOWER_OF_STORMS))
        mask |= FL_TOWER_STORM;
    if (boss->HasAura(SPELL_FL_TOWER_OF_FLAMES))
        mask |= FL_TOWER_FLAMES;
    if (boss->HasAura(SPELL_FL_TOWER_OF_FROST))
        mask |= FL_TOWER_FROST;
    if (boss->HasAura(SPELL_FL_TOWER_OF_LIFE))
        mask |= FL_TOWER_LIFE;
    return mask;
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

bool IsThorimHardModeActive(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.ulduarThorimHardMode)
        return false;

    Unit* sif = GetFirstAliveUnitByEntry(botAI, NPC_SIF);
    if (!sif)
        return false;

    // Sif spawns at Thorim's throne and only drops onto the arena floor once she interrupts her
    // channel to join the fight - being below the floor threshold is the live hard-mode signal.
    return sif->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD;
}

bool IsFreyaHardModeActive(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.ulduarFreyaHardMode)
        return false;

    Unit* freya = GetFirstAliveUnitByEntry(botAI, NPC_FREYA);
    return freya != nullptr && freya->IsInCombat();
}

bool IsHodirHardModeActive(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.ulduarHodirHardMode)
        return false;

    Unit* hodir = GetFirstAliveUnitByEntry(botAI, NPC_HODIR);
    return hodir != nullptr && hodir->IsInCombat();
}

bool IsMimironHardModeActive(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.ulduarMimironHardMode)
        return false;

    // Only the active mech is a real attack target (Mimiron himself never leaves his pod), so key
    // off the Emergency Mode aura firefighter puts on whichever mech is currently up.
    Unit* mkii = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    if (mkii && mkii->HasAura(SPELL_EMERGENCY_MODE))
        return true;

    Unit* vx001 = GetFirstAliveUnitByEntry(botAI, NPC_VX001);
    if (vx001 && vx001->HasAura(SPELL_EMERGENCY_MODE))
        return true;

    Unit* acu = GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    if (acu && acu->HasAura(SPELL_EMERGENCY_MODE))
        return true;

    return false;
}

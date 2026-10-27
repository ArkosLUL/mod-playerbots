/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_LichKing.h"
#include "ICCActions_LichKing.h"
#include "Playerbots.h"
#include "Trigger.h"

bool IccLichKingShadowTrapTrigger::IsActive()
{
    Unit* vdw = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (vdw)
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "the lich king");
    if (!boss)
        return false;

    bool hasPlague = botAI->HasAura("Necrotic Plague", bot);
    if (hasPlague)
        return false;

    if (!PlayerbotAI::IsMainTank(bot))
        return false;

    // Shadow Traps come in phase one only and the first winter's opening hit despawns them
    if (IccLichKingPhase(bot, boss) > IccLichKingPhaseId::Winter1)
        return false;

    // search for all nearby traps
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    std::vector<Unit*> nearbyTraps;

    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() == NPC_SHADOW_TRAP && bot->GetExactDist2d(unit) < 11.0f)  // shadow trap
            return true;
    }

    return false;
}

bool IccLichKingNecroticPlagueTrigger::IsActive()
{
    Unit* vdw = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (vdw)
        return false;

    if (!AI_VALUE2(Unit*, "find target", "the lich king"))
        return false;

    return botAI->HasAura("Necrotic Plague", bot);
}

bool IccLichKingWinterTrigger::IsActive()
{
    Unit* vdw = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (vdw)
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "the lich king");
    if (!boss)
        return false;

    // Asked every tick by every bot in the fight, so lk.phase stays current. The winter action runs
    // while the winter itself is up, a narrower window than the phase's walk and Quake wait.
    IccLichKingPhase(bot, boss);
    return IccBossHasRemorselessWinter(boss);
}

bool IccLichKingAddsTrigger::IsActive()
{
    Unit* vdw = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (vdw)
        return false;

    if (bot->HasAura(SPELL_HARVEST_SOUL_VALKYR))
        return false;

    if (botAI->HasAura("Necrotic Plague", bot))
        return false;

    if (bot->FindNearestCreature(NPC_TERENAS_MENETHIL_HC, 55.0f) ||
        bot->FindNearestCreature(NPC_TERENAS_MENETHIL, 55.0f))
        return true;

    if (!bot->FindNearestCreature(NPC_THE_LICH_KING, 100.0f))
        return false;

    return true;
}

bool IccLichKingSpiritBombTrigger::IsActive()
{
    return IccLichKingSpiritBombAction::IsBombThreatActive(botAI, bot);
}

/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoETriggers.h"

#include "SharedDefines.h"
#include "Spell.h"

Unit* MalygosTrigger::getMalygos(Player* bot)
{
    return bot->FindNearestCreature(NPC_MALYGOS, 250.0f, true);
}

uint8 MalygosTrigger::getPhase(Player* bot)
{
    if (bot->GetMapId() != EOE_MAP_ID) { return 0; }

    Unit* drake = bot->GetVehicleBase();
    if (drake && drake->GetEntry() == NPC_WYRMREST_SKYTALON)
    {
        return 3;
    }

    Unit* boss = getMalygos(bot);
    if (!boss || !boss->IsInCombat()) { return 0; }

    // P2: Malygos is airborne/untargetable while the disc adds are up.
    if (bot->FindNearestCreature(NPC_NEXUS_LORD, 250.0f, true) ||
        bot->FindNearestCreature(NPC_SCION_OF_ETERNITY, 250.0f, true))
    {
        return 2;
    }

    // Attackable with no adds and not on a drake -> Phase 1.
    if (!boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
    {
        return 1;
    }

    // In combat, non-attackable, no adds, not yet mounted -> P1->P2 / P2->P3 transition.
    return 4;
}

bool MalygosTrigger::IsActive()
{
    uint8 phase = getPhase(bot);
    return phase == 1 || phase == 2 || phase == 4;
}

bool PowerSparkTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 1) { return false; }

    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (unit && unit->GetEntry() == NPC_POWER_SPARK)
        {
            return true;
        }
    }

    return false;
}

bool DeepBreathTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 2) { return false; }

    // Void-zone hazard already on the ground, or Malygos winding it up.
    if (bot->FindNearestCreature(NPC_ARCANE_OVERLOAD, 40.0f, true))
    {
        return true;
    }

    Unit* boss = MalygosTrigger::getMalygos(bot);
    return boss && bool(boss->FindCurrentSpellBySpellId(SPELL_ARCANE_OVERLOAD));
}

bool SurgeOfPowerTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 2) { return false; }

    if (bot->FindNearestCreature(NPC_SURGE_OF_POWER, 100.0f, true))
    {
        return true;
    }

    Unit* boss = MalygosTrigger::getMalygos(bot);
    return boss && bool(boss->FindCurrentSpellBySpellId(SPELL_SURGE_OF_POWER_P2));
}

bool StaticFieldTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 3) { return false; }

    Unit* drake = bot->GetVehicleBase();
    if (!drake) { return false; }

    Creature* field = drake->FindNearestCreature(NPC_STATIC_FIELD, 20.0f, true);
    return bool(field);
}

bool DrakeSurgeTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 3) { return false; }

    Unit* drake = bot->GetVehicleBase();
    if (!drake) { return false; }

    Unit* boss = MalygosTrigger::getMalygos(bot);
    if (!boss) { return false; }

    Spell* surge = boss->FindCurrentSpellBySpellId(SPELL_SURGE_OF_POWER_P3);
    if (!surge) { surge = boss->FindCurrentSpellBySpellId(SPELL_SURGE_OF_POWER_P3_25); }
    if (!surge) { return false; }

    // Only the fixated drake should burn its Flame Shield / Blazing Speed and break formation; the
    // rest of the flight holds position and keeps those cooldowns for their own fixate.
    return surge->m_targets.GetUnitTargetGUID() == drake->GetGUID();
}

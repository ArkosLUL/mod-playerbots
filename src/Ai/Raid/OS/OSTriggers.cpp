/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSTriggers.h"
#include "OSShared.h"
#include "SharedDefines.h"

using namespace ObsidianSanctumHelpers;

bool SartharionTankTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss) { return false; }

    return botAI->IsTank(bot);
}

bool FlameTsunamiTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss) { return false; }

    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit)
        {
            if (unit->GetEntry() == NPC_FLAME_TSUNAMI)
            {
                return true;
            }
        }
    }

    return false;
}

bool TwilightFissureTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss) { return false; }

    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit)
        {
            if (unit->GetEntry() == NPC_TWILIGHT_FISSURE)
            {
                return true;
            }
        }
    }

    return false;
}

bool SartharionDpsTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss) { return false; }

    return botAI->IsDps(bot);
}

bool SartharionMeleePositioningTrigger::IsActive()
{
    if (!botAI->IsMelee(bot) || !botAI->IsDps(bot)) { return false; }

    // Rear-flank whatever the melee is on (Sartharion or a to-kill drake): boss Flame Breath/Cleave
    // and drake Shadow Breath are all frontal. Kept drakes are held by the off-tank, not attacked.
    return AI_VALUE2(Unit*, "find target", "sartharion") != nullptr;
}

bool SartharionRangedPositioningTrigger::IsActive()
{
    if (botAI->IsMelee(bot) || botAI->IsTank(bot)) { return false; }

    return AI_VALUE2(Unit*, "find target", "sartharion") != nullptr;
}

bool TwilightPortalEnterTrigger::IsActive()
{
    // Only the capped set of designated runners enters, so the raid isn't emptied into the realm
    // every cycle and the off-tank keeps holding the drakes instead of being pulled in.
    if (!IsTwilightRealmRunner(botAI, bot)) { return false; }

    // Nothing to do inside unless an acolyte is actually up to kill. Both auras this checks only
    // exist while Sartharion is being fought, so it doubles as the encounter gate.
    if (!TwilightRealmNeedsRunner(botAI, bot)) { return false; }

    return bool(bot->FindNearestGameObject(GO_TWILIGHT_PORTAL, 100.0f));
}

bool TwilightPortalExitTrigger::IsActive()
{
    // Leave the moment there's no acolyte left for this runner to kill in its realm. Tying the check
    // to the same perception-limited lookup the attack uses avoids stranding a runner that cleared
    // its acolyte while another remains out of reach in a different realm.
    return bot->HasAura(SPELL_TWILIGHT_SHIFT) && FindTwilightRealmAcolyte(botAI) == nullptr;
}

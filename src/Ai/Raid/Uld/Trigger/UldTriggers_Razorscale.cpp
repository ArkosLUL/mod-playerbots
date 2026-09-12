#include "UldTriggers_Razorscale.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldData.h"
#include "UldEncounter_Razorscale.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <RtiTargetValue.h>

using namespace EncounterHelpers;

bool RazorscaleFlyingAloneTrigger::IsActive()
{
    Unit* boss = GetRazorscaleScan(botAI).Boss();
    if (!boss)
    {
        return false;
    }

    // Check if the boss is flying
    if (boss->GetPositionZ() < RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD)
    {
        return false;
    }

    // Get the list of attackers
    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    if (attackers.empty())
    {
        return true;  // No attackers implies flying alone
    }

    std::vector<Unit*> dark_rune_adds;

    // Loop through attackers to find dark rune adds
    for (ObjectGuid const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        uint32 entry = unit->GetEntry();

        // Check for valid dark rune entries
        if (entry == RazorscaleBossHelper::UNIT_DARK_RUNE_WATCHER ||
            entry == RazorscaleBossHelper::UNIT_DARK_RUNE_GUARDIAN ||
            entry == RazorscaleBossHelper::UNIT_DARK_RUNE_SENTINEL)
        {
            dark_rune_adds.push_back(unit);
        }
    }

    // Return whether there are no dark rune adds
    return dark_rune_adds.empty();
}

bool RazorscaleDevouringFlamesTrigger::IsActive()
{
    Unit* boss = GetRazorscaleScan(botAI).Boss();
    if (!boss)
        return false;

    for (ObjectGuid const& npc : GetRazorscaleScan(botAI).Hostiles())
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == RazorscaleBossHelper::UNIT_DEVOURING_FLAME)
        {
            return true;
        }
    }

    return false;
}

bool RazorscaleAvoidSentinelTrigger::IsActive()
{
    Unit* boss = GetRazorscaleScan(botAI).Boss();
    if (!boss)
        return false;

    for (ObjectGuid const& npc : GetRazorscaleScan(botAI).Hostiles())
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == RazorscaleBossHelper::UNIT_DARK_RUNE_SENTINEL)
        {
            return true;
        }
    }

    return false;
}

bool RazorscaleAvoidWhirlwindTrigger::IsActive()
{
    Unit* boss = GetRazorscaleScan(botAI).Boss();
    if (!boss)
        return false;

    for (ObjectGuid const& npc : GetRazorscaleScan(botAI).Hostiles())
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == RazorscaleBossHelper::UNIT_DARK_RUNE_SENTINEL &&
            (unit->HasAura(RazorscaleBossHelper::SPELL_SENTINEL_WHIRLWIND) ||
             unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL)))
        {
            return true;
        }
    }

    return false;
}

bool RazorscaleGroundedTrigger::IsActive()
{
    Unit* boss = GetRazorscaleScan(botAI).Boss();
    if (!boss)
    {
        return false;
    }

    // Check if the boss is flying
    if (boss->GetPositionZ() < RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD)
    {
        return true;
    }
    return false;
}

bool RazorscaleHarpoonAvailableTrigger::IsActive()
{
    Unit* boss = GetRazorscaleScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Update the boss AI context in the helper
    RazorscaleBossHelper razorscaleHelper(botAI);

    if (!razorscaleHelper.UpdateBossAI())
    {
        return false;
    }

    // After UpdateBossAI(), which assigns the tank roles as a side effect. Nobody else fires one, so
    // nobody else needs the harpoon search.
    if (!IsRazorscaleHarpoonCrew(botAI, bot))
        return false;

    return GetRazorscaleClosestReadyHarpoon(botAI) != nullptr;
}

bool RazorscaleFuseArmorTrigger::IsActive()
{
    // Get the boss entity
    Unit* boss = GetRazorscaleScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Only proceed if this bot can actually tank
    if (!botAI->IsTank(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Iterate through group members to find the main tank with Fuse Armor
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !botAI->IsMainTank(member))
            continue;

        Aura* fuseArmor = member->GetAura(RazorscaleBossHelper::SPELL_FUSE_ARMOR);
        if (fuseArmor && fuseArmor->GetStackAmount() >= RazorscaleBossHelper::FUSEARMOR_THRESHOLD)
            return true;
    }

    return false;
}

bool RazorscaleKillTargetTrigger::IsActive()
{
    // One bot drives the marking to avoid the whole raid fighting over the icon
    if (!IsMechanicTrackerBot(bot, ULDUAR_MAP_ID))
        return false;

    Unit* target = GetRazorscaleKillTarget(botAI);
    if (!target)
        return false;

    Group* group = bot->GetGroup();
    if (group && group->GetTargetIcon(RtiTargetValue::skullIndex) == target->GetGUID())
        return false;

    return true;
}

bool RazorscalePetControlTrigger::IsActive()
{
    Unit* boss = GetRazorscaleScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    return bot->GetGuardianPet() != nullptr;
}

bool RazorscaleFlameBreathTrigger::IsActive()
{
    Unit* boss = GetRazorscaleScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    // Flame Breath is a grounded-phase frontal cone; while flying she is not breathing
    if (boss->GetPositionZ() >= RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD)
        return false;

    // The tank holds the boss and eats the breath; only reposition non-tanks caught in the cone
    if (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0))
        return false;

    return IsBotInFrontalCone(bot, boss, M_PI / 2.0f, 40.0f);
}

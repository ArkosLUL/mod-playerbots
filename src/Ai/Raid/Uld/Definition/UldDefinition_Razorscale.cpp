/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "Playerbots.h"
#include "Strategy.h"
#include "Timer.h"
#include "UldActions_Razorscale.h"
#include "UldData.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Razorscale.h"
#include "UldTriggers_Razorscale.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool RazorscaleMoversBlocked(PlayerbotAI* botAI)
{
    if (RazorscaleBossHelper::FindDevouringFlameNear(botAI, RazorscaleBossHelper::DEVOURING_FLAME_CLEAR_RADIUS))
        return true;

    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsMelee(bot))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (!target)
        return false;

    return RazorscaleBossHelper::DevouringFlameBlocks(bot, target->GetPositionX(), target->GetPositionY());
}

bool RazorscaleHoldMovers(PlayerbotAI* botAI)
{
    if (botAI->GetBot()->GetMapId() != ULDUAR_MAP_ID || !BossStateGateLive(botAI, ULD_BOSS_RAZORSCALE))
        return false;

    thread_local PlayerbotAI* cachedFor = nullptr;
    thread_local uint32 cachedAtMs = 0;
    thread_local bool cachedBlocked = false;

    uint32 const now = getMSTime();
    if (cachedFor != botAI || cachedAtMs != now || !cachedAtMs)
    {
        cachedFor = botAI;
        cachedAtMs = now;
        cachedBlocked = RazorscaleMoversBlocked(botAI);
    }

    return cachedBlocked;
}

void DefineRazorscale(EncounterBuilder& e)
{
    e.Node<RazorscaleDevouringFlamesTrigger, RazorscaleAvoidDevouringFlameAction>(ACTION_RAID + 4);

    e.Node<RazorscalePetControlTrigger, RazorscalePetControlAction>(ACTION_RAID + 5);

    e.Node<RazorscaleAvoidSentinelTrigger, RazorscaleAvoidSentinelAction>(ACTION_RAID + 2);
    e.Node<RazorscaleFlyingAloneTrigger, RazorscaleIgnoreBossAction>(ACTION_MOVE + 5);
    e.Node<RazorscaleAvoidWhirlwindTrigger, RazorscaleAvoidWhirlwindAction>(ACTION_RAID + 3);
    e.Node<RazorscaleGroundedTrigger, RazorscaleGroundedAction>(ACTION_RAID);
    e.Node<RazorscaleHarpoonAvailableTrigger, RazorscaleHarpoonAction>(ACTION_MOVE);
    e.Node<RazorscaleFuseArmorTrigger, RazorscaleFuseArmorAction>(ACTION_RAID + 2);
    e.Node<RazorscaleKillTargetTrigger, RazorscaleKillTargetAction>(ACTION_RAID);
    e.Node<RazorscaleFlameBreathTrigger, RazorscaleFlameBreathAction>(ACTION_RAID + 1);
    e.Node(
        "razorscale fire resistance trigger",
        [](PlayerbotAI* botAI) -> Trigger* { return new BossFireResistanceTrigger(botAI, "razorscale"); },
        "razorscale fire resistance action",
        [](PlayerbotAI* botAI) -> Action* { return new BossFireResistanceAction(botAI, "razorscale"); }, ACTION_RAID);

    e.Block("razorscale", Role::Any, RazorscaleHoldMovers,
            Family::Reach | Family::ReachHeal | Family::Charge | Family::CombatFormationMove | Family::TankFace |
                Family::SetBehind | Family::RearFlank | Family::Follow | Family::AvoidAoe);
}
}

EncounterDefinition const& UldRazorscaleDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_RAZORSCALE, BossStateGate, "razorscale", &DefineRazorscale);
    return definition;
}

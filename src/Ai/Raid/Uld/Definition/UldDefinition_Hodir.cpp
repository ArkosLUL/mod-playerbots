/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"
#include "UldActions_Hodir.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Hodir.h"
#include "UldMultipliers_Hodir.h"
#include "UldTriggers_Hodir.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool HodirTauntSuicide(PlayerbotAI* botAI)
{
    return IsHodirEngaged(botAI) && HodirTauntWouldBeSuicide(botAI, botAI->GetBot());
}

bool HodirOwnsTargets(PlayerbotAI* botAI) { return IsHodirEngaged(botAI) && !PlayerbotAI::IsHeal(botAI->GetBot()); }

bool HodirFreezeRun(PlayerbotAI* botAI) { return IsHodirEngaged(botAI) && IsHodirFlashFreezeIncoming(botAI); }

bool HodirFreezeRunInShelter(PlayerbotAI* botAI)
{
    return HodirFreezeRun(botAI) && IsHodirInLandedShelter(botAI, botAI->GetBot());
}

bool HodirFreezeRunOutsideShelter(PlayerbotAI* botAI)
{
    return HodirFreezeRun(botAI) && !IsHodirInLandedShelter(botAI, botAI->GetBot());
}

bool HodirIcicleInPath(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsHodirEngaged(botAI) || PlayerbotAI::IsTank(bot) || PlayerbotAI::IsHeal(bot))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && IsHodirWalkThroughLiveIcicle(bot, target->GetPosition(),
                                                  ULDUAR_HODIR_ICE_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN,
                                                  ULDUAR_HODIR_BIG_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN);
}

bool HodirAnchored(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return IsHodirEngaged(botAI) &&
           (PlayerbotAI::IsMainTank(bot) || PlayerbotAI::IsAssistTankOfIndex(bot, 0, true) ||
            PlayerbotAI::IsRanged(bot));
}

bool HodirResistancePaladin(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return bot->getClass() == CLASS_PALADIN && IsHodirEngaged(botAI) && GetHodirResistancePaladin(botAI, bot) == bot;
}

void DefineHodir(EncounterBuilder& e)
{
    e.Node<HodirNearSnowpackedIcicleTrigger, HodirMoveSnowpackedIcicleAction>(ACTION_RAID + 6, EncounterRow::Mover);
    e.Node<HodirIcicleDodgeTrigger, HodirIcicleDodgeAction>(ACTION_RAID + 5, EncounterRow::Mover);
    e.Node<HodirFrozenBlowsSwapTrigger, HodirFrozenBlowsSwapAction>(ACTION_RAID + 4);

    e.Node<HodirTankDefensiveTrigger, HodirTankDefensiveAction>(ACTION_RAID + 4);

    e.Node<HodirRedirectThreatTrigger, HodirRedirectThreatAction>(ACTION_RAID + 4);

    e.Node<HodirSetDpsPriorityTrigger, HodirSetDpsPriorityAction>(ACTION_RAID + 3);
    e.Node<HodirSpreadStormCloudTrigger, HodirSpreadStormCloudAction>(ACTION_RAID + 2, EncounterRow::Mover);
    e.Node<HodirCollectStormPowerTrigger, HodirCollectStormPowerAction>(ACTION_RAID + 2, EncounterRow::Mover);
    e.Node<HodirBitingColdTrigger, HodirBitingColdShedAction>(ACTION_RAID + 1, EncounterRow::Mover);
    e.Node<HodirFrostResistanceTrigger, HodirFrostResistanceAction>(ACTION_RAID);
    e.Node<HodirRaidPositionTrigger, HodirRaidPositionAction>(ACTION_RAID, EncounterRow::Mover);

    e.Block("hodir guard multiplier", Role::Any, HodirTauntSuicide, Family::Taunt);

    e.Block("hodir tank defensive hold", Role::Tank, IsHodirTankDefensiveHeld, 0, HeldTankDefensiveNames());

    e.OwnTargeting("hodir guard multiplier", Role::Any, HodirOwnsTargets,
                   Family::DpsAssist | Family::TankAssist | Family::DebuffOnAttacker);
    e.Multiplier<HodirBreakerAoeHoldMultiplier>(Family::Spell);
    e.Multiplier<HodirStormGatherCastHoldMultiplier>(Family::Spell);

    e.Block("hodir storm gather hold", Role::Any, IsHodirStormGatherHeld, Family::Reach | Family::SetBehind);

    e.Block("hodir guard multiplier", Role::Any, HodirFreezeRun, Family::Charge | Family::Blink | Family::Disengage);

    e.OwnMovement("hodir guard multiplier", Role::Any, HodirFreezeRunOutsideShelter, Family::Attack,
                  {HodirMoveSnowpackedIcicleAction::Name, HodirIcicleDodgeAction::Name});
    e.OwnMovement("hodir guard multiplier", Role::Any, HodirFreezeRunInShelter, Family::Attack,
                  {HodirMoveSnowpackedIcicleAction::Name, HodirIcicleDodgeAction::Name,
                   HodirBitingColdShedAction::Name});

    e.Block("hodir guard multiplier", Role::Any, HodirIcicleInPath,
            Family::Reach | Family::ReachHeal | Family::SetBehind);

    e.OwnMovement("hodir guard multiplier", Role::Any, HodirAnchored,
                  Family::Attack | Family::Reach | Family::ReachHeal);

    e.Block("hodir paladin aura multiplier", Role::Any, HodirResistancePaladin, 0,
            {"devotion aura", "retribution aura", "concentration aura", "crusader aura", "sanctity aura",
             "shadow resistance aura", "fire resistance aura"});

    e.Block("hodir lust hold", Role::Any, IsHodirLustHeld, 0, {"heroism", "bloodlust"});
}
}

EncounterDefinition const& UldHodirDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_HODIR, BossStateGate, "hodir", &DefineHodir);
    return definition;
}

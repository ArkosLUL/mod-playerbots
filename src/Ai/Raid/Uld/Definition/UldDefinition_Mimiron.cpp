/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "Strategy.h"
#include "UldActions_Mimiron.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Mimiron.h"
#include "UldHardMode.h"
#include "UldMultipliers_Mimiron.h"
#include "UldScripts.h"
#include "UldTargets.h"
#include "UldTriggers_Mimiron.h"

using namespace EncounterHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool MimironLethalWindowActive(PlayerbotAI* botAI)
{
    if (!GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII) && !GetUldUnitByEntry(botAI, NPC_VX001) &&
        !GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return false;

    MimironShockBlastTrigger shockBlast(botAI);
    if (shockBlast.IsActive())
        return true;

    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (barrage.IsActive())
        return true;

    MimironRocketStrikeTrigger rocketStrike(botAI);
    if (rocketStrike.IsActive())
        return true;

    MimironDodgeFlamesTrigger dodgeFlames(botAI);
    if (dodgeFlames.IsActive())
        return true;

    MimironFrostBombTrigger frostBomb(botAI);
    return frostBomb.IsActive();
}

bool MimironFormationHeld(PlayerbotAI* botAI)
{
    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (barrage.IsActive())
        return true;

    Player* bot = botAI->GetBot();
    Position slot;
    return GetMimironSpreadSlot(botAI, bot, slot) &&
           bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) <= ULDUAR_MIMIRON_SPREAD_TOLERANCE;
}

bool MimironFireFight(PlayerbotAI* botAI) { return IsMimironHardModeActive(botAI) && IsMimironEngaged(botAI); }

bool MimironFirefighterLive(PlayerbotAI* botAI)
{
    return IsMimironHardModeActive(botAI) && BossStateGateLive(botAI, ULD_BOSS_MIMIRON);
}

bool MimironNearFire(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);
    for (Position const& node : hazards.flames)
        if (bot->GetExactDist2d(node.GetPositionX(), node.GetPositionY()) < ULDUAR_MIMIRON_DRINK_FIRE_CLEARANCE)
            return true;

    return false;
}

bool MimironHandoverDrinkHold(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    MimironHandoverStep const step = GetMimironHandoverStep(botAI, bot);
    if (step == MimironHandoverStep::Late || step == MimironHandoverStep::WalkIn)
        return true;

    Position slot;
    return step == MimironHandoverStep::Hold && GetMimironSpreadSlot(botAI, bot, slot) &&
           bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) > ULDUAR_MIMIRON_SPREAD_TOLERANCE;
}

bool MimironHandoverFollowHold(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Position slot;
    return GetMimironHandoverStep(botAI, bot) != MimironHandoverStep::None &&
           GetMimironSpreadSlot(botAI, bot, slot);
}

bool MimironResistancePaladin(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return bot->getClass() == CLASS_PALADIN && GetMimironFrostResistancePaladin(botAI, bot) == bot;
}

bool MimironRaidDropping(PlayerbotAI* botAI)
{
    return IsMimironPhase2(botAI) && botAI->GetAiObjectContext()->GetValue<uint8>("aoe heal", "low")->Get() <
                                         ULDUAR_MIMIRON_STORM_COOLDOWN_LOW_COUNT;
}

bool MimironMeleeSector(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (PlayerbotAI::IsRanged(bot))
        return false;

    if (IsMimironHardModeActive(botAI) && (IsMimironPhase2(botAI) || IsMimironPhase4(bot)))
        return true;

    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    return barrage.IsActive();
}

bool MimironPhase4MainTank(PlayerbotAI* botAI) { return IsMimironPhase4(botAI->GetBot()); }

bool MimironPhase4AcuTank(PlayerbotAI* botAI) { return IsMimironPhase4AcuTank(botAI->GetBot()); }

bool MimironBombTank(PlayerbotAI* botAI) { return IsMimironBombTank(botAI, botAI->GetBot()); }

void CutMimironCastsInFire(PlayerbotAI* botAI)
{
    if (!IsMimironHardModeActive(botAI))
        return;

    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !member->IsInMap(bot) || IsRealPlayer(member))
            continue;

        Spell* spell = member->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell || spell->getState() != SPELL_STATE_PREPARING ||
            spell->GetCastTimeRemaining() <= ULDUAR_MIMIRON_FLAMES_CAST_FINISH_MS)
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI)
            continue;

        MimironDodgeFlamesTrigger dodge(memberAI);
        if (!dodge.IsActive())
            continue;

        memberAI->RequestSpellInterrupt();
        if (RaidObs::Active())
            RaidObs::Note(member, "mimiron.castcut", std::to_string(spell->GetSpellInfo()->Id));
    }
}

void MimironEncounterTick(PlayerbotAI* botAI)
{
    if (MimironTick(botAI))
        CutMimironCastsInFire(botAI);
}

bool MimironOwnsTargets(PlayerbotAI* botAI)
{
    return IsMimironEngaged(botAI) || IsMimironFireBotCullActive(botAI, botAI->GetBot());
}

void DefineMimiron(EncounterBuilder& e)
{
    e.Node<MimironP3Wx2LaserBarrageTrigger, MimironP3Wx2LaserBarrageAction>(ACTION_RAID + 7);
    e.Node<MimironShockBlastTrigger, MimironShockBlastAction>(ACTION_RAID + 5.5f);
    e.Node(
        "mimiron fire resistance trigger",
        [](PlayerbotAI* botAI) -> Trigger* { return new BossFireResistanceTrigger(botAI, "mimiron"); },
        "mimiron fire resistance action",
        [](PlayerbotAI* botAI) -> Action* { return new BossFireResistanceAction(botAI, "mimiron"); }, ACTION_RAID);
    e.Node<MimironFrostResistanceTrigger, MimironFrostResistanceAction>(ACTION_RAID);
    e.Node<MimironPhase1PositioningTrigger, MimironPhase1PositioningAction>(ACTION_RAID);
    e.Node<MimironArcSpreadTrigger, MimironArcSpreadAction>(ACTION_RAID);
    e.Node<MimironAerialCommandUnitTrigger, MimironAerialCommandUnitAction>(ACTION_RAID);
    e.Node<MimironRocketStrikeTrigger, MimironRocketStrikeAction>(ACTION_RAID + 5);
    e.Node<MimironPhase4FocusTrigger, MimironPhase4FocusAction>(ACTION_RAID);
    e.Node<MimironMagneticCoreTrigger, MimironMagneticCoreAction>(ACTION_RAID + 1);
    e.Node<MimironPlasmaBlastDefensiveTrigger, MimironPlasmaBlastDefensiveAction>(ACTION_RAID + 6);
    e.Node<MimironRedirectThreatTrigger, MimironRedirectThreatAction>(ACTION_RAID + 1);
    e.Node<MimironSetDpsPriorityTrigger, MimironSetDpsPriorityAction>(ACTION_RAID);
    e.Node<MimironProximityMineTrigger, MimironProximityMineAction>(ACTION_RAID - 1);
    e.Node<MimironUnreachableTrigger, MimironUnreachableStepAction>(ACTION_RAID + 2);
    e.Node<MimironBombBotTrigger, MimironBombBotAction>(ACTION_RAID + 2);
    e.Node<MimironBombTankFormTrigger, MimironBombTankFormAction>(ACTION_RAID + 5.5f);
    e.Node<MimironBombBotTauntTrigger, MimironBombBotTauntAction>(ACTION_RAID + 5);
    e.Node<MimironBombTankHoldTrigger, MimironBombTankHoldAction>(ACTION_RAID);
    e.Node<MimironSlowBombBotTrigger, MimironSlowBombBotAction>(ACTION_RAID + 2);
    e.Node<MimironPetControlTrigger, MimironPetControlAction>(ACTION_RAID);
    e.Node<MimironDodgeFlamesTrigger, MimironDodgeFlamesAction>(ACTION_RAID + 4);
    e.Node<MimironApproachTargetTrigger, MimironApproachTargetAction>(ACTION_RAID - 2);
    e.Node<MimironFrostBombTrigger, MimironFrostBombAction>(ACTION_RAID + 6);
    e.Node<MimironFireBotTrigger, MimironFireBotAction>(ACTION_RAID + 3);
    e.Node<MimironResetEncounterStateTrigger, MimironResetEncounterStateAction>(ACTION_RAID);

    e.OwnTargeting("mimiron target guard", Role::NonTank, MimironOwnsTargets, Family::DpsAssist);

    e.OwnTargeting("mimiron phase 4 tank target guard", Role::MainTank, MimironPhase4MainTank);

    e.OwnTargeting("mimiron phase 4 acu tank target guard", Role::Tank, MimironPhase4AcuTank);

    e.OwnTargeting("mimiron bomb tank target guard", Role::Tank, MimironBombTank,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                       Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti);

    e.Block("mimiron bomb tank hold", Role::Tank, MimironBombTank,
            Family::Reach | Family::Charge | Family::TankFace | Family::SetBehind | Family::Follow |
                Family::CombatFormationMove | Family::Taunt,
            {"caster form"});

    e.Block("mimiron charge guard", Role::Any, MimironLethalWindowActive, Family::Charge,
            {"reach melee", "killing spree"});

    e.Block("mimiron avoid aoe guard", Role::Any, MimironFireFight, 0, {"avoid aoe"});

    e.Block("mimiron revive pet hold", Role::Any, MimironFirefighterLive, 0, {"revive pet"});

    e.Block("mimiron formation guard", Role::Any, MimironFormationHeld, Family::CombatFormationMove);

    e.Block("mimiron generic redirect guard", Role::Any, MimironPhase1Active, 0, {"tricks of the trade"});

    e.Multiplier<MimironTankAnchorGuardMultiplier>(Family::TankFace | Family::Reach);

    e.Block("mimiron drink guard", Role::Any, MimironNearFire, 0, {"drink", "eat"});
    e.Block("mimiron handover drink hold", Role::Any, MimironHandoverDrinkHold, 0, {"drink", "eat"});
    e.Block("mimiron handover follow hold", Role::Any, MimironHandoverFollowHold, 0, {"follow"});

    e.Block("mimiron frost bomb guard", Role::Any, IsMimironNearFrostBomb, Family::Charge,
            {"reach melee", "reach spell", "reach party member to heal", "set behind", "follow",
             "killing spree", MimironApproachTargetAction::Name});
    e.Block("mimiron fire hold guard", Role::Any, IsMimironHeldByFire, 0,
            {"reach melee", "reach spell", "reach party member to heal", "set behind", "follow",
             MimironApproachTargetAction::Name});
    e.Block("mimiron rocket guard", Role::Any, IsMimironNearRocket, 0,
            {"reach melee", "reach spell", "reach party member to heal", "set behind", "follow",
             MimironApproachTargetAction::Name});
    e.Block("mimiron bomb bot guard", Role::Any, IsMimironNearBombBotBlast, 0,
            {"reach melee", "reach spell", "reach party member to heal", "set behind", "follow",
             MimironApproachTargetAction::Name});

    e.Multiplier<MimironFireBotAoeGuardMultiplier>(Family::AnyAction);
    e.Multiplier<MimironPlasmaDefensiveHoldMultiplier>(Family::AnyAction);

    e.Block("mimiron paladin aura", Role::Any, MimironResistancePaladin, 0,
            {"devotion aura", "retribution aura", "concentration aura", "crusader aura", "sanctity aura",
             "shadow resistance aura", "fire resistance aura", "mimiron fire resistance action"});

    e.Block("mimiron storm cooldown hold", Role::Any, MimironRaidDropping, 0,
            {"divine sacrifice", "divine hymn", "power infusion"});

    e.Block("mimiron vx001 facing guard", Role::Any, MimironMeleeSector, Family::TankFace, {"set behind"});

    e.Tick(MimironEncounterTick);
}
}

EncounterDefinition const& UldMimironDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_MIMIRON, BossStateGate, "mimiron", &DefineMimiron);
    return definition;
}

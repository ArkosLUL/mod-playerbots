#include "UldTriggers_YoggSaron.h"
#include "UldActions_YoggSaron.h"
#include "RaidTankDefensive.h"

#include <algorithm>
#include <cmath>

#include "GameObject.h"
#include "Group.h"
#include "Object.h"
#include "Pet.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_YoggSaron.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "SpellMgr.h"
#include "SharedDefines.h"
#include "PlayerbotAIConfig.h"
#include "RaidObs.h"
#include "Trigger.h"
#include "Vehicle.h"
#include "WorldSession.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <CombatStrategy.h>
#include <TankAssistStrategy.h>

using namespace EncounterHelpers;

bool YoggSaronTrigger::IsDesignatedBotTank()
{
    if (PlayerbotAI::IsBotMainTank(bot))
        return true;

    if (!PlayerbotAI::IsTank(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsTank(member))
            continue;

        if (!member->GetSession() || !member->GetSession()->IsHeadless())
            continue;

        return member == bot;
    }

    return false;
}

bool YoggSaronTrigger::IsPhase2()
{
    return YoggSaronInPhase2(botAI);
}

bool YoggSaronTrigger::IsPhase3()
{
    return YoggSaronInPhase3(botAI);
}

bool YoggSaronTrigger::IsInBrainLevel() { return YoggSaronOnBrainLevel(bot); }

bool YoggSaronTrigger::PhaseThreeStationReaches(Unit* target)
{
    Position const& spot = YoggSaronPhaseThreeSpot(bot);
    float const reach = PlayerbotAI::IsMelee(bot)
                            ? sPlayerbotAIConfig.MeleeDistance + bot->GetCombatReach() + target->GetCombatReach()
                            : sPlayerbotAIConfig.SpellDistance;

    return spot.GetExactDist2d(target->GetPositionX(), target->GetPositionY()) -
               ULDUAR_YOGG_SARON_PHASE_3_STATION_RADIUS <=
           reach;
}

bool YoggSaronTrigger::IsYoggSaronFight() { return YoggSaronEncounterActive(botAI); }

bool YoggSaronTrigger::IsInIllusionRoom()
{
    YoggSaronRoom const room = YoggSaronRoomOf(bot);

    return room == YOGG_SARON_ROOM_STORMWIND || room == YOGG_SARON_ROOM_ICECROWN ||
           room == YOGG_SARON_ROOM_CHAMBER;
}

bool YoggSaronTrigger::IsInStormwindKeeperIllusion() { return YoggSaronRoomOf(bot) == YOGG_SARON_ROOM_STORMWIND; }

bool YoggSaronTrigger::IsInIcecrownKeeperIllusion() { return YoggSaronRoomOf(bot) == YOGG_SARON_ROOM_ICECROWN; }

bool YoggSaronTrigger::IsInChamberOfTheAspectsIllusion() { return YoggSaronRoomOf(bot) == YOGG_SARON_ROOM_CHAMBER; }

bool YoggSaronTrigger::IsMasterIsInIllusionGroup()
{
    Player* master = botAI->GetMaster();
    return master && !PlayerbotAI::IsTank(master);
}

bool YoggSaronTrigger::IsMasterIsInBrainRoom()
{
    return YoggSaronRoomOf(botAI->GetMaster()) == YOGG_SARON_ROOM_BRAIN;
}

bool YoggSaronTrigger::IsBrainRoomApproachable() { return YoggSaronBrainRoomApproachable(botAI); }

bool YoggSaronGuardianPositioningTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsMelee(bot))
        return false;

    if (YoggSaronHandoverState(botAI).clearing)
    {
        returning = false;
        pulling = false;
        return true;
    }

    float const fromSara =
        bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY());

    bool const pile = !PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsHeal(bot);
    float const leash = pile ? ULDUAR_YOGG_SARON_P1_PILE_LEASH : ULDUAR_YOGG_SARON_P1_LEASH;
    float const release = pile ? ULDUAR_YOGG_SARON_P1_PILE_RELEASE : ULDUAR_YOGG_SARON_P1_LEASH_RELEASE;

    bool const beyond = fromSara > (returning ? release : leash);

    if (!beyond && fromSara >= ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS)
    {
        returning = false;
        pulling = false;
        return false;
    }

    bool const active = beyond && YoggSaronInPhase1Approach(bot) && YoggSaronInPhase1(botAI);

    if (active != returning && RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.p1leash",
                             !active                                       ? "held"
                             : fromSara > ULDUAR_YOGG_SARON_P1_ROOM_RADIUS ? "door"
                                                                           : "returning");

    returning = active;

    Position spot;
    bool const pull = !active && YoggSaronPhase1TankPullSpot(botAI, spot) &&
                      bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) >
                          ULDUAR_YOGG_SARON_P1_TANK_PULL_ARRIVED;

    if (pull != pulling && RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.p1pull", pull ? "pull" : "held");

    pulling = pull;

    return returning || pulling;
}

bool YoggSaronSanityTrigger::IsActive()
{
    if (IsInBrainLevel())
        return false;

    Aura* sanityAura = bot->GetAura(SPELL_SANITY);

    if (!sanityAura)
        return false;

    int sanityAuraStacks = sanityAura->GetStackAmount();

    if (sanityAuraStacks >= 100)
        return false;

    Creature* sanityWell = YoggSaronNearestCreature(botAI, NPC_SANITY_WELL);

    if (!sanityWell)
        return false;

    char const* reason = nullptr;
    if (bot->GetDistance(sanityWell) < 1.0f && !YoggSaronPortalWalkPending(botAI))
        reason = "topping";
    else if (sanityAuraStacks < static_cast<int>(ULDUAR_YOGG_SARON_SANITY_LOW))
        reason = "low";
    else if (IsPhase2() && sanityAuraStacks < static_cast<int>(ULDUAR_YOGG_SARON_BRAIN_TEAM_MIN_SANITY) &&
             !YoggSaronPortalWalkPending(botAI))
        reason = "refill";
    else if (IsPhase2() && YoggSaronPlatformIdle(botAI) && !YoggSaronPortalWalkPending(botAI))
        reason = "idle";

    if (!reason)
        return false;

    if (!YoggSaronWalkMakingProgress(botAI, "sanity", sanityWell->GetPosition()))
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.sanity", reason);

    return true;
}

bool YoggSaronPhase1SpacingTrigger::IsActive()
{
    if (!botAI->CanMove() || !YoggSaronInPhase1Room(bot))
        return false;

    if (!YoggSaronInPhase1(botAI))
        return false;

    bool hazardNear = bot->FindNearestCreature(NPC_OMINOUS_CLOUD, ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS, true);

    char const* reason = hazardNear ? "cloud" : nullptr;

    if (!hazardNear && !GetYoggSaronNovaThreats(botAI, ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS).empty())
        reason = bot->HasAura(SPELL_SARAS_FERVOR) ? "fervor" : "nova";

    if (!reason)
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.p1dodge", reason);

    return true;
}

bool YoggSaronPhase1StationTrigger::IsActive()
{
    if (!botAI->CanMove())
        return false;

    if (!PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsHeal(bot))
        return false;

    if (!YoggSaronInPhase1Approach(bot))
        return false;

    float const fromSara =
        bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY());

    float const fromSpot = bot->GetDistance2d(ULDUAR_YOGG_SARON_P1_RANGED_SPOT.GetPositionX(),
                                              ULDUAR_YOGG_SARON_P1_RANGED_SPOT.GetPositionY());

    float const stack =
        ULDUAR_YOGG_SARON_P1_RANGED_STACK_RADIUS * (parked ? ULDUAR_YOGG_SARON_P1_PARKED_STACK_SCALE : 1.0f);

    bool const offBand = std::abs(fromSara - ULDUAR_YOGG_SARON_P1_RANGED_STATION_RADIUS) >
                         ULDUAR_YOGG_SARON_P1_RANGED_BAND_TOLERANCE;

    char const* reason = fromSara > ULDUAR_YOGG_SARON_P1_ROOM_RADIUS ? "door"
                         : offBand                                   ? "band"
                         : fromSpot > stack                          ? "stack"
                                                                     : nullptr;

    parked = !reason;

    if (!reason || !YoggSaronInPhase1(botAI))
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.p1station", reason);

    return true;
}

bool YoggSaronPhase2SpacingTrigger::IsActive()
{
    if (!botAI->CanMove())
        return false;

    uint32 const phase = YoggSaronPhase(botAI);
    if (phase != 2 && phase != 3)
        return false;

    bool hazardNear = bot->FindNearestCreature(NPC_DEATH_RAY, ULDUAR_YOGG_SARON_DEATH_RAY_TRIGGER_RADIUS, true);

    if (!hazardNear)
    {
        std::vector<Position> wedges = GetYoggSaronCrushWedges(botAI, ULDUAR_YOGG_SARON_CRUSH_RANGE);
        hazardNear = InYoggSaronCrushWedge(wedges, bot->GetPositionX(), bot->GetPositionY(),
                                           ULDUAR_YOGG_SARON_CRUSH_TRIGGER_ARC);
    }

    if (!hazardNear)
        hazardNear = YoggSaronInCrusherReach(botAI, ULDUAR_YOGG_SARON_CRUSHER_REACH_TRIGGER_RADIUS);

    if (!hazardNear)
        hazardNear = YoggSaronInBodyKnockback(bot);

    if (!hazardNear)
        hazardNear = !GetYoggSaronNovaThreats(botAI, ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS).empty();

    return hazardNear;
}

bool YoggSaronSetDpsPriorityTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot) && !IsPhase2())
        return false;

    return IsYoggSaronFight();
}

bool YoggSaronDarkVolleyTrigger::IsActive()
{
    if (!YoggSaronCanInterrupt(bot))
        return false;

    for (Unit* caster : GetYoggSaronInterruptCasters(botAI))
    {
        if (YoggSaronInterruptClaimedByOther(bot, caster))
            continue;

        for (char const* spell : YoggSaronInterruptSpellsFor(bot, caster))
            if (YoggSaronInterruptReady(botAI, spell, caster))
                return true;
    }

    return false;
}

bool YoggSaronDiminishPowerJudgementTrigger::IsActive()
{
    std::vector<char const*> const spells = YoggSaronJudgementSpells(bot);
    if (spells.empty())
        return false;

    for (Unit* crusher : GetYoggSaronChannellingCrushers(botAI))
        for (char const* spell : spells)
            if (botAI->CanCastSpell(spell, crusher))
                return true;

    return false;
}

bool YoggSaronDiminishPowerBreakTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot) || !botAI->CanMove())
        return false;

    Unit* crusher = nullptr;
    Position spot;
    return YoggSaronPowerBreakPlan(botAI, crusher, spot) != YOGG_SARON_POWER_BREAK_NONE;
}

bool YoggSaronMortalStrikeTrigger::IsActive()
{
    if (bot->getClass() != CLASS_WARRIOR)
        return false;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", "mortal strike");
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info || bot->HasSpellCooldown(spellId) ||
        info->CalcPowerCost(bot, info->GetSchoolMask()) > static_cast<int32>(bot->GetPower(POWER_RAGE)))
        return false;

    char const* reason = nullptr;
    return YoggSaronMortalStrikeTarget(botAI, reason) != nullptr;
}

bool YoggSaronWarlockPetTrigger::IsActive()
{
    if (bot->getClass() != CLASS_WARLOCK)
        return false;

    Pet* pet = bot->GetPet();
    if ((pet && pet->IsAlive()) || YoggSaronOnBrainLevel(bot) || !IsPhase3())
        return false;

    char const* summon = YoggSaronWarlockSummon(botAI);
    return summon && botAI->CanCastSpell(summon, bot);
}

bool YoggSaronMaladyOfTheMindTrigger::IsActive()
{
    if (!IsPhase2())
        return false;

    TooCloseToPlayerWithDebuffTrigger tooCloseToPlayerWithDebuffTrigger(botAI);
    return tooCloseToPlayerWithDebuffTrigger.TooCloseToPlayerWithDebuff(SPELL_MALADY_OF_THE_MIND,
                                                                        ULDUAR_YOGG_SARON_MALADY_SPREAD_RADIUS) &&
           botAI->CanMove();
}

bool YoggSaronPhase3ControlTrigger::IsActive()
{
    if (!IsPhase3() || !IsDesignatedBotTank())
        return false;

    TankFaceStrategy tankFaceStrategy(botAI);
    if (botAI->HasStrategy(tankFaceStrategy.getName(), BotState::BOT_STATE_COMBAT))
        return true;

    TankAssistStrategy tankAssistStrategy(botAI);
    return !botAI->HasStrategy(tankAssistStrategy.getName(), BotState::BOT_STATE_COMBAT);
}

bool YoggSaronBrainLinkTrigger::IsActive()
{
    return IsPhase2() && YoggSaronBrainLinkTarget(botAI) != nullptr && !YoggSaronPortalWalkPending(botAI);
}

bool YoggSaronMoveToEnterPortalTrigger::IsActive()
{
    std::string const rti = AI_VALUE(std::string, "rti");
    if (rti != "skull" && rti != "diamond")
        return false;

    if (YoggSaronRoomOf(bot) != YOGG_SARON_ROOM_ARENA)
        return false;

    Position spot;
    YoggSaronPortalIntent const intent = YoggSaronPortalPlan(botAI, spot);

    return intent == YOGG_SARON_PORTAL_SPREADING || intent == YOGG_SARON_PORTAL_LATE;
}

bool YoggSaronFallFromFloorTrigger::IsActive()
{
    if (!IsYoggSaronFight())
        return false;

    std::string rtiMark = AI_VALUE(std::string, "rti");

    if (rtiMark == "skull" && bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return true;

    if ((rtiMark == "cross" || rtiMark == "circle" || rtiMark == "star") &&
        bot->GetPositionZ() < ULDUAR_YOGG_SARON_BRAIN_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return true;

    return false;
}

bool YoggSaronStopFollowingTrigger::IsActive()
{
    if (!IsYoggSaronFight() || !IsPhase2())
        return false;

    FollowMasterStrategy followMasterStrategy(botAI);
    return botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT);
}

bool YoggSaronUsePortalTrigger::IsActive()
{
    if (!IsPhase2())
        return false;

    if (AI_VALUE(std::string, "rti") != "diamond")
        return false;

    return YoggSaronPlannedPortal(botAI) != nullptr;
}

bool YoggSaronIllusionRoomTrigger::IsActive()
{
    if (!IsYoggSaronFight() || !IsInIllusionRoom() || AI_VALUE(std::string, "rti") == "square")
        return false;

    if (SetRtiMarkRequired())
        return true;

    if (GoToBrainRoomRequired())
        return true;

    return WalkIntoRoomRequired() || StageRequired();
}

bool YoggSaronIllusionRoomTrigger::WalkIntoRoomRequired()
{
    return YoggSaronRoomStateOf(botAI) == YOGG_SARON_ROOM_STATE_WALKING_IN && !PlayerbotAI::IsHeal(bot);
}

bool YoggSaronIllusionRoomTrigger::StageRequired()
{
    Position spot;
    if (YoggSaronRoomStateOf(botAI) != YOGG_SARON_ROOM_STATE_STAGED || !YoggSaronIllusionStageSpot(bot, spot) ||
        bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) <= ULDUAR_YOGG_SARON_STAGE_ARRIVED_RADIUS)
        return false;

    return YoggSaronWalkMakingProgress(botAI, "stage", spot);
}

bool YoggSaronIllusionRoomTrigger::GoToBrainRoomRequired()
{
    if (AI_VALUE(std::string, "rti") == "square")
        return false;

    return IsBrainRoomApproachable();
}

bool YoggSaronIllusionRoomTrigger::SetRtiMarkRequired()
{
    return AI_VALUE(std::string, "rti") == "diamond";
}

bool YoggSaronIllusionHealerStationTrigger::IsActive()
{
    if (!PlayerbotAI::IsHeal(bot) || !botAI->CanMove())
        return false;

    Position station;
    if (!YoggSaronIllusionHealerStation(botAI, station))
        return false;

    if (bot->GetExactDist2d(station.GetPositionX(), station.GetPositionY()) <=
        ULDUAR_YOGG_SARON_ILLUSION_HEALER_STATION_RADIUS)
        return false;

    if (!IsYoggSaronFight() || YoggSaronInfluenceTentaclesCleared(botAI))
        return false;

    return YoggSaronWalkMakingProgress(botAI, "healer", station);
}

bool YoggSaronReprisalRegroupTrigger::IsActive()
{
    if (!YoggSaronReprisalHold(botAI) || !botAI->CanMove() || !IsInBrainLevel())
        return false;

    Player* healer = YoggSaronRoomHealer(botAI);
    return healer && (bot->GetExactDist2d(healer) > ULDUAR_YOGG_SARON_REPRISAL_REGROUP_RANGE ||
                      !healer->IsWithinLOSInMap(bot));
}

bool YoggSaronReprisalHealTrigger::IsActive()
{
    Player* held = YoggSaronReprisalHealTarget(botAI);
    return held && YoggSaronReprisalHealSpell(botAI, held);
}

bool YoggSaronBrainSpotTrigger::IsActive()
{
    if (!IsInBrainLevel() || !botAI->CanMove())
        return false;

    if (AI_VALUE(std::string, "rti") != "square")
        return false;

    if (YoggSaronShouldLeaveBrainLevel(botAI))
        return false;

    Position const spot = YoggSaronBrainSpot(bot);
    if (bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) <= ULDUAR_YOGG_SARON_BRAIN_SPOT_ARRIVED_RADIUS)
        return false;

    Creature* brain = YoggSaronNearestCreature(botAI, NPC_BRAIN);
    if (!brain || !brain->IsAlive())
        return false;

    return YoggSaronWalkMakingProgress(botAI, "brainspot", spot);
}

bool YoggSaronNitroBoostsTrigger::IsActive()
{
    if (!IsInBrainLevel() || bot->HasAura(NITRO_BOOSTS_SPEED_SPELL) || !GetNitroBoostBoots(bot) ||
        YoggSaronLeavingBrainLevel(bot))
        return false;

    if (YoggSaronRoomOf(bot) != YOGG_SARON_ROOM_CHAMBER ||
        YoggSaronRoomStateOf(botAI) != YOGG_SARON_ROOM_STATE_TO_BRAIN)
        return false;

    Position const spot = YoggSaronBrainSpot(bot);
    if (bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) <= ULDUAR_YOGG_SARON_NITRO_MIN_WALK)
        return false;

    if (bot->HasSpellCooldown(NITRO_BOOSTS_USE_SPELL))
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "yogg.nitro", "cooldown");
        return false;
    }

    return true;
}

bool YoggSaronBrainDashTrigger::IsActive()
{
    if (bot->getClass() != CLASS_DRUID || !IsInBrainLevel() || bot->GetShapeshiftForm() != FORM_CAT ||
        bot->HasAura(NITRO_BOOSTS_SPEED_SPELL) || YoggSaronLeavingBrainLevel(bot))
        return false;

    YoggSaronRoom const room = YoggSaronRoomOf(bot);
    if (room == YOGG_SARON_ROOM_CHAMBER && GetNitroBoostBoots(bot))
        return false;

    if (YoggSaronRoomStateOf(botAI) != YOGG_SARON_ROOM_STATE_TO_BRAIN)
        return false;

    Position const spot = YoggSaronBrainSpot(bot);
    if (bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) <= ULDUAR_YOGG_SARON_NITRO_MIN_WALK ||
        botAI->HasAura("dash", bot))
        return false;

    if (!botAI->CanCastSpell("dash", bot))
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "yogg.dash", "cooldown");
        return false;
    }

    return true;
}

bool YoggSaronBrainChargeTrigger::IsActive()
{
    if (bot->getClass() != CLASS_WARRIOR || !IsInBrainLevel() || AI_VALUE(std::string, "rti") != "square" ||
        !botAI->CanMove())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || target->GetEntry() != NPC_BRAIN)
        return false;

    if (YoggSaronLeavingBrainLevel(bot) || YoggSaronShouldLeaveBrainLevel(botAI) || YoggSaronBrainHeld(botAI))
        return false;

    Position const spot = YoggSaronBrainSpot(bot);
    float const walkLeft = bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY());
    if (walkLeft <= ULDUAR_YOGG_SARON_BRAIN_SPOT_ARRIVED_RADIUS)
        return false;

    float const reach = target->GetCombatReach() + bot->GetCombatReach();
    float const angle = target->GetAngle(bot);
    float const landX = target->GetPositionX() + std::cos(angle) * reach;
    float const landY = target->GetPositionY() + std::sin(angle) * reach;
    float const walkAfter = spot.GetExactDist2d(landX, landY);
    if (walkLeft - walkAfter < ULDUAR_YOGG_SARON_BRAIN_CHARGE_MIN_GAIN)
        return false;

    return botAI->CanCastSpell("charge", target) || botAI->CanCastSpell("intercept", target);
}

bool YoggSaronMoveToExitPortalTrigger::IsActive()
{
    if (!IsInBrainLevel() || !IsYoggSaronFight())
        return false;

    if (YoggSaronRoomOf(bot) != YOGG_SARON_ROOM_BRAIN && !IsBrainRoomApproachable())
        return false;

    return YoggSaronShouldLeaveBrainLevel(botAI);
}

bool YoggSaronLaughingSkullTrigger::IsActive()
{
    if (!IsInBrainLevel())
        return false;

    return !GetYoggSaronSkullsInArc(botAI).empty();
}

bool YoggSaronIllusionFacingTrigger::IsActive()
{
    if (!IsInBrainLevel() || !botAI->CanMove())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    return !GetYoggSaronSkullsInArc(botAI).empty();
}

bool YoggSaronPetGuardTrigger::IsActive()
{
    if (bot->m_Controlled.empty() || !IsYoggSaronFight())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsAlive() && target->GetEntry() == NPC_CRUSHER_TENTACLE)
        return true;

    for (Unit* controlled : bot->m_Controlled)
    {
        Unit* victim = controlled->GetVictim();
        if (victim && victim->GetEntry() == NPC_CRUSHER_TENTACLE)
            return true;

        if (Creature* creature = controlled->ToCreature(); creature && creature->HasReactState(REACT_PASSIVE))
            return true;

        if (controlled->FindNearestCreature(NPC_CRUSHER_TENTACLE, ULDUAR_YOGG_SARON_CRUSH_RANGE, true))
            return true;
    }

    return bot->GetPet() && YoggSaronPhase3PetLeftover(botAI);
}

bool YoggSaronBodyDetourTrigger::IsActive()
{
    if (!botAI->CanMove() || !IsYoggSaronFight())
        return false;

    if (bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    if (target->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY()) <
        ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS)
        return false;

    bool const inReach = PlayerbotAI::IsMelee(bot) ? bot->IsWithinMeleeRange(target)
                                             : bot->IsWithinCombatRange(target, sPlayerbotAIConfig.SpellDistance);
    if (inReach || YoggSaronRouteClearOfBody(bot, target->GetPositionX(), target->GetPositionY()))
        return false;

    uint32 const phase = YoggSaronPhase(botAI);
    return (phase == 2 || phase == 3) && !YoggSaronHoldingPortalSpot(botAI);
}

bool YoggSaronLunaticGazeTrigger::IsActive()
{
    Creature* yoggsaron = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON);
    if (!yoggsaron || !yoggsaron->IsAlive())
        return false;

    if (PlayerbotAI::IsHeal(bot) && IsPhase3())
        return true;

    return yoggsaron->HasAura(SPELL_LUNATIC_GAZE_YS);
}

bool YoggSaronBeaconDragTrigger::IsActive()
{
    bool const mayDrag = PlayerbotAI::IsTank(bot) || bot->getClass() == CLASS_HUNTER ||
                         (!PlayerbotAI::IsHeal(bot) &&
                          (bot->getClass() == CLASS_PALADIN || bot->getClass() == CLASS_DEATH_KNIGHT ||
                           (bot->getClass() == CLASS_DRUID && !PlayerbotAI::IsRanged(bot))));
    if (!mayDrag || YoggSaronOnBrainLevel(bot))
        return false;

    if (YoggSaronBeaconReturning(bot))
        return true;

    return botAI->CanMove() && YoggSaronBeaconDragGuardian(botAI, false) != nullptr;
}

bool YoggSaronBeaconStageTrigger::IsActive()
{
    return YoggSaronBeaconStageSpot(bot) && botAI->CanMove() && !YoggSaronOnBrainLevel(bot);
}

bool YoggSaronBeaconBearFormTrigger::IsActive()
{
    if (bot->GetShapeshiftForm() == FORM_BEAR || bot->GetShapeshiftForm() == FORM_DIREBEAR ||
        !YoggSaronBeaconBearHeld(botAI) || !YoggSaronBearReady(bot))
        return false;

    return botAI->CanCastSpell("dire bear form", bot) || botAI->CanCastSpell("bear form", bot);
}

bool YoggSaronPhase3YoggDotTrigger::IsActive()
{
    Unit* yogg = nullptr;
    return YoggSaronPhase3YoggDot(botAI, yogg) != nullptr;
}

bool YoggSaronPhase3LustTrigger::IsActive()
{
    if (bot->getClass() != CLASS_SHAMAN || bot->HasAura(SPELL_SATED) || bot->HasAura(SPELL_EXHAUSTION))
        return false;

    if (!IsPhase3())
        return false;

    if (!botAI->CanCastSpell("heroism", bot) && !botAI->CanCastSpell("bloodlust", bot))
        return false;

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && (member->HasAura(SPELL_HEROISM_SHAMAN) || member->HasAura(SPELL_BLOODLUST_SHAMAN)))
                return false;
        }
    }

    return YoggSaronPhase3LustReady(botAI);
}

bool YoggSaronPhase3PositioningTrigger::IsActive()
{
    if (!IsYoggSaronFight() || !IsPhase3())
        return false;

    YoggSaronSanityTrigger sanityTrigger(botAI);
    if (sanityTrigger.IsActive())
        return false;

    if (PlayerbotAI::IsTank(bot))
        return bot->GetDistance(ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT) > ULDUAR_YOGG_SARON_PHASE_3_TANK_LEASH;

    Position const& spot = YoggSaronPhaseThreeSpot(bot);

    if (bot->GetDistance2d(spot.GetPositionX(), spot.GetPositionY()) <= ULDUAR_YOGG_SARON_PHASE_3_STATION_RADIUS)
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "yogg.station", "parked");

        return false;
    }

    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsAlive() && !PhaseThreeStationReaches(target))
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "yogg.station", "released");

        return false;
    }

    if (!YoggSaronWalkMakingProgress(botAI, "station", spot))
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "yogg.station", "unreachable");

        return false;
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.station",
                             &spot == &ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT ||
                                     &spot == &ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT
                                 ? "walking"
                                 : "shifted");

    return true;
}

bool YoggSaronGuardianControlTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    uint32 const phase = YoggSaronPhase(botAI);

    if (phase == 1)
    {
        if (!IsDesignatedBotTank() || !YoggSaronInPhase1Room(bot))
            return false;

        return YoggSaronPhase1TauntTarget(botAI) != nullptr;
    }

    if (phase != 3)
        return false;

    GuidVector const targets = GetYoggSaronGuardiansInSight(botAI);
    if (YoggSaronGuardianLoose(botAI, targets))
        return true;

    Unit* current = AI_VALUE(Unit*, "current target");

    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!IsYoggSaronHoldableGuardian(unit))
            continue;

        if (YoggSaronTauntFadesShort(bot, unit))
            return true;

        if (unit != current && unit->GetExactDist2d(ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT) <=
                                   ULDUAR_YOGG_SARON_PHASE_3_MELEE_GUARDIAN_RANGE)
            return true;
    }

    return false;
}

bool YoggSaronSanityConservationTrigger::IsActive()
{
    Aura* sanityAura = bot->GetAura(SPELL_SANITY);
    if (!sanityAura || sanityAura->GetStackAmount() > ULDUAR_YOGG_SARON_SANITY_CONSERVE_THRESHOLD)
        return false;

    if (PlayerbotAI::IsBotMainTank(bot))
        return false;

    if (bot->HasAura(SPELL_BRAIN_LINK))
        return false;

    if (YoggSaronNearestCreature(botAI, NPC_SANITY_WELL))
        return false;

    if (bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return false;

    if (bot->FindNearestCreature(NPC_DEATH_RAY, ULDUAR_YOGG_SARON_DEATH_RAY_TRIGGER_RADIUS, true))
        return false;

    return true;
}

bool YoggSaronSqueezeRescueTrigger::IsActive()
{
    if (bot->getClass() != CLASS_PALADIN || !IsYoggSaronFight())
        return false;

    if (bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_SQUEEZE, bot)))
        return false;

    return YoggSaronSqueezeVictim(botAI) != nullptr;
}

bool YoggSaronSqueezeEscapeTrigger::IsActive()
{
    if (!IsYoggSaronFight())
        return false;

    if (bot->getClass() != CLASS_PALADIN && bot->getClass() != CLASS_MAGE)
        return false;

    return bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_SQUEEZE, bot));
}

bool YoggSaronNovaDefensiveTrigger::IsActive()
{
    if (!YoggSaronInPhase1Room(bot))
        return false;

    uint32 const needMs = YoggSaronNovaNeedMs(bot, true);

    return needMs && NextMagicDefensive(botAI, bot, nullptr, needMs);
}

bool YoggSaronNovaExternalTrigger::IsActive()
{
    if (!PlayerbotAI::IsHeal(bot) && bot->getClass() != CLASS_PALADIN)
        return false;

    if (!YoggSaronInPhase1Room(bot))
        return false;

    Player* victim = YoggSaronNovaVictim(botAI);

    return victim && YoggSaronNovaExternalAction::PickExternal(botAI, victim);
}

bool YoggSaronRoarAuraTrigger::IsActive()
{
    return YoggSaronRoarAuraHeld(botAI) && !bot->HasAura(SPELL_CONCENTRATION_AURA, bot->GetGUID()) &&
           botAI->CanCastSpell("concentration aura", bot);
}

bool YoggSaronRoarAuraMasteryTrigger::IsActive()
{
    if (bot->getClass() != CLASS_PALADIN || !YoggSaronRoarExpected(botAI))
        return false;

    return YoggSaronRoarCasting(botAI) && bot->HasAura(SPELL_CONCENTRATION_AURA, bot->GetGUID()) &&
           botAI->CanCastSpell("aura mastery", bot);
}

bool YoggSaronBeaconDefensiveTrigger::IsActive()
{
    if (bot->getAttackers().empty())
        return false;

    if (PlayerbotAI::IsTank(bot) && !YoggSaronBeaconDragging(bot) && !YoggSaronBeaconReturning(bot))
        return YoggSaronPileHealedOn(bot) >= ULDUAR_YOGG_SARON_BEACON_PILE_RESET_GUARDIANS &&
               NextPhysicalDefensive(botAI, bot, nullptr, 0) != 0;

    return YoggSaronHealedGuardianOn(botAI, bot) && NextPhysicalDefensive(botAI, bot, nullptr, 0) != 0;
}

bool YoggSaronBeaconExternalTrigger::IsActive()
{
    if ((!PlayerbotAI::IsHeal(bot) && bot->getClass() != CLASS_PALADIN) || PlayerbotAI::IsTank(bot))
        return false;

    if (bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return false;

    Player* victim = YoggSaronBeaconExternalVictim(botAI);

    return victim && YoggSaronNovaExternalAction::PickExternal(botAI, victim);
}

bool YoggSaronTankLayOnHandsTrigger::IsActive()
{
    return YoggSaronInPhase3(botAI) && YoggSaronTankLayOnHandsTarget(botAI);
}

bool YoggSaronVigilanceTrigger::IsActive() { return YoggSaronVigilanceTarget(botAI) != nullptr; }

bool YoggSaronTankPileFacingTrigger::IsActive() { return YoggSaronPileTurn(botAI).has_value(); }

bool YoggSaronHealerCombatEngineTrigger::IsActive()
{
    return PlayerbotAI::IsHeal(bot) && bot->IsAlive() && bot->IsInCombat() &&
           botAI->GetState() == BOT_STATE_NON_COMBAT && YoggSaronEncounterActive(botAI);
}

bool YoggSaronShadowAuraTrigger::IsActive()
{
    return IsYoggSaronShadowAuraPaladin(bot) &&
           !botAI->HasAura("shadow resistance aura", bot, false, true) &&
           botAI->CanCastSpell("shadow resistance aura", bot);
}

bool YoggSaronNovaRaidCooldownTrigger::IsActive()
{
    if (bot->getClass() != CLASS_PALADIN || !YoggSaronInPhase1Room(bot))
        return false;

    if (!YoggSaronNovaRaidCover(botAI))
        return false;

    return YoggSaronNovaHurtCount(botAI) >= ULDUAR_YOGG_SARON_NOVA_RAID_COUNT;
}

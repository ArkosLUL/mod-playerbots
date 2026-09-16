#include "UldTriggers_YoggSaron.h"

#include <algorithm>
#include <cmath>

#include "GameObject.h"
#include "Group.h"
#include "Object.h"
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

// IsBotMainTank goes false for every bot the moment a human holds main tank, which silently disabled
// both single-actor nodes. First living bot tank instead, so a human MT does not switch them off.
bool YoggSaronTrigger::IsDesignatedBotTank()
{
    if (botAI->IsBotMainTank(bot))
        return true;

    if (!botAI->IsTank(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsTank(member))
            continue;

        if (!member->GetSession() || !member->GetSession()->IsBot())
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
    Position const& spot =
        botAI->IsRanged(bot) ? ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT : ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT;
    float const reach = botAI->IsMelee(bot) ? sPlayerbotAIConfig.meleeDistance : sPlayerbotAIConfig.spellDistance;

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
    return master && !botAI->IsTank(master);
}

bool YoggSaronTrigger::IsMasterIsInBrainRoom()
{
    return YoggSaronRoomOf(botAI->GetMaster()) == YOGG_SARON_ROOM_BRAIN;
}

bool YoggSaronTrigger::IsBrainRoomApproachable() { return YoggSaronBrainRoomApproachable(botAI); }

bool YoggSaronGuardianPositioningTrigger::IsActive()
{
    if (!botAI->IsTank(bot) && !PlayerbotAI::IsMelee(bot))
        return false;

    float const fromSara =
        bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY());

    // The release radius is what makes this hysteresis rather than a boundary: let go the instant the
    // bot crosses the leash, reach melee drags it straight back out and the two trade the tick.
    bool const beyond = fromSara > (returning ? ULDUAR_YOGG_SARON_P1_LEASH_RELEASE : ULDUAR_YOGG_SARON_P1_LEASH);
    bool const inRing = fromSara < ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS;

    if (!beyond && !inRing)
    {
        returning = false;
        return false;
    }

    // The walk out of the ring wins over the leash, and is tested first because it is two sweeps
    // against the phase read's four, and one of them before Sara dies. Sara is already down by then,
    // so holding a bot on the leash only buys it a knock back a second for the rest of the fight.
    if (inRing && YoggSaronHandoverState(botAI).clearing)
    {
        returning = false;
        return true;
    }

    // A live phase-1 Guardian before the phase read, which is four 200 yd sweeps - melee sit outside
    // this leash for most of phases 2 and 3. The price is no walk back before the first spawn.
    bool const active = beyond && fromSara <= ULDUAR_YOGG_SARON_P1_ROOM_RADIUS &&
                        bot->FindNearestCreature(NPC_GUARDIAN_OF_YS, sPlayerbotAIConfig.sightDistance, true) &&
                        YoggSaronInPhase1(botAI);

    if (active != returning && RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.p1leash", active ? "returning" : "held");

    returning = active;

    return returning;
}

bool YoggSaronSanityTrigger::IsActive()
{
    // All five Sanity Wells stand on the boss platform and nothing restores Sanity underground, so
    // below the floor this node can only ever walk a bot at something it will never reach.
    if (IsInBrainLevel())
        return false;

    Aura* sanityAura = bot->GetAura(SPELL_SANITY);

    if (!sanityAura)
        return false;

    int sanityAuraStacks = sanityAura->GetStackAmount();

    // Full Sanity is false whatever the well lookup below finds, so skip the sweep.
    if (sanityAuraStacks >= 100)
        return false;

    Creature* sanityWell = YoggSaronNearestCreature(botAI, NPC_SANITY_WELL);

    if (!sanityWell)
        return false;

    float distanceToSanityWell = bot->GetDistance(sanityWell);

    if ((distanceToSanityWell >= 1.0f && sanityAuraStacks >= 40) ||
        (distanceToSanityWell < 1.0f && sanityAuraStacks >= 100))
        return false;

    // A well the bot cannot get to parked three of them stationary for 202, 164 and 96 s, suppressing
    // everything below this node for as long as it lasted.
    return YoggSaronWalkMakingProgress(botAI, "sanity", sanityWell->GetPosition());
}

bool YoggSaronPhase1SpacingTrigger::IsActive()
{
    if (!botAI->CanMove() || !YoggSaronInPhase1Room(bot))
        return false;

    // Short-radius hazard reads first, phase read last. The gate leaves every Yogg trigger open
    // between pulls, and YoggSaronInPhase1 is three 200 yd grid sweeps on every bot on the map.
    //
    // The cloud read is "I am standing in one", not "one is coming". Anticipating never worked - the
    // orbit outruns the step - but a bot left inside the summon radius farms a Guardian every 10 s
    // forever, because the aura re-arms the moment the last one lands.
    bool hazardNear = bot->FindNearestCreature(NPC_OMINOUS_CLOUD, ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS, true);

    char const* reason = hazardNear ? "cloud" : nullptr;

    // Shadow Nova is the other half, and only a Guardian about to detonate counts - running from every
    // one of them scattered the raid to the rim to be picked off one at a time.
    if (!hazardNear && !GetYoggSaronNovaThreats(botAI, ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS).empty())
        reason = bot->HasAura(SPELL_SARAS_FERVOR) ? "fervor" : "nova";

    if (!reason || !YoggSaronInPhase1(botAI))
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.p1dodge", reason);

    return true;
}

bool YoggSaronPhase1StationTrigger::IsActive()
{
    if (!botAI->CanMove())
        return false;

    // Melee and tanks have their own station, the leash that holds them on Sara.
    if (!PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsHeal(bot))
        return false;

    // Distance only, no sweep, so both reads cost nothing before the phase read.
    float const fromSara =
        bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY());
    if (fromSara > ULDUAR_YOGG_SARON_P1_ROOM_RADIUS)
        return false;

    float const fromSpot = bot->GetDistance2d(ULDUAR_YOGG_SARON_P1_RANGED_SPOT.GetPositionX(),
                                              ULDUAR_YOGG_SARON_P1_RANGED_SPOT.GetPositionY());

    // The band is what keeps the first and third orbits off the back line and it has 1.13 yd of margin
    // in hand, so the parked latch widens only the stack radius. Widening the band instead hands the
    // bot to a neighbouring orbit, which is the one thing the station exists to prevent.
    float const stack = ULDUAR_YOGG_SARON_P1_RANGED_STACK_RADIUS * (parked ? 1.5f : 1.0f);

    bool const offBand = std::abs(fromSara - ULDUAR_YOGG_SARON_P1_RANGED_STATION_RADIUS) >
                         ULDUAR_YOGG_SARON_P1_RANGED_BAND_TOLERANCE;

    char const* reason = offBand ? "band" : (fromSpot > stack ? "stack" : nullptr);

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

    // Short-radius hazard reads first, phase read last, for the reason recorded on the phase 1
    // trigger: YoggSaronInPhase2 is a 200 yd grid sweep on every bot on the map.
    bool hazardNear = bot->FindNearestCreature(NPC_DEATH_RAY, ULDUAR_YOGG_SARON_DEATH_RAY_TRIGGER_RADIUS, true);

    if (!hazardNear)
    {
        std::vector<Position> wedges = GetYoggSaronCrushWedges(botAI, ULDUAR_YOGG_SARON_CRUSH_RANGE);
        hazardNear = InYoggSaronCrushWedge(wedges, bot->GetPositionX(), bot->GetPositionY(),
                                           ULDUAR_YOGG_SARON_CRUSH_TRIGGER_ARC);
    }

    // No victim exemption here, unlike the wedge. The victim inside the reach is exactly who dies, and
    // stepping out is what stops the swings.
    if (!hazardNear && !PlayerbotAI::IsMelee(bot))
        hazardNear = YoggSaronInCrusherReach(botAI, ULDUAR_YOGG_SARON_CRUSHER_REACH_TRIGGER_RADIUS);

    // The body's ring has to be read here as well as in the action: a circle the action clears but the
    // trigger does not leaves a band where the bot stands in a hazard and the node is never asked.
    if (!hazardNear)
        hazardNear = YoggSaronInBodyKnockback(bot);

    return hazardNear && IsPhase2();
}

bool YoggSaronSetDpsPriorityTrigger::IsActive()
{
    // Tanks keep the generic picker: the resolver would pull them off whatever they are holding.
    if (botAI->IsTank(bot))
        return false;

    // Fight-wide rather than per phase, and it has to stay in step with
    // YoggSaronDpsTargetGuardMultiplier: that zeroes the stock assist over exactly this window, so a
    // narrower gate here would leave a non-tank with no target source at all.
    return IsYoggSaronFight();
}

bool YoggSaronDarkVolleyTrigger::IsActive()
{
    std::vector<char const*> const spells = YoggSaronInterruptSpells(bot);
    if (spells.empty())
        return false;

    // Answer for this bot, not for the raid. The node sits at ACTION_EMERGENCY + 1, so a bot that
    // claims the tick and only then finds every caster out of reach has spent the whole tick on
    // nothing - 63 of 92 attempts in one pull.
    for (Unit* caster : GetYoggSaronDarkVolleyCasters(botAI))
        for (char const* spell : spells)
            if (botAI->CanCastSpell(spell, caster))
                return true;

    return false;
}

bool YoggSaronDiminishPowerJudgementTrigger::IsActive()
{
    // Class check first, the grid read only for paladins. No phase read: Crushers only exist in
    // phase 2, on the platform.
    std::vector<char const*> const spells = YoggSaronJudgementSpells(bot);
    if (spells.empty())
        return false;

    // Per bot, same as the dark volley trigger: a paladin out of range must not claim the tick.
    for (Unit* crusher : GetYoggSaronChannellingCrushers(botAI))
        for (char const* spell : spells)
            if (botAI->CanCastSpell(spell, crusher))
                return true;

    return false;
}

bool YoggSaronMaladyOfTheMindTrigger::IsActive()
{
    if (!IsPhase2())
        return false;

    TooCloseToPlayerWithDebuffTrigger tooCloseToPlayerWithDebuffTrigger(botAI);
    return tooCloseToPlayerWithDebuffTrigger.TooCloseToPlayerWithDebuff(SPELL_MALADY_OF_THE_MIND, 15.0f) &&
           botAI->CanMove();
}

bool YoggSaronPhase3ControlTrigger::IsActive()
{
    if (!IsDesignatedBotTank() || !IsPhase3())
        return false;

    // The strategy swap has to land once even with nothing left to kill, so it gates on its own state
    // rather than on a guardian being up.
    TankFaceStrategy tankFaceStrategy(botAI);
    if (botAI->HasStrategy(tankFaceStrategy.getName(), BotState::BOT_STATE_COMBAT))
        return true;

    TankAssistStrategy tankAssistStrategy(botAI);
    return !botAI->HasStrategy(tankAssistStrategy.getName(), BotState::BOT_STATE_COMBAT);
}

bool YoggSaronBrainLinkTrigger::IsActive()
{
    // Not TooFarFromPlayerWithAura, which measures the gap to other holders of the same aura: Brain
    // Link puts 63802 on one end only and keeps the partner's GUID inside the aura script, so that
    // helper finds nobody to measure against. The pair comes off the cast instead, which both ends
    // can read, so this fires for the partner as well as the owner.
    return IsPhase2() && YoggSaronBrainLinkTarget(botAI) != nullptr;
}

bool YoggSaronMoveToEnterPortalTrigger::IsActive()
{
    // The spread happens before the wave exists, so "diamond" - set by the walk itself - keeps the
    // node live rather than ending it. The arena test is what stops it firing again underground,
    // where the mark still reads diamond until the illusion room node renames it.
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

    if ((rtiMark == "cross" || rtiMark == "circle" || rtiMark == "star") && bot->GetPositionZ() < ULDUAR_YOGG_SARON_BRAIN_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
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

    return bot->FindNearestCreature(NPC_DESCEND_INTO_MADNESS, ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS, true) !=
           nullptr;
}

bool YoggSaronIllusionRoomTrigger::IsActive()
{
    if (!IsYoggSaronFight() || !IsInIllusionRoom() || AI_VALUE(std::string, "rti") == "square")
        return false;

    if (SetRtiMarkRequired())
        return true;

    if (GoToBrainRoomRequired())
        return true;

    return WalkIntoRoomRequired();
}

bool YoggSaronIllusionRoomTrigger::WalkIntoRoomRequired()
{
    return YoggSaronRoomStateOf(botAI) == YOGG_SARON_ROOM_STATE_WALKING_IN;
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

bool YoggSaronMoveToExitPortalTrigger::IsActive()
{
    // The Brain sits at z 265 while its room's floor is z 236-244, so a radius to it was never "am I in
    // the brain room" - it was that question plus a permanent 25 yd vertical tax, and two of the three
    // portal arrivals land outside 60 yd before the bot takes a step. The level is the real test.
    if (!IsInBrainLevel() || !IsYoggSaronFight())
        return false;

    // An illusion room has no exit to walk to until its door opens. All three Flee to the Surface
    // portals sit in the brain chamber, 93 to 109 yd from an illusion room's middle and behind doors
    // the Brain only opens when the last Influence Tentacle in that room dies. Without this the node
    // starts hauling a bot at a shut door half a minute before Induce Madness lands: 53 of 62 walks in
    // one pull came back as the same unreachable point re-issued.
    //
    // The brain chamber itself is exempt rather than routed through the same test: the portals are in
    // there, so a bot that made it through can always leave, and the next wave's tentacles must not
    // strand it.
    if (YoggSaronRoomOf(bot) != YOGG_SARON_ROOM_BRAIN && !IsBrainRoomApproachable())
        return false;

    return YoggSaronShouldLeaveBrainLevel(botAI);
}

bool YoggSaronLaughingSkullTrigger::IsActive()
{
    // Level test first: this runs for every bot in Ulduar and the sweep behind it is not free.
    if (!IsInBrainLevel())
        return false;

    return !GetYoggSaronSkullsInArc(botAI).empty();
}

bool YoggSaronIllusionFacingTrigger::IsActive()
{
    // Level test first: this runs for every bot in Ulduar and the sweeps behind it are not free.
    if (!IsInBrainLevel() || !botAI->CanMove())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    // What is hitting the bot now, not what is in range: a bot already facing away from every skull is
    // standing somewhere that works, whatever it may have to walk through later.
    return !GetYoggSaronSkullsInArc(botAI).empty();
}

bool YoggSaronPetGuardTrigger::IsActive()
{
    if (bot->m_Controlled.empty() || !IsYoggSaronFight())
        return false;

    return bot->FindNearestCreature(NPC_CRUSHER_TENTACLE, ULDUAR_YOGG_SARON_CRUSH_RANGE, true);
}

bool YoggSaronBodyDetourTrigger::IsActive()
{
    if (!botAI->CanMove() || !IsYoggSaronFight() || !IsPhase2())
        return false;

    // Platform only. The illusion rooms are 93 yd underneath and have no body in them, and the brain
    // room middle is 2.3 yd from the platform middle in 2d, so the ring test would fire down there.
    if (bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    // Nothing to route while the bot can already shoot from where it stands - that is exactly when
    // reach stands down too, so claiming the tick here would only cost it a cast.
    float const reach = botAI->IsMelee(bot) ? sPlayerbotAIConfig.meleeDistance : sPlayerbotAIConfig.spellDistance;
    if (bot->GetExactDist2d(target) <= reach)
        return false;

    return !YoggSaronRouteClearOfBody(bot, target->GetPositionX(), target->GetPositionY());
}

bool YoggSaronLunaticGazeTrigger::IsActive()
{
    // 64163 is a 4-second aura Yogg puts on himself, ticking 64164 once a second at 130 yd through his
    // front 180 degrees - not a channel, so GetCurrentSpell never saw it. And "find target" walks the
    // bot's own threat list, which Yogg is not reliably on.
    Creature* yoggsaron = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON);

    return yoggsaron && yoggsaron->IsAlive() && yoggsaron->HasAura(SPELL_LUNATIC_GAZE_YS);
}

bool YoggSaronPhase3PositioningTrigger::IsActive()
{
    if (!IsYoggSaronFight() || !IsPhase3())
        return false;

    YoggSaronSanityTrigger sanityTrigger(botAI);
    if (sanityTrigger.IsActive())
        return false;

    // Bringing a loose guardian back to the stack is guardian control's job, and that node outranks
    // this one. All the leash adds is a way home once nothing is loose and it returns false.
    if (botAI->IsTank(bot))
        return bot->GetDistance(ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT) > ULDUAR_YOGG_SARON_PHASE_3_TANK_LEASH;

    Position const& spot =
        botAI->IsRanged(bot) ? ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT : ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT;

    if (bot->GetDistance2d(spot.GetPositionX(), spot.GetPositionY()) <= ULDUAR_YOGG_SARON_PHASE_3_STATION_RADIUS)
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "yogg.station", "parked");

        return false;
    }

    // Somewhere to stand when standing there costs nothing, never a cage: a target the station cannot
    // reach from inside its radius leaves the bot free to go to it.
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
        RaidObs::NoteDerived(bot, "yogg.station", "parked");

    return true;
}

bool YoggSaronGuardianControlTrigger::IsActive()
{
    // Tank first: both branches below are tank-only and the phase read under them is two 200 yd sweeps.
    if (!botAI->IsTank(bot))
        return false;

    uint32 const phase = YoggSaronPhase(botAI);

    // Phase 1 wants the same thing for a harder reason. There a Guardian's death location is the phase:
    // 65719 reaches Sara from 15 yd, so one killed further out is worth nothing, and one killed more
    // than 6.5 yd out novas the back line as well as the melee pile. One pull lost 4 of its 13 kills
    // 18-22 yd out and wiped one kill short of the transition.
    if (phase == 1)
    {
        // The designated bot tank rather than the main tank: IsBotMainTank goes false for every bot
        // the moment a human holds main tank, which is how the phase 3 node stayed silent for a pull.
        if (!IsDesignatedBotTank() || !YoggSaronInPhase1Room(bot))
            return false;

        return YoggSaronPhase1TauntTarget(botAI) != nullptr;
    }

    // Not gated on Thorim. Which Keepers are up is a raid choice, and hard mode means fewer of them,
    // so asking for both demanded exactly the case where Thorim is least likely to be there. Guardians
    // parked on a tank beat guardians loose among the casters even where none of them can die.
    if (phase != 3)
        return false;

    // Fire while any guardian is loose - alive and not yet held by a tank.
    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_IMMORTAL_GUARDIAN && unit->GetEntry() != NPC_MARKED_IMMORTAL_GUARDIAN)
            continue;

        Player* targetedPlayer = botAI->GetPlayer(unit->GetTarget());
        if (!targetedPlayer || !botAI->IsTank(targetedPlayer))
            return true;
    }

    return false;
}

bool YoggSaronSanityConservationTrigger::IsActive()
{
    // The main tank must stay on the adds; never pull it out.
    if (botAI->IsBotMainTank(bot))
        return false;

    Aura* sanityAura = bot->GetAura(SPELL_SANITY);
    if (!sanityAura || sanityAura->GetStackAmount() > ULDUAR_YOGG_SARON_SANITY_CONSERVE_THRESHOLD)
        return false;

    // A linked bot must stay near its Brain Link partner - retreating would drain more sanity, not less.
    if (bot->HasAura(SPELL_BRAIN_LINK))
        return false;

    // If a Sanity Well is reachable (a Keeper set with Freya), the normal sanity action handles recovery.
    if (YoggSaronNearestCreature(botAI, NPC_SANITY_WELL))
        return false;

    // Boss room only: the spot this retreats to is behind Yogg, who is not down there.
    if (bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return false;

    // Let the phase 2 spacing dodge win while a hazard is on top of the bot: a Death Ray still kills at
    // low health. The Death Orb this used to test is a marker 27 yd overhead and never reaches anyone.
    if (bot->FindNearestCreature(NPC_DEATH_RAY, ULDUAR_YOGG_SARON_DEATH_RAY_TRIGGER_RADIUS, true))
        return false;

    return true;
}

bool YoggSaronSqueezeRescueTrigger::IsActive()
{
    // Class first: it is free, and the fight test behind it is a pair of 200 yd grid sweeps run for
    // every bot in Ulduar.
    if (bot->getClass() != CLASS_PALADIN || !IsYoggSaronFight())
        return false;

    // A paladin who is held bubbles instead, which is the squeeze escape node below this one. Hand of
    // Protection cannot be cast from inside the tentacle's grip anyway.
    if (bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_SQUEEZE, bot)))
        return false;

    return YoggSaronSqueezeVictim(botAI) != nullptr;
}

bool YoggSaronSqueezeEscapeTrigger::IsActive()
{
    if (!IsYoggSaronFight())
        return false;

    // Only these two can shed a periodic damage aura outright; Feign Death and Vanish do not.
    if (bot->getClass() != CLASS_PALADIN && bot->getClass() != CLASS_MAGE)
        return false;

    return bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_SQUEEZE, bot));
}

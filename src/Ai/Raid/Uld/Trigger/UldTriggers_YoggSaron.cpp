#include "UldTriggers_YoggSaron.h"

#include "GameObject.h"
#include "Group.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_YoggSaron.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "SpellMgr.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include "WorldSession.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <CombatStrategy.h>
#include <TankAssistStrategy.h>

using namespace EncounterHelpers;

Unit* YoggSaronTrigger::GetSaraIfAlive()
{
    // Not "find target": that walks the bot's own threat list, and Sara is FACTION_FRIENDLY for the
    // whole of phase 1 and only ever takes damage from a Guardian's Shadow Nova. No bot ever holds
    // threat on her, so the lookup returned null every tick and took all 21 Yogg nodes down with it.
    return bot->FindNearestCreature(NPC_SARA_PHASE_1, 200.0f, true);
}

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

bool YoggSaronTrigger::IsInBrainLevel()
{
    return bot->GetPositionZ() > 230.0f && bot->GetPositionZ() < 250.0f;
}

bool YoggSaronTrigger::IsYoggSaronFight()
{
    // Same threat-list trap as GetSaraIfAlive, and Yogg is no better: he is not reliably on a bot's
    // threat list either.
    return bot->FindNearestCreature(NPC_SARA_PHASE_1, 200.0f, true) ||
           bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);
}

bool YoggSaronTrigger::IsInIllusionRoom()
{
    if (!IsInBrainLevel())
        return false;

    if (IsInStormwindKeeperIllusion())
        return true;

    if (IsInIcecrownKeeperIllusion())
        return true;

    if (IsInChamberOfTheAspectsIllusion())
        return true;

    return false;
}

bool YoggSaronTrigger::IsInStormwindKeeperIllusion()
{
    return bot->GetDistance2d(ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE.GetPositionX(),
                              ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE.GetPositionY()) <
           ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS;
}

bool YoggSaronTrigger::IsInIcecrownKeeperIllusion()
{
    return bot->GetDistance2d(ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE.GetPositionX(),
                              ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE.GetPositionY()) <
           ULDUAR_YOGG_SARON_ICECROWN_CITADEL_RADIUS;
}

bool YoggSaronTrigger::IsInChamberOfTheAspectsIllusion()
{
    return bot->GetDistance2d(ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE.GetPositionX(),
                              ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE.GetPositionY()) <
           ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_RADIUS;
}

bool YoggSaronTrigger::IsMasterIsInIllusionGroup()
{
    Player* master = botAI->GetMaster();
    return master && !botAI->IsTank(master);
}

bool YoggSaronTrigger::IsMasterIsInBrainRoom()
{
    Player* master = botAI->GetMaster();

    if (!master)
        return false;

    return master->GetDistance2d(ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionX(),
                                 ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionY()) <
               ULDUAR_YOGG_SARON_BRAIN_ROOM_RADIUS &&
           master->GetPositionZ() > 230.0f && master->GetPositionZ() < 250.0f;
}

Position YoggSaronTrigger::GetIllusionRoomEntrancePosition()
{
    if (IsInChamberOfTheAspectsIllusion())
        return ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE;
    else if (IsInIcecrownKeeperIllusion())
        return ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE;
    else if (IsInStormwindKeeperIllusion())
        return ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE;
    else
        return Position();
}

// Two reads of the one server fact, neither of them a human: the Brain opens the room's door in the
// same branch that fires when the last Influence Tentacle dies. Getting it wrong now means standing
// still rather than walking in and dying, and the exit node rescues that.
bool YoggSaronTrigger::IsBrainRoomApproachable()
{
    if (!YoggSaronInfluenceTentaclesCleared(botAI))
        return false;

    uint32 doorEntry = 0;
    if (IsInChamberOfTheAspectsIllusion())
        doorEntry = GO_CHAMBER_ILLUSION_DOORS;
    else if (IsInIcecrownKeeperIllusion())
        doorEntry = GO_ICECROWN_ILLUSION_DOORS;
    else if (IsInStormwindKeeperIllusion())
        doorEntry = GO_STORMWIND_ILLUSION_DOORS;
    else
        return false;

    GameObject* door = bot->FindNearestGameObject(doorEntry, 200.0f);

    return door && door->GetGoState() == GO_STATE_ACTIVE;
}

Unit* YoggSaronTrigger::GetNextIllusionRoomRtiTarget()
{
    float detectionRadius = 0.0f;
    if (IsInStormwindKeeperIllusion())
        detectionRadius = ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS;
    else if (IsInIcecrownKeeperIllusion())
        detectionRadius = ULDUAR_YOGG_SARON_ICECROWN_CITADEL_RADIUS;
    else if (IsInChamberOfTheAspectsIllusion())
        detectionRadius = ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_RADIUS;
    else
        return nullptr;

    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");

    if (botAI->HasCheat(BotCheatMask::raid))
    {
        for (ObjectGuid const& guid : targets)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (unit && unit->IsAlive() && unit->GetEntry() == NPC_LAUGHING_SKULL)
                return unit;
        }
    }

    float nearestDistance = std::numeric_limits<float>::max();
    Unit* nextIllusionRoomRtiTarget = nullptr;

    for (uint32 const& creatureId : ULDUAR_YOGG_SARON_ILLUSION_MOBS)
    {
        for (ObjectGuid const& guid : targets)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (unit && unit->IsAlive() && unit->GetEntry() == creatureId)
            {
                float distance = bot->GetDistance(unit);
                if (distance < nearestDistance)
                {
                    nextIllusionRoomRtiTarget = unit;
                    nearestDistance = distance;
                }
            }
        }
    }

    if (nextIllusionRoomRtiTarget)
        return nextIllusionRoomRtiTarget;

    if (IsInStormwindKeeperIllusion())
    {
        Creature* target = bot->FindNearestCreature(NPC_SUIT_OF_ARMOR, detectionRadius, true);
        if (target)
            return target;
    }

    return nullptr;
}

bool YoggSaronOminousCloudCheatTrigger::IsActive()
{
    if (!botAI->HasCheat(BotCheatMask::raid))
        return false;

    Unit* boss = GetSaraIfAlive();
    if (!boss)
        return false;

    if (!IsDesignatedBotTank())
        return false;

    if (bot->GetDistance2d(boss->GetPositionX(), boss->GetPositionY()) > 50.0f)
        return false;

    Creature* target = boss->FindNearestCreature(NPC_OMINOUS_CLOUD, 25.0f, true);

    return target;
}

bool YoggSaronGuardianPositioningTrigger::IsActive()
{
    if (!GetSaraIfAlive())
        return false;

    if (!botAI->IsTank(bot))
        return false;

    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    bool thereIsAnyGuardian = false;

    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() == NPC_GUARDIAN_OF_YS)
        {
            thereIsAnyGuardian = true;
            ObjectGuid unitTargetGuid = unit->GetTarget();
            Player* targetedPlayer = botAI->GetPlayer(unitTargetGuid);
            if (!targetedPlayer || !botAI->IsTank(targetedPlayer))
                return false;
        }
    }

    return thereIsAnyGuardian &&
           bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY()) > 1.0f;
}

bool YoggSaronSanityTrigger::IsActive()
{
    Aura* sanityAura = bot->GetAura(SPELL_SANITY);

    if (!sanityAura)
        return false;

    int sanityAuraStacks = sanityAura->GetStackAmount();

    Creature* sanityWell = bot->FindNearestCreature(NPC_SANITY_WELL, 200.0f);

    if (!sanityWell)
        return false;

    float distanceToSanityWell = bot->GetDistance(sanityWell);

    return (distanceToSanityWell >= 1.0f && sanityAuraStacks < 40) ||
           (distanceToSanityWell < 1.0f && sanityAuraStacks < 100);
}

bool YoggSaronPhase1SpacingTrigger::IsActive()
{
    if (!botAI->CanMove())
        return false;

    // Short-radius hazard reads first, phase read last. The gate leaves every Yogg trigger open
    // between pulls, and YoggSaronInPhase1 is three 200 yd grid sweeps on every bot on the map.
    //
    // Everyone dodges clouds: one summons a Guardian on any player inside 6 yd, and the innermost
    // orbit runs at 11 yd from Sara, straight through where melee stand.
    bool hazardNear = bot->FindNearestCreature(NPC_OMINOUS_CLOUD, ULDUAR_YOGG_SARON_CLOUD_TRIGGER_RADIUS, true);

    // Shadow Nova is the other half, and only ranged and healers can answer it. Melee and tanks have
    // to stand in it to kill the Guardian, which is also the only way Sara takes damage.
    if (!hazardNear && (PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsHeal(bot)))
    {
        hazardNear =
            bot->FindNearestCreature(NPC_GUARDIAN_OF_YS, ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS, true);
    }

    return hazardNear && YoggSaronInPhase1(botAI);
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

    return hazardNear && IsPhase2();
}

bool YoggSaronSetDpsPriorityTrigger::IsActive()
{
    // Tanks keep the generic picker: the resolver would pull them off whatever they are holding.
    if (botAI->IsTank(bot))
        return false;

    return IsPhase2() || IsPhase3();
}

bool YoggSaronDarkVolleyTrigger::IsActive()
{
    if (!YoggSaronCanInterrupt(bot))
        return false;

    return !GetYoggSaronDarkVolleyCasters(botAI).empty();
}

bool YoggSaronMaladyOfTheMindTrigger::IsActive()
{
    TooCloseToPlayerWithDebuffTrigger tooCloseToPlayerWithDebuffTrigger(botAI);
    return IsPhase2() && tooCloseToPlayerWithDebuffTrigger.TooCloseToPlayerWithDebuff(SPELL_MALADY_OF_THE_MIND, 15.0f) && botAI->CanMove();
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
    if (!botAI->HasStrategy(tankAssistStrategy.getName(), BotState::BOT_STATE_COMBAT))
        return true;

    // All that is left is the cheat, and only an Immortal Guardian above the Weakened floor can take
    // it. With Thorim as a Keeper the raid plays it for real instead: melee cleave the guardian down
    // and Titanic Storm executes it.
    if (!botAI->HasCheat(BotCheatMask::raid) || (IsYoggSaronHardModeActive(botAI) && YoggThorimKeeperActive(botAI)))
        return false;

    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if ((unit->GetEntry() == NPC_IMMORTAL_GUARDIAN || unit->GetEntry() == NPC_MARKED_IMMORTAL_GUARDIAN) &&
            unit->GetHealthPct() > 10)
            return true;
    }

    return false;
}

bool YoggSaronBrainLinkTrigger::IsActive()
{
    TooFarFromPlayerWithAuraTrigger tooFarFromPlayerWithAuraTrigger(botAI);
    return IsPhase2() && bot->HasAura(SPELL_BRAIN_LINK) &&
           tooFarFromPlayerWithAuraTrigger.TooFarFromPlayerWithAura(SPELL_BRAIN_LINK, 20.0f, false);
}

bool YoggSaronMoveToEnterPortalTrigger::IsActive()
{
    if (!IsPhase2())
        return false;

    Creature* portal = bot->FindNearestCreature(NPC_DESCEND_INTO_MADNESS, 100.0f, true);
    if (!portal)
        return false;

    if (bot->GetDistance2d(portal->GetPositionX(), portal->GetPositionY()) < 2.0f)
        return false;

    if (AI_VALUE(std::string, "rti") != "skull")
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    int brainRoomTeamCount = 10;
    if (bot->GetRaidDifficulty() == Difficulty::RAID_DIFFICULTY_10MAN_NORMAL)
        brainRoomTeamCount = 4;

    if (IsMasterIsInIllusionGroup())
        brainRoomTeamCount--;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !member->IsAlive() || botAI->IsTank(member))
            continue;

        if (member->GetGUID() == bot->GetGUID())
            return true;

        brainRoomTeamCount--;
        if (brainRoomTeamCount == 0)
            break;
    }

    return false;
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

bool YoggSaronBossRoomMovementCheatTrigger::IsActive()
{
    if (!IsYoggSaronFight() || !IsPhase2())
        return false;

    FollowMasterStrategy followMasterStrategy(botAI);
    if (botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
        return true;

    if (!botAI->HasCheat(BotCheatMask::raid))
        return false;

    if (AI_VALUE(std::string, "rti") != "skull")
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");

    if (!target || !target->IsAlive() ||
        bot->GetDistance2d(target->GetPositionX(), target->GetPositionY()) < 40.0f)
        return false;

    return true;
}

bool YoggSaronUsePortalTrigger::IsActive()
{
    if (!IsPhase2())
        return false;

    if (AI_VALUE(std::string, "rti") != "diamond")
        return false;

    return bot->FindNearestCreature(NPC_DESCEND_INTO_MADNESS, 2.0f, true) != nullptr;
}

bool YoggSaronIllusionRoomTrigger::IsActive()
{
    if (!IsYoggSaronFight() || !IsInIllusionRoom() || AI_VALUE(std::string, "rti") == "square")
        return false;

    if (SetRtiMarkRequired())
        return true;

    if (SetRtiTargetRequired())
        return true;

    if (GoToBrainRoomRequired())
        return true;

    return false;
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

// Cheat-gated because the cheat is all that is left here: with targeting off the raid icons, a bot
// without it reaches the room's adds through the dps priority resolver like anything else.
bool YoggSaronIllusionRoomTrigger::SetRtiTargetRequired()
{
    return botAI->HasCheat(BotCheatMask::raid) && GetNextIllusionRoomRtiTarget() != nullptr;
}

bool YoggSaronMoveToExitPortalTrigger::IsActive()
{
    if (!IsYoggSaronFight() || !IsInBrainLevel())
        return false;

    // The Brain sits at z 265 while its room's floor is z 236-244, so a radius to it was never "am I in
    // the brain room" - it was that question plus a permanent 25 yd vertical tax, and two of the three
    // portal arrivals land outside 60 yd before the bot takes a step. IsInBrainLevel is the real test;
    // this only has to find the map's one Brain.
    Creature const* brain = bot->FindNearestCreature(NPC_BRAIN, 200.0f, true);
    if (!brain || !brain->IsAlive())
        return false;

    if (brain->HasUnitState(UNIT_STATE_CASTING))
    {
        Spell* induceMadnessSpell = brain->GetCurrentSpell(CURRENT_GENERIC_SPELL);

        if (induceMadnessSpell && induceMadnessSpell->m_spellInfo->Id == SPELL_INDUCE_MADNESS)
        {
            uint32 castingTimeLeft = induceMadnessSpell->GetCastTimeRemaining();
            uint32 lead = botAI->HasCheat(BotCheatMask::raid) ? ULDUAR_YOGG_SARON_EXIT_LEAD_CHEAT_MS
                                                             : ULDUAR_YOGG_SARON_EXIT_LEAD_WALK_MS;
            if (castingTimeLeft < lead)
                return true;
        }
    }
    else if (brain->GetHealth() < brain->GetMaxHealth() * 0.3f)
        return true;

    return false;
}

bool YoggSaronLunaticGazeTrigger::IsActive()
{
    // 64163 is a 4-second aura Yogg puts on himself, ticking 64164 once a second at 130 yd through his
    // front 180 degrees - not a channel, so GetCurrentSpell never saw it. And "find target" walks the
    // bot's own threat list, which Yogg is not reliably on.
    Creature* yoggsaron = bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);

    return yoggsaron && yoggsaron->IsAlive() && yoggsaron->HasAura(SPELL_LUNATIC_GAZE_YS);
}

bool YoggSaronPhase3PositioningTrigger::IsActive()
{
    if (!IsYoggSaronFight() || !IsPhase3())
        return false;

    YoggSaronSanityTrigger sanityTrigger(botAI);
    if (sanityTrigger.IsActive())
        return false;

    if (botAI->IsRanged(bot) && bot->GetDistance2d(ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionX(),
                                                   ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionY()) > 15.0f)
        return true;

    if (botAI->IsMelee(bot) && !botAI->IsTank(bot) &&
        bot->GetDistance2d(ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
            ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY()) > 15.0f)
    {
        return true;
    }

    if (botAI->IsTank(bot))
    {
        if (bot->GetDistance(ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT) > 30.0f)
            return true;

        GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
        bool thereIsAnyGuardian = false;

        for (ObjectGuid const& guid : targets)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive())
                continue;

            if (unit->GetEntry() == NPC_IMMORTAL_GUARDIAN || unit->GetEntry() == NPC_MARKED_IMMORTAL_GUARDIAN)
            {
                thereIsAnyGuardian = true;
                ObjectGuid unitTargetGuid = unit->GetTarget();
                Player* targetedPlayer = botAI->GetPlayer(unitTargetGuid);
                if (!targetedPlayer || !botAI->IsTank(targetedPlayer))
                    return false;
            }
        }

        if (thereIsAnyGuardian && bot->GetDistance2d(ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
                                                     ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY()) > 3.0f)
        {
            return true;
        }
    }

    return false;
}

bool YoggSaronGuardianControlTrigger::IsActive()
{
    // Only meaningful with Thorim: he executes the Weakened guardians the tank feeds to the melee stack.
    if (!IsYoggSaronHardModeActive(botAI) || !YoggThorimKeeperActive(botAI) || !IsPhase3())
        return false;

    if (!botAI->IsTank(bot))
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
    if (!IsYoggSaronHardModeActive(botAI))
        return false;

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
    if (bot->FindNearestCreature(NPC_SANITY_WELL, 200.0f))
        return false;

    // Only in the boss room; the brain/illusion level is cheated through.
    if (bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return false;

    // Let the phase 2 spacing dodge win while a hazard is on top of the bot: a Death Ray still kills at
    // low health. The Death Orb this used to test is a marker 27 yd overhead and never reaches anyone.
    if (bot->FindNearestCreature(NPC_DEATH_RAY, ULDUAR_YOGG_SARON_DEATH_RAY_TRIGGER_RADIUS, true))
        return false;

    return true;
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

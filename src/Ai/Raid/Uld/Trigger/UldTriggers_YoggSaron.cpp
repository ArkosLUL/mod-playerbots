#include "UldTriggers_YoggSaron.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <RtiTargetValue.h>

const std::vector<uint32> illusionMobs =
{
    NPC_INFLUENCE_TENTACLE,
    NPC_RUBY_CONSORT,
    NPC_AZURE_CONSORT,
    NPC_BRONZE_CONSORT,
    NPC_EMERALD_CONSORT,
    NPC_OBSIDIAN_CONSORT,
    NPC_ALEXTRASZA,
    NPC_MALYGOS_ILLUSION,
    NPC_NELTHARION,
    NPC_YSERA,
    NPC_DEATHSWORN_ZEALOT,
    NPC_LICH_KING_ILLUSION,
    NPC_IMMOLATED_CHAMPION,
    NPC_SUIT_OF_ARMOR,
    NPC_GARONA,
    NPC_KING_LLANE
};

Unit* YoggSaronTrigger::GetSaraIfAlive()
{
    Unit* sara = AI_VALUE2(Unit*, "find target", "sara");
    if (!sara || !sara->IsAlive())
        return nullptr;

    return sara;
}

bool YoggSaronTrigger::IsPhase2()
{
    Creature* target = bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);

    return target && target->IsAlive() && target->HasAura(SPELL_SHADOW_BARRIER);
}

bool YoggSaronTrigger::IsPhase3()
{
    Creature* target = bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);
    Creature* guardian = bot->FindNearestCreature(NPC_GUARDIAN_OF_YS, 200.0f, true);

    return target && target->IsAlive() && !target->HasAura(SPELL_SHADOW_BARRIER) && !guardian;
}

bool YoggSaronTrigger::IsInBrainLevel()
{
    return bot->GetPositionZ() > 230.0f && bot->GetPositionZ() < 250.0f;
}

bool YoggSaronTrigger::IsYoggSaronFight()
{
    Unit* sara = AI_VALUE2(Unit*, "find target", "sara");
    Unit* yoggsaron = AI_VALUE2(Unit*, "find target", "yogg-saron");

    if ((sara && sara->IsAlive()) || (yoggsaron && yoggsaron->IsAlive()))
        return true;

    return false;
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

Unit* YoggSaronTrigger::GetIllusionRoomRtiTarget()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    int32_t rtiIndex = RtiTargetValue::GetRtiIndex(AI_VALUE(std::string, "rti"));
    if (rtiIndex == -1)
        return nullptr;  // Invalid RTI mark

    ObjectGuid currentRtiTarget = group->GetTargetIcon(rtiIndex);
    Unit* currentRtiTargetUnit = botAI->GetUnit(currentRtiTarget);
    if (!currentRtiTargetUnit || !currentRtiTargetUnit->IsAlive())
        currentRtiTargetUnit = nullptr;

    return currentRtiTargetUnit;
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
        for (const ObjectGuid& guid : targets)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (unit && unit->IsAlive() && unit->GetEntry() == NPC_LAUGHING_SKULL)
                return unit;
        }
    }

    float nearestDistance = std::numeric_limits<float>::max();
    Unit* nextIllusionRoomRtiTarget = nullptr;

    for (const uint32& creatureId : illusionMobs)
    {
        for (const ObjectGuid& guid : targets)
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

    if (!botAI->IsBotMainTank(bot))
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

    for (const ObjectGuid& guid : targets)
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

bool YoggSaronDeathOrbTrigger::IsActive()
{
    TooCloseToCreatureTrigger tooCloseToDeathOrbTrigger(botAI);
    return IsPhase2() && tooCloseToDeathOrbTrigger.TooCloseToCreature(NPC_DEATH_ORB, 10.0f);
}

bool YoggSaronMaladyOfTheMindTrigger::IsActive()
{
    TooCloseToPlayerWithDebuffTrigger tooCloseToPlayerWithDebuffTrigger(botAI);
    return IsPhase2() && tooCloseToPlayerWithDebuffTrigger.TooCloseToPlayerWithDebuff(SPELL_MALADY_OF_THE_MIND, 15.0f) && botAI->CanMove();
}

bool YoggSaronMarkTargetTrigger::IsActive()
{
    if (!IsYoggSaronFight())
        return false;

    if (!botAI->IsBotMainTank(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    if (IsPhase2())
    {
        ObjectGuid currentMoonTarget = group->GetTargetIcon(RtiTargetValue::moonIndex);
        Creature* yogg_saron = bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);
        if (!currentMoonTarget || currentMoonTarget != yogg_saron->GetGUID())
            return true;

        ObjectGuid currentSkullTarget = group->GetTargetIcon(RtiTargetValue::skullIndex);

        Creature* nextPossibleTarget = bot->FindNearestCreature(NPC_CONSTRICTOR_TENTACLE, 200.0f, true);
        if (!nextPossibleTarget)
        {
            nextPossibleTarget = bot->FindNearestCreature(NPC_CORRUPTOR_TENTACLE, 200.0f, true);
            if (!nextPossibleTarget)
                return false;
        }

        if (currentSkullTarget)
        {
            Unit* currentSkullUnit = botAI->GetUnit(currentSkullTarget);

            if (!currentSkullUnit)
                return true;

            if (currentSkullUnit->IsAlive() && currentSkullUnit->GetGUID() == nextPossibleTarget->GetGUID())
                return false;
        }

        return true;
    }
    else if (IsPhase3())
    {
        ObjectGuid currentSkullTarget = group->GetTargetIcon(RtiTargetValue::skullIndex);
        Unit* currentSkullUnit = nullptr;
        if (currentSkullTarget)
            currentSkullUnit = botAI->GetUnit(currentSkullTarget);

        if (currentSkullUnit &&
            (currentSkullUnit->GetEntry() == NPC_IMMORTAL_GUARDIAN ||
             currentSkullUnit->GetEntry() == NPC_MARKED_IMMORTAL_GUARDIAN) &&
            currentSkullUnit->GetHealthPct() > 10)
        {
            return false;
        }

        GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
        for (const ObjectGuid& guid : targets)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive())
                continue;

            if ((unit->GetEntry() == NPC_IMMORTAL_GUARDIAN || unit->GetEntry() == NPC_MARKED_IMMORTAL_GUARDIAN) &&
                unit->GetHealthPct() > 10)
                return true;
        }

        if (!currentSkullUnit || currentSkullUnit->GetEntry() != NPC_YOGG_SARON)
            return true;

        return false;
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

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ObjectGuid currentSkullTarget = group->GetTargetIcon(RtiTargetValue::skullIndex);

    if (!currentSkullTarget)
        return false;

    Unit* currentSkullUnit = botAI->GetUnit(currentSkullTarget);

    if (!currentSkullUnit || !currentSkullUnit->IsAlive() || bot->GetDistance2d(currentSkullUnit->GetPositionX(), currentSkullUnit->GetPositionY()) < 40.0f)
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

    return IsMasterIsInBrainRoom();
}

bool YoggSaronIllusionRoomTrigger::SetRtiMarkRequired()
{
    return AI_VALUE(std::string, "rti") == "diamond";
}

bool YoggSaronIllusionRoomTrigger::SetRtiTargetRequired()
{
    Unit const* currentRtiTarget = GetIllusionRoomRtiTarget();
    if (currentRtiTarget != nullptr)
        return false;

    return GetNextIllusionRoomRtiTarget() != nullptr;
}

bool YoggSaronMoveToExitPortalTrigger::IsActive()
{
    if (!IsYoggSaronFight() || !IsInBrainLevel())
        return false;

    Creature const* brain = bot->FindNearestCreature(NPC_BRAIN, 60.0f, true);
    if (!brain || !brain->IsAlive())
        return false;

    if (brain->HasUnitState(UNIT_STATE_CASTING))
    {
        Spell* induceMadnessSpell = brain->GetCurrentSpell(CURRENT_GENERIC_SPELL);

        if (induceMadnessSpell && induceMadnessSpell->m_spellInfo->Id == SPELL_INDUCE_MADNESS)
        {
            uint32 castingTimeLeft = induceMadnessSpell->GetCastTimeRemaining();
            if ((botAI->HasCheat(BotCheatMask::raid) && castingTimeLeft < 6000) ||
                (!botAI->HasCheat(BotCheatMask::raid) && castingTimeLeft < 8000))
            {
                return true;
            }
        }
    }
    else if (brain->GetHealth() < brain->GetMaxHealth() * 0.3f)
        return true;

    return false;
}

bool YoggSaronLunaticGazeTrigger::IsActive()
{
    Unit* yoggsaron = AI_VALUE2(Unit*, "find target", "yogg-saron");

    if (yoggsaron && yoggsaron->IsAlive() && yoggsaron->HasUnitState(UNIT_STATE_CASTING))
    {
        Spell* currentSpell = yoggsaron->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (currentSpell && currentSpell->m_spellInfo->Id == SPELL_LUNATIC_GAZE_YS)
            return true;
    }

    return false;
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

        for (const ObjectGuid& guid : targets)
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

bool YoggSaronCrusherTentacleTrigger::IsActive()
{
    if (!IsYoggSaronHardModeActive(botAI) || !IsPhase2())
        return false;

    // Ranged DPS handle the stationary Crusher Tentacle; melee stay on the other targets and healers heal.
    if (!botAI->IsRangedDps(bot))
        return false;

    Unit* crusher = GetFirstAliveUnitByEntry(botAI, NPC_CRUSHER_TENTACLE);
    if (!crusher)
        return false;

    // Only fire when the bot is not already on it.
    return AI_VALUE(Unit*, "current target") != crusher;
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
    for (const ObjectGuid& guid : targets)
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

    // Let the death-orb dodge win when an orb is near (death rays still hurt at low health).
    TooCloseToCreatureTrigger tooCloseToDeathOrb(botAI);
    if (tooCloseToDeathOrb.TooCloseToCreature(NPC_DEATH_ORB, 10.0f))
        return false;

    return true;
}

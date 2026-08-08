#include "UldActions_YoggSaron.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <cmath>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

const Position ULDUAR_YOGG_SARON_BOSS_ROOM_RESTORE_POINT = Position(1928.8923f, -24.871964f, 324.88956f, 6.247805f);

const Position yoggPortalLoc[] = {
    {1970.48f, -9.75f, 325.5f},  {1992.76f, -10.21f, 325.5f}, {1995.53f, -39.78f, 325.5f}, {1969.25f, -42.00f, 325.5f},
    {1960.62f, -32.00f, 325.5f}, {1981.98f, -5.69f, 325.5f},  {1982.78f, -45.73f, 325.5f}, {2000.66f, -29.68f, 325.5f},
    {1999.88f, -19.61f, 325.5f}, {1961.37f, -19.54f, 325.5f}};

bool YoggSaronOminousCloudCheatAction::Execute(Event /*event*/)
{
    YoggSaronTrigger yoggSaronTrigger(botAI);

    Unit* boss = yoggSaronTrigger.GetSaraIfAlive();
    if (!boss)
        return false;

    Creature* target = boss->FindNearestCreature(NPC_OMINOUS_CLOUD, 25.0f);
    if (!target || !target->IsAlive())
        return false;

    target->Kill(bot, target);
    return true;
}

bool YoggSaronGuardianPositioningAction::Execute(Event /*event*/)
{
    return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY(),
                  ULDUAR_YOGG_SARON_MIDDLE.GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_FORCED, true, false);
}

bool YoggSaronSanityAction::Execute(Event /*event*/)
{
    Creature* sanityWell = bot->FindNearestCreature(NPC_SANITY_WELL, 200.0f);
    if (!sanityWell)
        return false;

    return MoveTo(bot->GetMapId(), sanityWell->GetPositionX(), sanityWell->GetPositionY(), sanityWell->GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_FORCED,
                  true, false);
}

bool YoggSaronMarkTargetAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    YoggSaronTrigger yoggSaronTrigger(botAI);
    if (yoggSaronTrigger.IsPhase2())
    {
        // In reduced-Keeper hard mode the Crusher Tentacles are played for real (ranged nuke them in
        // place), so skip the cheat instakill; normal mode keeps it.
        if (botAI->HasCheat(BotCheatMask::raid) && !IsYoggSaronHardModeActive(botAI))
        {
            Unit* crusherTentacle = bot->FindNearestCreature(NPC_CRUSHER_TENTACLE, 200.0f, true);
            if (crusherTentacle)
                crusherTentacle->Kill(bot, crusherTentacle);
        }

        ObjectGuid currentMoonTarget = group->GetTargetIcon(RtiTargetValue::moonIndex);
        Creature* yogg_saron = bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);
        if (!currentMoonTarget || currentMoonTarget != yogg_saron->GetGUID())
        {
            group->SetTargetIcon(RtiTargetValue::moonIndex, bot->GetGUID(), yogg_saron->GetGUID());
            return true;
        }

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

            if (currentSkullUnit && currentSkullUnit->IsAlive() &&
                currentSkullUnit->GetGUID() == nextPossibleTarget->GetGUID())
                return false;
        }

        group->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), nextPossibleTarget->GetGUID());
    }
    else if (yoggSaronTrigger.IsPhase3())
    {
        TankFaceStrategy tankFaceStrategy(botAI);
        if (botAI->HasStrategy(tankFaceStrategy.getName(), BotState::BOT_STATE_COMBAT))
            botAI->ChangeStrategy(REMOVE_STRATEGY_CHAR + tankFaceStrategy.getName(), BotState::BOT_STATE_COMBAT);

        TankAssistStrategy tankAssistStrategy(botAI);
        if (!botAI->HasStrategy(tankAssistStrategy.getName(), BotState::BOT_STATE_COMBAT))
            botAI->ChangeStrategy(ADD_STRATEGY_CHAR + tankAssistStrategy.getName(), BotState::BOT_STATE_COMBAT);

        GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");

        int lowestHealth = std::numeric_limits<int>::max();
        Unit* lowestHealthUnit = nullptr;
        for (const ObjectGuid& guid : targets)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive())
                continue;

            if ((unit->GetEntry() == NPC_IMMORTAL_GUARDIAN || unit->GetEntry() == NPC_MARKED_IMMORTAL_GUARDIAN) &&
                unit->GetHealthPct() > 10)
            {
                if (unit->GetHealth() < uint32(lowestHealth))
                {
                    lowestHealth = unit->GetHealth();
                    lowestHealthUnit = unit;
                }
            }
        }

        if (lowestHealthUnit)
        {
            // Added because lunatic gaze freeze all bots and they can't attack
            // If someone fix it then this cheat can be removed.
            // In reduced-Keeper hard mode with Thorim present we play it for real instead: the tank
            // brings the guardian to the melee stack, they cleave it to Weakened, and Thorim's Titanic
            // Storm executes it. Fall back to the cheat when hard mode is off or Thorim is not a Keeper -
            // nothing else can kill a Weakened guardian, so it would be immortal.
            if (botAI->HasCheat(BotCheatMask::raid) &&
                !(IsYoggSaronHardModeActive(botAI) && YoggThorimKeeperActive(botAI)))
                lowestHealthUnit->Kill(bot, lowestHealthUnit);
            else
                group->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), lowestHealthUnit->GetGUID());

            return true;
        }

        ObjectGuid currentSkullTarget = group->GetTargetIcon(RtiTargetValue::skullIndex);
        Unit* currentSkullUnit = nullptr;
        if (currentSkullTarget)
            currentSkullUnit = botAI->GetUnit(currentSkullTarget);

        if (!currentSkullUnit || currentSkullUnit->GetEntry() != NPC_YOGG_SARON)
        {
            Unit* yoggsaron = AI_VALUE2(Unit*, "find target", "yogg-saron");
            if (yoggsaron && yoggsaron->IsAlive())
            {
                group->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), yoggsaron->GetGUID());
                return true;
            }
        }

        return false;
    }

    return false;
}

bool YoggSaronBrainLinkAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (player && player->IsAlive() && player->HasAura(SPELL_BRAIN_LINK) && player->GetGUID() != bot->GetGUID())
            return MoveNear(player, 10.0f, MovementPriority::MOVEMENT_FORCED);
    }

    return false;
}

bool YoggSaronMoveToEnterPortalAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    bool isInBrainRoomTeam = false;
    int portalNumber = 0;
    int brainRoomTeamCount = 10;
    if (bot->GetRaidDifficulty() == Difficulty::RAID_DIFFICULTY_10MAN_NORMAL)
        brainRoomTeamCount = 4;

    Player* master = botAI->GetMaster();
    if (master && !botAI->IsTank(master))
    {
        portalNumber++;
        brainRoomTeamCount--;
    }

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !member->IsAlive() || botAI->IsTank(member) || botAI->GetMaster()->GetGUID() == member->GetGUID())
            continue;

        portalNumber++;
        if (member->GetGUID() == bot->GetGUID())
        {
            isInBrainRoomTeam = true;
            break;
        }

        brainRoomTeamCount--;
        if (brainRoomTeamCount == 0)
            break;
    }

    if (!isInBrainRoomTeam)
        return false;

    Position assignedPortalPosition = yoggPortalLoc[portalNumber - 1];

    botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("diamond");

    if (botAI->HasCheat(BotCheatMask::raid))
    {
        return bot->TeleportTo(bot->GetMapId(), assignedPortalPosition.GetPositionX(),
                                      assignedPortalPosition.GetPositionY(),
                        assignedPortalPosition.GetPositionZ(), bot->GetOrientation());
    }
    else
    {
        return MoveNear(bot->GetMapId(), assignedPortalPosition.GetPositionX(),
                               assignedPortalPosition.GetPositionY(),
                 assignedPortalPosition.GetPositionZ(), sPlayerbotAIConfig.contactDistance,
                 MovementPriority::MOVEMENT_FORCED);
    }
}

bool YoggSaronFallFromFloorAction::Execute(Event /*event*/)
{
    std::string rtiMark = AI_VALUE(std::string, "rti");
    if (rtiMark == "skull")
    {
        return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_BOSS_ROOM_RESTORE_POINT.GetPositionX(),
                               ULDUAR_YOGG_SARON_BOSS_ROOM_RESTORE_POINT.GetPositionY(),
                               ULDUAR_YOGG_SARON_BOSS_ROOM_RESTORE_POINT.GetPositionZ(),
                               ULDUAR_YOGG_SARON_BOSS_ROOM_RESTORE_POINT.GetOrientation());
    }
    if (rtiMark == "cross")
    {
        return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE.GetPositionX(),
                               ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE.GetPositionY(),
                               ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE.GetPositionZ(),
                               bot->GetOrientation());
    }
    if (rtiMark == "circle")
    {
        return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE.GetPositionX(),
                               ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE.GetPositionY(),
                               ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE.GetPositionZ(), bot->GetOrientation());
    }
    if (rtiMark == "star")
    {
        return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE.GetPositionX(),
                               ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE.GetPositionY(),
                               ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE.GetPositionZ(), bot->GetOrientation());
    }
    return false;
}

bool YoggSaronBossRoomMovementCheatAction::Execute(Event /*event*/)
{
    FollowMasterStrategy followMasterStrategy(botAI);
    if (botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
        botAI->ChangeStrategy(REMOVE_STRATEGY_CHAR + followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT);

    if (!botAI->HasCheat(BotCheatMask::raid))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ObjectGuid currentSkullTarget = group->GetTargetIcon(RtiTargetValue::skullIndex);

    if (!currentSkullTarget)
        return false;

    Unit* currentSkullUnit = botAI->GetUnit(currentSkullTarget);

    if (!currentSkullUnit || !currentSkullUnit->IsAlive())
        return false;

    return bot->TeleportTo(bot->GetMapId(), currentSkullUnit->GetPositionX(), currentSkullUnit->GetPositionY(),
                           currentSkullUnit->GetPositionZ(), bot->GetOrientation());
}

bool YoggSaronUsePortalAction::Execute(Event /*event*/)
{
    Creature* assignedPortal = bot->FindNearestCreature(NPC_DESCEND_INTO_MADNESS, 2.0f, true);
    if (!assignedPortal)
        return false;

    FollowMasterStrategy followMasterStrategy(botAI);
    if (botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
        botAI->ChangeStrategy(ADD_STRATEGY_CHAR + followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT);

    return assignedPortal->HandleSpellClick(bot);
}

bool YoggSaronIllusionRoomAction::Execute(Event /*event*/)
{
    YoggSaronTrigger yoggSaronTrigger(botAI);

    bool resultSetRtiMark = SetRtiMark(yoggSaronTrigger);
    bool resultSetIllusionRtiTarget = SetIllusionRtiTarget(yoggSaronTrigger);
    bool resultSetBrainRtiTarget = SetBrainRtiTarget(yoggSaronTrigger);

    return resultSetRtiMark || resultSetIllusionRtiTarget || resultSetBrainRtiTarget;
}

bool YoggSaronIllusionRoomAction::SetRtiMark(YoggSaronTrigger yoggSaronTrigger)
{
    if (AI_VALUE(std::string, "rti") == "diamond")
    {
        if (yoggSaronTrigger.IsInStormwindKeeperIllusion())
        {
            botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("cross");
            return true;
        }
        else if (yoggSaronTrigger.IsInIcecrownKeeperIllusion())
        {
            botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("circle");
            return true;
        }
        else if (yoggSaronTrigger.IsInChamberOfTheAspectsIllusion())
        {
            botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("star");
            return true;
        }
    }
    return false;
}

bool YoggSaronIllusionRoomAction::SetIllusionRtiTarget(YoggSaronTrigger yoggSaronTrigger)
{
    Unit* currentRtiTarget = yoggSaronTrigger.GetIllusionRoomRtiTarget();
    if (currentRtiTarget)
        return false;

    Unit* nextRtiTarget = yoggSaronTrigger.GetNextIllusionRoomRtiTarget();
    if (!nextRtiTarget)
        return false;

    // If proper adds handling in illusion room will be implemented, then this can be removed
    if (botAI->HasCheat(BotCheatMask::raid))
    {
        bot->TeleportTo(bot->GetMapId(), nextRtiTarget->GetPositionX(), nextRtiTarget->GetPositionY(),
                        nextRtiTarget->GetPositionZ(), bot->GetOrientation());

        Unit::DealDamage(bot->GetSession()->GetPlayer(), nextRtiTarget, nextRtiTarget->GetHealth(), nullptr,
                         DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false, true);
    }
    else
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        uint8 rtiIndex = RtiTargetValue::GetRtiIndex(AI_VALUE(std::string, "rti"));
        group->SetTargetIcon(rtiIndex, bot->GetGUID(), nextRtiTarget->GetGUID());
    }

    return true;
}

bool YoggSaronIllusionRoomAction::SetBrainRtiTarget(YoggSaronTrigger yoggSaronTrigger)
{
    if (AI_VALUE(std::string, "rti") == "square" || !yoggSaronTrigger.IsMasterIsInBrainRoom())
        return false;

    botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("square");

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Creature* brain = bot->FindNearestCreature(NPC_BRAIN, 200.0f, true);
    if (!brain)
        return false;

    group->SetTargetIcon(RtiTargetValue::squareIndex, bot->GetGUID(), brain->GetGUID());

    Position entrancePosition = yoggSaronTrigger.GetIllusionRoomEntrancePosition();

    if (botAI->HasCheat(BotCheatMask::raid))
    {
        if (Unit const* master = botAI->GetMaster())
        {
            Position masterPosition = master->GetPosition();
            bot->TeleportTo(bot->GetMapId(), masterPosition.GetPositionX(), masterPosition.GetPositionY(),
                            masterPosition.GetPositionZ(), bot->GetOrientation());
        }
        else
        {
            bot->TeleportTo(bot->GetMapId(), entrancePosition.GetPositionX(), entrancePosition.GetPositionY(),
                            entrancePosition.GetPositionZ(), bot->GetOrientation());
        }
    }
    else
    {
        MoveTo(bot->GetMapId(), entrancePosition.GetPositionX(), entrancePosition.GetPositionY(),
            entrancePosition.GetPositionZ(), false, false, false, true, MovementPriority::MOVEMENT_FORCED, true,
            false);
    }

    botAI->DoSpecificAction("attack rti target");
    return true;
}

bool YoggSaronMoveToExitPortalAction::Execute(Event /*event*/)
{
    GameObject* portal = bot->FindNearestGameObject(GO_FLEE_TO_THE_SURFACE_PORTAL, 100.0f);
    if (!portal)
        return false;

    if (botAI->HasCheat(BotCheatMask::raid))
        bot->TeleportTo(bot->GetMapId(), portal->GetPositionX(), portal->GetPositionY(), portal->GetPositionZ(),
                               bot->GetOrientation());
    else
        MoveTo(bot->GetMapId(), portal->GetPositionX(), portal->GetPositionY(), portal->GetPositionZ(), false,
                      false, false, true, MovementPriority::MOVEMENT_FORCED,
                      true, false);

    if (bot->GetDistance2d(portal) > 2.0f)
        return false;

    portal->Use(bot);

    botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("skull");
    return true;
}

bool YoggSaronLunaticGazeAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "yogg-saron");
    if (!boss || !boss->IsAlive())
        return false;

    float angle = bot->GetAngle(boss);
    float newAngle = Position::NormalizeOrientation(angle + M_PI);  // Add 180 degrees (PI radians)
    bot->SetFacingTo(newAngle);

    if (botAI->IsRangedDps(bot))
    {
        if (AI_VALUE(std::string, "rti") != "cross")
            botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("cross");
    }
    return true;
}

bool YoggSaronPhase3PositioningAction::Execute(Event /*event*/)
{
    if (botAI->IsRanged(bot))
    {
        if (botAI->HasCheat(BotCheatMask::raid))
        {
            return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionX(),
                            ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionY(),
                            ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionZ(),
                            bot->GetOrientation());
        }
        else
        {
            return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionX(),
                   ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionY(),
                   ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT.GetPositionZ(), false,
                   false, false, true, MovementPriority::MOVEMENT_FORCED, true, false);
        }
    }

    if (botAI->IsMelee(bot) && !botAI->IsTank(bot))
    {
        if (botAI->HasCheat(BotCheatMask::raid))
        {
            return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
                            ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY(),
                            ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionZ(), bot->GetOrientation());
        }
        else
        {
            return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
                   ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY(),
                   ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionZ(), false, false, false, true,
                   MovementPriority::MOVEMENT_FORCED, true, false);
        }
    }

    if (botAI->IsTank(bot))
    {
        if (bot->GetDistance(ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT) > 30.0f)
        {
            if (botAI->HasCheat(BotCheatMask::raid))
            {
                return bot->TeleportTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
                                       ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY(),
                                       ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionZ(), bot->GetOrientation());
            }
        }

        return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
                      ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY(),
                      ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionZ(), false, false, false, true,
                      MovementPriority::MOVEMENT_FORCED, true, false);
    }

    return false;
}

bool YoggSaronCrusherTentacleAction::Execute(Event /*event*/)
{
    Unit* crusher = GetFirstAliveUnitByEntry(botAI, NPC_CRUSHER_TENTACLE);
    if (!crusher)
        return false;

    return Attack(crusher);
}

bool YoggSaronGuardianControlAction::Execute(Event /*event*/)
{
    // Tank only: melee/ranged already focus the skull guardian and the phase-3 positioning stacks them.
    if (!botAI->IsTank(bot))
        return false;

    // Hold the melee stack so taunted guardians pile onto the melee bots to be cleaved down.
    if (bot->GetDistance(ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT) > 5.0f)
    {
        return MoveTo(bot->GetMapId(), ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionX(),
                      ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionY(),
                      ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT.GetPositionZ(), false, false, false, true,
                      MovementPriority::MOVEMENT_FORCED, true, false);
    }

    // Taunt the nearest loose guardian (not already coming to a tank) so it comes to the stack.
    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    Unit* looseGuardian = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const ObjectGuid& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_IMMORTAL_GUARDIAN && unit->GetEntry() != NPC_MARKED_IMMORTAL_GUARDIAN)
            continue;

        Player* targetedPlayer = botAI->GetPlayer(unit->GetTarget());
        if (targetedPlayer && botAI->IsTank(targetedPlayer))
            continue;

        float distance = bot->GetDistance(unit);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            looseGuardian = unit;
        }
    }

    if (!looseGuardian)
        return false;

    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            return botAI->CastSpell("taunt", looseGuardian);
        case CLASS_PALADIN:
            return botAI->CastSpell("hand of reckoning", looseGuardian);
        case CLASS_DEATH_KNIGHT:
            return botAI->CastSpell("dark command", looseGuardian);
        case CLASS_DRUID:
            return botAI->CastSpell("growl", looseGuardian);
        default:
            return false;
    }
}

bool YoggSaronSanityConservationAction::Execute(Event /*event*/)
{
    Unit* yogg = AI_VALUE2(Unit*, "find target", "yogg-saron");
    if (!yogg || !yogg->IsAlive())
        return false;

    // Pull to the back of Yogg-Saron - the spot behind him, opposite his facing.
    float const behindDistance = 15.0f;
    float behindAngle = Position::NormalizeOrientation(yogg->GetOrientation() + M_PI);
    float behindX = yogg->GetPositionX() + behindDistance * cos(behindAngle);
    float behindY = yogg->GetPositionY() + behindDistance * sin(behindAngle);
    float behindZ = yogg->GetPositionZ();

    if (bot->GetDistance2d(behindX, behindY) > 5.0f)
    {
        return MoveTo(bot->GetMapId(), behindX, behindY, behindZ, false, false, false, true,
                      MovementPriority::MOVEMENT_FORCED, true, false);
    }

    // Face directly away from Yogg: Lunatic Gaze only hits units with him in their front arc.
    float awayAngle = Position::NormalizeOrientation(bot->GetAngle(yogg) + M_PI);
    bot->SetFacingTo(awayAngle);

    // Only heal/DPS a target already in the front hemisphere (away from Yogg), so the bot never
    // turns back toward Yogg and eats a gaze. Otherwise hold, facing away.
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsAlive())
    {
        float diff = Position::NormalizeOrientation(bot->GetAngle(target) - awayAngle);
        if (diff > M_PI)
            diff = 2 * M_PI - diff;
        if (diff <= M_PI / 2)
            return false;
    }

    return true;
}

bool YoggSaronSqueezeEscapeAction::Execute(Event /*event*/)
{
    switch (bot->getClass())
    {
        case CLASS_MAGE:
            return botAI->CanCastSpell("ice block", bot) && botAI->CastSpell("ice block", bot);

        case CLASS_PALADIN:
            return botAI->CanCastSpell("divine shield", bot) && botAI->CastSpell("divine shield", bot);

        default:
            return false;
    }
}

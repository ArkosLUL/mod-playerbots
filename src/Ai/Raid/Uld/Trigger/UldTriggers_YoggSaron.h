#ifndef PLAYERBOTS_ULDTRIGGERS_YOGGSARON_H
#define PLAYERBOTS_ULDTRIGGERS_YOGGSARON_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Yogg-Saron
//
class YoggSaronTrigger : public Trigger
{
public:
    YoggSaronTrigger(PlayerbotAI* ai, std::string const name = "yogg saron trigger", int32 checkInteval = 1)
        : Trigger(ai, name, checkInteval) {}

    bool IsYoggSaronFight();
    bool IsPhase2();
    bool IsPhase3();
    bool IsInBrainLevel();
    bool IsInIllusionRoom();
    bool IsInStormwindKeeperIllusion();
    bool IsInIcecrownKeeperIllusion();
    bool IsInChamberOfTheAspectsIllusion();
    bool IsMasterIsInIllusionGroup();
    bool IsMasterIsInBrainRoom();
    Position GetIllusionRoomEntrancePosition();
    Unit* GetIllusionRoomRtiTarget();
    Unit* GetNextIllusionRoomRtiTarget();
    Unit* GetSaraIfAlive();
};

class YoggSaronOminousCloudCheatTrigger : public YoggSaronTrigger
{
public:
    YoggSaronOminousCloudCheatTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron ominous cloud cheat trigger") {}
    bool IsActive() override;
};

class YoggSaronGuardianPositioningTrigger : public YoggSaronTrigger
{
public:
    YoggSaronGuardianPositioningTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron guardian positioning trigger") {}
    bool IsActive() override;
};

class YoggSaronSanityTrigger : public YoggSaronTrigger
{
public:
    YoggSaronSanityTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron sanity trigger") {}
    bool IsActive() override;
};

class YoggSaronDeathOrbTrigger : public YoggSaronTrigger
{
public:
    YoggSaronDeathOrbTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron death orb trigger") {}
    bool IsActive() override;
};

class YoggSaronMaladyOfTheMindTrigger : public YoggSaronTrigger
{
public:
    YoggSaronMaladyOfTheMindTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron malady of the mind trigger") {}
    bool IsActive() override;
};

class YoggSaronMarkTargetTrigger : public YoggSaronTrigger
{
public:
    YoggSaronMarkTargetTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron mark target trigger") {}
    bool IsActive() override;
};

class YoggSaronBrainLinkTrigger : public YoggSaronTrigger
{
public:
    YoggSaronBrainLinkTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron brain link trigger") {}
    bool IsActive() override;
};

class YoggSaronMoveToEnterPortalTrigger : public YoggSaronTrigger
{
public:
    YoggSaronMoveToEnterPortalTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron move to enter portal trigger") {}
    bool IsActive() override;
};

class YoggSaronFallFromFloorTrigger : public YoggSaronTrigger
{
public:
    YoggSaronFallFromFloorTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron fall from floor trigger") {}
    bool IsActive() override;
};

class YoggSaronBossRoomMovementCheatTrigger : public YoggSaronTrigger
{
public:
    YoggSaronBossRoomMovementCheatTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron boss room movement cheat trigger") {}
    bool IsActive() override;
};

class YoggSaronUsePortalTrigger : public YoggSaronTrigger
{
public:
    YoggSaronUsePortalTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron use portal trigger") {}
    bool IsActive() override;
};

class YoggSaronIllusionRoomTrigger : public YoggSaronTrigger
{
public:
    YoggSaronIllusionRoomTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron illusion room trigger") {}
    bool IsActive() override;

private:
    bool GoToBrainRoomRequired();
    bool SetRtiMarkRequired();
    bool SetRtiTargetRequired();
};

class YoggSaronMoveToExitPortalTrigger : public YoggSaronTrigger
{
public:
    YoggSaronMoveToExitPortalTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron move to exit portal trigger") {}
    bool IsActive() override;
};

class YoggSaronLunaticGazeTrigger : public YoggSaronTrigger
{
public:
    YoggSaronLunaticGazeTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron lunatic gaze trigger") {}
    bool IsActive() override;
};

class YoggSaronPhase3PositioningTrigger : public YoggSaronTrigger
{
public:
    YoggSaronPhase3PositioningTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron phase 3 positioning trigger") {}
    bool IsActive() override;
};

#endif

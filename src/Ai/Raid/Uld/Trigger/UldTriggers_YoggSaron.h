#ifndef PLAYERBOTS_ULDTRIGGERS_YOGGSARON_H
#define PLAYERBOTS_ULDTRIGGERS_YOGGSARON_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "RaidAntiFear.h"
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

// Reduced-Keeper hard mode: ranged DPS peel onto a live Crusher Tentacle (P2) to clear its Diminish
// Power fast, so melee are not dragged out to the stationary tentacle.
class YoggSaronCrusherTentacleTrigger : public YoggSaronTrigger
{
public:
    YoggSaronCrusherTentacleTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron crusher tentacle trigger") {}
    bool IsActive() override;
};

// Reduced-Keeper hard mode with Thorim: the tank taunts loose Immortal Guardians (P3) to the melee
// stack so they get cleaved to Weakened and Thorim's Titanic Storm executes them.
class YoggSaronGuardianControlTrigger : public YoggSaronTrigger
{
public:
    YoggSaronGuardianControlTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron guardian control trigger") {}
    bool IsActive() override;
};

// Reduced-Keeper hard mode: with no Sanity Wells to run to, a bot near the Insane cliff retreats to a
// safe ranged spot and faces away from Yogg to stop every avoidable sanity drain.
class YoggSaronSanityConservationTrigger : public YoggSaronTrigger
{
public:
    YoggSaronSanityConservationTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron sanity conservation trigger") {}
    bool IsActive() override;
};

// A Constrictor Tentacle drops its passenger when Squeeze is removed, so a grabbed paladin or mage
// can free itself with an immunity.
class YoggSaronSqueezeEscapeTrigger : public YoggSaronTrigger
{
public:
    YoggSaronSqueezeEscapeTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron squeeze escape trigger") {}
    bool IsActive() override;
};

// Covers both fears the fight has: Malady of the Mind in P2 and Deafening Roar in P3. Complements
// the malady spread node, which handles the bot who is already feared.
class YoggSaronAntiFearTrigger : public RaidAntiFearTrigger
{
public:
    YoggSaronAntiFearTrigger(PlayerbotAI* ai) : RaidAntiFearTrigger(ai, "yogg-saron anti fear trigger") {}

protected:
    bool FearWindowActive() override { return YoggSaronFearWindowActive(botAI); }
};

#endif

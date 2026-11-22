#ifndef PLAYERBOTS_ULDTRIGGERS_YOGGSARON_H
#define PLAYERBOTS_ULDTRIGGERS_YOGGSARON_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "RaidAntiFear.h"
#include "UldEncounter_YoggSaron.h"
#include "Trigger.h"

class YoggSaronTrigger : public Trigger
{
public:
    YoggSaronTrigger(PlayerbotAI* botAI, std::string const name = "yogg saron trigger", int32 checkInteval = 1)
        : Trigger(botAI, name, checkInteval) {}

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
    bool IsBrainRoomApproachable();
    bool IsDesignatedBotTank();

    bool PhaseThreeStationReaches(Unit* target);
};

class YoggSaronPhase1SpacingTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron phase 1 spacing trigger";

    YoggSaronPhase1SpacingTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronPhase1StationTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron phase 1 station trigger";

    YoggSaronPhase1StationTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;

private:
    bool parked = false;
};

class YoggSaronDarkVolleyTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron dark volley trigger";

    YoggSaronDarkVolleyTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronDiminishPowerJudgementTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron diminish power judgement trigger";

    YoggSaronDiminishPowerJudgementTrigger(PlayerbotAI* botAI)
        : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronMortalStrikeTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron mortal strike trigger";

    YoggSaronMortalStrikeTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronWarlockPetTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron warlock pet trigger";

    YoggSaronWarlockPetTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronGuardianPositioningTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron guardian positioning trigger";

    YoggSaronGuardianPositioningTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;

private:
    bool returning = false;
};

class YoggSaronSanityTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron sanity trigger";

    YoggSaronSanityTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronMaladyOfTheMindTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron malady of the mind trigger";

    YoggSaronMaladyOfTheMindTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronPhase3ControlTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron phase 3 control trigger";

    YoggSaronPhase3ControlTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronSetDpsPriorityTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron set dps priority trigger";

    YoggSaronSetDpsPriorityTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronPhase2SpacingTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron phase 2 spacing trigger";

    YoggSaronPhase2SpacingTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronBrainLinkTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron brain link trigger";

    YoggSaronBrainLinkTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronMoveToEnterPortalTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron move to enter portal trigger";

    YoggSaronMoveToEnterPortalTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronFallFromFloorTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron fall from floor trigger";

    YoggSaronFallFromFloorTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronStopFollowingTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron stop following trigger";

    YoggSaronStopFollowingTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronUsePortalTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron use portal trigger";

    YoggSaronUsePortalTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronIllusionRoomTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron illusion room trigger";

    YoggSaronIllusionRoomTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;

private:
    bool GoToBrainRoomRequired();
    bool SetRtiMarkRequired();

    bool WalkIntoRoomRequired();

    bool StageRequired();
};

class YoggSaronIllusionHealerStationTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron illusion healer station trigger";

    YoggSaronIllusionHealerStationTrigger(PlayerbotAI* botAI)
        : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronReprisalRegroupTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron reprisal regroup trigger";

    YoggSaronReprisalRegroupTrigger(PlayerbotAI* botAI)
        : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronBrainSpotTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron brain spot trigger";

    YoggSaronBrainSpotTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronNitroBoostsTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron nitro boosts trigger";

    YoggSaronNitroBoostsTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronBrainDashTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron brain dash trigger";

    YoggSaronBrainDashTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronBrainChargeTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron brain charge trigger";

    YoggSaronBrainChargeTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronMoveToExitPortalTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron move to exit portal trigger";

    YoggSaronMoveToExitPortalTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronLaughingSkullTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron laughing skull trigger";

    YoggSaronLaughingSkullTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronIllusionFacingTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron illusion facing trigger";

    YoggSaronIllusionFacingTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronPetGuardTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron pet guard trigger";

    YoggSaronPetGuardTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronBodyDetourTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron body detour trigger";

    YoggSaronBodyDetourTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronLunaticGazeTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron lunatic gaze trigger";

    YoggSaronLunaticGazeTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronBeaconDragTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron beacon drag trigger";

    YoggSaronBeaconDragTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronBeaconStageTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron beacon stage trigger";

    YoggSaronBeaconStageTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronBeaconBearFormTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron beacon bear form trigger";

    YoggSaronBeaconBearFormTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronPhase3LustTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron phase 3 lust trigger";

    YoggSaronPhase3LustTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronPhase3PositioningTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron phase 3 positioning trigger";

    YoggSaronPhase3PositioningTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronGuardianControlTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron guardian control trigger";

    YoggSaronGuardianControlTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronSanityConservationTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron sanity conservation trigger";

    YoggSaronSanityConservationTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronSqueezeEscapeTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron squeeze escape trigger";

    YoggSaronSqueezeEscapeTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronSqueezeRescueTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron squeeze rescue trigger";

    YoggSaronSqueezeRescueTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronNovaDefensiveTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron nova defensive trigger";

    YoggSaronNovaDefensiveTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronNovaExternalTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron nova external trigger";

    YoggSaronNovaExternalTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronNovaRaidCooldownTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron nova raid cooldown trigger";

    YoggSaronNovaRaidCooldownTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronRoarAuraTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron roar aura trigger";

    YoggSaronRoarAuraTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronRoarAuraMasteryTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron roar aura mastery trigger";

    YoggSaronRoarAuraMasteryTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronBeaconDefensiveTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron beacon defensive trigger";

    YoggSaronBeaconDefensiveTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronBeaconExternalTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron beacon external trigger";

    YoggSaronBeaconExternalTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronTankLayOnHandsTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron tank lay on hands trigger";

    YoggSaronTankLayOnHandsTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronHealerCombatEngineTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron healer combat engine trigger";

    YoggSaronHealerCombatEngineTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronShadowAuraTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron shadow aura trigger";

    YoggSaronShadowAuraTrigger(PlayerbotAI* botAI) : YoggSaronTrigger(botAI, Name) {}
    bool IsActive() override;
};

class YoggSaronAntiFearTrigger : public RaidAntiFearTrigger
{
public:
    static constexpr char const* Name = "yogg-saron anti fear trigger";

    YoggSaronAntiFearTrigger(PlayerbotAI* botAI) : RaidAntiFearTrigger(botAI, Name) {}

protected:
    bool FearWindowActive() override { return YoggSaronFearWindowActive(botAI); }
};

#endif

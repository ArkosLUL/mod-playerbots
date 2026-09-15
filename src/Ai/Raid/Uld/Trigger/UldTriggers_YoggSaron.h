#ifndef PLAYERBOTS_ULDTRIGGERS_YOGGSARON_H
#define PLAYERBOTS_ULDTRIGGERS_YOGGSARON_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "RaidAntiFear.h"
#include "UldEncounter_YoggSaron.h"
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
    bool IsBrainRoomApproachable();
    bool IsDesignatedBotTank();

    // Whether the bot's phase 3 station still reaches a target from inside its radius. A fixed spot
    // with a radius is a fence whenever the target sits further out than radius plus range, and the
    // ranged spot is tuned so Yogg is reachable by 4.4 yd and nothing else in the room is.
    bool PhaseThreeStationReaches(Unit* target);
};

// Clouds and Guardians in one node. Two nodes at the same relevance cannot share a bot - the engine
// ends the tick at the first action returning true - so they would trade ticks and walk the bot down
// the line between their two destinations.
class YoggSaronPhase1SpacingTrigger : public YoggSaronTrigger
{
public:
    YoggSaronPhase1SpacingTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron phase 1 spacing trigger") {}
    bool IsActive() override;
};

// Ranged and healers hold one spot on the second orbit instead of dodging clouds, because no radius
// clears every orbit and the cloud outruns any sidestep. Standing there costs the whole back line one
// Guardian per 45 s orbit, against the 2.7 a minute a phase of dodging cost.
class YoggSaronPhase1StationTrigger : public YoggSaronTrigger
{
public:
    YoggSaronPhase1StationTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron phase 1 station trigger") {}
    bool IsActive() override;

private:
    // Per bot, like the guardian positioning trigger's leash: trigger instances belong to one bot's
    // context.
    bool parked = false;
};

// Dark Volley is a 1.5s cast hitting everything within 35 yd, so it cannot be walked out of. The
// always-on class interrupts only ever look at the bot's current target and caught under a third of
// them.
class YoggSaronDarkVolleyTrigger : public YoggSaronTrigger
{
public:
    YoggSaronDarkVolleyTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron dark volley trigger") {}
    bool IsActive() override;
};

// Melee and tanks are leashed to Sara rather than stationed on her: a Guardian walks to whoever holds
// threat, so where melee stand is where it dies, and only a death inside 15 yd of Sara advances the
// phase at all.
class YoggSaronGuardianPositioningTrigger : public YoggSaronTrigger
{
public:
    YoggSaronGuardianPositioningTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron guardian positioning trigger") {}
    bool IsActive() override;

private:
    // Per bot, like the spacing action's held spot: trigger instances belong to one bot's context.
    bool returning = false;
};

class YoggSaronSanityTrigger : public YoggSaronTrigger
{
public:
    YoggSaronSanityTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron sanity trigger") {}
    bool IsActive() override;
};

class YoggSaronMaladyOfTheMindTrigger : public YoggSaronTrigger
{
public:
    YoggSaronMaladyOfTheMindTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron malady of the mind trigger") {}
    bool IsActive() override;
};

// The strategy swap onto TankAssist the single designated bot tank does for the raid at the start of
// phase 3.
class YoggSaronPhase3ControlTrigger : public YoggSaronTrigger
{
public:
    YoggSaronPhase3ControlTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron phase 3 control trigger") {}
    bool IsActive() override;
};

// One owner of every non-tank's target for the whole encounter, replacing the raid icons this fight
// used to target through. An icon is a sticky override - RtiTargetValue hands it back before the smart
// picker runs and IsHighPriority pins it - so a wrong mark could not be corrected until combat ended.
class YoggSaronSetDpsPriorityTrigger : public YoggSaronTrigger
{
public:
    YoggSaronSetDpsPriorityTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron set dps priority trigger") {}
    bool IsActive() override;
};

// Crush wedges and Death Rays in one node, for the reason recorded on the phase 1 pair: two nodes at
// one relevance cannot share a bot, so they would trade ticks and walk it down the line between their
// destinations.
class YoggSaronPhase2SpacingTrigger : public YoggSaronTrigger
{
public:
    YoggSaronPhase2SpacingTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron phase 2 spacing trigger") {}
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

class YoggSaronStopFollowingTrigger : public YoggSaronTrigger
{
public:
    YoggSaronStopFollowingTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron stop following trigger") {}
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

    // Nothing else in the fight moves a bot once a portal has dropped it, and the dps resolver will
    // not pick a target it has no line of sight to. Two of the three rooms put their tentacles behind
    // a doorway, so the raid stood at the landing spot until Induce Madness landed.
    bool WalkIntoRoomRequired();
};

class YoggSaronMoveToExitPortalTrigger : public YoggSaronTrigger
{
public:
    YoggSaronMoveToExitPortalTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron move to exit portal trigger") {}
    bool IsActive() override;
};

// The Laughing Skull cannot be targeted or killed - UNIT_FLAG_NOT_SELECTABLE, flags_extra 128 - and
// its gaze picks targets by HasInArc(M_PI, caster), so turning around is the only defence. Four per
// room, 1750 shadow damage and -2 Sanity a second each at 30 yd.
class YoggSaronLaughingSkullTrigger : public YoggSaronTrigger
{
public:
    YoggSaronLaughingSkullTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron laughing skull trigger") {}
    bool IsActive() override;
};

// The facing node corrects a heading; this one moves the bot so the heading it will be forced into is
// already safe. Only while there is something to kill - clear of a fight, turning away is free and the
// facing node handles it.
class YoggSaronIllusionFacingTrigger : public YoggSaronTrigger
{
public:
    YoggSaronIllusionFacingTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron illusion facing trigger") {}
    bool IsActive() override;
};

class YoggSaronPetGuardTrigger : public YoggSaronTrigger
{
public:
    YoggSaronPetGuardTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron pet guard trigger") {}
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

// The tank taunts loose Immortal Guardians (P3) to the melee stack so they get cleaved to Weakened and
// Thorim's Titanic Storm executes them.
class YoggSaronGuardianControlTrigger : public YoggSaronTrigger
{
public:
    YoggSaronGuardianControlTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron guardian control trigger") {}
    bool IsActive() override;
};

// With no Sanity Wells to run to, a bot near the Insane cliff retreats to a safe ranged spot and faces
// away from Yogg to stop every avoidable sanity drain.
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

// A paladin frees somebody else's Constrictor Tentacle with Hand of Protection: it grants physical
// school immunity, which purges Squeeze, and the Squeeze aura script kills the tentacle on removal.
class YoggSaronSqueezeRescueTrigger : public YoggSaronTrigger
{
public:
    YoggSaronSqueezeRescueTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, "yogg-saron squeeze rescue trigger") {}
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

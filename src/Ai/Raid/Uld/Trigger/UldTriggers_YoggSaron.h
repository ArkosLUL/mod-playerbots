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
    static constexpr char const* Name = "yogg-saron phase 1 spacing trigger";

    YoggSaronPhase1SpacingTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// Ranged and healers hold one spot on the second orbit instead of dodging clouds, because no radius
// clears every orbit and the cloud outruns any sidestep. Standing there costs the whole back line one
// Guardian per 45 s orbit, against the 2.7 a minute a phase of dodging cost.
class YoggSaronPhase1StationTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron phase 1 station trigger";

    YoggSaronPhase1StationTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;

private:
    // Per bot, like the guardian positioning trigger's leash: trigger instances belong to one bot's
    // context.
    bool parked = false;
};

// Dark Volley is a 1.5s cast hitting everything within 35 yd, so it cannot be walked out of, and Drain
// Life gives an Immortal Guardian back three Empowered stacks. The always-on class interrupts only ever
// look at the bot's current target and caught under a third of the volleys and half the drains.
class YoggSaronDarkVolleyTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron dark volley trigger";

    YoggSaronDarkVolleyTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// A paladin with Judgement ready and a Crusher in reach that is mid Diminish Power channel.
class YoggSaronDiminishPowerJudgementTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron diminish power judgement trigger";

    YoggSaronDiminishPowerJudgementTrigger(PlayerbotAI* ai)
        : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// Melee and tanks are leashed to Sara rather than stationed on her: a Guardian walks to whoever holds
// threat, so where melee stand is where it dies, and only a death inside 15 yd of Sara advances the
// phase at all.
class YoggSaronGuardianPositioningTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron guardian positioning trigger";

    YoggSaronGuardianPositioningTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;

private:
    // Per bot, like the spacing action's held spot: trigger instances belong to one bot's context.
    bool returning = false;
};

class YoggSaronSanityTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron sanity trigger";

    YoggSaronSanityTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

class YoggSaronMaladyOfTheMindTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron malady of the mind trigger";

    YoggSaronMaladyOfTheMindTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// The strategy swap onto TankAssist the single designated bot tank does for the raid at the start of
// phase 3.
class YoggSaronPhase3ControlTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron phase 3 control trigger";

    YoggSaronPhase3ControlTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// One owner of every non-tank's target for the whole encounter, replacing the raid icons this fight
// used to target through. An icon is a sticky override - RtiTargetValue hands it back before the smart
// picker runs and IsHighPriority pins it - so a wrong mark could not be corrected until combat ended.
class YoggSaronSetDpsPriorityTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron set dps priority trigger";

    YoggSaronSetDpsPriorityTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// Crush wedges and Death Rays in one node, for the reason recorded on the phase 1 pair: two nodes at
// one relevance cannot share a bot, so they would trade ticks and walk it down the line between their
// destinations.
class YoggSaronPhase2SpacingTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron phase 2 spacing trigger";

    YoggSaronPhase2SpacingTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

class YoggSaronBrainLinkTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron brain link trigger";

    YoggSaronBrainLinkTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

class YoggSaronMoveToEnterPortalTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron move to enter portal trigger";

    YoggSaronMoveToEnterPortalTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

class YoggSaronFallFromFloorTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron fall from floor trigger";

    YoggSaronFallFromFloorTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

class YoggSaronStopFollowingTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron stop following trigger";

    YoggSaronStopFollowingTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

class YoggSaronUsePortalTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron use portal trigger";

    YoggSaronUsePortalTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

class YoggSaronIllusionRoomTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron illusion room trigger";

    YoggSaronIllusionRoomTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;

private:
    bool GoToBrainRoomRequired();
    bool SetRtiMarkRequired();

    // Nothing else in the fight moves a bot once a portal has dropped it, and the dps resolver will
    // not pick a target it has no line of sight to. Two of the three rooms put their tentacles behind
    // a doorway, so the raid stood at the landing spot until Induce Madness landed.
    bool WalkIntoRoomRequired();
};

// The brain team's healer, back to its illusion room's middle while tentacles live. Chasing its own
// target put it 39 yd off the middle with 57% of its mates out of heal range.
class YoggSaronIllusionHealerStationTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron illusion healer station trigger";

    YoggSaronIllusionHealerStationTrigger(PlayerbotAI* ai)
        : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// A damage dealer the reprisal hold took off the tentacles, out of its room healer's reach. Nothing heals
// it there, so it would sit out the rest of the room.
class YoggSaronReprisalRegroupTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron reprisal regroup trigger";

    YoggSaronReprisalRegroupTrigger(PlayerbotAI* ai)
        : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// Once the room's door is open, onto the spot the bot hits the Brain from. A single walk dies on the
// healer's first heal, and melee arriving in front of the Brain get walked out of range by set behind.
class YoggSaronBrainSpotTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron brain spot trigger";

    YoggSaronBrainSpotTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// Nitro Boosts on the walk from a cleared illusion room to the Brain, the longest walk of the wave and
// all of it time off the Brain. Never on the way out.
class YoggSaronNitroBoostsTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron nitro boosts trigger";

    YoggSaronNitroBoostsTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

class YoggSaronMoveToExitPortalTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron move to exit portal trigger";

    YoggSaronMoveToExitPortalTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// The Laughing Skull cannot be targeted or killed - UNIT_FLAG_NOT_SELECTABLE, flags_extra 128 - and
// its gaze picks targets by HasInArc(M_PI, caster), so turning around is the only defence. Four per
// room, 1750 shadow damage and -2 Sanity a second each at 30 yd.
class YoggSaronLaughingSkullTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron laughing skull trigger";

    YoggSaronLaughingSkullTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// The facing node corrects a heading; this one moves the bot so the heading it will be forced into is
// already safe. Only while there is something to kill - clear of a fight, turning away is free and the
// facing node handles it.
class YoggSaronIllusionFacingTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron illusion facing trigger";

    YoggSaronIllusionFacingTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

class YoggSaronPetGuardTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron pet guard trigger";

    YoggSaronPetGuardTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// The walk to a target on the far side of Yogg. Everything else about the body ring asks where the bot
// is standing; this asks where the walk is going, which is the only read that can stop a bot entering
// it in the first place.
class YoggSaronBodyDetourTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron body detour trigger";

    YoggSaronBodyDetourTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

class YoggSaronLunaticGazeTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron lunatic gaze trigger";

    YoggSaronLunaticGazeTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// Phase 3 without Thorim: a Shadow Beacon marked Guardian heals every Guardian and Yogg within 20 yd of it
// once its mark ends, and the pile tank can't separate it, the whole pile follows whoever holds it. So a
// second bot taunts it off and walks it clear. See YoggSaronBeaconDragGuardian for who.
class YoggSaronBeaconDragTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron beacon drag trigger";

    YoggSaronBeaconDragTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// A shaman's own Heroism trigger wants a boss target, and in phase 3 shamans are on Guardians and
// tentacles: one pull never cast it, another 40 s in. This one waits for the raid to be in place instead.
class YoggSaronPhase3LustTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron phase 3 lust trigger";

    YoggSaronPhase3LustTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

class YoggSaronPhase3PositioningTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron phase 3 positioning trigger";

    YoggSaronPhase3PositioningTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// The tank taunts loose Immortal Guardians (P3) to the melee stack so they get cleaved to Weakened, where
// Thorim's Titanic Storm executes them or, without him, they hit for no bonus.
class YoggSaronGuardianControlTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron guardian control trigger";

    YoggSaronGuardianControlTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// With no Sanity Wells to run to, a bot near the Insane cliff retreats to a safe ranged spot and faces
// away from Yogg to stop every avoidable sanity drain.
class YoggSaronSanityConservationTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron sanity conservation trigger";

    YoggSaronSanityConservationTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// A Constrictor Tentacle drops its passenger when Squeeze is removed, so a grabbed paladin or mage
// can free itself with an immunity.
class YoggSaronSqueezeEscapeTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron squeeze escape trigger";

    YoggSaronSqueezeEscapeTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// A paladin frees somebody else's Constrictor Tentacle with Hand of Protection: it grants physical
// school immunity, which purges Squeeze, and the Squeeze aura script kills the tentacle on removal.
class YoggSaronSqueezeRescueTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron squeeze rescue trigger";

    YoggSaronSqueezeRescueTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// Shadow Nova is a death explosion inside the melee pile, so melee cannot dodge it and every one of 25
// phase 1 deaths over four pulls was this spell. These three nodes mitigate it instead, and they fire
// on a Guardian about to die rather than on a bot already hurt.
//
// The bot's own button. Two gates by button length: a 12 s one goes when the Guardian passes 20%, a 5 s
// one waits for 10%, because the measured gap from gate to detonation runs 4.8 s median at 20% and 2.4 s
// at 10%.
class YoggSaronNovaDefensiveTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron nova defensive trigger";

    YoggSaronNovaDefensiveTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// A healer covers whoever the nova still kills after their own button, worst off first. One claim per
// victim, so two healers do not spend two three-minute cooldowns on one bot.
class YoggSaronNovaExternalTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron nova external trigger";

    YoggSaronNovaExternalTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// Divine Sacrifice, held until the nova threatens enough of the stack to be worth one cast for the whole
// raid. One per window.
class YoggSaronNovaRaidCooldownTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron nova raid cooldown trigger";

    YoggSaronNovaRaidCooldownTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// The roar paladin is missing its own Concentration Aura with the roar close.
class YoggSaronRoarAuraTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron roar aura trigger";

    YoggSaronRoarAuraTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// Deafening Roar is being cast and this paladin can answer it: its own Concentration Aura up and Aura
// Mastery ready.
class YoggSaronRoarAuraMasteryTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron roar aura mastery trigger";

    YoggSaronRoarAuraMasteryTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// A beacon-healed Guardian past the danger line is hitting this bot, a dragger or a non-tank, and it has a
// physical cut to press.
class YoggSaronBeaconDefensiveTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron beacon defensive trigger";

    YoggSaronBeaconDefensiveTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// Somebody a healed Guardian is hitting has no button of their own, and this healer or paladin has an
// external in range for them.
class YoggSaronBeaconExternalTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron beacon external trigger";

    YoggSaronBeaconExternalTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// A tank under ULDUAR_YOGG_SARON_TANK_LOH_PCT in phase 3 and this paladin can Lay on Hands it.
class YoggSaronTankLayOnHandsTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron tank lay on hands trigger";

    YoggSaronTankLayOnHandsTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// A healer in combat on the non-combat engine during the encounter.
class YoggSaronHealerCombatEngineTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron healer combat engine trigger";

    YoggSaronHealerCombatEngineTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// Shadow Resistance Aura on the one paladin whose Aura Mastery can double it, and its own copy of the
// aura: a human's sits on every raider and would otherwise read as already done.
class YoggSaronShadowAuraTrigger : public YoggSaronTrigger
{
public:
    static constexpr char const* Name = "yogg-saron shadow aura trigger";

    YoggSaronShadowAuraTrigger(PlayerbotAI* ai) : YoggSaronTrigger(ai, Name) {}
    bool IsActive() override;
};

// Covers Malady of the Mind, the fight's only fear. Complements the malady spread node, which handles the
// bot who is already feared.
class YoggSaronAntiFearTrigger : public RaidAntiFearTrigger
{
public:
    static constexpr char const* Name = "yogg-saron anti fear trigger";

    YoggSaronAntiFearTrigger(PlayerbotAI* ai) : RaidAntiFearTrigger(ai, Name) {}

protected:
    bool FearWindowActive() override { return YoggSaronFearWindowActive(botAI); }
};

#endif

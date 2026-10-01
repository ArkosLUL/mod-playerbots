/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"
#include "RaidTankDefensive.h"

#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Strategy.h"
#include "UldActions_YoggSaron.h"
#include "UldEncounterGate.h"
#include "UldEncounter_YoggSaron.h"
#include "UldMultipliers_YoggSaron.h"
#include "UldTriggers_YoggSaron.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
// Its picker ranks by "not attacking me", then nearest by GetDistance, which takes off Yogg's 30 yd
// combat reach, so Yogg always reads as nearest: it held one bot tank on him for all of phase 3 with no
// Guardian ever targeted. Guardian control hands out a Guardian while one can still be held; 70 yd
// covers the whole spawn ring, 38-48 yd round Yogg, from the melee spot 18.5 yd behind him.
bool YoggSaronGuardianToHold(PlayerbotAI* botAI)
{
    constexpr float guardianSearchRadius = 70.0f;
    return YoggSaronHoldableGuardianWithin(botAI->GetBot(), guardianSearchRadius) && YoggSaronInPhase3(botAI);
}

// The window the dps resolver owns a tank's target in, which has to match
// YoggSaronSetDpsPriorityTrigger or a tank is left with no target source at all. Phase 2 is in it
// because the generic picker walks a tank into a Crusher's reach and Crush is a 100% proc on the
// tentacle's swing: 53 of one pull's 59 Crushes landed on the bot tank standing in one.
bool YoggSaronTankTargetOwned(PlayerbotAI* botAI)
{
    return YoggSaronInPhase2(botAI) || YoggSaronGuardianToHold(botAI);
}

// The Brain's own position is 25 yd above its room's floor, so a leap at it leaves the bot hanging in
// the air with nothing to walk on: one druid charged it, hung there for 23 s and was charmed by
// Induce Madness. GetDistance hides the gap behind the Brain's combat reach, so 50 yd read as in
// range. The Influence Tentacles stand on the floor, so charges stay available for those.
bool YoggSaronBrainIsTarget(PlayerbotAI* botAI)
{
    if (!YoggSaronOnBrainLevel(botAI->GetBot()))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && target->GetEntry() == NPC_BRAIN;
}

// Food and drink sit a bot down with no AI at all for up to 18 s. With no Guardian alive the raid drops
// combat and eats wherever it stands: four melee sat down in the ring 8 s before it lit, and two were
// still eating when it threw them. The back line on the 21.5 yd station keeps eating.
bool YoggSaronInsideRing(PlayerbotAI* botAI)
{
    Position const& middle = ULDUAR_YOGG_SARON_MIDDLE;
    return botAI->GetBot()->GetDistance2d(middle.GetPositionX(), middle.GetPositionY()) <
               ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS &&
           YoggSaronEncounterActive(botAI);
}

void DefineYoggSaron(EncounterBuilder& e)
{
    e.Node(
        "sara shadow resistance trigger",
        [](PlayerbotAI* ai) -> Trigger* { return new BossShadowResistanceTrigger(ai, "sara"); },
        "sara shadow resistance action",
        [](PlayerbotAI* ai) -> Action* { return new BossShadowResistanceAction(ai, "sara"); }, ACTION_RAID);
    e.Node(
        "yogg-saron shadow resistance trigger",
        [](PlayerbotAI* ai) -> Trigger* { return new BossShadowResistanceTrigger(ai, "yogg-saron"); },
        "yogg-saron shadow resistance action",
        [](PlayerbotAI* ai) -> Action* { return new BossShadowResistanceAction(ai, "yogg-saron"); }, ACTION_RAID);
    e.Node<YoggSaronGuardianPositioningTrigger, YoggSaronGuardianPositioningAction>(ACTION_RAID);
    e.Node<YoggSaronPhase1SpacingTrigger, YoggSaronPhase1SpacingAction>(ACTION_RAID + 2);
    // Under the spacing node, so a nova still wins the tick. It never contends with guardian
    // positioning at the same relevance because that one only fires on melee and tanks.
    e.Node<YoggSaronPhase1StationTrigger, YoggSaronPhase1StationAction>(ACTION_RAID);
    e.Node<YoggSaronDarkVolleyTrigger, YoggSaronDarkVolleyInterruptAction>(ACTION_EMERGENCY + 1);
    e.Node<YoggSaronSanityTrigger, YoggSaronSanityAction>(ACTION_RAID + 1);
    e.Node<YoggSaronMaladyOfTheMindTrigger, YoggSaronMaladyOfTheMindAction>(ACTION_RAID);
    e.Node<YoggSaronAntiFearTrigger, YoggSaronAntiFearAction>(ACTION_RAID + 2);
    e.Node<YoggSaronPhase3ControlTrigger, YoggSaronPhase3ControlAction>(ACTION_RAID);
    e.Node<YoggSaronSetDpsPriorityTrigger, YoggSaronSetDpsPriorityAction>(ACTION_RAID + 1);
    e.Node<YoggSaronPhase2SpacingTrigger, YoggSaronPhase2SpacingAction>(ACTION_RAID + 3);
    // Over the dps resolver and the Sanity Well walk, under the hazard dodges and the fear counter: a
    // link costs 2 Sanity and a shared hit a second, a Death Ray costs the bot.
    e.Node<YoggSaronBrainLinkTrigger, YoggSaronBrainLinkAction>(ACTION_RAID + 1.5f);
    e.Node<YoggSaronMoveToEnterPortalTrigger, YoggSaronMoveToEnterPortalAction>(ACTION_RAID);
    e.Node<YoggSaronUsePortalTrigger, YoggSaronUsePortalAction>(ACTION_RAID);
    e.Node<YoggSaronFallFromFloorTrigger, YoggSaronFallFromFloorAction>(ACTION_RAID);
    e.Node<YoggSaronStopFollowingTrigger, YoggSaronStopFollowingAction>(ACTION_RAID);
    e.Node<YoggSaronIllusionRoomTrigger, YoggSaronIllusionRoomAction>(ACTION_RAID);
    e.Node<YoggSaronMoveToExitPortalTrigger, YoggSaronMoveToExitPortalAction>(ACTION_RAID + 4);
    e.Node<YoggSaronLunaticGazeTrigger, YoggSaronLunaticGazeAction>(ACTION_EMERGENCY);
    // Same relevance as the gaze node above and they cannot share a bot, which costs nothing: Yogg's
    // own Lunatic Gaze is a phase 3 self aura on the platform and the skulls only exist below it.
    e.Node<YoggSaronLaughingSkullTrigger, YoggSaronLaughingSkullAction>(ACTION_EMERGENCY);
    e.Node<YoggSaronPhase3PositioningTrigger, YoggSaronPhase3PositioningAction>(ACTION_RAID);
    e.Node<YoggSaronGuardianControlTrigger, YoggSaronGuardianControlAction>(ACTION_RAID + 3);
    e.Node<YoggSaronSanityConservationTrigger, YoggSaronSanityConservationAction>(ACTION_RAID + 5);
    e.Node<YoggSaronSqueezeEscapeTrigger, YoggSaronSqueezeEscapeAction>(ACTION_RAID + 1);
    // Above the sanity conservation retreat: cutting somebody out of 7.5k a second beats walking
    // somebody else to a well.
    e.Node<YoggSaronSqueezeRescueTrigger, YoggSaronSqueezeRescueAction>(ACTION_RAID + 6);

    // Shadow Nova is a death explosion in the middle of the melee pile, so melee cannot step out of it
    // and all 25 phase 1 deaths over four pulls were this one spell. These three mitigate it instead,
    // and they go above everything else because a nova lands in the next second or two and a bot that
    // walks instead of pressing a button is a bot that dies walking.
    //
    // The raid cooldown is highest: one Divine Sacrifice covers 25 raiders, so it beats any single bot's
    // own. The externals come last of the three because the bot's own button is cheaper than a healer's
    // three-minute one, and the victim pick already skips anyone their own button saves.
    e.Node<YoggSaronNovaRaidCooldownTrigger, YoggSaronNovaRaidCooldownAction>(ACTION_EMERGENCY + 7);
    e.Node<YoggSaronNovaDefensiveTrigger, YoggSaronNovaDefensiveAction>(ACTION_EMERGENCY + 6);
    e.Node<YoggSaronNovaExternalTrigger, YoggSaronNovaExternalAction>(ACTION_EMERGENCY + 5);
    // Level with the phase 2 dodge and above the dps resolver: where the bot stands has to be settled
    // before the reach node is asked to close on the target from there.
    e.Node<YoggSaronIllusionFacingTrigger, YoggSaronIllusionFacingAction>(ACTION_RAID + 3);
    // Above the dps resolver for the same reason, though it never claims the tick: the pet has to be
    // pulled off before it swings again, not after the bot has picked its own target.
    e.Node<YoggSaronPetGuardTrigger, YoggSaronPetGuardAction>(ACTION_RAID + 2);
    // Under every raid node and over reach melee, reach spell and charge: it is the last thing asked
    // before the plain walk it exists to replace, and it stands down the moment the route is clear.
    e.Node<YoggSaronBodyDetourTrigger, YoggSaronBodyDetourAction>(ACTION_RAID - 1);
    // An instant cast, so under every raid walk and dodge, and well over the paladin's own rotation,
    // which would only ever judge its current target.
    e.Node<YoggSaronDiminishPowerJudgementTrigger, YoggSaronDiminishPowerJudgementAction>(ACTION_RAID - 0.5f);
    // Level with the room walk it replaces for the healer, and above heal reach, which every healer spec
    // wires at 38-40. Getting to the station is what puts the whole room in range, so it comes before a
    // step toward one mate; the trigger stands down inside 10 yd of the station and heal reach owns the
    // bot from there.
    e.Node<YoggSaronIllusionHealerStationTrigger, YoggSaronIllusionHealerStationAction>(ACTION_RAID);
    // Over reach melee and set behind, so a bot arriving in front of the Brain goes round to the spot
    // rather than being walked out of range and back. Under the exit walk.
    e.Node<YoggSaronBrainSpotTrigger, YoggSaronBrainSpotAction>(ACTION_RAID);

    // The set dps priority row owns every non-tank's target, so the generic picker stands down rather
    // than pull bots back onto whatever is nearest. This is the exact test that row fires on: zeroing
    // the assist over a wider window leaves a bot with no target source. "attack rti target" is left
    // alone on purpose: a mark a player sets should still win.
    e.OwnTargeting("yogg-saron dps target guard multiplier", Role::NonTank, YoggSaronEncounterActive,
                   Family::DpsAssist);
    e.OwnTargeting("yogg-saron dps target guard multiplier", Role::Tank, YoggSaronTankTargetOwned,
                   Family::TankAssist);

    // Everything that moves a bot somewhere nobody picked. In phase 1 the room is six fixed cloud orbits,
    // so any jump is a jump onto a ring: over one pull 13 of 20 casts left the bot with more orbits in
    // summon range than it started with. Blink and Disengage stay off all fight, since both fire on
    // "something is too close" rather than to close a gap. The gap-closers come back in phases 2 and 3,
    // where there's no orbit to land on, and reach melee survives either way, so melee still walk in.
    e.Block("yogg-saron displacement guard multiplier", Role::Any, YoggSaronEncounterActive,
            Family::Blink | Family::Disengage);
    e.Block("yogg-saron displacement guard multiplier", Role::Any, YoggSaronInPhase1, Family::Charge,
            {"killing spree"});
    e.Block("yogg-saron displacement guard multiplier", Role::Any, YoggSaronBrainIsTarget, Family::Charge);

    e.Multiplier<YoggSaronMovementGuardMultiplier>(Family::Flee | Family::SetBehind | Family::Reach |
                                                   Family::ReachHeal);

    // Whether an action is AoE is its threat type, which any action can override.
    e.Multiplier<YoggSaronPhase1AoeHoldMultiplier>(Family::AnyAction);

    e.Block("yogg-saron stack food guard multiplier", Role::Any, YoggSaronInsideRing, 0, {"food", "drink"});

    // Every one of those buttons otherwise fires on "I am already low", which in phase 1 means after the
    // nova rather than before it: Totemist cast Shamanistic Rage nine times in one pull and still died
    // four times. Melee only, so the tank keeps its own against the Guardians it is holding and the back
    // line keeps its own against Dark Volley.
    e.Block("yogg-saron nova defensive hold", Role::Melee, YoggSaronInPhase1, 0, HeldMagicDefensiveNames());

    // The externals and the raid cooldown, held for the same reason. Every phase 1 death measured was a
    // nova, so nothing else in the phase is worth spending a three-minute cooldown on.
    e.Block("yogg-saron nova cooldown hold", Role::Any, YoggSaronInPhase1, 0,
            {"pain suppression", "pain suppression on party", "guardian spirit on party",
             "hand of sacrifice on party", "divine sacrifice"});

    e.Multiplier<YoggSaronPhase1WalkGuardMultiplier>(Family::AnyMovement);
    e.Multiplier<YoggSaronAntiFearTotemGuardMultiplier>(Family::Spell);

    e.Tick(YoggSaronTick);
}
}  // namespace

EncounterDefinition const& UldYoggSaronDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_YOGGSARON, BossStateGate, "yogg-saron", &DefineYoggSaron);
    return definition;
}

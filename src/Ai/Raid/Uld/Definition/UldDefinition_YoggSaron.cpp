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
// the air with nothing to walk on: one cat druid's Feral Charge (a jump to a spot behind the target)
// hung it there for 23 s until Induce Madness charmed it. GetDistance hides the gap behind the Brain's
// combat reach, so 50 yd read as in range. Warrior Charge and Intercept land on the floor instead, and
// the brain charge node casts those itself on the way in. Charges at the tentacles stay available.
bool YoggSaronBrainIsTarget(PlayerbotAI* botAI)
{
    if (!YoggSaronOnBrainLevel(botAI->GetBot()))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && target->GetEntry() == NPC_BRAIN;
}

// A Crusher that can swing, with no stun holding it.
bool YoggSaronCrusherIsTarget(PlayerbotAI* botAI)
{
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && target->IsAlive() && target->GetEntry() == NPC_CRUSHER_TENTACLE &&
           !YoggSaronCrusherStunHolds(botAI, target);
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
    // Those two take the first paladin that answers and ask only whether the aura is up, from anybody.
    // This one puts it on the paladin that can double it, which is the only way the slot pays for
    // itself: Prayer of Shadow Protection already gives everyone the same 130.
    e.Node<YoggSaronShadowAuraTrigger, YoggSaronShadowAuraAction>(ACTION_RAID);
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
    // Over guardian control: a bot tank only drags with no unmarked Guardian of its own, and guardian
    // control would walk it straight back to the pile.
    e.Node<YoggSaronBeaconDragTrigger, YoggSaronBeaconDragAction>(ACTION_RAID + 4);
    // Under the drag, which takes over at the first mark, and over guardian control for the same reason.
    e.Node<YoggSaronBeaconStageTrigger, YoggSaronBeaconStageAction>(ACTION_RAID + 3.5f);
    // Over the dps resolver and the rotation, under every dodge. One instant cast.
    e.Node<YoggSaronPhase3LustTrigger, YoggSaronPhase3LustAction>(ACTION_RAID + 2);
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

    // Phase 3's same pair against a beacon-healed Guardian, which hits a non-tank for 25-36k a few
    // seconds after the heal. Level with the nova pair, which only runs in phase 1.
    e.Node<YoggSaronBeaconDefensiveTrigger, YoggSaronBeaconDefensiveAction>(ACTION_EMERGENCY + 6);
    e.Node<YoggSaronBeaconExternalTrigger, YoggSaronBeaconExternalAction>(ACTION_EMERGENCY + 5);

    // Under the beacon pair, which a tank's own button and the externals answer first.
    e.Node<YoggSaronTankLayOnHandsTrigger, YoggSaronTankLayOnHandsAction>(ACTION_EMERGENCY + 4);

    // Over everything: the roar's cast is 2.3 s and Aura Mastery is off the global cooldown. The aura it
    // needs goes up ahead of time, as a raid node.
    e.Node<YoggSaronRoarAuraMasteryTrigger, YoggSaronRoarAuraMasteryAction>(ACTION_EMERGENCY + 8);

    // Above the roar press, so the engine is right before anything else runs. Only does anything on the
    // non-combat engine, which a healer with no target always falls back to.
    e.Node<YoggSaronHealerCombatEngineTrigger, YoggSaronHealerCombatEngineAction>(ACTION_EMERGENCY + 9);
    e.Node<YoggSaronRoarAuraTrigger, YoggSaronRoarAuraAction>(ACTION_RAID + 1);
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
    // Same reasoning for the warrior's Mortal Strike on a healing Guardian, over its own rotation, which
    // would only ever strike Yogg.
    e.Node<YoggSaronMortalStrikeTrigger, YoggSaronMortalStrikeAction>(ACTION_RAID - 0.5f);
    // Level with the room walk it replaces for the healer, and above heal reach, which every healer spec
    // wires at 38-40. Getting to the station is what puts the whole room in range, so it comes before a
    // step toward one mate; the trigger stands down inside 10 yd of the station and heal reach owns the
    // bot from there.
    e.Node<YoggSaronIllusionHealerStationTrigger, YoggSaronIllusionHealerStationAction>(ACTION_RAID);
    // Level with the room walk, which a held bot never runs: its room state reads reprisal, not walkingin.
    e.Node<YoggSaronReprisalRegroupTrigger, YoggSaronReprisalRegroupAction>(ACTION_RAID);
    // Over reach melee and set behind, so a bot arriving in front of the Brain goes round to the spot
    // rather than being walked out of range and back. Under the exit walk.
    e.Node<YoggSaronBrainSpotTrigger, YoggSaronBrainSpotAction>(ACTION_RAID);
    // Just over the brain spot walk it speeds up. It never claims the tick, so the walk still runs.
    e.Node<YoggSaronNitroBoostsTrigger, YoggSaronNitroBoostsAction>(ACTION_RAID + 0.5f);
    e.Node<YoggSaronBrainDashTrigger, YoggSaronBrainDashAction>(ACTION_RAID + 0.5f);
    // Over the brain spot walk, which outranks the class charge nodes, so they never fire on it.
    e.Node<YoggSaronBrainChargeTrigger, YoggSaronBrainChargeAction>(ACTION_RAID + 0.25f);

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
    // Killing Spree on the Brain lands the rogue on its model, 21 yd above the floor, where nothing walks
    // it out: one stood there 20 s until Induce Madness charmed it.
    e.Block("yogg-saron displacement guard multiplier", Role::Any, YoggSaronBrainIsTarget, Family::Charge,
            {"killing spree"});

    // Everything else that could put a held bot back on a tentacle. Not the whole Attack family: the dps
    // resolver is an AttackAction itself, and it is what drops the target. The assist is already blocked.
    e.Block("yogg-saron reprisal hold", Role::Any, YoggSaronReprisalHold,
            Family::Melee | Family::DpsAoe | Family::AggressiveTarget | Family::AttackAnything |
                Family::AttackLeastHp | Family::AttackRti | Family::DebuffOnAttacker | Family::Charge);

    // A bot waiting at the door stays there. A player's mark still wins, which the share already allows.
    e.Block("yogg-saron illusion stage hold", Role::Any, YoggSaronIllusionStaged,
            Family::Melee | Family::DpsAoe | Family::AggressiveTarget | Family::AttackAnything |
                Family::AttackLeastHp | Family::DebuffOnAttacker | Family::Charge);

    // Off the Brain while the brain team holds it, the same families as the reprisal hold. DoTs on
    // attackers stop at the earlier line, since they keep ticking after the bot stops.
    e.Block("yogg-saron brain hold", Role::Any, YoggSaronBrainHeld,
            Family::Melee | Family::DpsAoe | Family::AggressiveTarget | Family::AttackAnything |
                Family::AttackLeastHp | Family::AttackRti | Family::Charge | Family::PetAttack);
    e.Block("yogg-saron brain dot hold", Role::Any, YoggSaronBrainDotHeld, Family::DebuffOnAttacker);

    // A bot on its portal spot waits there. The portal walk is quiet once it arrives, so without this
    // reach takes it back to its target and the walk fetches it again for the whole lead. The Yogg dodges
    // are none of these and still move it.
    e.Block("yogg-saron portal spot hold", Role::Any, YoggSaronHoldingPortalSpot,
            Family::Reach | Family::ReachHeal | Family::SetBehind | Family::RearFlank |
                Family::CombatFormationMove | Family::Charge | Family::Follow | Family::MoveRandom);

    // Both put a pet in the Crusher's reach past the pet guard. Acid Spit is cast for the pet at the
    // hunter's target, and Shadowfiend spawns 3 yd from its target and melees it, so the priest saves it
    // for the next target that isn't a Crusher.
    e.Block("yogg-saron crusher pet spell hold", Role::Any, YoggSaronCrusherIsTarget, 0,
            {"pet acid spit", "shadowfiend"});

    e.Multiplier<YoggSaronMovementGuardMultiplier>(Family::Flee | Family::SetBehind | Family::Reach |
                                                   Family::ReachHeal);

    // Whether an action is AoE is its threat type, which any action can override.
    e.Multiplier<YoggSaronPhase1AoeHoldMultiplier>(Family::AnyAction);
    e.Multiplier<YoggSaronIllusionMultiTargetHoldMultiplier>(Family::AnyAction);
    e.Multiplier<YoggSaronExitHoldCastMultiplier>(Family::Spell);
    e.Multiplier<YoggSaronHeldTargetCastMultiplier>(Family::Spell);
    e.Multiplier<YoggSaronPhase3AreaMultiplier>(Family::AnyAction);

    e.Block("yogg-saron stack food guard multiplier", Role::Any, YoggSaronInsideRing, 0, {"food", "drink"});

    // Every one of those buttons otherwise fires on "I am already low", which means after the nova
    // rather than before it: Totemist cast Shamanistic Rage nine times in one pull and still died four
    // times. Melee only, so the tank keeps its own against the Guardians it is holding and the back line
    // keeps its own against Dark Volley.
    e.Block("yogg-saron nova defensive hold", Role::Melee, YoggSaronNovaHold, 0, HeldMagicDefensiveNames());

    // The externals and the raid cooldown, held for the same reason.
    e.Block("yogg-saron nova cooldown hold", Role::Any, YoggSaronNovaHold, 0,
            {"pain suppression", "pain suppression on party", "guardian spirit on party",
             "hand of sacrifice on party", "divine sacrifice"});

    // The aura slot is exclusive, so the paladin's own buff strategy trades it back on the next GCD:
    // the bot tank took Shadow Resistance Aura and cast Devotion Aura again inside the same pull, in
    // both pulls it took the aura at all.
    e.Block("yogg-saron shadow aura hold", Role::Any, YoggSaronShadowAuraHeld, 0,
            {"devotion aura", "retribution aura", "concentration aura", "crusader aura", "sanctity aura",
             "shadow resistance aura", "fire resistance aura", "frost resistance aura"});

    // The cover node spends this on the nova. Left to the class node it goes on an already hurt raid,
    // which is after the hit: six casts over six pulls, every one from the holy paladin, and none of
    // them from the paladin holding the aura it would have doubled.
    e.Block("yogg-saron aura mastery hold", Role::Any, YoggSaronNovaHold, 0, {"aura mastery"});

    // Every other paladin keeps off both, so the elected one owns the slot. Three of them ran the aura
    // in the last pull and its effects changed hands 207 times in 110 s, which cost the elected paladin
    // half a second to take them back right as it pressed the cover; and a press from a paladin whose
    // copy is not the strongest doubles nothing, which is where the pull's second press went. The two
    // generic nodes above are named here as well, because their action adds a strategy that re-casts
    // the aura on its own afterwards. Nothing else of theirs is blocked, so the paladin's own buff
    // strategy puts Devotion or Concentration back in the slot on the next GCD.
    e.Block("yogg-saron shadow aura other hold", Role::Any, YoggSaronShadowAuraOtherHeld, 0,
            {"shadow resistance aura", "aura mastery", "sara shadow resistance action",
             "yogg-saron shadow resistance action"});

    // Not back into the pile. A battle rez strips every raid buff, so the bot returns on a bar 13-17%
    // smaller with no Prayer of Shadow Protection, and the next nova kills it from full health: three
    // of five deaths in the last pull were the same two bots dying again, 14 s and 8 s after coming
    // back. Only Rebirth, since Reincarnation runs from the dead engine where no encounter rule reaches.
    e.Block("yogg-saron battle rez hold", Role::Any, YoggSaronBattleRezHeld, 0, {"rebirth"});

    // The roar paladin keeps its own Concentration Aura until it has pressed, or the buff strategy trades
    // it back on the next global cooldown. The generic shadow resistance nodes add a strategy that would
    // re-cast that aura on its own.
    e.Block("yogg-saron roar aura hold", Role::Any, YoggSaronRoarAuraHeld, 0,
            {"devotion aura", "retribution aura", "crusader aura", "sanctity aura", "shadow resistance aura",
             "fire resistance aura", "frost resistance aura", "sara shadow resistance action",
             "yogg-saron shadow resistance action"});

    // Aura Mastery is the roar's in phase 3: the class nodes spend it on an already hurt raid. The press
    // node casts it directly, past this.
    e.Block("yogg-saron roar aura mastery hold", Role::Any, YoggSaronRoarExpected, 0, {"aura mastery"});

    // An immunity on a healed Guardian's victim sheds it onto somebody else, at full power.
    e.Block("yogg-saron beacon immunity hold", Role::Any, YoggSaronBeaconImmunityHeld, 0,
            {"divine shield", "ice block"});

    // A hunter's drag dies with Feign Death, and Deterrence pacifies the shot.
    e.Block("yogg-saron beacon hunter hold", Role::Any, YoggSaronBeaconHunterHold, 0,
            {"feign death", "deterrence"});

    // Its hop answers a ray at combat priority, which a forced walk refuses, and it outranks the Yogg dodge,
    // so near a ray nothing moved the bot. Its spots also sat beside the ray.
    e.Block("yogg-saron death ray avoid aoe hold", Role::Any, YoggSaronDeathRayNear, Family::AvoidAoe);

    // The holder of a tank swap keeps its threat off the pile so the Guardians stay with the taker. Holy
    // Shield and Shield of Righteousness stay.
    e.Block("yogg-saron tank swap hold", Role::Tank, YoggSaronTankShedding, 0,
            {"consecration", "hammer of the righteous", "avenger's shield", "hand of reckoning",
             "righteous defense"});

    // Avenging Wrath locks Divine Protection for 30 s: one tank pressed it and had no button left when the
    // pile killed it.
    e.Block("yogg-saron tank wrath hold", Role::Tank, YoggSaronTankWrathHeld, 0, {"avenging wrath"});

    // Moves 30% of a dps's damage onto a tank already under the pile.
    e.Block("yogg-saron tank sacrifice hold", Role::Tank, YoggSaronInPhase3, 0, {"hand of sacrifice on party"});

    // Kept for the tanks: every pull spent all four on dps in phase 1. The tank node casts it directly.
    e.Block("yogg-saron lay on hands hold", Role::Any, YoggSaronEncounterActive, 0, {"lay on hands on party"});
    e.Block("yogg-saron lay on hands hold", Role::NonTank, YoggSaronEncounterActive, 0, {"lay on hands"});
    e.Block("yogg-saron lay on hands hold", Role::Tank, YoggSaronBeforePhase3, 0, {"lay on hands"});

    // A healer with no target is dropped straight back to the non-combat engine.
    e.Block("yogg-saron healer target hold", Role::Heal, YoggSaronHealerTargetless, 0, {"drop target"});

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

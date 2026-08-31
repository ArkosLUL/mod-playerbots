#include "UldMultipliers.h"

#include <set>
#include <string>

#include "BurstCooldowns.h"
#include "ChooseTargetActions.h"
#include "FollowActions.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PriestActions.h"
#include "EncounterHelpers.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldBossHelper.h"
#include "UldEncounter_IronAssembly.h"
#include "UldEncounter_Algalon.h"
#include "UldEncounter_Thorim.h"
#include "UldEncounter_Vezax.h"
#include "UldHardMode.h"
#include "UldActions.h"
#include "UldTriggers.h"
#include "UldScripts.h"
#include "VehicleActions.h"

using namespace EncounterHelpers;

// Algalon the Observer
//
// Big Bang is unavoidable raid-wide damage that immunity does not stop, so whoever is holding it off
// this cast has to still have the cooldown when it lands. Spending it on the low-mana or
// critical-health nodes thirty seconds earlier is what turns a survivable cast into a reset: with
// nobody left standing, CheckTargets finds no targets and Algalon ascends and evades.
float AlgalonSoakCooldownReserveMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    if (!dynamic_cast<CastDispersionAction*>(action) && action->getName() != "guardian spirit")
        return 1.0f;

    if (!AlgalonEncounterActive(botAI))
        return 1.0f;

    if (GetAlgalonBigBangSoaker(botAI) != bot)
        return 1.0f;

    // During the cast itself the soak action must be free to spend it.
    return AlgalonBigBangCasting(botAI) ? 1.0f : 0.0f;
}

float AlgalonCollapsingStarAoeMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<DpsAoeAction*>(action) || !AlgalonEncounterActive(botAI))
        return 1.0f;

    // One star alive is the state the pacing is trying to reach, so splash is harmless there. Phase 2
    // has no stars at all, which leaves Dark Matter cleave untouched.
    return AlgalonAliveStarCount(botAI) >= 2 ? 0.0f : 1.0f;
}

float AlgalonTargetGuardMultiplier::GetValue(Action* action)
{
    if (!action || !AlgalonEncounterActive(botAI))
        return 1.0f;

    static std::set<std::string> const encounterOwned = {
        "algalon constellation taunt action", "algalon constellation kite action",
        "algalon dark matter tank action",    "algalon collapsing star focus action",
        "algalon dark matter mark action"};

    if (encounterOwned.count(action->getName()))
        return 1.0f;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    // A Living Constellation carries 20x base health and cannot be killed inside the six minute
    // enrage; the kite is the only way one ever leaves. Whoever is holding it still hits it, since
    // that threat is what keeps it following.
    if (currentTarget && currentTarget->GetEntry() == PB_NPC_LIVING_CONSTELLATION &&
        currentTarget->GetVictim() != bot)
    {
        return 0.0f;
    }

    // No hole standing with a Big Bang closing in. A skull mark on the star is only advice, and the
    // raid has to actually stop hitting the boss for the star to die in time.
    if (currentTarget && currentTarget == GetAlgalon(botAI) && AlgalonNeedsShelterUrgently(botAI) &&
        !botAI->IsTank(bot))
    {
        return 0.0f;
    }

    return 1.0f;
}

float AlgalonControlMovementMultiplier::GetValue(Action* action)
{
    if (!action || !AlgalonEncounterActive(botAI))
        return 1.0f;

    // Only the roles the formation actually places. Melee and the off-tank hold the boss, so both
    // keep every generic mover.
    if (!AlgalonTakesRingSlot(bot) && !botAI->IsMainTank(bot))
        return 1.0f;

    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero would also kill targeting;
    // ReachTargetAction is what walks a healer into range of someone the rings cannot reach.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {
        "algalon raid position action", "algalon big bang hide action", "algalon cosmic smash action",
        "algalon leave black hole action", "algalon constellation kite action"};

    return encounterMovers.count(action->getName()) ? 1.0f : 0.0f;
}

// XT-002 Deconstructor
//
// The exposed Heart transfers its damage to XT, so it is the only real burst window the encounter
// offers - but spending cooldowns there is exactly what kills the Heart and triggers hard mode. The
// two modes therefore want opposite behaviour, and the config is the only statement of intent.
float XT002BurstWindowMultiplier::GetValue(Action* action)
{
    if (!action || !IsBurstCooldownAction(action->getName()))
        return 1.0f;

    uint32 now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedValue = EvaluateWindow();
    }
    return cachedValue;
}

float XT002BurstWindowMultiplier::EvaluateWindow()
{
    Unit* xt002 = GetXT002(botAI);
    if (!xt002)
        return 1.0f;

    // Heartbreak is up: there will be no further Heart phase, so there is nothing left to save for.
    if (IsXT002HeartbreakActive(botAI))
        return 1.0f;

    if (GetXT002ExposedHeart(botAI))
        return IsXT002HardModeActive(botAI) ? 1.0f : 0.0f;

    // Normal mode: the last Heart phase is behind us below this, so the push is the window.
    if (!IsXT002HardModeActive(botAI) && xt002->GetHealthPct() < ULDUAR_XT002_FINAL_PUSH_HP_PCT)
        return 1.0f;

    return 0.0f;
}

float XT002TargetGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    Unit* xt002 = GetXT002(botAI);
    if (!xt002)
        return 1.0f;

    // The stand-downs hand generic behaviour to encounter nodes that none of them run before the pull,
    // so out of combat they only stop bots fighting anything else in the room. The Heart floor below
    // is deliberately outside this gate: it has to hold whenever an exposed Heart is in the room, and
    // XT's combat flag is not something worth betting the raid's difficulty on.
    if (xt002->IsInCombat())
    {
        // The class-generic redirects buff whoever the group flags as main tank, which is the wrong
        // sink while a Pummeller is out; xt002 redirect threat action picks the tank that needs it.
        if (dynamic_cast<CastMisdirectionOnMainTankAction*>(action) ||
            dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action))
        {
            return 0.0f;
        }

        // "xt002 avoid hazard action" answers both hazards this fight has and is meant to be the only
        // thing moving a bot for them. The generic one sits at the same relevance, fires in every gap
        // where ours returns false, and flees a flat AiPlayerbot.FleeDistance in a direction of its
        // own picking - two movers pulling different ways is what leaves a bot sliding in place.
        if (dynamic_cast<AvoidAoeAction*>(action))
            return 0.0f;

        // Both immunities cover all magic and carry SPELL_ATTR1_IMMUNITY_PURGES_EFFECT, so applying
        // one strips the debuff outright - and the boss script summons the Void Zone or the Life Spark
        // from an AfterEffectRemove hook that does not care why the aura went away. Bubbling drops the
        // puddle instantly, wherever the carrier is standing, which is the middle of the raid at the
        // point where a bot is at critical health. Eating the hit is the cheaper trade. Hand of
        // Protection is physical-only and does not strip either debuff, so it is left alone.
        if (dynamic_cast<CastDivineShieldAction*>(action) || dynamic_cast<CastIceBlockAction*>(action))
        {
            if (bot->HasAura(GetXT002GravityBombSpellId(bot)) || bot->HasAura(GetXT002SearingLightSpellId(bot)))
                return 0.0f;
        }

        // xt002 set dps priority action owns every bot's target, so both generic pickers stand down
        // rather than pulling bots back onto whatever is nearest. The tank one matters most: it ranks
        // any add the tank has no aggro on above the boss, so it walks the tank into the add pile and
        // XT follows. "attack rti target" is deliberately left alone: bots no longer set marks, but a
        // player's mark should still win. Healers are out: a healer the encounter action has not
        // reached yet would be left with no target at all, and a bot that never attacks never enters
        // combat, which costs it every heal that lives on the combat engine.
        if (!botAI->IsTank(bot) && !botAI->IsHeal(bot) && dynamic_cast<DpsAssistAction*>(action))
            return 0.0f;

        if (botAI->IsTank(bot) && dynamic_cast<TankAssistAction*>(action))
            return 0.0f;

        // Ranged dps, healers and any carrier are pinned: each has a destination of its own, and a
        // generic mover walking them off it is either a bot back in the splash or a carrier dropping
        // one on the raid it just left. Healers are in the sweep because "follow" lives on the
        // non-combat engine and a healer with nothing to do drops combat constantly - it out-issued
        // the anchor three to one and walked them to the master all fight. Nothing is lost by taking
        // their disperse with it: the anchor hands out a slot per bot now, so it is the thing doing
        // the spreading. Melee keep every generic mover unless they are carrying.
        //
        // AttackAction derives from MovementAction but never moves the bot, so leaving it in the sweep
        // only zeroes the encounter's own targeting and leaves the bot standing with nothing to shoot.
        if (dynamic_cast<MovementAction*>(action) && !dynamic_cast<AttackAction*>(action))
        {
            bool const carryingDebuff = bot->HasAura(GetXT002SearingLightSpellId(bot)) ||
                                        bot->HasAura(GetXT002GravityBombSpellId(bot));

            if (carryingDebuff || botAI->IsRangedDps(bot) || botAI->IsHeal(bot))
            {
                static std::set<std::string> const encounterMovers = {
                    "xt002 raid position action", "xt002 debuff carrier action", "xt002 avoid hazard action"};

                if (!encounterMovers.count(action->getName()))
                    return 0.0f;
            }
        }
    }

    if (IsXT002HardModeActive(botAI))
        return 1.0f;

    // Normal mode safety floor. The priority action stops offering the Heart here too, but that only
    // stops bots from picking it up again - this is what stops the ones already swinging at it from
    // landing the hit that flips the raid into hard mode.
    Unit* heart = GetXT002ExposedHeart(botAI);
    if (!heart || heart->GetHealthPct() > ULDUAR_XT002_HEART_SAFE_HP_PCT)
        return 1.0f;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    if (currentTarget != heart)
        return 1.0f;

    // Only the actions that would land a hit on the Heart are blocked. Blocking everything would
    // leave the bot parked in a Gravity Bomb or a Void Zone with no heals and no way to retarget.
    if (dynamic_cast<MovementAction*>(action) || dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    static std::set<std::string> const retargets = {"xt002 set dps priority action",
                                                    "xt002 pummeller taunt action",
                                                    "xt002 redirect threat action"};

    return retargets.count(action->getName()) ? 1.0f : 0.0f;
}

// Iron Assembly
float IronAssemblyDisableAutomaticTargetingMultiplier::GetValue(Action* action)
{
    if (botAI->GetState() != BOT_STATE_COMBAT)
        return 1.0f;

    if (!dynamic_cast<DpsAssistAction*>(action) && !dynamic_cast<TankAssistAction*>(action))
        return 1.0f;

    return IronAssemblyFormationActive(botAI) ? 0.0f : 1.0f;
}

float IronAssemblyMovementGuardMultiplier::GetValue(Action* action)
{
    if (!action || !IronAssemblyFormationActive(botAI))
        return 1.0f;

    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero would also kill targeting, and
    // ReachTargetAction is what walks a healer into range of someone the formation cannot reach.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {
        "iron assembly overload action",
        "iron assembly lightning tendrils action",
        "iron assembly rune of death action",
        "iron assembly overwhelming power run out action",
        "iron assembly rune of power action",
        "iron assembly rune of power soak action",
        "iron assembly raid position action",
        "iron assembly tank assignment action"};

    if (encounterMovers.count(action->getName()))
        return 1.0f;

    // Only while this bot is actually committed to a hazard. Outside that window the generic movers
    // are what bring it back to the formation, and the position node has already yielded.
    return IronAssemblyMemberMustMove(botAI, bot) ? 0.0f : 1.0f;
}

// Thorim
//
// The nodes that pick what to attack, as opposed to the ones that act on what is already picked. They
// all derive from AttackAction, so a guard that zeroes AttackAction wholesale also zeroes the only
// thing that could correct a bad target, and a bot holding one is stuck on it for the rest of the
// pull. Thorim's balcony sits above the arena box, which is exactly how that happened.
static bool ThorimIsTargetSelectionAction(Action* action)
{
    return dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<DpsAoeAction*>(action) ||
           dynamic_cast<TankAssistAction*>(action) || dynamic_cast<AggressiveTargetAction*>(action) ||
           dynamic_cast<AttackAnythingAction*>(action) || dynamic_cast<AttackLeastHpTargetAction*>(action);
}

float ThorimRunicBarrierMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Only the swing and the walk that sets it up. Casts, heals and hazard dodges are untouched: a
    // blanket damage stop would throw away DPS the shield was never going to punish, and the gauntlet
    // is on a 2:45 timer in hard mode.
    if (!dynamic_cast<MeleeAction*>(action) && !dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    if (!ThorimBarrierBailLatched(botAI, bot))
        return 1.0f;

    // Gated on the current target, so a switch onto anything else releases this for free.
    Unit* colossus = GetThorimRunicColossus(botAI);
    return colossus && AI_VALUE(Unit*, "current target") == colossus ? 0.0f : 1.0f;
}

float ThorimArenaLeashMultiplier::GetValue(Action* action)
{
    if (!action || !dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // Hazard dodges stay live. Everything else, the chase included, is what this exists to stop.
    if (dynamic_cast<AvoidAoeAction*>(action))
        return 1.0f;

    // Picking a target is not walking anywhere. A bot outside the leash still has to be able to
    // choose one while the leash action carries it back, or it arrives with nothing to do.
    if (ThorimIsTargetSelectionAction(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {"thorim arena leash action",
                                                          "thorim arena positioning action",
                                                          "thorim charged orb action",
                                                          "thorim sif blizzard action",
                                                          "thorim sif frost nova action"};

    if (encounterMovers.count(action->getName()))
        return 1.0f;

    return ThorimArenaLeashBreached(botAI, bot) ? 0.0f : 1.0f;
}

float ThorimArenaTargetGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    if (!dynamic_cast<AttackAction*>(action) && !dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    // The escape hatch. Zeroing these too is a deadlock: the guard fires because the target is wrong,
    // and the action it kills is the one that would pick a different one.
    if (ThorimIsTargetSelectionAction(action))
        return 1.0f;

    if (!ThorimSplitActive(botAI) || GetThorimSquad(botAI, bot) != ThorimSquad::Arena)
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    return target && !ThorimInArenaBox(target) ? 0.0f : 1.0f;
}

float ThorimArenaAnchorGuardMultiplier::GetValue(Action* action)
{
    if (!action || !dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // Unlike the leash, this one exempts the chase: an add can land 24 yd from the centre, which puts
    // it up to 38 yd from an outer ring slot, and a ranged bot that cannot step into range is silent.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action) ||
        dynamic_cast<AvoidAoeAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {"thorim arena positioning action",
                                                          "thorim arena leash action",
                                                          "thorim charged orb action",
                                                          "thorim sif blizzard action",
                                                          "thorim sif frost nova action"};

    if (encounterMovers.count(action->getName()))
        return 1.0f;

    return ThorimArenaAnchorSettled(botAI, bot) ? 0.0f : 1.0f;
}

float ThorimMovementGuardMultiplier::GetValue(Action* action)
{
    if (!action || !dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero would also kill targeting, and a
    // bot that cannot clear a Rune Detonation is worse off than one standing slightly out of place.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action) ||
        dynamic_cast<AvoidAoeAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {"thorim phase 2 positioning action",
                                                          "thorim lightning charge action",
                                                          "thorim sif blizzard action",
                                                          "thorim sif frost nova action"};

    if (encounterMovers.count(action->getName()))
        return 1.0f;

    return ThorimMeleeRingSettled(botAI, bot) ? 0.0f : 1.0f;
}

// Freya
float FreyaDisableAutomaticTargetingMultiplier::GetValue(Action* action)
{
    bool const isDpsAssist = botAI->GetState() == BOT_STATE_COMBAT && dynamic_cast<DpsAssistAction*>(action);
    bool const isTankAssist = botAI->GetState() == BOT_STATE_COMBAT && dynamic_cast<TankAssistAction*>(action);

    if (!isDpsAssist && !isTankAssist)
        return 1.0f;

    Unit* freya = AI_VALUE2(Unit*, "find target", "freya");
    if (!freya || !freya->IsAlive())
        return 1.0f;

    if (isDpsAssist)
        return PlayerbotAI::IsDps(bot) ? 0.0f : 1.0f;

    // Every tank, for the whole encounter. Gating this on "the add ladder has something" is what let
    // generic assist through on a pure Detonating Lasher wave, where the off-tank collected the wave and
    // walked it into the raid stack.
    return botAI->IsTank(bot) ? 0.0f : 1.0f;
}

float FreyaTrioSyncMultiplier::GetValue(Action* action)
{
    if (botAI->IsTank(bot) || !PlayerbotAI::IsDps(bot))
        return 1.0f;

    // Movement and heals are never held back: a bot waiting out the floor still has to dodge a
    // Nature Bomb and reach a Healthy Spore.
    bool const isDamage = dynamic_cast<MeleeAction*>(action) ||
                          (dynamic_cast<CastSpellAction*>(action) && !dynamic_cast<CastHealingSpellAction*>(action));
    if (!isDamage)
        return 1.0f;

    Unit* freya = AI_VALUE2(Unit*, "find target", "freya");
    if (!freya || !freya->IsAlive())
        return 1.0f;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    return FreyaTrioSyncSuppress(state, AI_VALUE(Unit*, "current target")) ? 0.0f : 1.0f;
}

float FreyaLasherFinishAoeMultiplier::GetValue(Action* action)
{
    if (!action || action->getThreatType() != Action::ActionThreatType::Aoe)
        return 1.0f;

    // Healing AoE is never held: the finish is exactly when the raid is topping up between blasts.
    if (dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    Unit* freya = AI_VALUE2(Unit*, "find target", "freya");
    if (!freya || !freya->IsAlive())
        return 1.0f;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    // Held for as long as the bot is within a Detonate radius of the finish, which outlasts its own
    // step-out: a bot that walked clear and kept casting Blizzard onto the pile would blow the whole
    // pack up at once anyway, which is the thing this exists to stop.
    return GetFreyaFinishingPackNear(botAI, state, ULDUAR_FREYA_LASHER_PACK_CLEAR) ? 0.0f : 1.0f;
}

float FreyaGroundTremorCastGateMultiplier::GetValue(Action* action)
{
    CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
    if (!spellAction || dynamic_cast<CastMeleeSpellAction*>(action) || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    // The encounter's own hold action is what stops a cast already in flight; this only decides what
    // is allowed to start.
    if (action->getName() == "freya ground tremor hold cast")
        return 1.0f;

    uint32 const now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedRemainingMs = EvaluateWindow();
    }

    if (cachedRemainingMs <= 0)
        return 1.0f;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", spellAction->getSpell());
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 1.0f;

    // Anything that still lands before the tremor is worth starting, so the instants a locked-out bot
    // would fall through to are never held - only the casts the interrupt would eat.
    uint32 const castTime = spellInfo->CalcCastTime();
    if (castTime == 0 && !spellInfo->IsChanneled())
        return 1.0f;

    return static_cast<int32>(castTime) < cachedRemainingMs ? 1.0f : 0.0f;
}

int32 FreyaGroundTremorCastGateMultiplier::EvaluateWindow()
{
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_FREYA);
    if (!IsFreyaGroundTremorCasting(boss))
        return 0;

    Spell* tremor = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);

    return tremor ? tremor->GetCastTimeRemaining() : 0;
}

// Ignis the Furnace Master
float IgnisMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    // Slag Pot is a vehicle ride: the victim is held in place for the full duration, so movement
    // orders only fight the ride and leave the bot facing the wrong way when it drops.
    bool const slagPotRide = IsIgnisSlagPotVictim(bot) && dynamic_cast<MovementAction*>(action);

    // The construct tanks are deliberately parked on a Scorched Ground patch - that is what stacks
    // Heat on the construct - so the generic dodge would undo the kite every tick. The main tank is
    // exempt for the opposite reason: his arc rotation already steps him clear of every patch he
    // drops, and a dodge on top of it would drag Ignis across the room.
    bool const parkedTank = action->getName() == "ignis scorched ground action" &&
                            (GetIgnisConstructTankIndex(botAI, bot) >= 0 || botAI->IsMainTank(bot));

    // GetIgnis walks the grid, and this runs against every action of every bot on the tick, so it is
    // asked last - only once something Ignis-specific would actually be blocked.
    if (!slagPotRide && !parkedTank)
        return 1.0f;

    return GetIgnis(botAI) ? 0.0f : 1.0f;
}

float IgnisTankMovementMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    // Only the three roles the encounter places. Everyone else keeps every generic mover, which is
    // also what keeps the ranged half spread and in range without an anchor of their own.
    if (!botAI->IsMainTank(bot) && GetIgnisConstructTankIndex(botAI, bot) < 0)
        return 1.0f;

    if (!dynamic_cast<ReachTargetAction*>(action) && !dynamic_cast<CastReachTargetSpellAction*>(action) &&
        !dynamic_cast<FollowAction*>(action) && !dynamic_cast<FleeAction*>(action))
    {
        return 1.0f;
    }

    return IsIgnisEngaged(botAI) ? 0.0f : 1.0f;
}

float IgnisDisableDefaultTargetingMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    if (!dynamic_cast<DpsAssistAction*>(action) && !dynamic_cast<TankAssistAction*>(action) &&
        !dynamic_cast<AttackRtiTargetAction*>(action))
    {
        return 1.0f;
    }

    return IsIgnisEngaged(botAI) ? 0.0f : 1.0f;
}

float IgnisFlameJetsHoldCastMultiplier::GetValue(Action* action)
{
    CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
    if (!spellAction || dynamic_cast<CastMeleeSpellAction*>(action) || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    // The encounter's own hold action is what stops a cast already in flight; this only decides what
    // is allowed to start.
    if (action->getName() == "ignis flame jets hold cast action")
        return 1.0f;

    uint32 const now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedRemainingMs = EvaluateWindow();
    }

    if (cachedRemainingMs <= 0)
        return 1.0f;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", spellAction->getSpell());
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 1.0f;

    // Anything that still lands before the knockback is worth starting - only the casts Flame Jets
    // would eat are held, so healers keep their short heals through the window.
    uint32 const castTime = spellInfo->CalcCastTime();
    if (castTime == 0 && !spellInfo->IsChanneled())
        return 1.0f;

    return static_cast<int32>(castTime) < cachedRemainingMs ? 1.0f : 0.0f;
}

int32 IgnisFlameJetsHoldCastMultiplier::EvaluateWindow()
{
    Unit* boss = GetIgnis(botAI);
    if (!IsIgnisFlameJetsCasting(boss))
        return 0;

    Spell* jets = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);

    return jets ? jets->GetCastTimeRemaining() : 0;
}

// Only the two main-tank redirects are blocked - casting to the shared BuffOnMainTankAction base
// would take Beacon of Light, Earth Shield and Thorns down with them.
//
// Entry lookups rather than "find target": that value only resolves creatures which already have
// this bot on their threat list, so a bot parked on one Iron Assembly member never sees the other
// two.
// Flame Leviathan
// The whole encounter is fought from vehicles and FlameLeviathanDriveAction is the only thing that
// steers one. Anything else that moves would fight it for the MotionMaster, so the generic movers
// are zeroed outright - but only once the boss is actually engaged, or the raid could never drive
// into the arena in the first place.
float FlameLeviathanVehicleMovementMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase)
        return 1.0f;

    switch (vehicleBase->GetEntry())
    {
        case NPC_SALVAGED_SIEGE_ENGINE:
        case NPC_SALVAGED_SIEGE_ENGINE_TURRET:
        case NPC_SALVAGED_DEMOLISHER:
        case NPC_SALVAGED_DEMOLISHER_TURRET:
        case NPC_VEHICLE_CHOPPER:
            break;
        default:
            return 1.0f;
    }

    // One dynamic_cast up front: every rotation cast falls out here without touching the rest.
    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // Boarding has to survive, or a bot that lost its vehicle can never take another one; and
    // leaving has to survive so nobody is welded in after the kill.
    if (dynamic_cast<FlameLeviathanDriveAction*>(action) ||
        dynamic_cast<FlameLeviathanEnterVehicleAction*>(action) || dynamic_cast<LeaveVehicleAction*>(action))
        return 1.0f;

    // Asked last, because it walks the target list: only once something would actually be blocked.
    return FlameLeviathanEngaged(botAI) ? 0.0f : 1.0f;
}

namespace
{

// Everything generic that can walk a bot back onto a patch it has just cleared. "avoid aoe" is on the
// list because it sits at ACTION_EMERGENCY, outranks every Razorscale node, and picks its bearing
// with no knowledge of the other patches on the floor. CastReachTargetSpellAction is a CastSpellAction
// rather than a MovementAction, so it cannot be caught by a base-class filter.
bool IsRazorscaleGenericMover(Action* action)
{
    return dynamic_cast<ReachTargetAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action) ||
           dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<RearFlankAction*>(action) ||
           dynamic_cast<FollowAction*>(action) || dynamic_cast<AvoidAoeAction*>(action);
}

}

float RazorscaleMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    if (!IsRazorscaleGenericMover(action))
        return 1.0f;

    // Asked last, because it is the expensive half.
    uint32 const now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedBlocked = MoversBlocked();
    }

    return cachedBlocked ? 0.0f : 1.0f;
}

bool RazorscaleMultiplier::MoversBlocked()
{
    // Scoped to a live dodge only: held permanently this is the freeze bug, where a silently-failing
    // MoveTo strands the bot for the rest of the fight.
    if (RazorscaleBossHelper::FindDevouringFlameNear(botAI, RazorscaleBossHelper::DEVOURING_FLAME_CLEAR_RADIUS))
        return true;

    // Standing clear, but the spot the movers would walk to is on fire. Ranged never has to close, and
    // holding them here would fight the grounded stack-up for no gain.
    if (!botAI->IsMelee(bot))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    return RazorscaleBossHelper::DevouringFlameBlocks(bot, target->GetPositionX(), target->GetPositionY());
}

// Kologarn
//
// Every bot's target here is decided per role in code rather than through a raid icon, so the
// generic pickers have to be silenced or they fight it: "dps target" falls back to a smart-target
// strategy when no icon is set and is therefore never null, which keeps NotDpsTargetActiveTrigger
// permanently true. Same three casts as the Eredar Twins guard in SWP - the debuff one matters
// because it is what lands DoTs on whatever the bot drifted onto.
float KologarnDisableAutomaticTargetingMultiplier::GetValue(Action* action)
{
    if (botAI->GetState() == BOT_STATE_NON_COMBAT)
        return 1.0f;

    if (!dynamic_cast<DpsAssistAction*>(action) && !dynamic_cast<TankAssistAction*>(action) &&
        !dynamic_cast<CastDebuffSpellOnAttackerAction*>(action))
    {
        return 1.0f;
    }

    // Asked last, because it walks the target list: only once something would actually be blocked.
    // Engagement, not proximity: before the pull the per-role pickers are inert, so silencing the
    // generic ones would leave the bot with none, and trash within 100yd of him would be unfightable.
    return KologarnEncounterActive(botAI) ? 0.0f : 1.0f;
}

float KologarnMultiplier::GetValue(Action* action)
{
    if (!action || !dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // A gripped bot is a stunned passenger on the right arm until the arm releases it or it dies;
    // movement orders only fight the ride and leave it facing the wrong way when it drops.
    if (!IsKologarnStoneGripped(bot))
        return 1.0f;

    return GetKologarn(botAI) ? 0.0f : 1.0f;
}

float UldThreatRedirectMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CastMisdirectionOnMainTankAction*>(action) &&
        !dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action))
    {
        return 1.0f;
    }

    static uint32 const noRedirectBosses[] = {
        // "freya redirect threat" aims at the add tank while it holds the Snaplasher or the Conservator
        NPC_FREYA,
        // Every phase is a different creature with a fresh threat table; phase 3 splits VX-001 and
        // the Aerial Command Unit across two tanks
        NPC_LEVIATHAN_MKII, NPC_VX001, NPC_AERIAL_COMMAND_UNIT,
        // Arena and gauntlet squads each bring their own tank, then Unbalancing Strike swaps
        NPC_THORIM,
        // Phase Punch swaps main and assist tank on a stack timer
        PB_NPC_ALGALON};

    for (uint32 entry : noRedirectBosses)
    {
        if (GetFirstAliveUnitByEntry(botAI, entry))
            return 0.0f;
    }

    // Razorscale holds nothing outside the permanent ground phase - the Dark Rune adds belong to the
    // assist tanks, and the harpoon knockdowns above 50% put her on the floor with no tank on her at
    // all. Only the ground phase is single-tank, and the generic node is right there.
    if (Unit* razorscale = GetFirstAliveUnitByEntry(botAI, NPC_RAZORSCALE))
    {
        return RazorscaleBossHelper::IsFlyingPhaseFor(razorscale) ? 0.0f : 1.0f;
    }

    return 1.0f;
}

// Several Ulduar bosses put their real DPS check minutes after the pull, where the generic
// hold-until-the-tank-engages window spends everything on a phase that does not matter.
float UlduarBurstWindowMultiplier::GetValue(Action* action)
{
    if (!action || !IsBurstCooldownAction(action->getName()))
        return 1.0f;

    if (!bot->IsInCombat())
        return 1.0f;

    uint32 now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedValue = EvaluateWindow();
    }

    std::string const name = action->getName();
    bool const allowed = (name == "bloodlust" || name == "heroism") ? cachedValue.allowLust : cachedValue.allowAll;

    return allowed ? 1.0f : 0.0f;
}

UlduarBurstWindowMultiplier::BurstWindow UlduarBurstWindowMultiplier::EvaluateWindow()
{
    Unit* razorscale = nullptr;
    Unit* leviathanMkII = nullptr;
    Unit* vx001 = nullptr;
    Unit* aerialCommandUnit = nullptr;
    Unit* steelbreaker = nullptr;
    Unit* molgeim = nullptr;
    Unit* brundir = nullptr;
    Unit* freya = nullptr;
    Unit* thorim = nullptr;

    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        switch (unit->GetEntry())
        {
            case NPC_RAZORSCALE: razorscale = unit; break;
            case NPC_LEVIATHAN_MKII: leviathanMkII = unit; break;
            case NPC_VX001: vx001 = unit; break;
            case NPC_AERIAL_COMMAND_UNIT: aerialCommandUnit = unit; break;
            case NPC_STEELBREAKER: steelbreaker = unit; break;
            case NPC_MOLGEIM: molgeim = unit; break;
            case NPC_BRUNDIR: brundir = unit; break;
            case NPC_FREYA: freya = unit; break;
            case NPC_THORIM: thorim = unit; break;
            default: break;
        }
    }

    // The sweep above is capped at SightDistance, and her second flight point (619, -238, 475) sits
    // past 100yd from most of the raid - so she has to be resolved off the threat list as well, or
    // the fall-through at the bottom opens the gate instead of closing it. DoZoneInCombat() on her
    // first flight point puts the whole raid on that list, and "find target" does not touch
    // UpdateBossAI(), so the tank-reassignment hazard does not apply.
    if (!razorscale)
        razorscale = AI_VALUE2(Unit*, "find target", "razorscale");

    // She takes no damage at all while airborne, so every landing is a real burn window - harpoon
    // knockdowns included. Lust is the exception: a knockdown is ~30s at full health, and spending a
    // 10-minute cooldown there costs the permanent sub-50% ground phase the fight is balanced around.
    if (razorscale)
    {
        bool const grounded = razorscale->GetPositionZ() <= RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD;

        return {grounded, RazorscaleBossHelper::IsGroundPhaseFor(razorscale)};
    }

    // Damage in P1-P3 counts, so only lust waits. All three mechs up at once is phase 4, the burn
    // the fight is actually balanced around - and it outlasts a 10-minute lust.
    if (leviathanMkII || vx001 || aerialCommandUnit)
        return {true, leviathanMkII && vx001 && aerialCommandUnit};

    // The council members resurrect each other until one is left, so only the survivor is a real
    // kill. Covers the hard mode too, where that survivor is the empowered phase-3 Steelbreaker.
    if (steelbreaker || molgeim || brundir)
    {
        uint32 const aliveCount = (steelbreaker ? 1u : 0u) + (molgeim ? 1u : 0u) + (brundir ? 1u : 0u);

        return {true, aliveCount == 1};
    }

    // Attuned to Nature is 150 stacks of +8% healing received, so nothing spent on Freya lands until
    // the wave adds have taken it off her; the core drops it when it enters the final phase
    // (boss_freya.cpp). Personal cooldowns wait with lust rather than opening on the adds: they are
    // not boss-flagged, so HoldBurstUntilTankEngagedMultiplier zeroes burst on them anyway.
    if (freya)
    {
        bool const finalPhase =
            !freya->HasAura(SPELL_ATTUNED_TO_NATURE) || freya->GetHealthPct() <= FREYA_LUST_FALLBACK_PCT;

        return {finalPhase, finalPhase};
    }

    if (thorim)
    {
        // He is immune on his balcony for the whole gauntlet.
        bool const onArenaFloor = thorim->GetPositionZ() <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD;

        return {onArenaFloor, onArenaFloor};
    }

    // P1 damage lands on Sara and is wasted; P3 is the body burn, with no Shadow Barrier and no
    // Guardian soaking it up. Sara has to be part of the check because Yogg himself is not summoned
    // until the P2 transition, so P1 would otherwise fall through ungated. Presence is not the
    // encounter though: Sara is a static spawn that LoadAllGrids() makes findable from instance
    // creation, and she stands 114yd from Auriaya's room and 167yd from Vezax, so matching on sight
    // alone shut the gate for two unrelated bosses. She only enters combat when JustEngagedWith
    // calls SetInCombatWithZone().
    Unit* const yoggSaron = bot->FindNearestCreature(NPC_YOGG_SARON, sPlayerbotAIConfig.sightDistance, true);
    Unit* const saraPhaseOne = bot->FindNearestCreature(NPC_SARA_PHASE_1, sPlayerbotAIConfig.sightDistance, true);

    if ((yoggSaron && yoggSaron->IsInCombat()) || (saraPhaseOne && saraPhaseOne->IsInCombat()))
    {
        bool const phaseThree = YoggSaronInPhase3(botAI);

        return {phaseThree || YoggSaronInPhase2(botAI), phaseThree};
    }

    return {};
}

float AuriayaMovementGuardMultiplier::GetValue(Action* action)
{
    // Engaged, not merely present: these stand-downs hand generic behaviour to encounter nodes that
    // none of them run before the pull, so on sight alone they would leave bots rooted with no target.
    if (!action || !IsAuriayaEngaged(botAI))
        return 1.0f;

    // auriaya set dps priority action owns every non-tank's target, so the generic picker stands down
    // rather than pulling bots back onto whatever is nearest. "attack rti target" is deliberately left
    // alone: bots no longer set marks here, but a mark the player sets should still win.
    if (!botAI->IsTank(bot) && dynamic_cast<DpsAssistAction*>(action))
        return 0.0f;

    // Only the main tank and the ranged half of the raid stand on a spot. The off-tank chases Sanctum
    // Sentries and melee ride the boss, so both keep every generic mover - melee in particular still
    // need SetBehindTargetAction.
    if (!botAI->IsMainTank(bot) && !botAI->IsRanged(bot))
        return 1.0f;

    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero here would also kill targeting;
    // ReachTargetAction is what walks a healer into range of someone the anchor cannot reach.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {
        "auriaya raid position action", "auriaya seeping essence action", "auriaya fall from floor action"};

    return encounterMovers.count(action->getName()) ? 1.0f : 0.0f;
}

float VezaxControlMovementMultiplier::GetValue(Action* action)
{
    if (!action || !VezaxFormationActive(botAI))
        return 1.0f;

    // Only the roles the formation actually places. Melee ride the boss and keep every generic mover.
    // The main tank is not one of them any more: it holds the anchor slot, and FollowAction would
    // drag it off the spawn point that every other radius here is measured from.
    if (!botAI->IsRanged(bot) && !botAI->IsMainTank(bot))
        return 1.0f;

    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero would also kill targeting;
    // ReachTargetAction is what keeps the tank in melee and walks a healer into range of someone the
    // formation cannot reach.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {
        "vezax raid position action",       "vezax mark of the faceless action",
        "vezax vapor puddle clear action",  "vezax shadow crash dodge action",
        "vezax shadow crash soak action",   "vezax vapor soak action"};

    return encounterMovers.count(action->getName()) ? 1.0f : 0.0f;
}

namespace
{

// Every taunt the tank specs wire up. Matched by name because they live across four class headers, and
// righteous defense has to be in the list: it is the alternative Hand of Reckoning falls back to on
// cooldown, so leaving it out lets half of them through.
bool IsHodirTauntAction(std::string const& name)
{
    return name == "taunt" || name == "hand of reckoning" || name == "righteous defense" ||
           name == "dark command" || name == "growl" || name == "challenging shout" ||
           name == "challenging roar";
}

}  // namespace

float HodirGuardMultiplier::GetValue(Action* action)
{
    // Engaged, not merely present: these stand-downs hand generic behaviour to encounter nodes that
    // none of them run before the pull, so on sight alone they would leave bots rooted with no target.
    if (!action || !IsHodirEngaged(botAI))
        return 1.0f;

    // The class "lose aggro" node taunts on the 8s cooldown for as long as somebody else holds the
    // boss, and it knows nothing about Frozen Blows. Name first: this runs for every action in the
    // queue and the predicate behind it walks the boss lookup.
    if (IsHodirTauntAction(action->getName()) && HodirTauntWouldBeSuicide(botAI, bot))
        return 0.0f;

    // hodir set dps priority action owns the target for everyone who has one. Healers are not on
    // that node at all - they keep healing while the raid takes 14000 every two seconds - so they
    // keep the generic picker too. "attack rti target" is deliberately left alone: bots set no marks
    // here, but a mark the player sets should still win.
    //
    // Both pickers, because the encounter node now hands tanks the boss as well. Suppressing it
    // unconditionally is safe here for the reason docs/raids/README.md sets out: the ladder ends in a
    // terminal fallback, so no role is left with no target.
    if (!botAI->IsHeal(bot) &&
        (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action)))
        return 0.0f;

    // Everyone, every role, for the whole 9s Flash Freeze cast: the shelter run is the only thing that
    // matters and only the dodge may interrupt it. Nothing else gets to touch the bot's feet.
    //
    // The shelter action cannot defend itself. MoveTo answers Duplicate for the destination it already
    // issued, so from the second tick of a run its Execute returns false and the engine descends past
    // ACTION_RAID + 6 to whatever is below - which then takes the movement slot at equal priority and
    // clears the MotionMaster. Measured: 68 of 125 bot-freeze pairs had their last accepted move
    // before the freeze come from something else, 36 of them the ring anchor and 21 reach melee, and
    // bots that had already reached the shelter were walked 18-22 yd back out of it.
    //
    // Returning true from the shelter action instead would silence its casting for six seconds, seven
    // times a pull. The descent is fine; the movers below it are what has to be off.
    if (IsHodirFlashFreezeIncoming(botAI))
    {
        // Charge, Intercept and both Feral Charges, the only subclasses - a warrior charging back to
        // the boss mid-run is how the one melee catch in 603_3_hodir_1787590072 happened.
        if (dynamic_cast<CastReachTargetSpellAction*>(action))
            return 0.0f;

        if (!dynamic_cast<MovementAction*>(action))
            return 1.0f;

        // AttackAction only sets a target, so it stays exempt. ReachTargetAction does not: "reach
        // melee" and "reach spell" are 25 of those 68 thefts.
        if (dynamic_cast<AttackAction*>(action))
            return 1.0f;

        static std::set<std::string> const freezeMovers = {"hodir move snowpacked icicle",
                                                           "hodir icicle dodge action"};

        return freezeMovers.count(action->getName()) ? 1.0f : 0.0f;
    }

    // Only the two tanks and the ranged half stand on a spot. Melee ride the boss in the corner, so
    // they keep every generic mover - SetBehindTargetAction in particular.
    if (!botAI->IsMainTank(bot) && !botAI->IsAssistTankOfIndex(bot, 0, true) && !botAI->IsRanged(bot))
        return 1.0f;

    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero would also kill targeting;
    // ReachTargetAction is what walks a healer into range of someone the ring cannot reach.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {
        "hodir raid position action", "hodir move snowpacked icicle", "hodir icicle dodge action",
        "hodir biting cold shed", "hodir spread storm cloud"};

    return encounterMovers.count(action->getName()) ? 1.0f : 0.0f;
}

float HodirPaladinAuraMultiplier::GetValue(Action* action)
{
    if (!action || bot->getClass() != CLASS_PALADIN)
        return 1.0f;

    static std::set<std::string> const competingAuras = {
        "devotion aura", "retribution aura", "concentration aura", "crusader aura",
        "sanctity aura", "shadow resistance aura", "fire resistance aura"};

    // Name first: this runs for every action in the queue, and the paladin lookup walks the group.
    if (!competingAuras.count(action->getName()))
        return 1.0f;

    if (!IsHodirEngaged(botAI) || GetHodirResistancePaladin(botAI, bot) != bot)
        return 1.0f;

    return 0.0f;
}

// Both of these run behind the base class's shaman and totem-action checks, so the encounter lookup
// only happens for the handful of actions that could take the earth slot.
bool AuriayaAntiFearTotemGuardMultiplier::FearWindowActive() { return AuriayaFearWindowActive(botAI); }

bool YoggSaronAntiFearTotemGuardMultiplier::FearWindowActive() { return YoggSaronFearWindowActive(botAI); }

namespace
{
// The Mimiron windows where taking the hit ends the bot: Shock Blast is 100000 in a 15 yd circle, the
// Laser Barrage cone one-shots, a Rocket Strike lands on the ranged ring, and Firefighter's fire and
// Frost Bomb both tick people down. Proximity Mines and Bomb Bots are deliberately absent - the mine
// node was demoted below the whole ladder because eating one is healable, and letting it veto a charge
// would contradict its own ranking.
bool MimironLethalWindowActive(PlayerbotAI* botAI)
{
    if (!GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
        !GetFirstAliveUnitByEntry(botAI, NPC_VX001) &&
        !GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return false;

    MimironShockBlastTrigger shockBlast(botAI);
    if (shockBlast.IsActive())
        return true;

    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (barrage.IsActive())
        return true;

    MimironRocketStrikeTrigger rocketStrike(botAI);
    if (rocketStrike.IsActive())
        return true;

    // Both of these check the hard-mode config first, so they cost nothing on a normal clear.
    MimironDodgeFlamesTrigger dodgeFlames(botAI);
    if (dodgeFlames.IsActive())
        return true;

    MimironFrostBombTrigger frostBomb(botAI);
    return frostBomb.IsActive();
}
}  // namespace

float MimironChargeGuardMultiplier::GetValue(Action* action)
{
    // Cheap gate first: this only ever has an opinion about the gap-closers, and every one of them is a
    // CastReachTargetSpellAction - Charge, Intercept and both Feral Charges, with no other subclasses.
    if (!dynamic_cast<CastReachTargetSpellAction*>(action))
        return 1.0f;

    return MimironLethalWindowActive(botAI) ? 0.0f : 1.0f;
}

float MimironThreatRedirectGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Every BuffOnMainTankAction names itself "<spell> on main tank". Matching the two spells rather
    // than the type on purpose: the type would also catch every blessing and buff aimed at the tank.
    std::string const name = action->getName();
    if (name != "misdirection on main tank" && name != "tricks of the trade on main tank")
        return 1.0f;

    // Phase 1 only, where the two tanks trade Plasma Blast every 22 s. A taunt equalises threat for one
    // moment; twenty seconds of a hunter's and a rogue's redirected threat undoes that long before the
    // next cast, so the swap never sticks while these run. Nothing to swap in the later phases, so the
    // redirects stay useful there.
    return GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
                   !GetFirstAliveUnitByEntry(botAI, NPC_VX001) &&
                   !GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT)
               ? 0.0f
               : 1.0f;
}

float MimironTargetGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Engaged, not merely present: the stand-down hands targeting to a node that does not run before
    // the pull, so on sight alone it would leave every non-tank with no picker at all.
    if (!IsMimironEngaged(botAI))
        return 1.0f;

    // "mimiron set dps priority" owns every non-tank's target. "attack rti target" is deliberately
    // left alone: bots no longer set marks for each other, but a mark a player sets should still win.
    if (!botAI->IsTank(bot) && dynamic_cast<DpsAssistAction*>(action))
        return 0.0f;

    return 1.0f;
}

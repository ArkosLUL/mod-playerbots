#include "UldMultipliers.h"

#include <set>
#include <string>

#include "BurstCooldowns.h"
#include "ChooseTargetActions.h"
#include "FollowActions.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PriestActions.h"
#include "RaidBossHelpers.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "Timer.h"
#include "UldBossHelper.h"
#include "UldEncounter_IronAssembly.h"
#include "UldEncounter_Vezax.h"
#include "UldHardMode.h"
#include "UldActions.h"
#include "UldTriggers.h"
#include "UldScripts.h"
#include "VehicleActions.h"

// Algalon the Observer
// Reserve Dispersion for the designated Big Bang soaker priest. Big Bang is unavoidable raid-wide
// damage; the soaker survives it via Dispersion (90% reduction). Blocking the normal low-mana /
// critical-health Dispersion casts keeps the cooldown up for every Big Bang.
float AlgalonMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CastDispersionAction*>(action))
        return 1.0f;

    Unit* boss = AI_VALUE2(Unit*, "find target", "algalon observer");
    if (!boss || !boss->IsAlive())
        return 1.0f;

    // Only the designated soaker priest reserves the cooldown; other priests disperse normally.
    if (GetAlgalonBigBangSoakerPriest(bot) != bot)
        return 1.0f;

    // During the actual Big Bang cast the soak action must be free to spend Dispersion.
    if (boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_ALGALON_BIG_BANG))
        return 1.0f;

    // Otherwise block Dispersion so it is available for the next Big Bang.
    return 0.0f;
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

    // The class-generic redirects buff whoever the group flags as main tank, which is the wrong sink
    // while a Pummeller is out; xt002 redirect threat action picks the tank that actually needs it.
    if (dynamic_cast<CastMisdirectionOnMainTankAction*>(action) ||
        dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action))
    {
        return 0.0f;
    }

    // xt002 set dps priority action owns every non-tank's target, so the generic picker stands down
    // rather than pulling bots back onto whatever is nearest. "attack rti target" is deliberately left
    // alone: bots no longer set marks here, but a mark the player sets should still win.
    if (!botAI->IsTank(bot) && dynamic_cast<DpsAssistAction*>(action))
        return 0.0f;

    // Ranged DPS are the only role anchored to a fixed spot, and without this the generic movers walk
    // them straight back off it - the anchor then re-fires next tick and the bot paces all fight.
    // Healers and melee keep every generic mover, since neither is anchored.
    if (botAI->IsRangedDps(bot) && dynamic_cast<MovementAction*>(action) &&
        !bot->HasAura(GetXT002SearingLightSpellId(bot)) && !bot->HasAura(GetXT002GravityBombSpellId(bot)))
    {
        static std::set<std::string> const encounterMovers = {
            "xt002 raid position action",        "xt002 searing light carrier action",
            "xt002 gravity bomb carrier action", "xt002 searing light spread action",
            "xt002 gravity bomb spread action",  "xt002 boombot avoid action",
            "xt002 void zone action"};

        if (!encounterMovers.count(action->getName()))
            return 0.0f;
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

// Ignis the Furnace Master
float IgnisMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Slag Pot is a vehicle ride: the victim is held in place for the full duration, so movement
    // orders only fight the ride and leave the bot facing the wrong way when it drops.
    bool const slagPotRide = IsIgnisSlagPotVictim(bot) && dynamic_cast<MovementAction*>(action);

    // The construct tank is deliberately parked on a Scorched Ground patch - that is what stacks Heat
    // on the construct - so the generic dodge would undo the kite every tick.
    bool const parkedTank = action->getName() == "ignis scorched ground action" &&
                            GetIgnisConstructTank(botAI, bot) == bot;

    // GetIgnis walks the grid, and this runs against every action of every bot on the tick, so it is
    // asked last - only once something Ignis-specific would actually be blocked.
    if (!slagPotRide && !parkedTank)
        return 1.0f;

    return GetIgnis(botAI) ? 0.0f : 1.0f;
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
    // until the P2 transition, so P1 would otherwise fall through ungated.
    if (bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true) ||
        bot->FindNearestCreature(NPC_SARA_PHASE_1, 200.0f, true))
    {
        bool const phaseThree = YoggSaronInPhase3(botAI);

        return {phaseThree || YoggSaronInPhase2(botAI), phaseThree};
    }

    return {};
}

float AuriayaMovementGuardMultiplier::GetValue(Action* action)
{
    if (!action || !GetAuriaya(botAI))
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

    // Only the roles the arc actually places. The tank holds the boss and melee ride it, so both keep
    // every generic mover.
    if (!botAI->IsRanged(bot) || botAI->IsMainTank(bot))
        return 1.0f;

    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero would also kill targeting;
    // ReachTargetAction is what walks a healer into range of someone the arc cannot reach.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {
        "vezax raid position action",       "vezax mark of the faceless action",
        "vezax vapor puddle clear action",  "vezax shadow crash clear action",
        "vezax shadow crash soak action",   "vezax vapor soak action"};

    return encounterMovers.count(action->getName()) ? 1.0f : 0.0f;
}

float HodirGuardMultiplier::GetValue(Action* action)
{
    if (!action || !GetHodir(botAI))
        return 1.0f;

    // hodir set dps priority action owns the target for everyone who has one. Healers are not on
    // that node at all - they keep healing while the raid takes 14000 every two seconds - so they
    // keep the generic picker too. "attack rti target" is deliberately left alone: bots set no marks
    // here, but a mark the player sets should still win.
    if (!botAI->IsTank(bot) && !botAI->IsHeal(bot) && dynamic_cast<DpsAssistAction*>(action))
        return 0.0f;

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
        "hodir biting cold jump", "hodir spread storm cloud"};

    return encounterMovers.count(action->getName()) ? 1.0f : 0.0f;
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

    if (!GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
        !GetFirstAliveUnitByEntry(botAI, NPC_VX001) &&
        !GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return 1.0f;

    // "mimiron set dps priority" owns every non-tank's target. "attack rti target" is deliberately
    // left alone: bots no longer set marks for each other, but a mark a player sets should still win.
    if (!botAI->IsTank(bot) && dynamic_cast<DpsAssistAction*>(action))
        return 0.0f;

    return 1.0f;
}

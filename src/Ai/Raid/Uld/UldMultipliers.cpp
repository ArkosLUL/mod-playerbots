#include "UldMultipliers.h"

#include <set>
#include <string>

#include "BurstCooldowns.h"
#include "FollowActions.h"
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
#include "UldHardMode.h"
#include "UldActions.h"
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

    if (IsXT002HardModeActive(botAI))
        return 1.0f;

    // Normal mode safety floor. The attack-heart trigger goes false here too, but that only stops
    // bots from picking the Heart up again - this is what stops the ones already swinging at it from
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

    static std::set<std::string> const retargets = {"attack rti target", "xt002 mark kill target action",
                                                    "xt002 boombot ranged kill action", "xt002 pummeller taunt action",
                                                    "xt002 redirect threat action"};

    return retargets.count(action->getName()) ? 1.0f : 0.0f;
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

float UldThreatRedirectMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CastMisdirectionOnMainTankAction*>(action) &&
        !dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action))
    {
        return 1.0f;
    }

    static uint32 const noRedirectBosses[] = {
        // One tank per council member, and Fusion Punch then forces a swap
        NPC_STEELBREAKER, NPC_MOLGEIM, NPC_BRUNDIR,
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

    std::string const& name = action->getName();
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

    // Attuned to Nature is up for the whole wave phase and reduces her damage taken; the core drops
    // it when it enters the final phase (boss_freya.cpp).
    if (freya)
    {
        return {true, !freya->HasAura(SPELL_ATTUNED_TO_NATURE) || freya->GetHealthPct() <= FREYA_LUST_FALLBACK_PCT};
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

// Both of these run behind the base class's shaman and totem-action checks, so the encounter lookup
// only happens for the handful of actions that could take the earth slot.
bool AuriayaAntiFearTotemGuardMultiplier::FearWindowActive() { return AuriayaFearWindowActive(botAI); }

bool YoggSaronAntiFearTotemGuardMultiplier::FearWindowActive() { return YoggSaronFearWindowActive(botAI); }

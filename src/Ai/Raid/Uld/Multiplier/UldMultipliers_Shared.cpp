/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Shared.h"

#include "AttackAction.h"
#include "BurstCooldowns.h"
#include "ChooseTargetActions.h"
#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldEncounter_Algalon.h"
#include "UldEncounter_Freya.h"
#include "UldEncounter_Mimiron.h"
#include "UldEncounter_Razorscale.h"
#include "UldEncounter_Thorim.h"
#include "UldEncounter_YoggSaron.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

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

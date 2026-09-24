/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "BossResistanceMultipliers.h"
#include "PlayerbotAI.h"
#include "Strategy.h"
#include "UldActions_Freya.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Freya.h"
#include "UldMultipliers_Freya.h"
#include "UldTriggers_Freya.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool FreyaAlive(PlayerbotAI* botAI)
{
    Unit* freya = GetFreyaScan(botAI).Boss();
    return freya && freya->IsAlive();
}

bool FreyaOwnsTargets(PlayerbotAI* botAI) { return botAI->GetState() == BOT_STATE_COMBAT && FreyaAlive(botAI); }

void DefineFreya(EncounterBuilder& e)
{
    // Survival first, then the pacify counter, then targeting. A bot that is dead, blown up or
    // silenced contributes nothing to the trio wave it is being steered at.
    e.Node<FreyaNearNatureBombTrigger, FreyaMoveAwayNatureBombAction>(ACTION_RAID + 4);

    // The main tank answers a bomb by moving Freya, not itself. Bombs land at players' feet and the
    // melee stack is on the boss, so a volley buries her melee ring and the rest of the melee lose it.
    e.Node<FreyaTankNatureBombTrigger, FreyaTankNatureBombAction>(ACTION_RAID + 4);

    // The two 8 yd circles, and the whole reason the raid has to be able to come apart: Nature's Fury
    // fires five of them around one bot, Sunbeam drops one wherever its target is standing when the cast
    // ends. Both sit with the other escapes, above the spore node - which would otherwise walk the
    // carrier straight back into the ball it just left.
    e.Node<FreyaNaturesFuryBailTrigger, FreyaNaturesFuryBailAction>(ACTION_RAID + 4);
    e.Node<FreyaStepOutOfSunbeamTrigger, FreyaStepOutOfSunbeamAction>(ACTION_RAID + 4);

    // Detonating Lasher wave. Order is the doctrine: nothing here can be tanked, kited or outrun, so
    // the raid answers the wave with crowd control and a camp it can AoE. Leave the blast of anything
    // about to go off first - a bot that is dead does no crowd control - then root what has already
    // closed, then snare what has not, then the ghouls, which are the one thing on the encounter that
    // takes a lasher off a bot at all - and gather last, only when nothing urgent is asking.
    e.Node<FreyaLasherAboutToBlowTrigger, FreyaLasherAboutToBlowAction>(ACTION_RAID + 3);
    e.Node<FreyaFrostNovaLashersTrigger, FreyaFrostNovaLashersAction>(ACTION_RAID + 2);
    e.Node<FreyaTrapLashersTrigger, FreyaTrapLashersAction>(ACTION_RAID + 2);
    e.Node<FreyaSummonArmyTrigger, FreyaSummonArmyAction>(ACTION_RAID + 2);
    e.Node<FreyaRangedCampTrigger, FreyaRangedCampAction>(ACTION_RAID);

    // Conservator's Grip is raid-wide and cannot be outranged, so a spore outranks attacking: a
    // pacified bot cannot swing at anything anyway.
    e.Node<FreyaMoveToHealingSporeTrigger, FreyaMoveToHealingSporeAction>(ACTION_RAID + 2);

    e.Node<FreyaTankAddsTrigger, FreyaTankAddsAction>(ACTION_RAID + 1);

    // Freya walks after whoever holds her, so every escape the tank takes moves her and nothing used to
    // move her back. Under the bomb escape, which has to be able to leave the leash.
    e.Node<FreyaTankHoldFreyaTrigger, FreyaTankHoldFreyaAction>(ACTION_RAID + 1);

    e.Node<FreyaRedirectThreatTrigger, FreyaRedirectThreatAction>(ACTION_RAID + 1);
    e.Node<FreyaSetDpsPriorityTrigger, FreyaSetDpsPriorityAction>(ACTION_RAID);
    e.Node(
        "freya nature resistance trigger",
        [](PlayerbotAI* ai) -> Trigger* { return new BossNatureResistanceTrigger(ai, "freya"); },
        "freya nature resistance action",
        [](PlayerbotAI* ai) -> Action* { return new BossNatureResistanceAction(ai, "freya"); }, ACTION_RAID);
    e.Node(
        "freya fire resistance trigger",
        [](PlayerbotAI* ai) -> Trigger* { return new BossFireResistanceTrigger(ai, "freya"); },
        "freya fire resistance action",
        [](PlayerbotAI* ai) -> Action* { return new BossFireResistanceAction(ai, "freya"); }, ACTION_RAID);

    // Hard mode (config-gated): break out of Iron Roots and dodge the Unstable Sun Beam. Breaking the
    // root outranks the dodge - a rooted bot can't move, so it has to free itself before it can step out.
    e.Node<FreyaBreakIronRootsTrigger, FreyaBreakIronRootsAction>(ACTION_RAID + 5);
    e.Node<FreyaDodgeUnstableSunBeamTrigger, FreyaDodgeUnstableSunBeamAction>(ACTION_RAID + 4);
    e.Node<FreyaGroundTremorHoldCastTrigger, FreyaGroundTremorHoldCastAction>(ACTION_EMERGENCY + 2);

    // The set dps priority row splits the trio wave three ways, and the generic picker would reclaim
    // those targets on alternating ticks. Every tank is fenced off tank assist for the whole encounter:
    // gating it on the add ladder having something let generic assist through on a pure Detonating
    // Lasher wave, where the off-tank collected the wave and walked it into the raid stack.
    e.OwnTargeting("freya disable automatic targeting", Role::Dps, FreyaOwnsTargets, Family::DpsAssist);
    e.OwnTargeting("freya disable automatic targeting", Role::Tank, FreyaOwnsTargets, Family::TankAssist);

    e.Multiplier<FreyaTrioSyncMultiplier>(Family::Melee | Family::Spell);

    // Whether an action is AoE is its threat type, which any action can override.
    e.Multiplier<FreyaLasherFinishAoeMultiplier>(Family::AnyAction);

    e.Multiplier<FreyaLasherTrapReserveMultiplier>(Family::Spell);

    // Every hazard here has a node that reads all of them at once: Nature Bomb, the Unstable Sun Beam
    // and the lashers about to detonate. Avoid AoE sits at ACTION_EMERGENCY, outranks all three, and
    // swaps their answer for a flat FleeDistance hop on a bearing of its own, which clears none of them.
    e.Block("freya avoid aoe hold", Role::Any, FreyaAlive, Family::AvoidAoe);

    e.Multiplier<FreyaGroundTremorCastGateMultiplier>(Family::Spell);
    e.Multiplier<BossNatureAspectHoldMultiplier>(Family::Spell, "freya");
}
}  // namespace

EncounterDefinition const& UldFreyaDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_FREYA, BossStateGate, "freya", &DefineFreya);
    return definition;
}

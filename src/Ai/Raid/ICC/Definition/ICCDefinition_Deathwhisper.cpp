/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Deathwhisper.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Deathwhisper.h"
#include "ICCMultipliers_Deathwhisper.h"
#include "ICCSeams.h"
#include "ICCTriggers_Deathwhisper.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool LdwFightLive(PlayerbotAI* botAI) { return LdwBoss(botAI) != nullptr; }

bool LdwDodging(PlayerbotAI* botAI) { return LdwDodgeKind(botAI) != LdwHazardKind::None; }

void DefineDeathwhisper(EncounterBuilder& e)
{
    // A shade's blast kills outright; the other hazards hurt. One dodge clears them all, so no two forced
    // walks fight. Dark Reckoning is trash, never up while she is.
    e.Node<IccLadyDeathwhisperDodgeTrigger, IccLadyDeathwhisperDodgeAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccDarkReckoningTrigger, IccDarkReckoningAction>(ACTION_EMERGENCY + 5, EncounterRow::Mover);
    e.Node<IccLadyDeathwhisperFrostboltTrigger, IccLadyDeathwhisperFrostboltAction>(ACTION_RAID + 5);
    e.Node<IccLadyDeathwhisperDominateMindTrigger, IccLadyDeathwhisperDominateMindAction>(ACTION_RAID + 4);
    e.Node<IccLadyDeathwhisperTauntTrigger, IccLadyDeathwhisperTauntAction>(ACTION_RAID + 3);
    e.Node<IccLadyDeathwhisperRedirectTrigger, IccLadyDeathwhisperRedirectAction>(ACTION_RAID + 2);
    e.Node<IccLadyDeathwhisperTargetTrigger, IccLadyDeathwhisperTargetAction>(ACTION_RAID + 1);

    e.LiveMultiplier<IccLadyDeathwhisperMultiplier>(Family::Spell);

    e.Exclusive("lady deathwhisper dodge", Role::Any, &LdwDodging, 0, {IccLadyDeathwhisperDodgeAction::Name});
    e.Exclusive("lady deathwhisper dark reckoning", Role::Any, &LdwDarkReckoningDodging, 0,
                {IccDarkReckoningAction::Name});
    // Just past a hazard the generic movers would walk the bot back in.
    e.Block("lady deathwhisper hazard band", Role::Any, &LdwNearHazard,
            Family::Reach | Family::ReachHeal | Family::Follow | Family::CombatFormationMove | Family::Charge);
    // The generic dodge steps 5 yd at most and its flees lock out the encounter's own.
    e.Block("lady deathwhisper avoid aoe", Role::Any, &LdwFightLive, Family::AvoidAoe);
    // She carries taunt diminishing returns, so only the swap rule taunts her, and the tanks' own taunt
    // takes the adds.
    e.Block("lady deathwhisper taunts", Role::Tank, &LdwFightLive, Family::Taunt);
    e.OwnTargeting("lady deathwhisper targeting", Role::Any, &IccResolverPicks<&LdwTarget>);

    e.Tick(&LdwTick);
}
}  // namespace

EncounterDefinition const& IccDeathwhisperDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_LADY_DEATHWHISPER, BossStateGate, "lady-deathwhisper",
                                                &DefineDeathwhisper);
    return definition;
}

/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "Strategy.h"
#include "UldActions_Auriaya.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Auriaya.h"
#include "UldMultipliers_Auriaya.h"
#include "UldTriggers_Auriaya.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineAuriaya(EncounterBuilder& e)
{
    // Sonic Screech is deliberately soaked, not dodged: it shares its damage across everyone in the
    // cone, so there is no dodge node here and the anchors exist to put the raid in the arc. Position
    // sits at the bottom because the engine stops at the first action that succeeds - killing a sentry
    // beats standing on a spot, and the anchor tolerances make the drift cheap.
    e.Node<AuriayaFallFromFloorTrigger, AuriayaFallFromFloorAction>(ACTION_RAID + 4);

    // A loose Sanctum Sentry is the worst state the fight has: it buffs Auriaya while it lives and
    // pounces anything 8-25 yd away, which holding it in melee prevents outright. It outranks the pool
    // dodge, which is one step and can wait a tick.
    e.Node<AuriayaSentryTauntTrigger, AuriayaSentryTauntAction>(ACTION_RAID + 3);

    e.Node<AuriayaSeepingEssenceTrigger, AuriayaSeepingEssenceAction>(ACTION_RAID + 2, EncounterRow::Mover);
    e.Node<AuriayaAntiFearTrigger, AuriayaAntiFearAction>(ACTION_RAID + 2);
    e.Node<AuriayaSetDpsPriorityTrigger, AuriayaSetDpsPriorityAction>(ACTION_RAID + 1);
    e.Node<AuriayaRaidPositionTrigger, AuriayaRaidPositionAction>(ACTION_RAID, EncounterRow::Mover);

    // Engaged, not merely present: the nodes these hand generic behaviour to don't run before the pull,
    // so on sight alone they'd leave bots rooted with no target.
    //
    // The set dps priority row owns every non-tank's target. "attack rti target" is left alone on
    // purpose: bots don't set marks here, but a mark the player sets should still win.
    e.OwnTargeting("auriaya movement guard multiplier", Role::NonTank, IsAuriayaEngaged, Family::DpsAssist);

    // Only the main tank and the ranged half stand on a spot. The off-tank chases Sanctum Sentries and
    // melee ride the boss, so both keep every generic mover, SetBehindTargetAction included. Without
    // this the anchor oscillates: the bot reaches its spot, the trigger goes quiet, a generic mover
    // walks it off and the trigger fires again. ReachHeal walks a healer into range of someone the
    // anchor can't reach.
    e.OwnMovement("auriaya movement guard multiplier", Role::MainTank | Role::Ranged, IsAuriayaEngaged,
                  Family::Attack | Family::Reach | Family::ReachHeal);

    e.Multiplier<AuriayaAntiFearTotemGuardMultiplier>(Family::Spell);
}
}  // namespace

EncounterDefinition const& UldAuriayaDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_AURIAYA, BossStateGate, "auriaya", &DefineAuriaya);
    return definition;
}

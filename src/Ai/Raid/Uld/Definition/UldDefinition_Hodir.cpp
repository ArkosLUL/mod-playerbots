/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Strategy.h"
#include "UldActions_Hodir.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Hodir.h"
#include "UldTriggers_Hodir.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
// Every guard below waits for the pull: the nodes they hand generic behaviour to don't run before it,
// so on sight alone they'd leave bots rooted with no target.

bool HodirTauntSuicide(PlayerbotAI* botAI)
{
    return IsHodirEngaged(botAI) && HodirTauntWouldBeSuicide(botAI, botAI->GetBot());
}

bool HodirOwnsTargets(PlayerbotAI* botAI) { return IsHodirEngaged(botAI) && !botAI->IsHeal(botAI->GetBot()); }

bool HodirFreezeRun(PlayerbotAI* botAI) { return IsHodirEngaged(botAI) && IsHodirFlashFreezeIncoming(botAI); }

bool HodirFreezeRunInShelter(PlayerbotAI* botAI)
{
    return HodirFreezeRun(botAI) && IsHodirInLandedShelter(botAI, botAI->GetBot());
}

bool HodirFreezeRunOutsideShelter(PlayerbotAI* botAI)
{
    return HodirFreezeRun(botAI) && !IsHodirInLandedShelter(botAI, botAI->GetBot());
}

// Any target, not just Hodir: ranged walking to helper ice blocks made 50 such walks in one pull. The
// icicle sweep is the expensive part, so it goes last.
bool HodirIcicleInPath(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsHodirEngaged(botAI) || botAI->IsTank(bot) || botAI->IsHeal(bot))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && IsHodirWalkThroughLiveIcicle(bot, target->GetPosition(),
                                                  ULDUAR_HODIR_ICE_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN,
                                                  ULDUAR_HODIR_BIG_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN);
}

// Only the two tanks and the ranged half stand on a spot. Melee ride the boss, so they keep every
// generic mover, SetBehindTargetAction in particular.
bool HodirAnchored(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return IsHodirEngaged(botAI) &&
           (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0, true) || botAI->IsRanged(bot));
}

bool HodirResistancePaladin(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return bot->getClass() == CLASS_PALADIN && IsHodirEngaged(botAI) && GetHodirResistancePaladin(botAI, bot) == bot;
}

void DefineHodir(EncounterBuilder& e)
{
    // The engine stops at the first action that succeeds, so this order is a survival ranking, not a
    // preference. Sheltering is the only node whose failure is an outright death - Flash Freeze
    // encases everyone without the Safe Area aura, and a second one instakills whoever is still
    // trapped. Dodging is next because an icicle lands every 2s for 14000. Targeting sits above the
    // Biting Cold shed, which moves for seconds at a time and would otherwise starve it; that costs
    // nothing, because the targeting action returns false whenever the current target is already
    // right. Position is last on purpose: killing the block that is about to get an ally killed beats
    // standing on a dot.
    e.Node<HodirNearSnowpackedIcicleTrigger, HodirMoveSnowpackedIcicleAction>(ACTION_RAID + 6, EncounterRow::Mover);
    e.Node<HodirIcicleDodgeTrigger, HodirIcicleDodgeAction>(ACTION_RAID + 5, EncounterRow::Mover);
    e.Node<HodirFrozenBlowsSwapTrigger, HodirFrozenBlowsSwapAction>(ACTION_RAID + 4);

    // Shares the swap's relevance because the two can never contend: the swap only fires for the two
    // tanks and the redirect only for hunters and rogues. It has to outrank targeting, or a bot
    // switches to a block before spending charges the tank is waiting on.
    e.Node<HodirRedirectThreatTrigger, HodirRedirectThreatAction>(ACTION_RAID + 4);

    e.Node<HodirSetDpsPriorityTrigger, HodirSetDpsPriorityAction>(ACTION_RAID + 3);
    e.Node<HodirSpreadStormCloudTrigger, HodirSpreadStormCloudAction>(ACTION_RAID + 2, EncounterRow::Mover);
    e.Node<HodirCollectStormPowerTrigger, HodirCollectStormPowerAction>(ACTION_RAID + 2, EncounterRow::Mover);
    e.Node<HodirBitingColdTrigger, HodirBitingColdShedAction>(ACTION_RAID + 1, EncounterRow::Mover);
    e.Node<HodirFrostResistanceTrigger, HodirFrostResistanceAction>(ACTION_RAID);
    e.Node<HodirRaidPositionTrigger, HodirRaidPositionAction>(ACTION_RAID, EncounterRow::Mover);

    // Without the targeting rule DpsTargetValue is never null, so the generic node retakes the target
    // every other tick and bots drift off the ice block that is about to get an ally killed. Without
    // the movement rules the anchor oscillates.

    // The class "lose aggro" node taunts on the 8s cooldown for as long as somebody else holds the
    // boss, and it knows nothing about Frozen Blows.
    e.Block("hodir guard multiplier", Role::Any, HodirTauntSuicide, Family::Taunt);

    // The set dps priority row owns the target for everyone who has one, tanks included. Healers aren't
    // on it - they keep healing while the raid takes 14000 every two seconds - so they keep the generic
    // picker. "attack rti target" is left alone on purpose: bots set no marks here, but a mark the
    // player sets should still win.
    e.OwnTargeting("hodir guard multiplier", Role::Any, HodirOwnsTargets);

    // Everyone, every role, for the whole 9s Flash Freeze cast: the shelter run is the only thing that
    // matters and only the dodge may interrupt it. The shelter action can't defend itself: MoveTo
    // answers Duplicate for the destination it already issued, so from the second tick of a run its
    // Execute returns false and the engine descends to whatever is below, which then clears the
    // MotionMaster. Measured: 68 of 125 bot-freeze pairs had their last move before the freeze come
    // from something else, and bots already in a shelter were walked 18-22 yd back out.
    //
    // Charge, Blink and Disengage throw the bot and aren't MovementActions. A warrior charging back to
    // the boss mid-run and a Disengage 12 yd out of a shelter 2.5s before the landing both happened.
    e.Block("hodir guard multiplier", Role::Any, HodirFreezeRun, Family::Charge | Family::Blink | Family::Disengage);

    // Reach is not kept here: "reach melee" and "reach spell" were 25 of those 68. The shed only runs
    // inside a landed shelter, where its legs stay in the park ring.
    e.OwnMovement("hodir guard multiplier", Role::Any, HodirFreezeRunOutsideShelter, Family::Attack,
                  {HodirMoveSnowpackedIcicleAction::Name, HodirIcicleDodgeAction::Name});
    e.OwnMovement("hodir guard multiplier", Role::Any, HodirFreezeRunInShelter, Family::Attack,
                  {HodirMoveSnowpackedIcicleAction::Name, HodirIcicleDodgeAction::Name,
                   HodirBitingColdShedAction::Name});

    // Don't walk back through a pool the dodge just stepped the bot out of. Of the reach melee and set
    // behind moves right after a dodge, 25-52% ended within 4.5 yd of an icicle still due to blow, and
    // dodge <-> reach melee was the top flip at 70-77 a pull. Held only until the icicle goes off, 3.3s
    // at most.
    e.Block("hodir guard multiplier", Role::Any, HodirIcicleInPath,
            Family::Reach | Family::ReachHeal | Family::SetBehind);

    // ReachHeal walks a healer into range of someone standing out of it.
    e.OwnMovement("hodir guard multiplier", Role::Any, HodirAnchored,
                  Family::Attack | Family::Reach | Family::ReachHeal);

    // Holds the paladin aura slot open for Frost Resistance. The slot is exclusive, so the paladin's own
    // buff strategy would re-cast Retribution or Devotion on the next GCD and the two would trade it all
    // fight. Only the chosen paladin is held.
    e.Block("hodir paladin aura multiplier", Role::Any, HodirResistancePaladin, 0,
            {"devotion aura", "retribution aura", "concentration aura", "crusader aura", "sanctity aura",
             "shadow resistance aura", "fire resistance aura"});
}
}  // namespace

EncounterDefinition const& UldHodirDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_HODIR, BossStateGate, "hodir", &DefineHodir);
    return definition;
}

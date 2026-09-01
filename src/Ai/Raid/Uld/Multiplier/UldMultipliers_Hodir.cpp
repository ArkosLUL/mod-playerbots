/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Hodir.h"

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
#include "Playerbots.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldEncounter_Hodir.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

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

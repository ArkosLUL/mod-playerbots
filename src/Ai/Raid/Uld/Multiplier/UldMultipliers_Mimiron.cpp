/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Mimiron.h"

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
#include "RaidTankDefensive.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldEncounter_Mimiron.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

float MimironTankAnchorGuardMultiplier::GetValue(Action* action)
{
    if (!action || !PlayerbotAI::IsMainTank(bot))
        return 1.0f;

    // "reach melee" by name: "reach spell" and "reach party member to heal" walk ranged and healers
    // into range and must not be touched, and the gap-closers are the charge guard's business.
    bool const reachMelee = action->getName() == "reach melee";
    if (!dynamic_cast<TankFaceAction*>(action) && !reachMelee)
        return 1.0f;

    // Phase 3, either mode. The unit hovers out of reach and holds 30 yd from whoever it is on, so
    // walking toward it only drags it and the fight off the centre spot the tank is given, and the
    // ranged wedge with it.
    if (reachMelee && IsMimironAcuAirborne(botAI, bot))
        return 0.0f;

    if (!IsMimironHardModeActive(botAI) || !MimironPhase1Active(botAI))
        return 1.0f;

    Unit* leviathanMkII = GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    return leviathanMkII && leviathanMkII->GetVictim() == bot &&
                   IsMimironTankDragReady(botAI, bot)
               ? 0.0f
               : 1.0f;
}

namespace
{
// Whether this action actually splashes. The threat type does not say: CastHealingSpellAction
// declares Aoe, while whirlwind, divine storm, consecration, death and decay, starfall, swipe and
// pestilence declare something else and hit everything nearby anyway - all of them ran in a
// Firefighter phase 3 with the guard up. The spell says instead: IsTargetingArea covers the area
// and cone selection categories, IsAffectingArea adds the persistent area auras.
bool MimironActionSplashes(PlayerbotAI* botAI, Action* action)
{
    // Totem and trap summons stay invisible to that test - the summon is single-target and the area
    // lives on what it summons - so they are named.
    static std::set<std::string> const summoners = {"magma totem", "fire nova", "explosive trap",
                                                    "trap launcher: explosive trap"};
    if (summoners.count(action->getName()))
        return true;

    CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
    if (!spellAction || dynamic_cast<CastHealingSpellAction*>(action))
        return false;

    uint32 const spellId =
        botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spellAction->getSpell())->Get();
    SpellInfo const* spellInfo = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return spellInfo && (spellInfo->IsTargetingArea() || spellInfo->IsAffectingArea());
}
}  // namespace

float MimironFireBotAoeGuardMultiplier::GetValue(Action* action)
{
    // The spell test first: it answers off the action alone, while the kept list walks the bot's
    // whole target list, and this runs for every action of every bot in the instance.
    if (!action || !MimironActionSplashes(botAI, action))
        return 1.0f;

    std::vector<ObjectGuid> const kept = GetMimironKeptFireBots(botAI, bot);
    if (kept.empty())
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    for (ObjectGuid const& guid : kept)
    {
        Unit* fireBot = botAI->GetUnit(guid);
        if (!fireBot || !fireBot->IsAlive())
            continue;

        if (bot->GetExactDist2d(fireBot) < ULDUAR_MIMIRON_FIREBOT_AOE_CLEARANCE ||
            (target && target->GetExactDist2d(fireBot) < ULDUAR_MIMIRON_FIREBOT_AOE_CLEARANCE))
            return 0.0f;
    }

    return 1.0f;
}

float MimironPlasmaDefensiveHoldMultiplier::GetValue(Action* action)
{
    if (!action || !PlayerbotAI::IsMainTank(bot) || !IsHeldTankDefensive(action->getName()))
        return 1.0f;

    // "mimiron plasma blast defensive action" casts through CastSpell directly, so this never holds it.
    // Plasma Blast only exists while the MK II fights alone. Engaged too: the MK II stands in the room
    // unselectable before the pull, and trash nearby should keep the class nodes.
    return IsMimironEngaged(botAI) && MimironPhase1Active(botAI) ? 0.0f : 1.0f;
}

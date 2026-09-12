/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_XT002.h"

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
#include "UldEncounter_XT002.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

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
    if (IsXT002HeartbreakActive(bot, xt002))
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

    // This runs on every action every bot pops anywhere in Ulduar, and finding XT is a scan. So sort
    // the action first, and only look XT up for the kinds that can come out as anything but 1.0.
    bool const movement = dynamic_cast<MovementAction*>(action) != nullptr;

    // Two aura-map walks that only the immunity and the mover branches below ever read, so they sit
    // behind the dynamic_casts that gate those - a cast, a buff or a heal never pays for them.
    bool carryingKnown = false;
    bool carrying = false;
    auto const carryingDebuff = [&]() -> bool
    {
        if (!carryingKnown)
        {
            carrying =
                bot->HasAura(GetXT002SearingLightSpellId(bot)) || bot->HasAura(GetXT002GravityBombSpellId(bot));
            carryingKnown = true;
        }

        return carrying;
    };

    // The class-generic redirects buff whoever the group flags as main tank, which is the wrong
    // sink while a Pummeller is out; xt002 redirect threat action picks the tank that needs it.
    bool const genericRedirect = dynamic_cast<CastMisdirectionOnMainTankAction*>(action) ||
                                 dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action);

    // "xt002 avoid hazard action" answers both hazards this fight has and is meant to be the only
    // thing moving a bot for them. The generic one sits at the same relevance, fires in every gap
    // where ours returns false, and flees a flat AiPlayerbot.FleeDistance in a direction of its
    // own picking - two movers pulling different ways is what leaves a bot sliding in place.
    bool const genericAvoidAoe = dynamic_cast<AvoidAoeAction*>(action) != nullptr;

    // Both immunities cover all magic and carry SPELL_ATTR1_IMMUNITY_PURGES_EFFECT, so applying
    // one strips the debuff outright - and the boss script summons the Void Zone or the Life Spark
    // from an AfterEffectRemove hook that does not care why the aura went away. Bubbling drops the
    // puddle instantly, wherever the carrier is standing, which is the middle of the raid at the
    // point where a bot is at critical health. Eating the hit is the cheaper trade. Hand of
    // Protection is physical-only and does not strip either debuff, so it is left alone.
    bool const purgingImmunity = (dynamic_cast<CastDivineShieldAction*>(action) ||
                                  dynamic_cast<CastIceBlockAction*>(action)) &&
                                 carryingDebuff();

    // xt002 set dps priority action owns every bot's target, so both generic pickers stand down
    // rather than pulling bots back onto whatever is nearest. The tank one matters most: it ranks
    // any add the tank has no aggro on above the boss, so it walks the tank into the add pile and
    // XT follows. "attack rti target" is deliberately left alone: bots no longer set marks, but a
    // player's mark should still win. Healers are out: a healer the encounter action has not
    // reached yet would be left with no target at all, and a bot that never attacks never enters
    // combat, which costs it every heal that lives on the combat engine.
    bool const genericPicker =
        (dynamic_cast<DpsAssistAction*>(action) && !botAI->IsTank(bot) && !botAI->IsHeal(bot)) ||
        (dynamic_cast<TankAssistAction*>(action) && botAI->IsTank(bot));

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
    bool pinnedMover = false;
    if (movement && !dynamic_cast<AttackAction*>(action) &&
        (botAI->IsRangedDps(bot) || botAI->IsHeal(bot) || carryingDebuff()))
    {
        static std::set<std::string> const encounterMovers = {
            "xt002 raid position action", "xt002 debuff carrier action", "xt002 avoid hazard action"};

        pinnedMover = !encounterMovers.count(action->getName());
    }

    // Normal mode safety floor. The priority action stops offering the Heart here too, but that only
    // stops bots from picking it up again - this is what stops the ones already swinging at it from
    // landing the hit that flips the raid into hard mode.
    //
    // Only the actions that would land a hit on the Heart are blocked. Blocking everything would
    // leave the bot parked in a Gravity Bomb or a Void Zone with no heals and no way to retarget.
    Unit* currentTarget = nullptr;
    bool heartHit = false;
    if (!IsXT002HardModeActive(botAI) && !movement && !dynamic_cast<CastHealingSpellAction*>(action))
    {
        currentTarget = AI_VALUE(Unit*, "current target");
        if (currentTarget && currentTarget->GetEntry() == NPC_HEART_OF_DECONSTRUCTOR)
        {
            static std::set<std::string> const retargets = {"xt002 set dps priority action",
                                                            "xt002 pummeller taunt action",
                                                            "xt002 redirect threat action"};

            heartHit = !retargets.count(action->getName());
        }
    }

    bool const standDown = genericRedirect || genericAvoidAoe || purgingImmunity || genericPicker || pinnedMover;
    if (!standDown && !heartHit)
        return 1.0f;

    Unit* xt002 = GetXT002(botAI);
    if (!xt002)
        return 1.0f;

    // The stand-downs hand generic behaviour to encounter nodes that none of them run before the pull,
    // so out of combat they only stop bots fighting anything else in the room. The Heart floor below
    // is deliberately outside this gate: it has to hold whenever an exposed Heart is in the room, and
    // XT's combat flag is not something worth betting the raid's difficulty on.
    if (standDown && xt002->IsInCombat())
        return 0.0f;

    if (!heartHit)
        return 1.0f;

    Unit* heart = GetXT002ExposedHeart(botAI);
    if (!heart || heart->GetHealthPct() > ULDUAR_XT002_HEART_SAFE_HP_PCT)
        return 1.0f;

    return currentTarget == heart ? 0.0f : 1.0f;
}

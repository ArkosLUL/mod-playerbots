/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Algalon.h"

#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidEncounter.h"
#include "RaidSplash.h"
#include "RaidTankDefensive.h"
#include "ReachTargetActions.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UldActions_Algalon.h"
#include "UldEncounter_Algalon.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

namespace Family = RaidEncounterRules::Family;

namespace
{
// A buff or other friendly spell, which may well be an area one but lands on no star.
bool CastsPositiveSpell(PlayerbotAI* botAI, Action* action)
{
    CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
    if (!spellAction)
        return false;

    uint32 const spellId =
        botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spellAction->getSpell())->Get();
    SpellInfo const* spellInfo = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return spellInfo && spellInfo->IsPositive();
}

// The cooldown casting it now would start, talents included.
int32 CooldownMs(Player* bot, uint32 spellId)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return 0;

    int32 recovery = static_cast<int32>(info->RecoveryTime);
    int32 category = static_cast<int32>(info->CategoryRecoveryTime);
    if (recovery > 0)
        bot->ApplySpellMod(spellId, SPELLMOD_COOLDOWN, recovery);

    if (category > 0 && !info->HasAttribute(SPELL_ATTR6_NO_CATEGORY_COOLDOWN_MODS))
        bot->ApplySpellMod(spellId, SPELLMOD_COOLDOWN, category);

    return std::max(recovery, category);
}
}  // namespace

float AlgalonTargetGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    auto cached = damagesCurrentTarget.find(action);
    if (cached == damagesCurrentTarget.end())
    {
        bool damages = dynamic_cast<MeleeAction*>(action) || (ClassifyAction(action) & Family::PetAttack);
        if (!damages)
        {
            CastSpellAction* spell = dynamic_cast<CastSpellAction*>(action);
            damages = spell && !dynamic_cast<CastHealingSpellAction*>(action) &&
                      spell->GetTargetName() == "current target";
        }

        cached = damagesCurrentTarget.emplace(action, damages).first;
    }

    if (!cached->second)
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return 1.0f;

    switch (target->GetEntry())
    {
        case PB_NPC_ALGALON:
        case PB_NPC_COLLAPSING_STAR:
        case PB_NPC_LIVING_CONSTELLATION:
        case PB_NPC_UNLEASHED_DARK_MATTER:
            return AlgalonMayDamage(bot, target) ? 1.0f : 0.0f;
        default:
            return 1.0f;
    }
}

float AlgalonCooldownReserveMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    auto cached = reserves.find(action);
    if (cached == reserves.end())
    {
        static std::set<std::string> const always = {"divine shield", "lay on hands", "lay on hands on party"};
        static std::set<std::string> const anyone = {"dispersion",       "pain suppression",
                                                     "guardian spirit on party", "hand of sacrifice on party",
                                                     "divine sacrifice", "lay on hands on party"};
        std::string const& name = action->getName();
        Reserve reserve;
        reserve.alwaysForSwapTanks = always.count(name) > 0;
        reserve.swapTanksOnly = !anyone.count(name) && (reserve.alwaysForSwapTanks || IsHeldTankDefensive(name));
        CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
        if (spellAction && (anyone.count(name) || reserve.swapTanksOnly))
            reserve.spellId = context->GetValue<uint32>("spell id", spellAction->getSpell())->Get();

        cached = reserves.emplace(action, reserve).first;
    }

    Reserve const& reserve = cached->second;
    if (!reserve.spellId || !AlgalonEngaged(botAI))
        return 1.0f;

    bool const swapTank = IsAlgalonSwapTank(bot);
    if (reserve.swapTanksOnly && !swapTank)
        return 1.0f;

    if (reserve.alwaysForSwapTanks && swapTank)
        return 0.0f;

    int32 const untilHit = AlgalonMsToBigBangHit(botAI);
    return untilHit == 0 || CooldownMs(bot, reserve.spellId) > untilHit ? 0.0f : 1.0f;
}

float AlgalonSplashHoldMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    auto cached = splashes.find(action);
    if (cached == splashes.end())
    {
        static std::set<std::string> const always = {"starfall", "blade flurry", "sweeping strikes", "bladestorm"};
        static std::set<std::string> const groundAtBot = {"consecration", "magma totem", "explosive trap"};
        static std::set<std::string> const groundAtTarget = {"death and decay", "blizzard", "flamestrike",
                                                             "hurricane",       "rain of fire", "volley"};
        // Chains and detonations the spell test can't see, on top of the shared list.
        static std::set<std::string> const chains = {"hammer of the righteous", "avenger's shield",
                                                     "seed of corruption",      "mind sear",
                                                     "living bomb",             "corpse explosion"};
        std::string const& name = action->getName();
        Splash splash = Splash::None;
        if (always.count(name))
            splash = Splash::Always;
        else if (groundAtBot.count(name))
            splash = Splash::GroundAtBot;
        else if (groundAtTarget.count(name))
            splash = Splash::GroundAtTarget;
        else if (!dynamic_cast<CastHealingSpellAction*>(action) && !CastsPositiveSpell(botAI, action) &&
                 (action->getThreatType() == Action::ActionThreatType::Aoe || MULTI_TARGET_ACTIONS.count(name) ||
                  chains.count(name) || SpellSplashes(botAI, action)))
            splash = Splash::Near;

        cached = splashes.emplace(action, splash).first;
    }

    if (cached->second == Splash::None || !AlgalonEngaged(botAI))
        return 1.0f;

    // All of phase 1: one started before a wave spawns still runs when it lands.
    if (cached->second == Splash::Always)
        return AlgalonInPhaseOne(botAI) ? 0.0f : 1.0f;

    if (!AlgalonAliveStarCount(botAI))
        return 1.0f;

    // The star being hit is the team's to kill; only another one nearby is at risk.
    Unit* target = AI_VALUE(Unit*, "current target");
    ObjectGuid const targetGuid = target ? target->GetGUID() : ObjectGuid::Empty;
    switch (cached->second)
    {
        case Splash::GroundAtBot:
            return AlgalonStarWithin(botAI, bot->GetPosition(), ULDUAR_ALGALON_GROUND_SPLASH_RADIUS, targetGuid)
                       ? 0.0f
                       : 1.0f;
        case Splash::GroundAtTarget:
            return target && AlgalonStarWithin(botAI, target->GetPosition(), ULDUAR_ALGALON_GROUND_SPLASH_RADIUS,
                                               targetGuid)
                       ? 0.0f
                       : 1.0f;
        default:
            break;
    }

    if (AlgalonStarWithin(botAI, bot->GetPosition(), ULDUAR_ALGALON_SPLASH_RADIUS, targetGuid))
        return 0.0f;

    return target && AlgalonStarWithin(botAI, target->GetPosition(), ULDUAR_ALGALON_SPLASH_RADIUS, targetGuid)
               ? 0.0f
               : 1.0f;
}

float AlgalonWalkGuardMultiplier::GetValue(Action* action)
{
    // Holes first: most of the fight there is none, and this weighs every reach in the room.
    if (!action || !AlgalonHoleNear(bot, ULDUAR_ALGALON_SCAN_RADIUS) || !AlgalonEngaged(botAI) ||
        AlgalonShouldRunForShelter(bot))
    {
        return 1.0f;
    }

    // A phased bot back inside a field at the strip is phased again.
    float const stand =
        IsAlgalonPhased(bot) ? ULDUAR_ALGALON_HOLE_EXIT_RADIUS : ULDUAR_ALGALON_FIELD_STAND_CLEARANCE;

    std::string const& name = action->getName();
    if (name == "set behind" || name == "rear flank")
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        if (!target)
            return 1.0f;

        // Set behind walks to one of two spots, 108 degrees either side of his facing at the bot's own
        // distance from him, so both have to be clear.
        float const distance =
            std::max(bot->GetExactDist(target), bot->GetMeleeRange(target) / 2.0f) - bot->GetCombatReach();
        for (float const side : {1.0f, -1.0f})
        {
            float const angle = Position::NormalizeOrientation(target->GetOrientation() +
                                                               side * 3.0f * static_cast<float>(M_PI) / 5.0f);
            if (AlgalonWalkEntersField(bot, target->GetPositionX() + std::cos(angle) * distance,
                                       target->GetPositionY() + std::sin(angle) * distance, stand))
            {
                return 0.0f;
            }
        }

        return 1.0f;
    }

    Unit* target = context->GetValue<Unit*>(action->GetTargetName())->Get();
    if (!target)
        return 1.0f;

    // Where the walk stops: in melee range for melee, in heal or spell range for the rest.
    float range = botAI->GetRange("spell");
    if (name == "reach melee")
        range = bot->GetMeleeRange(target);
    else if (dynamic_cast<ReachPartyMemberToHealAction*>(action))
        range = botAI->GetRange("heal");
    if (bot->GetExactDist2d(target) <= range)
        return 1.0f;

    float const angle = target->GetAngle(bot);
    return AlgalonWalkEntersField(bot, target->GetPositionX() + std::cos(angle) * range,
                                  target->GetPositionY() + std::sin(angle) * range, stand)
               ? 0.0f
               : 1.0f;
}

float AlgalonLeaveFieldMultiplier::GetValue(Action* action)
{
    if (!action || !IsAlgalonPhased(bot) || action->getName() == AlgalonLeaveBlackHoleAction::Name)
        return 1.0f;

    return AlgalonPhasedInField(bot) ? 0.0f : 1.0f;
}

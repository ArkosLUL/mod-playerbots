#include "ToCMultipliers_Anubarak.h"

#include <string>

#include "AttackAction.h"
#include "Creature.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "Map.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidSplash.h"
#include "ReachTargetActions.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ToCActions_Anubarak.h"
#include "ToCData.h"
#include "ToCHelpers_Anubarak.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{

constexpr float ANUBARAK_AREA_THREAT_REACH = 30.0f;
constexpr float ANUBARAK_BURROWER_AREA_REACH = 10.0f;
constexpr float ANUBARAK_BLADESTORM_BURROWER_REACH = 25.0f;
constexpr float ANUBARAK_BLADESTORM_RADIUS = 8.0f;
constexpr float ANUBARAK_BLADESTORM_S = 6.0f;

char const* const SPHERE_CROWD_CONTROL[] = {
    "polymorph", "hex", "repentance", "freezing trap", "shackle undead", "fear", "banish",
    "hibernate", "entangling roots", "cyclone", "sap", "blind", "seduction",
};

bool IsSphereCrowdControl(std::string const& name)
{
    for (char const* cc : SPHERE_CROWD_CONTROL)
    {
        std::string const word(cc);
        if (name.compare(0, word.size(), word) == 0 && (name.size() == word.size() || name[word.size()] == ' '))
            return true;
    }

    return false;
}

bool SlowOrSpreadOnSphere(PlayerbotAI* botAI, Action* action, RaidSplashCache& splash)
{
    if (IsRaidAreaDamage(botAI, action, splash))
        return true;

    RaidSplashCache::Facts& facts = splash.Of(action);
    if (!facts.cast)
        return false;

    uint32 const spellId = facts.spellId.Get(botAI, facts.cast);
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info)
        return false;

    if (info->IsChanneled() || info->IsAffectingArea())
        return true;

    for (SpellEffectInfo const& effect : info->Effects)
        if (effect.Effect == SPELL_EFFECT_APPLY_AURA &&
            (effect.ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE || effect.ApplyAuraName == SPELL_AURA_PERIODIC_LEECH ||
             effect.ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE_PERCENT))
            return true;

    return false;
}

char const* const BURROWER_MOVERS[] = {"death grip", "dark command", "taunt", "growl", "hand of reckoning"};

bool MovesBurrower(std::string const& name)
{
    for (char const* mover : BURROWER_MOVERS)
        if (name == mover)
            return true;

    return false;
}

bool StopsCast(PlayerbotAI* botAI, Action* action, RaidSplashCache& splash)
{
    RaidSplashCache::Facts& facts = splash.Of(action);
    if (!facts.cast)
        return false;

    uint32 const spellId = facts.spellId.Get(botAI, facts.cast);
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info)
        return false;

    for (SpellEffectInfo const& effect : info->Effects)
        if (effect.Effect == SPELL_EFFECT_INTERRUPT_CAST ||
            (effect.Effect == SPELL_EFFECT_APPLY_AURA && effect.ApplyAuraName == SPELL_AURA_MOD_STUN))
            return true;

    return false;
}

}

float AnubarakControlTankMovementMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CombatFormationMoveAction*>(action))
        return 1.0f;

    if (!PlayerbotAI::IsTank(bot))
        return 1.0f;

    if (!AnubarakEngaged(botAI))
        return 1.0f;

    Unit* victim = bot->GetVictim();
    if (!victim)
        return 1.0f;

    uint32 const entry = victim->GetEntry();
    bool const tankingAnubarak =
        entry == static_cast<uint32>(ToCNpcs::NPC_ANUBARAK) ||
        entry == static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER);

    return tankingAnubarak ? 0.0f : 1.0f;
}

bool AnubarakProtectSpikeKiteMultiplier::HeldWhileMarked(Action* action)
{
    if (action->getName() == "feign death")
        return true;

    bool const spellMover = dynamic_cast<CastReachTargetSpellAction*>(action) ||
                            dynamic_cast<CastBlinkBackAction*>(action) || dynamic_cast<CastDisengageAction*>(action);
    if (spellMover)
        return true;

    if (!dynamic_cast<MovementAction*>(action) || dynamic_cast<AnubarakKiteSpikeToPermafrostAction*>(action))
        return false;

    bool const walkingHold = dynamic_cast<AnubarakMainTankHoldBossAction*>(action) ||
                             dynamic_cast<AnubarakAssistTankHoldBurrowerAction*>(action) ||
                             dynamic_cast<AnubarakSideTankReadyAction*>(action) ||
                             dynamic_cast<AnubarakGripFrostSphereAction*>(action);
    return !dynamic_cast<AttackAction*>(action) || walkingHold;
}

float AnubarakProtectSpikeKiteMultiplier::GetValue(Action* action)
{
    if (!action || !_held.Get(action, [action] { return HeldWhileMarked(action); }) || !bot->HasAura(SPELL_MARK))
        return 1.0f;

    return AnubarakEngaged(botAI) ? 0.0f : 1.0f;
}

float AnubarakThreatCapMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || !AnubarakThreatCapped(bot))
        return 1.0f;

    Creature* boss = GetAnubarak(bot);
    if (!boss)
        return 1.0f;

    Unit* target = action->GetTarget();
    if (target == boss || (!target && AI_VALUE(Unit*, "current target") == boss))
        return 0.0f;

    if (IsRaidAreaDamage(botAI, action, _splash) && bot->GetExactDist2d(boss) <= ANUBARAK_AREA_THREAT_REACH)
        return 0.0f;

    return 1.0f;
}

float AnubarakSphereGuardMultiplier::GetValue(Action* action)
{
    if (!action || !AnubarakEngaged(botAI))
        return 1.0f;

    Names const& named = _names.Get(action,
                                    [action]
                                    {
                                        std::string const name = action->getName();
                                        return Names{name == "starfall", IsSphereCrowdControl(name)};
                                    });
    Map* map = bot->GetMap();
    if (map && map->IsHeroic() && named.starfall)
        return 0.0f;

    Unit* target = action->GetTarget();
    Creature* sphere = target ? target->ToCreature() : nullptr;
    if (!sphere || sphere->GetEntry() != static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE))
        return 1.0f;

    if (named.crowdControl)
        return 0.0f;

    if (sphere != GetAnubarakDutySphere(bot))
        return 0.0f;

    return SlowOrSpreadOnSphere(botAI, action, _splash) ? 0.0f : 1.0f;
}

float AnubarakBurrowerThreatCapMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || !IsRaidDpsBot(bot) || !AnubarakEngaged(botAI))
        return 1.0f;

    Unit* target = action->GetTarget();
    Unit* aimed = target ? target : AI_VALUE(Unit*, "current target");
    if (aimed && AnubarakBurrowerThreatCapped(bot, aimed))
        return 0.0f;

    if (IsRaidAreaDamage(botAI, action, _splash) &&
        (AnubarakBurrowerThreatCappedNear(bot, bot->GetPosition(), ANUBARAK_BURROWER_AREA_REACH) ||
         (aimed && AnubarakBurrowerThreatCappedNear(bot, aimed->GetPosition(), ANUBARAK_BURROWER_AREA_REACH))))
        return 0.0f;

    return 1.0f;
}

float AnubarakBurrowerGuardMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || !AnubarakEngaged(botAI))
        return 1.0f;

    Names const& named = _names.Get(action,
                                    [action]
                                    {
                                        std::string const name = action->getName();
                                        return Names{name == "bladestorm", MovesBurrower(name)};
                                    });
    if (named.bladestorm &&
        (AnubarakBurrowerNear(bot, bot->GetPosition(), ANUBARAK_BLADESTORM_BURROWER_REACH, false) ||
         AnubarakLooseBurrowerReaches(bot, bot->GetPosition(), ANUBARAK_BLADESTORM_RADIUS, ANUBARAK_BLADESTORM_S)))
        return 0.0f;

    if (IsAnubarakTankPlayer(bot))
        return GetAnubarakBurrowerTankSide(bot) < 0 && IsRaidAreaDamage(botAI, action, _splash) &&
                       AnubarakBurrowerNear(bot, bot->GetPosition(), ANUBARAK_BURROWER_AREA_REACH, false)
                   ? 0.0f
                   : 1.0f;

    Unit* target = action->GetTarget();
    if (IsRaidAreaDamage(botAI, action, _splash))
    {
        Unit* aimed = target ? target : AI_VALUE(Unit*, "current target");
        bool const melee = PlayerbotAI::IsMelee(bot);
        auto const pulls = [&](Position const& at)
        {
            return AnubarakBurrowerNear(bot, at, ANUBARAK_BURROWER_AREA_REACH, true) ||
                   (melee && AnubarakBurrowerAreaCappedNear(bot, at, ANUBARAK_BURROWER_AREA_REACH));
        };

        if (pulls(bot->GetPosition()) || (aimed && pulls(aimed->GetPosition())))
            return 0.0f;
    }

    Creature* burrower = target ? target->ToCreature() : nullptr;
    if (!burrower || burrower->GetEntry() != static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER))
        return 1.0f;

    if (named.mover)
        return 0.0f;

    if (burrower->HasUnitState(UNIT_STATE_CASTING) && StopsCast(botAI, action, _splash))
        return 1.0f;

    return burrower == GetAnubarakDpsTarget(botAI) ? 1.0f : 0.0f;
}

float AnubarakSideTankAreaGuardMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || GetAnubarakBurrowerTankSide(bot) < 0 ||
        !AnubarakEngaged(botAI))
        return 1.0f;

    return AnubarakSideTankAreaAllowed(botAI, action->getName(), action->GetTarget(),
                                       IsRaidAreaDamage(botAI, action, _splash))
               ? 1.0f
               : 0.0f;
}

float AnubarakSwarmHealMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    auto cached = _heals.find(action);
    if (cached == _heals.end())
    {
        bool const heal =
            dynamic_cast<CastHealingSpellAction*>(action) || dynamic_cast<ReachPartyMemberToHealAction*>(action);
        cached = _heals.emplace(action, heal).first;
    }

    if (!cached->second || !AnubarakSwarmHealing(botAI))
        return 1.0f;

    if (dynamic_cast<CastAoeHealSpellAction*>(action))
        return 0.0f;

    Unit* target = action->GetTarget();
    if (!target)
        return 1.0f;

    return AnubarakSwarmHealWasted(target->ToPlayer()) ? 0.0f : 1.0f;
}

RaidBurstWindow ToCAnubarakBurstWindow(PlayerbotAI* botAI)
{
    return {!AnubarakBurstHeld(botAI), AnubarakLustOpen(botAI)};
}

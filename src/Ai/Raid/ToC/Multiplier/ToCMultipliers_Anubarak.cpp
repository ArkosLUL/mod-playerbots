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
#include "RaidSplash.h"
#include "ReachTargetActions.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ToCActions_Anubarak.h"
#include "ToCData.h"
#include "ToCHelpers_Anubarak.h"
#include "ToCHelpers_Shared.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{

// Area damage from this close counts as hitting him
constexpr float ANUBARAK_AREA_THREAT_REACH = 30.0f;

// Matched as a leading word too, so "cyclone on cc" or "repentance on enemy healer" count
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

// Damage that lands late (any DoT, a hit of its own or not, a channel) or on everything round the sphere:
// a sphere that dies late lands wherever it drifted, and a splash can down a second one
bool SlowOrSpreadOnSphere(PlayerbotAI* botAI, Action* action)
{
    if (IsToCAreaDamage(botAI, action))
        return true;

    CastSpellAction* cast = dynamic_cast<CastSpellAction*>(action);
    if (!cast)
        return false;

    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", cast->getSpell())->Get();
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

// Each one moves a burrower off its side tank's patch, where it submerges back to full
char const* const BURROWER_MOVERS[] = {"death grip", "dark command", "taunt", "growl", "hand of reckoning"};

bool MovesBurrower(std::string const& name)
{
    for (char const* mover : BURROWER_MOVERS)
        if (name == mover)
            return true;

    return false;
}

// Interrupts and stuns, for a burrower's Shadow Strike
bool StopsCast(PlayerbotAI* botAI, Action* action)
{
    CastSpellAction* cast = dynamic_cast<CastSpellAction*>(action);
    if (!cast)
        return false;

    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", cast->getSpell())->Get();
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

float AnubarakProtectSpikeKiteMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Feign Death hands the spike on at its current speed; the kiter immunity casts it only as a last resort
    if (action->getName() == "feign death")
        return bot->HasAura(SPELL_MARK) && AnubarakEngaged(botAI) ? 0.0f : 1.0f;

    // Spell movers: Blink and Disengage off a scarab would throw the kiter 15-20 yd the kite never chose.
    // The kite casts its own Blink directly, so this never holds that one.
    bool const spellMover = dynamic_cast<CastReachTargetSpellAction*>(action) ||
                            dynamic_cast<CastBlinkBackAction*>(action) || dynamic_cast<CastDisengageAction*>(action);
    if (!spellMover)
    {
        if (!dynamic_cast<MovementAction*>(action) || dynamic_cast<AnubarakKiteSpikeToPermafrostAction*>(action))
            return 1.0f;

        // Attack actions, but these holds walk the tank or the DK to its spot
        bool const walkingHold = dynamic_cast<AnubarakMainTankHoldBossAction*>(action) ||
                                 dynamic_cast<AnubarakAssistTankHoldBurrowerAction*>(action) ||
                                 dynamic_cast<AnubarakSideTankReadyAction*>(action) ||
                                 dynamic_cast<AnubarakGripFrostSphereAction*>(action);
        if (dynamic_cast<AttackAction*>(action) && !walkingHold)
            return 1.0f;
    }

    if (!bot->HasAura(SPELL_MARK))
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

    if (IsToCAreaDamage(botAI, action) && bot->GetExactDist2d(boss) <= ANUBARAK_AREA_THREAT_REACH)
        return 0.0f;

    return 1.0f;
}

float AnubarakSphereGuardMultiplier::GetValue(Action* action)
{
    if (!action || !AnubarakEngaged(botAI))
        return 1.0f;

    // Starfall's stars find the spheres overhead, and heroic has six for the pull
    Map* map = bot->GetMap();
    if (map && map->IsHeroic() && action->getName() == "starfall")
        return 0.0f;

    // actions that pick their target in Execute read "self target" here, so the stem's own picks pass
    Unit* target = action->GetTarget();
    Creature* sphere = target ? target->ToCreature() : nullptr;
    if (!sphere || sphere->GetEntry() != static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE))
        return 1.0f;

    if (IsSphereCrowdControl(action->getName()))
        return 0.0f;

    if (sphere != GetAnubarakDutySphere(bot))
        return 0.0f;

    return SlowOrSpreadOnSphere(botAI, action) ? 0.0f : 1.0f;
}

float AnubarakBurrowerGuardMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || IsAnubarakTankPlayer(bot) ||
        !AnubarakEngaged(botAI))
        return 1.0f;

    // actions that pick their target in Execute read "self target" here, so the stem's own picks pass
    Unit* target = action->GetTarget();
    Creature* burrower = target ? target->ToCreature() : nullptr;
    if (!burrower || burrower->GetEntry() != static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER))
        return 1.0f;

    if (MovesBurrower(action->getName()))
        return 0.0f;

    if (burrower->HasUnitState(UNIT_STATE_CASTING) && StopsCast(botAI, action))
        return 1.0f;

    return burrower == GetAnubarakDpsTarget(botAI) ? 1.0f : 0.0f;
}

void AddToCAnubarakMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new AnubarakControlTankMovementMultiplier(botAI));
    multipliers.push_back(new AnubarakProtectSpikeKiteMultiplier(botAI));
}

// Lust waits for phase 3, where Leeching Swarm turns the fight into a race
ToCBurstWindow ToCAnubarakBurstWindow(PlayerbotAI* botAI)
{
    return {true, GetAnubarakPhase(botAI) == AnubarakPhase::Swarm};
}

#include "ToCMultipliers_Anubarak.h"

#include <string>

#include "AttackAction.h"
#include "Creature.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "RaidSplash.h"
#include "ReachTargetActions.h"
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
    // Spell movers: Blink and Disengage off a scarab would throw the kiter 15-20 yd the kite never chose
    bool const spellMover = dynamic_cast<CastReachTargetSpellAction*>(action) ||
                            dynamic_cast<CastBlinkBackAction*>(action) || dynamic_cast<CastDisengageAction*>(action);
    if (!spellMover)
    {
        if (!dynamic_cast<MovementAction*>(action) || dynamic_cast<AnubarakKiteSpikeToPermafrostAction*>(action))
            return 1.0f;

        // Attack actions, but these two holds walk the tank to its spot
        bool const walkingHold = dynamic_cast<AnubarakMainTankHoldBossAction*>(action) ||
                                 dynamic_cast<AnubarakAssistTankHoldBurrowerAction*>(action);
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

    // actions that pick their target in Execute read "self target" here, so the stem's own picks pass
    Unit* target = action->GetTarget();
    Creature* sphere = target ? target->ToCreature() : nullptr;
    if (!sphere || sphere->GetEntry() != static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE))
        return 1.0f;

    if (IsSphereCrowdControl(action->getName()))
        return 0.0f;

    return sphere == GetAnubarakSphereToShoot(bot) ? 1.0f : 0.0f;
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

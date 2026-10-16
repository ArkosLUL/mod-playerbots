#include "ToCMultipliers_Anubarak.h"

#include "AttackAction.h"
#include "Creature.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "ToCActions_Anubarak.h"
#include "ToCData.h"
#include "ToCHelpers_Anubarak.h"

using namespace TrialOfTheCrusaderHelpers;

float AnubarakControlTankMovementMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CombatFormationMoveAction*>(action))
        return 1.0f;

    if (!botAI->IsTank(bot))
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

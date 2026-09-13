#include "RaidAq40Strategy.h"

#include "BossResistanceMultipliers.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "Strategy.h"

void RaidAq40Strategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("viscidus nature resistance trigger",
        { NextAction("viscidus nature resistance action", ACTION_RAID) }
    ));

    triggers.push_back(new TriggerNode("princess huhuran nature resistance trigger",
        { NextAction("princess huhuran nature resistance action", ACTION_RAID) }
    ));

    // // Twin Emperors: aggro holder moves away from the other emperor
    // triggers.push_back(new TriggerNode("aq40 has emperor aggro",
    //     { NextAction("aq40 move from other emperor", ACTION_EMERGENCY) }
    // ));

    // // Twin Emperors: warlock casts searing pain for spell threat on Vek'lor
    // triggers.push_back(new TriggerNode("aq40 warlock tank emperor",
    //     { NextAction("searing pain", ACTION_RAID) }
    // ));

    // // Twin Emperors: target assignment
    // triggers.push_back(new TriggerNode("aq40 target emperor vek'lor",
    //     { NextAction("aq40 attack emperor vek'lor", ACTION_RAID + 1) }
    // ));

    // triggers.push_back(new TriggerNode("aq40 target emperor vek'nilash",
    //     { NextAction("aq40 attack emperor vek'nilash", ACTION_RAID + 1) }
    // ));

    // triggers.push_back(new TriggerNode("aq40 target emperor pests",
    //     { NextAction("aq40 attack emperor pests", ACTION_RAID + 1) }
    // ));

    // // Twin Emperors: tanks move to anchor positions (torch_left / torch_right)
    // triggers.push_back(new TriggerNode("aq40 tank anchor",
    //     { NextAction("aq40 tank anchor position", ACTION_RAID) }
    // ));

    // // Twin Emperors: healers and caster DPS stay near room center
    // triggers.push_back(new TriggerNode("aq40 center position",
    //     { NextAction("aq40 move to room center", ACTION_RAID) }
    // ));

    // // Twin Emperors: pre-teleport positioning (melee DPS move to center ~5s before swap)
    // triggers.push_back(new TriggerNode("aq40 emperor pre teleport",
    //     { NextAction("aq40 move to room center", ACTION_RAID + 2) }
    // ));

    // // Twin Emperors: non-tanks flee from Vek'lor (Arcane Burst)
    // triggers.push_back(new TriggerNode("aq40 near veklor",
    //     { NextAction("aq40 move from veklor", ACTION_EMERGENCY) }
    // ));

    // Viscidus
    triggers.push_back(new TriggerNode("aq40 mage frostbolt viscidus",
        { NextAction("frostbolt", ACTION_RAID) }
    ));

    triggers.push_back(new TriggerNode("aq40 melee viscidus",
        { NextAction("aq40 melee viscidus", ACTION_RAID + 1) }
    ));

    // Ouro
    triggers.push_back(new TriggerNode("aq40 ouro burrowed",
        { NextAction("aq40 ouro burrowed flee", ACTION_RAID) }
    ));

    // C'Thun
    triggers.push_back(new TriggerNode("aq40 cthun1 started",
        { NextAction("aq40 cthun1 get positioned", ACTION_RAID) }
    ));

    triggers.push_back(new TriggerNode("aq40 cthun2 started",
        { NextAction("aq40 cthun2 get positioned", ACTION_RAID) }
    ));
}

void RaidAq40Strategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    // Same boss names the nature resistance actions use, so the multiplier and the trigger resolve the
    // same hunter. Without it the holder's own "bdps" node re-casts Dragonhawk on the next GCD.
    multipliers.push_back(new BossNatureAspectHoldMultiplier(botAI, "viscidus"));
    multipliers.push_back(new BossNatureAspectHoldMultiplier(botAI, "princess huhuran"));
}

// float Aq40EmperorMultiplier::GetValue(Action* action)
// {
//     Unit* boss = AI_VALUE2(Unit*, "find target", "emperor vek'lor");
//     if (!boss || !boss->IsInCombat())
//         return 1.0f;

//     // Suppress formation movement during emperor fight
//     if (dynamic_cast<CombatFormationMoveAction*>(action))
//         return 0.0f;

//     return 1.0f;
// }

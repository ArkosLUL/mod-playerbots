#ifndef PLAYERBOTS_RAID_TOCACTIONS_SHARED_H
#define PLAYERBOTS_RAID_TOCACTIONS_SHARED_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"

// Shared base for actions that flee a spreading ground hazard made of many same-entry creatures
// (Legion Flame trail, slime pools, ...). Flees the centre of the whole nearby cluster so the escape
// vector does not push the bot from one patch straight into the next.
class AvoidCreatureClusterAction : public MovementAction
{
public:
    AvoidCreatureClusterAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {};

protected:
    bool FleeFromCreatureCluster(uint32 entry);
};

// Shared base for the main-tank actions that hold their boss at a fixed point of the room.
class ToCMainTankHoldAction : public AttackAction
{
public:
    ToCMainTankHoldAction(PlayerbotAI* botAI, std::string const name) : AttackAction(botAI, name) {};

protected:
    // Backs this bot up to 5 yd toward anchor, boss in tow, while it tanks boss and stands more
    // than 12 yd from anchor. False once the bot is within 12 yd or boss is on someone else.
    bool DragBossToAnchor(Unit* boss, Position const& anchor);
};

#endif

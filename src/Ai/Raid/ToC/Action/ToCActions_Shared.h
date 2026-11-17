#ifndef PLAYERBOTS_RAID_TOCACTIONS_SHARED_H
#define PLAYERBOTS_RAID_TOCACTIONS_SHARED_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"

// DragBossToAnchor's longest step, and how close to its anchor it stops
inline constexpr float TOC_DRAG_STEP = 5.0f;
inline constexpr float TOC_DRAG_DONE = 12.0f;

// Base for fleeing a ground hazard made of many same-entry creatures (Legion Flame trail, slime pools):
// flees the whole cluster's centre, so the bot doesn't run from one patch straight into the next
class AvoidCreatureClusterAction : public MovementAction
{
public:
    AvoidCreatureClusterAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    bool FleeFromCreatureCluster(uint32 entry);
};

// Base for the main-tank actions that hold their boss at a fixed point of the room.
class ToCMainTankHoldAction : public AttackAction
{
public:
    ToCMainTankHoldAction(PlayerbotAI* botAI, std::string const name) : AttackAction(botAI, name) {}

protected:
    // Backs the bot up a step toward anchor, boss in tow, while it tanks boss further than TOC_DRAG_DONE
    // out. False once inside that or boss is on someone else.
    bool DragBossToAnchor(Unit* boss, Position const& anchor);
};

#endif

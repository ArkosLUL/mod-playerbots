#ifndef PLAYERBOTS_RAID_TOCACTIONS_SHARED_H
#define PLAYERBOTS_RAID_TOCACTIONS_SHARED_H

#include "Action.h"
#include "AttackAction.h"

inline constexpr float TOC_DRAG_STEP = 5.0f;
inline constexpr float TOC_DRAG_DONE = 12.0f;

class ToCMainTankHoldAction : public AttackAction
{
public:
    ToCMainTankHoldAction(PlayerbotAI* botAI, std::string const name) : AttackAction(botAI, name) {}

protected:
    bool DragBossToAnchor(Unit* boss, Position const& anchor);
};

#endif

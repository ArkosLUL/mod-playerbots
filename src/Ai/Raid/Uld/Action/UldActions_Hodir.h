#ifndef PLAYERBOTS_ULDACTIONS_HODIR_H
#define PLAYERBOTS_ULDACTIONS_HODIR_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class HodirMoveSnowpackedIcicleAction : public MovementAction
{
public:
    HodirMoveSnowpackedIcicleAction(PlayerbotAI* botAI) : MovementAction(botAI, "hodir move snowpacked icicle") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class HodirBitingColdJumpAction : public MovementAction
{
public:
    HodirBitingColdJumpAction(PlayerbotAI* ai) : MovementAction(ai, "hodir biting cold jump") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

//
// Hodir hard mode (config-gated): DPS-race behaviours to beat the 3-minute Rare Cache timer.
//

// Kill the ice block encasing a frozen helper so it can start handing out its buffs.
class HodirFreeFrozenHelperAction : public AttackAction
{
public:
    HodirFreeFrozenHelperAction(PlayerbotAI* botAI) : AttackAction(botAI, "hodir free frozen helper") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Carry Storm Cloud into the pack so its Storm Power crit buff spreads to nearby allies.
class HodirSpreadStormCloudAction : public MovementAction
{
public:
    HodirSpreadStormCloudAction(PlayerbotAI* ai) : MovementAction(ai, "hodir spread storm cloud") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Step into a Toasty Fire to stop Biting Cold stacking (no-cheat mitigation).
class HodirMoveToToastyFireAction : public MovementAction
{
public:
    HodirMoveToToastyFireAction(PlayerbotAI* ai) : MovementAction(ai, "hodir move to toasty fire") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif

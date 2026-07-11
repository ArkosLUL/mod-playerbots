#ifndef PLAYERBOTS_ULDTRIGGERS_ALGALON_H
#define PLAYERBOTS_ULDTRIGGERS_ALGALON_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Algalon the Observer
//
class AlgalonCosmicSmashTrigger : public Trigger
{
public:
    AlgalonCosmicSmashTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon cosmic smash trigger") {}
    bool IsActive() override;
};

class AlgalonBigBangTrigger : public Trigger
{
public:
    AlgalonBigBangTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon big bang trigger") {}
    bool IsActive() override;
};

class AlgalonBigBangSoakTrigger : public Trigger
{
public:
    AlgalonBigBangSoakTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon big bang soak trigger") {}
    bool IsActive() override;
};

class AlgalonPhasePunchSwapTrigger : public Trigger
{
public:
    AlgalonPhasePunchSwapTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon phase punch swap trigger") {}
    bool IsActive() override;
};

class AlgalonConstellationKiteTrigger : public Trigger
{
public:
    AlgalonConstellationKiteTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon constellation kite trigger") {}
    bool IsActive() override;
};

class AlgalonDarkMatterTrigger : public Trigger
{
public:
    AlgalonDarkMatterTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon dark matter trigger") {}
    bool IsActive() override;
};

class AlgalonCollapsingStarTrigger : public Trigger
{
public:
    AlgalonCollapsingStarTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon collapsing star trigger") {}
    bool IsActive() override;
};

#endif

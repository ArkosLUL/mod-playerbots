#ifndef PLAYERBOTS_ULDTRIGGERS_ALGALON_H
#define PLAYERBOTS_ULDTRIGGERS_ALGALON_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Algalon the Observer
//
// Anything that has to sweep the room for creatures runs on a 2s interval. The two reactions with a
// hard deadline - the 8s Big Bang cast and the 4s Cosmic Smash marker - stay on every tick.
//
class AlgalonResetEncounterStateTrigger : public Trigger
{
public:
    AlgalonResetEncounterStateTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon reset encounter state", 5) {}
    bool IsActive() override;
};

class AlgalonBigBangHideTrigger : public Trigger
{
public:
    AlgalonBigBangHideTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon big bang hide") {}
    bool IsActive() override;
};

class AlgalonBigBangSoakTrigger : public Trigger
{
public:
    AlgalonBigBangSoakTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon big bang soak") {}
    bool IsActive() override;
};

class AlgalonCosmicSmashTrigger : public Trigger
{
public:
    AlgalonCosmicSmashTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon cosmic smash") {}
    bool IsActive() override;
};

class AlgalonLeaveBlackHoleTrigger : public Trigger
{
public:
    AlgalonLeaveBlackHoleTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon leave black hole", 2) {}
    bool IsActive() override;
};

class AlgalonPhasePunchSwapTrigger : public Trigger
{
public:
    AlgalonPhasePunchSwapTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon phase punch swap", 2) {}
    bool IsActive() override;
};

class AlgalonConstellationTauntTrigger : public Trigger
{
public:
    AlgalonConstellationTauntTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon constellation taunt", 2) {}
    bool IsActive() override;
};

class AlgalonConstellationKiteTrigger : public Trigger
{
public:
    AlgalonConstellationKiteTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon constellation kite", 2) {}
    bool IsActive() override;
};

class AlgalonCollapsingStarFocusTrigger : public Trigger
{
public:
    AlgalonCollapsingStarFocusTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon collapsing star focus", 2) {}
    bool IsActive() override;
};

class AlgalonDarkMatterTankTrigger : public Trigger
{
public:
    AlgalonDarkMatterTankTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon dark matter tank", 2) {}
    bool IsActive() override;
};

class AlgalonDarkMatterMarkTrigger : public Trigger
{
public:
    AlgalonDarkMatterMarkTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon dark matter mark", 2) {}
    bool IsActive() override;
};

class AlgalonRaidPositionTrigger : public Trigger
{
public:
    AlgalonRaidPositionTrigger(PlayerbotAI* ai) : Trigger(ai, "algalon raid position", 2) {}
    bool IsActive() override;
};

#endif

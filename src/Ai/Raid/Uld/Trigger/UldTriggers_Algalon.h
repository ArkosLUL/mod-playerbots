#ifndef PLAYERBOTS_ULDTRIGGERS_ALGALON_H
#define PLAYERBOTS_ULDTRIGGERS_ALGALON_H

#include "EventMap.h"
#include "GenericTriggers.h"
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
    static constexpr char const* Name = "algalon reset encounter state";

    AlgalonResetEncounterStateTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 5) {}
    bool IsActive() override;
};

class AlgalonBigBangHideTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon big bang hide";

    AlgalonBigBangHideTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class AlgalonBigBangSoakTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon big bang soak";

    AlgalonBigBangSoakTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class AlgalonCosmicSmashTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon cosmic smash";

    AlgalonCosmicSmashTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class AlgalonLeaveBlackHoleTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon leave black hole";

    AlgalonLeaveBlackHoleTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2) {}
    bool IsActive() override;
};

class AlgalonPhasePunchSwapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon phase punch swap";

    AlgalonPhasePunchSwapTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2) {}
    bool IsActive() override;
};

class AlgalonConstellationTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon constellation taunt";

    AlgalonConstellationTauntTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2) {}
    bool IsActive() override;
};

class AlgalonConstellationKiteTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon constellation kite";

    AlgalonConstellationKiteTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2) {}
    bool IsActive() override;
};

class AlgalonCollapsingStarFocusTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon collapsing star focus";

    AlgalonCollapsingStarFocusTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2) {}
    bool IsActive() override;
};

class AlgalonDarkMatterTankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon dark matter tank";

    AlgalonDarkMatterTankTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2) {}
    bool IsActive() override;
};

class AlgalonDarkMatterMarkTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon dark matter mark";

    AlgalonDarkMatterMarkTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2) {}
    bool IsActive() override;
};

class AlgalonRaidPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon raid position";

    AlgalonRaidPositionTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2) {}
    bool IsActive() override;
};

#endif

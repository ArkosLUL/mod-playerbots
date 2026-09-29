#ifndef PLAYERBOTS_ULDTRIGGERS_ALGALON_H
#define PLAYERBOTS_ULDTRIGGERS_ALGALON_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

//
// Algalon the Observer
//
// Everything reads the per-instance room scan, so no trigger sweeps on its own. The Big Bang and Cosmic
// Smash reactions have hard deadlines and run every tick; the rest are paced.
//
class AlgalonBigBangSoakTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon big bang soak";

    AlgalonBigBangSoakTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class AlgalonBigBangExternalTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon big bang external";

    AlgalonBigBangExternalTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class AlgalonBigBangHideTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon big bang hide";

    AlgalonBigBangHideTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
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

    AlgalonLeaveBlackHoleTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 500) {}
    bool IsActive() override;
};

class AlgalonTankPickupTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon tank pickup";

    AlgalonTankPickupTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 1000) {}
    bool IsActive() override;
};

class AlgalonPhasePunchSwapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon phase punch swap";

    AlgalonPhasePunchSwapTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2000) {}
    bool IsActive() override;
};

class AlgalonConstellationTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon constellation taunt";

    AlgalonConstellationTauntTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 1000) {}
    bool IsActive() override;
};

class AlgalonDarkMatterTankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon dark matter tank";

    AlgalonDarkMatterTankTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2000) {}
    bool IsActive() override;
};

class AlgalonConstellationKiteTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon constellation kite";

    AlgalonConstellationKiteTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 500) {}
    bool IsActive() override;
};

class AlgalonStarTeamTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon star team";

    AlgalonStarTeamTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 1000) {}
    bool IsActive() override;
};

class AlgalonStarMarkTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon star mark";

    AlgalonStarMarkTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2000) {}
    bool IsActive() override;
};

class AlgalonRaidPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "algalon raid position";

    AlgalonRaidPositionTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 1000) {}
    bool IsActive() override;
};

#endif

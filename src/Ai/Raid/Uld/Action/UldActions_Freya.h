#ifndef PLAYERBOTS_ULDACTIONS_FREYA_H
#define PLAYERBOTS_ULDACTIONS_FREYA_H

#include <vector>

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_Freya.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class FreyaMoveAwayNatureBombAction : public MovementAction
{
public:
    static constexpr char const* Name = "freya move away nature bomb";

    FreyaMoveAwayNatureBombAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Position bombSpot;
    uint32 bombSpotMs = 0;
    Position bombOrigin;
    uint32 reachRetryMs = 0;
};

class FreyaTankNatureBombAction : public MovementAction
{
public:
    static constexpr char const* Name = "freya tank nature bomb";

    FreyaTankNatureBombAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Position kiteSpot;
    uint32 kiteSpotMs = 0;
};

class FreyaSetDpsPriorityAction : public AttackAction
{
public:
    static constexpr char const* Name = "freya set dps priority";

    FreyaSetDpsPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Unit* ResolveFreyaDpsTarget(Unit* currentTarget);

    bool TakesEonarsGift(FreyaWaveState const& state);

    Unit* ShedSnaplasherThreat(FreyaWaveState const& state, Unit* assigned);

    ObjectGuid giftGuid;
    uint32 giftSeenMs = 0;

    ObjectGuid snaplasherGuid;
    uint32 snaplasherSeenMs = 0;
    bool snaplasherShed = false;
};

class FreyaTankAddsAction : public AttackAction
{
public:
    static constexpr char const* Name = "freya tank adds";

    FreyaTankAddsAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    bool ParkConservator(Unit* conservator);

    ObjectGuid parkedSpore;
};

class FreyaMoveToHealingSporeAction : public MovementAction
{
public:
    static constexpr char const* Name = "freya move to healing spore action";

    FreyaMoveToHealingSporeAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class FreyaRedirectThreatAction : public Action
{
public:
    static constexpr char const* Name = "freya redirect threat";

    FreyaRedirectThreatAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Player* GetRedirectTank(Unit*& dumpTarget);
};

class FreyaLasherBailAction : public MovementAction
{
protected:
    FreyaLasherBailAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

    bool StepOutOfBlasts(std::vector<Position> const& blasts, float clearance);

private:
    Position bailSpot;
    uint32 bailSpotMs = 0;
};

class FreyaLasherAboutToBlowAction : public FreyaLasherBailAction
{
public:
    static constexpr char const* Name = "freya lasher about to blow";

    FreyaLasherAboutToBlowAction(PlayerbotAI* botAI) : FreyaLasherBailAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class FreyaRangedCampAction : public FreyaLasherBailAction
{
public:
    static constexpr char const* Name = "freya ranged camp";

    FreyaRangedCampAction(PlayerbotAI* botAI) : FreyaLasherBailAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class FreyaReachHeldPackAction : public MovementAction
{
public:
    static constexpr char const* Name = "freya reach held pack";

    FreyaReachHeldPackAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Position walkSpot;
    uint32 walkSpotMs = 0;
    uint32 retryMs = 0;
};

class FreyaFrostNovaLashersAction : public Action
{
public:
    static constexpr char const* Name = "freya frost nova lashers";

    FreyaFrostNovaLashersAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class FreyaTrapLashersAction : public Action
{
public:
    static constexpr char const* Name = "freya trap lashers";

    FreyaTrapLashersAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class FreyaSummonArmyAction : public Action
{
public:
    static constexpr char const* Name = "freya summon army";

    FreyaSummonArmyAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class FreyaGroundTremorHoldCastAction : public Action
{
public:
    static constexpr char const* Name = "freya ground tremor hold cast";

    FreyaGroundTremorHoldCastAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class FreyaBreakIronRootsAction : public AttackAction
{
public:
    static constexpr char const* Name = "freya break iron roots";

    FreyaBreakIronRootsAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class FreyaFreeRootedAllyAction : public Action
{
public:
    static constexpr char const* Name = "freya free rooted ally";

    FreyaFreeRootedAllyAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class FreyaDodgeUnstableSunBeamAction : public MovementAction
{
public:
    static constexpr char const* Name = "freya dodge unstable sun beam";

    FreyaDodgeUnstableSunBeamAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Position dodgeSpot;
    uint32 dodgeSpotMs = 0;
};

class FreyaNaturesFuryBailAction : public MovementAction
{
public:
    static constexpr char const* Name = "freya nature fury bail";

    FreyaNaturesFuryBailAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Position bailSpot;
    uint32 bailSpotMs = 0;
};

class FreyaStepOutOfSunbeamAction : public MovementAction
{
public:
    static constexpr char const* Name = "freya step out of sunbeam";

    FreyaStepOutOfSunbeamAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Position stepSpot;
    uint32 stepSpotMs = 0;
};

class FreyaTankHoldFreyaAction : public MovementAction
{
public:
    static constexpr char const* Name = "freya tank hold freya";

    FreyaTankHoldFreyaAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    uint32 holdMs = 0;
};

#endif

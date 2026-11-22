#ifndef PLAYERBOTS_ULDACTIONS_XT002_H
#define PLAYERBOTS_ULDACTIONS_XT002_H

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "UldEncounter_XT002.h"

class XT002MoveClearAction : public MovementAction
{
public:
    XT002MoveClearAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

    bool isPossible() override;

protected:
    enum class MoveIssue
    {
        Taken,
        NoPath,
        Refused
    };

    MoveIssue IssueMove(float x, float y, float z);

    bool MoveClearOf(std::vector<std::pair<Unit*, float>> const& avoid);
};

class XT002DebuffCarrierAction : public XT002MoveClearAction
{
public:
    static constexpr char const* Name = "xt002 debuff carrier action";

    XT002DebuffCarrierAction(PlayerbotAI* botAI) : XT002MoveClearAction(botAI, Name) {}

    bool Execute(Event event) override;

private:
    enum class ParkResult
    {
        None,
        OutOfReach,
        Moving,
        Parked
    };

    ParkResult ParkVoidZone(std::list<Creature*> const& voidZones, float reach, float& cellX, float& cellY);

    float TravelReach(uint32 remainingMs) const;

    bool StopShortOf(float cellX, float cellY, float reach, std::list<Creature*> const& voidZones);

    bool HoldsXT002OrPummeller(Unit* xt002) const;

    bool MoveToSearingLightSpot(bool heartbreak);

    float RaidClearance(float x, float y) const;

    float ApproachClearance(float x, float y, std::list<Creature*> const& voidZones) const;

    bool StandingInVoidZone(std::list<Creature*> const& voidZones) const;

    bool InsideParkingLot() const;

    bool parked = false;
};

class XT002AvoidHazardAction : public XT002MoveClearAction
{
public:
    static constexpr char const* Name = "xt002 avoid hazard action";

    XT002AvoidHazardAction(PlayerbotAI* botAI) : XT002MoveClearAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class XT002BombHealAction : public XT002MoveClearAction
{
public:
    static constexpr char const* Name = "xt002 bomb heal action";

    XT002BombHealAction(PlayerbotAI* botAI) : XT002MoveClearAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class XT002BombCarrierHealAction : public Action
{
public:
    static constexpr char const* Name = "xt002 bomb carrier heal action";

    XT002BombCarrierHealAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class XT002PummellerTauntAction : public Action
{
public:
    static constexpr char const* Name = "xt002 pummeller taunt action";

    XT002PummellerTauntAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
};

class XT002RedirectThreatAction : public Action
{
public:
    static constexpr char const* Name = "xt002 redirect threat action";

    XT002RedirectThreatAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Player* GetRedirectTank(Unit* xt002);
};

class XT002HolderBuffAction : public Action
{
public:
    XT002HolderBuffAction(PlayerbotAI* botAI, std::string const name, std::string const spell)
        : Action(botAI, name), spell(spell)
    {
    }

    bool Execute(Event event) override;

private:
    std::string spell;
};

class XT002RaidPositionAction : public MovementAction
{
public:
    static constexpr char const* Name = "xt002 raid position action";

    XT002RaidPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class XT002SetDpsPriorityAction : public AttackAction
{
public:
    static constexpr char const* Name = "xt002 set dps priority action";

    XT002SetDpsPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}

    bool Execute(Event event) override;

private:
    std::vector<std::pair<uint32, Unit*>> BuildPriorityList(Unit*& xt002);

    Unit* SelectByEntry(Unit* currentTarget, uint32 entry, std::vector<Unit*> const& candidates) const;

    bool IsAllowedTarget(Unit* unit, Unit* xt002) const;

    Unit* ResolveTarget(Unit* currentTarget);
};

#endif

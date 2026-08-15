#ifndef PLAYERBOTS_ULDACTIONS_XT002_H
#define PLAYERBOTS_ULDACTIONS_XT002_H

#include <string>
#include <utility>
#include <vector>

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "UldBossHelper.h"

//
// XT-002 Deconstructor
//

// Shared 8-direction ring search, the same shape the generic MoveAwayFrom* actions use.
class XT002MoveClearAction : public MovementAction
{
public:
    XT002MoveClearAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

    bool isPossible() override;

protected:
    // Steps towards the nearest spot that puts every unit in `avoid` at least `range` away, or the
    // best partial improvement when the raid is packed too tightly for that. False when nothing needs
    // avoiding or standing still is already as good as it gets.
    bool MoveClearOf(std::vector<Unit*> const& avoid, float range);
};

// Steps out of the splash of every debuffed raid member. MoveAwayFromPlayerWithDebuffAction takes a
// single spell id fixed at construction, which cannot cover both the 10- and 25-man versions of
// Searing Light and Gravity Bomb, so the id is resolved per tick here instead.
class XT002MoveAwayFromDebuffedAllyAction : public XT002MoveClearAction
{
public:
    XT002MoveAwayFromDebuffedAllyAction(PlayerbotAI* botAI, std::string const name, float range)
        : XT002MoveClearAction(botAI, name), range(range)
    {
    }

    bool Execute(Event event) override;

protected:
    virtual uint32 GetDebuffSpellId() = 0;

    float range;
};

class XT002SearingLightSpreadAction : public XT002MoveAwayFromDebuffedAllyAction
{
public:
    XT002SearingLightSpreadAction(PlayerbotAI* botAI)
        : XT002MoveAwayFromDebuffedAllyAction(botAI, "xt002 searing light spread action",
                                              ULDUAR_XT002_DEBUFF_SPREAD_RADIUS)
    {
    }

protected:
    uint32 GetDebuffSpellId() override { return GetXT002SearingLightSpellId(bot); }
};

class XT002GravityBombSpreadAction : public XT002MoveAwayFromDebuffedAllyAction
{
public:
    XT002GravityBombSpreadAction(PlayerbotAI* botAI)
        : XT002MoveAwayFromDebuffedAllyAction(botAI, "xt002 gravity bomb spread action",
                                              ULDUAR_XT002_DEBUFF_SPREAD_RADIUS)
    {
    }

protected:
    uint32 GetDebuffSpellId() override { return GetXT002GravityBombSpellId(bot); }
};

// The carrier runs clear of the raid: the splash hurts everyone around it, and once Heartbreak is up
// the Void Zone lands wherever the debuff expires, so from that point the run has a fixed destination.
class XT002GravityBombCarrierAction : public XT002MoveClearAction
{
public:
    XT002GravityBombCarrierAction(PlayerbotAI* botAI)
        : XT002MoveClearAction(botAI, "xt002 gravity bomb carrier action")
    {
    }

    bool Execute(Event event) override;

private:
    // Walks the parking grid from the role's origin and moves to the first cell that is free of Void
    // Zones and in line of sight. False when no cell qualifies, leaving the dynamic search as fallback.
    bool ParkVoidZone(Unit* boss);
};

class XT002BoombotAvoidAction : public MoveAwayFromCreatureAction
{
public:
    XT002BoombotAvoidAction(PlayerbotAI* botAI)
        : MoveAwayFromCreatureAction(botAI, "xt002 boombot avoid action", PB_NPC_XT002_BOOMBOT,
                                     ULDUAR_XT002_BOOMBOT_AVOID_RADIUS)
    {
    }
};

class XT002VoidZoneAction : public MoveAwayFromCreatureAction
{
public:
    XT002VoidZoneAction(PlayerbotAI* botAI)
        : MoveAwayFromCreatureAction(botAI, "xt002 void zone action", PB_NPC_XT002_VOID_ZONE,
                                     ULDUAR_XT002_VOID_ZONE_RADIUS)
    {
    }
};

class XT002PummellerTauntAction : public Action
{
public:
    XT002PummellerTauntAction(PlayerbotAI* botAI) : Action(botAI, "xt002 pummeller taunt action") {}

    bool Execute(Event event) override;
};

class XT002RedirectThreatAction : public Action
{
public:
    XT002RedirectThreatAction(PlayerbotAI* botAI) : Action(botAI, "xt002 redirect threat action") {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    // The tank this bot's threat should land on, or nullptr when there is nothing to redirect to.
    Player* GetRedirectTank();
};


// Anchors the fight. Only the main tank and ranged DPS get a spot: healers position by heal range,
// which a fixed point cannot track, and pinning melee costs uptime on a boss that moves.
class XT002RaidPositionAction : public MovementAction
{
public:
    XT002RaidPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, "xt002 raid position action") {}

    bool Execute(Event event) override;
};

// Searing Light splashes everyone within ULDUAR_XT002_DEBUFF_SPREAD_RADIUS, and once Heartbreak is up
// its expiry spawns a Life Spark, so the carrier always leaves from the same place.
class XT002SearingLightCarrierAction : public MovementAction
{
public:
    XT002SearingLightCarrierAction(PlayerbotAI* botAI)
        : MovementAction(botAI, "xt002 searing light carrier action")
    {
    }

    bool Execute(Event event) override;
};

// Owns "current target" for every non-tank while XT is up, so nothing has to be marked. Raid icons are
// group-global and stamping one here would overwrite whatever the player and the other bots are using.
class XT002SetDpsPriorityAction : public AttackAction
{
public:
    XT002SetDpsPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, "xt002 set dps priority action") {}

    bool Execute(Event event) override;

private:
    // Ordered candidates for this bot's role, most urgent first. Entries the role must not touch are
    // left out entirely rather than filtered later.
    std::vector<std::pair<uint32, Unit*>> BuildPriorityList();

    // Nearest live candidate of `entry`, preferring the current target so two identical adds cannot
    // make the bot alternate between them every tick.
    Unit* SelectByEntry(Unit* currentTarget, uint32 entry, std::vector<Unit*> const& candidates) const;

    bool IsAllowedTarget(Unit* unit) const;

    Unit* ResolveTarget(Unit* currentTarget);
};

#endif

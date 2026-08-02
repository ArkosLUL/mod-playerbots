#ifndef PLAYERBOTS_ULDACTIONS_XT002_H
#define PLAYERBOTS_ULDACTIONS_XT002_H

#include <string>
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

// The carrier runs clear of the raid: the splash hurts everyone around it, and in hard mode the Void
// Zone lands wherever the debuff expires.
class XT002GravityBombCarrierAction : public XT002MoveClearAction
{
public:
    XT002GravityBombCarrierAction(PlayerbotAI* botAI)
        : XT002MoveClearAction(botAI, "xt002 gravity bomb carrier action")
    {
    }

    bool Execute(Event event) override;
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

class XT002MarkKillTargetAction : public Action
{
public:
    XT002MarkKillTargetAction(PlayerbotAI* botAI) : Action(botAI, "xt002 mark kill target action") {}

    bool Execute(Event event) override;
};

class XT002BoombotRangedKillAction : public AttackAction
{
public:
    XT002BoombotRangedKillAction(PlayerbotAI* botAI) : AttackAction(botAI, "xt002 boombot ranged kill action") {}

    bool Execute(Event event) override;
};

// The Heart is attacked directly rather than through the skull, so the add focus and the Heart never
// fight over the same mark.
class XT002AttackHeartAction : public AttackAction
{
public:
    XT002AttackHeartAction(PlayerbotAI* botAI) : AttackAction(botAI, "xt002 attack heart action") {}

    bool Execute(Event event) override;
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

#endif

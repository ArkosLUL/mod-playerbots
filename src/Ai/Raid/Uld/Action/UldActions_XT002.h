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
    // Steps towards the nearest spot that gives every unit in `avoid` the clearance paired with it, or
    // the best partial improvement when the raid is packed too tightly for that. Clearance is per unit
    // so one search can mix hazards of different sizes. False when nothing needs avoiding or standing
    // still is already as good as it gets.
    bool MoveClearOf(std::vector<std::pair<Unit*, float>> const& avoid);
};

// Owns every move a debuff carrier makes, with both debuffs in the one node. Two nodes at the same
// relevance cannot share a bot: the engine ends the tick at the first action that returns true and
// leaves the loser queued, so the pair trade the tick and the bot ends up walking the line between
// their destinations. Gravity Bomb wins when a bot holds both - its puddle denies raid floor for 180s,
// where the Searing Light splash lasts 9s and expires over an empty parking lot.
class XT002DebuffCarrierAction : public XT002MoveClearAction
{
public:
    XT002DebuffCarrierAction(PlayerbotAI* botAI) : XT002MoveClearAction(botAI, "xt002 debuff carrier action") {}

    bool Execute(Event event) override;

private:
    enum class ParkResult
    {
        None,        // no cell qualifies; the dynamic search is the fallback
        OutOfReach,  // a cell was chosen, but the bomb goes off before the bot could get there
        Moving,      // walking to a cell, so this action owns the tick
        Parked       // standing on one, so the tick is free for casts and heals
    };

    // Picks the nearest cell of the role's parking grid that is free of Void Zones and in line of
    // sight, preferring one the bot can still reach inside `reach` yards, then one with room to spare
    // and a clear approach. Reports on having chosen a cell, never on whether MoveTo issued an order:
    // MoveTo goes false while the bot is already walking there. The winner is written to
    // `cellX`/`cellY` whether or not it turned out to be reachable.
    ParkResult ParkVoidZone(Unit* boss, float reach, float& cellX, float& cellY);

    // How far the bot can still walk before the bomb goes off. GetSpeed(MOVE_RUN) already carries
    // Tympanic Tantrum's 50% slow, which is the case this exists for: slowed, no cell in either lot
    // is inside a 9s debuff.
    float TravelReach(uint32 remainingMs) const;

    // Follows the bearing to a cell as far as the time budget allows and stops there, so a carrier
    // that cannot make the lot drops its puddle on the approach instead of mid-stride in the raid.
    // False when the stopping point is not clear of the expiry pull, which hands the tick to the
    // dynamic search - that one maximises clearance rather than following a fixed bearing.
    bool StopShortOf(float cellX, float cellY, float reach);

    // Whether the straight line from the bot to (x, y) stays ULDUAR_XT002_BOMB_APPROACH_CLEARANCE
    // clear of every Void Zone in `voidZones`. An approximation - the bot follows a navmesh path, not
    // this line - but the carrier action outranks the hazard dodge, so it is the only guard there is.
    bool ApproachIsClear(float x, float y, std::list<Creature*> const& voidZones) const;

    // Whether the bot is standing in its role's parking grid. Geometry only, ignoring which cells are
    // occupied: the case this exists for is a carrier whose bomb has just expired under its feet, so
    // the cell it is standing on is certain to be occupied by its own fresh puddle.
    bool InsideParkingLot() const;

    // Latched once standing on a cell, so drift inside the deadband does not re-issue a move. A bot
    // that re-issues every tick slides in place and cannot cast.
    bool parked = false;
};

// One node for both things a bot has to step out of, for the same reason the carriers are one node:
// two movers at the same relevance trade the tick and the bot slides between their destinations.
// Puddles are skipped while the bot carries a debuff - the carrier action decides where a carrier
// stands relative to those, and it is also what walks one off its own bomb.
class XT002AvoidHazardAction : public XT002MoveClearAction
{
public:
    XT002AvoidHazardAction(PlayerbotAI* botAI) : XT002MoveClearAction(botAI, "xt002 avoid hazard action") {}

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

// Anchors the fight. The main tank and ranged DPS get a point each; healers share the ranged one but
// with a band wide enough that heal range still picks the spot inside it, which is what stops them
// trailing whoever is taking damage across the room. Melee are left alone - pinning them costs uptime
// on a boss that moves.
class XT002RaidPositionAction : public MovementAction
{
public:
    XT002RaidPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, "xt002 raid position action") {}

    bool Execute(Event event) override;
};

// Owns "current target" for every role while XT is up, so nothing has to be marked. Raid icons are
// group-global and stamping one here would overwrite whatever the player and the other bots are using.
// Tanks get a list of their own: the boss, the Pummeller only for the tank that owns it while a second
// tank is alive to hold XT, and the Heart in hard mode.
//
// Healers get nothing at all and have any leftover target cleared - Ulduar is in
// RestrictedHealerDPSMaps, so they have no damage node that could use one, and all a target does there
// is fire "reach spell". Adds are offered only once they are inside the leash around XT and within the
// bot's own reach, so nobody walks at one that never left its toy pile. Tanks are exempt from the
// reach gate: going and getting the Pummeller is the off-tank's job.
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

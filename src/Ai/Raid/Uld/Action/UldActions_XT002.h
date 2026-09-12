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
    // What offering a destination to the pathfinder achieved.
    enum class MoveIssue
    {
        Taken,    // the command reached the MotionMaster, or the bot is already on its way there
        NoPath,   // this destination is refused; a different one may not be
        Refused   // the bot is not moving this tick whatever it is offered
    };

    // One destination, reported precisely enough to decide whether trying another is worth the tick.
    // Plain MoveTo cannot answer that: it returns false both for a genuine refusal and for the
    // ordinary "already walking there", and treating the second as failure turns a carrier around
    // mid-run.
    MoveIssue IssueMove(float x, float y, float z);

    // Steps towards the nearest spot that gives every unit in `avoid` the clearance paired with it, or
    // the best partial improvement when the raid is packed too tightly for that. Clearance is per unit
    // so one search can mix hazards of different sizes. Walks its ranked candidates until the
    // pathfinder accepts one - this room refuses plainly walkable points, and it refuses the same one
    // every tick, so a search that offered only its winner left carriers standing in the raid for the
    // whole debuff. False when nothing needs avoiding or standing still is already as good as it gets.
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

    // Picks a cell from every parking grid the role owns that is free of Void Zones, preferring one the
    // bot can still reach inside `reach` yards, then one with room to spare, then a clear approach,
    // then the shortest walk. Ranged and healers have a lot on each side of the formation, and that
    // last tie-break is what sends a carrier to the near one rather than across the room. Offers its
    // ranked cells to the pathfinder in turn and reports Moving on the first one accepted. There is
    // deliberately no line-of-sight test: the lots sit past the edge of the Ulduar building geometry,
    // so a ray from the raid clips the rim and rejects every cell while the navmesh path to each of
    // them is clean. The best cell is written to `cellX`/`cellY` whether or not it turned out to be
    // usable.
    ParkResult ParkVoidZone(Unit* boss, float reach, float& cellX, float& cellY);

    // How far the bot can still walk before the bomb goes off. GetSpeed(MOVE_RUN) already carries
    // Tympanic Tantrum's 50% slow, which is the case this exists for: slowed, the reach is about 25yd,
    // which is one lot away and no further.
    float TravelReach(uint32 remainingMs) const;

    // Follows the bearing to a cell as far as the time budget allows and stops there, so a carrier
    // that cannot make the lot drops its puddle on the approach instead of mid-stride in the raid.
    // False when the stopping point is not clear of the expiry pull or the pathfinder will not take
    // it, which hands the tick to the dynamic search - that one maximises clearance rather than
    // following a fixed bearing.
    bool StopShortOf(float cellX, float cellY, float reach);

    // The Searing Light spot, or the nearest alternate around it that is clear of Void Zones and that
    // the pathfinder accepts. Neither guard is optional: a puddle sat 2 yd from the spot for half a
    // fight, and findSmoothPath refuses the spot outright from most of the melee stack, which left
    // carriers irradiating the raid for the full 9s. `heartbreak` is Execute's answer, since Void Zones
    // only exist once XT carries it.
    bool MoveToSearingLightSpot(bool heartbreak);

    // Distance from (x, y) to the nearest living raider, this bot aside. Ranks Searing Light
    // alternates: they are all far enough from the raid on paper, and this picks the one that is
    // actually roomiest once the raid has drifted off its slots.
    float RaidClearance(float x, float y) const;

    // Whether the straight line from the bot to (x, y) stays ULDUAR_XT002_BOMB_APPROACH_CLEARANCE
    // clear of every Void Zone in `voidZones`. An approximation - the bot follows a navmesh path, not
    // this line - but the carrier action outranks the hazard dodge, so it is the only guard there is.
    bool ApproachIsClear(float x, float y, std::list<Creature*> const& voidZones) const;

    // Whether the bot is standing in any parking grid its role owns. Geometry only, ignoring which
    // cells are occupied: the case this exists for is a carrier whose bomb has just expired under its
    // feet, so the cell it is standing on is certain to be occupied by its own fresh puddle.
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
    Player* GetRedirectTank(Unit* xt002);
};

// Anchors the fight. The main tank gets a point; ranged DPS and healers get a slot each in the
// formation around the ranged anchor, rather than the anchor itself - sharing one coordinate put
// thirteen bots inside 3 yd of it and let a single Searing Light take the group. Melee are left alone,
// since pinning them costs uptime on a boss that moves.
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
// Healers get one too. Ulduar is in RestrictedHealerDPSMaps, so they never spend a GCD on it, but a bot
// with no target never enters combat and loses every heal that lives on the combat engine. Adds are
// offered only once they are inside the leash around XT and within the bot's own reach, so nobody walks
// at one that never left its toy pile. Tanks are exempt from the reach gate: going and getting the
// Pummeller is the off-tank's job.
class XT002SetDpsPriorityAction : public AttackAction
{
public:
    XT002SetDpsPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, "xt002 set dps priority action") {}

    bool Execute(Event event) override;

private:
    // Ordered candidates for this bot's role, most urgent first. Entries the role must not touch are
    // left out entirely rather than filtered later. `xt002` comes back as the unit GetXT002 would return,
    // found in the same pass.
    std::vector<std::pair<uint32, Unit*>> BuildPriorityList(Unit*& xt002);

    // Nearest live candidate of `entry`, preferring the current target so two identical adds cannot
    // make the bot alternate between them every tick.
    Unit* SelectByEntry(Unit* currentTarget, uint32 entry, std::vector<Unit*> const& candidates) const;

    bool IsAllowedTarget(Unit* unit, Unit* xt002) const;

    Unit* ResolveTarget(Unit* currentTarget);
};

#endif

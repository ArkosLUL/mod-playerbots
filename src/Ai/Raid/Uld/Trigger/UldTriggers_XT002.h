#ifndef PLAYERBOTS_ULDTRIGGERS_XT002_H
#define PLAYERBOTS_ULDTRIGGERS_XT002_H

#include "GenericTriggers.h"
#include "Trigger.h"
#include "UldBossHelper.h"

//
// XT-002 Deconstructor
//
// Every trigger here returns false as soon as XT is not around, so the shared Ulduar strategy stays
// inert during the raid's other thirteen encounters.
//

// This bot carries Searing Light, Gravity Bomb, or both. One trigger for both debuffs because one
// action has to own the bot: a bot that draws both while two nodes share a relevance gets pulled
// between their destinations and drops its Void Zone somewhere in the middle. Nobody steps aside for
// a carrier either - the raid holds still and the carrier solves its own mechanic.
class XT002DebuffCarrierTrigger : public Trigger
{
public:
    XT002DebuffCarrierTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 debuff carrier trigger") {}
    bool IsActive() override;
};

// A Boombot explodes for 15-18k when it reaches XT or drops to 50% health, so melee never stand next
// to one; ranged kill it from outside the blast instead, via the DPS priority action. Void Zones are
// the carrier action's business while the bot is carrying, so this only reports them once it is not.
class XT002AvoidHazardTrigger : public Trigger
{
public:
    XT002AvoidHazardTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 avoid hazard trigger") {}
    bool IsActive() override;
};

// Anchors the main tank, ranged DPS and healers, but only once XT is engaged: the anchors are combat
// spots, and walking to them before the pull drags bots across the room while the raid is still
// forming up. Goes false the moment anything the bot has to dodge is live, so a spot can never beat a
// mechanic. The tank anchor also needs him to be holding XT - the boss is taunt-immune, so a tank
// parked on the spot without aggro has no way of getting him back. Ranged and healers each answer for
// their own slot in the formation rather than a shared point; the healer branch stands down while
// anyone is out of heal range, since it outranks the node that would go and fetch them.
class XT002RaidPositionTrigger : public Trigger
{
public:
    XT002RaidPositionTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 raid position trigger") {}
    bool IsActive() override;
};

// Every role picks its target from the encounter's priority order, tanks included: the generic smart
// tank targeting ranks any add the tank lacks aggro on above the boss, which drags XT into the add
// pile. Nothing is marked - raid icons are group-global, so setting one here would overwrite whatever
// the player and other bots rely on.
class XT002SetDpsPriorityTrigger : public Trigger
{
public:
    XT002SetDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 set dps priority trigger") {}
    bool IsActive() override;
};

class XT002PummellerTauntTrigger : public Trigger
{
public:
    XT002PummellerTauntTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 pummeller taunt trigger") {}
    bool IsActive() override;
};

class XT002RedirectThreatTrigger : public Trigger
{
public:
    XT002RedirectThreatTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 redirect threat trigger") {}
    bool IsActive() override;
};

#endif

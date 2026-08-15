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

// Another raid member within splash range carries Searing Light. The carrier itself cannot escape
// its own splash, so it is excluded and only the neighbours move.
class XT002SearingLightSpreadTrigger : public Trigger
{
public:
    XT002SearingLightSpreadTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 searing light spread trigger") {}
    bool IsActive() override;
};

class XT002GravityBombSpreadTrigger : public Trigger
{
public:
    XT002GravityBombSpreadTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 gravity bomb spread trigger") {}
    bool IsActive() override;
};

// This bot carries Gravity Bomb: it has to clear the raid itself, both for the splash and because
// in hard mode the Void Zone drops where the debuff expires.
class XT002GravityBombCarrierTrigger : public Trigger
{
public:
    XT002GravityBombCarrierTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 gravity bomb carrier trigger") {}
    bool IsActive() override;
};

// This bot carries Searing Light. It leaves for a fixed spot rather than an emergent one, so the rest
// of the raid can hold still and knows where any hard-mode Life Spark is about to appear.
class XT002SearingLightCarrierTrigger : public Trigger
{
public:
    XT002SearingLightCarrierTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 searing light carrier trigger") {}
    bool IsActive() override;
};

// A Boombot explodes for 15-18k when it reaches XT or drops to 50% health, so melee never stand next
// to one. Ranged kill it from outside the blast instead, via the DPS priority action.
class XT002BoombotAvoidTrigger : public Trigger
{
public:
    XT002BoombotAvoidTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 boombot avoid trigger") {}
    bool IsActive() override;
};

class XT002VoidZoneTrigger : public Trigger
{
public:
    XT002VoidZoneTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 void zone trigger") {}
    bool IsActive() override;
};

// Anchors the main tank and ranged DPS. Goes false the moment anything the bot has to dodge is live,
// so walking back to a spot can never compete with a mechanic.
class XT002RaidPositionTrigger : public Trigger
{
public:
    XT002RaidPositionTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 raid position trigger") {}
    bool IsActive() override;
};

// Non-tanks pick their own target from the encounter's priority order. Nothing is marked: raid icons
// are group-global, so setting one here would overwrite whatever the player and other bots rely on.
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

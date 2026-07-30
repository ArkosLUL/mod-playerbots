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

// A Boombot explodes for 15-18k when it reaches XT or drops to 50% health, so melee never stand next
// to one. Ranged kill it from outside the blast instead (xt002 boombot ranged kill trigger).
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

class XT002MarkKillTargetTrigger : public Trigger
{
public:
    XT002MarkKillTargetTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 mark kill target trigger") {}
    bool IsActive() override;
};

class XT002AttackKillTargetTrigger : public Trigger
{
public:
    XT002AttackKillTargetTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 attack kill target trigger") {}
    bool IsActive() override;
};

class XT002BoombotRangedKillTrigger : public Trigger
{
public:
    XT002BoombotRangedKillTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 boombot ranged kill trigger") {}
    bool IsActive() override;
};

// The exposed Heart is the encounter's damage multiplier. In normal mode bots stop at
// ULDUAR_XT002_HEART_SAFE_HP_PCT so they never flip the raid into hard mode by accident.
class XT002AttackHeartTrigger : public Trigger
{
public:
    XT002AttackHeartTrigger(PlayerbotAI* ai) : Trigger(ai, "xt002 attack heart trigger") {}
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

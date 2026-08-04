/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// OnyxiaTriggers.h
#ifndef PLAYERBOTS_ONYTRIGGERS_H
#define PLAYERBOTS_ONYTRIGGERS_H

#include "PlayerbotAI.h"
#include "RaidAntiFear.h"
#include "Trigger.h"

constexpr float ONYXIA_PHASE_3_HEALTH_PCT = 40.0f;

// True while Bellowing Roar (18431) can land, i.e. from the phase 3 landing at 40% onwards.
bool OnyxiaBellowingRoarWindowActive(PlayerbotAI* botAI);

// Mechanics
class OnyxiaDeepBreathTrigger : public Trigger
{
public:
    OnyxiaDeepBreathTrigger(PlayerbotAI* botAI);
    bool IsActive() override;
};

class OnyxiaNearTailTrigger : public Trigger
{
public:
    OnyxiaNearTailTrigger(PlayerbotAI* botAI);
    bool IsActive() override;
};

class RaidOnyxiaFireballSplashTrigger : public Trigger
{
public:
    RaidOnyxiaFireballSplashTrigger(PlayerbotAI* botAI);
    bool IsActive() override;
};

class RaidOnyxiaWhelpsSpawnTrigger : public Trigger
{
public:
    RaidOnyxiaWhelpsSpawnTrigger(PlayerbotAI* botAI);
    bool IsActive() override;
};

class OnyxiaAvoidEggsTrigger : public Trigger
{
public:
    OnyxiaAvoidEggsTrigger(PlayerbotAI* botAI);
    bool IsActive() override;
};

class OnyxiaAntiFearTrigger : public RaidAntiFearTrigger
{
public:
    OnyxiaAntiFearTrigger(PlayerbotAI* botAI);

protected:
    bool FearWindowActive() override;
};

#endif

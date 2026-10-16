/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ONYTRIGGERS_H
#define PLAYERBOTS_ONYTRIGGERS_H

#include "RaidAntiFear.h"
#include "Trigger.h"

// Egg, breath lane, Blast Nova or Fireball splash. Shares OnyHelpers::DodgeReasons with the rule that
// hands it the bot's movement, so the two never disagree.
class OnyxiaDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "onyxia dodge";

    OnyxiaDodgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OnyxiaMainTankCooldownTrigger : public Trigger
{
public:
    static constexpr char const* Name = "onyxia main tank cooldown";

    OnyxiaMainTankCooldownTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OnyxiaMainTankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "onyxia main tank";

    OnyxiaMainTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

// Bellowing Roar from the moment she starts coming down to the end of the fight.
class OnyxiaAntiFearTrigger : public RaidAntiFearTrigger
{
public:
    static constexpr char const* Name = "onyxia anti fear";

    OnyxiaAntiFearTrigger(PlayerbotAI* botAI) : RaidAntiFearTrigger(botAI, Name) {}

protected:
    bool FearWindowActive() override;
};

class OnyxiaRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "onyxia redirect threat";

    OnyxiaRedirectThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OnyxiaChooseTargetTrigger : public Trigger
{
public:
    static constexpr char const* Name = "onyxia choose target";

    OnyxiaChooseTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OnyxiaGroundSlotTrigger : public Trigger
{
public:
    static constexpr char const* Name = "onyxia ground slot";

    OnyxiaGroundSlotTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OnyxiaAirSpotTrigger : public Trigger
{
public:
    static constexpr char const* Name = "onyxia air spot";

    OnyxiaAirSpotTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OnyxiaRearFlankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "onyxia rear flank";

    OnyxiaRearFlankTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif

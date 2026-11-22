/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSTRIGGERS_H
#define PLAYERBOTS_OSTRIGGERS_H

#include "OSHelpers.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class SartharionDpsTrigger : public Trigger
{
public:
    SartharionDpsTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sartharion dps") {}
    bool IsActive() override;
};

class SartharionMeleePositioningTrigger : public Trigger
{
public:
    static constexpr char const* Name = "sartharion melee positioning";

    SartharionMeleePositioningTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsTsunamiCorridorTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os tsunami corridor";

    OsTsunamiCorridorTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsTwilightFissureTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os twilight fissure";

    OsTwilightFissureTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsMainTankHoldTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os main tank hold";

    OsMainTankHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsDrakeLandingTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os drake landing";

    OsDrakeLandingTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsOffTankHoldTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os offtank hold";

    OsOffTankHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os redirect threat";

    OsRedirectThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsMainTankCooldownTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os main tank cooldown";

    OsMainTankCooldownTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsTranquilizeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os tranquilize";

    OsTranquilizeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsRaidHoldTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os raid hold";

    OsRaidHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsSartharionFlankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os sartharion flank";

    OsSartharionFlankTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsDrakeRearTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os drake rear";

    OsDrakeRearTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsTankShapeshiftTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os tank shapeshift";

    OsTankShapeshiftTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class OsOffPlatformTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os off platform";

    OsOffPlatformTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class TwilightPortalEnterTrigger : public Trigger
{
public:
    static constexpr char const* Name = "twilight portal enter";

    TwilightPortalEnterTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class TwilightPortalExitTrigger : public Trigger
{
public:
    TwilightPortalExitTrigger(PlayerbotAI* botAI) : Trigger(botAI, "twilight portal exit") {}
    bool IsActive() override;
};

#endif

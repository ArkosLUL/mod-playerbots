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

// Only gates the arc positioning. RearFlankAction does the angle maths and stops re-issuing once the
// bot is inside the safe band, so all this has to decide is who arcs: melee who are not holding
// something, i.e. neither tank.
class SartharionMeleePositioningTrigger : public Trigger
{
public:
    static constexpr char const* Name = "sartharion melee positioning";

    SartharionMeleePositioningTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

// Fires on the tsunami creature existing, not on its damage aura going up at +3.6s. Waiting for the
// aura spends the entire reaction window before anyone moves.
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

// Melee on a drake. Its cone is frontal only, so the rear is free and the shared "rear flank" band at
// 90-120 degrees leaves them on the side of it for nothing.
class OsDrakeRearTrigger : public Trigger
{
public:
    static constexpr char const* Name = "os drake rear";

    OsDrakeRearTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

// A druid tank out of bear form is wearing cloth against a drake. He is the one bot in this fight that
// can be out of combat while it runs - the off-tank holds nothing between drakes - and out of combat
// every druid buff, heal and revive node pulls "caster form" as a prerequisite.
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

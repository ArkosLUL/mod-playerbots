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
    SartharionMeleePositioningTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sartharion melee positioning") {}
    bool IsActive() override;
};

// Fires on the tsunami creature existing, not on its damage aura going up at +3.6s. Waiting for the
// aura spends the entire reaction window before anyone moves.
class OsTsunamiCorridorTrigger : public Trigger
{
public:
    OsTsunamiCorridorTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os tsunami corridor") {}
    bool IsActive() override;
};

class OsTwilightFissureTrigger : public Trigger
{
public:
    OsTwilightFissureTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os twilight fissure") {}
    bool IsActive() override;
};

class OsMainTankHoldTrigger : public Trigger
{
public:
    OsMainTankHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os main tank hold") {}
    bool IsActive() override;
};

class OsDrakeLandingTrigger : public Trigger
{
public:
    OsDrakeLandingTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os drake landing") {}
    bool IsActive() override;
};

class OsOffTankHoldTrigger : public Trigger
{
public:
    OsOffTankHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os offtank hold") {}
    bool IsActive() override;
};

class OsRedirectThreatTrigger : public Trigger
{
public:
    OsRedirectThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os redirect threat") {}
    bool IsActive() override;
};

class OsMainTankCooldownTrigger : public Trigger
{
public:
    OsMainTankCooldownTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os main tank cooldown") {}
    bool IsActive() override;
};

class OsTranquilizeTrigger : public Trigger
{
public:
    OsTranquilizeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os tranquilize") {}
    bool IsActive() override;
};

class OsRaidHoldTrigger : public Trigger
{
public:
    OsRaidHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os raid hold") {}
    bool IsActive() override;
};

class OsSartharionFlankTrigger : public Trigger
{
public:
    OsSartharionFlankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os sartharion flank") {}
    bool IsActive() override;
};

// Melee on a drake. Its cone is frontal only, so the rear is free and the shared "rear flank" band at
// 90-120 degrees leaves them on the side of it for nothing.
class OsDrakeRearTrigger : public Trigger
{
public:
    OsDrakeRearTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os drake rear") {}
    bool IsActive() override;
};

// A druid tank out of bear form is wearing cloth against a drake. He is the one bot in this fight that
// can be out of combat while it runs - the off-tank holds nothing between drakes - and out of combat
// every druid buff, heal and revive node pulls "caster form" as a prerequisite.
class OsTankShapeshiftTrigger : public Trigger
{
public:
    OsTankShapeshiftTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os tank shapeshift") {}
    bool IsActive() override;
};

class OsOffPlatformTrigger : public Trigger
{
public:
    OsOffPlatformTrigger(PlayerbotAI* botAI) : Trigger(botAI, "os off platform") {}
    bool IsActive() override;
};

class TwilightPortalEnterTrigger : public Trigger
{
public:
    TwilightPortalEnterTrigger(PlayerbotAI* botAI) : Trigger(botAI, "twilight portal enter") {}
    bool IsActive() override;
};

class TwilightPortalExitTrigger : public Trigger
{
public:
    TwilightPortalExitTrigger(PlayerbotAI* botAI) : Trigger(botAI, "twilight portal exit") {}
    bool IsActive() override;
};

#endif

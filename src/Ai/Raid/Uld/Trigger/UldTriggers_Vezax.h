#ifndef PLAYERBOTS_ULDTRIGGERS_VEZAX_H
#define PLAYERBOTS_ULDTRIGGERS_VEZAX_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// General Vezax
//
class VezaxCheatTrigger : public Trigger
{
public:
    VezaxCheatTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax cheat trigger") {}
    bool IsActive() override;
};

class VezaxShadowCrashTrigger : public Trigger
{
public:
    VezaxShadowCrashTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax shadow crash trigger") {}
    bool IsActive() override;
};

class VezaxMarkOfTheFacelessTrigger : public Trigger
{
public:
    VezaxMarkOfTheFacelessTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax mark of the faceless trigger") {}
    bool IsActive() override;
};

class VezaxSaroniteVaporsTrigger : public Trigger
{
public:
    VezaxSaroniteVaporsTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax saronite vapors trigger") {}
    bool IsActive() override;
};

// Hard mode: Saronite Animus alive, bot not already attacking it.
class VezaxSaroniteAnimusTrigger : public Trigger
{
public:
    VezaxSaroniteAnimusTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax saronite animus trigger") {}
    bool IsActive() override;
};

// Hard mode: ranged/healer standing inside the Animus' Profound Darkness radius.
class VezaxProfoundDarknessTrigger : public Trigger
{
public:
    VezaxProfoundDarknessTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax profound darkness trigger") {}
    bool IsActive() override;
};

#endif

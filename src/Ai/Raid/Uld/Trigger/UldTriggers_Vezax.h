#ifndef PLAYERBOTS_ULDTRIGGERS_VEZAX_H
#define PLAYERBOTS_ULDTRIGGERS_VEZAX_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "UldEncounter_Vezax.h"
#include "Trigger.h"

//
// General Vezax
//

// Vezax gone but this instance still holds slot assignments.
class VezaxResetEncounterStateTrigger : public Trigger
{
public:
    VezaxResetEncounterStateTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax reset encounter state", 5) {}
    bool IsActive() override;
};

class VezaxMarkOfTheFacelessTrigger : public Trigger
{
public:
    VezaxMarkOfTheFacelessTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax mark of the faceless") {}
    bool IsActive() override;
};

// Riding a Saronite Vapors puddle past the point where the next tick is worth the mana.
class VezaxVaporPuddleClearTrigger : public Trigger
{
public:
    VezaxVaporPuddleClearTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax vapor puddle clear") {}
    bool IsActive() override;
};

// Healer standing in a field. Melee and tanks are deliberately left alone: the field cannot hurt
// them, so moving them out would only cost uptime.
class VezaxShadowCrashClearTrigger : public Trigger
{
public:
    VezaxShadowCrashClearTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax shadow crash clear") {}
    bool IsActive() override;
};

class VezaxSearingFlamesInterruptTrigger : public Trigger
{
public:
    VezaxSearingFlamesInterruptTrigger(PlayerbotAI* ai)
        : Trigger(ai, "vezax searing flames interrupt")
    {
    }
    bool IsActive() override;
};

class VezaxSurgeOfDarknessTrigger : public Trigger
{
public:
    VezaxSurgeOfDarknessTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax surge of darkness") {}
    bool IsActive() override;
};

// Hard mode: Saronite Animus alive, bot not already attacking it.
class VezaxSaroniteAnimusTrigger : public Trigger
{
public:
    VezaxSaroniteAnimusTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax saronite animus") {}
    bool IsActive() override;
};

// Normal mode only: killing a vapor is what creates the raid's mana, and in hard mode it is what
// destroys the hard mode.
class VezaxVaporSoakTrigger : public Trigger
{
public:
    VezaxVaporSoakTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax vapor soak", 2) {}
    bool IsActive() override;
};

class VezaxKillVaporTrigger : public Trigger
{
public:
    VezaxKillVaporTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax kill vapor") {}
    bool IsActive() override;
};

// Mana caster with a Shadow Crash field in walking range. The field is the fight's damage and mana
// answer, so this moves bots in rather than out.
class VezaxShadowCrashSoakTrigger : public Trigger
{
public:
    // Two seconds, not every tick: this is the one Vezax trigger that has to sweep the grid, and the
    // field lands on a 10s cadence and lasts 20s, so reacting a little late costs almost nothing.
    VezaxShadowCrashSoakTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax shadow crash soak", 2) {}
    bool IsActive() override;
};

class VezaxRaidPositionTrigger : public Trigger
{
public:
    VezaxRaidPositionTrigger(PlayerbotAI* ai) : Trigger(ai, "vezax raid position", 2) {}
    bool IsActive() override;
};

#endif

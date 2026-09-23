#ifndef PLAYERBOTS_ULDTRIGGERS_VEZAX_H
#define PLAYERBOTS_ULDTRIGGERS_VEZAX_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldEncounter_Vezax.h"
#include "Trigger.h"

//
// General Vezax
//

// Vezax gone but this instance still holds slot assignments.
class VezaxResetEncounterStateTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax reset encounter state";

    VezaxResetEncounterStateTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 5) {}
    bool IsActive() override;
};

class VezaxMarkOfTheFacelessTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax mark of the faceless";

    VezaxMarkOfTheFacelessTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Standing inside the leech around someone else's mark. Melee only: the camp is far enough out that
// nothing marked in the ball reaches it.
class VezaxMarkOfTheFacelessBreakTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax mark of the faceless break";

    VezaxMarkOfTheFacelessBreakTrigger(PlayerbotAI* ai)
        : Trigger(ai, Name)
    {
    }
    bool IsActive() override;
};

// A Shadow Crash missile in flight with this bot standing where it will land. Melee and the tank are
// left out because they hold the boss, not because they are safe: SelectTarget only skips what is
// inside 3 yd plus both combat reaches, about 12.5 yd, and a melee bot sits right on that line - one
// traced pull crashed a rogue at 16.1 yd and caught three more melee with it.
class VezaxShadowCrashDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax shadow crash dodge";

    VezaxShadowCrashDodgeTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class VezaxSearingFlamesInterruptTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax searing flames interrupt";

    VezaxSearingFlamesInterruptTrigger(PlayerbotAI* ai)
        : Trigger(ai, Name)
    {
    }
    bool IsActive() override;
};

class VezaxSurgeOfDarknessTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax surge of darkness";

    VezaxSurgeOfDarknessTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Hard mode: Saronite Animus alive, bot not already attacking it.
class VezaxSaroniteAnimusTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax saronite animus";

    VezaxSaroniteAnimusTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Mana caster with a Shadow Crash field in walking range. The field is the fight's damage and mana
// answer, so this moves bots in rather than out.
class VezaxShadowCrashSoakTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax shadow crash soak";

    // Two seconds, not every tick: this is the one Vezax trigger that has to sweep the grid, and the
    // field lands on a 10s cadence and lasts 20s, so reacting a little late costs almost nothing.
    VezaxShadowCrashSoakTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2) {}
    bool IsActive() override;
};

// Not on the boss and not on the Animus. The target guard silences the generic pickers here, so this
// is the only thing that hands a bot a target at all - and a bot with none never calls Attack, never
// reaches the combat engine and spends the fight buffing.
class VezaxHoldTargetTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax hold target";

    VezaxHoldTargetTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class VezaxRaidPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax raid position";

    VezaxRaidPositionTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2) {}
    bool IsActive() override;
};

#endif

#ifndef PLAYERBOTS_ULDTRIGGERS_VEZAX_H
#define PLAYERBOTS_ULDTRIGGERS_VEZAX_H

#include "BurstCooldowns.h"
#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldEncounter_Vezax.h"
#include "Trigger.h"

class VezaxResetEncounterStateTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax reset encounter state";

    VezaxResetEncounterStateTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name, 5000) {}
    bool IsActive() override;
};

class VezaxMarkOfTheFacelessTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax mark of the faceless";

    VezaxMarkOfTheFacelessTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class VezaxMarkOfTheFacelessBreakTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax mark of the faceless break";

    VezaxMarkOfTheFacelessBreakTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, Name)
    {
    }
    bool IsActive() override;
};

class VezaxShadowCrashDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax shadow crash dodge";

    VezaxShadowCrashDodgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class VezaxSearingFlamesInterruptTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax searing flames interrupt";

    VezaxSearingFlamesInterruptTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, Name)
    {
    }
    bool IsActive() override;
};

class VezaxSurgeOfDarknessTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax surge of darkness";

    VezaxSurgeOfDarknessTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class VezaxLateSurgeExternalTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax late surge external";

    VezaxLateSurgeExternalTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class VezaxSaroniteAnimusTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax saronite animus";

    VezaxSaroniteAnimusTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;

private:
    BurstHoldState tankHold;
};

class VezaxAnimusRedirectTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax animus redirect";

    VezaxAnimusRedirectTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class VezaxAnimusBringBackTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax animus bring back";

    VezaxAnimusBringBackTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class VezaxAnimusMeleeSpotTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax animus melee spot";

    VezaxAnimusMeleeSpotTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class VezaxShadowCrashSoakTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax shadow crash soak";

    VezaxShadowCrashSoakTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name, 2000) {}
    bool IsActive() override;
};

class VezaxHoldTargetTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax hold target";

    VezaxHoldTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class VezaxDamageHoldTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax damage hold";

    VezaxDamageHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class VezaxTankBuffTrigger : public Trigger
{
public:
    VezaxTankBuffTrigger(PlayerbotAI* botAI, std::string const name, std::string const spell)
        : Trigger(botAI, name), spell(spell)
    {
    }
    bool IsActive() override;

private:
    std::string spell;
};

class VezaxRaidPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "vezax raid position";

    VezaxRaidPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name, 2000) {}
    bool IsActive() override;
};

#endif

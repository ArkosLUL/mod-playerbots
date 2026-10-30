/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_BLOODQUEEN_H
#define PLAYERBOTS_ICCTRIGGERS_BLOODQUEEN_H

#include "ICCHelpers_BloodQueen.h"
#include "ICCTriggers_RaidWide.h"
#include "RaidAntiFear.h"
#include "Trigger.h"

class IccBqlDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql dodge";

    IccBqlDodgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlBiteTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql vampiric bite";

    IccBqlBiteTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlShadowsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql swarming shadows";

    IccBqlShadowsTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlPactTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql pact of darkfallen";

    IccBqlPactTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlWhirlDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql whirl defensive";

    IccBqlWhirlDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlWhirlCoverTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql whirl cover";

    IccBqlWhirlCoverTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql tank defensive";

    IccBqlTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlAntiFearTrigger : public RaidAntiFearTrigger
{
public:
    static constexpr char const* Name = "icc bql anti fear";

    IccBqlAntiFearTrigger(PlayerbotAI* botAI) : RaidAntiFearTrigger(botAI, Name) {}
    bool IsActive() override;

protected:
    bool FearWindowActive() override;
};

class IccBqlMassDispelTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql mass dispel";

    IccBqlMassDispelTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlFrenzyCcTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql frenzy cc";

    IccBqlFrenzyCcTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql taunt";

    IccBqlTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlBrakeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql brake";

    IccBqlBrakeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc bql target";

    IccBqlTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetBqlTarget) {}
};

class IccBqlStandTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql stand";

    IccBqlStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif

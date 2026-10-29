/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_SINDRAGOSAADDS_H
#define PLAYERBOTS_ICCTRIGGERS_SINDRAGOSAADDS_H

#include "ICCHelpers_SindragosaAdds.h"
#include "ICCTriggers_RaidWide.h"
#include "RaidAntiFear.h"
#include "Trigger.h"

class IccSgDrakeDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa drake dodge";

    IccSgDrakeDodgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSgDrakeTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa drake tank defensive";

    IccSgDrakeTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSgDrakeAntiFearTrigger : public RaidAntiFearTrigger
{
public:
    static constexpr char const* Name = "icc sindragosa drake anti fear";

    IccSgDrakeAntiFearTrigger(PlayerbotAI* botAI) : RaidAntiFearTrigger(botAI, Name) {}

protected:
    bool FearWindowActive() override;
};

class IccSgDrakeBrakeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa drake brake";

    IccSgDrakeBrakeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSgDrakeTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc sindragosa drake target";

    IccSgDrakeTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetSgDrakeTarget) {}
};

class IccSgDrakeStandTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa drake stand";

    IccSgDrakeStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSgGauntletDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa gauntlet dodge";

    IccSgGauntletDodgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSgGauntletTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa gauntlet taunt";

    IccSgGauntletTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSgGauntletTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc sindragosa gauntlet target";

    IccSgGauntletTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetSgGauntletTarget) {}
};

#endif

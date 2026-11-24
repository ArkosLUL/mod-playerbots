/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_YOGGSARON_H
#define PLAYERBOTS_ULDMULTIPLIERS_YOGGSARON_H

#include "Define.h"
#include "Multiplier.h"
#include "RaidAntiFear.h"
#include "RaidSplash.h"

class CastSpellAction;
class Unit;

class YoggSaronMovementGuardMultiplier : public Multiplier
{
public:
    YoggSaronMovementGuardMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron movement guard multiplier")
    {
    }

    float GetValue(Action* action) override;

private:
    float FleeGuard();
    float SetBehindGuard();
    bool ReachIsWrong(Action* action);
    bool CrusherReachIsWrong();
};

class YoggSaronPhase1AoeHoldMultiplier : public Multiplier
{
public:
    YoggSaronPhase1AoeHoldMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron phase 1 aoe hold multiplier")
    {
    }

    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

class YoggSaronIllusionMultiTargetHoldMultiplier : public Multiplier
{
public:
    YoggSaronIllusionMultiTargetHoldMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron illusion multi target hold multiplier")
    {
    }

    float GetValue(Action* action) override;
};

class YoggSaronExitHoldCastMultiplier : public Multiplier
{
public:
    YoggSaronExitHoldCastMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron exit hold cast multiplier")
    {
    }

    float GetValue(Action* action) override;
};

class YoggSaronHeldTargetCastMultiplier : public Multiplier
{
public:
    YoggSaronHeldTargetCastMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron held target cast multiplier")
    {
    }

    float GetValue(Action* action) override;

private:
    bool GuardianThreatHeld(CastSpellAction* spellAction, Unit* target);
};

class YoggSaronPhase3AreaMultiplier : public Multiplier
{
public:
    YoggSaronPhase3AreaMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "yogg-saron phase 3 area multiplier") {}

    float GetValue(Action* action) override;
};

class YoggSaronCrusherAreaHoldMultiplier : public Multiplier
{
public:
    YoggSaronCrusherAreaHoldMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron crusher area hold multiplier")
    {
    }

    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

class YoggSaronPhase1WalkGuardMultiplier : public Multiplier
{
public:
    YoggSaronPhase1WalkGuardMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron phase 1 walk guard multiplier")
    {
    }

    float GetValue(Action* action) override;

private:
    bool WalkCrossesCloud(bool station, bool reachMelee);
};

class YoggSaronAntiFearTotemGuardMultiplier : public RaidAntiFearTotemGuardMultiplier
{
public:
    YoggSaronAntiFearTotemGuardMultiplier(PlayerbotAI* botAI)
        : RaidAntiFearTotemGuardMultiplier(botAI, "yogg-saron anti fear totem guard multiplier")
    {
    }

protected:
    bool FearWindowActive() override;
};

#endif

/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDANTIFEAR_H
#define PLAYERBOTS_RAIDANTIFEAR_H

#include "Action.h"
#include "Define.h"
#include "Multiplier.h"
#include "Trigger.h"

class Player;
class PlayerbotAI;

constexpr uint32 SPELL_FEAR_WARD = 6346;

// Preferred Fear Ward target: the main tank, else the first alive healer in range without the ward.
// Returns nullptr when nobody needs it or the priest cannot cast it right now (Fear Ward is on a
// cooldown, so one priest covers one target per window).
Player* GetAntiFearWardTarget(PlayerbotAI* botAI, Player* bot);

// True when this shaman knows Tremor Totem, does not already have one out and can cast it now.
bool ShouldDropTremorTotem(PlayerbotAI* botAI, Player* bot);

// True when the bot has an anti-fear counter it can actually use right now.
bool RaidAntiFearReady(PlayerbotAI* botAI, Player* bot);

// Shared anti-fear component. An encounter subclasses the three types below and supplies only the
// window in which the boss's fear can land; the class split, the readiness checks and the earth
// totem slot guard are the same everywhere.
//
// Deliberately no ChangeStrategy("+tremor"): the shaman totem strategies pick the earth totem by
// first match out of {strength of earth, stoneskin, tremor, earthbind}, so adding "tremor" never
// wins the slot and the strategy leaks past the instance. Casting directly and zeroing the
// competing earth totem actions for the length of the window is self-reverting and keeps the totem
// down.
class RaidAntiFearTrigger : public Trigger
{
public:
    RaidAntiFearTrigger(PlayerbotAI* botAI, std::string const name) : Trigger(botAI, name) {}
    bool IsActive() override;

protected:
    virtual bool FearWindowActive() = 0;
};

class RaidAntiFearAction : public Action
{
public:
    RaidAntiFearAction(PlayerbotAI* botAI, std::string const name) : Action(botAI, name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

protected:
    virtual bool FearWindowActive() = 0;
};

class RaidAntiFearTotemGuardMultiplier : public Multiplier
{
public:
    RaidAntiFearTotemGuardMultiplier(PlayerbotAI* botAI, std::string const name) : Multiplier(botAI, name) {}
    float GetValue(Action* action) override;

protected:
    virtual bool FearWindowActive() = 0;
};

#endif

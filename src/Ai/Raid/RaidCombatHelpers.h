/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDCOMBATHELPERS_H
#define PLAYERBOTS_RAIDCOMBATHELPERS_H

#include <functional>
#include <string>
#include <unordered_map>

#include "Define.h"

class Action;
class CastSpellAction;
class Player;
class PlayerbotAI;
class RaidSplashCache;
class Unit;
template <class T>
class Value;

// Not a tank by role or spec, not a healer. Every DPS resolver and its targeting rule test this one,
// so the two can't drift apart.
bool IsRaidDpsBot(Player* bot);

// An action whose damage lands on everything around its target or the bot, so it threatens a boss
// it isn't aimed at
bool IsRaidAreaDamage(PlayerbotAI* botAI, Action* action);
// Same answer and the same "spell id" reads, with the per-action lookups kept in cache
bool IsRaidAreaDamage(PlayerbotAI* botAI, Action* action, RaidSplashCache& cache);

// Casts the first heal this bot can land on target: big instants, then cast-time heals, HoTs last. A HoT
// it already has ticking there is skipped, and so is any heal skip() turns down.
bool CastRaidThroughputHeal(PlayerbotAI* botAI, Unit* target,
                            std::function<bool(std::string const&)> const& skip = nullptr);

// What a multiplier works out about an action that can't change once the action is built: its type,
// name, threat type. Keep one per multiplier: an Action* is only unique inside one bot's AI, whose
// actions outlive every multiplier built from it.
template <class Facts>
class RaidActionCache
{
public:
    template <class Compute>
    Facts& Get(Action* action, Compute&& compute)
    {
        if (_last && action == _lastAction)
            return *_last;

        auto found = _facts.find(action);
        if (found == _facts.end())
            found = _facts.emplace(action, compute()).first;

        _lastAction = action;
        _last = &found->second;
        return found->second;
    }

private:
    std::unordered_map<Action*, Facts> _facts;
    Action* _lastAction = nullptr;
    Facts* _last = nullptr;
};

// A cast action's "spell id" value without the context lookup on every read. Get() still runs every
// time so the value refreshes on its own clock. Fetched again when the spell changes: PullAction
// swaps its spell at run time.
class RaidSpellIdRef
{
public:
    uint32 Get(PlayerbotAI* botAI, CastSpellAction* cast);

private:
    Value<uint32>* _value = nullptr;
    std::string _spell;
};

#endif

/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDCOMBATHELPERS_H
#define PLAYERBOTS_RAIDCOMBATHELPERS_H

#include <functional>
#include <string>

class Action;
class Player;
class PlayerbotAI;
class Unit;

// Not a tank by role or spec, not a healer. Every DPS resolver and its targeting rule test this one,
// so the two can't drift apart.
bool IsRaidDpsBot(Player* bot);

// An action whose damage lands on everything around its target or the bot, so it threatens a boss
// it isn't aimed at
bool IsRaidAreaDamage(PlayerbotAI* botAI, Action* action);

// Casts the first heal this bot can land on target: big instants, then cast-time heals, HoTs last. A HoT
// it already has ticking there is skipped, and so is any heal skip() turns down.
bool CastRaidThroughputHeal(PlayerbotAI* botAI, Unit* target,
                            std::function<bool(std::string const&)> const& skip = nullptr);

#endif

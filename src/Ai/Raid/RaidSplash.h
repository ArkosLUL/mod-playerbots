/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDSPLASH_H
#define PLAYERBOTS_RAIDSPLASH_H

#include <set>
#include <string>

class Action;
class PlayerbotAI;

// Whether the action's spell lands on an area. The threat type doesn't say: CastHealingSpellAction
// declares Aoe, while whirlwind, divine storm, consecration, death and decay, swipe and pestilence
// declare something else and hit everything nearby anyway. IsTargetingArea covers the area and cone
// selection categories, IsAffectingArea adds the persistent area auras. Heals are never counted.
bool SpellSplashes(PlayerbotAI* botAI, Action* action);

// The totem and trap summons whose area lives on what they summon: Magma Totem, Fire Nova, Explosive Trap
// and its Trap Launcher. Their own spell reads as positive.
bool SummonSplashes(Action* action);

// Cleaves, chains and melee area abilities, by action name. Sweeping strikes, cleave, blade flurry,
// bladestorm, multi-shot and chain lightning carry no area in their own spell, so the spell test misses
// them.
extern std::set<std::string> const MULTI_TARGET_ACTIONS;

#endif

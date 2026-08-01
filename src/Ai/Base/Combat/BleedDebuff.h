/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BLEEDDEBUFF_H
#define PLAYERBOTS_BLEEDDEBUFF_H

class Player;
class Unit;

// Any bleed on the target, whoever applied it. Matches on MECHANIC_BLEED rather than a spell list, so
// Rend, Deep Wounds, Rip, Rake, Lacerate, Garrote and Rupture all count without maintenance.
bool TargetHasBleed(Unit* target);

// True when a group member other than the bot is the one bleeding the target. Pets are not resolved
// back to their owner, so a hunter or warlock pet bleed does not count.
bool GroupSuppliesBleedOn(Player* bot, Unit* target);

#endif

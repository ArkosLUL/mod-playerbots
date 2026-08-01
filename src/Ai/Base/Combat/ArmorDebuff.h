/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ARMORDEBUFF_H
#define PLAYERBOTS_ARMORDEBUFF_H

class Player;
class PlayerbotAI;
class Unit;

// Sunder Armor, Expose Armor and a worm pet's Acid Spit all share the same 20% armor slot, so a
// second application overwrites the first and gains the group nothing.
bool TargetHasMajorArmorDebuff(PlayerbotAI* botAI, Unit* target);

// Same slot, minus Sunder Armor. A warrior needs this rather than the check above: its own Sunder is
// visible from the first stack, so testing for any major debuff would abort the 1->5 ramp instantly.
bool TargetHasNonSunderMajorArmorDebuff(PlayerbotAI* botAI, Unit* target);

// True when some other group member's class can supply that debuff. Class-based on purpose: no
// spellbook scan, no talent check. A warrior who is present but never sunders will suppress a
// rogue's Expose Armor, which costs 20% armor in that one case but avoids burning a finisher on a
// debuff the warrior overwrites two seconds later on every other pull.
bool GroupSuppliesMajorArmorDebuff(Player* bot);

#endif

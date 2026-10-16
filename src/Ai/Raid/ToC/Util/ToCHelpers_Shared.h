#ifndef PLAYERBOTS_RAID_TOCHELPERS_SHARED_H
#define PLAYERBOTS_RAID_TOCHELPERS_SHARED_H

#include "PlayerbotAI.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// Nearest alive creature of the given entry within radius of the bot (nullptr if none)
Unit* GetNearestCreatureByEntry(Player* bot, uint32 entry, float radius);

// Average position of all alive creatures of the given entry within radius of the bot.
// Returns false (and leaves center untouched) when none are found. Used to flee away from a
// whole cluster/trail of hazards rather than a single nearest patch.
bool GetCreatureClusterCenter(Player* bot, uint32 entry, float radius, Position& center);

// Not a tank by role or spec, not a healer. Every DPS resolver and its targeting rule test this one,
// so the two can't drift apart.
bool IsToCDpsBot(Player* bot);

// The group skull when it sits on a unit belongs() accepts that is alive, in combat, selectable and
// attackable by this bot. Bots here place no skull, so it's a player's. Null without a group.
Unit* GetToCHumanSkull(PlayerbotAI* botAI, bool (*belongs)(Unit*));

// A living pet CommandPetAttack would send to pick, not on it yet. Resolver triggers fire on this
// too: no generic node sends the pet, only their action does.
bool ToCPetOffPick(PlayerbotAI* botAI, Unit* pick);

// On map 649, puts a bot's rti of cross, square, triangle or diamond back to skull: rti is saved per
// bot and no ToC node reads those tags. rti cc stays, Faction Champions uses it.
void ToCRestoreLegacyRti(PlayerbotAI* botAI);

}

#endif

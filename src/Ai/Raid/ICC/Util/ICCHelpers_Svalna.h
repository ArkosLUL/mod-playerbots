/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_SVALNA_H
#define PLAYERBOTS_ICCHELPERS_SVALNA_H

#include "ICCData.h"
#include "Playerbots.h"

// Aether Shield is up from each Impaling Spear until a Hurl Spear breaks it. Publishes ss.shield.
bool IccSvalnaShielded(Player* bot, Unit* svalna);

#endif

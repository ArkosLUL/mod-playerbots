/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSHELPERS_H
#define PLAYERBOTS_OSHELPERS_H

// Umbrella include for the Obsidian Sanctum helpers. The strategy, trigger, multiplier and action
// files include this one and get all three parts; nothing here declares anything of its own.
//
//   OSData.h       ids, coordinates, tuning constants, entry lists
//   OSGeometry.h   platform bounds, wave lines and corridors, cone tests, drake geometry
//   OSEncounter.h  encounter state, assignments, target selection, tank cooldown and drag latches

#include "OSData.h"
#include "OSEncounter.h"
#include "OSGeometry.h"

#endif

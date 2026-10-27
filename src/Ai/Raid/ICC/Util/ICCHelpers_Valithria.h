/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_VALITHRIA_H
#define PLAYERBOTS_ICCHELPERS_VALITHRIA_H

#include "ICCData.h"
#include "Playerbots.h"
#include <vector>
#include <optional>

inline const Position ICC_VDW_HEAL_POSITION = Position(4203.752f, 2483.4343f, 364.87274f);
inline const Position ICC_VDW_GROUP1_POSITION = Position(4203.585f, 2464.422f, 364.87323f);
inline const Position ICC_VDW_GROUP2_POSITION = Position(4203.5806f, 2505.2383f, 364.87323f);
inline const Position ICC_VDW_PORTALSTART_POSITION = Position(4202.637f, 2488.171f, 375.00256f);

// Every Dream or Nightmare Portal and pre-effect within 100 yd. Publishes vd.wave: open while a real portal
// stands, pre while a pre-effect still shows its 15 s visual, none otherwise.
std::vector<Creature*> IccValithriaPortals(Player* bot);

std::optional<bool> IccValithriaShouldHealRaid(Player* bot);

#endif

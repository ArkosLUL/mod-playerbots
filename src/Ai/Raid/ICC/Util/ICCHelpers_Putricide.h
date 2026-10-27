/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_PUTRICIDE_H
#define PLAYERBOTS_ICCHELPERS_PUTRICIDE_H

#include "ICCData.h"
#include "Playerbots.h"

inline const Position ICC_PUTRICIDE_TANK_POSITION = Position(4373.227f, 3222.058f, 389.4029f);
inline const Position ICC_PUTRICIDE_GREEN_POSITION = Position(4423.4126f, 3194.2715f, 389.37683f);
inline const Position ICC_PUTRICIDE_GATE_POSITION = Position(4356.3345f, 3167.9407f, 389.39825f);

// Putricide - Mutated Abomination vehicle
constexpr uint32 GO_PUTRICIDE_DRINK_ME = 201584;
constexpr uint32 NPC_MUTATED_ABOMINATION_10 = 37672;
constexpr uint32 NPC_MUTATED_ABOMINATION_25 = 38285;
constexpr uint32 SPELL_MUTATED_TRANSFORMATION = 70311;
constexpr uint32 SPELL_ABO_EAT_OOZE = 70346;
constexpr uint32 SPELL_ABO_REGURGITATED_OOZE = 70539;

// Professor Putricide's phase, 1 to 3, from the health his DamageTaken hook tests: phase 2 at 80% and
// below, phase 3 at 35% and below. Publishes pp.phase.
uint8 IccPutricidePhase(Player* bot, Unit* boss);
// Drops the bot's committed Gaseous Bloat flee direction, so the next bloat starts without one.
void IccPutricideForgetBloatFlee(Player* bot);

#endif

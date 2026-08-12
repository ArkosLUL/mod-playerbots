/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSACTIONS_H
#define PLAYERBOTS_OSACTIONS_H

#include "AttackAction.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

// Flame Tsunami crosses as walls of 8-yard segments: the left wave has three (Y 472-496, 520-544,
// 568-592), the right wave two (Y 496-520, 544-568). The two sets are exact complements, so no Y is
// safe from both directions and everyone - tanks included - has to move for one of them. Values
// below are gap midpoints. The left wave leaves two gaps, so melee and ranged split; the right
// leaves one, so everyone shares it.
const float TSUNAMI_LEFT_SAFE_MELEE = 556.0f;
const float TSUNAMI_LEFT_SAFE_RANGED = 508.0f;
const float TSUNAMI_RIGHT_SAFE_ALL = 532.0f;
const std::pair<float, float> SARTHARION_MAINTANK_POSITION = {3258.5f, 532.5f};
const std::pair<float, float> SARTHARION_OFFTANK_POSITION = {3230.0f, 526.0f};
// Sits in range of both tanks (26y to the main tank, 21y to the off-tank) and off Sartharion's front
// axis. Flame Breath is a 60-yard cone reaching 41 degrees either side of his facing, so standing in
// line with the main tank is what gets ranged hit, not standing close.
const std::pair<float, float> SARTHARION_RANGED_POSITION = {3240.0f, 508.0f};

class SartharionTankPositionAction : public AttackAction
{
public:
    SartharionTankPositionAction(PlayerbotAI* botAI, std::string const name = "sartharion tank position")
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;
};

class AvoidTwilightFissureAction : public MovementAction
{
public:
    AvoidTwilightFissureAction(PlayerbotAI* botAI, std::string const name = "avoid twilight fissure")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

class AvoidFlameTsunamiAction : public MovementAction
{
public:
    AvoidFlameTsunamiAction(PlayerbotAI* botAI, std::string const name = "avoid flame tsunami")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

class SartharionAttackPriorityAction : public AttackAction
{
public:
    SartharionAttackPriorityAction(PlayerbotAI* botAI, std::string const name = "sartharion attack priority")
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;
};

class SartharionRangedPositionAction : public MovementAction
{
public:
    SartharionRangedPositionAction(PlayerbotAI* botAI, std::string const name = "sartharion ranged position")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

class EnterTwilightPortalAction : public MovementAction
{
public:
    EnterTwilightPortalAction(PlayerbotAI* botAI, std::string const name = "enter twilight portal")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

class ExitTwilightPortalAction : public MovementAction
{
public:
    ExitTwilightPortalAction(PlayerbotAI* botAI, std::string const name = "exit twilight portal")
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

#endif

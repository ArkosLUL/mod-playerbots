/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDREDIRECTTHREAT_H
#define PLAYERBOTS_RAIDREDIRECTTHREAT_H

#include "Action.h"
#include "AttackAction.h"
#include "Define.h"

class Player;
class PlayerbotAI;
class Unit;

// Proc aura left on the hunter after casting Misdirection (34477); consumed by the next 3 shots.
constexpr uint32 SPELL_MISDIRECTION_PROC = 35079;

// Hunter Misdirection and rogue Tricks of the Trade solve the same problem at the same moments, so
// the per-boss actions below only answer two questions: which tank should own the threat, and which
// unit do we spend the charges on.
class RaidRedirectThreatAction : public AttackAction
{
public:
    RaidRedirectThreatAction(PlayerbotAI* ai, std::string const name) : AttackAction(ai, name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

protected:
    // Either may return nullptr, which just means "nothing to redirect this tick".
    virtual Player* GetRedirectTank() = 0;
    virtual Unit* GetThreatDumpTarget() = 0;

    // Group tank that `target` is currently attacking, if any.
    Player* GetTankHolding(Unit* target);

    // Position of this bot among the group's living redirecters, in group order, so encounters with
    // one tank per boss can hand out different assignments. -1 when the bot cannot redirect.
    int32 GetRedirecterIndex();
};

#endif

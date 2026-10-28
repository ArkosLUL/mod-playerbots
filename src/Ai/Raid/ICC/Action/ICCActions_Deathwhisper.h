/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_DEATHWHISPER_H
#define PLAYERBOTS_ICCACTIONS_DEATHWHISPER_H

#include <vector>

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_Deathwhisper.h"
#include "MovementActions.h"
#include "RaidDirectTargeting.h"
#include "RaidRedirectThreat.h"

// One forced walk to the nearest spot clear of every hazard, left running while its spot stays clear.
class IccDeathwhisperDodgeAction : public MovementAction
{
public:
    IccDeathwhisperDodgeAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    bool Dodge(std::vector<LdwHazard> const& hazards, LdwHazardKind kind);
};

class IccDarkReckoningAction : public IccDeathwhisperDodgeAction
{
public:
    static constexpr char const* Name = "icc dark reckoning";

    IccDarkReckoningAction(PlayerbotAI* botAI) : IccDeathwhisperDodgeAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLadyDeathwhisperDodgeAction : public IccDeathwhisperDodgeAction
{
public:
    static constexpr char const* Name = "icc lady deathwhisper dodge";

    IccLadyDeathwhisperDodgeAction(PlayerbotAI* botAI) : IccDeathwhisperDodgeAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLadyDeathwhisperDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc lady deathwhisper defensive";

    IccLadyDeathwhisperDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLadyDeathwhisperFrostboltAction : public Action
{
public:
    static constexpr char const* Name = "icc lady deathwhisper frostbolt";

    IccLadyDeathwhisperFrostboltAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLadyDeathwhisperDominateMindAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc lady deathwhisper dominate mind";

    IccLadyDeathwhisperDominateMindAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLadyDeathwhisperTauntAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc lady deathwhisper taunt";

    IccLadyDeathwhisperTauntAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLadyDeathwhisperRedirectAction : public RaidRedirectThreatAction
{
public:
    static constexpr char const* Name = "icc lady deathwhisper redirect";

    IccLadyDeathwhisperRedirectAction(PlayerbotAI* botAI) : RaidRedirectThreatAction(botAI, Name) {}

protected:
    Player* GetRedirectTank() override { return LdwRedirectTank(botAI); }
    Unit* GetThreatDumpTarget() override { return LdwBoss(botAI); }
};

class IccLadyDeathwhisperTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc lady deathwhisper target";

    IccLadyDeathwhisperTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &LdwTarget) {}
};

#endif

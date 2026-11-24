/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_VEZAX_H
#define PLAYERBOTS_ULDMULTIPLIERS_VEZAX_H

#include "Define.h"
#include "Multiplier.h"
#include "RaidCombatHelpers.h"
#include "UldEncounter_Vezax.h"

#include <string>
#include <unordered_map>

class VezaxSuppressLifeTapMultiplier : public Multiplier
{
public:
    VezaxSuppressLifeTapMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "vezax suppress life tap multiplier")
    {
    }

    float GetValue(Action* action) override;
};

class VezaxHoldCastOutsideFieldMultiplier : public Multiplier
{
public:
    VezaxHoldCastOutsideFieldMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "vezax hold cast outside field multiplier")
    {
    }

    float GetValue(Action* action) override;
};

class VezaxThreatMultiplier : public Multiplier
{
public:
    VezaxThreatMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "vezax threat multiplier") {}

    float GetValue(Action* action) override;
};

class VezaxTricksOnTankMultiplier : public Multiplier
{
public:
    VezaxTricksOnTankMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "vezax tricks on tank multiplier") {}

    float GetValue(Action* action) override;
};

class VezaxBurstForAnimusMultiplier : public Multiplier
{
public:
    VezaxBurstForAnimusMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "vezax burst for the animus multiplier") {}

    float GetValue(Action* action) override;

private:
    void NoteDecision(std::string const& name, VezaxBurstDecision const& decision);

    RaidActionCache<std::string> burstNames;
    std::unordered_map<std::string, VezaxBurstCall> noted;
    uint32 notedPull = 0;
};

class VezaxDamageHoldMultiplier : public Multiplier
{
public:
    VezaxDamageHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "vezax damage hold multiplier") {}

    float GetValue(Action* action) override;

private:
    enum class Reach : uint8
    {
        None,
        CurrentTarget,
        Area
    };

    std::unordered_map<Action*, Reach> reaches;
};

#endif

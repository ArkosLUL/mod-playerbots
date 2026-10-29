/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_LICHKINGLATE_H
#define PLAYERBOTS_ICCACTIONS_LICHKINGLATE_H

#include "Action.h"
#include "ICCActions_LichKing.h"
#include "ICCHelpers_LichKingLate.h"

class IccLichKingDefileAction : public IccLkWalkAction
{
public:
    static constexpr char const* Name = "icc lich king defile";

    IccLichKingDefileAction(PlayerbotAI* botAI) : IccLkWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingVileSpiritsAction : public IccLkWalkAction
{
public:
    static constexpr char const* Name = "icc lich king vile spirits";

    IccLichKingVileSpiritsAction(PlayerbotAI* botAI) : IccLkWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingFrostmourneAction : public IccLkWalkAction
{
public:
    static constexpr char const* Name = "icc lich king frostmourne";

    IccLichKingFrostmourneAction(PlayerbotAI* botAI) : IccLkWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingSiphonTauntAction : public Action
{
public:
    static constexpr char const* Name = "icc lich king siphon taunt";

    IccLichKingSiphonTauntAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingValkyrControlAction : public Action
{
public:
    static constexpr char const* Name = "icc lich king valkyr control";

    IccLichKingValkyrControlAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingHarvestSoulAction : public Action
{
public:
    static constexpr char const* Name = "icc lich king harvest soul";

    IccLichKingHarvestSoulAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLichKingStackAction : public IccLkWalkAction
{
public:
    static constexpr char const* Name = "icc lich king stack";

    IccLichKingStackAction(PlayerbotAI* botAI) : IccLkWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif

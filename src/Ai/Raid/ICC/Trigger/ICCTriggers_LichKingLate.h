/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_LICHKINGLATE_H
#define PLAYERBOTS_ICCTRIGGERS_LICHKINGLATE_H

#include "ICCHelpers_LichKingLate.h"
#include "Trigger.h"

class IccLichKingDefileTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king defile";

    IccLichKingDefileTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingVileSpiritsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king vile spirits";

    IccLichKingVileSpiritsTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingFrostmourneTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king frostmourne";

    IccLichKingFrostmourneTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingSiphonTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king siphon taunt";

    IccLichKingSiphonTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingValkyrControlTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king valkyr control";

    IccLichKingValkyrControlTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingHarvestSoulTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king harvest soul";

    IccLichKingHarvestSoulTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingStackTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king stack";

    IccLichKingStackTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif

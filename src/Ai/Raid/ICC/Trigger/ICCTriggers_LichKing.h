/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_LICHKING_H
#define PLAYERBOTS_ICCTRIGGERS_LICHKING_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class IccLichKingShadowTrapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king shadow trap";

    IccLichKingShadowTrapTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingNecroticPlagueTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king necrotic plague";

    IccLichKingNecroticPlagueTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingWinterTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king winter";

    IccLichKingWinterTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingAddsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king adds";

    IccLichKingAddsTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingSpiritBombTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king spirit bomb";

    IccLichKingSpiritBombTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif

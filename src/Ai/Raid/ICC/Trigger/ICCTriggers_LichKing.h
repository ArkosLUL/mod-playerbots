/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_LICHKING_H
#define PLAYERBOTS_ICCTRIGGERS_LICHKING_H

#include "ICCHelpers_LichKing.h"
#include "ICCTriggers_RaidWide.h"
#include "Trigger.h"

class IccLichKingQuakeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king quake";

    IccLichKingQuakeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingShadowTrapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king shadow trap";

    IccLichKingShadowTrapTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king tank defensive";

    IccLichKingTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king taunt";

    IccLichKingTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingPlagueCureTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king plague cure";

    IccLichKingPlagueCureTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingPlagueWalkTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king plague walk";

    IccLichKingPlagueWalkTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingTranquilizeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king tranquilize";

    IccLichKingTranquilizeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingStunTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king stun";

    IccLichKingStunTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingSilenceDispelTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king silence dispel";

    IccLichKingSilenceDispelTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc lich king target";

    IccLichKingTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetLkTarget) {}
};

class IccLichKingBrakeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king brake";

    IccLichKingBrakeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLichKingStandTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lich king stand";

    IccLichKingStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif

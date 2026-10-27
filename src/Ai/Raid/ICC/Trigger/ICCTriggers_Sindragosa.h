/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_SINDRAGOSA_H
#define PLAYERBOTS_ICCTRIGGERS_SINDRAGOSA_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class IccSindragosaGroupPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa group position";

    IccSindragosaGroupPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaFrostBeaconTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa frost beacon";

    IccSindragosaFrostBeaconTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaHotTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa hot";

    IccSindragosaHotTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaBlisteringColdTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa blistering cold";

    IccSindragosaBlisteringColdTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaUnchainedMagicTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa unchained magic";

    IccSindragosaUnchainedMagicTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaChilledToTheBoneTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa chilled to the bone";

    IccSindragosaChilledToTheBoneTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaMysticBuffetTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa mystic buffet";

    IccSindragosaMysticBuffetTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaFrostBombTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa frost bomb";

    IccSindragosaFrostBombTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif

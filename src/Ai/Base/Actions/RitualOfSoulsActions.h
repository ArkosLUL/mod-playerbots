/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RITUALOFSOULSACTIONS_H
#define PLAYERBOTS_RITUALOFSOULSACTIONS_H

#include "MovementActions.h"
#include "Trigger.h"

class PlayerbotAI;
class GameObject;

// Ritual of Souls spawns a "Soul Portal" (GAMEOBJECT_TYPE_RITUAL). Once enough group members have
// clicked it, the channel completes and a Soulwell (GAMEOBJECT_TYPE_SPELLCASTER) spawns that hands
// out healthstones to the caster's raid. R1 = Master Healthstone, R2 = Fel Healthstone.
constexpr uint32 GO_SOUL_PORTAL_R1 = 181622;
constexpr uint32 GO_SOUL_PORTAL_R2 = 193168;
constexpr uint32 GO_SOULWELL_R1 = 181621;
constexpr uint32 GO_SOULWELL_R2 = 193169;

constexpr float RITUAL_OF_SOULS_GO_RANGE = 40.0f;

// Nearest Soul Portal owned by a member of the bot's group. When excludeSelf is set the bot's own
// ritual is skipped (a caster must not try to "join" its own portal).
GameObject* FindGroupRitualPortal(PlayerbotAI* botAI, bool excludeSelf = true,
                                  float range = RITUAL_OF_SOULS_GO_RANGE);
// Nearest Soulwell owned by a member of the bot's group.
GameObject* FindGroupSoulwell(PlayerbotAI* botAI, float range = RITUAL_OF_SOULS_GO_RANGE);

class JoinRitualOfSoulsAction : public MovementAction
{
public:
    JoinRitualOfSoulsAction(PlayerbotAI* botAI) : MovementAction(botAI, "join ritual of souls") {}
    bool Execute(Event event) override;
};

class UseSoulwellAction : public MovementAction
{
public:
    UseSoulwellAction(PlayerbotAI* botAI) : MovementAction(botAI, "use soulwell") {}
    bool Execute(Event event) override;
};

class RitualPortalNearbyTrigger : public Trigger
{
public:
    RitualPortalNearbyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "ritual of souls portal nearby") {}
    bool IsActive() override;
};

class SoulwellNearbyTrigger : public Trigger
{
public:
    SoulwellNearbyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "soulwell nearby") {}
    bool IsActive() override;
};

#endif

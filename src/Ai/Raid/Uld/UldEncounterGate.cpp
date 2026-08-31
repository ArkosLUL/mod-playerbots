/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounterGate.h"

#include "InstanceScript.h"
#include "Player.h"
#include "PlayerbotAI.h"

namespace
{
    struct EncounterPrefix
    {
        char const* prefix;
        uint32 bossId;
    };

    // Every Ulduar trigger name starts with its encounter. No prefix here is a prefix of another, so
    // first match wins. `sara` is Yogg-Saron's phase-one form and the one name that does not lead with
    // the encounter.
    constexpr EncounterPrefix ENCOUNTER_PREFIXES[] = {
        {"flame leviathan", ULD_BOSS_LEVIATHAN},
        {"ignis", ULD_BOSS_IGNIS},
        {"razorscale", ULD_BOSS_RAZORSCALE},
        {"xt002", ULD_BOSS_XT002},
        {"iron assembly", ULD_BOSS_ASSEMBLY},
        {"kologarn", ULD_BOSS_KOLOGARN},
        {"auriaya", ULD_BOSS_AURIAYA},
        {"freya", ULD_BOSS_FREYA},
        {"hodir", ULD_BOSS_HODIR},
        {"mimiron", ULD_BOSS_MIMIRON},
        {"thorim", ULD_BOSS_THORIM},
        {"vezax", ULD_BOSS_VEZAX},
        {"yogg-saron", ULD_BOSS_YOGGSARON},
        {"sara", ULD_BOSS_YOGGSARON},
        {"algalon", ULD_BOSS_ALGALON},
    };
}

bool UldEncounterOfTrigger(std::string const& triggerName, uint32& bossId)
{
    for (EncounterPrefix const& entry : ENCOUNTER_PREFIXES)
    {
        if (triggerName.rfind(entry.prefix, 0) == 0)
        {
            bossId = entry.bossId;
            return true;
        }
    }

    return false;
}

bool UldEncounterGateOpen(PlayerbotAI* botAI, uint32 bossId)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    InstanceScript* instance = bot ? bot->GetInstanceScript() : nullptr;
    // Null outside an instance, and the encounter count guards a boss id this script does not have -
    // both leave the trigger ungated rather than silently dead.
    if (!instance || bossId >= instance->GetEncounterCount())
        return true;

    switch (instance->GetBossState(bossId))
    {
        case DONE:
            return false;
        case IN_PROGRESS:
            return true;
        default:
            break;
    }

    // Nothing settled for this boss, so the only thing that closes it is another encounter being
    // live. That is the case the traces caught, and it leaves everything open between pulls.
    for (uint32 other = 0; other < instance->GetEncounterCount(); ++other)
        if (other != bossId && instance->GetBossState(other) == IN_PROGRESS)
            return false;

    return true;
}

// Name and check interval are copied off the inner trigger, not defaulted: Engine::ProcessTriggers
// calls needCheck on this object, so a wrapper built with the default interval of 1 would quietly
// promote every throttled trigger - Algalon's on 2 and 5, Iron Assembly's Rune of Death on 200 - to
// running every tick.
UldGatedTrigger::UldGatedTrigger(PlayerbotAI* botAI, Trigger* inner, uint32 bossId)
    : Trigger(botAI, inner ? inner->getName() : "trigger", inner ? inner->getCheckInterval() : 1),
      inner(inner),
      bossId(bossId)
{
}

UldGatedTrigger::~UldGatedTrigger() { delete inner; }

Event UldGatedTrigger::Check()
{
    if (!inner || !UldEncounterGateOpen(botAI, bossId))
        return Event();

    return inner->Check();
}

bool UldGatedTrigger::IsActive()
{
    return inner && UldEncounterGateOpen(botAI, bossId) && inner->IsActive();
}

bool UldGatedTrigger::IsBuffTrigger() { return inner && inner->IsBuffTrigger(); }

bool UldGatedTrigger::IsDebuffTrigger() { return inner && inner->IsDebuffTrigger(); }

std::vector<NextAction> UldGatedTrigger::getHandlers()
{
    return inner ? inner->getHandlers() : std::vector<NextAction>();
}

void UldGatedTrigger::Reset()
{
    if (inner)
        inner->Reset();
}

Unit* UldGatedTrigger::GetTarget() { return inner ? inner->GetTarget() : Trigger::GetTarget(); }

Value<Unit*>* UldGatedTrigger::GetTargetValue()
{
    return inner ? inner->GetTargetValue() : Trigger::GetTargetValue();
}

std::string const UldGatedTrigger::GetTargetName()
{
    return inner ? inner->GetTargetName() : Trigger::GetTargetName();
}

void UldGatedTrigger::ExternalEvent(std::string const param, Player* owner)
{
    if (inner)
        inner->ExternalEvent(param, owner);
}

void UldGatedTrigger::ExternalEvent(WorldPacket& packet, Player* owner)
{
    if (inner)
        inner->ExternalEvent(packet, owner);
}

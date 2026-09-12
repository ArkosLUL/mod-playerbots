/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounterGate.h"

#include "InstanceScript.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "RaidObs.h"
#include "Timer.h"

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

namespace
{
    // The instance's boss states, read once per trigger pass instead of once per gated trigger:
    // every one of the 165 Ulduar triggers asks the gate, and each ask can read all fourteen states.
    //
    // A pass is one Engine::ProcessTriggers: the first Check opens it for that bot and the Reset
    // sweep the engine runs right after the checks closes it. Reading once is exact because boss
    // state only moves on a pull, a wipe or a kill, and nothing in a trigger pass attacks anything.
    // Only Check uses it. IsActive can be called from elsewhere, so it keeps reading live.
    //
    // The ms stamp is what makes it safe rather than the Reset sweep. Nothing in Trigger's contract
    // promises Reset runs: MultiplyAndPush sits between the checks and the resets, and a strategy
    // swapped there rebuilds the trigger list, so the sweep can iterate a list holding no gated
    // trigger at all. Stamped, a pass that outlives its Reset expires on its own instead of
    // answering the next tick's gates from a pull or a kill that has since landed.
    struct GatePass
    {
        PlayerbotAI* botAI = nullptr;
        uint32 atMs = 0;
        bool readLive = false;  // more encounters than the masks hold
        bool hasInstance = false;
        uint32 encounterCount = 0;
        uint32 inProgressMask = 0;
        uint32 doneMask = 0;
    };

    // Per thread, not shared: a bot's whole pass runs on one map thread, start to finish.
    thread_local GatePass gatePass;

    bool UldEncounterGateOpenInPass(PlayerbotAI* botAI, uint32 bossId)
    {
        if (!botAI)
            return UldEncounterGateOpen(botAI, bossId);

        uint32 const now = getMSTime();
        if (gatePass.botAI != botAI || gatePass.atMs != now)
        {
            gatePass = GatePass();
            gatePass.botAI = botAI;
            gatePass.atMs = now;

            Player* bot = botAI->GetBot();
            InstanceScript* instance = bot ? bot->GetInstanceScript() : nullptr;
            if (instance && instance->GetEncounterCount() > 32)
                gatePass.readLive = true;
            else if (instance)
            {
                gatePass.hasInstance = true;
                gatePass.encounterCount = instance->GetEncounterCount();
                for (uint32 id = 0; id < gatePass.encounterCount; ++id)
                {
                    EncounterState const state = instance->GetBossState(id);
                    if (state == IN_PROGRESS)
                        gatePass.inProgressMask |= 1u << id;
                    else if (state == DONE)
                        gatePass.doneMask |= 1u << id;
                }
            }
        }

        if (gatePass.readLive)
            return UldEncounterGateOpen(botAI, bossId);

        // The same rules as UldEncounterGateOpen, read off the masks.
        if (!gatePass.hasInstance || bossId >= gatePass.encounterCount)
            return true;

        uint32 const bit = 1u << bossId;
        if (gatePass.doneMask & bit)
            return false;

        if (gatePass.inProgressMask & bit)
            return true;

        return (gatePass.inProgressMask & ~bit) == 0;
    }
}

bool UldEncounterIsLive(PlayerbotAI* botAI, uint32 bossId)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    InstanceScript* instance = bot ? bot->GetInstanceScript() : nullptr;
    if (!instance || bossId >= instance->GetEncounterCount())
        return false;

    return instance->GetBossState(bossId) == IN_PROGRESS;
}

char const* UldEncounterName(uint32 bossId)
{
    // First entry wins, which is why `sara` sitting after `yogg-saron` matters: both name the same
    // encounter and only one of them reads as a boss.
    for (EncounterPrefix const& entry : ENCOUNTER_PREFIXES)
        if (entry.bossId == bossId)
            return entry.prefix;

    return nullptr;
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
    if (!inner || !UldEncounterGateOpenInPass(botAI, bossId))
        return Event();

    Event event = inner->Check();
    if (!event)
        return event;

    // A trigger firing while its own encounter is the live one is the only thing that can name a pull
    // the engage hook missed - Yogg-Saron's phase one has no boss to enter combat with a player, so
    // 2026-09-04's wipe there was filed as `ulduar`. IN_PROGRESS rather than the gate's weaker test on
    // purpose: with nothing engaged every trigger is open, and a Vezax trigger firing in that window
    // must not name a Yogg-Saron pull after Vezax.
    if (RaidObs::Active() && UldEncounterIsLive(botAI, bossId))
        RaidObs::NamePull(botAI->GetBot()->GetMap(), UldEncounterName(bossId));

    return event;
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
    // End of the pass, so the next Check reads the boss states afresh. Belt to the stamp's braces:
    // this closes the pass on the tick it belongs to, the stamp covers a Reset that never arrives.
    if (gatePass.botAI == botAI)
        gatePass = GatePass();

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

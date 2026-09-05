/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERGATE_H
#define PLAYERBOTS_ULDENCOUNTERGATE_H

#include <string>

#include "Trigger.h"

class PlayerbotAI;

// Boss ids ported from ulduar.h in AC, which lives in AC's /scripts directory and so cannot be
// included from here. These are the indices InstanceScript::GetBossState is keyed by.
enum UlduarEncounterId
{
    ULD_BOSS_LEVIATHAN = 0,
    ULD_BOSS_IGNIS = 1,
    ULD_BOSS_RAZORSCALE = 2,
    ULD_BOSS_XT002 = 3,
    ULD_BOSS_ASSEMBLY = 4,
    ULD_BOSS_KOLOGARN = 5,
    ULD_BOSS_AURIAYA = 6,
    ULD_BOSS_FREYA = 7,
    ULD_BOSS_HODIR = 8,
    ULD_BOSS_MIMIRON = 9,
    ULD_BOSS_THORIM = 10,
    ULD_BOSS_VEZAX = 11,
    ULD_BOSS_YOGGSARON = 12,
    ULD_BOSS_ALGALON = 13
};

// RaidUlduarStrategy registers all 165 Ulduar trigger nodes for every encounter - nothing keys the
// registration to the boss in the room - so a Kologarn pull still evaluates Mimiron, Vezax and Flame
// Leviathan every tick. Usually the action declines, but on 2026-08-31 twenty bots spent a second
// trying to board a leftover siege vehicle mid-Kologarn, because the Flame Leviathan trigger asks
// only whether the master is in one.
//
// Closed while a different encounter is IN_PROGRESS, and while this one is DONE. Deliberately not a
// boss-presence or room check: with nothing engaged every trigger stays live, so the pre-pull
// sequences that have no boss to point at yet - Mimiron's console walk, boarding a vehicle before
// Flame Leviathan is pulled - keep working.
bool UldEncounterGateOpen(PlayerbotAI* botAI, uint32 bossId);

// Reads the encounter off the trigger's own name, which carries its boss as a prefix throughout.
// False for a name belonging to no single encounter, which is then left ungated.
bool UldEncounterOfTrigger(std::string const& triggerName, uint32& bossId);

// Stricter than the gate: this encounter and no other is what the instance script says is running.
// The gate leaves everything open between pulls, which is far too loose to name a trace by.
bool UldEncounterIsLive(PlayerbotAI* botAI, uint32 bossId);

// The encounter's name, for a trace that could not name itself. Null for an id with no entry.
char const* UldEncounterName(uint32 bossId);

// Wraps rather than subclasses. All 165 derive from Trigger directly, so one decorator applied where
// the context builds them beats editing every class, and leaves their IsActive bodies alone.
class UldGatedTrigger : public Trigger
{
public:
    UldGatedTrigger(PlayerbotAI* botAI, Trigger* inner, uint32 bossId);
    ~UldGatedTrigger() override;

    Event Check() override;
    bool IsActive() override;
    bool IsBuffTrigger() override;
    bool IsDebuffTrigger() override;
    std::vector<NextAction> getHandlers() override;
    void Reset() override;
    Unit* GetTarget() override;
    Value<Unit*>* GetTargetValue() override;
    std::string const GetTargetName() override;
    void ExternalEvent(std::string const param, Player* owner = nullptr) override;
    void ExternalEvent(WorldPacket& packet, Player* owner = nullptr) override;

private:
    Trigger* inner;
    uint32 bossId;
};

#endif

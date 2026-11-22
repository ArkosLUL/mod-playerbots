#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_FACTIONCHAMPIONSDEFENCE_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_FACTIONCHAMPIONSDEFENCE_H

#include "Multiplier.h"
#include "RaidCombatHelpers.h"

class FactionChampionsAoeGuardMultiplier : public Multiplier
{
public:
    FactionChampionsAoeGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions aoe guard multiplier") {}
    float GetValue(Action* action) override;

private:
    RaidActionCache<bool> _meleeMove;
};

class FactionChampionsPhysicalSwitchGuardMultiplier : public Multiplier
{
public:
    FactionChampionsPhysicalSwitchGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions physical switch guard multiplier") {}
    float GetValue(Action* action) override;
};

class FactionChampionsDispelGuardMultiplier : public Multiplier
{
public:
    FactionChampionsDispelGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions dispel guard multiplier") {}
    float GetValue(Action* action) override;

private:
    RaidActionCache<bool> _magicDispel;
};

class FactionChampionsPurgeGuardMultiplier : public Multiplier
{
public:
    FactionChampionsPurgeGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions purge guard multiplier") {}
    float GetValue(Action* action) override;

private:
    RaidActionCache<bool> _purge;
};

class FactionChampionsGripGuardMultiplier : public Multiplier
{
public:
    FactionChampionsGripGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions grip guard multiplier") {}
    float GetValue(Action* action) override;

private:
    struct Facts
    {
        bool petToggle = false;
        CastSpellAction* cast = nullptr;
        RaidSpellIdRef spellId;
    };

    RaidActionCache<Facts> _facts;
};

class FactionChampionsIceGuardMultiplier : public Multiplier
{
public:
    FactionChampionsIceGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions ice guard multiplier") {}
    float GetValue(Action* action) override;

private:
    RaidActionCache<bool> _classHammer;
};

#endif

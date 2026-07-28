/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXMULTIPLIERS_H
#define PLAYERBOTS_NAXXMULTIPLIERS_H

#include "Multiplier.h"
#include "NaxxBossHelper.h"
#include "GenericSpellActions.h"

class GrobbulusMultiplier : public Multiplier
{
public:
    GrobbulusMultiplier(PlayerbotAI* ai) : Multiplier(ai, "grobbulus") {}

public:
    virtual float GetValue(Action* action);
};
class HeiganDanceMultiplier : public Multiplier
{
public:
    HeiganDanceMultiplier(PlayerbotAI* ai) : Multiplier(ai, "heigan dance"), helper(ai) {}

public:
    virtual float GetValue(Action* action);

private:
    // How long before an eruption bots stop starting anything they cannot finish before the step.
    static constexpr uint32 EruptionCastCutoffMs = 2500;

    HeiganBossHelper helper;
};

class LoathebGenericMultiplier : public Multiplier
{
public:
    LoathebGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "loatheb generic") {}

public:
    virtual float GetValue(Action* action);
};

class ThaddiusGenericMultiplier : public Multiplier
{
public:
    ThaddiusGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thaddius generic"), helper(ai) {}

public:
    virtual float GetValue(Action* action);

private:
    ThaddiusBossHelper helper;
};

class SapphironGenericMultiplier : public Multiplier
{
public:
    SapphironGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "sapphiron generic"), helper(ai) {}

    virtual float GetValue(Action* action);

private:
    SapphironBossHelper helper;
};

class InstructorRazuviousGenericMultiplier : public Multiplier
{
public:
    InstructorRazuviousGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "instructor razuvious generic"), helper(ai)
    {
    }
    virtual float GetValue(Action* action);

private:
    RazuviousBossHelper helper;
};

class KelthuzadGenericMultiplier : public Multiplier
{
public:
    KelthuzadGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "kelthuzad generic"), helper(ai) {}
    virtual float GetValue(Action* action);

private:
    KelthuzadBossHelper helper;
};

class AnubrekhanGenericMultiplier : public Multiplier
{
public:
    AnubrekhanGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "anubrekhan generic"), helper(ai) {}

public:
    virtual float GetValue(Action* action);

private:
    AnubrekhanBossHelper helper;
};

class FourhorsemanGenericMultiplier : public Multiplier
{
public:
    FourhorsemanGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "fourhorseman generic") {}

public:
    virtual float GetValue(Action* action);
};

// class GothikGenericMultiplier : public Multiplier
// {
// public:
//     GothikGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "gothik generic") {}

// public:
//     virtual float GetValue(Action* action);
// };

class GluthGenericMultiplier : public Multiplier
{
public:
    GluthGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "gluth generic"), helper(ai) {}
    float GetValue(Action* action) override;

private:
    GluthBossHelper helper;
};

class NaxxThreatRedirectMultiplier : public Multiplier
{
public:
    NaxxThreatRedirectMultiplier(PlayerbotAI* ai) : Multiplier(ai, "naxx threat redirect"), heigan(ai) {}
    float GetValue(Action* action) override;

private:
    HeiganBossHelper heigan;
};

// Holds the offensive burst cooldowns until the boss's actual DPS check. One multiplier for all six
// bosses so the IsBurstCooldownAction early-out runs once per action instead of six times.
class NaxxBurstWindowMultiplier : public Multiplier
{
public:
    NaxxBurstWindowMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "naxx burst window"), kelthuzad(ai), sapphiron(ai), thaddius(ai), loatheb(ai), noth(ai)
    {
    }
    float GetValue(Action* action) override;

private:
    // Sweeps every boss helper, so it is cached for the rest of the tick rather than re-run per
    // action. Each helper resolves through "find target", which utf8-lowercases the whole threat
    // list on a 1 ms cache.
    float EvaluateWindow();

    // Fungal Creep is not scripted in this core's boss_loatheb.cpp, so without a timed fallback the
    // cooldowns would be held for the whole fight.
    static constexpr uint32 LOATHEB_FALLBACK_MS = 45000;

    // Guardians of Icecrown start spawning here (boss_kelthuzad.cpp, HealthBelowPct(45)).
    static constexpr float KELTHUZAD_GUARDIAN_PCT = 45.0f;

    KelthuzadBossHelper kelthuzad;
    SapphironBossHelper sapphiron;
    ThaddiusBossHelper thaddius;
    LoathebBossHelper loatheb;
    NothBossHelper noth;
    uint32 loathebFightStartMs = 0;

    uint32 cachedAtMs = 0;
    float cachedValue = 1.0f;
};

class NothGenericMultiplier : public Multiplier
{
public:
    NothGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "noth generic"), helper(ai) {}
    float GetValue(Action* action) override;

private:
    NothBossHelper helper;
};

#endif

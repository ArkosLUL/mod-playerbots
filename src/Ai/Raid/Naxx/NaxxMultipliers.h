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

class HeiganDanceMultiplier : public Multiplier
{
public:
    HeiganDanceMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "heigan dance"), helper(botAI) {}

public:
    float GetValue(Action* action) override;

private:
    // How long before an eruption bots stop starting anything they cannot finish before the step.
    static constexpr uint32 EruptionCastCutoffMs = 2500;

    HeiganBossHelper helper;
};

class LoathebGenericMultiplier : public Multiplier
{
public:
    LoathebGenericMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "loatheb generic") {}

public:
    float GetValue(Action* action) override;
};

class ThaddiusPrepullMultiplier : public Multiplier
{
public:
    ThaddiusPrepullMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "thaddius prepull"), helper(botAI) {}

public:
    float GetValue(Action* action) override;

private:
    ThaddiusBossHelper helper;
};

class ThaddiusGenericMultiplier : public Multiplier
{
public:
    ThaddiusGenericMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "thaddius generic"), helper(botAI) {}

public:
    float GetValue(Action* action) override;

private:
    ThaddiusBossHelper helper;
};

class SapphironGenericMultiplier : public Multiplier
{
public:
    SapphironGenericMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "sapphiron generic"), helper(botAI) {}

    float GetValue(Action* action) override;

private:
    SapphironBossHelper helper;
};

class InstructorRazuviousGenericMultiplier : public Multiplier
{
public:
    InstructorRazuviousGenericMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "instructor razuvious generic"), helper(botAI) {}
    float GetValue(Action* action) override;

private:
    RazuviousBossHelper helper;
};

class KelthuzadGenericMultiplier : public Multiplier
{
public:
    KelthuzadGenericMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "kelthuzad generic"), helper(botAI) {}
    float GetValue(Action* action) override;

private:
    KelthuzadBossHelper helper;
};

class AnubrekhanGenericMultiplier : public Multiplier
{
public:
    AnubrekhanGenericMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubrekhan generic"), helper(botAI) {}

public:
    float GetValue(Action* action) override;

private:
    AnubrekhanBossHelper helper;
};

class FourhorsemanGenericMultiplier : public Multiplier
{
public:
    FourhorsemanGenericMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "fourhorseman generic") {}

public:
    float GetValue(Action* action) override;
};

class GothikGenericMultiplier : public Multiplier
{
public:
    GothikGenericMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "gothik generic"), helper(botAI) {}
    float GetValue(Action* action) override;

private:
    GothikBossHelper helper;
};

class GluthGenericMultiplier : public Multiplier
{
public:
    GluthGenericMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "gluth generic"), helper(botAI) {}
    float GetValue(Action* action) override;

private:
    GluthBossHelper helper;
};

class NaxxThreatRedirectMultiplier : public Multiplier
{
public:
    NaxxThreatRedirectMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "naxx threat redirect"), heigan(botAI) {}
    float GetValue(Action* action) override;

private:
    HeiganBossHelper heigan;
};

// Holds the offensive burst cooldowns until the boss's actual DPS check. One multiplier for all six
// bosses so the IsBurstCooldownAction early-out runs once per action instead of six times.
class NaxxBurstWindowMultiplier : public Multiplier
{
public:
    NaxxBurstWindowMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "naxx burst window"), kelthuzad(botAI), sapphiron(botAI), thaddius(botAI), loatheb(botAI), noth(botAI)
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
    NothGenericMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "noth generic"), helper(botAI) {}
    float GetValue(Action* action) override;

private:
    NothBossHelper helper;
};

#endif

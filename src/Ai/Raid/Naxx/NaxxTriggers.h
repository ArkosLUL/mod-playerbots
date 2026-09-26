/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXTRIGGERS_H
#define PLAYERBOTS_NAXXTRIGGERS_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "NaxxBossHelper.h"
#include "PlayerbotAIConfig.h"
#include "Trigger.h"

// Above this the encounter has effectively just been pulled, which is where a threat redirect is
// worth its cooldown.
static constexpr float NAXX_PULL_HEALTH_PCT = 95.0f;

// HasAuraTrigger looks the aura up by the trigger's own name, so the subclasses keep this one and their
// Name is only what they're registered under.
class MutatingInjectionTrigger : public HasAuraTrigger
{
public:
    MutatingInjectionTrigger(PlayerbotAI* ai) : HasAuraTrigger(ai, "mutating injection", 1) {}
};

class MutatingInjectionMeleeTrigger : public MutatingInjectionTrigger
{
public:
    static constexpr char const* Name = "mutating injection melee";

    MutatingInjectionMeleeTrigger(PlayerbotAI* ai) : MutatingInjectionTrigger(ai) {}
    bool IsActive() override;
};

class MutatingInjectionRangedTrigger : public MutatingInjectionTrigger
{
public:
    static constexpr char const* Name = "mutating injection ranged";

    MutatingInjectionRangedTrigger(PlayerbotAI* ai) : MutatingInjectionTrigger(ai) {}
    bool IsActive() override;
};

class AuraRemovedTrigger : public Trigger
{
public:
    AuraRemovedTrigger(PlayerbotAI* botAI, std::string name) : Trigger(botAI, name, 1) { this->prev_check = false; }
    virtual bool IsActive() override;

protected:
    bool prev_check;
};

// Same as MutatingInjectionTrigger: the base reads its own name as the aura, so Name only registers.
class MutatingInjectionRemovedTrigger : public HasNoAuraTrigger
{
public:
    static constexpr char const* Name = "mutating injection removed";

    MutatingInjectionRemovedTrigger(PlayerbotAI* ai) : HasNoAuraTrigger(ai, "mutating injection") {}
    virtual bool IsActive();
};

class GrobbulusCloudTrigger : public Trigger
{
public:
    static constexpr char const* Name = "grobbulus cloud";

    GrobbulusCloudTrigger(PlayerbotAI* ai) : Trigger(ai, Name), last_cloud_ms(0) {}
    bool IsActive() override;

private:
    uint32 last_cloud_ms;
    static constexpr uint32 CloudRotationDelayMs = 15000;
};

class HeiganMeleeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "heigan melee";

    HeiganMeleeTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    virtual bool IsActive();

private:
    HeiganBossHelper helper;
};

class HeiganRangedTrigger : public Trigger
{
public:
    static constexpr char const* Name = "heigan ranged";

    HeiganRangedTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    HeiganBossHelper helper;
};

class HeiganDecrepitFeverTrigger : public Trigger
{
public:
    static constexpr char const* Name = "heigan decrepit fever";

    HeiganDecrepitFeverTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    HeiganBossHelper helper;
};

class RazuviousTankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "razuvious tank";

    RazuviousTankTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    RazuviousBossHelper helper;
};

class RazuviousNontankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "razuvious nontank";

    RazuviousNontankTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    RazuviousBossHelper helper;
};

class KelthuzadTrigger : public Trigger
{
public:
    KelthuzadTrigger(PlayerbotAI* ai) : Trigger(ai, "kel'thuzad trigger"), helper(ai) {}
    bool IsActive() override;

private:
    KelthuzadBossHelper helper;
};

class KelthuzadShadowFissureTrigger : public Trigger
{
public:
    KelthuzadShadowFissureTrigger(PlayerbotAI* ai) : Trigger(ai, "kel'thuzad shadow fissure"), helper(ai) {}
    bool IsActive() override;

private:
    KelthuzadBossHelper helper;
};

// Fires on Balance/Restoration druids while a raid member is charmed by Chains of Kel'Thuzad.
class KelthuzadChainsTrigger : public Trigger
{
public:
    KelthuzadChainsTrigger(PlayerbotAI* ai) : Trigger(ai, "kel'thuzad chains"), helper(ai) {}
    bool IsActive() override;

private:
    KelthuzadBossHelper helper;
};

class AnubrekhanTrigger : public Trigger
{
public:
    static constexpr char const* Name = "anub'rekhan";

    AnubrekhanTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class AnubrekhanLocustSwarmTrigger : public Trigger
{
public:
    static constexpr char const* Name = "anub'rekhan locust swarm";

    AnubrekhanLocustSwarmTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    AnubrekhanBossHelper helper;
};

class FaerlinaTrigger : public Trigger
{
public:
    static constexpr char const* Name = "faerlina";

    FaerlinaTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class FaerlinaFrenzyTrigger : public Trigger
{
public:
    static constexpr char const* Name = "faerlina frenzy";

    FaerlinaFrenzyTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MaexxnaTrigger : public Trigger
{
public:
    static constexpr char const* Name = "maexxna";

    MaexxnaTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MaexxnaWebWrapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "maexxna web wrap";

    MaexxnaWebWrapTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MaexxnaSpiderlingsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "maexxna spiderlings";

    MaexxnaSpiderlingsTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class GothikTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gothik";

    GothikTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    GothikBossHelper helper;
};

// A safety net for a bot that started on the dead side or got shoved across it. While the gate is
// shut nobody can cross on purpose.
class GothikWrongSideTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gothik wrong side";

    GothikWrongSideTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    GothikBossHelper helper;
};

// class PatchwerkTankTrigger : public Trigger
// {
// public:
//     PatchwerkTankTrigger(PlayerbotAI* ai) : Trigger(ai, "patchwerk tank") {}
//     bool IsActive() override;
// };

// class PatchwerkNonTankTrigger : public Trigger
// {
// public:
//     PatchwerkNonTankTrigger(PlayerbotAI* ai) : Trigger(ai, "patchwerk non-tank") {}
//     bool IsActive() override;
// };

// class PatchwerkRangedTrigger : public Trigger
// {
// public:
//     PatchwerkRangedTrigger(PlayerbotAI* ai) : Trigger(ai, "patchwerk ranged") {}
//     bool IsActive() override;
// };

class ThaddiusPrepullSplitTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thaddius prepull split";

    // Resolving the adds out of combat costs a grid search, so do not run it every tick.
    ThaddiusPrepullSplitTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 2), helper(ai) {}
    bool IsActive() override;

private:
    ThaddiusBossHelper helper;
};

class ThaddiusPhasePetTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thaddius phase pet";

    ThaddiusPhasePetTrigger(PlayerbotAI* ai) : ThaddiusPhasePetTrigger(ai, Name) {}
    bool IsActive() override;

protected:
    ThaddiusPhasePetTrigger(PlayerbotAI* ai, char const* name) : Trigger(ai, name), helper(ai) {}

private:
    ThaddiusBossHelper helper;
};

class ThaddiusPhasePetLoseAggroTrigger : public ThaddiusPhasePetTrigger
{
public:
    static constexpr char const* Name = "thaddius phase pet lose aggro";

    ThaddiusPhasePetLoseAggroTrigger(PlayerbotAI* ai) : ThaddiusPhasePetTrigger(ai, Name) {}
    virtual bool IsActive()
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        return ThaddiusPhasePetTrigger::IsActive() && botAI->IsTank(bot) && target && target->GetVictim() != bot;
    }
};

class ThaddiusPhaseTransitionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thaddius phase transition";

    ThaddiusPhaseTransitionTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    ThaddiusBossHelper helper;
};

class ThaddiusPhaseThaddiusTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thaddius phase thaddius";

    ThaddiusPhaseThaddiusTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    ThaddiusBossHelper helper;
};

class ThaddiusRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thaddius redirect threat";

    ThaddiusRedirectThreatTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    ThaddiusBossHelper helper;
};

class FourhorsemanRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "four horsemen redirect threat";

    FourhorsemanRedirectThreatTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    FourhorsemanBossHelper helper;
};

class HorsemanAttractorsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "horseman attractors";

    HorsemanAttractorsTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    FourhorsemanBossHelper helper;
};

class HorsemanExceptAttractorsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "horseman except attractors";

    HorsemanExceptAttractorsTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    FourhorsemanBossHelper helper;
};

class SapphironGroundTrigger : public Trigger
{
public:
    SapphironGroundTrigger(PlayerbotAI* ai) : Trigger(ai, "sapphiron ground"), helper(ai) {}
    bool IsActive() override;

private:
    SapphironBossHelper helper;
};

class SapphironFlightTrigger : public Trigger
{
public:
    SapphironFlightTrigger(PlayerbotAI* ai) : Trigger(ai, "sapphiron flight"), helper(ai) {}
    bool IsActive() override;

private:
    SapphironBossHelper helper;
};

class GluthTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gluth";

    GluthTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    GluthBossHelper helper;
};

class GluthLowHealthZombieAoeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gluth low health zombie aoe";

    GluthLowHealthZombieAoeTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    GluthBossHelper helper;
};

class GluthMainTankMortalWoundTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gluth main tank mortal wound";

    GluthMainTankMortalWoundTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    GluthBossHelper helper;
};

class GluthFrenzyTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gluth frenzy";

    GluthFrenzyTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    GluthBossHelper helper;
};

class GluthRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gluth redirect threat";

    GluthRedirectThreatTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    GluthBossHelper helper;
};

class LoathebTrigger : public Trigger
{
public:
    static constexpr char const* Name = "loatheb";

    LoathebTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    LoathebBossHelper helper;
};

class NothTrigger : public Trigger
{
public:
    static constexpr char const* Name = "noth";

    NothTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    NothBossHelper helper;
};

class NothCurseTrigger : public Trigger
{
public:
    static constexpr char const* Name = "noth curse";

    NothCurseTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    NothBossHelper helper;
};

class NothBlinkTrigger : public Trigger
{
public:
    static constexpr char const* Name = "noth blink";

    NothBlinkTrigger(PlayerbotAI* ai) : Trigger(ai, Name), helper(ai) {}
    bool IsActive() override;

private:
    NothBossHelper helper;
};
#endif

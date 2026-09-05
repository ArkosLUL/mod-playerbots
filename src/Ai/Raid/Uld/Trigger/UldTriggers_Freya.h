#ifndef PLAYERBOTS_ULDTRIGGERS_FREYA_H
#define PLAYERBOTS_ULDTRIGGERS_FREYA_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

//
// Freya
//
class FreyaNearNatureBombTrigger : public Trigger
{
public:
    FreyaNearNatureBombTrigger(PlayerbotAI* ai) : Trigger(ai, "freya near nature bomb") {}
    bool IsActive() override;
};

// The main tank, who walks Freya off a bomb field instead of standing in it. Bombs land at players'
// feet and the melee stack is on the boss, so half of every volley drops inside her melee ring.
class FreyaTankNatureBombTrigger : public Trigger
{
public:
    FreyaTankNatureBombTrigger(PlayerbotAI* ai) : Trigger(ai, "freya tank nature bomb") {}
    bool IsActive() override;
};

// Owns every DPS bot's target for the whole encounter, so the trio wave can be split three ways.
class FreyaSetDpsPriorityTrigger : public Trigger
{
public:
    FreyaSetDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, "freya set dps priority") {}
    bool IsActive() override;
};

// The main tank and assist tank 0, who both always have something on GetFreyaTankTarget's ladder.
class FreyaTankAddsTrigger : public Trigger
{
public:
    FreyaTankAddsTrigger(PlayerbotAI* ai) : Trigger(ai, "freya tank adds") {}
    bool IsActive() override;
};

// Hunters and rogues, for as long as Freya is up. The action decides which tank the redirect goes to.
class FreyaRedirectThreatTrigger : public Trigger
{
public:
    FreyaRedirectThreatTrigger(PlayerbotAI* ai) : Trigger(ai, "freya redirect threat") {}
    bool IsActive() override;
};

class FreyaMoveToHealingSporeTrigger : public Trigger
{
public:
    FreyaMoveToHealingSporeTrigger(PlayerbotAI* ai) : Trigger(ai, "freya move to healing spore trigger") {}
    bool IsActive() override;
};

// Hard mode: bot is trapped by Iron Roots (Ironbranch or Freya-cast) and must break out.
class FreyaBreakIronRootsTrigger : public Trigger
{
public:
    FreyaBreakIronRootsTrigger(PlayerbotAI* ai) : Trigger(ai, "freya break iron roots") {}
    bool IsActive() override;
};

// A Detonating Lasher next to this bot is about to blow. Melee only: they are the ones standing on the
// pile, and one below the bail line gives them about 2.6s to clear its 15 yd blast.
class FreyaLasherAboutToBlowTrigger : public Trigger
{
public:
    FreyaLasherAboutToBlowTrigger(PlayerbotAI* ai) : Trigger(ai, "freya lasher about to blow") {}
    bool IsActive() override;
};

// Ranged and healers hold one camp a fixed standoff from the pack so the lashers gather themselves
// into a pile the raid can AoE. Nothing is walked anywhere: a lasher moves at 8.0 yd/s against a
// player's 7.0.
class FreyaRangedCampTrigger : public Trigger
{
public:
    FreyaRangedCampTrigger(PlayerbotAI* ai) : Trigger(ai, "freya ranged camp") {}
    bool IsActive() override;
};

// Frost Nova roots whatever has closed on the mage for a full 8s: it carries no
// AURA_INTERRUPT_FLAG_TAKE_DAMAGE, so raid damage does not break it, and the lasher has no
// CREATURE_FLAG_EXTRA_ALL_DIMINISH, so it never diminishes either.
class FreyaFrostNovaLashersTrigger : public Trigger
{
public:
    FreyaFrostNovaLashersTrigger(PlayerbotAI* ai) : Trigger(ai, "freya frost nova lashers") {}
    bool IsActive() override;
};

// Every hunter snares what is running at it. 30s patch on a 30s cooldown, so with two hunters the
// -50% is up for the whole wave.
class FreyaTrapLashersTrigger : public Trigger
{
public:
    FreyaTrapLashersTrigger(PlayerbotAI* ai) : Trigger(ai, "freya trap lashers") {}
    bool IsActive() override;
};

// Army of the Dead on the lasher wave. Eight ghouls that AoE-taunt (43263, which filters only world
// bosses) are worth more here than the damage they do, and two lasher waves a pull fit its 10 min
// cooldown. Its own name is deliberately not "army of the dead": IsBurstCooldownAction matches on the
// action name, and the Ulduar burst gates hold everything on that list until Attuned to Nature drops,
// which is the whole add phase.
class FreyaSummonArmyTrigger : public Trigger
{
public:
    FreyaSummonArmyTrigger(PlayerbotAI* ai) : Trigger(ai, "freya summon army") {}
    bool IsActive() override;
};

// Hard mode: Freya is 2s into Ground Tremor and this bot has a cast in flight that it would eat, along
// with the 10s school lockout that comes with being cut.
class FreyaGroundTremorHoldCastTrigger : public Trigger
{
public:
    FreyaGroundTremorHoldCastTrigger(PlayerbotAI* ai) : Trigger(ai, "freya ground tremor hold cast") {}
    bool IsActive() override;
};

// Hard mode: bot is standing in an Unstable Sun Beam and must step out before it detonates.
class FreyaDodgeUnstableSunBeamTrigger : public Trigger
{
public:
    FreyaDodgeUnstableSunBeamTrigger(PlayerbotAI* ai) : Trigger(ai, "freya dodge unstable sun beam") {}
    bool IsActive() override;
};

#endif

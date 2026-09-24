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
    static constexpr char const* Name = "freya near nature bomb";

    FreyaNearNatureBombTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// The main tank, who walks Freya off a bomb field instead of standing in it. Bombs land at players'
// feet and the melee stack is on the boss, so half of every volley drops inside her melee ring.
class FreyaTankNatureBombTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya tank nature bomb";

    FreyaTankNatureBombTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Owns every DPS bot's target for the whole encounter, so the trio wave can be split three ways.
class FreyaSetDpsPriorityTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya set dps priority";

    FreyaSetDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// The main tank and assist tank 0, who both always have something on GetFreyaTankTarget's ladder.
class FreyaTankAddsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya tank adds";

    FreyaTankAddsTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Hunters and rogues, for as long as Freya is up. The action decides which tank the redirect goes to.
class FreyaRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya redirect threat";

    FreyaRedirectThreatTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class FreyaMoveToHealingSporeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya move to healing spore trigger";

    FreyaMoveToHealingSporeTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Hard mode: bot is trapped by Iron Roots (Ironbranch or Freya-cast) and must break out.
class FreyaBreakIronRootsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya break iron roots";

    FreyaBreakIronRootsTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// A Detonating Lasher next to this bot is about to blow. Melee only: they are the ones standing on the
// pile, and one below the bail line gives them about 2.6s to clear its 15 yd blast.
class FreyaLasherAboutToBlowTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya lasher about to blow";

    FreyaLasherAboutToBlowTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Ranged and healers hold one camp a fixed standoff from the pack so the lashers gather themselves
// into a pile the raid can AoE. Nothing is walked anywhere: a lasher moves at 8.0 yd/s against a
// player's 7.0.
class FreyaRangedCampTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya ranged camp";

    FreyaRangedCampTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Frost Nova roots whatever has closed on the mage for a full 8s: it carries no
// AURA_INTERRUPT_FLAG_TAKE_DAMAGE, so raid damage does not break it, and the lasher has no
// CREATURE_FLAG_EXTRA_ALL_DIMINISH, so it never diminishes either.
class FreyaFrostNovaLashersTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya frost nova lashers";

    FreyaFrostNovaLashersTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Every hunter snares what is running at it. 30s patch on a 30s cooldown, so with two hunters the
// -50% is up for the whole wave.
class FreyaTrapLashersTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya trap lashers";

    FreyaTrapLashersTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
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
    static constexpr char const* Name = "freya summon army";

    FreyaSummonArmyTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Hard mode: Freya is 2s into Ground Tremor and this bot has a cast in flight that it would eat, along
// with the 10s school lockout that comes with being cut.
class FreyaGroundTremorHoldCastTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya ground tremor hold cast";

    FreyaGroundTremorHoldCastTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Hard mode: bot is standing in an Unstable Sun Beam and must step out before it detonates.
class FreyaDodgeUnstableSunBeamTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya dodge unstable sun beam";

    FreyaDodgeUnstableSunBeamTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// The bot wearing Nature's Fury, with the raid still inside the splash. The mark ticks five times over
// 10s and every tick covers 8 yd around the carrier, so one bot standing in the ball is five volleys
// into it - 202k off a single mark on a healer, with seventeen bots in range.
class FreyaNaturesFuryBailTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya nature fury bail";

    FreyaNaturesFuryBailTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// A ranged bot or healer standing next to whoever Freya's Sunbeam is aimed at. The target itself can do
// nothing - the blast lands where it is when the cast ends, not where it was when the cast started - so
// its neighbours are the only ones with an answer.
class FreyaStepOutOfSunbeamTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya step out of sunbeam";

    FreyaStepOutOfSunbeamTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Freya has been walked off her anchor. Whoever is holding her walks her back.
class FreyaTankHoldFreyaTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya tank hold freya";

    FreyaTankHoldFreyaTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

#endif

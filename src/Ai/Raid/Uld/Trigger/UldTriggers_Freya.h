#ifndef PLAYERBOTS_ULDTRIGGERS_FREYA_H
#define PLAYERBOTS_ULDTRIGGERS_FREYA_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
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

// Owns every DPS bot's target for the whole encounter, so the trio wave can be split three ways.
class FreyaSetDpsPriorityTrigger : public Trigger
{
public:
    FreyaSetDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, "freya set dps priority") {}
    bool IsActive() override;
};

// The main tank and assist tank 0, who both always have something on the ladder in UldBossHelper.
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

// Detonating Lashers blow up for ~4-5k on death and cannot be tanked, so non-tanks that would not
// survive the blast step outside it.
class FreyaAvoidDetonatingLasherTrigger : public Trigger
{
public:
    FreyaAvoidDetonatingLasherTrigger(PlayerbotAI* ai) : Trigger(ai, "freya avoid detonating lasher") {}
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

// Ranged and healers walk a lasher that has picked them out to the corral behind Freya. Melee are
// excluded: a melee bot that ferried one would then be standing in the pile the blasts go off in.
class FreyaDragLasherToCorralTrigger : public Trigger
{
public:
    FreyaDragLasherToCorralTrigger(PlayerbotAI* ai) : Trigger(ai, "freya drag lasher to corral") {}
    bool IsActive() override;
};

// The corral has enough lashers on it to be lethal. Doubles as the return leg of the drag - there is
// no separate walk-back node - and as the mage's exit after it novas.
class FreyaLasherPackStepOutTrigger : public Trigger
{
public:
    FreyaLasherPackStepOutTrigger(PlayerbotAI* ai) : Trigger(ai, "freya lasher pack step out") {}
    bool IsActive() override;
};

// Frost Nova roots the pack for a full 8s: it carries no AURA_INTERRUPT_FLAG_TAKE_DAMAGE, so raid
// damage does not break it, and the lasher has no CREATURE_FLAG_EXTRA_ALL_DIMINISH, so it never
// diminishes either.
class FreyaFrostNovaLashersTrigger : public Trigger
{
public:
    FreyaFrostNovaLashersTrigger(PlayerbotAI* ai) : Trigger(ai, "freya frost nova lashers") {}
    bool IsActive() override;
};

// One hunter keeps a Frost Trap on the lane out of the corral. 30s patch on a 30s cooldown, so the
// -50% snare is up continuously.
class FreyaTrapLasherCorralTrigger : public Trigger
{
public:
    FreyaTrapLasherCorralTrigger(PlayerbotAI* ai) : Trigger(ai, "freya trap lasher corral") {}
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

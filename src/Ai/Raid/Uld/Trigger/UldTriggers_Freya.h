#ifndef PLAYERBOTS_ULDTRIGGERS_FREYA_H
#define PLAYERBOTS_ULDTRIGGERS_FREYA_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

class FreyaNearNatureBombTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya near nature bomb";

    FreyaNearNatureBombTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaTankNatureBombTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya tank nature bomb";

    FreyaTankNatureBombTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaSetDpsPriorityTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya set dps priority";

    FreyaSetDpsPriorityTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaTankAddsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya tank adds";

    FreyaTankAddsTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaKiteSnaplasherTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya kite snaplasher";

    FreyaKiteSnaplasherTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaAddTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya add tank defensive";

    FreyaAddTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaAddTankExternalTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya add tank external";

    FreyaAddTankExternalTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya redirect threat";

    FreyaRedirectThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaMoveToHealingSporeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya move to healing spore trigger";

    FreyaMoveToHealingSporeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaBreakIronRootsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya break iron roots";

    FreyaBreakIronRootsTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaFreeRootedAllyTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya free rooted ally";

    FreyaFreeRootedAllyTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaLasherAboutToBlowTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya lasher about to blow";

    FreyaLasherAboutToBlowTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaRangedCampTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya ranged camp";

    FreyaRangedCampTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaReachHeldPackTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya reach held pack";

    FreyaReachHeldPackTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaReachGiftTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya reach gift";

    FreyaReachGiftTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaFrostNovaLashersTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya frost nova lashers";

    FreyaFrostNovaLashersTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaTrapLashersTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya trap lashers";

    FreyaTrapLashersTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaSummonArmyTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya summon army";

    FreyaSummonArmyTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaGroundTremorHoldCastTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya ground tremor hold cast";

    FreyaGroundTremorHoldCastTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaDodgeUnstableSunBeamTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya dodge unstable sun beam";

    FreyaDodgeUnstableSunBeamTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaNaturesFuryBailTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya nature fury bail";

    FreyaNaturesFuryBailTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaStepOutOfSunbeamTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya step out of sunbeam";

    FreyaStepOutOfSunbeamTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FreyaTankHoldFreyaTrigger : public Trigger
{
public:
    static constexpr char const* Name = "freya tank hold freya";

    FreyaTankHoldFreyaTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif

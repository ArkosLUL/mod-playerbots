#ifndef PLAYERBOTS_ULDMULTIPLIERS_H
#define PLAYERBOTS_ULDMULTIPLIERS_H

#include "Define.h"
#include "Multiplier.h"
#include "RaidAntiFear.h"

// Algalon the Observer
class AlgalonMultiplier : public Multiplier
{
public:
    AlgalonMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon") {}
    virtual float GetValue(Action* action);
};

// XT-002: holds the offensive burst cooldowns until the fight's real damage window, which differs
// per mode - the first exposed Heart in hard mode, the sub-25% push in normal mode.
class XT002BurstWindowMultiplier : public Multiplier
{
public:
    XT002BurstWindowMultiplier(PlayerbotAI* ai) : Multiplier(ai, "xt002 burst window") {}
    float GetValue(Action* action) override;

private:
    // Runs against every candidate action, so the encounter sweep is memoised for the rest of the tick.
    float EvaluateWindow();

    uint32 cachedAtMs = 0;
    float cachedValue = 1.0f;
};

// XT-002 normal mode: the safety floor that keeps bots from killing the exposed Heart and flipping
// the raid into hard mode. Also keeps the class-generic threat redirects off the wrong tank.
class XT002TargetGuardMultiplier : public Multiplier
{
public:
    XT002TargetGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "xt002 target guard") {}
    float GetValue(Action* action) override;
};

// Ignis: the two places where the generic behaviour actively breaks the encounter - the Slag Pot
// victim cannot walk, and the construct tank has to stand in the fire everyone else runs from.
class IgnisMultiplier : public Multiplier
{
public:
    IgnisMultiplier(PlayerbotAI* ai) : Multiplier(ai, "ignis") {}
    float GetValue(Action* action) override;
};

// Flame Leviathan: one action owns every vehicle's movement, so the generic movers are shut out
// entirely while a bot is riding. Two actions steering one MotionMaster bounce rather than
// compromise, and lowering a priority only decides who wins each alternating tick.
class FlameLeviathanVehicleMovementMultiplier : public Multiplier
{
public:
    FlameLeviathanVehicleMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "flame leviathan vehicle movement") {}
    float GetValue(Action* action) override;
};

// Razorscale: keeps the generic movers off a bot that is clearing a Devouring Flame patch. The dodge
// action wins on priority, but it releases the tick the moment the bot is standing clear, and the
// movers then walk it straight back onto the 5yd patch it just left.
class RazorscaleMultiplier : public Multiplier
{
public:
    RazorscaleMultiplier(PlayerbotAI* ai) : Multiplier(ai, "razorscale") {}
    float GetValue(Action* action) override;

private:
    // Walks the npc list and, for the destination test, the grid. Every generic mover in the queue
    // asks the same question, so the verdict is memoised for the rest of the tick.
    bool MoversBlocked();

    uint32 cachedAtMs = 0;
    bool cachedBlocked = false;
};

// The class-generic Misdirection / Tricks nodes always redirect at the group main tank. On the
// encounters below he is not the tank holding what the raid is hitting, so the redirect is held.
class UldThreatRedirectMultiplier : public Multiplier
{
public:
    UldThreatRedirectMultiplier(PlayerbotAI* ai) : Multiplier(ai, "uld threat redirect") {}
    float GetValue(Action* action) override;
};

// Holds the offensive burst cooldowns until the encounter's real DPS check. One multiplier for every
// gated boss so the IsBurstCooldownAction early-out runs once per action rather than once per boss.
class UlduarBurstWindowMultiplier : public Multiplier
{
public:
    UlduarBurstWindowMultiplier(PlayerbotAI* ai) : Multiplier(ai, "ulduar burst window") {}
    float GetValue(Action* action) override;

private:
    // Lust is a 10-minute raid cooldown, so several bosses want it held past the point where the
    // personal cooldowns - back up within a phase - are already worth spending.
    struct BurstWindow
    {
        bool allowAll = true;
        bool allowLust = true;
    };

    // Sweeps the whole threat list, so the verdict is memoised for the rest of the tick instead of
    // being recomputed for every candidate action.
    BurstWindow EvaluateWindow();

    // Freya's final phase is six waves of adds away, so a health release keeps lust from being held
    // for the whole fight should the Attuned to Nature read ever miss.
    static constexpr float FREYA_LUST_FALLBACK_PCT = 25.0f;

    uint32 cachedAtMs = 0;
    BurstWindow cachedValue;
};

// The earth totem slot holds one totem, so the shaman's own Stoneskin / Strength of Earth nodes have
// to be held for as long as the boss can fear, or Tremor is replaced on the next GCD.
class AuriayaAntiFearTotemGuardMultiplier : public RaidAntiFearTotemGuardMultiplier
{
public:
    AuriayaAntiFearTotemGuardMultiplier(PlayerbotAI* botAI)
        : RaidAntiFearTotemGuardMultiplier(botAI, "auriaya anti fear totem guard multiplier")
    {
    }

protected:
    bool FearWindowActive() override;
};

class YoggSaronAntiFearTotemGuardMultiplier : public RaidAntiFearTotemGuardMultiplier
{
public:
    YoggSaronAntiFearTotemGuardMultiplier(PlayerbotAI* botAI)
        : RaidAntiFearTotemGuardMultiplier(botAI, "yogg-saron anti fear totem guard multiplier")
    {
    }

protected:
    bool FearWindowActive() override;
};

#endif

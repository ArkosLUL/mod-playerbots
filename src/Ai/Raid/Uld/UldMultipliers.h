#ifndef PLAYERBOTS_ULDMULTIPLIERS_H
#define PLAYERBOTS_ULDMULTIPLIERS_H

#include "Define.h"
#include "Multiplier.h"
#include "RaidAntiFear.h"

// Algalon: holds the designated Big Bang soaker's escape cooldown for the cast it is meant to
// survive. Every other priest keeps Dispersion and Guardian Spirit for their normal uses.
class AlgalonSoakCooldownReserveMultiplier : public Multiplier
{
public:
    AlgalonSoakCooldownReserveMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon soak cooldown reserve") {}
    float GetValue(Action* action) override;
};

// Algalon: Collapsing Stars are killed one at a time on purpose - each death is 16-21k to the whole
// raid - so an area attack that clips a second one undoes the pacing.
class AlgalonCollapsingStarAoeMultiplier : public Multiplier
{
public:
    AlgalonCollapsingStarAoeMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon collapsing star aoe") {}
    float GetValue(Action* action) override;
};

// Algalon: keeps the raid off a Living Constellation, which nobody but its kiter should be touching,
// and off Algalon himself in the one window where a Collapsing Star matters more than he does.
class AlgalonTargetGuardMultiplier : public Multiplier
{
public:
    AlgalonTargetGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon target guard") {}
    float GetValue(Action* action) override;
};

// Algalon: the generic movers would walk the ranged half straight off their formation slots, and the
// slot node would then re-fire next tick and pace them all fight.
class AlgalonControlMovementMultiplier : public Multiplier
{
public:
    AlgalonControlMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon control movement") {}
    float GetValue(Action* action) override;
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
// the raid into hard mode. Also stands down the generic DPS and tank targeting, which the encounter's
// own priority action replaces, the generic movers for anyone pinned in place - anchored ranged DPS
// and debuff carriers - and the class-generic threat redirects that would aim at the wrong tank.
class XT002TargetGuardMultiplier : public Multiplier
{
public:
    XT002TargetGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "xt002 target guard") {}
    float GetValue(Action* action) override;
};

// Mimiron: "mimiron set dps priority" owns every non-tank's target, so the generic picker has to
// stand down or it drags bots back onto whatever is nearest each tick.
class MimironTargetGuardMultiplier : public Multiplier
{
public:
    MimironTargetGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "mimiron target guard") {}
    float GetValue(Action* action) override;
};

// Mimiron: a gap-closer moves the bot in a straight line and consults nothing about the ground, so it
// must not fire while the bot is under orders to dodge something that kills.
class MimironChargeGuardMultiplier : public Multiplier
{
public:
    MimironChargeGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "mimiron charge guard") {}
    float GetValue(Action* action) override;
};

// Mimiron phase 1: Misdirection and Tricks of the Trade both hand their threat to the main tank by
// name, which is the one tank the Plasma Blast swap is trying to move off.
class MimironThreatRedirectGuardMultiplier : public Multiplier
{
public:
    MimironThreatRedirectGuardMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "mimiron threat redirect guard") {}
    float GetValue(Action* action) override;
};

// Ignis: the places where the generic behaviour actively breaks the encounter - the Slag Pot victim
// cannot walk, and both the construct tanks and the main tank have to stand where everyone else runs
// from.
class IgnisMultiplier : public Multiplier
{
public:
    IgnisMultiplier(PlayerbotAI* ai) : Multiplier(ai, "ignis") {}
    float GetValue(Action* action) override;
};

// Ignis: the three tanks whose spot the encounter owns keep no generic movers. Scoped to those three
// on purpose - a blanket suppression here would leave the whole raid standing still if the
// replacement mover ever failed quietly.
class IgnisTankMovementMultiplier : public Multiplier
{
public:
    IgnisTankMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "ignis tank movement") {}
    float GetValue(Action* action) override;
};

// Ignis: the encounter's own two attack nodes own every target for the whole fight. Left on, the
// generic pickers re-pick the lowest-lifetime add on any tick the settled attack returns false,
// which on this boss means walking off the Brittle construct and into the shatter blast.
class IgnisDisableDefaultTargetingMultiplier : public Multiplier
{
public:
    IgnisDisableDefaultTargetingMultiplier(PlayerbotAI* ai) : Multiplier(ai, "ignis disable default targeting") {}
    float GetValue(Action* action) override;
};

// Ignis: Flame Jets knocks the raid back and locks casting for six seconds, so a cast that cannot
// land inside the 2.7 s warning is thrown away. Instants and anything short enough keep going.
class IgnisFlameJetsHoldCastMultiplier : public Multiplier
{
public:
    IgnisFlameJetsHoldCastMultiplier(PlayerbotAI* ai) : Multiplier(ai, "ignis flame jets hold cast") {}
    float GetValue(Action* action) override;

private:
    // Runs against every candidate action, so the boss lookup is memoised for the rest of the tick.
    int32 EvaluateWindow();

    uint32 cachedAtMs = 0;
    int32 cachedRemainingMs = 0;
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

// Kologarn selects every bot's target in code, per role, so the generic target pickers have to be
// shut out entirely - otherwise "dps target" (which falls back to a smart-target strategy when no
// raid icon is set, so it is never null) fights the role assignment on alternating ticks.
class KologarnDisableAutomaticTargetingMultiplier : public Multiplier
{
public:
    KologarnDisableAutomaticTargetingMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "kologarn disable automatic targeting")
    {
    }
    float GetValue(Action* action) override;
};

// Stone Grip victims ride the right arm as stunned passengers; movement orders only fight the ride.
class KologarnMultiplier : public Multiplier
{
public:
    KologarnMultiplier(PlayerbotAI* ai) : Multiplier(ai, "kologarn") {}
    float GetValue(Action* action) override;
};

// The Iron Assembly picks every non-tank's target in code, because killing one council member
// restores the other two to full - damage the generic picker sprays across three health bars is
// simply thrown away. Suppressed for every role and for the whole fight, not only while the boss
// node has something to say: the terminal fallback is always a living council member, so nothing is
// ever left nodeless. It matches on action type alone and never asks for a role, which is what keeps
// healers out of it - IsRanged() returns true for them.
class IronAssemblyDisableAutomaticTargetingMultiplier : public Multiplier
{
public:
    IronAssemblyDisableAutomaticTargetingMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "iron assembly disable automatic targeting")
    {
    }
    float GetValue(Action* action) override;
};

// Scoped to the hazard window rather than the whole fight. A permanent guard would leave the raid
// parked wherever its last dodge ended, because the encounter's own position node yields once it
// arrives and nothing else would be left to walk anyone back - the Void Reaver failure.
class IronAssemblyMovementGuardMultiplier : public Multiplier
{
public:
    IronAssemblyMovementGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "iron assembly movement guard") {}
    float GetValue(Action* action) override;
};

// Freya picks every DPS bot's target in code so the trio wave can be split three ways. Without this
// the generic picker reclaims those targets on alternating ticks and the split never holds. The main
// tank is also fenced off generic tank assist, which would otherwise walk it off Freya onto a
// loose Storm Lasher.
class FreyaDisableAutomaticTargetingMultiplier : public Multiplier
{
public:
    FreyaDisableAutomaticTargetingMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "freya disable automatic targeting")
    {
    }
    float GetValue(Action* action) override;
};

// Backstop for the trio sync: the target split normally steers bots off a member that is too far
// ahead, so this only catches damage the targeting cannot steer - a swing mid-animation, a DoT
// already ticking.
class FreyaTrioSyncMultiplier : public Multiplier
{
public:
    FreyaTrioSyncMultiplier(PlayerbotAI* ai) : Multiplier(ai, "freya trio sync") {}
    float GetValue(Action* action) override;
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

// Stands the generic pickers down so Auriaya's own nodes own targeting and, for the anchored roles,
// movement. Without the movement half the anchor oscillates: the bot reaches its spot, the trigger
// goes quiet, a generic mover walks it off, and the trigger fires again - the failure
// RazorscaleMultiplier exists to prevent.
// Vezax pins the ranged half and the healers to derived arc slots, so the generic movers have to
// stand down for them. Melee and the tank are untouched: they hold the boss and own every generic
// mover, including the de-clump the position node falls through to.
class VezaxControlMovementMultiplier : public Multiplier
{
public:
    VezaxControlMovementMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "vezax control movement multiplier")
    {
    }

    float GetValue(Action* action) override;
};

class AuriayaMovementGuardMultiplier : public Multiplier
{
public:
    AuriayaMovementGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "auriaya movement guard multiplier") {}

    float GetValue(Action* action) override;
};


// Stands the generic pickers down so Hodir's own nodes own targeting and, for the anchored roles,
// movement. Both halves are load-bearing: without the targeting half DpsTargetValue is never null,
// so the generic node retakes the target every other tick and bots drift off the ice block that is
// about to get an ally killed; without the movement half the anchor oscillates, the failure
// RazorscaleMultiplier exists to prevent.
class HodirGuardMultiplier : public Multiplier
{
public:
    HodirGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "hodir guard multiplier") {}

    float GetValue(Action* action) override;
};

// Thorim: Runic Barrier answers every melee swing with 2000 arcane, so a bot that has backed out on
// the health band must not keep walking back in. Only the swing and the chase are held - the target
// is deliberately kept, and everything that is not a melee hit carries on from out there.
class ThorimRunicBarrierMultiplier : public Multiplier
{
public:
    ThorimRunicBarrierMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim runic barrier") {}
    float GetValue(Action* action) override;
};

// Thorim's arena squad has to keep one living body inside the box boss_thorim.cpp scans, or he
// summons the Lightning Orb and the raid dies. Unlike every other guard here this one does NOT exempt
// AttackAction or ReachTargetAction: corridor mobs sit ~92 yd from the arena centre, inside the 100 yd
// sight cap, so the chase is exactly what walks a bot out of the box. The window is "currently
// outside" and closes on re-entry, so it cannot become a permanent freeze.
class ThorimArenaLeashMultiplier : public Multiplier
{
public:
    ThorimArenaLeashMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim arena leash") {}
    float GetValue(Action* action) override;
};

// Without this the leash and the chase take turns: the leash walks the bot back, the picker still
// holds the corridor mob, and it walks straight out again. Zeroing the attack drops the target instead.
class ThorimArenaTargetGuardMultiplier : public Multiplier
{
public:
    ThorimArenaTargetGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim arena target guard") {}
    float GetValue(Action* action) override;
};

// Thorim: the phase 1 arena formation, once a bot has reached its spot. Same shape as the phase 2
// guard and for the same reason - the generic movers are what smear the squad east across the arena
// until it is standing in the corridor mouth, where boss_thorim_arena_npcs::CanAIAttack stops seeing
// it and the adds re-roll onto a healer. The window is "settled on the anchor", so a bot that gets
// knocked off it is free to walk back.
class ThorimArenaAnchorGuardMultiplier : public Multiplier
{
public:
    ThorimArenaAnchorGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim arena anchor guard") {}
    float GetValue(Action* action) override;
};

// Thorim: every melee DPS gets the "behind" strategy from AiFactory, so once the phase 2 ring node
// yields, SetBehindTargetAction walks all three stacks back into one arc behind the boss - and Chain
// Lightning jumps 5 yd here, so one arc is one chain. Scoped to a settled ring holder rather than the
// whole phase, because a permanent movement freeze is the Void Reaver failure.
class ThorimMovementGuardMultiplier : public Multiplier
{
public:
    ThorimMovementGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim movement guard") {}
    float GetValue(Action* action) override;
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

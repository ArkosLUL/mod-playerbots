/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_IGNIS_H
#define PLAYERBOTS_ULDMULTIPLIERS_IGNIS_H

#include "Define.h"
#include "Multiplier.h"

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

#endif

/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_YOGGSARON_H
#define PLAYERBOTS_ULDMULTIPLIERS_YOGGSARON_H

#include "Define.h"
#include "Multiplier.h"
#include "RaidAntiFear.h"

// Companion to yogg-saron set dps priority action: that node owns every non-tank's target from phase 2
// on, so the generic picker has to stand down rather than pull bots back onto whatever is nearest.
class YoggSaronDpsTargetGuardMultiplier : public Multiplier
{
public:
    YoggSaronDpsTargetGuardMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron dps target guard multiplier")
    {
    }

    float GetValue(Action* action) override;
};

// Everything that moves a bot somewhere nobody picked. In phase 1 the room is six fixed cloud orbits
// and two places between them that nothing reaches, so any jump is a jump onto a ring: over one pull
// 13 of 20 casts left the bot with more orbits in summon range than it started with. Blink landed on
// the fourth orbit, Disengage on the third, and the gap-closers fired straight out of the cloud-free
// circle around Sara through the first two.
//
// Blink and Disengage stay off all fight - both fire on "something is too close" rather than to close
// a gap, so neither is aimed at anything. The gap-closers come back in phases 2 and 3, where there is
// no orbit to land on.
class YoggSaronDisplacementGuardMultiplier : public Multiplier
{
public:
    YoggSaronDisplacementGuardMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron displacement guard multiplier")
    {
    }

    float GetValue(Action* action) override;
};

// Yogg's body knocks players away once a second forever inside 13.3 yd, and the walk out of that ring
// at the phase 1 to 2 handover is a forced move against reach melee's combat one. Neither wins
// outright: the two traded a melee bot back and forth every ~300 ms for the whole walk, and 77 times
// over one phase 2. So reach stands down over exactly the windows where an encounter node owns where
// the bot stands, and nowhere else - it is the only generic way a bot closes on anything, so a blanket
// zero strands the raid.
//
// Flee is the same problem from the other end of the room. Nothing in phase 1 is escaped by backing
// off: Dark Volley reaches 35 yd and the nova is a death explosion, so the 3 yd a caster gives up only
// costs it the station. One pull spent 957 of them, 96% ending further from the 21.5 yd band and a
// third landing inside 20.1 where the innermost orbit summons.
class YoggSaronMovementGuardMultiplier : public Multiplier
{
public:
    YoggSaronMovementGuardMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron movement guard multiplier")
    {
    }

    float GetValue(Action* action) override;

private:
    float FleeGuard();
    bool MeleeReachIsWrong(Action* action);
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

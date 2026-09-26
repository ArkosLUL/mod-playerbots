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
    float SetBehindGuard();
    bool MeleeReachIsWrong(Action* action);
    bool CrusherReachIsWrong();
};

// Phase 1 AoE, held while it would finish a Guardian nobody is on. With the raid on one focus, the
// Guardian next to it on the stack still lost 5.6%/s, and two of those died 2 s apart around a focus
// kill during the handover: three novas in 3.4 s on the melee pile. Heals are never held.
class YoggSaronPhase1AoeHoldMultiplier : public Multiplier
{
public:
    YoggSaronPhase1AoeHoldMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron phase 1 aoe hold multiplier")
    {
    }

    float GetValue(Action* action) override;
};

// Phase 1 walks back to the stack or the station, held in two cases:
// - A Sara's Fervor holder near a live nova. The dodge only fires inside 17 yd, so the station, the
//   leash and the reach nodes walked the bot straight back in: 31 of 44 runs were inside the blast
//   again while still holding Fervor, and three of those bots died.
// - A walk that would cross a cloud. 7 Guardians in 9 pulls came off a cloud that only a bot running
//   from Fervor or walking back from it touched. The walk waits for the cloud to pass instead.
class YoggSaronPhase1WalkGuardMultiplier : public Multiplier
{
public:
    YoggSaronPhase1WalkGuardMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron phase 1 walk guard multiplier")
    {
    }

    float GetValue(Action* action) override;

private:
    bool WalkCrossesCloud(bool station, bool reachMelee);
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

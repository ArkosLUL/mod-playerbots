#ifndef PLAYERBOTS_RAID_TOCHELPERS_OPENER_H
#define PLAYERBOTS_RAID_TOCHELPERS_OPENER_H

#include "Define.h"

class Player;
class PlayerbotAI;
class Unit;

// The first seconds of a boss belong to its tank. A DPS bot holds its damage on a boss of every
// encounter but the champions until a group tank has held that boss for TOC_OPENER_DWELL_MS, or
// TOC_OPENER_CAP_MS after it came into combat. A hunter or rogue whose Misdirection or Tricks of the
// Trade is running is free: its threat lands on the tank.
namespace TrialOfTheCrusaderHelpers
{

constexpr uint32 TOC_OPENER_DWELL_MS = 2000;
// A boss no tank picks up in this time needs a taunt, and DPS standing idle doesn't help it
constexpr uint32 TOC_OPENER_CAP_MS = 5000;

// unit is a boss still in its opener and this bot holds its damage on it
bool IsToCOpenerHeld(PlayerbotAI* botAI, Unit* unit);

// The boss this bot holds on within reach of it, for area damage. nullptr if none.
Unit* GetToCOpenerHeldBossNear(PlayerbotAI* botAI, float reach);

// This bot holds on some boss. Notes toc.opener for DPS bots.
bool IsToCOpenerHolding(PlayerbotAI* botAI);

// For the hold multiplier, no note
bool ToCOpenerHolds(PlayerbotAI* botAI);

// A hunter or rogue that holds and can redirect: the tank holding its current target, else the tank
// on any boss it holds on. nullptr if none.
Player* GetToCOpenerRedirectTank(PlayerbotAI* botAI);

}

#endif

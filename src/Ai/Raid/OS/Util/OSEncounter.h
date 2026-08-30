/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_OSENCOUNTER_H
#define _PLAYERBOT_OSENCOUNTER_H

#include "OSData.h"
#include "PlayerbotAI.h"
#include <string>

class Player;
class Unit;

// Who is doing what: the per-instance encounter blackboard, role and squad assignment, target
// selection, and the main tank's cooldown and drag latches.
namespace OsHelpers
{

// Sartharion resolution. Both go through phase-aware searches, so a bot inside the Twilight Realm
// (phase 16) will not find him - that is intended, in-realm behaviour must not depend on the boss.
Unit* GetSartharion(Player* bot);
bool SartharionEncounterActive(Player* bot);
bool SartharionDamageImmune(Player* bot);
uint32 EncounterElapsedMs(Player* bot);

bool HasTwilightShift(Unit const* unit);
// Tenebron's realm holds nothing but eggs, and they hatch into phase-1 whelps the off-tank picks up
// anyway, so the trip is not worth making. The other two are: Shadron's acolyte makes Sartharion
// immune to every school and Vesperon's torments the raid. Which realm the portal leads to cannot be
// read off the portal - all three drakes share one refcounted GameObject when Sartharion calls them -
// and the acolytes themselves are phase 16, which the grid searches filter out. So the reason to go
// in is read off what they cast on the platform instead.
bool TwilightRealmWorthEntering(Player* bot);
bool HasMoltenFury(Unit const* unit);

// A shifted bot is in phase 16 and cannot be touched by, or even see, anything on the platform.
bool OnThePlatform(Player* bot);

// The three emergency dodges, each answering "does this bot have to move for this mechanic right now".
//
// Both the trigger that fires the dodge and OsMechanicPriorityMultiplier, which zeroes every other
// mover while the mechanic is live, read these. They have to agree exactly: a multiplier claiming a
// mechanic its trigger will not fire on leaves the bot with every mover suppressed and nothing to
// replace them, which is the freeze the multiplier's own scoping comments warn about.
bool NeedsPlatformReturn(PlayerbotAI* botAI, Player* bot);
bool NeedsTsunamiDodge(Player* bot);
bool NeedsFissureDodge(Player* bot);

Player* GetOffTank(PlayerbotAI* botAI, Player* bot);
bool IsOffTank(Player* bot);
// Logs one warning per pull when the raid has no assist tank, then returns false so every off-tank
// behaviour stays inert. A half-working single-tank fallback is harder to diagnose than a bail.
bool RequireOffTank(PlayerbotAI* botAI, Player* bot);
Player* RedirectTarget(PlayerbotAI* botAI, Player* bot);
// Rogues redirect off their own target rather than off the raid-wide RedirectTarget above: Tricks moves
// everything for 6s, so it has to land on the tank of the thing the rogue is actually hitting - and
// never on a Lava Blaze, which dies to one Fan of Knives and takes the 30s cooldown with it. Sartharion
// needs no pull-window test of its own; his tank is the main tank for the whole fight. Hunters keep
// RedirectTarget, because Misdirection is three shots rather than a blanket window and the off-tank
// pulling a whelp off a healer is worth one.
Player* RedirectTankFor(PlayerbotAI* botAI, Player* bot);
// A landed drake that is not on the off-tank, nearest first. He attacks only the newest one, so without
// this nothing pulls back the drake he walks away from at a handover - and by the time it peels it is
// 35.5yd behind him at the far spot, outside the taunt range this searches with.
Unit* OffTankTauntTarget(Player* bot);
// The bot's own single-target taunt, or nullptr for a class that has none. The four names the
// multiplier already lists one by one, from the other side.
char const* TauntSpellFor(Player* bot);

// The first two healers by GUID.
constexpr size_t PORTAL_SQUAD_HEALERS = 2;

// Fixed at the pull by stable GUID sort and never reshuffled: every DPS, melee and ranged alike, the
// first two healers, and the second off-tank if the raid has one. The acolytes are worth the whole
// raid's damage - Shadron's makes Sartharion immune to every school - and the platform survives the
// trip, because the main tank keeps the boss and the first off-tank keeps the drakes.
bool PortalSquadMember(Player* bot);
bool TwilightAddsAlive(Player* bot);

// Shadron once it is on the ground, or nullptr while it is still circling: drakes carry
// UNIT_FLAG_NOT_SELECTABLE for their whole flight and clear it on POINT_LANDING.
Unit* LandedShadron(Player* bot);
// True once Shadron is down for good - dead, or never called at all. Power of Shadron is on the raid
// from the pull and permanent, so "no drake and no aura" is what separates gone from still in the air.
bool ShadronGone(Player* bot);

// One-way, latched per encounter: true once Shadron is on the ground. Burst is spent once, so the
// window does not close again when he dies. Sartharion is 60s into the fight by then and the Gift of
// Twilight interlock parks Bloodlust for as long as Shadron's acolyte lives, so it lands on the first
// moment the raid can actually damage the boss with a second drake up.
bool SartharionBurstWindowOpen(Player* bot);

// The window the main tank holds his cooldowns for, latched on Shadron and unlatched on his own health.
bool MainTankCooldownWindowOpen(Player* bot);
// The weakest cooldown he can cast right now, or nullptr - either because none is off cooldown or
// because one is already running. Ordered by cooldown length, which is what "weakest" means here.
char const* NextTankDefensive(PlayerbotAI* botAI, Player* bot);
// True for the cast names in that table, so the class nodes can be held off them.
bool IsHeldTankDefensive(std::string const& actionName);

// One-shot, latched per encounter: true once Sartharion has come south with the pull drag, or once
// the tank has given up waiting for him. EncounterState clears itself 15s after combat ends, so a
// wipe re-arms it.
bool MainTankDragDone(Unit* boss);
void SetMainTankDragged(Unit* boss);
// When the tank reached the drag corner, or 0 while he is not standing on it. Cleared rather than
// kept when he is pushed off, so a tsunami mid-wait costs the whole dwell instead of banking it.
uint32 MainTankDragArrivedMs(Unit* boss);
void SetMainTankDragArrivedMs(Unit* boss, uint32 arrivedMs);
// When the drag started, stamped on its first tick. The timeout runs off this rather than off the
// encounter clock, so it means what it says: the tank has been trying to drag for this long.
uint32 MainTankDragStartedMs(Unit* boss);
void SetMainTankDragStartedMs(Unit* boss, uint32 startedMs);

Unit* PriorityTarget(Player* bot);
Unit* TranquilizeTargetFor(Player* bot);

}

#endif

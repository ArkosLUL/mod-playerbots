/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_LICHKING_H
#define PLAYERBOTS_ICCHELPERS_LICHKING_H

#include <vector>

#include "Define.h"
#include "ICCData.h"
#include "Position.h"
#include "RaidBurstWindow.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

// icecrown_citadel.h: his guid slot and the Frozen Throne platform's
constexpr uint32 LK_DATA_THE_LICH_KING = 12;
constexpr uint32 LK_DATA_ARTHAS_PLATFORM = 38;

// GetEntry reports these on every difficulty
constexpr uint32 NPC_LK_DRUDGE_GHOUL = 37695;
constexpr uint32 NPC_LK_SHAMBLING_HORROR = 37698;
constexpr uint32 NPC_LK_RAGING_SPIRIT = 36701;
constexpr uint32 NPC_LK_ICE_SPHERE = 36633;
constexpr uint32 NPC_LK_SHADOW_TRAP = 39137;
constexpr uint32 NPC_LK_VALKYR = 36609;
constexpr uint32 NPC_LK_VILE_SPIRIT = 37799;
constexpr uint32 NPC_LK_WICKED_SPIRIT = 39190;
constexpr uint32 NPC_LK_SPIRIT_WARDEN = 36824;

// Difficulty rows: remap at the call
constexpr uint32 SPELL_LK_NECROTIC_PLAGUE = 70337;
constexpr uint32 SPELL_LK_NECROTIC_PLAGUE_JUMP = 70338;
constexpr uint32 SPELL_LK_SOUL_REAPER = 69409;
constexpr uint32 SPELL_LK_SOUL_SHRIEK = 69242;
constexpr uint32 SPELL_LK_SHAMBLING_ENRAGE = 72143;
// No difficulty row: one id on every difficulty
constexpr uint32 SPELL_LK_PLAGUE_SIPHON = 74074;
constexpr uint32 SPELL_LK_SHAMBLING_FRENZY = 28747;
constexpr uint32 SPELL_LK_TRANQUILIZING_SHOT = 19801;
// He sits with it until Tirion's intro stands him up, and again after a wipe
constexpr uint32 SPELL_LK_SIT = 73220;

// The platform centre C and the Frostmourne room's F. Stands are polar from C: degrees from +x (north)
// toward +y (west), then yards.
inline Position const LK_CENTER(503.63f, -2124.66f, 840.857f);
inline Position const LK_FROSTMOURNE(495.71f, -2523.76f, 1049.95f);

// The Lich King's spelldifficulty rows, 10N/25N/10H/25H. The first Remorseless Winter (70%) and the
// second (40%) are separate rows, which is how the phase tells them apart.
inline constexpr uint32 ICC_LK_FIRST_WINTER[]  = {68981, 74270, 74271, 74272};
inline constexpr uint32 ICC_LK_SECOND_WINTER[] = {72259, 74273, 74274, 74275};
// Normal mode's single-player Harvest Soul, and heroic's Harvest Souls on the whole raid
inline constexpr uint32 ICC_LK_HARVEST_SOUL[]  = {68980, 74325, 74326, 74327};
inline constexpr uint32 ICC_LK_HARVEST_SOULS[] = {73654, 74295, 74296, 74297};

// Ordered, so "past the first threshold" reads phase >= Winter1.
enum class IccLichKingPhaseId : uint8
{
    One,
    Winter1,
    Two,
    Winter2,
    Three,
    Frostmourne,
    Outro,
};

// The Lich King's phase as his script runs it, for the bot asking: a winter phase from the turn to
// passive at 70% or 40% until he turns aggressive 5 s after the Quake that ends it, Frostmourne while
// heroic's Harvest Souls holds him passive, the second after the return included, Outro once Fury of
// Frostmourne disallows combat. Publishes
// lk.phase. A unit that is not the Lich King reads One.
IccLichKingPhaseId IccLichKingPhase(Player* bot, Unit* boss);
// Inside the Frostmourne room, the 40 yd round its centre that the script itself tests.
bool IccLichKingInFrostmourne(Unit const* unit);

// 1 for the first Remorseless Winter, 2 for the second, 0 for none: the aura or its 2.5 s cast.
uint8 IccRemorselessWinterOf(Unit* boss);
bool IccBossHasRemorselessWinter(Unit* boss);
bool IccBossCastingRemorselessWinter(Unit* boss);

// His unit from guid slot 12, alive, with the bot on map 631 within 120 yd of C or 50 yd of F
Creature* GetLkBoss(PlayerbotAI* botAI);
// He is in combat, or in the outro, where combat is disallowed and he is out of it
bool IsLkLive(PlayerbotAI* botAI);

// The ring past the crack (49-62 yd from C) stands while the platform is not DAMAGED, from the Quake's
// shatter until the second winter's cast lands
bool IsLkRingStandable(Player* bot);
// Where a walk may end: near F within 20 yd of it, else within 44 yd of C or on a standable ring. Both floors
// are gameobject collision only, so a spot must also read within 2 yd of the floor, once the bot's own does.
bool LkFloorAccepts(Player* bot, float x, float y);

enum class LkTankRole : uint8
{
    None = 0,
    Holder = 1,
    Free = 2,
    StandIn = 3
};

// From the last duty pass. The holder is his victim when a group tank, else the last holder while it
// lives, else the group's main tank, and can be a player. The free tank is the first other bot tank by guid.
// With no other tank player in phase one, a DK or paladin damage dealer stands in to hold the ghouls.
LkTankRole GetLkTankRole(PlayerbotAI* botAI, Player* player);
Player* GetLkHolder(PlayerbotAI* botAI);
Player* GetLkFreeTank(PlayerbotAI* botAI);

// Alive, in the world on the bot's own instance, the bot included
std::vector<Player*> LkMembersOnMap(Player* bot);
// Players hold no duty, and a selfbot counts as a player here
bool IsLkDutyBot(Player* player);
bool IsLkTankPlayer(Player* player);
bool IsLkHealer(Player* player);

// The stun or slow this bot can land on target right now, else 0: known, ready, paid for, right form, in
// range and sight, and a stun only below the target's immune diminishing level. Intimidation and a ghoul's
// Gnaw count with the pet in melee of target. Cast it with LkCastControl.
uint32 LkReadyStun(Player* member, Unit* target);
uint32 LkReadySlow(Player* member, Unit* target);
// Casts what LkReadyStun or LkReadySlow handed out: Gnaw through the ghoul, Intimidation on the hunter
// with its pet sent at target, anything else straight at target
bool LkCastControl(PlayerbotAI* botAI, Unit* target, uint32 spellId);

// Tanks and DPS: a player's skull, the outro, the late phases' picks, then the winter and phase one
// rules. Nothing for a bot on a vehicle, stunned by Harvest Soul or braked off him. Notes lk.target.
Unit* GetLkTarget(PlayerbotAI* botAI);
// A tank or DPS bot on a vehicle, stunned by Harvest Soul, braked with nothing to hit, or held by the late
// phases
bool IsLkTargetHeld(PlayerbotAI* botAI);
// A quake, trap, plague or stand walk, parked or in flight, or the quake hold
bool IsLkMovementOwned(PlayerbotAI* botAI);
// While he is live the class tank buttons wait for the encounter's windows, down to 25% health
bool IsLkTankDefensiveHeld(PlayerbotAI* botAI);

// Each walk answers true with its spot while it is the bot's current walk and the bot has not parked
bool GetLkQuakeWalk(PlayerbotAI* botAI, Position& spot);
bool GetLkTrapWalk(PlayerbotAI* botAI, Position& spot);
bool GetLkPlagueWalk(PlayerbotAI* botAI, Position& spot);
bool GetLkStandWalk(PlayerbotAI* botAI, Position& spot);
// The stand is a winter slot or a spirit spot: walked forced, with Nitro Boosts
bool IsLkStandUrgent(PlayerbotAI* botAI);

// The button this tank spends now, 0 for none. Notes under noteKind.
uint32 GetLkTankDefensive(PlayerbotAI* botAI, char const* noteKind);
// The unit this tank's elected taunt is due on, in range and sight with the taunt ready. A nonzero
// `aoeSpell` is Challenging Shout or Roar, cast on the tank itself, in place of the class taunt.
Unit* GetLkTauntTarget(PlayerbotAI* botAI, uint32& aoeSpell);
// The plague carrier this curer dispels now, and the spell
Unit* GetLkCureTarget(PlayerbotAI* botAI, uint32& spellId);
// The enraged Shambling Horror this hunter's Tranquilizing Shot is due on
Unit* GetLkTranquilizeTarget(PlayerbotAI* botAI);
// The Frenzied Shambling Horror this bot's stun is due on, and the stun
Unit* GetLkStunTarget(PlayerbotAI* botAI, uint32& spellId);
// The member this priest or paladin frees from Soul Shriek's silence, and the spell
Unit* GetLkSilenceDispelTarget(PlayerbotAI* botAI, uint32& spellId);
// Past 90% of the holder's threat on him, until under 80%
bool IsLkBraked(PlayerbotAI* botAI);
// A plague carrier is on the map: no cure but the elected one may reach it
bool IsLkCureHeld(PlayerbotAI* botAI);
// A shaman bot's Cleansing Totem is down while a carrier lives: the water totem that replaces it
char const* GetLkTotemSwap(PlayerbotAI* botAI);
bool IsLkPlagueCarrier(Unit* unit);
// Every live phase one add while phase one runs, for the splash hold
void GetLkSplashAdds(PlayerbotAI* botAI, std::vector<Position>& out);

// The scan, the duties, the tank roster and taunts, the per-bot walks and brake, the calls to a human
// tank, the heroic buffs, the floor rescue and the reset after a wipe
void TickLkEncounter(PlayerbotAI* botAI);

// Lust in phase one, the damage check while Plague Siphon ramps
RaidBurstWindow IccLichKingBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& GetLkOpenerBosses(PlayerbotAI* botAI);
// Shadow for most of his damage, frost for the winters
uint8 IccLichKingResistSchools(PlayerbotAI* botAI, uint8 previous);

#endif

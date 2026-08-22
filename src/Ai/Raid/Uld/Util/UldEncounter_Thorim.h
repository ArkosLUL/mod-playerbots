/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERTHORIM_H
#define PLAYERBOTS_ULDENCOUNTERTHORIM_H

#include "ObjectGuid.h"
#include "Position.h"
#include "RaidObs.h"
#include "UldBossHelper.h"

#include <unordered_map>
#include <unordered_set>

class Player;
class PlayerbotAI;
class Unit;
class WorldObject;

// Thorim.
//
// Two halves that share nothing. The gauntlet is a corridor walk past the Runic Colossus, whose
// Runic Smash rolls a blast wave up one side at a time and whose Runic Barrier answers every melee
// swing with 2000 arcane. Phase 2 is a stationary fight on the arena floor where Chain Lightning
// arcs 5 yd from victim to victim and Lightning Charge fires a 75 degree cone at whichever pillar
// orb just lit up.

// Both scans below are raid-wide answers, so they are folded once per instance per interval instead
// of once per bot - and sharing the result is also what guarantees no two bots disagree about which
// corridor lane is hot.
struct ThorimEncounterState
{
    // Held rather than re-derived, for the reason Vezax holds his: ranking the raid by guid every
    // tick means one death renumbers everyone behind the corpse and the ring shuffles mid-fight.
    RaidObs::ObsGuidMap<uint8> meleeSlots{"thorim.slot"};

    // 0 = nothing seen yet, otherwise SPELL_THORIM_RUNIC_SMASH_LEFT / _RIGHT. The side is sticky and
    // the timestamp is not: the timestamp says the wave is still rolling, the side says which lane
    // the squad now walks, and that has to outlive the wave or the formation walks straight back.
    RaidObs::ObsValue<uint32> runicSmashSide{"thorim.smashside"};
    uint32 runicSmashSeenMs = 0;
    uint32 smashScanMs = 0;

    RaidObs::ObsValue<ObjectGuid> chargedOrbGuid{"thorim.chargedorb"};
    uint32 orbScanMs = 0;

    ObjectGuid colossusGuid;
    uint32 colossusScanMs = 0;

    RaidObs::ObsGuidSet barrierBailing{"thorim.barrierbail"};

    // Which half of the raid each member belongs to, struck once and then left alone. Recomputing it
    // per tick is what let a role predicate flipping mid-fight walk the arena squad into the corridor.
    RaidObs::ObsGuidMap<uint8> squads{"thorim.squad"};
    RaidObs::ObsValue<bool> squadsAssigned{"thorim.squadsassigned"};

    // Bots the arena node took "follow master" away from, so the reset can hand it back.
    RaidObs::ObsGuidSet followMasterStripped{"thorim.followstripped"};

    // Phase 1 arrival latch. Its own set rather than ringArrived below: the two phases are mutually
    // exclusive on the z threshold today, and sharing a latch across that is a trap waiting for the
    // first time it stops being true.
    RaidObs::ObsGuidSet arenaAnchorArrived{"thorim.arenaarrived"};

    // Reach-then-hold latch. A bot that has arrived stops issuing moves until it drifts past the
    // wider tolerance, because a moving bot casts nothing.
    RaidObs::ObsGuidSet ringArrived{"thorim.ringarrived"};

    // The corridor fight is trash to the instance script, so nothing else opens a trace for it.
    bool gauntletTraced = false;

    // Whether this raid has ever had him in combat. An untouched Thorim looks identical to one that
    // has just reset, and the reset path clears the squad split that the corridor forms up on before
    // the pull - so without the latch the two fight each other every tick.
    bool engagedSeen = false;
};

enum class ThorimSquad : uint8
{
    None,
    Arena,
    Gauntlet
};

enum class ThorimPhase2Role : uint8
{
    None,
    MainTank,
    OffTank,
    Ranged,
    MeleeRing
};

// By entry, never "find target": that value walks only the bot's own threat list and matches on the
// localized creature name, so a bot that is on an add goes blind to the boss.
Unit* GetThorim(PlayerbotAI* botAI);

// Wider, targeted lookup rather than the sight-capped target values: the Colossus is 131 yd from the
// top pair of corridor waypoints, and that is exactly where its telegraph has to be visible.
Unit* GetThorimRunicColossus(PlayerbotAI* botAI);

//
// Corridor gauntlet
//
constexpr uint8 ULDUAR_THORIM_GAUNTLET_WAYPOINTS = 6;

// The two lanes are index-matched by y - entry i on one side is the same progress down the corridor
// as entry i on the other - which is what makes a lane swap a straight index map. Each lane is
// inside its own hand's 10 yd blast and 15.5-22.9 yd clear of the other's.
Position const& GetThorimGauntletWaypoint(bool leftLane, uint8 index);

// True when `who` is standing at one of this lane's waypoints, reporting the nearest one.
bool ThorimGauntletLaneIndexInLane(WorldObject const* who, bool leftLane, uint8& index);
bool ThorimGauntletLaneIndex(WorldObject const* who, uint8& index, bool& leftLane);

// The waypoint index the squad is at: the master's, or this bot's own when the master matches no
// waypoint at all - walking the centre line, or off fighting an add.
bool ThorimResolveGauntletIndex(PlayerbotAI* botAI, Player* bot, uint8& index);

// The lane the squad should be in. Sticky once a hand has gone up, so the formation does not walk
// everyone back into the lane they just dodged out of; false before the first telegraph, and once
// the Colossus is engaged, because EVENT_RC_RUNIC_SMASH is cancelled in JustEngagedWith.
bool ThorimPreferredGauntletLane(PlayerbotAI* botAI, bool& useLeftLane);

// True only while a wave is still rolling, which is what separates the urgent dodge from the
// standing lane preference.
bool ThorimRunicSmashImminent(PlayerbotAI* botAI);

// Runic Barrier is recast every 20s for its own 20s duration, so "stop attacking while it is up"
// would mean never attacking. Non-tank melee back out on a health band instead and keep their
// target, so ranged and instant abilities carry on from outside the shield's reach.
bool ThorimBarrierBailLatched(PlayerbotAI* botAI, Player* bot);

//
// Phase 1 split
//
// The raid fights phase 1 in two halves and the arena half must never empty out: boss_thorim.cpp
// scans a fixed box every 5 seconds and summons a raid-killing Lightning Orb the first time it finds
// nobody alive inside it. There is no grace period, so this is a hard constraint rather than a
// preference.
//
// Set MEMBER_FLAG_MAINTANK in the raid frame. Without it GetMainTankGuid falls back to the first tank
// in roster order, and a human tank ahead of the bot main tank silently takes the role - which sends
// the bot main tank down the corridor and leaves the arena untanked.
bool ThorimSplitActive(PlayerbotAI* botAI);

// Latched on first ask and stable for the rest of the pull. Anyone with no entry - a late arrival -
// holds the arena, because that is the side that cannot wipe the raid by being short.
ThorimSquad GetThorimSquad(PlayerbotAI* botAI, Player* bot);

// The boss script's own box, so the two sides cannot disagree about what counts as "in the arena".
bool ThorimInArenaBox(WorldObject const* who);

// Melee are held to the tank spot and everyone else to the wider box radius, because melee are the
// only ones who have to walk out to an add at all.
bool ThorimArenaLeashBreached(PlayerbotAI* botAI, Player* bot);

// Where this bot stands in the arena. The tank holds the centre, ranged and healers get a ring slot
// around him, and melee get the centre only while out of combat - pinning them in the fight would
// cost uptime, and the leash is what holds them there instead. Trigger and action both go through
// here so they cannot disagree.
//
// Not gated on ThorimSplitActive: that also requires combat, and the squad has to walk to its own
// side of the room before the pull rather than after it.
bool GetThorimArenaAnchor(PlayerbotAI* botAI, Player* bot, Position& out);

// Updates this bot's arrival latch and reports whether it still needs to walk. Idempotent, so the
// trigger and the action can both ask.
bool ThorimArenaAnchorNeedsMove(PlayerbotAI* botAI, Player* bot, Position const& spot);

// A settled anchor holder, which is the only window the movement guard covers. Outside it the generic
// movers are what bring a bot back, and freezing them permanently is the Void Reaver failure.
bool ThorimArenaAnchorSettled(PlayerbotAI* botAI, Player* bot);

void ThorimNoteFollowMasterStripped(Player* bot);
bool ThorimFollowMasterStripped(Player const* bot);

//
// Phase 2
//
bool ThorimPhase2Active(PlayerbotAI* botAI);
ThorimPhase2Role GetThorimPhase2Role(PlayerbotAI* botAI, Player* bot);
bool TryGetThorimPhase2Spot(PlayerbotAI* botAI, Player* bot, ThorimPhase2Role role, Position& position);

// Updates this bot's arrival latch and reports whether it still needs to walk. Idempotent, so the
// trigger and the action can both ask.
bool ThorimRingNeedsMove(PlayerbotAI* botAI, Player* bot, Position const& spot);

// A settled ring holder, which is the only window the movement guard covers. Outside it the generic
// movers are what bring a bot back, and freezing them permanently is the Void Reaver failure.
bool ThorimMeleeRingSettled(PlayerbotAI* botAI, Player* bot);

// The orb Thorim is about to fire at, or nullptr. Lightning Charge itself is instant with no cast
// bar; the aura landing on a Thunder Orb is the entire 5 second warning.
Unit* ThorimChargedThunderOrb(PlayerbotAI* botAI);
bool ThorimLightningChargeActive(PlayerbotAI* botAI);

void ResetThorimEncounterState(Player* bot, bool clearInstance);
bool ThorimEncounterStateIsStale(PlayerbotAI* botAI);
// Nothing to reset means nothing to do, which keeps the reset node from swallowing every tick before
// the pull - Thorim sits at full health on his balcony for the whole gauntlet.
bool ThorimBotHasEncounterState(Player* bot);

#endif

/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_BLOODQUEEN_H
#define PLAYERBOTS_ICCHELPERS_BLOODQUEEN_H

#include <vector>

#include "Define.h"
#include "Position.h"
#include "RaidBurstWindow.h"
#include "RaidDefensive.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

// icecrown_citadel.h CreaturesIds. GetEntry reports these on every difficulty.
constexpr uint32 NPC_BQL_BLOOD_QUEEN = 37955;
constexpr uint32 NPC_BQL_SWARMING_SHADOWS = 38163;
// icecrown_citadel.h DataTypes DATA_BLOOD_QUEEN_LANA_THEL: her boss id and her guid slot
constexpr uint32 BQL_DATA_BLOOD_QUEEN = 8;

// Difficulty rows: remap at the call
constexpr uint32 SPELL_BQL_ESSENCE = 70867;
constexpr uint32 SPELL_BQL_ESSENCE_BITTEN = 70879;
constexpr uint32 SPELL_BQL_FRENZIED_BLOODTHIRST = 70877;
// The vampire's own bite. An override spell, so it is never in the spellbook and a name finds nothing.
constexpr uint32 SPELL_BQL_VAMPIRIC_BITE = 70946;
// Bloodbolt Whirl's bolt on each player it picks
constexpr uint32 SPELL_BQL_WHIRL_BOLT = 71446;
constexpr uint32 SPELL_BQL_SHROUD_OF_SORROW = 70985;
// No difficulty row: one id on every difficulty
constexpr uint32 SPELL_BQL_UNCONTROLLABLE_FRENZY = 70923;
constexpr uint32 SPELL_BQL_PACT = 71340;
// The 2 s warning, then the 6 s trail that drops a shadow every 0.5 s
constexpr uint32 SPELL_BQL_SHADOWS_WARNING = 71264;
constexpr uint32 SPELL_BQL_SHADOWS_TRAIL = 71265;
// On the linked player, who takes her victim's damage
constexpr uint32 SPELL_BQL_BLOOD_MIRROR_LINK = 70838;
constexpr uint32 SPELL_BQL_BLOODBOLT_WHIRL = 71772;
// Heroic: a stack on her per bite, +5% damage each
constexpr uint32 SPELL_BQL_PRESENCE = 70995;

// Where she walks for the air phase, on the floor
inline Position const ICC_BQL_ROOM_CENTER(4595.709f, 2769.419f, 400.138f);
// On the dais, 3.5 yd short of its north edge. She stops about 9.2 yd short of a tank she chases, so she
// stands on the ramp at (4621.51, 2769.42, 401.32).
inline Position const ICC_BQL_TANK_SPOT(4630.709f, 2769.419f, 401.748f);
// The mirror tank stands this far beside a bot main tank parked on the tank spot
constexpr float BQL_MIRROR_OFFSET = 1.5f;
// 6 yd south of the centre, then 8 yd either side when a shadow fouls it
inline Position const ICC_BQL_PACT_SPOTS[] = {
    {4589.709f, 2769.419f, 400.137f}, {4589.709f, 2777.419f, 400.136f}, {4589.709f, 2761.419f, 400.137f}};
constexpr uint8 BQL_PACT_SPOT_COUNT = sizeof(ICC_BQL_PACT_SPOTS) / sizeof(ICC_BQL_PACT_SPOTS[0]);

// One triangular lattice with 7.5 yd sides, 6 yd Bloodbolt Splash plus two 0.75 yd park tolerances, within
// 25 yd of the room centre so a shadow on the inner lane misses it. The first BQL_GROUND_NODES keep 12 yd
// off the first Pact spot and 9 yd off her three melee group spots, nearest her first; the rest are air
// nodes only. navprobe --nav 0x09 settles every node within 0.1 yd of its Z, and a 3x3 grid at 1 yd round
// each sees her ramp spot and her air spot 22 yd above the centre; those in 38 yd of the tank spot see it
// and the group spots too.
inline Position const ICC_BQL_NODES[] = {
    {4608.709f, 2777.664f, 400.138f}, {4604.959f, 2771.169f, 400.136f}, {4612.459f, 2784.159f, 400.139f},
    {4604.959f, 2758.179f, 400.138f}, {4601.209f, 2764.674f, 400.137f}, {4608.709f, 2751.683f, 400.138f},
    {4601.209f, 2777.664f, 400.136f}, {4604.959f, 2784.159f, 400.135f}, {4608.709f, 2790.655f, 400.136f},
    {4597.459f, 2758.179f, 400.138f}, {4601.209f, 2751.683f, 400.138f}, {4597.459f, 2784.159f, 400.138f},
    {4601.209f, 2790.655f, 400.136f}, {4593.709f, 2751.683f, 400.138f}, {4597.459f, 2745.188f, 400.138f},
    {4589.959f, 2784.159f, 400.136f}, {4593.709f, 2790.655f, 400.137f}, {4586.209f, 2751.683f, 400.138f},
    {4589.959f, 2745.188f, 400.138f}, {4582.459f, 2758.179f, 400.138f}, {4586.209f, 2790.655f, 400.135f},
    {4582.459f, 2784.159f, 400.137f}, {4578.709f, 2777.664f, 400.139f}, {4578.709f, 2751.683f, 400.138f},
    {4574.959f, 2771.169f, 400.138f}, {4574.959f, 2758.179f, 400.138f}, {4571.209f, 2764.674f, 400.138f},
    {4619.959f, 2771.169f, 400.734f}, {4616.209f, 2764.674f, 400.138f}, {4612.459f, 2771.169f, 400.138f},
    {4616.209f, 2777.664f, 400.138f}, {4608.709f, 2764.674f, 400.137f}, {4612.459f, 2758.179f, 400.138f},
    {4597.459f, 2771.169f, 400.138f}, {4593.709f, 2764.674f, 400.138f}, {4593.709f, 2777.664f, 400.137f},
    {4589.959f, 2771.169f, 400.137f}, {4589.959f, 2758.179f, 400.137f}, {4586.209f, 2764.674f, 400.137f},
    {4586.209f, 2777.664f, 400.135f}, {4582.459f, 2771.169f, 400.136f}, {4578.709f, 2764.674f, 400.139f},
};
constexpr uint8 BQL_NODE_COUNT = sizeof(ICC_BQL_NODES) / sizeof(ICC_BQL_NODES[0]);
constexpr uint8 BQL_GROUND_NODES = 27;
constexpr float BQL_NODE_SPACING = 7.5f;

// Swarming Shadows lanes, arcs of 35.5 and 30.5 yd round the room centre from 40 degrees west of north round
// the south end to 40 degrees east of it, a point every 5 yd. The walls stand at 37.5-39 yd on the sides and past
// 41 yd at the south end. navprobed like the nodes.
inline Position const ICC_BQL_LANE_OUTER[] = {
    {4622.904f, 2792.238f, 400.138f}, {4619.431f, 2795.830f, 400.138f}, {4615.489f, 2798.898f, 400.138f},
    {4611.154f, 2801.383f, 400.137f}, {4606.514f, 2803.235f, 400.138f}, {4601.660f, 2804.417f, 400.138f},
    {4596.688f, 2804.905f, 400.138f}, {4591.697f, 2804.692f, 400.138f}, {4586.785f, 2803.779f, 400.138f},
    {4582.050f, 2802.186f, 400.137f}, {4577.586f, 2799.944f, 400.138f}, {4573.480f, 2797.098f, 400.138f},
    {4569.815f, 2793.703f, 400.138f}, {4566.662f, 2789.828f, 400.138f}, {4564.085f, 2785.548f, 400.138f},
    {4562.134f, 2780.949f, 400.138f}, {4560.847f, 2776.121f, 400.138f}, {4560.252f, 2771.161f, 400.138f},
    {4560.358f, 2766.166f, 400.138f}, {4561.165f, 2761.236f, 400.138f}, {4562.656f, 2756.468f, 400.138f},
    {4564.801f, 2751.956f, 400.138f}, {4567.559f, 2747.790f, 400.138f}, {4570.874f, 2744.053f, 400.138f},
    {4574.680f, 2740.817f, 400.138f}, {4578.904f, 2738.149f, 400.138f}, {4583.460f, 2736.099f, 400.138f},
    {4588.259f, 2734.710f, 400.138f}, {4593.205f, 2734.007f, 400.138f}, {4598.201f, 2734.007f, 400.138f},
    {4603.147f, 2734.707f, 400.138f}, {4607.947f, 2736.095f, 400.138f}, {4612.503f, 2738.143f, 400.138f},
    {4616.728f, 2740.810f, 400.138f}, {4620.536f, 2744.044f, 400.138f},
};
inline Position const ICC_BQL_LANE_INNER[] = {
    {4619.073f, 2789.024f, 400.138f}, {4615.561f, 2792.574f, 400.139f}, {4611.515f, 2795.504f, 400.136f},
    {4607.046f, 2797.734f, 400.137f}, {4602.274f, 2799.204f, 400.137f}, {4597.325f, 2799.876f, 400.138f},
    {4592.332f, 2799.731f, 400.137f}, {4587.430f, 2798.774f, 400.137f}, {4582.751f, 2797.029f, 400.136f},
    {4578.418f, 2794.544f, 400.138f}, {4574.550f, 2791.386f, 400.139f}, {4571.248f, 2787.638f, 400.138f},
    {4568.603f, 2783.402f, 400.138f}, {4566.684f, 2778.790f, 400.138f}, {4565.544f, 2773.928f, 400.138f},
    {4565.213f, 2768.945f, 400.138f}, {4565.699f, 2763.974f, 400.138f}, {4566.990f, 2759.149f, 400.138f},
    {4569.051f, 2754.600f, 400.138f}, {4571.827f, 2750.448f, 400.138f}, {4575.243f, 2746.805f, 400.138f},
    {4579.208f, 2743.768f, 400.138f}, {4583.616f, 2741.419f, 400.138f}, {4588.348f, 2739.821f, 400.138f},
    {4593.277f, 2739.016f, 400.138f}, {4598.271f, 2739.027f, 400.138f}, {4603.197f, 2739.852f, 400.138f},
    {4607.922f, 2741.471f, 400.138f}, {4612.319f, 2743.839f, 400.138f}, {4616.271f, 2746.892f, 400.138f},
};

// A stand parks within BQL_STAND_PARK and walks back once pushed past BQL_STAND_UNPARK
constexpr float BQL_STAND_PARK = 0.75f;
constexpr float BQL_STAND_UNPARK = 2.5f;
static_assert(BQL_STAND_PARK < BQL_STAND_UNPARK, "a walk has to end inside its own trigger");
static_assert(BQL_NODE_SPACING >= 6.0f + 2 * BQL_STAND_PARK, "two parked neighbours must not share a splash");

// Any tank under BQL_TANK_DEFENSIVE_PCT spends a button. Under BQL_TANK_LAST_RESORT_PCT, or in the last
// minute before Berserk, its class buttons are free again.
constexpr float BQL_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float BQL_TANK_LAST_RESORT_PCT = 25.0f;

enum class IccBqlPhase : uint8
{
    Ground,
    Center,  // passive and walking to the room's centre, then Incite Terror there
    Air,     // gravity off, from takeoff until she lands
};

// From her own air phase: REACT_PASSIVE from the walk to the centre until she lands, gravity off from
// takeoff to landing. She carries CREATURE_FLAG_EXTRA_NO_MOVE_FLAGS_UPDATE, so only her script moves
// the gravity flag. Publishes bql.phase.
IccBqlPhase IccBqlPhaseOf(Player* bot, Unit* boss);

// She is alive and in combat and the bot is in her room. Her engage puts the whole map in combat with her.
bool IsBqlLive(PlayerbotAI* botAI);
Creature* GetBqlQueen(PlayerbotAI* botAI);

// Tanks and DPS: a player's skull on her or a mind-controlled vampire, else her. Nothing for a frenzied bot,
// nor for a braked one but a skulled vampire. Notes bql.target.
Unit* GetBqlTarget(PlayerbotAI* botAI);
// Her, for the main tank she has left, its taunt ready and her below taunt DR level 3
Unit* GetBqlTauntTarget(PlayerbotAI* botAI);
// Past the threat brake, or a tank that isn't the main tank before the first vampire exists
bool IsBqlBraked(PlayerbotAI* botAI);

// A frenzied bot: only the bite moves it, and it bites the player the tick handed it
bool IsBqlBiting(PlayerbotAI* botAI);
Player* GetBqlBiteTarget(PlayerbotAI* botAI);
// The mind-controlled vampire this bot keeps under control, and the spell, when one is due now
Unit* GetBqlFrenzyCc(PlayerbotAI* botAI, uint32& spellId);
// A mind-controlled vampire within range of the bot or its target, where area damage would break its CC
bool IsBqlVampireInSplash(PlayerbotAI* botAI, float range);

// A Pact member walks to the Pact spot, unless frenzied or carrying shadows. Notes bql.pact.
bool IsBqlPactMember(PlayerbotAI* botAI);
bool GetBqlPactWalk(PlayerbotAI* botAI, Position& spot);
// A Swarming Shadows carrier walks its lane unless frenzied, the next point of it here
bool IsBqlShadowCarrier(PlayerbotAI* botAI);
bool GetBqlShadowsWalk(PlayerbotAI* botAI, Position& spot);
// Anyone else standing on a shadow. Notes bql.dodge.
bool IsBqlInShadow(PlayerbotAI* botAI);
bool FindBqlDodgeSpot(PlayerbotAI* botAI, Position& spot);

bool IsBqlHoldingStand(PlayerbotAI* botAI);
bool IsBqlHoldingTankStand(PlayerbotAI* botAI);
bool GetBqlStandWalk(PlayerbotAI* botAI, Position& spot);
// A stand, a dodge, a bite, a Pact or a Shadows walk owns the bot's feet
bool IsBqlMovementOwned(PlayerbotAI* botAI);

// From 8 s before her walk to the centre until she takes off: Incite Terror lands as she arrives
bool IsBqlFearWindow(PlayerbotAI* botAI);

// Bloodbolt Whirl while she channels it: what is left of a bolt per player plus Shroud of Sorrow's ticks
bool GetBqlWhirlHit(PlayerbotAI* botAI, RaidPredictedHit& hit);
RaidCover GetBqlWhirlCover(PlayerbotAI* botAI);
bool IsBqlWhirlCoverHeld(PlayerbotAI* botAI);

// The button this tank spends now, 0 for none: under BQL_TANK_DEFENSIVE_PCT, or in the last minute while
// she or her mirror hits it
uint32 GetBqlTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsBqlTankDefensiveHeld(PlayerbotAI* botAI);

// The scan, the tank roster, the nodes, groups, bites and CC, the brake, the Shadows lanes, the stand
// latch, the calls to a human tank, the reset after a wipe
void TickBqlEncounter(PlayerbotAI* botAI);

// Lust waits for the most vampires the fight reaches before Berserk
RaidBurstWindow IccBloodQueenBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& GetBqlOpenerBosses(PlayerbotAI* botAI);
// Shadow all fight: Shroud of Sorrow, Pact, Swarming Shadows, the Bloodbolts, Blood Mirror
uint8 IccBloodQueenResistSchools(PlayerbotAI* botAI, uint8 previous);

#endif

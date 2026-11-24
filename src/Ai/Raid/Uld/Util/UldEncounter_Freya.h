/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERFREYA_H
#define PLAYERBOTS_ULDENCOUNTERFREYA_H

#include "EncounterHelpers.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "UldData.h"

#include <functional>
#include <vector>

class GameObject;
class Player;
class PlayerbotAI;
class Unit;

enum UlduarFreyaIds
{
    NPC_SNAPLASHER = 32916,
    NPC_STORM_LASHER = 32919,
    NPC_DETONATING_LASHER = 32918,
    NPC_ANCIENT_WATER_SPIRIT = 33202,
    NPC_ANCIENT_CONSERVATOR = 33203,
    NPC_HEALTHY_SPORE = 33215,
    NPC_EONARS_GIFT = 33228,
    GOBJECT_NATURE_BOMB = 194902,
    SPELL_ATTUNED_TO_NATURE = 62519,

    SPELL_FREYA_GROUND_TREMOR_10 = 62437,
    SPELL_FREYA_GROUND_TREMOR_25 = 62859,

    SPELL_FREYA_SUNBEAM_10 = 62623,
    SPELL_FREYA_SUNBEAM_25 = 62872,

    SPELL_NATURES_FURY_10 = 62589,
    SPELL_NATURES_FURY_25 = 63571,

    NPC_FREYA_IRON_ROOTS = 33088,
    NPC_FREYA_STRENGTHENED_IRON_ROOTS = 33168,
    NPC_FREYA_SUN_BEAM = 33170,
    NPC_FREYA_UNSTABLE_SUN_BEAM = 33050,
    SPELL_IRON_ROOTS_DAMAGE_10 = 62283,
    SPELL_IRON_ROOTS_DAMAGE_25 = 62930,
    SPELL_IRON_ROOTS_FREYA_DAMAGE_10 = 62861,
    SPELL_IRON_ROOTS_FREYA_DAMAGE_25 = 62438,

    SPELL_POTENT_PHEROMONES = 64321,

    SPELL_ARMY_OF_THE_DEAD = 42650,

    SPELL_HARDENED_BARK_STACK_10 = 62663,
    SPELL_HARDENED_BARK_STACK_25 = 64190,

    SPELL_FREYA_PAIN_SUPPRESSION = 33206,
    SPELL_FREYA_GUARDIAN_SPIRIT = 47788,
    SPELL_FREYA_HAND_OF_SACRIFICE = 6940,

    SPELL_FREYA_SHIELD_WALL = 871,
    SPELL_FREYA_DIVINE_PROTECTION = 498,
    SPELL_FREYA_BARKSKIN = 22812,
    SPELL_FREYA_ICEBOUND_FORTITUDE = 48792,
};

inline constexpr uint32 ULDUAR_FREYA_ADD_TANK_DEFENSIVE_STACKS = 25;

inline constexpr float ULDUAR_FREYA_ADD_TANK_DEFENSIVE_LAST_RESORT_PCT = 25.0f;
inline constexpr float ULDUAR_FREYA_ADD_TANK_LASHER_LAST_RESORT_PCT = 50.0f;

inline constexpr uint32 ULDUAR_FREYA_ADD_TANK_EXTERNAL_LEAD_MS = 1500;

inline constexpr float ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS = 8.0f;

inline constexpr float ULDUAR_FREYA_SUN_BEAM_CLEARANCE = 11.0f;

inline constexpr uint32 ULDUAR_FREYA_SUN_BEAM_LATCH_MS = 3000;

inline constexpr float ULDUAR_FREYA_SUNBEAM_AVOID_RADIUS = 11.0f;
inline constexpr float ULDUAR_FREYA_SUNBEAM_CLEAR_RADIUS = 13.0f;

inline constexpr uint32 ULDUAR_FREYA_SUNBEAM_LATCH_MS = 1500;

inline constexpr float ULDUAR_FREYA_TRIO_SYNC_WINDOW_PCT = 30.0f;
inline constexpr float ULDUAR_FREYA_TRIO_FLOOR_RELEASE_PCT = 15.0f;
inline constexpr float ULDUAR_FREYA_TRIO_HARD_FLOOR_PCT = 10.0f;

inline constexpr float ULDUAR_FREYA_SPORE_RADIUS = 6.0f;

inline constexpr uint32 ULDUAR_FREYA_SPORE_CROWD = 6;

inline constexpr float ULDUAR_FREYA_NATURES_FURY_RADIUS = 8.0f;

inline constexpr float ULDUAR_FREYA_NATURES_FURY_CLEAR = 11.0f;

inline constexpr uint32 ULDUAR_FREYA_NATURES_FURY_LATCH_MS = 3000;

inline constexpr float ULDUAR_FREYA_SPORE_STAND_RANGE = 4.0f;

inline constexpr float ULDUAR_FREYA_SPORE_SEARCH_RADIUS = 40.0f;

inline constexpr float ULDUAR_FREYA_DETONATE_RADIUS = 15.0f;

inline constexpr float ULDUAR_FREYA_MELEE_LASHER_RANGE = 12.0f;

inline constexpr float ULDUAR_FREYA_LASHER_TAUNT_RANGE = 30.0f;
inline constexpr float ULDUAR_FREYA_RIGHTEOUS_DEFENSE_RANGE = 40.0f;
inline constexpr float ULDUAR_FREYA_LASHER_SHOUT_RADIUS = 10.0f;

inline constexpr float ULDUAR_FREYA_TANK_TRIO_SWITCH_PCT = 5.0f;

inline constexpr float ULDUAR_FREYA_NATURE_BOMB_BLAST_RADIUS = 10.0f;

inline constexpr float ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS = 11.0f;

inline constexpr float ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS = 13.0f;

inline constexpr uint32 ULDUAR_FREYA_NATURE_BOMB_LATCH_MS = 7000;

inline constexpr uint32 ULDUAR_FREYA_MELEE_REACH_RETRY_MS = 500;

inline constexpr uint32 ULDUAR_FREYA_TANK_BOMB_LATCH_MS = 3000;

extern const Position ULDUAR_FREYA_TANK_ANCHOR;

inline constexpr float ULDUAR_FREYA_TANK_LEASH = 20.0f;

inline constexpr uint32 ULDUAR_FREYA_TANK_HOLD_LATCH_MS = 3000;

inline constexpr float ULDUAR_FREYA_HAZARD_SEARCH_RADIUS = 30.0f;

inline constexpr float ULDUAR_FREYA_SWEEP_STEP = 2.0f;
inline constexpr float ULDUAR_FREYA_SWEEP_ANGLE_STEP = static_cast<float>(M_PI) / 8.0f;

inline constexpr float ULDUAR_FREYA_IRON_ROOTS_SEARCH_RADIUS = 10.0f;

inline constexpr float ULDUAR_FREYA_ROOTED_BEAM_RADIUS = 6.0f;

inline constexpr float ULDUAR_FREYA_HAND_OF_FREEDOM_RANGE = 30.0f;

inline constexpr float ULDUAR_FREYA_LASHER_PACK_RADIUS = 8.0f;
inline constexpr float ULDUAR_FREYA_LASHER_PACK_CLEAR = 16.0f;
inline constexpr uint32 ULDUAR_FREYA_LASHER_PACK_MIN_COUNT = 3;
inline constexpr float ULDUAR_FREYA_LASHER_FINISH_PCT = 20.0f;

inline constexpr float ULDUAR_FREYA_FROST_NOVA_RADIUS = 10.0f;
inline constexpr uint32 ULDUAR_FREYA_FROST_NOVA_MIN_LASHERS = 2;

inline constexpr float ULDUAR_FREYA_FROST_TRAP_ARM_RANGE = 20.0f;

inline constexpr float ULDUAR_FREYA_LASHER_BAIL_PCT = 15.0f;

inline constexpr uint32 ULDUAR_FREYA_LASHER_BAIL_LATCH_MS = 3000;

inline constexpr float ULDUAR_FREYA_LASHER_CAMP_STANDOFF = 18.0f;

inline constexpr uint32 ULDUAR_FREYA_GIFT_SHARE = 5;

inline constexpr uint32 ULDUAR_FREYA_GIFT_SHARE_MS = 5000;

inline constexpr float ULDUAR_FREYA_RANGED_CAMP_TOLERANCE = 10.0f;
inline constexpr float ULDUAR_FREYA_HEALER_CAMP_TOLERANCE = 15.0f;

inline constexpr float ULDUAR_FREYA_RANGED_CAMP_SPACING = 6.0f;

inline constexpr float ULDUAR_FREYA_SNAPLASHER_THREAT_SHED_PCT = 80.0f;
inline constexpr float ULDUAR_FREYA_SNAPLASHER_THREAT_RETURN_PCT = 60.0f;

inline constexpr uint32 ULDUAR_FREYA_SNAPLASHER_TANK_LEAD_MS = 5000;

inline constexpr float ULDUAR_FREYA_SNAPLASHER_KITE_MARGIN = 5.0f;
inline constexpr float ULDUAR_FREYA_SNAPLASHER_KITE_CLEAR = 10.0f;

inline constexpr uint32 ULDUAR_FREYA_SNAPLASHER_KITE_LATCH_MS = 1000;

inline constexpr float ULDUAR_FREYA_HELD_PACK_REACH_MARGIN = 3.0f;

inline constexpr uint32 ULDUAR_FREYA_HELD_PACK_LATCH_MS = 3000;

inline constexpr uint32 ULDUAR_FREYA_HELD_PACK_RETRY_MS = 1000;

inline constexpr float ULDUAR_FREYA_GIFT_WALK_MARGIN = 1.0f;

inline constexpr uint32 ULDUAR_FREYA_GIFT_WALK_LATCH_MS = 3000;

inline constexpr uint32 ULDUAR_FREYA_GIFT_WALK_RETRY_MS = 1000;

struct FreyaWaveState
{
    Unit* eonarsGift = nullptr;
    Unit* conservator = nullptr;
    Unit* snaplasher = nullptr;
    Unit* stormLasher = nullptr;
    Unit* waterSpirit = nullptr;
    std::vector<Unit*> detonatingLashers;

    std::vector<Unit*> LivingTrio() const;

    bool TrioLocked() const;

    bool TrioReleased() const;
};

void GatherFreyaWaveState(PlayerbotAI* botAI, FreyaWaveState& state);

class FreyaScan
{
public:
    explicit FreyaScan(PlayerbotAI* botAI) : botAI(botAI) {}

    Unit* Boss();

    GuidVector const& PossibleTargets();

    GuidVector const& Stalkers();

    Position const& LasherCamp();

private:
    PlayerbotAI* botAI;

    uint32 bossAtMs = 0;
    ObjectGuid boss;
    uint32 targetsAtMs = 0;
    GuidVector targets;
    uint32 stalkersAtMs = 0;
    GuidVector stalkers;
    uint32 campAtMs = 0;
    Position camp;
};

FreyaScan& GetFreyaScan(PlayerbotAI* botAI);

Unit* GetFreyaScanUnitByEntry(PlayerbotAI* botAI, uint32 entry);

Unit* GetFreyaBossByEntry(PlayerbotAI* botAI);

bool FreyaTrioSyncSuppress(FreyaWaveState const& state, Unit* target);

Unit* GetFreyaTrioAssignment(PlayerbotAI* botAI, FreyaWaveState const& state);

Unit* GetFreyaTankTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget);

Unit* GetFreyaSnaplasherToKite(PlayerbotAI* botAI);

float GetFreyaSnaplasherKiteReach(Unit* snaplasher, Player* bot, float margin);

Unit* GetFreyaLocalLasherTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget, float range);

Unit* GetFreyaRangedLasherFocus(FreyaWaveState const& state);

Unit* GetFreyaLowLasherOnPlayer(Player* bot, FreyaWaveState const& state, float range, bool backLineOnly = false);

bool IsFreyaLowLasherChasing(Player* bot, Unit* lasher);

Unit* GetFreyaLowLasherChasing(Player* bot, FreyaWaveState const& state);

bool IsFreyaLasherTail(FreyaWaveState const& state);

Player* GetFreyaStackedSnaplasherTank(FreyaWaveState const& state);

bool IsFreyaAddTankDefensiveHeld(PlayerbotAI* botAI);

bool IsFreyaAddTankExternalHeld(PlayerbotAI* botAI);

bool HasFreyaTankExternal(Player* tank, uint32 forMs = 0);

bool IsFreyaAddTankCovered(Player* tank);

char const* GetFreyaTankExternal(PlayerbotAI* botAI, Player*& tank);

Unit* GetFreyaConservatorSpore(Unit* conservator);

Unit* GetFreyaTargetSpore(PlayerbotAI* botAI);

std::vector<Unit*> GetFreyaSpores(PlayerbotAI* botAI);

uint32 CountFreyaRaidNear(PlayerbotAI* botAI, Position const& centre, float radius, Player* except);

Unit* GetFreyaNaturesFuryShelter(PlayerbotAI* botAI);

bool IsFreyaGiftFiller(PlayerbotAI* botAI, Unit* gift);

std::vector<Position> GetFreyaNatureBombPositions(Player* bot, float searchRadius);

bool FreyaWalkCrossesNatureBomb(Position const& from, Position const& to, std::vector<Position> const& bombs,
                                float clearance);

std::vector<Position> GetFreyaSunBeamPositions(PlayerbotAI* botAI, float searchRadius);

std::vector<EncounterHelpers::HazardCircle> GetFreyaEscapeHazards(PlayerbotAI* botAI, float searchRadius);

std::vector<EncounterHelpers::HazardCircle> BuildFreyaEscapeHazards(std::vector<Position> const& bombs,
                                                                    std::vector<Position> const& beams,
                                                                    std::vector<Position> const& lashers = {});

std::vector<Position> GetFreyaEscapeLasherPositions(PlayerbotAI* botAI, float searchRadius);

Player* GetFreyaRangedCampAnchor(PlayerbotAI* botAI);

uint32 GetFreyaRangedDpsRank(PlayerbotAI* botAI);

bool IsFreyaArmyTurn(PlayerbotAI* botAI);

bool HasUncoveredFreyaLasher(PlayerbotAI* botAI, FreyaWaveState const& state);

bool IsFreyaArmyChanneling(Player* player);

Player* GetFreyaLooseLasherVictim(Unit* lasher);

bool IsFreyaRooted(Unit* unit);

uint32 GetFreyaHazardsUnder(PlayerbotAI* botAI, Unit* unit, Position* centre = nullptr);

Player* GetFreyaRootedAllyToFree(PlayerbotAI* botAI);

Position GetFreyaLasherCampSpot(PlayerbotAI* botAI);

bool FreyaShedsSnaplasherThreat(PlayerbotAI* botAI, Unit* snaplasher, uint32 seenForMs, bool shedding);

std::vector<Position> GetFreyaLowLasherPositions(PlayerbotAI* botAI, FreyaWaveState const& state, float maxPct,
                                                 float radius, bool spareChasers = false);

bool IsFreyaLasherSubmerged(Unit* lasher);

std::vector<Position> GetFreyaSubmergedLasherPositions(PlayerbotAI* botAI, FreyaWaveState const& state, float radius);

Position FindFreyaStaticFloorSpot(Player* bot, std::vector<EncounterHelpers::HazardCircle> const& hazards,
                                  float maxRadius, Position const* preferNear,
                                  std::function<bool(float, float)> const& accept);

Unit* GetFreyaLasherPackFocus(FreyaWaveState const& state);

bool IsFreyaLasherPackFinishing(FreyaWaveState const& state, Position const& centre);

Unit* GetFreyaFinishingPackNear(PlayerbotAI* botAI, FreyaWaveState const& state, float radius);

Unit* GetFreyaRangedLasherAim(FreyaWaveState const& state);

bool IsFreyaLasherHeldByGhoul(PlayerbotAI* botAI, Unit* lasher);

uint32 CountFreyaLashersNear(Position const& centre, FreyaWaveState const& state, float radius);

bool IsFreyaGroundTremorCasting(Unit* boss);

Unit* GetFreyaSunbeamTarget(Unit* boss);

#endif

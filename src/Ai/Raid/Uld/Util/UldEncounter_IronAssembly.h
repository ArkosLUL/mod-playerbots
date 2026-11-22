/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERIRONASSEMBLY_H
#define PLAYERBOTS_ULDENCOUNTERIRONASSEMBLY_H

#include "ObjectGuid.h"
#include "Position.h"
#include "RaidResistAura.h"
#include "UldData.h"

#include <vector>

class Player;
class PlayerbotAI;
class Unit;

enum UlduarIronAssemblyIds
{
    SPELL_LIGHTNING_TENDRILS_10_MAN = 61887,
    SPELL_LIGHTNING_TENDRILS_25_MAN = 63486,
    SPELL_OVERLOAD_10_MAN = 61869,
    SPELL_OVERLOAD_25_MAN = 63481,
    SPELL_CHAIN_LIGHTNING_10_MAN = 61879,
    SPELL_CHAIN_LIGHTNING_25_MAN = 63479,
    SPELL_LIGHTNING_WHIRL_10_MAN = 61915,
    SPELL_LIGHTNING_WHIRL_25_MAN = 63483,
    SPELL_RUNE_OF_DEATH_10_MAN = 62269,
    SPELL_RUNE_OF_DEATH_25_MAN = 63490,
    SPELL_SHIELD_OF_RUNES_10_MAN = 62274,
    SPELL_SHIELD_OF_RUNES_25_MAN = 63489,
    SPELL_FUSION_PUNCH_10_MAN = 61903,
    SPELL_FUSION_PUNCH_25_MAN = 63493,
    SPELL_OVERWHELMING_POWER_10_MAN = 64637,
    SPELL_OVERWHELMING_POWER_25_MAN = 61888,
    NPC_IRON_ASSEMBLY_RUNE_OF_POWER = 33705,
    SPELL_RUNE_OF_POWER_PULSE = 61974,
    SPELL_OVERLOAD_DAMAGE = 61878,
    SPELL_LIGHTNING_TENDRILS_DAMAGE_10_MAN = 61886,
    SPELL_LIGHTNING_TENDRILS_DAMAGE_25_MAN = 63485,
    SPELL_MELTDOWN = 61889,
};

inline constexpr float ULDUAR_IRON_ASSEMBLY_OVERLOAD_RADIUS = 20.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_OVERLOAD_CLEARANCE = 25.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_TENDRILS_RADIUS = 18.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_TENDRILS_CLEARANCE = 28.0f;

inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_RADIUS = 13.0f;

inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_DANGER_RADIUS = 16.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_CLEARANCE = 21.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_SEARCH_RADIUS = 40.0f;

inline constexpr float ULDUAR_IRON_ASSEMBLY_ESCAPE_SWEEP_STEP = 2.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_ESCAPE_SWEEP_ANGLE_STEP = static_cast<float>(M_PI) / 8.0f;

inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_ROUTE_RADIUS = 18.5f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_DETOUR_STEP = 0.7854f;

inline constexpr uint8 ULDUAR_IRON_ASSEMBLY_STACK_SHIFT_HEADINGS = 8;
inline constexpr float ULDUAR_IRON_ASSEMBLY_STACK_SHIFT_RADIUS = 25.0f;

inline constexpr float ULDUAR_IRON_ASSEMBLY_MELTDOWN_RADIUS = 15.0f;

inline constexpr uint32 ULDUAR_IRON_ASSEMBLY_MELTDOWN_SWAP_LEAD_MS = 10000;
inline constexpr uint32 ULDUAR_IRON_ASSEMBLY_MELTDOWN_WALK_SLACK_MS = 1000;
inline constexpr float ULDUAR_IRON_ASSEMBLY_MELTDOWN_POINT_BEARING = 5.4978f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_MELTDOWN_POINT_RADIUS = 30.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_MELTDOWN_POINT_TOLERANCE = 2.0f;

inline constexpr uint32 ULDUAR_IRON_ASSEMBLY_MELTDOWN_STEP_OUT_MS = 3000;
inline constexpr float ULDUAR_IRON_ASSEMBLY_MELTDOWN_DANGER_RADIUS = 16.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_MELTDOWN_CLEARANCE = 17.0f;

inline constexpr uint32 ULDUAR_IRON_ASSEMBLY_FUSION_PUNCH_DECIDE_MS = 1500;

inline constexpr float ULDUAR_IRON_ASSEMBLY_TANK_DEFENSIVE_LAST_RESORT_PCT = 25.0f;

inline constexpr float ULDUAR_IRON_ASSEMBLY_REBIRTH_CAST_REACH = 28.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_REBIRTH_WALK_REACH = 26.0f;

inline constexpr uint32 ULDUAR_IRON_ASSEMBLY_REBUFF_WINDOW_MS = 30000;
inline constexpr float ULDUAR_IRON_ASSEMBLY_REBUFF_REACH = 30.0f;

inline constexpr float ULDUAR_IRON_ASSEMBLY_NITRO_MIN_WALK = 10.0f;

inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_RADIUS = 5.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_SOAK_MAX_TRAVEL = 25.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_SEARCH_RADIUS = 50.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_OWNER_RADIUS = 15.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_BOSS_OFFSET = 5.5f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_BOSS_OVERSHOOT = 2.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_SHIFT_BRUNDIR_MIN_STACK = 27.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_SHIFT_MELEE_MIN_STACK = 11.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_SHIFT_MAX_STACK = 36.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_MELEE_DEPTH = 3.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_MELEE_DEPTH_MAX = 4.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_MELEE_HOLD = 4.5f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_SLOT_TOLERANCE = 2.0f;

inline constexpr uint32 ULDUAR_IRON_ASSEMBLY_OPENER_BURST_WINDOW_MS = 20000;
inline constexpr int32 ULDUAR_IRON_ASSEMBLY_OPENER_BURST_MAX_COOLDOWN_MS = 180000;

inline constexpr float ULDUAR_IRON_ASSEMBLY_BRUNDIR_BEARING = 0.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_BRUNDIR_RADIUS = 28.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_STEELBREAKER_BEARING = 2.3562f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_MOLGEIM_BEARING = 3.9270f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_MELEE_BOSS_RADIUS = 16.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_STACK_BEARING = 3.1416f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_STACK_RADIUS = 10.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_BRUNDIR_LAST_STACK_RADIUS = 3.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_SLOT_TOLERANCE = 3.0f;

inline constexpr float ULDUAR_IRON_ASSEMBLY_ARENA_RADIUS = 78.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_ARENA_HEIGHT = 10.0f;
inline constexpr float ULDUAR_IRON_ASSEMBLY_TANK_SPOT_TOLERANCE = 4.0f;

inline constexpr float ULDUAR_IRON_ASSEMBLY_SPREAD_RING_RADIUS = 18.0f;
inline constexpr uint8 ULDUAR_IRON_ASSEMBLY_SPREAD_SLOTS = 16;

inline constexpr float ULDUAR_IRON_ASSEMBLY_EMPOWERED_SPREAD_RING_RADIUS = 22.0f;

inline constexpr uint8 ULDUAR_IRON_ASSEMBLY_SPREAD_RING_SAMPLES = 64;

inline constexpr uint8 ULDUAR_IRON_ASSEMBLY_NATURE_HUNTER_SLOT = 0;
inline constexpr uint8 ULDUAR_IRON_ASSEMBLY_NATURE_TOTEM_SLOT = ULDUAR_IRON_ASSEMBLY_SPREAD_SLOTS / 2;
inline constexpr float ULDUAR_IRON_ASSEMBLY_NATURE_TOTEM_REDROP = 10.0f;

inline constexpr uint32 ULDUAR_IRON_ASSEMBLY_HAZARD_NOTE_INTERVAL_MS = 1000;

extern const Position ULDUAR_IRON_ASSEMBLY_ANCHOR;

struct IronAssemblyTargets
{
    Unit* steelbreaker = nullptr;
    Unit* molgeim = nullptr;
    Unit* brundir = nullptr;

    uint8 AliveCount() const;
};

Unit* GetIronAssemblyMember(PlayerbotAI* botAI, uint32 entry);
void GatherIronAssemblyTargets(PlayerbotAI* botAI, IronAssemblyTargets& targets);
bool IronAssemblyEncounterActive(PlayerbotAI* botAI);

class IronAssemblyScan
{
public:
    explicit IronAssemblyScan(PlayerbotAI* botAI) : botAI(botAI) {}

    void Members(IronAssemblyTargets& targets);
    Unit* Member(uint32 entry);

    std::vector<Position> RunesOfDeath();

    std::vector<Position> RunesOfPower();

private:
    void RefreshMembers();
    Unit* Resolve(ObjectGuid const& guid) const;

    PlayerbotAI* botAI;

    uint32 membersAtMs = 0;
    ObjectGuid steelbreaker;
    ObjectGuid molgeim;
    ObjectGuid brundir;

    uint32 runesAtMs = 0;
    std::vector<Position> runes;

    uint32 runesOfPowerAtMs = 0;
    std::vector<Position> runesOfPower;
};

IronAssemblyScan& GetIronAssemblyScan(PlayerbotAI* botAI);

bool IronAssemblyFormationActive(PlayerbotAI* botAI);

bool IronAssemblyBrundirIsLast(PlayerbotAI* botAI);

Unit* IronAssemblyFocusTarget(PlayerbotAI* botAI);

Unit* IronAssemblyAssignedBoss(PlayerbotAI* botAI, Player* bot);

bool TryGetIronAssemblyTankSpot(PlayerbotAI* botAI, Player* bot, Position& position);

bool TryGetIronAssemblyBossTankSpot(PlayerbotAI* botAI, Unit* boss, Position& position);

bool TryGetIronAssemblyBossRuneOfPower(PlayerbotAI* botAI, Unit* boss, Position& rune);

bool TryGetIronAssemblyRaidSpot(PlayerbotAI* botAI, Player* bot, Position& position, float* tolerance = nullptr);

bool IronAssemblyTakesRuneOfPower(Player* bot);
bool IronAssemblyOnRuneOfPower(PlayerbotAI* botAI);

bool TryGetIronAssemblyMeleeRuneSpot(PlayerbotAI* botAI, Player* bot, Position& spot);

bool IronAssemblyOverloadActive(Unit* brundir);
bool IronAssemblyTendrilsActive(Unit* brundir);

void GatherIronAssemblyRunesOfDeath(Player* bot, std::vector<Position>& runes);

bool IsIronAssemblyPositionClearOfRunes(Position const& spot, std::vector<Position> const& runes,
                                        float radius);

bool TryGetIronAssemblyRouteStep(PlayerbotAI* botAI, Player* bot, Position const& goal, Position& step);

bool IronAssemblyMemberMustMove(PlayerbotAI* botAI, Player* member);

char const* IronAssemblyReadyInterrupt(Player* bot, Unit* target);

char const* IronAssemblyInterruptDuty(PlayerbotAI* botAI, Player* bot, Unit* brundir);

bool IronAssemblyLightningWhirlActive(Unit* brundir);
bool IronAssemblyChainLightningCasting(Unit* brundir);

bool IronAssemblyShieldOfRunesUp(Unit* molgeim);
bool IronAssemblyHasFusionPunch(Unit* unit);
bool IronAssemblyHasOverwhelmingPower(Unit* unit);

bool IronAssemblyHeldByOtherTank(Player* bot, Unit* boss);

Unit* IronAssemblyUnheldMemberToTaunt(PlayerbotAI* botAI, Player* bot);

uint32 IronAssemblyFusionPunchIncomingMs(PlayerbotAI* botAI, Player* bot);

uint8 IronAssemblyResistWantedSchools(PlayerbotAI* botAI, uint8 previous);
inline constexpr RaidResistAuraConfig ULD_IRON_ASSEMBLY_RESIST_AURA{ULDUAR_MAP_ID, "ironassembly.aura",
                                                                   &IronAssemblyResistWantedSchools, nullptr};
bool IsIronAssemblyNatureHunter(PlayerbotAI* botAI);

Player* IronAssemblyNatureTotemShaman(PlayerbotAI* botAI);
bool IsIronAssemblyNatureTotemShaman(PlayerbotAI* botAI);

enum class IronAssemblyNatureTotemStep : uint8
{
    None,
    SetButton,
    Drop,
};

IronAssemblyNatureTotemStep IronAssemblyNatureTotemNext(PlayerbotAI* botAI);
uint32 IronAssemblyNatureTotemSpell(Player* bot);

enum class IronAssemblyMeltdownRole : uint8
{
    None,
    Hold,
    NoPartner,
    Taunt,
    Run,
    Parked,
    Late,
};

IronAssemblyMeltdownRole IronAssemblyMeltdownRoleFor(PlayerbotAI* botAI, Player* bot);
Position IronAssemblyMeltdownPoint();

bool IronAssemblyMeltdownMeleeWindow(PlayerbotAI* botAI);

bool TryGetIronAssemblyMeltdownStepOut(PlayerbotAI* botAI, Player* bot, Position& spot);

enum class IronAssemblyBattleRezStep : uint8
{
    None,
    Walk,
    Cast,
};

struct IronAssemblyBattleRez
{
    Player* tank = nullptr;
    uint32 spellId = 0;
    IronAssemblyBattleRezStep step = IronAssemblyBattleRezStep::None;
    Position walkTo;
};

bool IronAssemblyBattleRezDuty(PlayerbotAI* botAI, Player* bot, IronAssemblyBattleRez& duty);

struct IronAssemblyTankRebuff
{
    Player* tank = nullptr;
    uint32 spellId = 0;
};

bool IronAssemblyTankRebuffDuty(PlayerbotAI* botAI, Player* bot, IronAssemblyTankRebuff& duty);

bool IronAssemblyUseNitroBoosts(Player* bot, float walk, char const* why);

bool IronAssemblyOpenerBurstWindowOpen(PlayerbotAI* botAI);

void ResetIronAssemblyEncounterState(Player* bot, bool clearInstance);
bool IronAssemblyEncounterStateIsStale(PlayerbotAI* botAI);
bool IronAssemblyBotHasEncounterState(Player* bot);

#endif

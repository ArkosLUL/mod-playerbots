/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERRAZORSCALE_H
#define PLAYERBOTS_ULDENCOUNTERRAZORSCALE_H

#include "AiObject.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "UldData.h"

#include <array>
#include <cstddef>
#include <ctime>
#include <vector>

class GameObject;
class Player;
class PlayerbotAI;
class Unit;

class RazorscaleBossHelper : public AiObject
{
public:
    enum RazorscaleUnits : uint32
    {
        UNIT_RAZORSCALE          = 33186,
        UNIT_DARK_RUNE_SENTINEL  = 33846,
        UNIT_DARK_RUNE_WATCHER   = 33453,
        UNIT_DARK_RUNE_GUARDIAN  = 33388,
        UNIT_DEVOURING_FLAME     = 34188,
    };

    enum RazorscaleGameObjects : uint32
    {
        GO_RAZORSCALE_HARPOON_1 = 194519,
        GO_RAZORSCALE_HARPOON_2 = 194541,
        GO_RAZORSCALE_HARPOON_3 = 194542,
        GO_RAZORSCALE_HARPOON_4 = 194543,
    };

    enum RazorscaleSpells : uint32
    {
        SPELL_SENTINEL_WHIRLWIND = 63806,
        SPELL_STUN_AURA         = 62794,
        SPELL_FUSE_ARMOR        = 64821
    };

    static constexpr uint32 FUSEARMOR_THRESHOLD = 2;

    static constexpr float DEVOURING_FLAME_RADIUS = 5.0f;
    static constexpr float DEVOURING_FLAME_CLEAR_RADIUS = DEVOURING_FLAME_RADIUS + 2.0f;
    static constexpr float DEVOURING_FLAME_STAND_MARGIN = 2.0f;

    static constexpr float DEVOURING_FLAME_AIRBORNE_TANK_SCALE = 2.3f;

    static constexpr float SENTINEL_AVOID_RADIUS = 8.0f;

    static constexpr float RAZORSCALE_FLYING_Z_THRESHOLD = 440.0f;
    static constexpr float RAZORSCALE_ARENA_CENTER_X = 587.54f;
    static constexpr float RAZORSCALE_ARENA_CENTER_Y = -175.04f;
    static constexpr float RAZORSCALE_ARENA_RADIUS = 30.0f;
    static constexpr float RAZORSCALE_ARENA_LEASH_MARGIN = 25.0f;
    static constexpr float RAZORSCALE_ARENA_RETURN_INSET = 10.0f;

    static constexpr float RAZORSCALE_PERMANENT_GROUND_HEALTH_PCT = 50.0f;

    static constexpr float RAZORSCALE_FIRST_LANDING_X = 588.0f;
    static constexpr float RAZORSCALE_FIRST_LANDING_Y = -166.0f;
    static constexpr float RAZORSCALE_FIRST_LANDING_Z = 391.1f;
    static constexpr float RAZORSCALE_FIRST_LANDING_TOLERANCE = 2.0f;
    static constexpr float RAZORSCALE_FIRST_LANDING_Z_TOLERANCE = 1.0f;
    static constexpr float RAZORSCALE_FIRST_LANDING_STACK_OFFSET_Y = 20.0f;
    static constexpr float RAZORSCALE_FIRST_LANDING_STACK_RADIUS = 18.0f;
    static constexpr float RAZORSCALE_RANGED_STACK_RADIUS = 12.0f;

    static constexpr int RAZORSCALE_ASSIST_TANK_SLOTS = 3;

    static constexpr float FLAME_BREATH_CONE = static_cast<float>(M_PI) / 2.0f;
    static constexpr float FLAME_BREATH_RANGE = 40.0f;

    static constexpr float HARPOON_SEARCH_RANGE = 200.0f;
    static constexpr std::size_t HARPOON_ENTRY_COUNT = 4;

    struct HarpoonData
    {
        uint32 gameObjectEntry;
    };

    explicit RazorscaleBossHelper(PlayerbotAI* botAI)
        : AiObject(botAI), _boss(nullptr) {}

    bool UpdateBossAI();
    Unit* GetBoss() const;

    bool IsGroundPhase() const;
    bool IsFlyingPhase() const;

    static bool IsGroundPhaseFor(Unit* boss);
    static bool IsFlyingPhaseFor(Unit* boss);

    static Unit* FindDevouringFlameNear(PlayerbotAI* botAI, float radius);

    static void CollectDevouringFlames(Player* bot, float radius, std::vector<Position>& out);

    static bool DevouringFlameBlocks(std::vector<Position> const& flames, float x, float y);

    static bool DevouringFlameBlocks(Player* bot, float x, float y);

    static bool IsHarpoonReady(GameObject* harpoonGO);
    GameObject* FindNearestHarpoon(float x, float y, float z) const;

    static std::vector<HarpoonData> const& GetHarpoonData();

    void AssignRolesBasedOnHealth();
    bool AreRolesAssigned() const;
    bool CanSwapRoles() const;

private:
    Unit* _boss;

    static constexpr std::time_t _roleSwapCooldown = 10;
};

class RazorscaleScan
{
public:
    explicit RazorscaleScan(PlayerbotAI* botAI) : botAI(botAI) {}

    Unit* Boss();

    GuidVector const& Hostiles();
    GuidVector const& PossibleTargets();

    std::array<ObjectGuid, RazorscaleBossHelper::HARPOON_ENTRY_COUNT> const& NearestHarpoons();

private:
    PlayerbotAI* botAI;

    uint32 bossAtMs = 0;
    ObjectGuid boss;
    uint32 hostilesAtMs = 0;
    GuidVector hostiles;
    uint32 targetsAtMs = 0;
    GuidVector targets;
    uint32 harpoonsAtMs = 0;
    std::array<ObjectGuid, RazorscaleBossHelper::HARPOON_ENTRY_COUNT> harpoons;
};

RazorscaleScan& GetRazorscaleScan(PlayerbotAI* botAI);

bool IsRazorscaleHarpoonCrew(Player* bot);

GameObject* GetRazorscaleClosestReadyHarpoon(PlayerbotAI* botAI);

Unit* GetRazorscaleAddKillTarget(PlayerbotAI* botAI);

bool IsRazorscaleAtFirstLanding(Unit* boss);

bool GetRazorscaleRangedStackSpot(Player* bot, float x, float y, float radius, Position& spot);

Unit* GetRazorscaleKillTarget(PlayerbotAI* botAI);

#endif

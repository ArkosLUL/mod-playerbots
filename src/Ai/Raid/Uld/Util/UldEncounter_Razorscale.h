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

#include <ctime>
#include <unordered_map>
#include <vector>

class GameObject;
class Player;
class PlayerbotAI;
class Unit;

// Razorscale.
//
// Two alternating phases. While she is flying the raid kills Dark Rune adds and the harpoon crews
// reload; a harpoon volley grounds her, and the ground phase is the only window in which she takes
// damage. Roles are assigned by health rather than by spec, and both the harpoon cooldowns and the
// role assignments are held per bot so a re-derivation cannot flip them mid-phase.

class RazorscaleBossHelper : public AiObject
{
public:
    // Enums and constants specific to Razorscale
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

    // The Devouring Flame stalker's tick (64704 / 64733) carries radius index 8. The clear radius adds
    // the margin a step needs to actually leave the patch rather than stopping on its edge.
    static constexpr float DEVOURING_FLAME_RADIUS = 5.0f;
    static constexpr float DEVOURING_FLAME_CLEAR_RADIUS = DEVOURING_FLAME_RADIUS + 2.0f;

    // Constants for arena parameters
    static constexpr float RAZORSCALE_FLYING_Z_THRESHOLD = 440.0f;
    static constexpr float RAZORSCALE_ARENA_CENTER_X = 587.54f;
    static constexpr float RAZORSCALE_ARENA_CENTER_Y = -175.04f;
    static constexpr float RAZORSCALE_ARENA_RADIUS = 30.0f;

    // Harpoon cooldown (seconds)
    static constexpr time_t HARPOON_COOLDOWN_DURATION = 5;

    // Structure for harpoon data
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

    // Same phase reads against a boss unit the caller already holds, for code that must not run
    // UpdateBossAI() first - it reassigns the raid's tank roles as a side effect.
    static bool IsGroundPhaseFor(Unit* boss);
    static bool IsFlyingPhaseFor(Unit* boss);

    // Nearest live Devouring Flame patch within radius of the bot, or nullptr.
    static Unit* FindDevouringFlameNear(PlayerbotAI* botAI, float radius);

    // Centres of the live patches within radius of the bot. The dodge sweep tests a dozen-plus
    // candidate destinations against the same set, and one grid search beats one per candidate.
    static void CollectDevouringFlames(Player* bot, float radius, std::vector<Position>& out);

    // True when one of the collected patches covers (x, y).
    static bool DevouringFlameBlocks(std::vector<Position> const& flames, float x, float y);

    // True when a Devouring Flame patch covers (x, y). Searched around the bot, so the radius has to
    // reach a destination he is not standing on yet as well as the patch's own reach around it.
    static bool DevouringFlameBlocks(Player* bot, float x, float y);

    static bool IsHarpoonReady(GameObject* harpoonGO);
    static void SetHarpoonOnCooldown(GameObject* harpoonGO);
    GameObject* FindNearestHarpoon(float x, float y, float z) const;

    static std::vector<HarpoonData> const& GetHarpoonData();

    void AssignRolesBasedOnHealth();
    bool AreRolesAssigned() const;
    bool CanSwapRoles() const;

private:
    Unit* _boss;

    // A map to track the last role swap *per bot* by their GUID
    static std::unordered_map<ObjectGuid, std::time_t> _lastRoleSwapTime;

    // The cooldown that applies to every bot
    static const std::time_t _roleSwapCooldown = 10;

    static std::unordered_map<ObjectGuid, time_t> _harpoonCooldowns;
};

// Dark Rune add the raid should be killing, most urgent first: Sentinel (whirlwinds the raid) >
// Watcher (ranged caster) > Guardian, lowest health first within a tier so the raid focuses one down
// instead of splitting across two. Returns nullptr when none are up.
Unit* GetRazorscaleAddKillTarget(PlayerbotAI* botAI);

// What the skull belongs on right now: the boss whenever she is on the floor - harpoon knockdowns
// included, since she is damageable then - and otherwise the add above.
Unit* GetRazorscaleKillTarget(PlayerbotAI* botAI);

#endif

#ifndef PLAYERBOTS_ULDBOSSHELPER_H
#define PLAYERBOTS_ULDBOSSHELPER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "AiObject.h"
#include "AiObjectContext.h"
#include "EventMap.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "ScriptedCreature.h"
#include "UldScripts.h"

constexpr uint32 ULDUAR_MAP_ID = 603;

enum UlduarIDs
{
    // Iron Assembly
    SPELL_LIGHTNING_TENDRILS_10_MAN = 61887,
    SPELL_LIGHTNING_TENDRILS_25_MAN = 63486,
    SPELL_OVERLOAD_10_MAN = 61869,
    SPELL_OVERLOAD_25_MAN = 63481,
    SPELL_OVERLOAD_10_MAN_2 = 63485,
    SPELL_OVERLOAD_25_MAN_2 = 61886,
    SPELL_RUNE_OF_POWER = 64320,
    // NPC_STEELBREAKER / NPC_MOLGEIM / NPC_BRUNDIR come from core ulduar.h via UldScripts.h
    SPELL_FUSION_PUNCH = 61903,
    SPELL_OVERWHELMING_POWER = 64637,

    // Kologarn
    NPC_RIGHT_ARM = 32934,
    NPC_RUBBLE = 33768,
    SPELL_CRUNCH_ARMOR = 64002,

    SPELL_FOCUSED_EYEBEAM_10_2 = 63346,
    SPELL_FOCUSED_EYEBEAM_10 = 63347,
    SPELL_FOCUSED_EYEBEAM_25_2 = 63976,
    SPELL_FOCUSED_EYEBEAM_25 = 63977,

    // Hodir
    NPC_SNOWPACKED_ICICLE = 33174,
    NPC_TOASTY_FIRE = 33342,
    SPELL_FLASH_FREEZE = 61968,
    SPELL_BITING_COLD_PLAYER_AURA = 62039,

    // Freya
    NPC_SNAPLASHER = 32916,
    NPC_STORM_LASHER = 32919,
    NPC_DETONATING_LASHER = 32918,
    NPC_ANCIENT_WATER_SPIRIT = 33202,
    NPC_ANCIENT_CONSERVATOR = 33203,
    NPC_HEALTHY_SPORE = 33215,
    NPC_EONARS_GIFT = 33228,
    GOBJECT_NATURE_BOMB = 194902,

    // Thorim
    NPC_DARK_RUNE_ACOLYTE_I = 32886,
    NPC_CAPTURED_MERCENARY_SOLDIER_ALLY = 32885,
    NPC_CAPTURED_MERCENARY_SOLDIER_HORDE = 32883,
    NPC_CAPTURED_MERCENARY_CAPTAIN_ALLY = 32908,
    NPC_CAPTURED_MERCENARY_CAPTAIN_HORDE = 32907,
    NPC_JORMUNGAR_BEHEMOT = 32882,
    NPC_DARK_RUNE_WARBRINGER = 32877,
    NPC_DARK_RUNE_EVOKER = 32878,
    NPC_DARK_RUNE_CHAMPION = 32876,
    NPC_DARK_RUNE_COMMONER = 32904,
    NPC_IRON_RING_GUARD = 32874,
    NPC_RUNIC_COLOSSUS = 32872,
    NPC_ANCIENT_RUNE_GIANT = 32873,
    NPC_DARK_RUNE_ACOLYTE_G = 33110,
    NPC_IRON_HONOR_GUARD = 32875,
    SPELL_UNBALANCING_STRIKE = 62130,

    // Mimiron
    NPC_LEVIATHAN_MKII = 33432,
    NPC_VX001 = 33651,
    NPC_AERIAL_COMMAND_UNIT = 33670,
    NPC_BOMB_BOT = 33836,
    NPC_ROCKET_STRIKE_N = 34047,
    NPC_ASSAULT_BOT = 34057,
    NPC_PROXIMITY_MINE = 34362,
    SPELL_P3WX2_LASER_BARRAGE_1 = 63293,
    SPELL_P3WX2_LASER_BARRAGE_2 = 63297,
    SPELL_SPINNING_UP = 63414,
    SPELL_SHOCK_BLAST = 63631,
    SPELL_P3WX2_LASER_BARRAGE_3 = 64042,
    SPELL_P3WX2_LASER_BARRAGE_AURA_1 = 63274,
    SPELL_P3WX2_LASER_BARRAGE_AURA_2 = 63300,

    // General Vezax
    SPELL_MARK_OF_THE_FACELESS = 63276,
    SPELL_VEZAX_SHADOW_CRASH = 63277,

    // Yogg-Saron
    ACTION_ILLUSION_DRAGONS = 1,
    ACTION_ILLUSION_ICECROWN = 2,
    ACTION_ILLUSION_STORMWIND = 3,
    NPC_GUARDIAN_OF_YS = 33136,
    NPC_YOGG_SARON = 33288,
    NPC_OMINOUS_CLOUD = 33292,
    NPC_RUBY_CONSORT = 33716,
    NPC_AZURE_CONSORT = 33717,
    NPC_BRONZE_CONSORT = 33718,
    NPC_EMERALD_CONSORT = 33719,
    NPC_OBSIDIAN_CONSORT = 33720,
    NPC_ALEXTRASZA = 33536,
    NPC_MALYGOS_ILLUSION = 33535,
    NPC_NELTHARION = 33523,
    NPC_YSERA = 33495,
    GO_DRAGON_SOUL = 194462,
    NPC_SARA_PHASE_1 = 33134,
    NPC_LICH_KING_ILLUSION = 33441,
    NPC_IMMOLATED_CHAMPION = 33442,
    NPC_SUIT_OF_ARMOR = 33433,
    NPC_GARONA = 33436,
    NPC_KING_LLANE = 33437,
    NPC_DEATHSWORN_ZEALOT = 33567,
    NPC_INFLUENCE_TENTACLE = 33943,
    NPC_DEATH_ORB = 33882,
    NPC_BRAIN = 33890,
    NPC_CRUSHER_TENTACLE = 33966,
    NPC_CONSTRICTOR_TENTACLE = 33983,
    NPC_CORRUPTOR_TENTACLE = 33985,
    NPC_IMMORTAL_GUARDIAN = 33988,
    NPC_LAUGHING_SKULL = 33990,
    NPC_SANITY_WELL = 33991,
    NPC_DESCEND_INTO_MADNESS = 34072,
    NPC_MARKED_IMMORTAL_GUARDIAN = 36064,
    SPELL_SANITY = 63050,
    SPELL_BRAIN_LINK = 63802,
    SPELL_MALADY_OF_THE_MIND = 63830,
    SPELL_SHADOW_BARRIER = 63894,
    SPELL_TELEPORT_TO_CHAMBER = 63997,
    SPELL_TELEPORT_TO_ICECROWN = 63998,
    SPELL_TELEPORT_TO_STORMWIND = 63989,
    SPELL_TELEPORT_BACK = 63992,
    SPELL_CANCEL_ILLUSION_AURA = 63993,
    SPELL_INDUCE_MADNESS = 64059,
    SPELL_LUNATIC_GAZE_YS = 64163,
    GO_FLEE_TO_THE_SURFACE_PORTAL = 194625,

    // Algalon the Observer
    PB_NPC_ALGALON = 32871,
    PB_NPC_LIVING_CONSTELLATION = 33052,
    PB_NPC_COLLAPSING_STAR = 32955,
    PB_NPC_BLACK_HOLE = 32953,
    PB_NPC_WORM_HOLE = 34099,
    PB_NPC_UNLEASHED_DARK_MATTER = 34097,
    NPC_ALGALON_ASTEROID_TARGET_1 = 33104,
    NPC_ALGALON_ASTEROID_TARGET_2 = 33105,
    SPELL_ALGALON_BIG_BANG = 64443,
    SPELL_ALGALON_PHASE_PUNCH = 64412,
    SPELL_ALGALON_COSMIC_SMASH = 62301,
    SPELL_ALGALON_BLACK_HOLE_DAMAGE = 62169,

    // Buffs
    SPELL_FROST_TRAP = 13809,

    // Ignis the Furnace Master
    NPC_IGNIS_IRON_CONSTRUCT = 33121,
    NPC_IGNIS_SCORCHED_GROUND = 33123,

    // Auriaya
    NPC_AURIAYA_SANCTUM_SENTRY = 34014,
    NPC_AURIAYA_FERAL_DEFENDER = 34035,
    NPC_AURIAYA_SEEPING_FERAL_ESSENCE = 34098,

    // General Vezax
    NPC_VEZAX_SARONITE_VAPORS = 33488,
    NPC_VEZAX_SARONITE_ANIMUS = 33524,

    // Flame Leviathan hard mode (each tower left standing empowers the boss and
    // spawns that tower's periodic ground hazard).
    NPC_FLAME_LEVIATHAN = 33113,
    SPELL_FL_TOWER_OF_STORMS = 65076,        // boss empower aura while Storm tower up
    SPELL_FL_TOWER_OF_FLAMES = 65075,        // boss empower aura while Flame tower up
    SPELL_FL_TOWER_OF_FROST = 65077,         // boss empower aura while Frost tower up
    SPELL_FL_TOWER_OF_LIFE = 64482,          // boss empower aura while Life tower up
    NPC_FL_THORIM_HAMMER_TARGET = 33364,     // Storm: static lightning-strike marks
    NPC_FL_MIMIRONS_INFERNO_TARGET = 33369,  // Flame: moving fire trail
    NPC_FL_HODIRS_FURY_TARGET = 33108,       // Frost: chases a random player then drops frost

    // Thorim hard mode (arena gauntlet cleared fast enough that Sif joins the fight).
    NPC_SIF = 33196,              // spawns at Thorim's throne, drops into the arena when she joins
    NPC_SIF_BLIZZARD = 32879      // moving Blizzard ground AoE, only ever exists in hard mode
};

// Flame Leviathan hard-mode active-tower bitmask (which empower auras the boss carries).
enum FlameLeviathanTowerFlags
{
    FL_TOWER_STORM = 0x1,
    FL_TOWER_FLAMES = 0x2,
    FL_TOWER_FROST = 0x4,
    FL_TOWER_LIFE = 0x8
};

// Vehicle keeps this clear of any active-tower ground hazard (strike / fire / frost).
constexpr float ULDUAR_FL_TOWER_HAZARD_RADIUS = 18.0f;

// Vezax hard mode: ranged/healers stay outside the Saronite Animus' Profound Darkness (63420).
constexpr float ULDUAR_VEZAX_PROFOUND_DARKNESS_RADIUS = 15.0f;

// Off-tank taunts once the active tank reaches this many Phase Punch stacks
constexpr uint32 ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS = 3;

// Kiter stops this far past the Black Hole (away from the constellation) to stay out of its phase/damage aura
constexpr float ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET = 5.0f;

// Designated Big Bang soaker: the first alive Shadow Priest in the raid, who stays out and pops
// Dispersion (90% damage reduction) to survive Big Bang instead of hiding in a hole. Big Bang is
// unavoidable — full immunity (Paladin Divine Shield) does not prevent it, only mitigation survives.
// Returns nullptr if the raid has no living Shadow Priest.
Player* GetAlgalonBigBangSoakerPriest(Player* bot);

constexpr float ULDUAR_KOLOGARN_AXIS_Z_PATHING_ISSUE_DETECT = 420.0f;
constexpr float ULDUAR_KOLOGARN_EYEBEAM_RADIUS = 3.0f;
constexpr float ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD = 429.6094f;
constexpr float ULDUAR_THORIM_AXIS_Z_PATHING_ISSUE_DETECT = 410.0f;

// Thorim hard mode: bots clear Sif's moving Blizzard, and ranged/healers keep this far from
// Sif herself so her point-blank Frost Nova (cast after she teleports next to a target) misses.
constexpr float ULDUAR_THORIM_SIF_BLIZZARD_RADIUS = 12.0f;
constexpr float ULDUAR_THORIM_SIF_FROST_NOVA_RADIUS = 12.0f;
constexpr float ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT = 410.0f;
constexpr float ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 300.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 200.0f;
constexpr float ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_ICECROWN_CITADEL_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_RADIUS = 50.0f;

extern const Position ULDUAR_THORIM_NEAR_ARENA_CENTER;
extern const Position ULDUAR_THORIM_NEAR_ENTRANCE_POSITION;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3;
extern const Position ULDUAR_THORIM_JUMP_END_POINT;
extern const Position ULDUAR_THORIM_PHASE2_TANK_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE1_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE2_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE3_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE1RANGE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE1MELEE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE2RANGE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE2MELEE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE3RANGE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE3MELEE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE4_TANK_SPOT;
extern const Position ULDUAR_VEZAX_MARK_OF_THE_FACELESS_SPOT;
extern const Position ULDUAR_YOGG_SARON_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT;

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
        SPELL_CHAIN_1           = 49679,
        SPELL_CHAIN_2           = 49682,
        SPELL_CHAIN_3           = 49683,
        SPELL_CHAIN_4           = 49684,
        SPELL_SENTINEL_WHIRLWIND = 63806,
        SPELL_STUN_AURA         = 62794,
        SPELL_FUSEARMOR         = 64771
    };

    static constexpr uint32 FUSEARMOR_THRESHOLD = 2;

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
        uint32 chainSpellId;
    };

    explicit RazorscaleBossHelper(PlayerbotAI* botAI)
        : AiObject(botAI), _boss(nullptr) {}

    bool UpdateBossAI();
    Unit* GetBoss() const;

    bool IsGroundPhase() const;
    bool IsFlyingPhase() const;

    bool IsHarpoonFired(uint32 chainSpellId) const;
    static bool IsHarpoonReady(GameObject* harpoonGO);
    static void SetHarpoonOnCooldown(GameObject* harpoonGO);
    GameObject* FindNearestHarpoon(float x, float y, float z) const;

    static const std::vector<HarpoonData>& GetHarpoonData();

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

// template <class BossAiType>
// class GenericBossHelper : public AiObject
// {
// public:
//     GenericBossHelper(PlayerbotAI* botAI, std::string name) : AiObject(botAI), _name(name) {}
//     virtual bool UpdateBossAI()
//     {
//         if (!bot->IsInCombat())
//         {
//             _unit = nullptr;
//         }
//         if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
//         {
//             _unit = nullptr;
//         }
//         if (!_unit)
//         {
//             _unit = AI_VALUE2(Unit*, "find target", _name);
//             if (!_unit)
//             {
//                 return false;
//             }
//             _target = _unit->ToCreature();
//             if (!_target)
//             {
//                 return false;
//             }
//             _ai = dynamic_cast<BossAiType*>(_target->GetAI());
//             if (!_ai)
//             {
//                 return false;
//             }
//             _event_map = &_ai->events;
//             if (!_event_map)
//             {
//                 return false;
//             }
//         }
//         if (!_event_map)
//         {
//             return false;
//         }
//         _timer = _event_map->GetTimer();
//         return true;
//     }
//     virtual void Reset()
//     {
//         _unit = nullptr;
//         _target = nullptr;
//         _ai = nullptr;
//         _event_map = nullptr;
//         _timer = 0;
//     }

// protected:
//     std::string _name;
//     Unit* _unit = nullptr;
//     Creature* _target = nullptr;
//     BossAiType* _ai = nullptr;
//     EventMap* _event_map = nullptr;
//     uint32 _timer = 0;
// };

#endif

#include "ToCHelpers_FactionChampionsDefence.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <iterator>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Creature.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampions.h"

namespace TrialOfTheCrusaderHelpers
{

namespace
{
// Same radius as the encounter gate, which sees the whole arena from anywhere on its floor
constexpr float CHAMPION_SEARCH_RADIUS = 200.0f;
constexpr float DISPEL_RANGE = 30.0f;
constexpr float MASS_DISPEL_RANGE = 30.0f;
// Its friendly half, centre to centre around the point
constexpr float MASS_DISPEL_ALLY_RADIUS = 15.0f;
constexpr float PURGE_RANGE = 30.0f;
constexpr float TOTEM_DUTY_RANGE = 30.0f;

// Less than that left isn't worth a GCD
constexpr int32 CC_MIN_LEFT_MS = 2000;
// Room for Mass Dispel's 1.5 s cast
constexpr int32 SHIELD_MIN_LEFT_MS = 3000;
constexpr int32 BLADESTORM_MIN_LEFT_MS = 300;
// The kicker stays put this long after he starts Hellfire, so its kick can land
constexpr int32 HELLFIRE_KICK_WINDOW_MS = 2000;
constexpr uint32 HAZARD_NOTE_MS = 1000;

// Script radii, centre to centre. Clearances add margin, more for the warrior, who walks at 5.6 yd/s
// while spinning
constexpr float BLADESTORM_RADIUS = 8.0f;
constexpr float BLADESTORM_CLEARANCE = 12.0f;
constexpr float HELLFIRE_RADIUS = 10.0f;
constexpr float HELLFIRE_CLEARANCE = 13.0f;
constexpr float IN_AOE_MARGIN = 1.0f;

// Only these need the kicker inside the fire, a ranged interrupt reaches him from the clearance
constexpr char const* MELEE_INTERRUPTS[] = {"kick", "pummel", "shield bash", "mind freeze"};

constexpr float AOE_SEARCH_RADIUS = 30.0f;
constexpr float HEAL_REACH = 30.0f;
// The floor the Beasts spots are known to reach, round ARENA_CENTER
constexpr float ARENA_FLOOR_RADIUS = 44.0f;

// The player spell, one rank. Its taunt and attack-me effects bounce off a heroic champion, the pull
// still lands.
constexpr uint32 SPELL_DEATH_GRIP = 49576;
// A DK this far off still gets the duty and walks to the stand
constexpr float GRIP_ELECT_REACH = 50.0f;
// The champion lands in front of the DK, so this is about how far it travels. Grip reaches 30.
constexpr float GRIP_STAND_DIST = 27.0f;
constexpr uint32 GRIP_BEARINGS = 16;
constexpr float GRIP_KEEP_RANGE = 29.0f;
constexpr uint32 GRIP_DUTY_MS = 8000;
// Past the champion healers' 40 yd heal range, plus the pack's own spread
constexpr float GRIP_GOAL = 45.0f;
constexpr float CHAMPION_HEAL_REACH = 40.0f;
// The guide grips the next healer out once the current one is about this low
constexpr float NEXT_GRIP_PCT = 20.0f;

constexpr float SNARE_HEALTH_PCT = 50.0f;
constexpr float PIERCING_HOWL_RADIUS = 10.0f;
constexpr float CHAINS_OF_ICE_RANGE = 20.0f;
constexpr float CONCUSSIVE_SHOT_RANGE = 35.0f;
constexpr float FROST_SHOCK_RANGE = 25.0f;
constexpr uint32 SPELL_HAMSTRING = 1715;
constexpr uint32 SPELL_PIERCING_HOWL = 12323;
constexpr uint32 SPELL_CHAINS_OF_ICE = 45524;
constexpr uint32 SPELL_WING_CLIP = 2974;
constexpr uint32 SPELL_CONCUSSIVE_SHOT = 5116;
constexpr std::array<uint32, 7> FROST_SHOCK_RANKS = {49236, 49235, 25464, 10473, 10472, 8058, 8056};

constexpr uint32 PROTECT_MIN_ATTACKERS = 2;
constexpr uint32 SPELL_FORBEARANCE = 25771;
constexpr std::array<uint32, 3> HAND_OF_PROTECTION_RANKS = {10278, 5599, 1022};
constexpr uint32 SPELL_PAIN_SUPPRESSION = 33206;
constexpr uint32 SPELL_GUARDIAN_SPIRIT = 47788;
constexpr uint32 SPELL_HAND_OF_SACRIFICE = 6940;
constexpr float HAND_RANGE = 30.0f;
constexpr float PRIEST_EXTERNAL_RANGE = 40.0f;
// Hand of Sacrifice moves 30% of the damage onto the paladin
constexpr float SACRIFICE_MIN_HEALTH_PCT = 50.0f;

constexpr std::array<uint32, 7> REBIRTH_RANKS = {48477, 26994, 20748, 20747, 20742, 20739, 20484};
constexpr float REBIRTH_RANGE = 30.0f;
constexpr float CORPSE_PACK_RADIUS = 8.0f;
constexpr uint32 CORPSE_PACK_MAX = 1;

struct DispelSpell
{
    char const* name;
    std::array<uint32, 2> ranks;  // highest first, 0 pads
    bool removesMagic;
};

constexpr DispelSpell PRIEST_DISPEL_MAGIC{"dispel magic", {988, 527}, true};
constexpr DispelSpell PALADIN_CLEANSE{"cleanse", {4987, 0}, true};
constexpr DispelSpell MAGE_REMOVE_CURSE{"remove curse", {475, 0}, false};
constexpr DispelSpell DRUID_REMOVE_CURSE{"remove curse", {2782, 0}, false};
constexpr DispelSpell DRUID_ABOLISH_POISON{"abolish poison", {2893, 0}, false};
constexpr DispelSpell DRUID_CURE_POISON{"cure poison", {8946, 0}, false};
constexpr DispelSpell SHAMAN_CLEANSE_SPIRIT{"cleanse spirit", {51886, 0}, false};
constexpr DispelSpell SHAMAN_CURE_TOXINS{"cure toxins", {526, 0}, false};
constexpr DispelSpell SHAMAN_PURGE{"purge", {8012, 370}, false};

struct CountedCc
{
    uint32 spellId;
    DispelType type;
    bool healersOnly;
};

// Roots and Psychic Horror left out: a GCD on them buys little
constexpr CountedCc COUNTED_CC[] = {
    {SPELL_POLYMORPH, DISPEL_MAGIC, false},
    {SPELL_FEAR, DISPEL_MAGIC, false},
    {SPELL_PSYCHIC_SCREAM, DISPEL_MAGIC, false},
    {SPELL_REPENTANCE, DISPEL_MAGIC, false},
    {SPELL_HAMMER_OF_JUSTICE_HOLY, DISPEL_MAGIC, false},
    {SPELL_HAMMER_OF_JUSTICE_RET, DISPEL_MAGIC, false},
    {SPELL_SILENCE, DISPEL_MAGIC, true},
    {SPELL_STRANGULATE, DISPEL_MAGIC, true},
    {SPELL_HEX, DISPEL_CURSE, false},
    {SPELL_WYVERN_STING, DISPEL_POISON, false},
};

// Tried in this order on a member holding several
constexpr DispelType CC_DISPEL_ORDER[] = {DISPEL_MAGIC, DISPEL_CURSE, DISPEL_POISON};

struct HazardSource
{
    ObjectGuid guid;
    uint32 spellId = 0;
    Position pos;
    float radius = 0.0f;
    float clearance = 0.0f;
    ObjectGuid kicker;  // the one bot a fresh Hellfire leaves in place
};

struct CcDispel
{
    ObjectGuid member;
    char const* spell = nullptr;
};

struct GripDuty
{
    ObjectGuid dk;
    ObjectGuid target;
    Position stand;
    uint32 sinceMs = 0;
};

// A champion, a group member or a corpse, and the spell the bot holding the duty casts on it
struct SpellDuty
{
    ObjectGuid target;
    uint32 spellId = 0;
};

struct FactionChampionsDefenceState
{
    RaidObs::ObsValue<ObjectGuid> physical{"fc.physical"};
    RaidObs::ObsGuidMap<ObjectGuid> totemKillers{"fc.totem"};
    RaidObs::ObsGuidMap<ObjectGuid> grips{"fc.grip"};
    GripDuty grip;
    std::unordered_map<ObjectGuid, SpellDuty> snares;     // snarer -> melee champion
    std::unordered_map<ObjectGuid, SpellDuty> protects;   // caster -> group member
    std::unordered_map<ObjectGuid, SpellDuty> battleRes;  // druid -> corpse

    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value
    std::vector<HazardSource> hazards;
    std::vector<ObjectGuid> totems;  // champion totems, sorted by guid
    std::unordered_map<ObjectGuid, uint32> hazardNotedMs;
    std::unordered_map<ObjectGuid, CcDispel> ccDispels;  // dispeller -> member
    ObjectGuid massDispeller;
    ObjectGuid massDispelTarget;
    ObjectGuid purger;
    ObjectGuid purgeTarget;
    char const* purgeSpell = nullptr;
};

RaidInstanceState<FactionChampionsDefenceState> defenceStates;

struct Member
{
    Player* player;
    bool healer;
};

// Alive group members on the bot's map, humans included. Just the bot without a group.
std::vector<Member> MembersOnMap(Player* bot)
{
    std::vector<Member> members;
    Map* map = bot->FindMap();
    auto const add = [&members, map](Player* player)
    {
        if (player && player->IsInWorld() && player->IsAlive() && player->FindMap() == map)
            members.push_back({player, PlayerbotAI::IsHeal(player)});
    };

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            add(ref->GetSource());
    }
    else
        add(bot);

    return members;
}

void SortByRole(std::vector<Member>& members, bool healersFirst)
{
    std::sort(members.begin(), members.end(),
              [healersFirst](Member const& a, Member const& b)
              {
                  if (a.healer != b.healer)
                      return a.healer == healersFirst;

                  return a.player->GetGUID() < b.player->GetGUID();
              });
}

// A negative duration is a permanent aura
bool LastsAtLeast(Aura const* aura, int32 ms) { return aura && (aura->GetDuration() < 0 || aura->GetDuration() >= ms); }

// Hex silences without taking control, so both are needed
bool InControl(Player* player)
{
    return !player->HasUnitState(UNIT_STATE_LOST_CONTROL) && !player->HasUnitFlag(UNIT_FLAG_SILENCED);
}

uint32 KnownRank(Player* player, DispelSpell const& spell)
{
    for (uint32 rank : spell.ranks)
        if (rank && player->HasSpell(rank))
            return rank;

    return 0;
}

bool CanPayFor(Player* player, uint32 spellId)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info || info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return false;

    return info->CalcPowerCost(player, info->GetSchoolMask()) <=
           static_cast<int32>(player->GetPower(Powers(info->PowerType)));
}

bool CarriesUnstableAffliction(Unit* unit)
{
    return unit && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_UNSTABLE_AFFLICTION, unit));
}

Unit* CurrentTarget(PlayerbotAI* botAI)
{
    return botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
}

struct DispelChoice
{
    DispelSpell const* spell = nullptr;
    uint32 spellId = 0;
};

DispelChoice FirstKnown(Player* player, std::initializer_list<DispelSpell const*> options)
{
    for (DispelSpell const* option : options)
        if (uint32 const rank = KnownRank(player, *option))
            return {option, rank};

    return {};
}

DispelChoice CcDispelSpell(Player* dispeller, DispelType type)
{
    switch (dispeller->getClass())
    {
        case CLASS_PRIEST:
            return type == DISPEL_MAGIC ? FirstKnown(dispeller, {&PRIEST_DISPEL_MAGIC}) : DispelChoice{};
        case CLASS_PALADIN:
            return type == DISPEL_MAGIC || type == DISPEL_POISON ? FirstKnown(dispeller, {&PALADIN_CLEANSE})
                                                                 : DispelChoice{};
        case CLASS_MAGE:
            return type == DISPEL_CURSE ? FirstKnown(dispeller, {&MAGE_REMOVE_CURSE}) : DispelChoice{};
        case CLASS_DRUID:
            if (type == DISPEL_CURSE)
                return FirstKnown(dispeller, {&DRUID_REMOVE_CURSE});

            return type == DISPEL_POISON ? FirstKnown(dispeller, {&DRUID_ABOLISH_POISON, &DRUID_CURE_POISON})
                                         : DispelChoice{};
        case CLASS_SHAMAN:
            if (type == DISPEL_CURSE)
                return FirstKnown(dispeller, {&SHAMAN_CLEANSE_SPIRIT});

            return type == DISPEL_POISON ? FirstKnown(dispeller, {&SHAMAN_CLEANSE_SPIRIT, &SHAMAN_CURE_TOXINS})
                                         : DispelChoice{};
        default:
            return {};
    }
}

// One bit per DispelType of the counted CC the member holds
uint32 CountedCcTypes(Member const& member)
{
    uint32 types = 0;
    for (CountedCc const& cc : COUNTED_CC)
        if ((!cc.healersOnly || member.healer) && LastsAtLeast(member.player->GetAura(cc.spellId), CC_MIN_LEFT_MS))
            types |= 1u << cc.type;

    return types;
}

void UpdateCcDispels(FactionChampionsDefenceState& state, std::vector<Member> const& members)
{
    state.ccDispels.clear();

    std::vector<std::pair<Member, uint32>> held;
    for (Member const& member : members)
        if (uint32 const types = CountedCcTypes(member))
            held.push_back({member, types});

    if (held.empty())
        return;

    std::sort(held.begin(), held.end(),
              [](auto const& a, auto const& b)
              {
                  if (a.first.healer != b.first.healer)
                      return a.first.healer;

                  return a.first.player->GetGUID() < b.first.player->GetGUID();
              });

    // Busy casters skip, so an idle dispeller takes the member instead of waiting on the cast
    std::vector<Member> dispellers;
    for (Member const& member : members)
        if (GET_PLAYERBOT_AI(member.player) && InControl(member.player) && !member.player->IsNonMeleeSpellCast(false))
            dispellers.push_back(member);

    SortByRole(dispellers, false);
    std::vector<bool> taken(dispellers.size(), false);

    for (auto const& [member, types] : held)
    {
        bool const backfires = CarriesUnstableAffliction(member.player);
        bool assigned = false;
        for (DispelType type : CC_DISPEL_ORDER)
        {
            if (!(types & (1u << type)))
                continue;

            for (size_t i = 0; i < dispellers.size() && !assigned; ++i)
            {
                Player* dispeller = dispellers[i].player;
                if (taken[i] || dispeller == member.player)
                    continue;

                // Counterspell, Spell Lock or Earth Shock on a cast locks the school's dispels too
                DispelChoice const choice = CcDispelSpell(dispeller, type);
                if (!choice.spell || (choice.spell->removesMagic && backfires) ||
                    dispeller->HasSpellCooldown(choice.spellId) || !CanPayFor(dispeller, choice.spellId) ||
                    !dispeller->IsWithinDistInMap(member.player, DISPEL_RANGE) ||
                    !dispeller->IsWithinLOSInMap(member.player))
                    continue;

                state.ccDispels[dispeller->GetGUID()] = {member.player->GetGUID(), choice.spell->name};
                taken[i] = true;
                assigned = true;
            }

            if (assigned)
                break;
        }
    }
}

Unit* MassDispelTargetNow(PlayerbotAI* botAI, Unit* killTarget)
{
    if (killTarget && (LastsAtLeast(killTarget->GetAura(SPELL_HAND_OF_PROTECTION), SHIELD_MIN_LEFT_MS) ||
                       LastsAtLeast(killTarget->GetAura(SPELL_DIVINE_SHIELD), SHIELD_MIN_LEFT_MS)))
        return killTarget;

    Unit* suspended = FactionChampionsSuspendedTarget(botAI);
    return suspended && LastsAtLeast(suspended->GetAura(SPELL_DIVINE_SHIELD), SHIELD_MIN_LEFT_MS) ? suspended
                                                                                                 : nullptr;
}

void UpdateMassDispel(FactionChampionsDefenceState& state, PlayerbotAI* botAI, std::vector<Member> const& members,
                      Unit* killTarget)
{
    state.massDispeller.Clear();
    state.massDispelTarget.Clear();

    Unit* target = MassDispelTargetNow(botAI, killTarget);
    if (!target)
        return;

    // The friendly half would pull it off a raider near the point and backlash on the priest
    bool const backfires = std::any_of(members.begin(), members.end(),
                                       [target](Member const& member)
                                       {
                                           return member.player->GetExactDist(target) <= MASS_DISPEL_ALLY_RADIUS &&
                                                  CarriesUnstableAffliction(member.player);
                                       });
    if (backfires)
        return;

    std::vector<Member> priests;
    for (Member const& member : members)
    {
        Player* player = member.player;
        if (player->getClass() == CLASS_PRIEST && GET_PLAYERBOT_AI(player) && player->HasSpell(SPELL_MASS_DISPEL) &&
            !player->HasSpellCooldown(SPELL_MASS_DISPEL) && InControl(player) && CanPayFor(player, SPELL_MASS_DISPEL))
            priests.push_back(member);
    }

    SortByRole(priests, false);
    for (Member const& priest : priests)
    {
        // A point cast: the core adds the priest's own size to the range, not the champion's
        if (priest.player->IsWithinDist3d(target, MASS_DISPEL_RANGE) && priest.player->IsWithinLOSInMap(target))
        {
            state.massDispeller = priest.player->GetGUID();
            state.massDispelTarget = target->GetGUID();
            return;
        }
    }
}

// Thorns and Nature's Grasp don't count: Thorns alone would keep a purger busy all pull
bool WorthPurging(Unit* target)
{
    uint32 const buffs[] = {
        SPELL_HAND_OF_PROTECTION,
        SPELL_EARTH_SHIELD,
        SPELL_HEROISM,
        SPELL_BLOODLUST,
        SPELL_AVENGING_WRATH,
        SPELL_BARKSKIN,
        sSpellMgr->GetSpellIdForDifficulty(SPELL_HAND_OF_FREEDOM, target),
        sSpellMgr->GetSpellIdForDifficulty(SPELL_POWER_WORD_SHIELD, target),
        sSpellMgr->GetSpellIdForDifficulty(SPELL_RENEW, target),
        sSpellMgr->GetSpellIdForDifficulty(SPELL_RIPTIDE, target),
        sSpellMgr->GetSpellIdForDifficulty(SPELL_REJUVENATION, target),
        sSpellMgr->GetSpellIdForDifficulty(SPELL_LIFEBLOOM, target),
        sSpellMgr->GetSpellIdForDifficulty(SPELL_REGROWTH, target),
    };

    return std::any_of(std::begin(buffs), std::end(buffs), [target](uint32 buff) { return target->HasAura(buff); });
}

// Class purges on champions are vetoed, so a purger with no shot at the kill target hands the duty on
void UpdatePurge(FactionChampionsDefenceState& state, std::vector<Member> const& members, Unit* killTarget)
{
    state.purger.Clear();
    state.purgeTarget.Clear();
    state.purgeSpell = nullptr;

    Player* purger = nullptr;
    DispelSpell const* spell = nullptr;
    for (Member const& member : members)
    {
        Player* player = member.player;
        if (member.healer || !GET_PLAYERBOT_AI(player))
            continue;

        DispelSpell const* option = player->getClass() == CLASS_SHAMAN   ? &SHAMAN_PURGE
                                    : player->getClass() == CLASS_PRIEST ? &PRIEST_DISPEL_MAGIC
                                                                         : nullptr;
        uint32 const rank = option ? KnownRank(player, *option) : 0;
        if (!rank || player->HasSpellCooldown(rank) || !CanPayFor(player, rank))
            continue;

        if (killTarget && (!InControl(player) || !player->IsWithinDistInMap(killTarget, PURGE_RANGE)))
            continue;

        if (!purger || player->GetGUID() < purger->GetGUID())
        {
            purger = player;
            spell = option;
        }
    }

    if (!purger)
        return;

    state.purger = purger->GetGUID();
    state.purgeSpell = spell->name;
    if (killTarget && WorthPurging(killTarget))
        state.purgeTarget = killTarget->GetGUID();
}

bool PhysicallyImmune(Unit* unit) { return unit->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL); }

// Hand of Protection. Each half asked apart: Ice Block is a physical aura plus a magic one, and
// IsImmunedToDamage wants one aura covering the whole mask
void UpdatePhysicalLatch(FactionChampionsDefenceState& state, PlayerbotAI* botAI, Unit* killTarget)
{
    ObjectGuid next;
    if (killTarget && PhysicallyImmune(killTarget) && !killTarget->IsImmunedToDamage(SPELL_SCHOOL_MASK_MAGIC))
    {
        ObjectGuid const latched = state.physical.Get();
        Unit* pick = nullptr;
        // never one in the pack, the melee would follow it in
        auto const isLatched = [botAI, latched](Unit* champion)
        {
            return champion->GetGUID() == latched && !PhysicallyImmune(champion) &&
                   FactionChampionsIsPulled(botAI, champion);
        };
        if (!latched.IsEmpty())
            pick = FactionChampionsNextTarget(botAI, isLatched);

        if (!pick)
        {
            auto const isOut = [botAI](Unit* champion)
            { return !PhysicallyImmune(champion) && FactionChampionsIsPulled(botAI, champion); };
            pick = FactionChampionsNextTarget(botAI, isOut);
        }

        if (pick)
            next = pick->GetGUID();
    }

    // Written every refresh, so a new trace hears the latch even when it didn't move
    state.physical = next;
}

bool IsWarrior(uint32 entry)
{
    return entry == static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_WARRIOR) ||
           entry == static_cast<uint32>(ToCFactionChampions::NPC_HORDE_WARRIOR);
}

// AoE casters and totems, so one grid walk covers both
std::vector<uint32> const& ScanEntries()
{
    static std::vector<uint32> const entries = {
        static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_WARRIOR),
        static_cast<uint32>(ToCFactionChampions::NPC_HORDE_WARRIOR),
        static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_WARLOCK),
        static_cast<uint32>(ToCFactionChampions::NPC_HORDE_WARLOCK),
        NPC_GROUNDING_TOTEM,
        NPC_WINDFURY_TOTEM,
        NPC_TREMOR_TOTEM,
    };

    return entries;
}

// The raid's own totems share the entries
bool IsChampionTotem(Player* bot, Creature* totem)
{
    ObjectGuid const owner = totem->GetOwnerGUID();
    return owner.IsCreature() && IsFactionChampion(owner.GetEntry()) && bot->IsValidAttackTarget(totem);
}

// Cooldown only, no CanCastSpell or power test: the top ranks of Kick, Pummel and Shield Bash sit on
// the GCD and energy dips under Kick's cost between swings, so either would flip the kicker every swing
bool HasReadyMeleeInterrupt(Player* player, PlayerbotAI* botAI, Unit* warlock)
{
    if (!player->IsWithinMeleeRange(warlock))
        return false;

    for (char const* name : MELEE_INTERRUPTS)
    {
        uint32 const id = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", name)->Get();
        if (id && player->HasSpell(id) && !player->HasSpellCooldown(id))
            return true;
    }

    return false;
}

// First by guid of the bots on the warlock with a melee kick ready
ObjectGuid ElectHellfireKicker(std::vector<Member> const& members, Unit* warlock)
{
    std::vector<Member> byGuid = members;
    std::sort(byGuid.begin(), byGuid.end(),
              [](Member const& a, Member const& b) { return a.player->GetGUID() < b.player->GetGUID(); });

    for (Member const& member : byGuid)
    {
        Player* player = member.player;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (ai && InControl(player) && CurrentTarget(ai) == warlock && HasReadyMeleeInterrupt(player, ai, warlock))
            return player->GetGUID();
    }

    return ObjectGuid::Empty;
}

void UpdateHazards(FactionChampionsDefenceState& state, Player* bot, std::vector<Member> const& members, uint32 now)
{
    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, ScanEntries(), CHAMPION_SEARCH_RADIUS);

    state.hazards.clear();
    state.totems.clear();
    for (Creature* creature : creatures)
    {
        if (!creature || !creature->IsAlive())
            continue;

        if (creature->IsTotem())
        {
            if (IsChampionTotem(bot, creature))
                state.totems.push_back(creature->GetGUID());

            continue;
        }

        HazardSource source;
        source.guid = creature->GetGUID();
        source.pos = creature->GetPosition();
        if (IsWarrior(creature->GetEntry()))
        {
            if (!LastsAtLeast(creature->GetAura(SPELL_BLADESTORM), BLADESTORM_MIN_LEFT_MS))
                continue;

            source.spellId = SPELL_BLADESTORM;
            source.radius = BLADESTORM_RADIUS;
            source.clearance = BLADESTORM_CLEARANCE;
        }
        else
        {
            uint32 const hellfire = sSpellMgr->GetSpellIdForDifficulty(SPELL_HELLFIRE, creature);
            Aura const* aura = creature->GetAura(hellfire);
            if (!aura)
                continue;

            source.spellId = hellfire;
            source.radius = HELLFIRE_RADIUS;
            source.clearance = HELLFIRE_CLEARANCE;
            if (aura->GetMaxDuration() >= 0 && aura->GetMaxDuration() - aura->GetDuration() < HELLFIRE_KICK_WINDOW_MS)
                source.kicker = ElectHellfireKicker(members, creature);
        }

        state.hazards.push_back(source);
    }

    std::sort(state.totems.begin(), state.totems.end());

    for (auto noted = state.hazardNotedMs.begin(); noted != state.hazardNotedMs.end();)
    {
        bool const active = std::any_of(state.hazards.begin(), state.hazards.end(),
                                        [&noted](HazardSource const& source) { return source.guid == noted->first; });
        noted = active ? std::next(noted) : state.hazardNotedMs.erase(noted);
    }

    if (!RaidObs::Active())
        return;

    for (HazardSource const& source : state.hazards)
    {
        auto const [noted, first] = state.hazardNotedMs.try_emplace(source.guid, now);
        if (!first && getMSTimeDiff(noted->second, now) < HAZARD_NOTE_MS)
            continue;

        noted->second = now;
        RaidObs::NoteHazardCircle(bot->GetMap(), source.spellId, source.pos, source.radius, HAZARD_NOTE_MS);
    }
}

bool CanKillTotem(Member const& member, Creature* totem)
{
    Player* player = member.player;
    return !member.healer && GET_PLAYERBOT_AI(player) && !player->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
           player->GetExactDist2d(totem) <= TOTEM_DUTY_RANGE;
}

// Latched, so a bot doesn't swap totems as the distances shift
void UpdateTotemKillers(FactionChampionsDefenceState& state, Player* bot, std::vector<Member> const& members)
{
    Map* map = bot->FindMap();
    auto const countedTotem = [&state, map](ObjectGuid guid) -> Creature*
    {
        bool const counts = std::find(state.totems.begin(), state.totems.end(), guid) != state.totems.end();
        return counts && map ? map->GetCreature(guid) : nullptr;
    };

    std::vector<ObjectGuid> released;
    for (auto const& hold : state.totemKillers.Raw())
    {
        Creature* totem = countedTotem(hold.second);
        ObjectGuid const killerGuid = hold.first;
        auto const killer = std::find_if(members.begin(), members.end(), [killerGuid](Member const& member)
                                         { return member.player->GetGUID() == killerGuid; });
        if (!totem || killer == members.end() || !CanKillTotem(*killer, totem))
            released.push_back(hold.first);
    }

    for (ObjectGuid const& killer : released)
        state.totemKillers.erase(killer);

    for (ObjectGuid const& guid : state.totems)
    {
        auto const& holds = state.totemKillers.Raw();
        bool const held =
            std::any_of(holds.begin(), holds.end(), [guid](auto const& hold) { return hold.second == guid; });
        Creature* totem = countedTotem(guid);
        if (held || !totem)
            continue;

        Player* nearest = nullptr;
        float nearestDist = 0.0f;
        for (Member const& member : members)
        {
            if (!CanKillTotem(member, totem) || holds.count(member.player->GetGUID()))
                continue;

            float const dist = member.player->GetExactDist2d(totem);
            bool const closer = !nearest || dist < nearestDist ||
                                (dist == nearestDist && member.player->GetGUID() < nearest->GetGUID());
            if (closer)
            {
                nearest = member.player;
                nearestDist = dist;
            }
        }

        if (nearest)
            state.totemKillers.Set(nearest->GetGUID(), guid);
    }
}

template <std::size_t N>
uint32 FirstKnownRank(Player* player, std::array<uint32, N> const& ranks)
{
    for (uint32 rank : ranks)
        if (player->HasSpell(rank))
            return rank;

    return 0;
}

bool SpellReady(Player* player, uint32 spellId)
{
    return spellId && player->HasSpell(spellId) && !player->HasSpellCooldown(spellId) && CanPayFor(player, spellId);
}

// Not casting, so an instant goes out this tick and a dodge isn't pinned
bool FreeToAct(Player* player) { return InControl(player) && !player->IsNonMeleeSpellCast(false); }

bool IsTankMember(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

std::vector<Member>::const_iterator FindMember(std::vector<Member> const& members, ObjectGuid guid)
{
    return std::find_if(members.begin(), members.end(),
                        [guid](Member const& member) { return member.player->GetGUID() == guid; });
}

bool IsMeleeChampion(uint32 entry)
{
    switch (static_cast<ToCFactionChampions>(entry))
    {
        case ToCFactionChampions::NPC_ALLIANCE_DEATH_KNIGHT:
        case ToCFactionChampions::NPC_HORDE_DEATH_KNIGHT:
        case ToCFactionChampions::NPC_ALLIANCE_WARRIOR:
        case ToCFactionChampions::NPC_HORDE_WARRIOR:
        case ToCFactionChampions::NPC_ALLIANCE_ROGUE:
        case ToCFactionChampions::NPC_HORDE_ROGUE:
        case ToCFactionChampions::NPC_ALLIANCE_SHAMAN_ENHANCEMENT:
        case ToCFactionChampions::NPC_HORDE_SHAMAN_ENHANCEMENT:
        case ToCFactionChampions::NPC_ALLIANCE_PALADIN_RETRIBUTION:
        case ToCFactionChampions::NPC_HORDE_PALADIN_RETRIBUTION:
            return true;
        default:
            return false;
    }
}

bool IsPhysicalChampion(uint32 entry)
{
    return IsMeleeChampion(entry) || entry == static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_HUNTER) ||
           entry == static_cast<uint32>(ToCFactionChampions::NPC_HORDE_HUNTER);
}

bool GripLands(Unit* target)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(SPELL_DEATH_GRIP);
    return info && target->IsInCombat() && !target->IsImmunedToSpell(info);
}

// Still inside the pack or in heal reach of another champion healer
bool WantsPull(Unit* target, std::vector<Unit*> const& champions, Position const& packCentre)
{
    if (target->GetExactDist2d(&packCentre) < GRIP_GOAL)
        return true;

    return std::any_of(champions.begin(), champions.end(),
                       [target](Unit* champion)
                       {
                           return champion != target && IsFactionChampionHealer(champion->GetEntry()) &&
                                  champion->GetExactDist2d(target) <= CHAMPION_HEAL_REACH;
                       });
}

// The kill target until it is clear of the pack, then the next healer once the kill target is low
Unit* GripCandidate(PlayerbotAI* botAI, std::vector<Unit*> const& champions, Unit* killTarget)
{
    Position anchor;
    Position centre;
    if (!killTarget || !FactionChampionsPack(botAI, anchor, centre))
        return nullptr;

    if (GripLands(killTarget) && WantsPull(killTarget, champions, centre))
        return killTarget;

    if (killTarget->GetHealthPct() > NEXT_GRIP_PCT)
        return nullptr;

    Unit* next = FactionChampionsNextHealerInPack(botAI);
    return next && GripLands(next) && WantsPull(next, champions, centre) ? next : nullptr;
}

Player* ElectGripper(std::vector<Member> const& members, Unit* target)
{
    Player* chosen = nullptr;
    for (Member const& member : members)
    {
        Player* player = member.player;
        if (player->getClass() != CLASS_DEATH_KNIGHT || !GET_PLAYERBOT_AI(player) ||
            FactionChampionsIsPackTank(player) || !InControl(player) || !player->HasSpell(SPELL_DEATH_GRIP) ||
            player->HasSpellCooldown(SPELL_DEATH_GRIP) || player->GetExactDist2d(target) > GRIP_ELECT_REACH)
            continue;

        if (!chosen || player->GetGUID() < chosen->GetGUID())
            chosen = player;
    }

    return chosen;
}

// The champion lands where the DK stands, so the stand on the floor farthest from the pack. Chained
// grips, a human's included, each carry it another 27 yd.
bool DeriveGripStand(Unit* target, Position const& packCentre, Position& stand)
{
    bool found = false;
    float best = 0.0f;
    for (uint32 bearing = 0; bearing < GRIP_BEARINGS; ++bearing)
    {
        float const angle = 2.0f * static_cast<float>(M_PI) * bearing / GRIP_BEARINGS;
        Position const spot(target->GetPositionX() + GRIP_STAND_DIST * std::cos(angle),
                            target->GetPositionY() + GRIP_STAND_DIST * std::sin(angle), target->GetPositionZ());
        if (ARENA_CENTER.GetExactDist2d(spot) > ARENA_FLOOR_RADIUS)
            continue;

        float const away = packCentre.GetExactDist2d(spot);
        if (!found || away > best)
        {
            stand = spot;
            best = away;
            found = true;
        }
    }

    return found;
}

// Latched until the grip goes out, the target changes or 8 s pass, so the DK walks to one spot
void UpdateGrip(FactionChampionsDefenceState& state, PlayerbotAI* botAI, std::vector<Member> const& members,
                std::vector<Unit*> const& champions, Unit* killTarget, uint32 now)
{
    GripDuty& duty = state.grip;
    Unit* target = GripCandidate(botAI, champions, killTarget);
    if (!duty.dk.IsEmpty())
    {
        auto const holder = FindMember(members, duty.dk);
        Player* dk = holder != members.end() ? holder->player : nullptr;
        bool const holds = dk && InControl(dk) && !dk->HasSpellCooldown(SPELL_DEATH_GRIP) && target &&
                           target->GetGUID() == duty.target && target->GetExactDist2d(&duty.stand) <= GRIP_KEEP_RANGE &&
                           getMSTimeDiff(duty.sinceMs, now) < GRIP_DUTY_MS;
        if (holds)
            return;

        state.grips.erase(duty.dk);
        duty = GripDuty();
    }

    Position anchor;
    Position centre;
    if (!target || !FactionChampionsPack(botAI, anchor, centre))
        return;

    Player* dk = ElectGripper(members, target);
    Position stand;
    if (!dk || !DeriveGripStand(target, centre, stand))
        return;

    duty.dk = dk->GetGUID();
    duty.target = target->GetGUID();
    duty.stand = stand;
    duty.sinceMs = now;
    state.grips.Set(duty.dk, duty.target);
}

bool FrostRuneReady(Player* player)
{
    for (uint8 index = 0; index < MAX_RUNES; ++index)
    {
        RuneType const rune = player->GetCurrentRune(index);
        if (!player->GetRuneCooldown(index) && (rune == RUNE_FROST || rune == RUNE_DEATH))
            return true;
    }

    return false;
}

// The snare this bot can put on champion from where it stands, 0 when none
uint32 ReadySnare(Player* snarer, Unit* champion)
{
    bool const inMelee = snarer->IsWithinMeleeRange(champion);
    switch (snarer->getClass())
    {
        case CLASS_WARRIOR:
            if (inMelee && SpellReady(snarer, SPELL_HAMSTRING))
                return SPELL_HAMSTRING;

            return snarer->GetExactDist2d(champion) <= PIERCING_HOWL_RADIUS && SpellReady(snarer, SPELL_PIERCING_HOWL)
                       ? SPELL_PIERCING_HOWL
                       : 0;
        case CLASS_DEATH_KNIGHT:
            return snarer->IsWithinCombatRange(champion, CHAINS_OF_ICE_RANGE) &&
                           SpellReady(snarer, SPELL_CHAINS_OF_ICE) && FrostRuneReady(snarer)
                       ? SPELL_CHAINS_OF_ICE
                       : 0;
        case CLASS_HUNTER:
            if (inMelee)
                return SpellReady(snarer, SPELL_WING_CLIP) ? SPELL_WING_CLIP : 0;

            return snarer->IsWithinCombatRange(champion, CONCUSSIVE_SHOT_RANGE) &&
                           SpellReady(snarer, SPELL_CONCUSSIVE_SHOT)
                       ? SPELL_CONCUSSIVE_SHOT
                       : 0;
        case CLASS_SHAMAN:
        {
            uint32 const rank = FirstKnownRank(snarer, FROST_SHOCK_RANKS);
            return snarer->IsWithinCombatRange(champion, FROST_SHOCK_RANGE) && SpellReady(snarer, rank) ? rank : 0;
        }
        default:
            return 0;
    }
}

// Hand of Freedom, or Divine Shield, Ice Block and Cyclone
bool SnareImmune(Unit* champion)
{
    if (champion->HasAuraTypeWithMiscvalue(SPELL_AURA_MECHANIC_IMMUNITY, MECHANIC_SNARE))
        return true;

    return champion->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL) &&
           champion->IsImmunedToDamage(SPELL_SCHOOL_MASK_MAGIC);
}

void UpdateSnares(FactionChampionsDefenceState& state, PlayerbotAI* botAI, std::vector<Member> const& members,
                  std::vector<Unit*> const& champions)
{
    state.snares.clear();

    struct Needy
    {
        Unit* champion;
        bool healerVictim;
        float victimHealth;
    };

    std::vector<Needy> needy;
    for (Unit* champion : champions)
    {
        if (!IsMeleeChampion(champion->GetEntry()) || champion->HasUnitState(UNIT_STATE_LOST_CONTROL) ||
            champion->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED) || SnareImmune(champion))
            continue;

        Unit* victim = champion->GetVictim();
        auto const member = victim ? FindMember(members, victim->GetGUID()) : members.end();
        bool const healerVictim = member != members.end() && member->healer;
        float const victimHealth = member != members.end() ? victim->GetHealthPct() : 100.0f;
        // one off the tank and out of the pack is on its way to the raid
        bool const loose = FactionChampionsIsEscapee(botAI, champion);
        if (!loose && (member == members.end() || (!healerVictim && victimHealth >= SNARE_HEALTH_PCT)))
            continue;

        needy.push_back({champion, healerVictim, victimHealth});
    }

    if (needy.empty())
        return;

    std::sort(needy.begin(), needy.end(),
              [](Needy const& a, Needy const& b)
              {
                  if (a.healerVictim != b.healerVictim)
                      return a.healerVictim;

                  if (a.victimHealth != b.victimHealth)
                      return a.victimHealth < b.victimHealth;

                  return a.champion->GetGUID() < b.champion->GetGUID();
              });

    // The DK walking to its grip stand runs its grip node first
    std::vector<Player*> snarers;
    for (Member const& member : members)
        if (!member.healer && member.player->GetGUID() != state.grip.dk && GET_PLAYERBOT_AI(member.player) &&
            FreeToAct(member.player))
            snarers.push_back(member.player);

    std::vector<bool> taken(snarers.size(), false);
    for (Needy const& entry : needy)
    {
        Unit* champion = entry.champion;
        std::vector<std::pair<float, size_t>> byDistance;
        for (size_t i = 0; i < snarers.size(); ++i)
            if (!taken[i])
                byDistance.push_back({snarers[i]->GetExactDist2d(champion), i});

        std::sort(byDistance.begin(), byDistance.end());
        for (auto const& nearest : byDistance)
        {
            size_t const i = nearest.second;
            Player* snarer = snarers[i];
            uint32 const spellId = ReadySnare(snarer, champion);
            SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
            if (!info || champion->IsImmunedToSpell(info) || !snarer->IsWithinLOSInMap(champion))
                continue;

            state.snares[snarer->GetGUID()] = {champion->GetGUID(), spellId};
            taken[i] = true;
            break;
        }
    }
}

bool HasAnyAura(Unit* unit, std::initializer_list<uint32> spells)
{
    return std::any_of(spells.begin(), spells.end(), [unit](uint32 spellId) { return unit->HasAura(spellId); });
}

// Already shielded from the hits, by one of the four or by its own immunity
bool ProtectCovered(Player* member)
{
    return member->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL) ||
           HasAnyAura(member, {HAND_OF_PROTECTION_RANKS[0], HAND_OF_PROTECTION_RANKS[1], HAND_OF_PROTECTION_RANKS[2],
                               SPELL_PAIN_SUPPRESSION, SPELL_GUARDIAN_SPIRIT, SPELL_HAND_OF_SACRIFICE});
}

// The spell id of the external this caster can put on member now, in the order they're worth spending
uint32 ReadyExternal(Player* caster, Player* member, bool handOfProtection)
{
    bool const self = caster == member;
    float const distance = self ? 0.0f : caster->GetExactDist(member);
    switch (caster->getClass())
    {
        case CLASS_PALADIN:
        {
            uint32 const rank = FirstKnownRank(caster, HAND_OF_PROTECTION_RANKS);
            if (handOfProtection && distance <= HAND_RANGE && SpellReady(caster, rank))
                return rank;

            bool const canSacrifice = !self && distance <= HAND_RANGE &&
                                      caster->GetHealthPct() >= SACRIFICE_MIN_HEALTH_PCT;
            return canSacrifice && SpellReady(caster, SPELL_HAND_OF_SACRIFICE) ? SPELL_HAND_OF_SACRIFICE : 0;
        }
        case CLASS_PRIEST:
            if (distance > PRIEST_EXTERNAL_RANGE)
                return 0;

            if (SpellReady(caster, SPELL_PAIN_SUPPRESSION))
                return SPELL_PAIN_SUPPRESSION;

            return SpellReady(caster, SPELL_GUARDIAN_SPIRIT) ? SPELL_GUARDIAN_SPIRIT : 0;
        default:
            return 0;
    }
}

// Hand of Protection first: it leaves the holder suppressed for melee champions, who move on at once
uint32 ExternalRank(uint32 spellId)
{
    if (std::find(HAND_OF_PROTECTION_RANKS.begin(), HAND_OF_PROTECTION_RANKS.end(), spellId) !=
        HAND_OF_PROTECTION_RANKS.end())
        return 0;

    switch (spellId)
    {
        case SPELL_PAIN_SUPPRESSION:
            return 1;
        case SPELL_GUARDIAN_SPIRIT:
            return 2;
        default:
            return 3;
    }
}

void UpdateProtect(FactionChampionsDefenceState& state, std::vector<Member> const& members,
                   std::vector<Unit*> const& champions)
{
    state.protects.clear();

    std::unordered_map<ObjectGuid, uint32> attackers;
    for (Unit* champion : champions)
        if (IsPhysicalChampion(champion->GetEntry()) && !champion->HasUnitState(UNIT_STATE_LOST_CONTROL))
            if (Unit* victim = champion->GetVictim())
                if (victim->IsPlayer())
                    ++attackers[victim->GetGUID()];

    std::vector<Member> focused;
    for (Member const& member : members)
    {
        auto const hits = attackers.find(member.player->GetGUID());
        if (hits != attackers.end() && hits->second >= PROTECT_MIN_ATTACKERS && !ProtectCovered(member.player))
            focused.push_back(member);
    }

    if (focused.empty())
        return;

    std::sort(focused.begin(), focused.end(),
              [](Member const& a, Member const& b)
              {
                  if (a.healer != b.healer)
                      return a.healer;

                  if (a.player->GetHealthPct() != b.player->GetHealthPct())
                      return a.player->GetHealthPct() < b.player->GetHealthPct();

                  return a.player->GetGUID() < b.player->GetGUID();
              });

    std::vector<Member> casters;
    for (Member const& member : members)
    {
        uint8 const playerClass = member.player->getClass();
        if ((playerClass == CLASS_PALADIN || playerClass == CLASS_PRIEST) && GET_PLAYERBOT_AI(member.player) &&
            FreeToAct(member.player))
            casters.push_back(member);
    }

    // Healers heal, so the others go first
    SortByRole(casters, false);
    std::vector<bool> taken(casters.size(), false);

    for (Member const& member : focused)
    {
        bool const handOfProtection = !IsTankMember(member.player) && !member.player->HasAura(SPELL_FORBEARANCE);
        size_t chosen = casters.size();
        uint32 chosenSpell = 0;
        for (size_t i = 0; i < casters.size(); ++i)
        {
            if (taken[i])
                continue;

            Player* caster = casters[i].player;
            uint32 const spellId = ReadyExternal(caster, member.player, handOfProtection);
            if (!spellId || (chosenSpell && ExternalRank(spellId) >= ExternalRank(chosenSpell)))
                continue;

            if (caster != member.player && !caster->IsWithinLOSInMap(member.player))
                continue;

            chosen = i;
            chosenSpell = spellId;
        }

        if (chosen == casters.size())
            continue;

        state.protects[casters[chosen].player->GetGUID()] = {member.player->GetGUID(), chosenSpell};
        taken[chosen] = true;
    }
}

std::vector<Player*> CorpsesOnMap(Player* bot)
{
    std::vector<Player*> corpses;
    Group* group = bot->GetGroup();
    Map* map = bot->FindMap();
    if (!group || !map)
        return corpses;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* player = ref->GetSource();
        if (!player || !player->IsInWorld() || player->FindMap() != map || player->IsAlive() ||
            player->getDeathState() != DeathState::Corpse || player->isResurrectRequested())
            continue;

        // A Soulstone or Reincarnation still pending brings it back without a Rebirth
        if (!player->HasPlayerFlag(PLAYER_FLAGS_GHOST) && player->GetUInt32Value(PLAYER_SELF_RES_SPELL))
            continue;

        corpses.push_back(player);
    }

    return corpses;
}

bool BeingResurrected(std::vector<Member> const& members, ObjectGuid corpse)
{
    return std::any_of(members.begin(), members.end(),
                       [corpse](Member const& member)
                       {
                           Spell const* spell = member.player->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                           if (!spell || spell->m_targets.GetUnitTargetGUID() != corpse)
                               return false;

                           SpellInfo const* info = spell->GetSpellInfo();
                           return info->HasEffect(SPELL_EFFECT_RESURRECT) ||
                                  info->HasEffect(SPELL_EFFECT_RESURRECT_NEW);
                       });
}

void UpdateBattleRes(FactionChampionsDefenceState& state, Player* bot, std::vector<Member> const& members,
                     std::vector<Unit*> const& champions)
{
    state.battleRes.clear();

    std::vector<Player*> corpses;
    for (Player* corpse : CorpsesOnMap(bot))
    {
        uint32 const near = static_cast<uint32>(
            std::count_if(champions.begin(), champions.end(), [corpse](Unit* champion)
                          { return champion->GetExactDist2d(corpse) <= CORPSE_PACK_RADIUS; }));
        if (near <= CORPSE_PACK_MAX && !BeingResurrected(members, corpse->GetGUID()))
            corpses.push_back(corpse);
    }

    if (corpses.empty())
        return;

    std::sort(corpses.begin(), corpses.end(),
              [](Player* a, Player* b)
              {
                  bool const healerA = PlayerbotAI::IsHeal(a);
                  bool const healerB = PlayerbotAI::IsHeal(b);
                  if (healerA != healerB)
                      return healerA;

                  return a->GetGUID() < b->GetGUID();
              });

    std::vector<std::pair<Player*, uint32>> druids;
    for (Member const& member : members)
    {
        Player* player = member.player;
        if (player->getClass() != CLASS_DRUID || !GET_PLAYERBOT_AI(player) || IsTankMember(player) ||
            !FreeToAct(player))
            continue;

        uint32 const rank = FirstKnownRank(player, REBIRTH_RANKS);
        if (SpellReady(player, rank))
            druids.push_back({player, rank});
    }

    std::vector<bool> taken(druids.size(), false);
    for (Player* corpse : corpses)
    {
        size_t chosen = druids.size();
        float chosenDistance = 0.0f;
        for (size_t i = 0; i < druids.size(); ++i)
        {
            Player* druid = druids[i].first;
            float const distance = druid->GetExactDist(corpse);
            if (taken[i] || !druid->IsWithinDistInMap(corpse, REBIRTH_RANGE) ||
                (chosen != druids.size() && distance >= chosenDistance))
                continue;

            chosen = i;
            chosenDistance = distance;
        }

        if (chosen == druids.size() || !druids[chosen].first->IsWithinLOSInMap(corpse))
            continue;

        state.battleRes[druids[chosen].first->GetGUID()] = {corpse->GetGUID(), druids[chosen].second};
        taken[chosen] = true;
    }
}

// Null unless live: a refresh after the kill would write into the next encounter's trace
FactionChampionsDefenceState* Refresh(PlayerbotAI* botAI)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    Player* bot = botAI->GetBot();
    if (!bot->GetInstanceId())
        return nullptr;

    FactionChampionsDefenceState& state = defenceStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.memoValid && state.memoMs == now)
        return &state;

    state.memoMs = now;
    state.memoValid = true;

    Unit* killTarget = FactionChampionsKillTarget(botAI);
    std::vector<Member> const members = MembersOnMap(bot);
    std::vector<Unit*> const champions = FactionChampionsInCombat(botAI);
    UpdatePhysicalLatch(state, botAI, killTarget);
    UpdateHazards(state, bot, members, now);
    UpdateTotemKillers(state, bot, members);
    UpdateCcDispels(state, members);
    UpdateMassDispel(state, botAI, members, killTarget);
    UpdatePurge(state, members, killTarget);
    UpdateGrip(state, botAI, members, champions, killTarget, now);
    UpdateSnares(state, botAI, members, champions);
    UpdateProtect(state, members, champions);
    UpdateBattleRes(state, bot, members, champions);
    return &state;
}

bool IgnoredBy(HazardSource const& source, Player* bot) { return source.kicker == bot->GetGUID(); }

Creature* AliveCreature(Player* bot, ObjectGuid guid)
{
    Map* map = bot->FindMap();
    if (!map || guid.IsEmpty())
        return nullptr;

    Creature* creature = map->GetCreature(guid);
    return creature && creature->IsAlive() ? creature : nullptr;
}

bool IsPhysicalAttacker(Player* bot)
{
    return !PlayerbotAI::IsHeal(bot) && (PlayerbotAI::IsMelee(bot) || bot->getClass() == CLASS_HUNTER);
}

Unit* DutyTarget(std::unordered_map<ObjectGuid, SpellDuty> const& duties, Player* bot, uint32& spellId)
{
    auto const duty = duties.find(bot->GetGUID());
    if (duty == duties.end())
        return nullptr;

    spellId = duty->second.spellId;
    return ObjectAccessor::GetUnit(*bot, duty->second.target);
}

// FindNearestPositionClearOfHazards answers Position() when nothing is clear
bool IsNoSpot(Position const& spot) { return !spot.GetPositionX() && !spot.GetPositionY(); }
}

Unit* FactionChampionsDispelCcTarget(PlayerbotAI* botAI, char const*& spell)
{
    FactionChampionsDefenceState* state = Refresh(botAI);
    if (!state)
        return nullptr;

    Player* bot = botAI->GetBot();
    auto const duty = state->ccDispels.find(bot->GetGUID());
    if (duty == state->ccDispels.end())
        return nullptr;

    Player* member = ObjectAccessor::GetPlayer(*bot, duty->second.member);
    if (!member || !member->IsAlive())
        return nullptr;

    spell = duty->second.spell;
    return member;
}

Unit* FactionChampionsMassDispelTarget(PlayerbotAI* botAI)
{
    if (!botAI || botAI->GetBot()->getClass() != CLASS_PRIEST)
        return nullptr;

    FactionChampionsDefenceState* state = Refresh(botAI);
    if (!state || state->massDispeller != botAI->GetBot()->GetGUID())
        return nullptr;

    return AliveCreature(botAI->GetBot(), state->massDispelTarget);
}

Unit* FactionChampionsPurgeTarget(PlayerbotAI* botAI, char const*& spell)
{
    FactionChampionsDefenceState* state = Refresh(botAI);
    if (!state || state->purger != botAI->GetBot()->GetGUID())
        return nullptr;

    Creature* target = AliveCreature(botAI->GetBot(), state->purgeTarget);
    if (!target)
        return nullptr;

    spell = state->purgeSpell;
    return target;
}

Unit* FactionChampionsPhysicalSwitchTarget(PlayerbotAI* botAI)
{
    if (!botAI || !IsPhysicalAttacker(botAI->GetBot()))
        return nullptr;

    FactionChampionsDefenceState* state = Refresh(botAI);
    return state ? AliveCreature(botAI->GetBot(), state->physical.Get()) : nullptr;
}

Unit* FactionChampionsTotemTarget(PlayerbotAI* botAI)
{
    FactionChampionsDefenceState* state = Refresh(botAI);
    if (!state)
        return nullptr;

    auto const hold = state->totemKillers.find(botAI->GetBot()->GetGUID());
    return hold != state->totemKillers.end() ? AliveCreature(botAI->GetBot(), hold->second) : nullptr;
}

bool FactionChampionsInAoe(PlayerbotAI* botAI)
{
    FactionChampionsDefenceState* state = Refresh(botAI);
    if (!state)
        return false;

    Player* bot = botAI->GetBot();
    return std::any_of(state->hazards.begin(), state->hazards.end(),
                       [bot](HazardSource const& source)
                       {
                           return !IgnoredBy(source, bot) &&
                                  bot->GetExactDist2d(source.pos) <= source.radius + IN_AOE_MARGIN;
                       });
}

std::vector<EncounterHelpers::HazardCircle> FactionChampionsAoeClearances(PlayerbotAI* botAI)
{
    std::vector<EncounterHelpers::HazardCircle> clearances;
    FactionChampionsDefenceState* state = Refresh(botAI);
    if (!state)
        return clearances;

    for (HazardSource const& source : state->hazards)
        if (!IgnoredBy(source, botAI->GetBot()))
            clearances.emplace_back(source.pos, source.clearance);

    return clearances;
}

bool FactionChampionsReachIntoAoe(PlayerbotAI* botAI)
{
    if (!botAI || !PlayerbotAI::IsMelee(botAI->GetBot()))
        return false;

    FactionChampionsDefenceState* state = Refresh(botAI);
    Unit* target = state ? CurrentTarget(botAI) : nullptr;
    if (!target)
        return false;

    // Sized to where the bot would stand: up to its melee range on the near side of the target
    Player* bot = botAI->GetBot();
    float const standOff = bot->GetMeleeRange(target);
    return std::any_of(state->hazards.begin(), state->hazards.end(),
                       [bot, target, standOff](HazardSource const& source)
                       {
                           return !IgnoredBy(source, bot) &&
                                  (target->GetGUID() == source.guid ||
                                   target->GetExactDist2d(source.pos) <= source.radius + IN_AOE_MARGIN + standOff);
                       });
}

bool FactionChampionsDispelBackfires(PlayerbotAI* botAI, Unit* target)
{
    return botAI && ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions) && CarriesUnstableAffliction(target);
}

bool FactionChampionsGripPending(PlayerbotAI* botAI)
{
    FactionChampionsDefenceState* state = Refresh(botAI);
    Unit* killTarget = state ? FactionChampionsKillTarget(botAI) : nullptr;
    return killTarget && !state->grip.dk.IsEmpty() && state->grip.target == killTarget->GetGUID() &&
           !FactionChampionsIsPulled(botAI, killTarget);
}

Unit* FactionChampionsGripTarget(PlayerbotAI* botAI, Position& stand)
{
    if (!botAI || botAI->GetBot()->getClass() != CLASS_DEATH_KNIGHT)
        return nullptr;

    FactionChampionsDefenceState* state = Refresh(botAI);
    if (!state || state->grip.dk != botAI->GetBot()->GetGUID())
        return nullptr;

    Creature* target = AliveCreature(botAI->GetBot(), state->grip.target);
    if (target)
        stand = state->grip.stand;

    return target;
}

Unit* FactionChampionsSnareTarget(PlayerbotAI* botAI, uint32& spellId)
{
    FactionChampionsDefenceState* state = Refresh(botAI);
    Unit* champion = state ? DutyTarget(state->snares, botAI->GetBot(), spellId) : nullptr;
    return champion && champion->IsAlive() ? champion : nullptr;
}

Unit* FactionChampionsProtectTarget(PlayerbotAI* botAI, uint32& spellId)
{
    FactionChampionsDefenceState* state = Refresh(botAI);
    Unit* member = state ? DutyTarget(state->protects, botAI->GetBot(), spellId) : nullptr;
    return member && member->IsAlive() ? member : nullptr;
}

Unit* FactionChampionsBattleResTarget(PlayerbotAI* botAI, uint32& spellId)
{
    if (!botAI || botAI->GetBot()->getClass() != CLASS_DRUID)
        return nullptr;

    FactionChampionsDefenceState* state = Refresh(botAI);
    Unit* corpse = state ? DutyTarget(state->battleRes, botAI->GetBot(), spellId) : nullptr;
    return corpse && !corpse->IsAlive() ? corpse : nullptr;
}

Position FactionChampionsAoeDodgeSpot(PlayerbotAI* botAI)
{
    FactionChampionsDefenceState* state = Refresh(botAI);
    if (!state)
        return Position();

    Player* bot = botAI->GetBot();
    std::vector<EncounterHelpers::HazardCircle> clearances;
    // The bot stands in at least one, its walk out may cross that one but no other
    std::vector<EncounterHelpers::HazardCircle> others;
    for (HazardSource const& source : state->hazards)
    {
        if (IgnoredBy(source, bot))
            continue;

        clearances.emplace_back(source.pos, source.clearance);
        if (bot->GetExactDist2d(source.pos) > source.radius + IN_AOE_MARGIN)
            others.emplace_back(source.pos, source.radius);
    }

    std::vector<Position> healers;
    for (Member const& member : MembersOnMap(bot))
        if (member.healer && member.player != bot)
            healers.push_back(member.player->GetPosition());

    // the pack follows its tank, so he never walks it toward the healers
    EncounterHelpers::HazardSweepCache cache;
    if (healers.empty() || FactionChampionsIsPackTank(bot))
        return EncounterHelpers::FindNearestPositionClearOfHazards(bot, clearances, AOE_SEARCH_RADIUS, &cache);

    float x = 0.0f;
    float y = 0.0f;
    for (Position const& healer : healers)
    {
        x += healer.GetPositionX();
        y += healer.GetPositionY();
    }

    float const count = static_cast<float>(healers.size());
    Position const centre(x / count, y / count, bot->GetPositionZ());
    Position const from = bot->GetPosition();
    auto const inHealReach = [&healers, &others, &from](float spotX, float spotY)
    {
        bool const reached = std::any_of(healers.begin(), healers.end(), [spotX, spotY](Position const& healer)
                                         { return healer.GetExactDist2d(spotX, spotY) <= HEAL_REACH; });
        Position const spot(spotX, spotY, from.GetPositionZ());
        return reached && std::none_of(others.begin(), others.end(),
                                       [&from, &spot](EncounterHelpers::HazardCircle const& circle)
                                       { return EncounterHelpers::SegmentCrossesCircle(from, spot, circle); });
    };

    constexpr float ringStep = 2.0f;
    constexpr float angleStep = static_cast<float>(M_PI) / 8.0f;
    Position spot = EncounterHelpers::FindNearestPositionClearOfHazards(bot, clearances, AOE_SEARCH_RADIUS, ringStep,
                                                                        angleStep, &centre, inHealReach, &cache);
    if (IsNoSpot(spot))
        spot = EncounterHelpers::FindNearestPositionClearOfHazards(bot, clearances, AOE_SEARCH_RADIUS, ringStep,
                                                                   angleStep, &centre, {}, &cache);

    return spot;
}

}

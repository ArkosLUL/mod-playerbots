#include "ToCHelpers_FactionChampionsDefence.h"

#include <algorithm>
#include <array>
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

struct FactionChampionsDefenceState
{
    RaidObs::ObsValue<ObjectGuid> physical{"fc.physical"};

    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value
    std::vector<HazardSource> hazards;
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
        auto const isLatched = [latched](Unit* champion)
        { return champion->GetGUID() == latched && !PhysicallyImmune(champion); };
        if (!latched.IsEmpty())
            pick = FactionChampionsNextTarget(botAI, isLatched);

        if (!pick)
            pick = FactionChampionsNextTarget(botAI, [](Unit* champion) { return !PhysicallyImmune(champion); });

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

std::vector<uint32> const& AoeCasterEntries()
{
    static std::vector<uint32> const entries = {
        static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_WARRIOR),
        static_cast<uint32>(ToCFactionChampions::NPC_HORDE_WARRIOR),
        static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_WARLOCK),
        static_cast<uint32>(ToCFactionChampions::NPC_HORDE_WARLOCK),
    };

    return entries;
}

// Cooldown only, no CanCastSpell or power test: the top ranks of Kick, Pummel and Shield Bash sit on
// the GCD and energy dips under Kick's cost between swings, so either would flip the kicker every swing
bool HasReadyMeleeInterrupt(Player* player, PlayerbotAI* ai, Unit* warlock)
{
    if (!player->IsWithinMeleeRange(warlock))
        return false;

    for (char const* name : MELEE_INTERRUPTS)
    {
        uint32 const id = ai->GetAiObjectContext()->GetValue<uint32>("spell id", name)->Get();
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
    bot->GetCreatureListWithEntryInGrid(creatures, AoeCasterEntries(), CHAMPION_SEARCH_RADIUS);

    state.hazards.clear();
    for (Creature* creature : creatures)
    {
        if (!creature || !creature->IsAlive())
            continue;

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
    UpdatePhysicalLatch(state, botAI, killTarget);
    UpdateHazards(state, bot, members, now);
    UpdateCcDispels(state, members);
    UpdateMassDispel(state, botAI, members, killTarget);
    UpdatePurge(state, members, killTarget);
    return &state;
}

bool IgnoredBy(HazardSource const& source, Player* bot) { return source.kicker == bot->GetGUID(); }

Creature* AliveChampion(Player* bot, ObjectGuid guid)
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

    return AliveChampion(botAI->GetBot(), state->massDispelTarget);
}

Unit* FactionChampionsPurgeTarget(PlayerbotAI* botAI, char const*& spell)
{
    FactionChampionsDefenceState* state = Refresh(botAI);
    if (!state || state->purger != botAI->GetBot()->GetGUID())
        return nullptr;

    Creature* target = AliveChampion(botAI->GetBot(), state->purgeTarget);
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
    return state ? AliveChampion(botAI->GetBot(), state->physical.Get()) : nullptr;
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

}

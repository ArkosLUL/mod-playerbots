/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Saurfang.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "AiFactory.h"
#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Shared.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDifficulty.h"
#include "RaidDirectTargeting.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

namespace
{
constexpr float BEAST_SCAN_RANGE = 100.0f;
constexpr uint32 BEAST_SCAN_MS = 200;
constexpr uint32 ROSTER_REFRESH_MS = 1000;
constexpr uint32 DUTY_REFRESH_MS = 200;
constexpr float TAUNT_RANGE = 30.0f;
constexpr float ROOM_RADIUS = 80.0f;
// Where reach spell walks a caster in: 28.5 + 0.5 plus both 1.5 yd reaches
constexpr float RANGED_REACH = 32.0f;
// 10 s after each wave every live beast gets Scent of Blood for 10 s. His casts can hold the event back
constexpr uint32 SCENT_FROM_MS = 9000;
constexpr uint32 SCENT_UNTIL_MS = 21500;
constexpr int32 TAUNT_SPARE_SLACK_MS = 500;
constexpr uint32 CONTROL_TIMEOUT_MS = 4000;
// A beast held this much longer needs no caster yet
constexpr int32 CONTROL_HELD_MS = 1000;
constexpr float MELEE_SPELL_RANGE = 5.0f;
constexpr float MARK_HEAL_BELOW = 75.0f;
constexpr float TANK_EMERGENCY_BELOW = 35.0f;
constexpr int32 BEACON_REFRESH_MS = 5000;

// The central slots first: from 1 a healer reaches every slot and tank spot
constexpr uint8 HEALER_SLOTS[] = {1, 3, 2, 10, 11};
constexpr uint8 RANGED_SLOTS[] = {7, 6, 5, 4, 9, 8, 1, 3, 2};

struct ControlSpell
{
    uint8 cls;
    char const* name;
};

// Best first: the stuns, then the snares. No root: under the kill duty's fire one breaks at its damage cap.
constexpr ControlSpell CONTROL_SPELLS[] = {
    {CLASS_PALADIN, "hammer of justice"}, {CLASS_WARRIOR, "concussion blow"},
    {CLASS_DRUID, "bash"},                {CLASS_DEATH_KNIGHT, "chains of ice"},
    {CLASS_MAGE, "slow"},                 {CLASS_HUNTER, "concussive shot"},
    {CLASS_SHAMAN, "frost shock"},        {CLASS_WARLOCK, "curse of exhaustion"},
    {CLASS_WARRIOR, "hamstring"},         {CLASS_HUNTER, "wing clip"},
};

constexpr uint64 HOLD_MECHANICS = (1ull << MECHANIC_STUN) | (1ull << MECHANIC_ROOT) | (1ull << MECHANIC_SNARE) |
                                  (1ull << MECHANIC_DAZE) | (1ull << MECHANIC_KNOCKOUT) | (1ull << MECHANIC_FREEZE);

struct Stand
{
    uint8 id = 0;
    Position spot;
    bool held = false;
    bool parked = false;
};

// The resolver is asked by its trigger, its action and the targeting rule for every action a bot
// weighs, and a bot never ticks twice in one ms.
struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
};

struct DbsState
{
    uint32 scanMs = 0;
    bool scanned = false;
    std::vector<ObjectGuid> beasts;
    std::unordered_set<ObjectGuid> beastsSeen;
    uint32 waveMs = 0;
    bool waveSeen = false;

    uint32 rosterMs = 0;
    bool rosterBuilt = false;
    std::vector<ObjectGuid> rangedDps;
    RaidObs::ObsGuidMap<uint8> slot{"dbs.slot"};

    uint32 dutyMs = 0;
    bool dutiesRan = false;
    RaidObs::ObsGuidMap<ObjectGuid> kill{"dbs.kill"};
    RaidObs::ObsGuidMap<ObjectGuid> control{"dbs.cc"};
    std::unordered_map<ObjectGuid, uint32> controlSince;
    // Keyed on the marked player, so one healer can carry two marks
    RaidObs::ObsGuidMap<ObjectGuid> markHealer{"dbs.markheal"};

    std::unordered_map<ObjectGuid, Stand> stands;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
};

RaidInstanceState<DbsState> dbsStates;

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

// A bot of its own, not a player, selfbot included: players hold no duty.
bool IsDutyBot(Player* player) { return GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

bool IsDbsBeast(Unit* unit) { return unit && unit->GetEntry() == NPC_DBS_BLOOD_BEAST; }

bool IsDbsUnit(Unit* unit) { return unit && (unit->GetEntry() == NPC_DBS_SAURFANG || IsDbsBeast(unit)); }

bool IsHeroic(Player* bot)
{
    Difficulty const difficulty = GetRaidMapDifficulty(bot);
    return difficulty == RAID_DIFFICULTY_10MAN_HEROIC || difficulty == RAID_DIFFICULTY_25MAN_HEROIC;
}

bool HasRune(Unit* unit) { return unit->HasAura(SPELL_DBS_RUNE_OF_BLOOD); }

void Note(Player* bot, char const* key, char const* value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

std::vector<Player*> MembersOnMap(Player* bot)
{
    std::vector<Player*> members;
    Group* group = bot->GetGroup();
    if (!group)
    {
        members.push_back(bot);
        return members;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && IccSameInstance(bot, member))
            members.push_back(member);
    }

    return members;
}

DbsState* LiveState(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    return bot && bot->GetInstanceId() && IsDbsLive(botAI) ? dbsStates.Find(bot->GetInstanceId()) : nullptr;
}

Position const& StandSpot(uint8 id)
{
    if (id >= 1 && id <= DBS_SLOT_COUNT)
        return ICC_DBS_SLOTS[id - 1];

    if (id >= DBS_STAND_TANK && id < DBS_STAND_TANK + DBS_TANK_SPOTS)
        return ICC_DBS_TANK_SPOTS[id - DBS_STAND_TANK];

    return ICC_DBS_STACK_SPOT;
}

void ScanBeasts(DbsState& state, Creature* boss, uint32 now)
{
    if (state.scanned && getMSTimeDiff(state.scanMs, now) < BEAST_SCAN_MS)
        return;

    state.scanned = true;
    state.scanMs = now;
    state.beasts.clear();

    std::unordered_set<ObjectGuid> seen;
    for (Creature* beast : IccGetCreaturesByEntries(boss, {NPC_DBS_BLOOD_BEAST}, BEAST_SCAN_RANGE))
    {
        if (!beast->IsInCombat())
            continue;

        ObjectGuid const guid = beast->GetGUID();
        state.beasts.push_back(guid);
        seen.insert(guid);
        if (!state.beastsSeen.count(guid))
        {
            state.waveMs = now;
            state.waveSeen = true;
        }
    }
    state.beastsSeen = std::move(seen);
}

std::vector<Creature*> LiveBeasts(Player* bot, DbsState const& state)
{
    std::vector<Creature*> beasts;
    for (ObjectGuid const& guid : state.beasts)
    {
        Creature* beast = bot->GetMap()->GetCreature(guid);
        if (beast && beast->IsAlive() && beast->IsInCombat())
            beasts.push_back(beast);
    }

    return beasts;
}

// The scent lands on every live beast, older waves too, so the latest wave's clock decides for all
bool Scented(Player* bot, DbsState const& state, uint32 now)
{
    if (!state.waveSeen || !IsHeroic(bot))
        return false;

    uint32 const age = getMSTimeDiff(state.waveMs, now);
    return age >= SCENT_FROM_MS && age <= SCENT_UNTIL_MS;
}

// On a player who is no tank. A pet it chews on is no one-shot.
Player* LooseVictim(Unit* beast)
{
    Unit* victim = beast->GetVictim();
    Player* player = victim ? victim->ToPlayer() : nullptr;
    return player && !IsTankPlayer(player) ? player : nullptr;
}

int32 HeldForMs(Unit* beast)
{
    int32 longest = 0;
    for (auto const& [id, application] : beast->GetAppliedAuras())
    {
        Aura const* aura = application->GetBase();
        if (aura->GetSpellInfo()->GetAllEffectsMechanicMask() & HOLD_MECHANICS)
            longest = std::max(longest, aura->GetDuration());
    }

    return longest;
}

bool CanPayFor(Player* bot, SpellInfo const* info)
{
    // POWER_HEALTH is -2 and Powers is signed, hence the cast.
    if (info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return true;

    return static_cast<int32>(bot->GetPower(Powers(info->PowerType))) >=
           info->CalcPowerCost(bot, info->GetSchoolMask());
}

// The member's best control spell it can land on the beast from where it stands, with its rank
char const* ReadyControlSpell(Player* member, Unit* beast, size_t& rank)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    if (!memberAI)
        return nullptr;

    for (size_t i = 0; i < std::size(CONTROL_SPELLS); ++i)
    {
        if (CONTROL_SPELLS[i].cls != member->getClass())
            continue;

        uint32 const spellId =
            memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", CONTROL_SPELLS[i].name)->Get();
        SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        // CanCastSpell lets a wrong form or stance through, so Hamstring would stick to a Defensive Stance warrior
        if (!info || !CanPayFor(member, info) || info->CheckShapeshift(member->GetShapeshiftForm()) != SPELL_CAST_OK)
            continue;

        float const maxRange = member->GetSpellMaxRangeForTarget(beast, info);
        float const minRange = member->GetSpellMinRangeForTarget(beast, info);
        bool const inReach = maxRange <= MELEE_SPELL_RANGE ? member->IsWithinMeleeRange(beast)
                                                           : member->IsWithinCombatRange(beast, maxRange);
        if (!inReach || (minRange > 0.0f && member->IsWithinCombatRange(beast, minRange)) ||
            !member->IsWithinLOSInMap(beast) || !memberAI->CanCastSpell(spellId, beast))
            continue;

        rank = i;
        return CONTROL_SPELLS[i].name;
    }

    return nullptr;
}

char const* TauntName(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_PALADIN:
            return "hand of reckoning";
        case CLASS_DEATH_KNIGHT:
            return "dark command";
        case CLASS_DRUID:
            return "growl";
        case CLASS_WARRIOR:
            return "taunt";
        default:
            return nullptr;
    }
}

// Off cooldown and castable in its current form. Growl needs bear form and Taunt Defensive Stance, and a
// tank elected without them would hold the swap without ever landing it.
bool TauntUsable(Player* member)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    char const* taunt = memberAI ? TauntName(member) : nullptr;
    uint32 const spellId = taunt ? memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", taunt)->Get() : 0;
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return info && !member->HasSpellCooldown(spellId) &&
           info->CheckShapeshift(member->GetShapeshiftForm()) == SPELL_CAST_OK;
}

// Paladins first, a mark on Beacon of Light, then the healers least needed on the tanks
uint8 MarkHealerRank(Player* member)
{
    switch (member->getClass())
    {
        case CLASS_PALADIN:
            return 0;
        case CLASS_PRIEST:
            return AiFactory::GetPlayerSpecTab(member) == PRIEST_TAB_HOLY ? 1 : 4;
        case CLASS_SHAMAN:
            return 2;
        case CLASS_DRUID:
            return 3;
        default:
            return 5;
    }
}

bool RuneYields(Player* bot, Creature* boss)
{
    if (!IsDutyBot(bot) || !IsTankPlayer(bot) || !HasRune(bot))
        return false;

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    return holder && holder != bot && IsTankPlayer(holder);
}

// The tank to take him now. A taunt aura on him means one just landed and his victim catches up next
// update, so nobody else spends one.
Player* SwapTaunter(Player* bot, Creature* boss)
{
    Unit* victim = boss->GetVictim();
    if (!victim || boss->HasAuraType(SPELL_AURA_MOD_TAUNT))
        return nullptr;

    Player* holder = victim->ToPlayer();
    bool const tankHolds = holder && IsTankPlayer(holder);
    if (tankHolds && !HasRune(holder))
        return nullptr;

    Player* free = nullptr;
    Player* runed = nullptr;
    for (Player* member : MembersOnMap(bot))
    {
        if (member == victim || !IsDutyBot(member) || !IsTankPlayer(member) ||
            member->GetExactDist2d(boss) > TAUNT_RANGE || !TauntUsable(member))
            continue;

        Player*& pick = HasRune(member) ? runed : free;
        if (!pick || member->GetGUID() < pick->GetGUID())
            pick = member;
    }

    if (free)
        return free;

    return tankHolds ? nullptr : runed;
}

// Spending the taunt on a beast still leaves the next swap covered: the bot's own rune outlasts the
// taunt's cooldown, the rune he puts on the holder next coming no earlier than that, or another tank
// free of it stands ready.
bool TauntSpareForBeast(PlayerbotAI* botAI, Creature* boss)
{
    Player* bot = botAI->GetBot();
    if (Aura* rune = bot->GetAura(SPELL_DBS_RUNE_OF_BLOOD))
    {
        char const* taunt = TauntName(bot);
        uint32 const spellId = taunt ? botAI->GetAiObjectContext()->GetValue<uint32>("spell id", taunt)->Get() : 0;
        if (spellId && rune->GetDuration() >= EncounterHelpers::SpellCooldownMs(bot, spellId) + TAUNT_SPARE_SLACK_MS)
            return true;
    }

    Unit* holder = boss->GetVictim();
    for (Player* member : MembersOnMap(bot))
    {
        if (member != bot && member != holder && IsDutyBot(member) && IsTankPlayer(member) && !HasRune(member) &&
            TauntUsable(member))
            return true;
    }

    return false;
}

void RefreshRoster(DbsState& state, Player* bot, uint32 now)
{
    if (state.rosterBuilt && getMSTimeDiff(state.rosterMs, now) < ROSTER_REFRESH_MS)
        return;

    state.rosterBuilt = true;
    state.rosterMs = now;

    std::vector<Player*> tanks;
    std::vector<Player*> healers;
    std::vector<Player*> ranged;
    for (Player* member : MembersOnMap(bot))
    {
        if (!IsDutyBot(member))
            continue;

        if (IsTankPlayer(member))
            tanks.push_back(member);
        else if (PlayerbotAI::IsHeal(member))
            healers.push_back(member);
        else if (PlayerbotAI::IsRanged(member))
            ranged.push_back(member);
    }

    std::sort(healers.begin(), healers.end(), IccGuidLess());
    std::sort(ranged.begin(), ranged.end(), IccGuidLess());
    state.rangedDps.clear();
    for (Player* member : ranged)
        state.rangedDps.push_back(member->GetGUID());

    std::sort(tanks.begin(), tanks.end(),
              [](Player* a, Player* b)
              {
                  bool const mainA = PlayerbotAI::IsMainTank(a);
                  bool const mainB = PlayerbotAI::IsMainTank(b);
                  return mainA != mainB ? mainA : a->GetGUID() < b->GetGUID();
              });
    // A hunter can't shoot from the stack, inside its minimum range
    std::stable_sort(ranged.begin(), ranged.end(),
                     [](Player* a, Player* b)
                     { return (a->getClass() == CLASS_HUNTER) > (b->getClass() == CLASS_HUNTER); });

    std::unordered_map<ObjectGuid, uint8> wanted;
    for (size_t i = 0; i < tanks.size() && i < DBS_TANK_SPOTS; ++i)
        wanted[tanks[i]->GetGUID()] = static_cast<uint8>(DBS_STAND_TANK + i);

    bool taken[DBS_SLOT_COUNT + 1] = {};
    auto const keep = [&](Player* member, bool healer)
    {
        auto const held = state.slot.find(member->GetGUID());
        if (held == state.slot.end())
            return false;

        uint8 const id = held->second;
        if (id < 1 || id > (healer ? DBS_SLOT_COUNT : DBS_INNER_SLOTS) || taken[id])
            return false;

        taken[id] = true;
        wanted[member->GetGUID()] = id;
        return true;
    };
    auto const take = [&](uint8 const* order, size_t count, Player* member)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (taken[order[i]])
                continue;

            taken[order[i]] = true;
            wanted[member->GetGUID()] = order[i];
            return true;
        }

        return false;
    };

    std::vector<Player*> openHealers;
    std::vector<Player*> openRanged;
    for (Player* member : healers)
        if (!keep(member, true))
            openHealers.push_back(member);
    for (Player* member : ranged)
        if (!keep(member, false))
            openRanged.push_back(member);

    std::vector<Player*> spareHealers;
    for (Player* member : openHealers)
        if (!take(HEALER_SLOTS, std::size(HEALER_SLOTS), member))
            spareHealers.push_back(member);
    for (Player* member : openRanged)
        if (!take(RANGED_SLOTS, std::size(RANGED_SLOTS), member))
            wanted[member->GetGUID()] = DBS_STAND_STACK;
    for (Player* member : spareHealers)
        if (!take(RANGED_SLOTS, std::size(RANGED_SLOTS), member))
            wanted[member->GetGUID()] = DBS_STAND_STACK;

    std::vector<ObjectGuid> departed;
    for (auto const& [guid, id] : state.slot.Raw())
        if (!wanted.count(guid))
            departed.push_back(guid);
    for (ObjectGuid const& guid : departed)
        state.slot.erase(guid);
    for (auto const& [guid, id] : wanted)
        state.slot.Set(guid, id);
}

void UpdateKill(DbsState& state, Player* bot, std::vector<Creature*> const& beasts)
{
    std::vector<Player*> ranged;
    for (ObjectGuid const& guid : state.rangedDps)
    {
        Player* member = ObjectAccessor::GetPlayer(*bot, guid);
        if (member && member->IsAlive() && IccSameInstance(bot, member))
            ranged.push_back(member);
    }

    std::unordered_map<ObjectGuid, ObjectGuid> wanted;
    if (!beasts.empty())
    {
        std::vector<size_t> const quota = IccBalancedGroupSizes(ranged.size(), beasts.size());
        std::vector<size_t> load(beasts.size(), 0);
        auto const indexOf = [&beasts](ObjectGuid guid)
        {
            for (size_t i = 0; i < beasts.size(); ++i)
                if (beasts[i]->GetGUID() == guid)
                    return static_cast<int32>(i);
            return -1;
        };

        std::vector<Player*> open;
        for (Player* member : ranged)
        {
            auto const held = state.kill.find(member->GetGUID());
            int32 const index = held == state.kill.end() ? -1 : indexOf(held->second);
            if (index >= 0 && load[index] < quota[index] && member->GetExactDist2d(beasts[index]) <= RANGED_REACH)
            {
                ++load[index];
                wanted[member->GetGUID()] = beasts[index]->GetGUID();
            }
            else
                open.push_back(member);
        }

        // The beast a bot hits is the one that comes for it, so it takes the furthest it reaches: that one
        // has the longest way to go through the slows.
        for (Player* member : open)
        {
            int32 pick = -1;
            bool pickUnder = false;
            float pickDistance = 0.0f;
            for (size_t i = 0; i < beasts.size(); ++i)
            {
                float const distance = member->GetExactDist2d(beasts[i]);
                if (distance > RANGED_REACH)
                    continue;

                bool const under = load[i] < quota[i];
                if (pick < 0 || (under && !pickUnder) || (under == pickUnder && distance > pickDistance))
                {
                    pick = static_cast<int32>(i);
                    pickUnder = under;
                    pickDistance = distance;
                }
            }

            if (pick >= 0)
            {
                ++load[pick];
                wanted[member->GetGUID()] = beasts[pick]->GetGUID();
            }
        }
    }

    std::vector<ObjectGuid> released;
    for (auto const& [guid, beast] : state.kill.Raw())
        if (!wanted.count(guid))
            released.push_back(guid);
    for (ObjectGuid const& guid : released)
        state.kill.erase(guid);
    for (auto const& [guid, beast] : wanted)
        state.kill.Set(guid, beast);
}

void UpdateControl(DbsState& state, Creature* boss, std::vector<Player*> const& members,
                   std::vector<Creature*> const& beasts, uint32 now)
{
    auto const memberOf = [&members](ObjectGuid guid) -> Player*
    {
        auto const found =
            std::find_if(members.begin(), members.end(), [guid](Player* p) { return p->GetGUID() == guid; });
        return found == members.end() ? nullptr : *found;
    };
    auto const beastOf = [&beasts](ObjectGuid guid) -> Creature*
    {
        auto const found =
            std::find_if(beasts.begin(), beasts.end(), [guid](Creature* c) { return c->GetGUID() == guid; });
        return found == beasts.end() ? nullptr : *found;
    };

    std::vector<ObjectGuid> released;
    for (auto const& [caster, beastGuid] : state.control.Raw())
    {
        Creature* beast = beastOf(beastGuid);
        auto const since = state.controlSince.find(caster);
        bool const stale = since == state.controlSince.end() || getMSTimeDiff(since->second, now) > CONTROL_TIMEOUT_MS;
        if (!memberOf(caster) || !beast || !LooseVictim(beast) || HeldForMs(beast) > CONTROL_HELD_MS || stale)
            released.push_back(caster);
    }
    for (ObjectGuid const& caster : released)
    {
        state.control.erase(caster);
        state.controlSince.erase(caster);
    }

    // The one nearest the player it runs at first
    struct Want
    {
        Creature* beast;
        float gap;
    };
    std::vector<Want> wants;
    for (Creature* beast : beasts)
    {
        Player* victim = LooseVictim(beast);
        if (victim && HeldForMs(beast) <= CONTROL_HELD_MS)
            wants.push_back({beast, beast->GetExactDist2d(victim)});
    }
    std::sort(wants.begin(), wants.end(),
              [](Want const& a, Want const& b)
              { return a.gap != b.gap ? a.gap < b.gap : a.beast->GetGUID() < b.beast->GetGUID(); });

    for (Want const& want : wants)
    {
        bool covered = false;
        for (auto const& [caster, beastGuid] : state.control.Raw())
            covered = covered || beastGuid == want.beast->GetGUID();
        if (covered)
            continue;

        Player* best = nullptr;
        size_t bestRank = 0;
        float bestDistance = 0.0f;
        // Every bot but the tank holding him
        for (Player* member : members)
        {
            if (!IsDutyBot(member) || member == boss->GetVictim() || state.control.count(member->GetGUID()))
                continue;

            size_t rank = 0;
            if (!ReadyControlSpell(member, want.beast, rank))
                continue;

            float const distance = member->GetExactDist2d(want.beast);
            if (!best || rank < bestRank || (rank == bestRank && distance < bestDistance))
            {
                best = member;
                bestRank = rank;
                bestDistance = distance;
            }
        }

        if (best)
        {
            state.control.Set(best->GetGUID(), want.beast->GetGUID());
            state.controlSince[best->GetGUID()] = now;
        }
    }
}

void UpdateMarks(DbsState& state, std::vector<Player*> const& members)
{
    std::vector<Player*> marks;
    std::vector<Player*> healers;
    for (Player* member : members)
    {
        if (member->HasAura(SPELL_DBS_MARK))
            marks.push_back(member);
        if (IsDutyBot(member) && PlayerbotAI::IsHeal(member) && !IsTankPlayer(member))
            healers.push_back(member);
    }
    std::sort(marks.begin(), marks.end(), IccGuidLess());
    std::sort(healers.begin(), healers.end(),
              [](Player* a, Player* b)
              {
                  uint8 const rankA = MarkHealerRank(a);
                  uint8 const rankB = MarkHealerRank(b);
                  return rankA != rankB ? rankA < rankB : a->GetGUID() < b->GetGUID();
              });

    auto const listed = [](std::vector<Player*> const& list, ObjectGuid guid)
    { return std::any_of(list.begin(), list.end(), [guid](Player* p) { return p->GetGUID() == guid; }); };

    std::vector<ObjectGuid> released;
    std::unordered_map<ObjectGuid, size_t> load;
    for (auto const& [mark, healer] : state.markHealer.Raw())
    {
        if (!listed(marks, mark) || !listed(healers, healer))
            released.push_back(mark);
        else
            ++load[healer];
    }
    for (ObjectGuid const& mark : released)
        state.markHealer.erase(mark);

    float const healDistance = sPlayerbotAIConfig.HealDistance;
    for (Player* mark : marks)
    {
        if (state.markHealer.count(mark->GetGUID()))
            continue;

        // In reach of the mark before least loaded, then the rank order the list is sorted in
        Player* best = nullptr;
        bool bestNear = false;
        size_t bestLoad = 0;
        for (Player* healer : healers)
        {
            bool const near = healer->GetExactDist2d(mark) <= healDistance;
            size_t const carried = load[healer->GetGUID()];
            if (!best || (near && !bestNear) || (near == bestNear && carried < bestLoad))
            {
                best = healer;
                bestNear = near;
                bestLoad = carried;
            }
        }

        if (best)
        {
            state.markHealer.Set(mark->GetGUID(), best->GetGUID());
            ++load[best->GetGUID()];
        }
    }
}

void UpdateStand(DbsState& state, Player* bot)
{
    Stand& stand = state.stands[bot->GetGUID()];
    auto const held = state.slot.find(bot->GetGUID());
    if (!bot->IsAlive() || held == state.slot.end())
    {
        stand.held = false;
        return;
    }

    if (!stand.held || stand.id != held->second)
    {
        stand.id = held->second;
        stand.spot = StandSpot(stand.id);
        stand.held = true;
        stand.parked = false;
    }

    float const distance = bot->GetExactDist2d(stand.spot);
    stand.parked = stand.parked ? distance <= DBS_STAND_UNPARK : distance <= DBS_STAND_PARK;
}

Stand const* HeldStand(PlayerbotAI* botAI)
{
    DbsState* state = LiveState(botAI);
    if (!state)
        return nullptr;

    auto const itr = state->stands.find(botAI->GetBot()->GetGUID());
    return itr != state->stands.end() && itr->second.held ? &itr->second : nullptr;
}

Unit* MeleeBeast(PlayerbotAI* botAI, std::vector<Creature*> const& beasts)
{
    Player* bot = botAI->GetBot();
    Unit* current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    Creature* pick = nullptr;
    for (Creature* beast : beasts)
    {
        if (!bot->IsWithinMeleeRange(beast))
            continue;

        if (beast == current)
            return beast;

        if (!pick || beast->GetHealthPct() < pick->GetHealthPct() ||
            (beast->GetHealthPct() == pick->GetHealthPct() && beast->GetGUID() < pick->GetGUID()))
            pick = beast;
    }

    return pick;
}

Unit* BeastOn(Player* bot, std::vector<Creature*> const& beasts)
{
    for (Creature* beast : beasts)
        if (beast->GetVictim() == bot)
            return beast;

    return nullptr;
}

Unit* ResolveTarget(PlayerbotAI* botAI, DbsState& state, Creature* boss)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if (!tank && !IsRaidDpsBot(bot))
        return nullptr;

    if (tank && boss->GetVictim() == bot)
    {
        Note(bot, "dbs.target", "boss");
        return boss;
    }

    bool const yielding = tank && RuneYields(bot, boss);
    char const* why = "skull";
    Unit* pick = GetRaidHumanSkull(botAI, yielding ? &IsDbsBeast : &IsDbsUnit);
    if (!pick)
    {
        std::vector<Creature*> const beasts = LiveBeasts(bot, state);
        if (tank)
        {
            why = "beast";
            pick = BeastOn(bot, beasts);
            if (!pick && yielding)
                pick = MeleeBeast(botAI, beasts);
        }
        else if (PlayerbotAI::IsRanged(bot))
        {
            why = "kill";
            auto const duty = state.kill.find(bot->GetGUID());
            for (Creature* beast : beasts)
                if (duty != state.kill.end() && beast->GetGUID() == duty->second)
                    pick = beast;
        }
        else
        {
            why = "reach";
            pick = MeleeBeast(botAI, beasts);
        }

        if (!pick && !yielding)
        {
            why = "boss";
            pick = boss;
        }
    }

    Note(bot, "dbs.target", pick ? why : "none");
    return pick;
}
}  // namespace

bool IsDbsLive(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->GetMapId() != ICC_MAP_ID)
        return false;

    InstanceScript* instance = bot->GetInstanceScript();
    return instance && instance->GetBossState(ICC_BOSS_DEATHBRINGER_SAURFANG) == IN_PROGRESS;
}

Creature* GetDbsBoss(PlayerbotAI* botAI)
{
    if (!IsDbsLive(botAI))
        return nullptr;

    Player* bot = botAI->GetBot();
    InstanceScript* instance = bot->GetInstanceScript();
    Creature* boss = ObjectAccessor::GetCreature(*bot, instance->GetGuidData(ICC_BOSS_DEATHBRINGER_SAURFANG));
    return boss && boss->IsAlive() && boss->IsInCombat() ? boss : nullptr;
}

bool IsDbsArmyHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return IsDbsLive(botAI) ||
           (bot->GetMapId() == ICC_MAP_ID && bot->GetExactDist2d(ICC_DBS_BOSS_SPOT) <= ROOM_RADIUS);
}

Unit* GetDbsTarget(PlayerbotAI* botAI)
{
    DbsState* state = LiveState(botAI);
    Creature* boss = state ? GetDbsBoss(botAI) : nullptr;
    if (!boss)
        return nullptr;

    TargetMemo& memo = state->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveTarget(botAI, *state, boss);
    }

    return memo.pick;
}

bool IsDbsSwapTaunter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* boss = GetDbsBoss(botAI);
    if (!boss || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return false;

    bool const elected = SwapTaunter(bot, boss) == bot;
    Note(bot, "dbs.swap", elected ? "1" : "0");
    return elected;
}

bool IsDbsRuneYielding(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* boss = GetDbsBoss(botAI);
    if (!boss || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return false;

    bool const yielding = RuneYields(bot, boss);
    Note(bot, "dbs.yield", yielding ? "1" : "0");
    return yielding;
}

Unit* GetDbsBeastToTaunt(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    DbsState* state = LiveState(botAI);
    Creature* boss = state ? GetDbsBoss(botAI) : nullptr;
    if (!boss || !IsDutyBot(bot) || !IsTankPlayer(bot) || boss->GetVictim() == bot || !TauntUsable(bot))
        return nullptr;

    Creature* best = nullptr;
    float bestGap = 0.0f;
    for (Creature* beast : LiveBeasts(bot, *state))
    {
        Player* victim = LooseVictim(beast);
        if (!victim || beast->HasUnitState(UNIT_STATE_STUNNED) || beast->HasAuraType(SPELL_AURA_MOD_TAUNT) ||
            bot->GetExactDist2d(beast) > TAUNT_RANGE)
            continue;

        float const gap = beast->GetExactDist2d(victim);
        if (!best || gap < bestGap || (gap == bestGap && beast->GetGUID() < best->GetGUID()))
        {
            best = beast;
            bestGap = gap;
        }
    }

    // A scented beast one-shots whoever it reaches, which costs more than a late swap
    if (best && !Scented(bot, *state, getMSTime()) && !TauntSpareForBeast(botAI, boss))
        return nullptr;

    return best;
}

Unit* GetDbsControlDuty(PlayerbotAI* botAI, char const*& spell)
{
    Player* bot = botAI->GetBot();
    DbsState* state = LiveState(botAI);
    if (!state)
        return nullptr;

    auto const duty = state->control.find(bot->GetGUID());
    if (duty == state->control.end())
        return nullptr;

    Creature* beast = bot->GetMap()->GetCreature(duty->second);
    if (!beast || !beast->IsAlive() || !LooseVictim(beast))
        return nullptr;

    size_t rank = 0;
    spell = ReadyControlSpell(bot, beast, rank);
    return spell ? beast : nullptr;
}

Player* GetDbsMarkWork(PlayerbotAI* botAI, bool& beacon)
{
    beacon = false;
    Player* bot = botAI->GetBot();
    DbsState* state = LiveState(botAI);
    if (!state || !bot->IsAlive())
        return nullptr;

    std::vector<Player*> marks;
    for (auto const& [markGuid, healer] : state->markHealer.Raw())
    {
        if (healer != bot->GetGUID())
            continue;

        Player* mark = ObjectAccessor::GetPlayer(*bot, markGuid);
        if (mark && mark->IsAlive() && IccSameInstance(bot, mark))
            marks.push_back(mark);
    }
    if (marks.empty())
        return nullptr;

    std::sort(marks.begin(), marks.end(), IccGuidLess());
    if (bot->getClass() == CLASS_PALADIN)
    {
        Aura* held = marks.front()->GetAura(SPELL_DBS_BEACON_OF_LIGHT, bot->GetGUID());
        if ((!held || held->GetDuration() < BEACON_REFRESH_MS) && botAI->CanCastSpell("beacon of light", marks.front()))
        {
            beacon = true;
            return marks.front();
        }
    }

    // A tank this low is the generic heals' to save first
    float const healDistance = sPlayerbotAIConfig.HealDistance;
    for (Player* member : MembersOnMap(bot))
        if (IsTankPlayer(member) && member->GetHealthPct() < TANK_EMERGENCY_BELOW &&
            bot->GetExactDist2d(member) <= healDistance)
            return nullptr;

    Player* low = nullptr;
    for (Player* mark : marks)
    {
        if (mark->GetHealthPct() >= MARK_HEAL_BELOW || bot->GetExactDist2d(mark) > healDistance ||
            !bot->IsWithinLOSInMap(mark))
            continue;

        if (!low || mark->GetHealthPct() < low->GetHealthPct())
            low = mark;
    }

    return low;
}

bool IsDbsBeaconDutyHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_PALADIN)
        return false;

    DbsState* state = LiveState(botAI);
    if (!state)
        return false;

    for (auto const& [mark, healer] : state->markHealer.Raw())
        if (healer == bot->GetGUID())
            return true;

    return false;
}

bool IsDbsHoldingStand(PlayerbotAI* botAI) { return HeldStand(botAI) != nullptr; }

bool IsDbsHoldingSpreadStand(PlayerbotAI* botAI)
{
    Stand const* stand = HeldStand(botAI);
    return stand && stand->id <= DBS_STAND_STACK;
}

bool IsDbsHoldingTankStand(PlayerbotAI* botAI)
{
    Stand const* stand = HeldStand(botAI);
    return stand && stand->id >= DBS_STAND_TANK;
}

bool GetDbsStandWalk(PlayerbotAI* botAI, Position& spot)
{
    Stand const* stand = HeldStand(botAI);
    if (!stand || stand->parked)
        return false;

    spot = stand->spot;
    return true;
}

void TickDbsEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    // A wipe or a kill ends the pull: the next one rebuilds every slot and duty
    Creature* boss = GetDbsBoss(botAI);
    if (!boss)
    {
        if (dbsStates.Find(bot->GetInstanceId()))
            dbsStates.Reset(bot->GetInstanceId());
        return;
    }

    Note(bot, "dbs.phase", boss->HasAura(SPELL_DBS_FRENZY) ? "2" : "1");

    DbsState& state = dbsStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    ScanBeasts(state, boss, now);
    RefreshRoster(state, bot, now);

    if (!state.dutiesRan || getMSTimeDiff(state.dutyMs, now) >= DUTY_REFRESH_MS)
    {
        state.dutiesRan = true;
        state.dutyMs = now;
        std::vector<Player*> const members = MembersOnMap(bot);
        std::vector<Creature*> const beasts = LiveBeasts(bot, state);
        UpdateKill(state, bot, beasts);
        UpdateControl(state, boss, members, beasts, now);
        UpdateMarks(state, members);
    }

    UpdateStand(state, bot);
}

RaidBurstWindow IccSaurfangBurstWindow(PlayerbotAI* botAI)
{
    RaidBurstWindow window;
    Creature* boss = GetDbsBoss(botAI);
    window.allowLust = boss && boss->HasAura(SPELL_DBS_FRENZY);
    return window;
}

std::vector<uint32> const& GetDbsOpenerBosses(PlayerbotAI* /*botAI*/)
{
    static std::vector<uint32> const bosses = {NPC_DBS_SAURFANG};
    return bosses;
}

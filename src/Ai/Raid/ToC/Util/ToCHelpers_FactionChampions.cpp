#include "ToCHelpers_FactionChampions.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <unordered_map>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Creature.h"
#include "Group.h"
#include "Map.h"
#include "Pet.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDirectTargeting.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampionsBall.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "ToCHelpers_Shared.h"
#include "ToCHelpers_Staging.h"

namespace TrialOfTheCrusaderHelpers
{

namespace
{
struct KillOrderRow
{
    ToCFactionChampions alliance;
    ToCFactionChampions horde;
};

constexpr KillOrderRow KILL_ORDER[] = {
    {ToCFactionChampions::NPC_ALLIANCE_PALADIN_HOLY, ToCFactionChampions::NPC_HORDE_PALADIN_HOLY},
    {ToCFactionChampions::NPC_ALLIANCE_PRIEST_DISCIPLINE, ToCFactionChampions::NPC_HORDE_PRIEST_DISCIPLINE},
    {ToCFactionChampions::NPC_ALLIANCE_SHAMAN_RESTORATION, ToCFactionChampions::NPC_HORDE_SHAMAN_RESTORATION},
    {ToCFactionChampions::NPC_ALLIANCE_DRUID_RESTORATION, ToCFactionChampions::NPC_HORDE_DRUID_RESTORATION},
    {ToCFactionChampions::NPC_ALLIANCE_HUNTER, ToCFactionChampions::NPC_HORDE_HUNTER},
    {ToCFactionChampions::NPC_ALLIANCE_MAGE, ToCFactionChampions::NPC_HORDE_MAGE},
    {ToCFactionChampions::NPC_ALLIANCE_WARLOCK, ToCFactionChampions::NPC_HORDE_WARLOCK},
    {ToCFactionChampions::NPC_ALLIANCE_PRIEST_SHADOW, ToCFactionChampions::NPC_HORDE_PRIEST_SHADOW},
    {ToCFactionChampions::NPC_ALLIANCE_DRUID_BALANCE, ToCFactionChampions::NPC_HORDE_DRUID_BALANCE},
    {ToCFactionChampions::NPC_ALLIANCE_ROGUE, ToCFactionChampions::NPC_HORDE_ROGUE},
    {ToCFactionChampions::NPC_ALLIANCE_DEATH_KNIGHT, ToCFactionChampions::NPC_HORDE_DEATH_KNIGHT},
    {ToCFactionChampions::NPC_ALLIANCE_WARRIOR, ToCFactionChampions::NPC_HORDE_WARRIOR},
    {ToCFactionChampions::NPC_ALLIANCE_PALADIN_RETRIBUTION, ToCFactionChampions::NPC_HORDE_PALADIN_RETRIBUTION},
    {ToCFactionChampions::NPC_ALLIANCE_SHAMAN_ENHANCEMENT, ToCFactionChampions::NPC_HORDE_SHAMAN_ENHANCEMENT},
};

constexpr uint8 NOT_A_CHAMPION = static_cast<uint8>(std::size(KILL_ORDER));

constexpr KillOrderRow DANGER_ORDER[] = {
    {ToCFactionChampions::NPC_ALLIANCE_ROGUE, ToCFactionChampions::NPC_HORDE_ROGUE},
    {ToCFactionChampions::NPC_ALLIANCE_DEATH_KNIGHT, ToCFactionChampions::NPC_HORDE_DEATH_KNIGHT},
    {ToCFactionChampions::NPC_ALLIANCE_PALADIN_RETRIBUTION, ToCFactionChampions::NPC_HORDE_PALADIN_RETRIBUTION},
    {ToCFactionChampions::NPC_ALLIANCE_WARRIOR, ToCFactionChampions::NPC_HORDE_WARRIOR},
};

constexpr float CHAMPION_SEARCH_RADIUS = 200.0f;
constexpr float COUNTERSPELL_RANGE = 30.0f;
constexpr int32 CC_MIN_HOLD_MS = 2500;
constexpr int32 CC_RECAST_LEAD_MS = 2000;
constexpr int32 CC_RECAST_SLACK_MS = 400;
constexpr uint32 CC_DUTY_MS = 3000;
constexpr uint32 CC_SETTLE_MS = 1500;

constexpr std::array<uint32, 4> POLYMORPH_RANKS = {12826, 12825, 12824, 118};
constexpr std::array<uint32, 3> FEAR_RANKS = {6215, 6213, 5782};
constexpr std::array<uint32, 2> BANISH_RANKS = {18647, 710};
constexpr uint32 SPELL_HEX_RANK = 51514;
constexpr float CC_WALK_REACH = 15.0f;
constexpr uint32 CC_WALK_MS = 3000;
constexpr float CC_CHASED_RADIUS = 30.0f;

constexpr float PACK_RADIUS = 20.0f;
constexpr float PACK_SEED_RADIUS = 15.0f;
constexpr float PULLED_FROM_PACK = 20.0f;
constexpr float PULLED_CLEAR = 12.0f;
constexpr float PULLED_SWITCH_MIN_PCT = 30.0f;
constexpr float GRIP_JUMP_YD = 10.0f;
constexpr uint32 GRIP_JUMP_MS = 1000;
constexpr uint32 TRACK_STEP_MS = 200;
constexpr float ESCAPE_DIST = 20.0f;

constexpr KillOrderRow ICE_ORDER[] = {
    {ToCFactionChampions::NPC_ALLIANCE_PALADIN_RETRIBUTION, ToCFactionChampions::NPC_HORDE_PALADIN_RETRIBUTION},
    {ToCFactionChampions::NPC_ALLIANCE_SHAMAN_ENHANCEMENT, ToCFactionChampions::NPC_HORDE_SHAMAN_ENHANCEMENT},
    {ToCFactionChampions::NPC_ALLIANCE_ROGUE, ToCFactionChampions::NPC_HORDE_ROGUE},
    {ToCFactionChampions::NPC_ALLIANCE_DEATH_KNIGHT, ToCFactionChampions::NPC_HORDE_DEATH_KNIGHT},
    {ToCFactionChampions::NPC_ALLIANCE_WARRIOR, ToCFactionChampions::NPC_HORDE_WARRIOR},
};

constexpr uint32 SPELL_CHAINS_OF_ICE = 45524;

struct Champion
{
    ObjectGuid guid;
    uint32 entry = 0;
    uint8 rank = NOT_A_CHAMPION;
    bool inCombat = false;
    bool touchable = false;
    float healthPct = 0.0f;
    uint32 ccAura = 0;
    bool disarmed = false;
    float x = 0.0f;
    float y = 0.0f;
    bool healer = false;
    bool pulled = false;
    bool escapee = false;
    bool covered = false;
    bool iced = false;
    float packDist = 0.0f;
};

struct TrackPoint
{
    uint32 ms = 0;
    float x = 0.0f;
    float y = 0.0f;
};

struct CcDuty
{
    ObjectGuid target;
    uint32 spellId = 0;
    uint32 sinceMs = 0;
    bool walk = false;
};

struct FactionChampionsState
{
    RaidObs::ObsValue<ObjectGuid> killTarget{"fc.kill"};
    RaidObs::ObsGuidMap<ObjectGuid> ccTargets{"fc.cc"};
    RaidObs::ObsGuidMap<uint32> ccAuras{"fc.ccaura"};
    RaidObs::ObsValue<ObjectGuid> danger{"fc.danger"};

    uint32 memoMs = 0;
    bool memoValid = false;
    bool live = false;
    uint32 liveSinceMs = 0;
    Position packAnchor;
    Position packCentre;
    std::vector<Champion> champions;
    ObjectGuid suspended;
    ObjectGuid skulled;
    ObjectGuid outOnce;
    std::unordered_map<ObjectGuid, std::vector<TrackPoint>> tracks;
    std::unordered_set<ObjectGuid> gripped;
    std::unordered_map<ObjectGuid, CcDuty> ccDuties;
    RaidObs::ObsGuidMap<ObjectGuid> ice{"fc.ice"};
    ToCGroupMemo<ObjectGuid> counterspellMage;
};

RaidInstanceState<FactionChampionsState> championStates;

uint8 KillRank(uint32 entry)
{
    for (uint8 rank = 0; rank < NOT_A_CHAMPION; ++rank)
        if (entry == static_cast<uint32>(KILL_ORDER[rank].alliance) ||
            entry == static_cast<uint32>(KILL_ORDER[rank].horde))
            return rank;

    return NOT_A_CHAMPION;
}

size_t DangerRank(uint32 entry)
{
    for (size_t rank = 0; rank < std::size(DANGER_ORDER); ++rank)
        if (entry == static_cast<uint32>(DANGER_ORDER[rank].alliance) ||
            entry == static_cast<uint32>(DANGER_ORDER[rank].horde))
            return rank;

    return std::size(DANGER_ORDER);
}

std::vector<uint32> const& ChampionEntries()
{
    static std::vector<uint32> const entries = []
    {
        std::vector<uint32> list;
        for (KillOrderRow const& row : KILL_ORDER)
        {
            list.push_back(static_cast<uint32>(row.alliance));
            list.push_back(static_cast<uint32>(row.horde));
        }

        return list;
    }();

    return entries;
}

bool CastsFear(uint32 entry)
{
    switch (static_cast<ToCFactionChampions>(entry))
    {
        case ToCFactionChampions::NPC_ALLIANCE_WARLOCK:
        case ToCFactionChampions::NPC_HORDE_WARLOCK:
        case ToCFactionChampions::NPC_ALLIANCE_PRIEST_DISCIPLINE:
        case ToCFactionChampions::NPC_HORDE_PRIEST_DISCIPLINE:
        case ToCFactionChampions::NPC_ALLIANCE_PRIEST_SHADOW:
        case ToCFactionChampions::NPC_HORDE_PRIEST_SHADOW:
        case ToCFactionChampions::NPC_ALLIANCE_WARRIOR:
        case ToCFactionChampions::NPC_HORDE_WARRIOR:
            return true;
        default:
            return false;
    }
}

uint32 ToCInstanceId(Player* bot)
{
    Map* map = bot ? bot->FindMap() : nullptr;
    return map && map->GetId() == TRIAL_OF_THE_CRUSADER_MAP_ID ? bot->GetInstanceId() : 0;
}

FactionChampionsState* StateFor(Player* bot)
{
    uint32 const instanceId = ToCInstanceId(bot);
    return instanceId ? &championStates.For(instanceId) : nullptr;
}

Creature* AliveCreature(Player* bot, ObjectGuid guid)
{
    Map* map = bot->FindMap();
    if (!map || guid.IsEmpty())
        return nullptr;

    Creature* creature = map->GetCreature(guid);
    return creature && creature->IsAlive() ? creature : nullptr;
}

template <typename V>
void EraseAll(RaidObs::ObsGuidMap<V>& map)
{
    std::vector<ObjectGuid> keys;
    for (auto const& held : map.Raw())
        keys.push_back(held.first);

    for (ObjectGuid const& key : keys)
        map.erase(key);
}

void EndPull(FactionChampionsState& state, Player* bot)
{
    state.champions.clear();
    state.suspended.Clear();
    state.skulled.Clear();
    state.outOnce.Clear();
    state.tracks.clear();
    state.gripped.clear();

    if (!state.killTarget.Get().IsEmpty())
    {
        RaidObs::Note(bot, "fc.switch", "reset");
        state.killTarget = ObjectGuid::Empty;
    }

    if (!state.danger.Get().IsEmpty())
        state.danger = ObjectGuid::Empty;

    EraseAll(state.ccTargets);
    EraseAll(state.ccAuras);
    state.ccDuties.clear();
    EraseAll(state.ice);
}

uint32 PlayerCcAura(Unit* unit, int32* leftMs = nullptr)
{
    uint32 spellId = 0;
    int32 longest = 0;
    for (AuraType type : {SPELL_AURA_MOD_FEAR, SPELL_AURA_MOD_STUN, SPELL_AURA_MOD_CONFUSE})
    {
        for (AuraEffect const* effect : unit->GetAuraEffectsByType(type))
        {
            if (!effect->GetCasterGUID().IsPlayer())
                continue;

            int32 const left = effect->GetBase()->GetDuration();
            if (!spellId || left < 0 || (longest >= 0 && left > longest))
            {
                spellId = effect->GetId();
                longest = left;
            }
        }
    }

    if (leftMs)
        *leftMs = longest;

    return spellId;
}

bool BreaksOnDamage(SpellInfo const* info)
{
    if (info->AuraInterruptFlags & AURA_INTERRUPT_FLAG_TAKE_DAMAGE)
        return true;

    if (!info->ProcFlags || info->HasAura(SPELL_AURA_PROC_TRIGGER_SPELL))
        return false;

    return info->HasAura(SPELL_AURA_MOD_CONFUSE) || info->HasAura(SPELL_AURA_MOD_FEAR) ||
           info->HasAura(SPELL_AURA_MOD_STUN) || info->HasAura(SPELL_AURA_MOD_ROOT) ||
           info->HasAura(SPELL_AURA_TRANSFORM);
}

bool AllDiminish(Unit const* unit)
{
    Creature const* creature = unit->ToCreature();
    return creature && creature->HasFlagsExtra(CREATURE_FLAG_EXTRA_ALL_DIMINISH);
}

int32 NextCcDurationMs(Unit* target, SpellInfo const* info)
{
    int32 duration = info->GetMaxDuration();
    DiminishingGroup const group = GetDiminishingReturnsGroupForSpell(info, false);
    if (duration < 0 || group == DIMINISHING_NONE)
        return duration;

    Unit const* owner = target->GetOwner();
    Unit const* drTarget = owner ? owner : target;

    int32 const limit = GetDiminishingReturnsLimitDuration(group, info);
    if (limit > 0 && duration > limit && (drTarget->IsPlayer() || AllDiminish(drTarget)))
        duration = limit;

    bool const likePlayer = drTarget->IsPlayer() || AllDiminish(target);
    DiminishingReturnsType const type = GetDiminishingReturnsGroupType(group);
    if (type != DRTYPE_ALL && !(type == DRTYPE_PLAYER && likePlayer))
        return duration;

    switch (target->GetDiminishing(group))
    {
        case DIMINISHING_LEVEL_2:
            return duration / 2;
        case DIMINISHING_LEVEL_3:
            return duration / 4;
        case DIMINISHING_LEVEL_IMMUNE:
            return 0;
        default:
            return duration;
    }
}

bool CanCast(Player* player, uint32 spellId)
{
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info || !player->HasSpell(spellId) || player->HasSpellCooldown(spellId) ||
        info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return false;

    return info->CalcPowerCost(player, info->GetSchoolMask()) <=
           static_cast<int32>(player->GetPower(Powers(info->PowerType)));
}

template <std::size_t N>
uint32 FirstKnownRank(Player* player, std::array<uint32, N> const& ranks)
{
    for (uint32 rank : ranks)
        if (player->HasSpell(rank))
            return rank;

    return 0;
}

bool InControl(Player* player)
{
    return !player->HasUnitState(UNIT_STATE_LOST_CONTROL) && !player->HasUnitFlag(UNIT_FLAG_SILENCED);
}

bool CycloneBot(Player* member)
{
    return member && member->getClass() == CLASS_DRUID && GET_PLAYERBOT_AI(member) && IsRaidDpsBot(member) &&
           !PlayerbotAI::IsMelee(member) && member->HasSpell(SPELL_CYCLONE);
}

std::vector<uint32> CcSpells(Player* member, Map* map)
{
    std::vector<uint32> spells;
    if (!member || !member->IsInWorld() || !member->IsAlive() || member->FindMap() != map ||
        !GET_PLAYERBOT_AI(member) || !IsRaidDpsBot(member))
        return spells;

    auto const add = [&spells](uint32 spellId)
    {
        if (spellId)
            spells.push_back(spellId);
    };

    switch (member->getClass())
    {
        case CLASS_MAGE:
            add(FirstKnownRank(member, POLYMORPH_RANKS));
            break;
        case CLASS_WARLOCK:
            add(FirstKnownRank(member, BANISH_RANKS));
            add(FirstKnownRank(member, FEAR_RANKS));
            break;
        case CLASS_DRUID:
            if (CycloneBot(member))
                add(SPELL_CYCLONE);
            break;
        case CLASS_SHAMAN:
            if (member->HasSpell(SPELL_HEX_RANK))
                add(SPELL_HEX_RANK);
            break;
        default:
            break;
    }

    return spells;
}

bool IsCcSpell(SpellInfo const* info)
{
    return info->HasAura(SPELL_AURA_MOD_FEAR) || info->HasAura(SPELL_AURA_MOD_STUN) ||
           info->HasAura(SPELL_AURA_MOD_CONFUSE);
}

bool CastingCcAt(Player* member, ObjectGuid target, uint32 spellId = 0)
{
    Spell const* spell = member->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!spell || spell->m_targets.GetUnitTargetGUID() != target)
        return false;

    SpellInfo const* info = spell->GetSpellInfo();
    return spellId ? info->Id == spellId : IsCcSpell(info);
}

bool CarriesGroupDot(Unit* target)
{
    for (AuraType type : {SPELL_AURA_PERIODIC_DAMAGE, SPELL_AURA_PERIODIC_DAMAGE_PERCENT, SPELL_AURA_PERIODIC_LEECH})
    {
        for (AuraEffect const* effect : target->GetAuraEffectsByType(type))
        {
            Unit* caster = effect->GetCaster();
            if (caster && caster->GetCharmerOrOwnerPlayerOrPlayerItself())
                return true;
        }
    }

    return false;
}

void ScanChampions(FactionChampionsState& state, Player* bot)
{
    static ToCEntrySet const championSet(ChampionEntries());
    std::vector<Creature*> creatures;
    GetCreatureListWithEntriesInGrid(bot, creatures, championSet, CHAMPION_SEARCH_RADIUS);

    state.champions.clear();
    for (Creature* creature : creatures)
    {
        if (!creature || !creature->IsAlive())
            continue;

        Champion champion;
        champion.guid = creature->GetGUID();
        champion.entry = creature->GetEntry();
        champion.rank = KillRank(champion.entry);
        champion.inCombat = creature->IsInCombat();
        champion.touchable = !(creature->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL) &&
                               creature->IsImmunedToDamage(SPELL_SCHOOL_MASK_MAGIC));
        champion.healthPct = creature->GetHealthPct();
        champion.ccAura = PlayerCcAura(creature);
        champion.disarmed = creature->HasAuraType(SPELL_AURA_MOD_DISARM);
        champion.x = creature->GetPositionX();
        champion.y = creature->GetPositionY();
        champion.healer = IsFactionChampionHealer(champion.entry);
        champion.iced = std::any_of(state.ice.begin(), state.ice.end(),
                                    [creature](auto const& duty)
                                    {
                                        return duty.second == creature->GetGUID() &&
                                               creature->HasAura(SPELL_CHAINS_OF_ICE, duty.first);
                                    });
        Unit* victim = creature->GetVictim();
        Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
        champion.covered = champion.iced || (victimPlayer && PlayerbotAI::IsTank(victimPlayer));
        champion.escapee = creature->GetExactDist2d(&state.packCentre) > ESCAPE_DIST && !champion.covered;
        state.champions.push_back(champion);
    }

    std::sort(state.champions.begin(), state.champions.end(),
              [](Champion const& a, Champion const& b)
              { return a.rank != b.rank ? a.rank < b.rank : a.guid < b.guid; });
}

float Dist2d(float ax, float ay, float bx, float by) { return std::hypot(ax - bx, ay - by); }

bool PackMean(FactionChampionsState const& state, ObjectGuid killTarget, float aroundX, float aroundY, float& x,
              float& y)
{
    float sumX = 0.0f;
    float sumY = 0.0f;
    uint32 count = 0;
    for (Champion const& champion : state.champions)
    {
        if (!champion.inCombat || champion.guid == killTarget ||
            Dist2d(aroundX, aroundY, champion.x, champion.y) > PACK_RADIUS)
            continue;

        sumX += champion.x;
        sumY += champion.y;
        ++count;
    }

    if (!count)
        return false;

    x = sumX / count;
    y = sumY / count;
    return true;
}

void TrackHealer(FactionChampionsState& state, Champion const& champion, uint32 now)
{
    std::vector<TrackPoint>& track = state.tracks[champion.guid];
    track.erase(std::remove_if(track.begin(), track.end(),
                               [now](TrackPoint const& point) { return getMSTimeDiff(point.ms, now) > GRIP_JUMP_MS; }),
                track.end());

    if (std::any_of(track.begin(), track.end(), [&champion](TrackPoint const& point)
                    { return Dist2d(point.x, point.y, champion.x, champion.y) >= GRIP_JUMP_YD; }))
        state.gripped.insert(champion.guid);

    if (track.empty() || getMSTimeDiff(track.back().ms, now) >= TRACK_STEP_MS)
        track.push_back({now, champion.x, champion.y});
}

void UpdatePack(FactionChampionsState& state, uint32 now)
{
    ObjectGuid const killTarget = state.killTarget.Get();
    float x = 0.0f;
    float y = 0.0f;
    bool found = PackMean(state, killTarget, state.packCentre.GetPositionX(), state.packCentre.GetPositionY(), x, y);
    if (!found)
    {
        Champion const* densest = nullptr;
        uint32 most = 0;
        for (Champion const& champion : state.champions)
        {
            if (!champion.inCombat || champion.guid == killTarget)
                continue;

            uint32 const packed = static_cast<uint32>(std::count_if(
                state.champions.begin(), state.champions.end(), [&champion](Champion const& other)
                { return other.inCombat && Dist2d(other.x, other.y, champion.x, champion.y) <= PACK_SEED_RADIUS; }));
            if (!densest || packed > most)
            {
                densest = &champion;
                most = packed;
            }
        }

        found = densest && PackMean(state, killTarget, densest->x, densest->y, x, y);
    }

    if (found)
        state.packCentre.Relocate(x, y, state.packAnchor.GetPositionZ());

    for (Champion& champion : state.champions)
    {
        champion.packDist = state.packCentre.GetExactDist2d(champion.x, champion.y);
        champion.pulled = false;
        if (!champion.healer)
            continue;

        TrackHealer(state, champion, now);
        if (champion.packDist < PULLED_FROM_PACK)
            state.gripped.erase(champion.guid);

        if (!champion.inCombat || champion.packDist < PULLED_FROM_PACK || !state.gripped.count(champion.guid))
            continue;

        champion.pulled = std::none_of(state.champions.begin(), state.champions.end(),
                                       [&champion](Champion const& other)
                                       {
                                           return other.guid != champion.guid &&
                                                  Dist2d(other.x, other.y, champion.x, champion.y) < PULLED_CLEAR;
                                       });
    }
}

void UpdateCcAuras(FactionChampionsState& state)
{
    std::vector<ObjectGuid> cleared;
    for (auto const& held : state.ccAuras.Raw())
    {
        ObjectGuid const guid = held.first;
        bool const stillHeld = std::any_of(state.champions.begin(), state.champions.end(),
                                           [guid](Champion const& champion)
                                           { return champion.guid == guid && champion.ccAura; });
        if (!stillHeld)
            cleared.push_back(guid);
    }

    for (ObjectGuid const& guid : cleared)
        state.ccAuras.erase(guid);

    for (Champion const& champion : state.champions)
        if (champion.ccAura)
            state.ccAuras.Set(champion.guid, champion.ccAura);
}

bool IsChampionUnit(Unit* unit) { return unit->IsCreature() && IsFactionChampion(unit->GetEntry()); }

Champion const* FindCandidate(FactionChampionsState const& state, ObjectGuid guid)
{
    if (guid.IsEmpty())
        return nullptr;

    for (Champion const& champion : state.champions)
        if (champion.guid == guid)
            return champion.inCombat ? &champion : nullptr;

    return nullptr;
}

Champion const* BlobHealer(FactionChampionsState const& state)
{
    Champion const* kill = FindCandidate(state, state.killTarget.Get());
    if (!kill)
        return nullptr;

    Champion const* best = nullptr;
    float bestDist = 0.0f;
    for (Champion const& champion : state.champions)
    {
        if (!champion.healer || !champion.inCombat || champion.pulled || champion.guid == kill->guid ||
            champion.guid == state.suspended)
            continue;

        float const dist = std::hypot(champion.x - kill->x, champion.y - kill->y);
        if (!best || dist < bestDist)
        {
            best = &champion;
            bestDist = dist;
        }
    }

    return best;
}

bool CycloneStandUp(FactionChampionsState const& state, Player* member)
{
    return CycloneBot(member) && !state.outOnce.IsEmpty() && state.outOnce == state.killTarget.Get() &&
           BlobHealer(state);
}

void UpdateDanger(FactionChampionsState& state)
{
    ObjectGuid const kill = state.killTarget.Get();
    Champion const* best = nullptr;
    auto const held = [](Champion const& champion) { return champion.ccAura || champion.disarmed; };
    for (Champion const& champion : state.champions)
    {
        if (!champion.inCombat || champion.pulled || champion.iced || champion.guid == kill ||
            champion.guid == state.suspended || DangerRank(champion.entry) == std::size(DANGER_ORDER))
            continue;

        if (!best || std::make_pair(held(champion), DangerRank(champion.entry)) <
                         std::make_pair(held(*best), DangerRank(best->entry)))
            best = &champion;
    }

    state.danger = best ? best->guid : ObjectGuid::Empty;
}

Champion const* BestPulled(FactionChampionsState const& state, ObjectGuid exclude)
{
    Champion const* best = nullptr;
    for (Champion const& champion : state.champions)
        if (champion.inCombat && champion.touchable && champion.pulled && champion.guid != exclude &&
            (!best || champion.packDist > best->packDist))
            best = &champion;

    return best;
}

Champion const* BestCandidate(FactionChampionsState const& state, bool touchable, ObjectGuid exclude)
{
    for (Champion const& champion : state.champions)
        if (champion.inCombat && champion.touchable == touchable && champion.guid != exclude)
            return &champion;

    return nullptr;
}

void UpdateKillLatch(FactionChampionsState& state, Player* bot)
{
    ObjectGuid const latched = state.killTarget.Get();
    Champion const* current = FindCandidate(state, latched);
    Champion const* suspended = FindCandidate(state, state.suspended);
    if (!suspended)
        state.suspended.Clear();

    Champion const* skulled = FindCandidate(state, state.skulled);
    if (skulled && skulled->touchable)
    {
        if (skulled->guid != latched)
        {
            if (skulled->guid == state.suspended)
                state.suspended.Clear();

            RaidObs::Note(bot, "fc.switch", "skull");
            state.killTarget = skulled->guid;
        }

        return;
    }

    ObjectGuid next = latched;
    char const* reason = nullptr;

    if (current && current->touchable)
    {
        Champion const* best = BestCandidate(state, true, ObjectGuid::Empty);
        bool const healerBack = suspended && suspended->healer && !current->healer;
        if (suspended && suspended->touchable && (healerBack || suspended->healthPct <= current->healthPct))
        {
            next = suspended->guid;
            reason = "back";
            state.suspended.Clear();
        }
        else if (!current->healer && best && best->healer)
        {
            next = best->guid;
            reason = "healer";
        }
        else if (!current->pulled && state.outOnce != current->guid && current->healthPct > PULLED_SWITCH_MIN_PCT)
        {
            if (Champion const* pulled = BestPulled(state, current->guid))
            {
                next = pulled->guid;
                reason = "pulled";
            }
        }
    }
    else if (current)
    {
        Champion const* other = BestPulled(state, current->guid);
        if (!other)
            other = BestCandidate(state, true, current->guid);

        if (other)
        {
            state.suspended = current->guid;
            next = other->guid;
            reason = "immune";
        }
    }
    else
    {
        Champion const* best = BestPulled(state, ObjectGuid::Empty);
        if (!best)
            best = BestCandidate(state, true, ObjectGuid::Empty);
        if (!best)
            best = BestCandidate(state, false, ObjectGuid::Empty);

        next = best ? best->guid : ObjectGuid::Empty;
        if (best)
        {
            reason = latched.IsEmpty() ? "first" : "dead";
            if (best->guid == state.suspended)
                state.suspended.Clear();

            if (!best->healer)
                for (Champion const& champion : state.champions)
                    if (champion.healer && champion.inCombat && !champion.touchable)
                    {
                        state.suspended = champion.guid;
                        break;
                    }
        }
    }

    if (reason)
        RaidObs::Note(bot, "fc.switch", reason);

    state.killTarget = next;

    if (state.outOnce != next)
        state.outOnce.Clear();

    Champion const* held = FindCandidate(state, next);
    if (held && held->pulled)
        state.outOnce = next;
}

bool ClearForBreakable(Unit* target, std::vector<Player*> const& members, std::vector<ObjectGuid> const& ccBots)
{
    for (Player* member : members)
    {
        if (std::find(ccBots.begin(), ccBots.end(), member->GetGUID()) != ccBots.end())
            continue;

        if (member->GetVictim() == target || member->GetTarget() == target->GetGUID())
            return false;

        Pet* pet = member->GetPet();
        if (pet && pet->IsAlive() && pet->GetVictim() == target)
            return false;
    }

    return !CarriesGroupDot(target);
}

std::vector<Champion const*> CcCandidates(FactionChampionsState const& state, ObjectGuid chainTarget)
{
    ObjectGuid const killGuid = state.killTarget.Get();
    Champion const* kill = nullptr;
    for (Champion const& champion : state.champions)
        if (champion.guid == killGuid)
            kill = &champion;

    std::vector<Champion const*> candidates;
    for (Champion const& champion : state.champions)
        if (champion.inCombat && !champion.pulled && !champion.iced && champion.guid != killGuid &&
            champion.guid != state.suspended && champion.guid != state.skulled && champion.guid != chainTarget)
            candidates.push_back(&champion);

    ObjectGuid const danger = state.danger.Get();
    auto const group = [danger](Champion const* champion)
    {
        if (champion->healer)
            return champion->covered ? 1 : 0;

        if (champion->guid == danger)
            return 2;

        if (champion->covered)
            return 5;

        return IsFactionChampionMelee(champion->entry) ? 4 : 3;
    };

    auto const fromKill = [kill](Champion const* champion)
    { return kill ? Dist2d(kill->x, kill->y, champion->x, champion->y) : 0.0f; };

    std::stable_sort(candidates.begin(), candidates.end(),
                     [&](Champion const* a, Champion const* b)
                     {
                         if (group(a) != group(b))
                             return group(a) < group(b);

                         return group(a) == 0 && fromKill(a) < fromKill(b);
                     });

    return candidates;
}

struct CcOption
{
    size_t caster;
    uint32 spellId;
    bool breaks;
    int32 holdMs;
    float distance;
    bool walk;
};

bool BetterCcOption(CcOption const& a, CcOption const& b)
{
    if (a.walk != b.walk)
        return !a.walk;

    if (a.breaks != b.breaks)
        return !a.breaks;

    if (a.holdMs != b.holdMs)
        return a.holdMs < 0 || (b.holdMs >= 0 && a.holdMs > b.holdMs);

    return a.distance < b.distance;
}

bool HoldsPastRecast(Aura const* aura, Player* caster, uint32 spellId)
{
    if (!aura)
        return false;

    if (aura->GetDuration() < 0)
        return true;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    int32 const castMs = info ? static_cast<int32>(info->CalcCastTime(caster)) : 0;
    return aura->GetDuration() > castMs + CC_RECAST_SLACK_MS;
}

void UpdateCcDuties(FactionChampionsState& state, Player* bot, std::vector<Player*> const& members, uint32 now)
{
    Map* map = bot->FindMap();
    std::vector<std::pair<Player*, std::vector<uint32>>> casters;
    std::vector<ObjectGuid> ccBots;
    for (Player* member : members)
    {
        std::vector<uint32> spells = CcSpells(member, map);
        if (spells.empty())
            continue;

        ccBots.push_back(member->GetGUID());
        casters.push_back({member, std::move(spells)});
    }

    std::sort(casters.begin(), casters.end(),
              [](auto const& a, auto const& b) { return a.first->GetGUID() < b.first->GetGUID(); });

    std::vector<Champion const*> const candidates = CcCandidates(state, FactionChampionsChainTarget(bot));
    auto const isCandidate = [&candidates](ObjectGuid guid)
    {
        return std::any_of(candidates.begin(), candidates.end(),
                           [guid](Champion const* champion) { return champion->guid == guid; });
    };

    auto const chased = [&state, bot](Player* member)
    {
        return std::any_of(state.champions.begin(), state.champions.end(),
                           [bot, member](Champion const& champion)
                           {
                               Creature* creature = champion.inCombat ? AliveCreature(bot, champion.guid) : nullptr;
                               return creature && FactionChampionsKitesFrom(member, creature) &&
                                      member->GetExactDist2d(creature) <= CC_CHASED_RADIUS;
                           });
    };

    std::vector<ObjectGuid> released;
    for (auto const& held : state.ccDuties)
    {
        ObjectGuid const ccBot = held.first;
        CcDuty const& duty = held.second;
        auto const caster = std::find_if(casters.begin(), casters.end(),
                                         [ccBot](auto const& entry) { return entry.first->GetGUID() == ccBot; });
        Creature* target = isCandidate(duty.target) ? AliveCreature(bot, duty.target) : nullptr;
        if (target && FactionChampionsChainGuarded(bot, target))
            target = nullptr;

        bool const casting = caster != casters.end() && CastingCcAt(caster->first, duty.target, duty.spellId);
        if (casting && !target)
            caster->first->InterruptNonMeleeSpells(false);

        bool keep = caster != casters.end() && target && InControl(caster->first) && !chased(caster->first);
        if (keep && !casting)
        {
            bool const landed = HoldsPastRecast(target->GetAura(duty.spellId, ccBot), caster->first, duty.spellId);
            keep = !landed && getMSTimeDiff(duty.sinceMs, now) < CC_DUTY_MS + (duty.walk ? CC_WALK_MS : 0);
        }

        if (!keep)
            released.push_back(ccBot);
    }

    for (ObjectGuid const& ccBot : released)
    {
        state.ccDuties.erase(ccBot);
        state.ccTargets.erase(ccBot);
    }

    if (getMSTimeDiff(state.liveSinceMs, now) < CC_SETTLE_MS)
        return;

    std::vector<bool> busy(casters.size(), false);
    for (size_t i = 0; i < casters.size(); ++i)
        busy[i] = state.ccDuties.count(casters[i].first->GetGUID()) > 0 || chased(casters[i].first);

    std::vector<std::vector<Champion const*>> tied(casters.size());
    for (size_t i = 0; i < casters.size(); ++i)
    {
        for (uint32 spellId : casters[i].second)
        {
            Champion const* holder = nullptr;
            for (Champion const& champion : state.champions)
            {
                Creature* creature = AliveCreature(bot, champion.guid);
                Aura const* aura = creature ? creature->GetAura(spellId, casters[i].first->GetGUID()) : nullptr;
                if (HoldsPastRecast(aura, casters[i].first, spellId))
                {
                    holder = &champion;
                    break;
                }
            }

            tied[i].push_back(holder);
        }
    }

    for (Champion const* champion : candidates)
    {
        if (std::all_of(busy.begin(), busy.end(), [](bool taken) { return taken; }))
            break;

        bool const covered = std::any_of(state.ccDuties.begin(), state.ccDuties.end(),
                                         [champion](auto const& held) { return held.second.target == champion->guid; });
        Creature* target = covered ? nullptr : AliveCreature(bot, champion->guid);
        if (!target || FactionChampionsChainGuarded(bot, target) || target->HasAura(SPELL_BLADESTORM))
            continue;

        int32 ccLeftMs = 0;
        if (PlayerCcAura(target, &ccLeftMs) && (ccLeftMs < 0 || ccLeftMs > CC_RECAST_LEAD_MS))
            continue;

        if (std::any_of(members.begin(), members.end(),
                        [champion](Player* member) { return CastingCcAt(member, champion->guid); }))
            continue;

        bool clearChecked = false;
        bool clear = false;
        std::vector<CcOption> options;
        for (size_t i = 0; i < casters.size(); ++i)
        {
            if (busy[i] || !InControl(casters[i].first))
                continue;

            Player* caster = casters[i].first;
            bool const standsForCyclone = CycloneStandUp(state, caster);
            for (size_t k = 0; k < casters[i].second.size(); ++k)
            {
                uint32 const spellId = casters[i].second[k];
                Champion const* holder = tied[i][k];
                if (holder && (holder->guid == champion->guid || !(champion->healer && !holder->healer)))
                    continue;

                SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
                if (!info || !CanCast(caster, spellId) || !info->CheckTargetCreatureType(target) ||
                    target->IsImmunedToSpell(info))
                    continue;

                float const range = info->GetMaxRange(false, caster);
                bool const walk = !caster->IsWithinCombatRange(target, range);
                if (walk && (standsForCyclone || !caster->IsWithinCombatRange(target, range + CC_WALK_REACH)))
                    continue;

                int32 const holdMs = NextCcDurationMs(target, info);
                if (holdMs >= 0 && holdMs <= CC_MIN_HOLD_MS)
                    continue;

                bool const breaks = BreaksOnDamage(info);
                if (breaks && !clearChecked)
                {
                    clear = ClearForBreakable(target, members, ccBots);
                    clearChecked = true;
                }

                if (breaks && !clear)
                    continue;

                options.push_back({i, spellId, breaks, holdMs, caster->GetExactDist(target), walk});
            }
        }

        std::sort(options.begin(), options.end(), BetterCcOption);
        for (CcOption const& option : options)
        {
            Player* caster = casters[option.caster].first;
            if (busy[option.caster] || (!option.walk && !caster->IsWithinLOSInMap(target)))
                continue;

            state.ccDuties[caster->GetGUID()] = {champion->guid, option.spellId, now, option.walk};
            state.ccTargets.Set(caster->GetGUID(), champion->guid);
            busy[option.caster] = true;
            break;
        }
    }
}

std::vector<Player*> GroupMembers(Player* bot)
{
    Map* map = bot->FindMap();
    std::vector<Player*> members;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsInWorld() && member->IsAlive() && member->FindMap() == map)
                members.push_back(member);
        }
    }

    return members;
}

bool IsEntryOf(KillOrderRow const& row, uint32 entry)
{
    return entry == static_cast<uint32>(row.alliance) || entry == static_cast<uint32>(row.horde);
}

bool KeepsIceDuty(Player* member)
{
    if (member->getClass() != CLASS_DEATH_KNIGHT || !member->HasSpell(SPELL_CHAINS_OF_ICE))
        return false;

    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    return memberAI && !FactionChampionsChainLink(memberAI);
}

void UpdateIce(FactionChampionsState& state, std::vector<Player*> const& members)
{
    ObjectGuid const kill = state.killTarget.Get();
    auto const memberOf = [&members](ObjectGuid guid) -> Player*
    {
        auto const found = std::find_if(members.begin(), members.end(),
                                        [guid](Player* member) { return member->GetGUID() == guid; });
        return found != members.end() ? *found : nullptr;
    };

    auto const taken = [&state](ObjectGuid guid)
    {
        return std::any_of(state.ice.begin(), state.ice.end(),
                           [guid](auto const& duty) { return duty.second == guid; });
    };

    auto const free = [&](Champion const& champion)
    {
        return champion.inCombat && champion.guid != kill && IsFactionChampionMelee(champion.entry) &&
               !taken(champion.guid);
    };

    std::vector<std::pair<ObjectGuid, char const*>> released;
    for (auto const& [dk, guid] : state.ice)
    {
        Player* member = memberOf(dk);
        PlayerbotAI* memberAI = member ? GET_PLAYERBOT_AI(member) : nullptr;
        auto const champion = std::find_if(state.champions.begin(), state.champions.end(),
                                           [guid = guid](Champion const& alive) { return alive.guid == guid; });
        char const* reason = nullptr;
        if (!memberAI)
            reason = "dk gone";
        else if (FactionChampionsChainLink(memberAI))
            reason = "chain link";
        else if (champion == state.champions.end())
            reason = "champion dead";
        else if (!champion->inCombat)
            reason = "out of combat";
        else if (guid == kill && std::any_of(state.champions.begin(), state.champions.end(), free))
            reason = "kill target";

        if (reason)
            released.emplace_back(dk, reason);
    }

    for (auto const& [dk, reason] : released)
    {
        state.ice.erase(dk);
        if (RaidObs::Active())
            RaidObs::NoteAssignment(dk, "fc.iceoff", reason);
    }

    auto const deal = [&](Champion const& champion)
    {
        Player* best = nullptr;
        float bestDist = 0.0f;
        for (Player* member : members)
        {
            if (state.ice.count(member->GetGUID()) || member->HasUnitState(UNIT_STATE_LOST_CONTROL) ||
                !KeepsIceDuty(member))
                continue;

            float const dist = Dist2d(member->GetPositionX(), member->GetPositionY(), champion.x, champion.y);
            if (!best || dist < bestDist)
            {
                best = member;
                bestDist = dist;
            }
        }

        if (best)
            state.ice.Set(best->GetGUID(), champion.guid);
    };

    for (KillOrderRow const& row : ICE_ORDER)
        for (Champion const& champion : state.champions)
            if (IsEntryOf(row, champion.entry) && free(champion))
                deal(champion);

    for (Champion const& champion : state.champions)
        if (champion.guid == kill && champion.inCombat && IsFactionChampionMelee(champion.entry) &&
            !taken(champion.guid))
            deal(champion);
}

FactionChampionsState* Refresh(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    FactionChampionsState* state = StateFor(bot);
    if (!state)
        return nullptr;

    uint32 const now = getMSTime();
    if (state->memoValid && state->memoMs == now)
        return state;

    state->memoMs = now;
    state->memoValid = true;
    bool const wasLive = state->live;
    state->live = ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions);
    if (!state->live)
    {
        EndPull(*state, bot);
        return state;
    }

    if (!wasLive)
    {
        state->liveSinceMs = now;
        state->packAnchor = GetFactionChampionsLandingCentre(bot);
        state->packCentre = state->packAnchor;
    }

    ScanChampions(*state, bot);
    UpdatePack(*state, now);
    UpdateCcAuras(*state);
    Unit* skull = GetRaidHumanSkull(botAI, &IsChampionUnit);
    state->skulled = skull ? skull->GetGUID() : ObjectGuid::Empty;
    UpdateKillLatch(*state, bot);
    std::vector<Player*> const members = GroupMembers(bot);
    UpdateIce(*state, members);
    UpdateDanger(*state);
    UpdateCcDuties(*state, bot, members, now);
    return state;
}

bool CounterspellStopsHeal(Unit* champion, int32& leftMs)
{
    for (CurrentSpellTypes type : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL})
    {
        Spell* spell = champion->GetCurrentSpell(type);
        if (!spell)
            continue;

        SpellInfo const* info = spell->GetSpellInfo();
        bool const casting = spell->getState() == SPELL_STATE_CASTING ||
                             (spell->getState() == SPELL_STATE_PREPARING && spell->GetCastTime() > 0);
        bool const interruptible = type == CURRENT_CHANNELED_SPELL
                                       ? (info->ChannelInterruptFlags & CHANNEL_INTERRUPT_FLAG_INTERRUPT)
                                       : (info->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT);

        if (info->IsPositive() && casting && interruptible && info->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
        {
            leftMs = type == CURRENT_GENERIC_SPELL ? spell->GetCastTimeRemaining() : 0;
            return true;
        }
    }

    return false;
}

bool CounterspellReady(Player* mage, Unit* target)
{
    return mage->HasSpell(SPELL_COUNTERSPELL) && !mage->HasSpellCooldown(SPELL_COUNTERSPELL) &&
           !mage->IsNonMeleeSpellCast(false) && !mage->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
           !mage->HasUnitFlag(UNIT_FLAG_SILENCED) && mage->IsWithinDistInMap(target, COUNTERSPELL_RANGE) &&
           !target->IsImmunedToSpell(sSpellMgr->GetSpellInfo(SPELL_COUNTERSPELL));
}

ObjectGuid CounterspellMage(Player* bot, Unit* target)
{
    Group* group = bot->GetGroup();
    if (!group)
        return CounterspellReady(bot, target) ? bot->GetGUID() : ObjectGuid::Empty;

    FactionChampionsState* state = StateFor(bot);
    if (!state)
        return ObjectGuid::Empty;

    return state->counterspellMage.Get(
        bot,
        [bot, group, target](ObjectGuid& chosen)
        {
            Map* map = bot->FindMap();
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || !member->IsInWorld() || member->FindMap() != map || !member->IsAlive() ||
                    member->getClass() != CLASS_MAGE || !GET_PLAYERBOT_AI(member) || !CounterspellReady(member, target))
                    continue;

                if (chosen.IsEmpty() || member->GetGUID() < chosen)
                    chosen = member->GetGUID();
            }
        },
        target->GetGUID());
}

}

bool IsFactionChampionHealer(uint32 entry)
{
    switch (entry)
    {
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_DRUID_RESTORATION):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_SHAMAN_RESTORATION):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PALADIN_HOLY):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PRIEST_DISCIPLINE):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_DRUID_RESTORATION):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_SHAMAN_RESTORATION):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PALADIN_HOLY):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PRIEST_DISCIPLINE):
            return true;
        default:
            return false;
    }
}

bool IsFactionChampionMelee(uint32 entry)
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

float FactionChampionsSwingReach(Unit* champion, Unit* victim)
{
    return champion->GetMeleeRange(victim) + LEEWAY_BONUS_RANGE;
}

bool IsFactionChampion(uint32 entry) { return KillRank(entry) != NOT_A_CHAMPION; }

Unit* FactionChampionsKillTarget(PlayerbotAI* botAI)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    FactionChampionsState* state = Refresh(botAI);
    return state && state->live ? AliveCreature(botAI->GetBot(), state->killTarget.Get()) : nullptr;
}

Unit* FactionChampionsSuspendedTarget(PlayerbotAI* botAI)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    FactionChampionsState* state = Refresh(botAI);
    return state && state->live ? AliveCreature(botAI->GetBot(), state->suspended) : nullptr;
}

Unit* FactionChampionsNextTarget(PlayerbotAI* botAI, std::function<bool(Unit*)> const& accept)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    FactionChampionsState* state = Refresh(botAI);
    if (!state || !state->live)
        return nullptr;

    ObjectGuid const killTarget = state->killTarget.Get();
    for (Champion const& champion : state->champions)
    {
        if (!champion.inCombat || champion.guid == killTarget || champion.guid == state->suspended)
            continue;

        Creature* creature = AliveCreature(botAI->GetBot(), champion.guid);
        if (creature && !FactionChampionsCcProtected(botAI, creature) && (!accept || accept(creature)))
            return creature;
    }

    return nullptr;
}

std::vector<Unit*> FactionChampionsInCombat(PlayerbotAI* botAI)
{
    std::vector<Unit*> out;
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return out;

    FactionChampionsState* state = Refresh(botAI);
    if (!state || !state->live)
        return out;

    for (Champion const& champion : state->champions)
        if (champion.inCombat)
            if (Creature* creature = AliveCreature(botAI->GetBot(), champion.guid))
                out.push_back(creature);

    return out;
}

bool FactionChampionsIsPackTank(Player* player)
{
    return player && GET_PLAYERBOT_AI(player) && PlayerbotAI::IsTank(player);
}

ObjectGuid FactionChampionsIceDutyOf(Player* member)
{
    FactionChampionsState* state = member ? StateFor(member) : nullptr;
    if (!state || !state->live)
        return ObjectGuid::Empty;

    auto const duty = state->ice.find(member->GetGUID());
    return duty != state->ice.end() ? duty->second : ObjectGuid::Empty;
}

Unit* FactionChampionsIceTarget(PlayerbotAI* botAI)
{
    if (!botAI || !ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    FactionChampionsState* state = Refresh(botAI);
    if (!state || !state->live)
        return nullptr;

    Player* bot = botAI->GetBot();
    auto const duty = state->ice.find(bot->GetGUID());
    return duty != state->ice.end() ? AliveCreature(bot, duty->second) : nullptr;
}

bool FactionChampionsHasIceDuty(PlayerbotAI* botAI) { return FactionChampionsIceTarget(botAI) != nullptr; }

bool FactionChampionsTrinketReady(Unit* champion)
{
    Creature* creature = champion ? champion->ToCreature() : nullptr;
    Map* map = creature ? creature->GetMap() : nullptr;
    return map && map->IsHeroic() && !creature->HasSpellCooldown(SPELL_PVP_TRINKET);
}

bool FactionChampionsFocusBot(PlayerbotAI* botAI)
{
    return botAI && !PlayerbotAI::IsHeal(botAI->GetBot()) && FactionChampionsIceDutyOf(botAI->GetBot()).IsEmpty();
}

bool FactionChampionsPack(PlayerbotAI* botAI, Position& anchor, Position& centre)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return false;

    FactionChampionsState* state = Refresh(botAI);
    if (!state || !state->live)
        return false;

    anchor = state->packAnchor;
    centre = state->packCentre;
    return true;
}

namespace
{
Champion const* LiveChampion(PlayerbotAI* botAI, Unit* unit)
{
    if (!unit || !ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    FactionChampionsState* state = Refresh(botAI);
    return state && state->live ? FindCandidate(*state, unit->GetGUID()) : nullptr;
}
}

bool FactionChampionsKillTargetOut(PlayerbotAI* botAI)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return false;

    FactionChampionsState* state = Refresh(botAI);
    return state && state->live && !state->outOnce.IsEmpty() && state->outOnce == state->killTarget.Get();
}

bool FactionChampionsIsPulled(PlayerbotAI* botAI, Unit* champion)
{
    Champion const* found = LiveChampion(botAI, champion);
    return found && found->pulled;
}

bool FactionChampionsIced(PlayerbotAI* botAI, Unit* champion)
{
    Champion const* found = LiveChampion(botAI, champion);
    return found && found->iced;
}

bool FactionChampionsIsEscapee(PlayerbotAI* botAI, Unit* champion)
{
    Champion const* found = LiveChampion(botAI, champion);
    return found && found->escapee;
}

bool FactionChampionsFearWindowActive(PlayerbotAI* botAI)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return false;

    FactionChampionsState* state = Refresh(botAI);
    return state && state->live &&
           std::any_of(state->champions.begin(), state->champions.end(),
                       [](Champion const& champion) { return CastsFear(champion.entry); });
}

Unit* FactionChampionsCcTarget(PlayerbotAI* botAI, uint32& spellId)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    FactionChampionsState* state = Refresh(botAI);
    if (!state || !state->live)
        return nullptr;

    Player* bot = botAI->GetBot();
    auto const duty = state->ccDuties.find(bot->GetGUID());
    if (duty == state->ccDuties.end())
        return nullptr;

    Creature* creature = AliveCreature(bot, duty->second.target);
    if (creature)
        spellId = duty->second.spellId;

    return creature;
}

bool FactionChampionsHasCcDuty(PlayerbotAI* botAI, ObjectGuid member)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return false;

    FactionChampionsState* state = Refresh(botAI);
    return state && state->live && state->ccDuties.count(member);
}

bool FactionChampionsCcProtected(PlayerbotAI* botAI, Unit* unit)
{
    if (!unit || !ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return false;

    FactionChampionsState* state = Refresh(botAI);
    if (!state || !state->live)
        return false;

    for (auto const& held : state->ccDuties)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(held.second.spellId);
        if (held.second.target == unit->GetGUID() && info && BreaksOnDamage(info))
            return true;
    }

    return FactionChampionsHeldByBreakableCc(unit) || FactionChampionsBreakablePeelAt(botAI, unit);
}

bool FactionChampionsHeldByBreakableCc(Unit* unit)
{
    SpellInfo const* aura = unit ? sSpellMgr->GetSpellInfo(PlayerCcAura(unit)) : nullptr;
    return aura && BreaksOnDamage(aura);
}

bool FactionChampionsBreaksOnDamage(uint32 spellId)
{
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return info && BreaksOnDamage(info);
}

bool FactionChampionsCarriesGroupDot(Unit* unit) { return unit && CarriesGroupDot(unit); }

bool FactionChampionsCounterspellDuty(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_MAGE)
        return false;

    Unit* killTarget = FactionChampionsKillTarget(botAI);
    int32 leftMs = 0;
    return killTarget && CounterspellStopsHeal(killTarget, leftMs) && leftMs <= LATE_INTERRUPT_MS &&
           CounterspellMage(bot, killTarget) == bot->GetGUID();
}

bool FactionChampionsCounterspellHold(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_MAGE)
        return false;

    Unit* killTarget = FactionChampionsKillTarget(botAI);
    int32 leftMs = 0;
    return killTarget && CounterspellStopsHeal(killTarget, leftMs) && leftMs > LATE_INTERRUPT_MS &&
           CounterspellMage(bot, killTarget) == bot->GetGUID();
}

bool FactionChampionsCycloneBot(Player* player) { return CycloneBot(player); }

Unit* FactionChampionsBlobHealer(PlayerbotAI* botAI)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    FactionChampionsState* state = Refresh(botAI);
    Champion const* healer = state && state->live ? BlobHealer(*state) : nullptr;
    return healer ? AliveCreature(botAI->GetBot(), healer->guid) : nullptr;
}

Unit* FactionChampionsDangerTarget(PlayerbotAI* botAI)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return nullptr;

    FactionChampionsState* state = Refresh(botAI);
    return state && state->live ? AliveCreature(botAI->GetBot(), state->danger.Get()) : nullptr;
}

}

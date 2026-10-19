#include "ToCHelpers_FactionChampions.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Creature.h"
#include "Group.h"
#include "Map.h"
#include "Pet.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCEncounterGate.h"
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

// Healers first, then the Warcraft Tavern Faction Champions guide's DPS danger table. CC goes the same way.
constexpr KillOrderRow KILL_ORDER[] = {
    {ToCFactionChampions::NPC_ALLIANCE_PALADIN_HOLY, ToCFactionChampions::NPC_HORDE_PALADIN_HOLY},
    {ToCFactionChampions::NPC_ALLIANCE_PRIEST_DISCIPLINE, ToCFactionChampions::NPC_HORDE_PRIEST_DISCIPLINE},
    {ToCFactionChampions::NPC_ALLIANCE_SHAMAN_RESTORATION, ToCFactionChampions::NPC_HORDE_SHAMAN_RESTORATION},
    {ToCFactionChampions::NPC_ALLIANCE_DRUID_RESTORATION, ToCFactionChampions::NPC_HORDE_DRUID_RESTORATION},
    {ToCFactionChampions::NPC_ALLIANCE_ROGUE, ToCFactionChampions::NPC_HORDE_ROGUE},
    {ToCFactionChampions::NPC_ALLIANCE_WARRIOR, ToCFactionChampions::NPC_HORDE_WARRIOR},
    {ToCFactionChampions::NPC_ALLIANCE_HUNTER, ToCFactionChampions::NPC_HORDE_HUNTER},
    {ToCFactionChampions::NPC_ALLIANCE_SHAMAN_ENHANCEMENT, ToCFactionChampions::NPC_HORDE_SHAMAN_ENHANCEMENT},
    {ToCFactionChampions::NPC_ALLIANCE_DEATH_KNIGHT, ToCFactionChampions::NPC_HORDE_DEATH_KNIGHT},
    {ToCFactionChampions::NPC_ALLIANCE_PALADIN_RETRIBUTION, ToCFactionChampions::NPC_HORDE_PALADIN_RETRIBUTION},
    {ToCFactionChampions::NPC_ALLIANCE_WARLOCK, ToCFactionChampions::NPC_HORDE_WARLOCK},
    {ToCFactionChampions::NPC_ALLIANCE_PRIEST_SHADOW, ToCFactionChampions::NPC_HORDE_PRIEST_SHADOW},
    {ToCFactionChampions::NPC_ALLIANCE_MAGE, ToCFactionChampions::NPC_HORDE_MAGE},
    {ToCFactionChampions::NPC_ALLIANCE_DRUID_BALANCE, ToCFactionChampions::NPC_HORDE_DRUID_BALANCE},
};

constexpr uint8 NOT_A_CHAMPION = static_cast<uint8>(std::size(KILL_ORDER));

// Same radius as the encounter gate, which sees the whole arena from anywhere on its floor
constexpr float CHAMPION_SEARCH_RADIUS = 200.0f;
constexpr float COUNTERSPELL_RANGE = 30.0f;
// Cyclone's third application, 1.5 s, holds no longer than its own cast
constexpr int32 CC_MIN_HOLD_MS = 2500;
// A CC this close to running out gets the next one: a 1.5 s cast plus slack
constexpr int32 CC_RECAST_LEAD_MS = 2000;
constexpr uint32 CC_DUTY_MS = 3000;
// a human's opening grip lands and moves the kill latch in this window
constexpr uint32 CC_SETTLE_MS = 1500;
// Consecration and Death and Decay round a pack tank break Polymorph and Fear
constexpr float CC_AOE_CLEAR = 12.0f;

// Highest rank first
constexpr std::array<uint32, 4> POLYMORPH_RANKS = {12826, 12825, 12824, 118};
constexpr std::array<uint32, 3> FEAR_RANKS = {6215, 6213, 5782};
// Elementals and demons only, the Resto Druid champion is an elemental
constexpr std::array<uint32, 2> BANISH_RANKS = {18647, 710};

// The pack: champions this close to where they landed, the kill target left out
constexpr float PACK_ANCHOR_RADIUS = 25.0f;
// A healer this far off the pack, with nobody next to it, has been pulled out
constexpr float PULLED_FROM_PACK = 20.0f;
constexpr float PULLED_CLEAR = 12.0f;
// Below this the current kill target is nearly dead, so a pulled healer waits its turn
constexpr float PULLED_SWITCH_MIN_PCT = 30.0f;
// Off the landing spot by this much and not on a tank: chasing the raid
constexpr float ESCAPE_DIST = 20.0f;
constexpr uint32 SPELL_DEATH_GRIP_RANK = 49576;

struct Champion
{
    ObjectGuid guid;
    uint32 entry = 0;
    uint8 rank = NOT_A_CHAMPION;
    bool inCombat = false;
    bool touchable = false;
    float healthPct = 0.0f;
    uint32 ccAura = 0;  // a player's fear, stun or confuse on it
    float x = 0.0f;
    float y = 0.0f;
    bool healer = false;
    bool pulled = false;
    bool escapee = false;
    bool onFront = false;
    float packDist = 0.0f;
};

struct CcDuty
{
    ObjectGuid target;
    uint32 spellId = 0;
    uint32 sinceMs = 0;
};

struct FactionChampionsState
{
    RaidObs::ObsValue<ObjectGuid> killTarget{"fc.kill"};
    RaidObs::ObsGuidMap<ObjectGuid> ccTargets{"fc.cc"};
    RaidObs::ObsGuidMap<uint32> ccAuras{"fc.ccaura"};

    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value
    bool live = false;
    uint32 liveSinceMs = 0;
    Position packAnchor;
    Position packCentre;
    std::vector<Champion> champions;  // alive, in kill order
    ObjectGuid suspended;
    // A player's skull on a champion
    ObjectGuid skulled;
    // CC bot -> its cast, mirrored into fc.cc
    std::unordered_map<ObjectGuid, CcDuty> ccDuties;
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

// Fear 65809, Psychic Scream 65543 (disc and shadow), Intimidating Shout 65930
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

// Null off a ToC instance.
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

// Erased by guid, so the trace hears each key go
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

    if (!state.killTarget.Get().IsEmpty())
    {
        RaidObs::Note(bot, "fc.switch", "reset");
        state.killTarget = ObjectGuid::Empty;
    }

    EraseAll(state.ccTargets);
    EraseAll(state.ccAuras);
    state.ccDuties.clear();
}

// The spell id of a player's fear, stun or confuse on it, and the longest time one has left. Negative is
// permanent.
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

// Any hit ends a take-damage aura. A CC with proc flags and no trigger spell gets a damage cap in
// AuraEffect::CalculateAmount and breaks past it: Fear and Entangling Roots, DoT ticks included.
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

// What Unit::ApplyDiminishingToDuration gives a player's cast at the level the next hit reads. Champions
// carry ALL_DIMINISH, so they diminish like players. Negative is permanent.
int32 NextCcDurationMs(Unit* target, SpellInfo const* info)
{
    int32 duration = info->GetMaxDuration();
    DiminishingGroup const group = GetDiminishingReturnsGroupForSpell(info, false);
    if (duration < 0 || group == DIMINISHING_NONE)
        return duration;

    // The cap reads the owner's ALL_DIMINISH, the scaling the target's own
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

// The CC spells this bot knows, or none when it isn't a CC bot: a dps mage, warlock or ranged druid
std::vector<uint32> CcSpells(Player* member, Map* map)
{
    std::vector<uint32> spells;
    if (!member || !member->IsInWorld() || !member->IsAlive() || member->FindMap() != map ||
        !GET_PLAYERBOT_AI(member) || !IsToCDpsBot(member))
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
            // a melee druid would leave cat form for it
            if (!PlayerbotAI::IsMelee(member) && member->HasSpell(SPELL_CYCLONE))
                add(SPELL_CYCLONE);
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

// A periodic damage aura from a player, a pet, a totem or a guardian
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
    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, ChampionEntries(), CHAMPION_SEARCH_RADIUS);

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
        // Divine Shield, Ice Block or Cyclone, not Hand of Protection (physical only). Asked per half since
        // Ice Block is a physical aura plus a magic one, and IsImmunedToDamage wants one aura per mask
        champion.touchable = !(creature->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL) &&
                               creature->IsImmunedToDamage(SPELL_SCHOOL_MASK_MAGIC));
        champion.healthPct = creature->GetHealthPct();
        champion.ccAura = PlayerCcAura(creature);
        champion.x = creature->GetPositionX();
        champion.y = creature->GetPositionY();
        champion.healer = IsFactionChampionHealer(champion.entry);
        Unit* victim = creature->GetVictim();
        Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
        champion.onFront =
            victimPlayer && (PlayerbotAI::IsTank(victimPlayer) || FactionChampionsIsFrontline(victimPlayer));
        champion.escapee = creature->GetExactDist2d(&state.packAnchor) > ESCAPE_DIST && !champion.onFront;
        state.champions.push_back(champion);
    }

    std::sort(state.champions.begin(), state.champions.end(),
              [](Champion const& a, Champion const& b)
              { return a.rank != b.rank ? a.rank < b.rank : a.guid < b.guid; });
}

float Dist2d(float ax, float ay, float bx, float by) { return std::hypot(ax - bx, ay - by); }

// Off the kill target as latched last tick, since the latch reads what this writes
void UpdatePack(FactionChampionsState& state)
{
    ObjectGuid const killTarget = state.killTarget.Get();
    float x = 0.0f;
    float y = 0.0f;
    uint32 count = 0;
    for (bool nearOnly : {true, false})
    {
        for (Champion const& champion : state.champions)
        {
            if (!champion.inCombat || champion.guid == killTarget ||
                (nearOnly && state.packAnchor.GetExactDist2d(champion.x, champion.y) > PACK_ANCHOR_RADIUS))
                continue;

            x += champion.x;
            y += champion.y;
            ++count;
        }

        if (count)
            break;
    }

    if (count)
        state.packCentre.Relocate(x / count, y / count, state.packAnchor.GetPositionZ());
    else
        state.packCentre = state.packAnchor;

    for (Champion& champion : state.champions)
    {
        champion.packDist = state.packCentre.GetExactDist2d(champion.x, champion.y);
        champion.pulled = false;
        if (!champion.healer || !champion.inCombat || champion.packDist < PULLED_FROM_PACK)
            continue;

        champion.pulled = std::none_of(state.champions.begin(), state.champions.end(),
                                       [&champion](Champion const& other)
                                       {
                                           return other.guid != champion.guid &&
                                                  Dist2d(other.x, other.y, champion.x, champion.y) < PULLED_CLEAR;
                                       });
    }
}

// A creature's auras reach no other trace stream
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

// Farthest off the pack, kill order breaking ties
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

    // Checked before the return to a suspended one, which would otherwise take the latch off the skull
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
        if (suspended && suspended->touchable && suspended->healthPct <= current->healthPct)
        {
            next = suspended->guid;
            reason = "back";
            state.suspended.Clear();
        }
        // a healer dragged out of the pack gets no heals from the others, so it goes first. Once the
        // latch sits on a pulled one it stays there.
        else if (!current->pulled && current->healthPct > PULLED_SWITCH_MIN_PCT)
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

        // With nobody else touchable there's nothing better to hit, so it stays
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
        }
    }

    if (reason)
        RaidObs::Note(bot, "fc.switch", reason);

    state.killTarget = next;
}

// Nothing of ours would break a Polymorph or Fear on it: no pack tank's AoE, nobody but a CC bot on it,
// no DoT
bool ClearForBreakable(Unit* target, std::vector<Player*> const& members, std::vector<ObjectGuid> const& ccBots)
{
    for (Player* member : members)
    {
        if (FactionChampionsIsPackTank(member) && member->GetExactDist2d(target) < CC_AOE_CLEAR)
            return false;

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

// The healer the grip DK pulls next: first in kill order still in the pack and open to a grip
ObjectGuid NextGripHealer(FactionChampionsState const& state)
{
    for (Champion const& champion : state.champions)
        if (champion.healer && champion.inCombat && champion.touchable && !champion.pulled &&
            champion.guid != state.killTarget.Get() && champion.guid != state.suspended)
            return champion.guid;

    return ObjectGuid::Empty;
}

// Healers off the tanks nearest the kill target first, then the other healers, then champions off the
// tanks casters first, then the rest in kill order. Not the kill target, nor the suspended one: the latch
// goes back to it and CC would break or block that. Nor the skulled one, even while it's immune and so
// not the latch. Nor a pulled healer, the latch's next pick, nor the player's pull or the likely next one:
// CC would make each immune to the grip when its turn comes.
std::vector<Champion const*> CcCandidates(FactionChampionsState const& state, ObjectGuid chainTarget)
{
    ObjectGuid const killGuid = state.killTarget.Get();
    Champion const* kill = nullptr;
    for (Champion const& champion : state.champions)
        if (champion.guid == killGuid)
            kill = &champion;

    ObjectGuid const nextGrip = kill && kill->pulled && kill->healthPct <= NEXT_GRIP_WALK_PCT
                                    ? NextGripHealer(state)
                                    : ObjectGuid::Empty;
    std::vector<Champion const*> candidates;
    for (Champion const& champion : state.champions)
        if (champion.inCombat && !champion.pulled && champion.guid != killGuid && champion.guid != state.suspended &&
            champion.guid != state.skulled && champion.guid != nextGrip && champion.guid != chainTarget)
            candidates.push_back(&champion);

    auto const group = [](Champion const* champion)
    {
        if (champion->healer)
            return champion->onFront ? 1 : 0;

        if (champion->onFront)
            return 4;

        return IsFactionChampionMelee(champion->entry) ? 3 : 2;
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
};

// Never-breaking first, then the longest hold, then the nearest caster
bool BetterCcOption(CcOption const& a, CcOption const& b)
{
    if (a.breaks != b.breaks)
        return !a.breaks;

    if (a.holdMs != b.holdMs)
        return a.holdMs < 0 || (b.holdMs >= 0 && a.holdMs > b.holdMs);

    return a.distance < b.distance;
}

// Latched until the cast lands or 3 s pass with none going, so each bot finishes the cast it started
void UpdateCcDuties(FactionChampionsState& state, Player* bot, uint32 now)
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

    std::vector<ObjectGuid> released;
    for (auto const& held : state.ccDuties)
    {
        ObjectGuid const ccBot = held.first;
        CcDuty const& duty = held.second;
        auto const caster = std::find_if(casters.begin(), casters.end(),
                                         [ccBot](auto const& entry) { return entry.first->GetGUID() == ccBot; });
        Creature* target = isCandidate(duty.target) ? AliveCreature(bot, duty.target) : nullptr;
        bool const casting = caster != casters.end() && CastingCcAt(caster->first, duty.target, duty.spellId);
        if (casting && !target)
            caster->first->InterruptNonMeleeSpells(false);

        bool keep = caster != casters.end() && target && InControl(caster->first);
        if (keep && !casting)
        {
            Aura const* landed = target->GetAura(duty.spellId, ccBot);
            keep = !landed && getMSTimeDiff(duty.sinceMs, now) < CC_DUTY_MS;
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
        busy[i] = state.ccDuties.count(casters[i].first->GetGUID()) > 0;

    for (Champion const* champion : candidates)
    {
        if (std::all_of(busy.begin(), busy.end(), [](bool taken) { return taken; }))
            break;

        bool const covered = std::any_of(state.ccDuties.begin(), state.ccDuties.end(),
                                         [champion](auto const& held) { return held.second.target == champion->guid; });
        Creature* target = covered ? nullptr : AliveCreature(bot, champion->guid);
        if (!target)
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
            for (uint32 spellId : casters[i].second)
            {
                SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
                if (!info || !CanCast(caster, spellId) || !info->CheckTargetCreatureType(target) ||
                    target->IsImmunedToSpell(info) ||
                    !caster->IsWithinCombatRange(target, info->GetMaxRange(false, caster)))
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

                options.push_back({i, spellId, breaks, holdMs, caster->GetExactDist(target)});
            }
        }

        std::sort(options.begin(), options.end(), BetterCcOption);
        for (CcOption const& option : options)
        {
            Player* caster = casters[option.caster].first;
            if (busy[option.caster] || !caster->IsWithinLOSInMap(target))
                continue;

            state.ccDuties[caster->GetGUID()] = {champion->guid, option.spellId, now};
            state.ccTargets.Set(caster->GetGUID(), champion->guid);
            busy[option.caster] = true;
            break;
        }
    }
}

FactionChampionsState* Refresh(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    FactionChampionsState* state = StateFor(bot);
    if (!state)
        return nullptr;

    // Every node and multiplier asks, and a refresh walks a 200 yd grid
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
    }

    ScanChampions(*state, bot);
    UpdatePack(*state);
    UpdateCcAuras(*state);
    Unit* skull = GetToCHumanSkull(botAI, &IsChampionUnit);
    state->skulled = skull ? skull->GetGUID() : ObjectGuid::Empty;
    UpdateKillLatch(*state, bot);
    UpdateCcDuties(*state, bot, now);
    return state;
}

// Counterspell only interrupts a spell passing these checks (Spell::EffectInterruptCast), so the
// cooldown only goes on a heal it stops
bool CounterspellStopsHeal(Unit* champion)
{
    for (CurrentSpellTypes type : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL})
    {
        Spell const* spell = champion->GetCurrentSpell(type);
        if (!spell)
            continue;

        SpellInfo const* info = spell->GetSpellInfo();
        bool const casting = spell->getState() == SPELL_STATE_CASTING ||
                             (spell->getState() == SPELL_STATE_PREPARING && spell->GetCastTime() > 0);
        bool const interruptible = type == CURRENT_CHANNELED_SPELL
                                       ? (info->ChannelInterruptFlags & CHANNEL_INTERRUPT_FLAG_INTERRUPT)
                                       : (info->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT);

        if (info->IsPositive() && casting && interruptible && info->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
            return true;
    }

    return false;
}

// No pacify check, only silence stops Counterspell (Hex sets both). A school lockout shows up as
// the spell's own cooldown.
bool CounterspellReady(Player* mage, Unit* target)
{
    return mage->HasSpell(SPELL_COUNTERSPELL) && !mage->HasSpellCooldown(SPELL_COUNTERSPELL) &&
           !mage->IsNonMeleeSpellCast(false) && !mage->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
           !mage->HasUnitFlag(UNIT_FLAG_SILENCED) && mage->IsWithinDistInMap(target, COUNTERSPELL_RANGE) &&
           !target->IsImmunedToSpell(sSpellMgr->GetSpellInfo(SPELL_COUNTERSPELL));
}

// Lowest guid among the ready mage bots, so one Counterspell answers each heal
Player* CounterspellMage(Player* bot, Unit* target)
{
    Group* group = bot->GetGroup();
    if (!group)
        return CounterspellReady(bot, target) ? bot : nullptr;

    Map* map = bot->FindMap();
    Player* chosen = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsInWorld() || member->FindMap() != map || !member->IsAlive() ||
            member->getClass() != CLASS_MAGE || !GET_PLAYERBOT_AI(member) || !CounterspellReady(member, target))
            continue;

        if (!chosen || member->GetGUID() < chosen->GetGUID())
            chosen = member;
    }

    return chosen;
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

bool IsFactionChampion(uint32 entry) { return KillRank(entry) != NOT_A_CHAMPION; }

// These two check live first: after the kill only multipliers still ask, and a refresh from them
// would write the reset into the next encounter's trace.
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

uint32 FactionChampionsPetTauntAutocast(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Pet* pet = bot->GetPet();
    Map* map = bot->FindMap();
    if (!pet || !pet->IsAlive() || !map || !map->IsHeroic() ||
        !ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 0;

    for (uint8 pos = 0; pos < pet->GetPetAutoSpellSize(); ++pos)
    {
        uint32 const spellId = pet->GetPetAutoSpellOnPos(pos);
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (info && (info->HasAura(SPELL_AURA_MOD_TAUNT) || info->HasEffect(SPELL_EFFECT_ATTACK_ME) ||
                     info->HasEffect(SPELL_EFFECT_THREAT)))
            return spellId;
    }

    return 0;
}

bool FactionChampionsIsPackTank(Player* player)
{
    return player && GET_PLAYERBOT_AI(player) && PlayerbotAI::IsTank(player);
}

ObjectGuid FactionChampionsChainDk(Player* bot)
{
    ObjectGuid chosen;
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group)
        return chosen;

    Map* map = bot->FindMap();
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* player = ref->GetSource();
        if (player && player->IsInWorld() && player->IsAlive() && player->FindMap() == map &&
            player->getClass() == CLASS_DEATH_KNIGHT && GET_PLAYERBOT_AI(player) &&
            !FactionChampionsIsPackTank(player) && player->HasSpell(SPELL_DEATH_GRIP_RANK) &&
            (chosen.IsEmpty() || player->GetGUID() < chosen))
            chosen = player->GetGUID();
    }

    return chosen;
}

bool FactionChampionsIsFrontline(Player* player)
{
    if (!player || !GET_PLAYERBOT_AI(player))
        return false;

    if (FactionChampionsIsPackTank(player))
        return true;

    if (!PlayerbotAI::IsMelee(player) || PlayerbotAI::IsHeal(player))
        return false;

    switch (player->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
            return true;
        case CLASS_DEATH_KNIGHT:
            return player->GetGUID() != FactionChampionsChainDk(player);
        default:
            return false;
    }
}

bool FactionChampionsFocusBot(PlayerbotAI* botAI)
{
    return botAI && !PlayerbotAI::IsHeal(botAI->GetBot()) && !FactionChampionsIsFrontline(botAI->GetBot());
}

bool FactionChampionsHealerLeft(PlayerbotAI* botAI)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return false;

    FactionChampionsState* state = Refresh(botAI);
    return state && state->live &&
           std::any_of(state->champions.begin(), state->champions.end(),
                       [](Champion const& champion) { return champion.healer; });
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
}  // namespace

bool FactionChampionsIsPulled(PlayerbotAI* botAI, Unit* champion)
{
    Champion const* found = LiveChampion(botAI, champion);
    return found && found->pulled;
}

bool FactionChampionsIsEscapee(PlayerbotAI* botAI, Unit* champion)
{
    Champion const* found = LiveChampion(botAI, champion);
    return found && found->escapee;
}

uint32 FactionChampionsLiveMs(PlayerbotAI* botAI)
{
    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 0;

    FactionChampionsState* state = Refresh(botAI);
    return state && state->live ? getMSTimeDiff(state->liveSinceMs, getMSTime()) : 0;
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

    SpellInfo const* aura = sSpellMgr->GetSpellInfo(PlayerCcAura(unit));
    return aura && BreaksOnDamage(aura);
}

bool FactionChampionsCounterspellDuty(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_MAGE)
        return false;

    Unit* killTarget = FactionChampionsKillTarget(botAI);
    return killTarget && CounterspellStopsHeal(killTarget) && CounterspellMage(bot, killTarget) == bot;
}

}

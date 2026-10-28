/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Festergut.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Shared.h"
#include "ICCScripts.h"
#include "ICCSeams.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDifficulty.h"
#include "RaidDirectTargeting.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidResistAura.h"
#include "RaidTankDefensive.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

namespace
{
// His room is a 45 yd circle round his spot
constexpr float ROOM_RADIUS = 60.0f;
constexpr uint32 ROSTER_REFRESH_MS = 1000;
constexpr uint32 DEAL_REFRESH_MS = 200;
constexpr uint32 HUMAN_CALL_CHECK_MS = 500;
constexpr float TAUNT_RANGE = 30.0f;
// Vile Gas picks among players past his melee by 5 yd once this many stand there, else among everyone
constexpr size_t VILE_GAS_MIN_TARGETS_10 = 3;
constexpr size_t VILE_GAS_MIN_TARGETS_25 = 8;
constexpr float DODGE_SWEEP_RADIUS = 16.0f;
// Slots sit 18-28.5 yd off his spot against a 38.5 yd reach spell, so they hold until he is this far off it
constexpr float SLOT_DRIFT = 10.0f;
// A dodging ranged bot keeps this far from everyone else, so one Vile Gas still takes one player
constexpr float SPREAD_KEEPOUT = FG_VILE_GAS_RADIUS + 0.5f;
// Inoculated takes 25% a stack off his shadow area damage, three at most
constexpr float INOCULATED_CUT = 0.25f;
constexpr uint32 INOCULATED_MAX = 3;

// The middle slots first: from those every slot and tank spot is in heal range
constexpr uint8 HEALER_SLOTS[] = {1, 2, 3, 8, 9, 10};
constexpr uint8 RANGED_SLOTS[] = {4, 5, 11, 12, 13, 14, 6, 7, 15, 16, 17, 18, 2, 3, 9, 10, 1, 8};
// Melee fill these when too few ranged stand out for Vile Gas, nearest the legs first
constexpr uint8 FILL_SLOTS[] = {2, 3, 1, 4, 5, 6, 7, 9, 10, 8, 11, 12, 13, 14, 15, 16, 17, 18};

// Stored in fg.carrier: the group a spore carrier was sent to
constexpr uint8 GROUP_CLOSE = 1;
constexpr uint8 GROUP_FAR = 2;

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

struct FgState
{
    uint32 rosterMs = 0;
    bool rosterBuilt = false;
    RaidObs::ObsGuidMap<uint8> slot{"fg.slot"};

    uint32 dealMs = 0;
    bool dealRan = false;
    // The wave's carriers by guid, to tell a new wave from the one already dealt
    std::vector<ObjectGuid> wave;
    RaidObs::ObsGuidMap<uint8> carrier{"fg.carrier"};
    bool closeCarried = false;
    bool farCarried = false;
    // A group with no bot carrier stands round this player instead of its anchor
    ObjectGuid closeOn;
    ObjectGuid farOn;
    RaidObs::ObsValue<uint8> leg{"fg.leg", 1};
    RaidObs::ObsValue<uint8> far{"fg.sporespot", 1};
    RaidObs::ObsGuidSet brake{"fg.brake"};

    // Pungent Blight takes his Inhaled Blight stacks: each drop starts a cycle
    uint32 blightLast = 0;
    uint32 blightCycle = 0;
    std::unordered_map<ObjectGuid, uint32> minorSpent;

    uint32 humanCallMs = 0;
    bool humanCallRan = false;
    // The cycle of the last minor call plus one, 0 for none
    uint32 humanMinorCall = 0;

    std::unordered_map<ObjectGuid, Stand> stands;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
};

RaidInstanceState<FgState> fgStates;

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

// A bot of its own, not a player, selfbot included: players hold no duty.
bool IsDutyBot(Player* player) { return GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

// Tanks and melee stand with him, healers and ranged out
bool IsCloseRole(Player* player)
{
    return IsTankPlayer(player) || (!PlayerbotAI::IsHeal(player) && !PlayerbotAI::IsRanged(player));
}

bool IsFgUnit(Unit* unit) { return unit && unit->GetEntry() == NPC_FG_FESTERGUT; }

void Note(Player* bot, char const* key, char const* value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

uint32 Stacks(Unit* unit, uint32 spellId)
{
    Aura const* aura = unit->GetAura(spellId);
    return aura ? aura->GetStackAmount() : 0;
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

// In combat on the bot's map, whoever the bot is and wherever it stands
Creature* BossInCombat(Player* bot)
{
    InstanceScript* instance = bot->GetMapId() == ICC_MAP_ID ? bot->GetInstanceScript() : nullptr;
    Creature* boss = instance ? ObjectAccessor::GetCreature(*bot, instance->GetGuidData(ICC_BOSS_FESTERGUT)) : nullptr;
    return boss && boss->IsAlive() && boss->IsInCombat() ? boss : nullptr;
}

FgState* LiveState(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    return bot && bot->GetInstanceId() && GetFgBoss(botAI) ? fgStates.Find(bot->GetInstanceId()) : nullptr;
}

bool ClearOf(Position const& spot, std::vector<Position> const& impacts, float radius)
{
    return std::none_of(impacts.begin(), impacts.end(),
                        [&spot, radius](Position const& impact) { return impact.GetExactDist2d(spot) < radius; });
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

uint32 TauntSpellId(Player* player)
{
    switch (player->getClass())
    {
        case CLASS_PALADIN:
            return SPELL_TAUNT_PALADIN;
        case CLASS_DEATH_KNIGHT:
            return SPELL_TAUNT_DK;
        case CLASS_DRUID:
            return SPELL_TAUNT_DRUID;
        case CLASS_WARRIOR:
            return SPELL_TAUNT_WARRIOR;
        default:
            return 0;
    }
}

// Off cooldown and castable in its current form: Growl needs bear form and Taunt Defensive Stance, and a
// tank elected without them would hold the swap without ever landing it
bool TauntUsable(Player* member)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    char const* taunt = memberAI ? TauntName(member) : nullptr;
    uint32 const spellId = taunt ? memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", taunt)->Get() : 0;
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return info && !member->HasSpellCooldown(spellId) &&
           info->CheckShapeshift(member->GetShapeshiftForm()) == SPELL_CAST_OK;
}

uint32 BloatId(Creature* boss) { return sSpellMgr->GetSpellIdForDifficulty(SPELL_FG_GASTRIC_BLOAT, boss); }

uint32 BlightStacks(Creature* boss)
{
    return Stacks(boss, sSpellMgr->GetSpellIdForDifficulty(SPELL_FG_INHALED_BLIGHT, boss));
}

// The tank to take him now. A taunt aura on him means one just landed and his victim catches up next
// update, so nobody else spends one.
Player* SwapTaunter(Player* bot, Creature* boss)
{
    Unit* victim = boss->GetVictim();
    if (!victim || boss->HasAuraType(SPELL_AURA_MOD_TAUNT))
        return nullptr;

    uint32 const bloat = BloatId(boss);
    Player* holder = victim->ToPlayer();
    if (holder && IsTankPlayer(holder) && Stacks(holder, bloat) < FG_SWAP_STACKS)
        return nullptr;

    Player* pick = nullptr;
    uint32 pickStacks = 0;
    for (Player* member : MembersOnMap(bot))
    {
        if (member == victim || !IsDutyBot(member) || !IsTankPlayer(member) ||
            member->GetExactDist2d(boss) > TAUNT_RANGE || !TauntUsable(member))
            continue;

        uint32 const stacks = Stacks(member, bloat);
        if (stacks > FG_TAUNTER_MAX_STACKS)
            continue;

        if (!pick || stacks < pickStacks || (stacks == pickStacks && member->GetGUID() < pick->GetGUID()))
        {
            pick = member;
            pickStacks = stacks;
        }
    }

    return pick;
}

void RefreshRoster(FgState& state, Player* bot, uint32 now)
{
    if (state.rosterBuilt && getMSTimeDiff(state.rosterMs, now) < ROSTER_REFRESH_MS)
        return;

    state.rosterBuilt = true;
    state.rosterMs = now;

    std::vector<Player*> tanks;
    std::vector<Player*> healers;
    std::vector<Player*> ranged;
    std::vector<Player*> melee;
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
        else
            melee.push_back(member);
    }

    std::sort(healers.begin(), healers.end(), IccGuidLess());
    std::sort(ranged.begin(), ranged.end(), IccGuidLess());
    std::sort(melee.begin(), melee.end(), IccGuidLess());
    std::sort(tanks.begin(), tanks.end(),
              [](Player* a, Player* b)
              {
                  bool const mainA = PlayerbotAI::IsMainTank(a);
                  bool const mainB = PlayerbotAI::IsMainTank(b);
                  return mainA != mainB ? mainA : a->GetGUID() < b->GetGUID();
              });

    std::unordered_map<ObjectGuid, uint8> wanted;
    for (size_t i = 0; i < tanks.size(); ++i)
        wanted[tanks[i]->GetGUID()] = i < FG_TANK_SPOTS ? static_cast<uint8>(FG_STAND_TANK + i) : FG_STAND_LEG;
    for (Player* member : melee)
        wanted[member->GetGUID()] = FG_STAND_LEG;

    bool taken[FG_SLOT_COUNT + 1] = {};
    auto const keep = [&](Player* member)
    {
        auto const held = state.slot.find(member->GetGUID());
        if (held == state.slot.end())
            return false;

        uint8 const id = held->second;
        if (id < 1 || id > FG_SLOT_COUNT || taken[id])
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
        if (!keep(member))
            openHealers.push_back(member);
    for (Player* member : ranged)
        if (!keep(member))
            openRanged.push_back(member);

    std::vector<Player*> spareHealers;
    for (Player* member : openHealers)
        if (!take(HEALER_SLOTS, std::size(HEALER_SLOTS), member))
            spareHealers.push_back(member);
    for (Player* member : openRanged)
        take(RANGED_SLOTS, std::size(RANGED_SLOTS), member);
    for (Player* member : spareHealers)
        take(RANGED_SLOTS, std::size(RANGED_SLOTS), member);

    // Too few out and Vile Gas picks among everyone, so one on the melee would take the tanks too
    size_t slotted = 0;
    for (uint8 id = 1; id <= FG_SLOT_COUNT; ++id)
        slotted += taken[id] ? 1 : 0;
    size_t const needed = IsRaid25Man(bot) ? VILE_GAS_MIN_TARGETS_25 : VILE_GAS_MIN_TARGETS_10;
    if (slotted < needed)
    {
        size_t fill = needed - slotted;
        for (Player* member : melee)
            if (fill && keep(member))
                --fill;
        for (Player* member : melee)
        {
            auto const held = wanted.find(member->GetGUID());
            if (fill && held != wanted.end() && held->second == FG_STAND_LEG &&
                take(FILL_SLOTS, std::size(FILL_SLOTS), member))
                --fill;
        }
    }

    std::vector<ObjectGuid> departed;
    for (auto const& [guid, id] : state.slot.Raw())
        if (!wanted.count(guid))
            departed.push_back(guid);
    for (ObjectGuid const& guid : departed)
        state.slot.erase(guid);
    for (auto const& [guid, id] : wanted)
        state.slot.Set(guid, id);
}

Position const& ActiveFarAnchor(FgState const& state) { return ICC_FG_FAR_ANCHORS[state.far.Get() == 2 ? 1 : 0]; }

// The legs and the far anchors flip once goo lands on the one in use and the other is clear. Assigned every
// pass, so each trace hears the value it opens on.
void UpdateFlips(FgState& state, std::vector<Position> const& impacts)
{
    float const legClear = FG_GOO_CLEARANCE + FG_STAND_PARK;
    uint8 leg = state.leg.Get() == 2 ? 2 : 1;
    if (!ClearOf(ICC_FG_LEGS[leg - 1], impacts, legClear) && ClearOf(ICC_FG_LEGS[2 - leg], impacts, legClear))
        leg = static_cast<uint8>(3 - leg);
    state.leg = leg;

    float const farClear = FG_GOO_CLEARANCE + FG_SPORE_RING + FG_STAND_PARK;
    uint8 far = state.far.Get() == 2 ? 2 : 1;
    if (!ClearOf(ICC_FG_FAR_ANCHORS[far - 1], impacts, farClear) &&
        ClearOf(ICC_FG_FAR_ANCHORS[2 - far], impacts, farClear))
        far = static_cast<uint8>(3 - far);
    state.far = far;
}

void ClearDeal(FgState& state)
{
    state.wave.clear();
    std::vector<ObjectGuid> released;
    for (auto const& [guid, group] : state.carrier.Raw())
        released.push_back(guid);
    for (ObjectGuid const& guid : released)
        state.carrier.erase(guid);
    state.closeCarried = false;
    state.farCarried = false;
    state.closeOn.Clear();
    state.farOn.Clear();
}

// Picks the carrier sent across to an empty group: a bot not holding him, nearest that group's anchor
Player* SpareCarrier(std::vector<Player*> const& carriers, Unit* holder, Position const& to, bool melee)
{
    Player* pick = nullptr;
    float pickDistance = 0.0f;
    for (Player* member : carriers)
    {
        if (!IsDutyBot(member) || member == holder || (melee && IsTankPlayer(member)))
            continue;

        float const distance = member->GetExactDist2d(to);
        if (!pick || distance < pickDistance || (distance == pickDistance && member->GetGUID() < pick->GetGUID()))
        {
            pick = member;
            pickDistance = distance;
        }
    }

    return pick;
}

// A group with a bot carrier stands on its anchor, which that bot walks to. One carried only by the
// holder stands round him, one carried only by players round the first of them.
ObjectGuid GroupAnchorOn(std::vector<Player*> const& carriers, Unit* holder)
{
    Player* human = nullptr;
    bool holderCarries = false;
    for (Player* member : carriers)
    {
        if (member == holder)
            holderCarries = true;
        else if (IsDutyBot(member))
            return ObjectGuid::Empty;
        else if (!human)
            human = member;
    }

    if (holderCarries)
        return holder->GetGUID();

    return human ? human->GetGUID() : ObjectGuid::Empty;
}

// Dealt once a wave, on the first read that sees its carriers: each group keeps the spores that landed in
// it, a group with none takes one off a group with two, and a group still without one stands with the other
void UpdateDeal(FgState& state, Creature* boss, std::vector<Player*> const& members)
{
    std::vector<Player*> carriers;
    for (Player* member : members)
        if (member->HasAura(SPELL_GAS_SPORE))
            carriers.push_back(member);
    std::sort(carriers.begin(), carriers.end(), IccGuidLess());

    if (carriers.empty())
    {
        if (!state.wave.empty() || !state.carrier.empty())
            ClearDeal(state);
        return;
    }

    // A swap mid-wave deals again: a holder that carried a spore can walk once relieved
    Unit* holder = boss->GetVictim();
    std::vector<ObjectGuid> wave;
    for (Player* member : carriers)
        wave.push_back(member->GetGUID());
    wave.push_back(holder ? holder->GetGUID() : ObjectGuid::Empty);
    if (wave == state.wave)
        return;

    ClearDeal(state);
    state.wave = wave;

    std::vector<Player*> close;
    std::vector<Player*> far;
    for (Player* member : carriers)
        (IsCloseRole(member) ? close : far).push_back(member);

    if (close.empty() && far.size() >= 2)
    {
        if (Player* spare = SpareCarrier(far, holder, ICC_FG_CLOSE_ANCHOR, false))
        {
            far.erase(std::find(far.begin(), far.end(), spare));
            close.push_back(spare);
        }
    }
    else if (far.empty() && close.size() >= 2)
    {
        if (Player* spare = SpareCarrier(close, holder, ActiveFarAnchor(state), true))
        {
            close.erase(std::find(close.begin(), close.end(), spare));
            far.push_back(spare);
        }
    }

    for (Player* member : close)
        state.carrier.Set(member->GetGUID(), GROUP_CLOSE);
    for (Player* member : far)
        state.carrier.Set(member->GetGUID(), GROUP_FAR);

    state.closeCarried = !close.empty();
    state.farCarried = !far.empty();
    state.closeOn = GroupAnchorOn(close, holder);
    state.farOn = GroupAnchorOn(far, holder);
}

// A spot on the ring round an anchor, by guid so neighbours spread out
Position RingSpot(Position const& anchor, ObjectGuid guid)
{
    float const angle = static_cast<float>(guid.GetCounter() % 16) * static_cast<float>(M_PI) / 8.0f;
    return Position(anchor.GetPositionX() + std::cos(angle) * FG_SPORE_RING,
                    anchor.GetPositionY() + std::sin(angle) * FG_SPORE_RING, anchor.GetPositionZ());
}

// Stand ids past every roster id, so a change of kind always resets the latch
constexpr uint8 STAND_SPORE_CENTRE = 200;
constexpr uint8 STAND_SPORE_RING = 201;

bool SporeStand(FgState const& state, Player* bot, uint8& id, Position& spot, char const*& kind)
{
    if (state.wave.empty() || (!state.closeCarried && !state.farCarried))
        return false;

    auto const carried = state.carrier.find(bot->GetGUID());
    uint8 group = carried != state.carrier.end() ? carried->second : (IsCloseRole(bot) ? GROUP_CLOSE : GROUP_FAR);
    if (group == GROUP_CLOSE && !state.closeCarried)
        group = GROUP_FAR;
    else if (group == GROUP_FAR && !state.farCarried)
        group = GROUP_CLOSE;

    ObjectGuid const on = group == GROUP_CLOSE ? state.closeOn : state.farOn;
    Player* anchorPlayer = on.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, on);
    if (anchorPlayer && (anchorPlayer == bot || !anchorPlayer->IsAlive() || !IccSameInstance(bot, anchorPlayer)))
        anchorPlayer = nullptr;

    Position const anchor = anchorPlayer ? anchorPlayer->GetPosition()
                                         : (group == GROUP_CLOSE ? ICC_FG_CLOSE_ANCHOR : ActiveFarAnchor(state));
    bool const centre = carried != state.carrier.end() && !anchorPlayer;
    id = centre ? STAND_SPORE_CENTRE : STAND_SPORE_RING;
    spot = centre ? anchor : RingSpot(anchor, bot->GetGUID());
    kind = group == GROUP_CLOSE ? "close" : "far";
    return true;
}

bool DesiredStand(FgState const& state, Player* bot, Creature* boss, uint8& id, Position& spot, char const*& kind)
{
    if (!IsDutyBot(bot))
        return false;

    Unit* holder = boss->GetVictim();
    if (holder == bot && !IsTankPlayer(bot))
        return false;

    if (holder != bot && SporeStand(state, bot, id, spot, kind))
        return true;

    auto const held = state.slot.find(bot->GetGUID());
    if (held == state.slot.end())
        return false;

    id = held->second;
    if (id >= FG_STAND_TANK && id < FG_STAND_TANK + FG_TANK_SPOTS)
    {
        spot = ICC_FG_TANK_SPOTS[id - FG_STAND_TANK];
        kind = "tank";
        return true;
    }

    if (id == FG_STAND_LEG)
    {
        if (boss->GetExactDist2d(ICC_FG_BOSS_SPOT) > FG_BOSS_DRIFT)
            return false;

        spot = ICC_FG_LEGS[state.leg.Get() == 2 ? 1 : 0];
        kind = "leg";
        return true;
    }

    // Reach stays blocked on a slot, so a slot holder would watch him walk out of range
    if (id < 1 || id > FG_SLOT_COUNT || boss->GetExactDist2d(ICC_FG_BOSS_SPOT) > SLOT_DRIFT)
        return false;

    spot = ICC_FG_SLOTS[id - 1];
    kind = "slot";
    return true;
}

void UpdateStand(FgState& state, Player* bot, Creature* boss)
{
    uint8 id = 0;
    Position spot;
    char const* kind = "none";
    bool const wanted = bot->IsAlive() && DesiredStand(state, bot, boss, id, spot, kind);
    Note(bot, "fg.stand", wanted ? kind : "none");

    Stand& stand = state.stands[bot->GetGUID()];
    if (!wanted)
    {
        stand.held = false;
        return;
    }

    if (!stand.held || stand.id != id)
    {
        stand.id = id;
        stand.held = true;
        stand.parked = false;
    }

    // A ring round a player follows him
    stand.spot = spot;
    float const distance = bot->GetExactDist2d(stand.spot);
    stand.parked = stand.parked ? distance <= FG_STAND_UNPARK : distance <= FG_STAND_PARK;
}

Stand const* HeldStand(PlayerbotAI* botAI)
{
    FgState* state = LiveState(botAI);
    if (!state)
        return nullptr;

    auto const itr = state->stands.find(botAI->GetBot()->GetGUID());
    return itr != state->stands.end() && itr->second.held ? &itr->second : nullptr;
}

// A tank not holding him past FG_BRAKE_CAP of the holder's threat, until it falls under FG_BRAKE_RELEASE.
// Read from the threat list: the "threat" value is a uint8 percentage and wraps past 255%.
void UpdateBrake(Player* bot, Creature* boss, FgState& state)
{
    bool hold = false;
    Unit* holder = boss->GetVictim();
    if (IsDutyBot(bot) && IsTankPlayer(bot) && holder && holder != bot)
    {
        float const top = boss->GetThreatMgr().GetThreat(holder);
        float const own = boss->GetThreatMgr().GetThreat(bot);
        float const share = state.brake.count(bot->GetGUID()) ? FG_BRAKE_RELEASE : FG_BRAKE_CAP;
        hold = top > 0.0f && own > 0.0f && own >= share * top;
    }

    if (hold)
        state.brake.insert(bot->GetGUID());
    else
        state.brake.erase(bot->GetGUID());
}

void UpdateBlight(FgState& state, Creature* boss)
{
    uint32 const stacks = BlightStacks(boss);
    if (stacks < state.blightLast)
        ++state.blightCycle;

    state.blightLast = stacks;
}

bool BlightWindow(FgState const& state, Player* tank, uint32 stacks)
{
    if (stacks >= FG_BLIGHT_MAJOR)
        return true;

    auto const spent = state.minorSpent.find(tank->GetGUID());
    return stacks >= FG_BLIGHT_MINOR && (spent == state.minorSpent.end() || spent->second != state.blightCycle);
}

// A human tank has no trigger, so it gets told: its taunt when the bot holding him reaches the swap with no
// bot to take him, a defensive at the holder's Inhaled Blight windows
void CallHumanTank(FgState& state, Player* sender, Creature* boss, uint32 now)
{
    if (state.humanCallRan && getMSTimeDiff(state.humanCallMs, now) < HUMAN_CALL_CHECK_MS)
        return;

    state.humanCallRan = true;
    state.humanCallMs = now;

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (!holder || !holder->IsAlive() || !IsTankPlayer(holder))
        return;

    uint32 const bloat = BloatId(boss);
    if (IsDutyBot(holder))
    {
        if (Stacks(holder, bloat) < FG_SWAP_STACKS || boss->HasAuraType(SPELL_AURA_MOD_TAUNT) ||
            SwapTaunter(sender, boss))
            return;

        for (Player* member : MembersOnMap(sender))
        {
            uint32 const taunt = IsDutyBot(member) ? 0 : TauntSpellId(member);
            if (taunt && member != holder && IsTankPlayer(member) && Stacks(member, bloat) <= FG_TAUNTER_MAX_STACKS &&
                member->HasSpell(taunt) && !member->HasSpellCooldown(taunt))
            {
                CallHumanTankDefensive(sender, member, taunt, 0, "Gastric Bloat is at 9 on your partner", "fg.humancall");
                return;
            }
        }

        return;
    }

    uint32 const stacks = BlightStacks(boss);
    bool const minor = stacks >= FG_BLIGHT_MINOR && stacks < FG_BLIGHT_MAJOR &&
                       state.humanMinorCall != state.blightCycle + 1;
    bool const major = stacks >= FG_BLIGHT_MAJOR;
    bool const low = holder->GetHealthPct() < FG_TANK_DEFENSIVE_PCT;
    if (!minor && !major && !low)
        return;

    uint32 const spellId = ReadyTankDefensiveSpell(holder, true, 0, 0);
    char const* why = low ? "you are low" : "Inhaled Blight is stacking";
    if (CallHumanTankDefensive(sender, holder, spellId, 0, why, "fg.humancall") && minor)
        state.humanMinorCall = state.blightCycle + 1;
}

Unit* ResolveTarget(PlayerbotAI* botAI, FgState const& state, Creature* boss)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if (!tank && !IsRaidDpsBot(bot))
        return nullptr;

    if (tank && state.brake.count(bot->GetGUID()))
    {
        Note(bot, "fg.target", "brake");
        return nullptr;
    }

    Unit* pick = GetRaidHumanSkull(botAI, &IsFgUnit);
    Note(bot, "fg.target", pick ? "skull" : "boss");
    return pick ? pick : boss;
}
}  // namespace

Creature* GetFgBoss(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Creature* boss = bot && bot->IsAlive() ? BossInCombat(bot) : nullptr;
    return boss && bot->GetExactDist2d(ICC_FG_BOSS_SPOT) <= ROOM_RADIUS ? boss : nullptr;
}

bool IsFgFight(PlayerbotAI* botAI) { return GetFgBoss(botAI) != nullptr; }

bool IccFestergutSporeCycle(Player* bot)
{
    bool const spores = IccAnyGroupMemberHasAura(bot, SPELL_GAS_SPORE);
    Note(bot, "fg.phase", spores ? "spore" : "normal");
    return spores;
}

Unit* GetFgTarget(PlayerbotAI* botAI)
{
    FgState* state = LiveState(botAI);
    Creature* boss = state ? GetFgBoss(botAI) : nullptr;
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

bool IsFgSwapTaunter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* boss = GetFgBoss(botAI);
    if (!boss || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return false;

    bool const elected = SwapTaunter(bot, boss) == bot;
    Note(bot, "fg.swap", elected ? "1" : "0");
    return elected;
}

bool IsFgBraked(PlayerbotAI* botAI)
{
    FgState* state = LiveState(botAI);
    return state && state->brake.count(botAI->GetBot()->GetGUID());
}

uint32 GetFgTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    FgState* state = LiveState(botAI);
    Creature* boss = state ? GetFgBoss(botAI) : nullptr;
    Player* bot = botAI->GetBot();
    if (!boss || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return 0;

    bool const window = bot->GetHealthPct() < FG_TANK_DEFENSIVE_PCT ||
                        (boss->GetVictim() == bot && BlightWindow(*state, bot, BlightStacks(boss)));
    if (!window)
    {
        // Change-latched, so the way out of a window is written too
        if (noteKind)
            Note(bot, noteKind, "idle");

        return 0;
    }

    return NextTankDefensive(botAI, bot, noteKind, true);
}

bool IsFgTankDefensiveHeld(PlayerbotAI* botAI)
{
    return IsFgFight(botAI) && botAI->GetBot()->GetHealthPct() >= FG_TANK_LAST_RESORT_PCT;
}

void LatchFgTankDefensive(PlayerbotAI* botAI)
{
    FgState* state = LiveState(botAI);
    Creature* boss = state ? GetFgBoss(botAI) : nullptr;
    if (boss && boss->GetVictim() == botAI->GetBot() && BlightStacks(boss) >= FG_BLIGHT_MINOR)
        state->minorSpent[botAI->GetBot()->GetGUID()] = state->blightCycle;
}

bool GetFgPungentHit(PlayerbotAI* botAI, RaidPredictedHit& hit, bool own)
{
    Player* bot = botAI->GetBot();
    Creature* boss = GetFgBoss(botAI);
    Spell* spell = boss ? boss->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
    SpellInfo const* info = spell ? spell->GetSpellInfo() : nullptr;
    if (!info || info->Id != sSpellMgr->GetSpellIdForDifficulty(SPELL_FG_PUNGENT_BLIGHT, boss))
        return false;

    uint32 const inoculatedId = sSpellMgr->GetSpellIdForDifficulty(SPELL_FG_INOCULATED, boss);
    uint32 inoculated = Stacks(bot, inoculatedId);
    if (!own)
        for (Player* member : MembersOnMap(bot))
            inoculated = std::min(inoculated, Stacks(member, inoculatedId));

    SpellEffectInfo const& effect = info->Effects[EFFECT_0];
    float const base = static_cast<float>(std::max(effect.BasePoints + (effect.DieSides + 1) / 2, 0));
    float const cut = 1.0f - INOCULATED_CUT * static_cast<float>(std::min(inoculated, INOCULATED_MAX));

    hit = RaidPredictedHit();
    hit.startMs = getMSTime() + static_cast<uint32>(std::max(spell->GetCastTimeRemaining(), 0));
    hit.damage = static_cast<uint32>(base * cut);
    hit.school = SPELL_SCHOOL_MASK_SHADOW;
    return true;
}

RaidCover GetFgPungentCover(PlayerbotAI* botAI)
{
    if (!IsFgFight(botAI))
        return RaidCover::None;

    RaidPredictedHit hit;
    return GetRaidCover(botAI, ICC_DEFENSIVE_SCOPE, GetFgPungentHit(botAI, hit, false) ? &hit : nullptr);
}

bool IsFgPungentCoverHeld(PlayerbotAI* botAI)
{
    RaidPredictedHit hit;
    return GetFgPungentHit(botAI, hit, false) && RaidHitLeadIn(hit);
}

bool GetFgGoo(PlayerbotAI* botAI, std::vector<Position>& impacts)
{
    impacts.clear();
    Player* bot = botAI->GetBot();
    if (!IsFgFight(botAI))
        return false;

    impacts = IcecrownHelpers::ActiveGooPositions(bot->GetInstanceId(), FG_GOO_HAZARD_MS);
    return !impacts.empty();
}

bool IsFgInGoo(PlayerbotAI* botAI)
{
    std::vector<Position> impacts;
    Player* bot = botAI->GetBot();
    if (!GetFgGoo(botAI, impacts))
        return false;

    Creature* boss = GetFgBoss(botAI);
    return boss && boss->GetVictim() != bot && !ClearOf(bot->GetPosition(), impacts, FG_GOO_RUN);
}

bool GetFgGooDodge(PlayerbotAI* botAI, Position& spot)
{
    std::vector<Position> impacts;
    FgState* state = LiveState(botAI);
    if (!state || !GetFgGoo(botAI, impacts))
        return false;

    Player* bot = botAI->GetBot();
    Stand const* stand = HeldStand(botAI);
    if (stand && ClearOf(stand->spot, impacts, FG_GOO_CLEARANCE))
    {
        spot = stand->spot;
        Note(bot, "fg.goo", "stand");
        return true;
    }

    std::vector<EncounterHelpers::HazardCircle> hazards;
    for (Position const& impact : impacts)
        hazards.emplace_back(impact, FG_GOO_CLEARANCE);

    // Out of a spore stack a ranged bot keeps its Vile Gas spacing while it dodges
    std::vector<EncounterHelpers::HazardCircle> spread = hazards;
    if (!IsCloseRole(bot) && state->wave.empty())
        for (Player* member : MembersOnMap(bot))
            if (member != bot)
                spread.emplace_back(member->GetPosition(), SPREAD_KEEPOUT);

    Position const prefer = stand ? stand->spot : bot->GetPosition();
    EncounterHelpers::HazardSweepCache sweep;
    char const* branch = "spread";
    spot = EncounterHelpers::FindNearestPositionClearOfHazards(bot, spread, DODGE_SWEEP_RADIUS, 2.0f,
                                                               static_cast<float>(M_PI) / 8.0f, &prefer, {}, &sweep);
    if (spot == Position() && spread.size() > hazards.size())
    {
        branch = "tight";
        spot = EncounterHelpers::FindNearestPositionClearOfHazards(bot, hazards, DODGE_SWEEP_RADIUS, 2.0f,
                                                                   static_cast<float>(M_PI) / 8.0f, &prefer, {}, &sweep);
    }

    bool const found = spot != Position();
    Note(bot, "fg.goo", found ? branch : "none");
    return found;
}

bool IsFgHoldingStand(PlayerbotAI* botAI) { return HeldStand(botAI) != nullptr; }

bool IsFgHoldingTankStand(PlayerbotAI* botAI)
{
    Stand const* stand = HeldStand(botAI);
    return stand && stand->id >= FG_STAND_TANK && stand->id < FG_STAND_TANK + FG_TANK_SPOTS;
}

bool GetFgStandWalk(PlayerbotAI* botAI, Position& spot)
{
    Stand const* stand = HeldStand(botAI);
    if (!stand || stand->parked)
        return false;

    // The dodge walked it off a stand the goo still covers
    std::vector<Position> impacts;
    if (GetFgGoo(botAI, impacts) && !ClearOf(stand->spot, impacts, FG_GOO_CLEARANCE))
        return false;

    spot = stand->spot;
    return true;
}

void TickFgEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    // A wipe or a kill ends the pull: the next one rebuilds every slot and duty
    if (!BossInCombat(bot))
    {
        if (fgStates.Find(bot->GetInstanceId()))
            fgStates.Reset(bot->GetInstanceId());
        return;
    }

    Creature* boss = GetFgBoss(botAI);
    if (!boss)
        return;

    IccFestergutSporeCycle(bot);

    FgState& state = fgStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    UpdateBlight(state, boss);
    RefreshRoster(state, bot, now);

    if (!state.dealRan || getMSTimeDiff(state.dealMs, now) >= DEAL_REFRESH_MS)
    {
        state.dealRan = true;
        state.dealMs = now;
        UpdateFlips(state, IcecrownHelpers::ActiveGooPositions(bot->GetInstanceId(), FG_GOO_HAZARD_MS));
        UpdateDeal(state, boss, MembersOnMap(bot));
    }

    UpdateBrake(bot, boss, state);
    CallHumanTank(state, bot, boss, now);
    UpdateStand(state, bot, boss);
}

uint8 IccFestergutResistSchools(PlayerbotAI* /*botAI*/, uint8 /*previous*/) { return RAID_RESIST_SHADOW; }

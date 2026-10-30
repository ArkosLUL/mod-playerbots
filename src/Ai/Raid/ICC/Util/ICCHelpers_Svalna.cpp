/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Svalna.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "AiObjectContext.h"
#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCData.h"
#include "ICCHelpers_Shared.h"
#include "InstanceScript.h"
#include "Item.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDirectTargeting.h"
#include "RaidEncounter.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace
{
constexpr float LIVE_RANGE = 100.0f;
constexpr float SCAN_RANGE = 100.0f;
constexpr uint32 SCAN_MS = 200;
constexpr float SHIELD_NOTE_RANGE = 30.0f;
// Hurl Spear reaches 25 yd
constexpr float HURL_RANGE = 24.0f;
constexpr float TAUNT_RANGE = 30.0f;
constexpr uint32 HUMAN_CALL_CHECK_MS = 500;
constexpr float MELEE_INTERRUPT_RANGE = 5.0f;

// The hall's packs stand within 40 yd of each other, so one pull reaches this far round its first add
constexpr float GAUNTLET_SCAN_RANGE = 60.0f;
// Whirlwind's 41057 hits 8 yd round the Warlord as its 2 s cast ends. Frozen Orb ticks once as its 10 s
// aura runs out, into 71286 6 yd round the Frostbinder itself.
constexpr float WHIRLWIND_RADIUS = 8.0f;
constexpr float ORB_RADIUS = 6.0f;
constexpr int32 ORB_LEAD_MS = 3000;
// A dodge fires a little past a radius and clears it by the park tolerance and a margin
constexpr float DANGER_PAD = 0.75f;
constexpr float CLEAR_PAD = 2.5f;
constexpr float DODGE_SWEEP = 15.0f;
// A melee bot's dodge spot keeps its target this far past its combat reach where it can
constexpr float MELEE_KEEP = 3.0f;

// Kill order among the raised captains: Arnath heals the others and, on 25-man, casts Dominate Mind
constexpr uint8 CAPTAIN_ORDER[SS_CAPTAINS] = {0, 3, 2, 1};

// Interrupts first, then silences where the caster takes them
constexpr char const* KICKS[] = {"kick", "pummel", "shield bash", "mind freeze", "counterspell", "wind shear"};
constexpr char const* SILENCES[] = {"silencing shot", "silence", "strangulate"};

// Asked by a trigger, its action and a rule for every action a bot weighs, and a bot never ticks twice in
// one ms
struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
};

struct SsState
{
    uint32 scanMs = 0;
    bool scanned = false;
    std::vector<ObjectGuid> spears;
    // Bot -> the Impaling Spear it pulls out
    RaidObs::ObsGuidMap<ObjectGuid> spear{"ss.spear"};
    // Bot tank -> the raised captain it holds
    RaidObs::ObsGuidMap<ObjectGuid> tank{"ss.tank"};
    // Kicker -> the Arnath it kicks
    std::unordered_map<ObjectGuid, ObjectGuid> kicks;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
    uint32 humanCallMs = 0;
    bool humanCallRan = false;
};

struct GauntletState
{
    uint32 scanMs = 0;
    bool scanned = false;
    // Every live Ymirjar and summon round the pull in combat; who it fights is read at use
    std::vector<ObjectGuid> adds;
    // Kicker -> the Deathbringer it kicks
    std::unordered_map<ObjectGuid, ObjectGuid> kicks;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
    // The reach hold is asked for every action a bot weighs: per bot, the ms it was read and the answer
    std::unordered_map<ObjectGuid, std::pair<uint32, bool>> reachHeld;
    uint32 humanCallMs = 0;
    bool humanCallRan = false;
};

RaidInstanceState<SsState> ssStates;
RaidInstanceState<GauntletState> gauntletStates;

bool IsTankPlayer(Player* player)
{
    return player && (PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true));
}

// Players hold no duty, and a selfbot counts as a player here.
bool IsDutyBot(Player* player) { return player && GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

void NoteDerived(Player* bot, char const* key, std::string const& value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

InstanceScript* IccInstance(Player* bot)
{
    return bot && bot->IsInWorld() && bot->GetMapId() == ICC_MAP_ID ? bot->GetInstanceScript() : nullptr;
}

Creature* SlotCreature(Player* bot, uint32 data)
{
    InstanceScript* instance = IccInstance(bot);
    return instance ? ObjectAccessor::GetCreature(*bot, instance->GetGuidData(data)) : nullptr;
}

bool InGroup(Player* bot, Unit* victim)
{
    Player* player = victim ? victim->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    return player && bot->IsInSameRaidWith(player);
}

// The Spell constructor remaps a difficulty row's id, so a caller passes the remapped one. Whirlwind 41056
// is self-channelled and sits in the channel slot through its 2 s cast.
bool Casting(Unit* caster, uint32 spellId)
{
    if (!caster)
        return false;

    for (CurrentSpellTypes slot : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL})
        if (Spell* spell = caster->GetCurrentSpell(slot))
            if (spell->GetSpellInfo() && spell->GetSpellInfo()->Id == spellId)
                return true;

    return false;
}

bool CastingFlashHeal(Creature* arnath)
{
    return Casting(arnath, sSpellMgr->GetSpellIdForDifficulty(SPELL_SS_ARNATH_FLASH_HEAL, arnath));
}

bool CastingShadowBolt(Creature* deathbringer)
{
    return Casting(deathbringer, sSpellMgr->GetSpellIdForDifficulty(SPELL_SS_EMPOWERED_SHADOW_BOLT, deathbringer));
}

bool IsRaisedCaptain(Unit* unit)
{
    switch (unit ? unit->GetEntry() : 0)
    {
        case NPC_SS_ARNATH_UNDEAD:
        case NPC_SS_BRANDON_UNDEAD:
        case NPC_SS_GRONDEL_UNDEAD:
        case NPC_SS_RUPERT_UNDEAD:
            return true;
        default:
            return false;
    }
}

bool IsFighting(Unit* unit) { return unit && unit->IsAlive() && unit->IsInCombat(); }

// Arnath, Brandon, Grondel, Rupert by slot; null where that captain is not a raised one in the fight
Creature* FightingCaptain(Player* bot, uint8 slot)
{
    Creature* captain = SlotCreature(bot, SS_DATA_CAPTAIN_ARNATH_GUID + slot);
    return IsRaisedCaptain(captain) && IsFighting(captain) ? captain : nullptr;
}

bool IsSsUnit(Unit* unit) { return IsRaisedCaptain(unit) || (unit && unit->GetEntry() == NPC_SS_SVALNA); }

bool AnyCaptainFighting(Player* bot)
{
    for (uint8 slot = 0; slot < SS_CAPTAINS; ++slot)
        if (FightingCaptain(bot, slot))
            return true;

    return false;
}

bool CaptainHits(Player* bot, Player* member)
{
    for (uint8 slot = 0; slot < SS_CAPTAINS; ++slot)
        if (Creature* captain = FightingCaptain(bot, slot))
            if (captain->GetVictim() == member)
                return true;

    return false;
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

bool HoldsSomething(Player* bot, Player* member)
{
    if (Creature* svalna = SlotCreature(bot, SS_DATA_SVALNA_GUID))
        if (IsFighting(svalna) && svalna->GetVictim() == member)
            return true;

    return CaptainHits(bot, member);
}

bool CanPayFor(Player* bot, SpellInfo const* info)
{
    // POWER_HEALTH is -2 and Powers is signed, hence the cast.
    if (info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return true;

    return static_cast<int32>(bot->GetPower(Powers(info->PowerType))) >=
           info->CalcPowerCost(bot, info->GetSchoolMask());
}

// The first kick the member can land on the caster right now. A false ready here holds every other kicker
// back from the election, so power and form are checked too.
char const* ReadyKick(Player* member, Unit* caster, bool silences)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    if (!memberAI || member->HasUnitState(UNIT_STATE_CASTING) || member->GetExactDist(caster) > SCAN_RANGE)
        return nullptr;

    auto const ready = [&](char const* name)
    {
        uint32 const spellId = memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", name)->Get();
        SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!info || !CanPayFor(member, info) || info->CheckShapeshift(member->GetShapeshiftForm()) != SPELL_CAST_OK)
            return false;

        float const range = member->GetSpellMaxRangeForTarget(caster, info);
        bool const inReach = range <= MELEE_INTERRUPT_RANGE ? member->IsWithinMeleeRange(caster)
                                                            : member->IsWithinCombatRange(caster, range);
        return inReach && memberAI->CanCastSpell(spellId, caster) && member->IsWithinLOSInMap(caster);
    };

    for (char const* kick : KICKS)
        if (ready(kick))
            return kick;

    if (silences)
        for (char const* silence : SILENCES)
            if (ready(silence))
                return silence;

    return nullptr;
}

// Each caster, in the order given, to the nearest bot with a kick ready that no earlier caster took
void AssignKicks(std::unordered_map<ObjectGuid, ObjectGuid>& kicks, std::vector<Creature*> const& casters,
                 std::vector<Player*> const& members)
{
    kicks.clear();
    for (Creature* caster : casters)
    {
        Player* best = nullptr;
        std::tuple<float, ObjectGuid> bestKey;
        for (Player* member : members)
        {
            if (!IsDutyBot(member) || kicks.count(member->GetGUID()) || member->HasAura(SPELL_SS_IMPALING_SPEAR) ||
                member->IsCharmed() || !ReadyKick(member, caster, true))
                continue;

            auto const key = std::make_tuple(member->GetExactDist(caster), member->GetGUID());
            if (!best || key < bestKey)
            {
                best = member;
                bestKey = key;
            }
        }

        if (best)
            kicks[best->GetGUID()] = caster->GetGUID();
    }
}

// The caster this bot is elected to kick, still casting what it was elected for
Creature* KickFor(std::unordered_map<ObjectGuid, ObjectGuid> const& kicks, Player* bot)
{
    auto const itr = kicks.find(bot->GetGUID());
    Creature* caster = itr != kicks.end() ? bot->GetMap()->GetCreature(itr->second) : nullptr;
    return caster && caster->IsAlive() && caster->IsNonMeleeSpellCast(false) ? caster : nullptr;
}

SsState* LiveState(PlayerbotAI* botAI)
{
    return IsSsLive(botAI) ? ssStates.Find(botAI->GetBot()->GetInstanceId()) : nullptr;
}

void Scan(SsState& st, Player* bot)
{
    st.spears.clear();
    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_SPEAR, SCAN_RANGE);
    for (Creature* spear : found)
        if (spear->IsAlive() && spear->GetVehicleBase())
            st.spears.push_back(spear->GetGUID());
}

Creature* LiveSpear(Player* bot, ObjectGuid const& guid)
{
    Creature* spear = bot->GetMap()->GetCreature(guid);
    return spear && spear->IsAlive() && spear->GetVehicleBase() ? spear : nullptr;
}

// Who may pull a spear: no impaled bot (stunned), no bot under Arnath's Dominate Mind, no tank holding an
// add, no bot locked to one ability (Bladestorm turns the click's cast into a silent no-op)
bool CanPull(Player* bot, Player* member)
{
    return IsDutyBot(member) && !member->HasAura(SPELL_SS_IMPALING_SPEAR) && !member->GetVehicle() &&
           !member->IsCharmed() && !member->HasAuraType(SPELL_AURA_ALLOW_ONLY_ABILITY) &&
           !(IsTankPlayer(member) && HoldsSomething(bot, member));
}

void AssignSpears(SsState& st, Player* bot, std::vector<Player*> const& members)
{
    // A puller that can no longer pull hands its spear on: the victim loses 10% a second meanwhile
    std::vector<ObjectGuid> lapsed;
    for (auto const& [puller, spear] : st.spear)
    {
        Player* member = ObjectAccessor::GetPlayer(*bot, puller);
        if (!LiveSpear(bot, spear) || !member || !member->IsAlive() || !IccSameInstance(bot, member) ||
            !CanPull(bot, member))
            lapsed.push_back(puller);
    }
    for (ObjectGuid const& puller : lapsed)
        st.spear.erase(puller);

    for (ObjectGuid const& guid : st.spears)
    {
        bool claimed = false;
        for (auto const& [puller, spear] : st.spear)
            claimed = claimed || spear == guid;
        Creature* spear = LiveSpear(bot, guid);
        if (claimed || !spear)
            continue;

        // Damage dealers first, then healers, then a tank holding nothing
        Player* best = nullptr;
        std::tuple<uint8, float, ObjectGuid> bestKey;
        for (Player* member : members)
        {
            if (!CanPull(bot, member) || st.spear.count(member->GetGUID()))
                continue;

            uint8 const tier = IsRaidDpsBot(member) ? 0 : PlayerbotAI::IsHeal(member) ? 1 : 2;
            auto const key = std::make_tuple(tier, member->GetExactDist2d(spear), member->GetGUID());
            if (!best || key < bestKey)
            {
                best = member;
                bestKey = key;
            }
        }

        if (best)
            st.spear[best->GetGUID()] = guid;
    }
}

void AssignCaptainTanks(SsState& st, Player* bot, std::vector<Player*> const& members)
{
    std::vector<ObjectGuid> lapsed;
    for (auto const& [holder, captainGuid] : st.tank)
    {
        Creature* captain = bot->GetMap()->GetCreature(captainGuid);
        Player* member = ObjectAccessor::GetPlayer(*bot, holder);
        if (!IsRaisedCaptain(captain) || !IsFighting(captain) || !member || !member->IsAlive() ||
            !IccSameInstance(bot, member))
            lapsed.push_back(holder);
    }
    for (ObjectGuid const& holder : lapsed)
        st.tank.erase(holder);

    for (uint8 slot : CAPTAIN_ORDER)
    {
        Creature* captain = FightingCaptain(bot, slot);
        if (!captain)
            continue;

        bool owned = false;
        for (auto const& [holder, captainGuid] : st.tank)
            owned = owned || captainGuid == captain->GetGUID();
        if (owned)
            continue;

        Player* best = nullptr;
        float bestDistance = 0.0f;
        for (Player* member : members)
        {
            if (!IsDutyBot(member) || !IsTankPlayer(member) || st.tank.count(member->GetGUID()))
                continue;

            float const distance = member->GetExactDist2d(captain);
            if (!best || distance < bestDistance ||
                (distance == bestDistance && member->GetGUID() < best->GetGUID()))
            {
                best = member;
                bestDistance = distance;
            }
        }

        if (best)
            st.tank[best->GetGUID()] = captain->GetGUID();
    }
}

// Arnath's casts worth a kick: Dominate Mind exists on 25-man only, and his Flash Heal tops the others up
bool ArnathKickable(Creature* arnath)
{
    return Casting(arnath, SPELL_SS_DOMINATE_MIND) || CastingFlashHeal(arnath);
}

void AssignArnathKicks(SsState& st, Player* bot, std::vector<Player*> const& members)
{
    std::vector<Creature*> casters;
    if (Creature* arnath = FightingCaptain(bot, 0))
        if (ArnathKickable(arnath))
            casters.push_back(arnath);

    AssignKicks(st.kicks, casters, members);
}

// A human tank has no trigger, so it gets told when a captain has it low
void CallHumanTank(SsState& st, Player* sender, uint32 now)
{
    if (st.humanCallRan && getMSTimeDiff(st.humanCallMs, now) < HUMAN_CALL_CHECK_MS)
        return;

    st.humanCallRan = true;
    st.humanCallMs = now;
    for (Player* member : MembersOnMap(sender))
    {
        if (IsDutyBot(member) || !IsTankPlayer(member) || member->GetHealthPct() >= SS_TANK_DEFENSIVE_PCT ||
            !CaptainHits(sender, member))
            continue;

        if (uint32 const spellId = ReadyTankDefensiveSpell(member, true, 0, 0))
            CallHumanTankDefensive(sender, member, spellId, 0, "you are low", "ss.humancall");
    }
}

Creature* OwnedCaptain(SsState const& st, Player* bot)
{
    auto const itr = st.tank.find(bot->GetGUID());
    Creature* captain = itr != st.tank.end() ? bot->GetMap()->GetCreature(itr->second) : nullptr;
    return IsRaisedCaptain(captain) && IsFighting(captain) ? captain : nullptr;
}

bool Shielded(Unit* svalna) { return svalna && svalna->HasAura(SPELL_AETHER_SHIELD); }

Unit* ResolveTarget(PlayerbotAI* botAI, SsState const& st)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if (!tank && !IsRaidDpsBot(bot))
        return nullptr;

    if (Unit* skull = GetRaidHumanSkull(botAI, &IsSsUnit))
    {
        NoteDerived(bot, "ss.target", "skull");
        return skull;
    }

    if (tank)
        if (Creature* captain = OwnedCaptain(st, bot))
        {
            NoteDerived(bot, "ss.target", "owned");
            return captain;
        }

    for (uint8 slot : CAPTAIN_ORDER)
    {
        Creature* captain = FightingCaptain(bot, slot);
        if (!captain || captain->HasAura(SPELL_SS_CAPTAIN_DIVINE_SHIELD))
            continue;

        // A tank leaves another tank's captain alone
        Player* victim = captain->GetVictim() ? captain->GetVictim()->ToPlayer() : nullptr;
        if (tank && victim && victim != bot && IsTankPlayer(victim))
            continue;

        NoteDerived(bot, "ss.target", "captain");
        return captain;
    }

    Creature* svalna = GetSsSvalna(botAI);
    if (IsFighting(svalna) && !Shielded(svalna))
    {
        NoteDerived(bot, "ss.target", "svalna");
        return svalna;
    }

    NoteDerived(bot, "ss.target", "none");
    return nullptr;
}

// The holder nearest Svalna throws; the rest keep theirs for the next shield
Player* ElectedThrower(Player* bot, Creature* svalna)
{
    Player* best = nullptr;
    float bestDistance = 0.0f;
    for (Player* member : MembersOnMap(bot))
    {
        if (!IsDutyBot(member) || !member->HasItemCount(ITEM_SPEAR, 1) || member->HasAura(SPELL_SS_IMPALING_SPEAR) ||
            member->IsCharmed())
            continue;

        float const distance = member->GetExactDist(svalna);
        if (!best || distance < bestDistance || (distance == bestDistance && member->GetGUID() < best->GetGUID()))
        {
            best = member;
            bestDistance = distance;
        }
    }

    return best;
}

Creature* GauntletLiveUnit(PlayerbotAI* botAI)
{
    return UnitGateLiveUnit<NPC_SS_BATTLE_MAIDEN, NPC_SS_DEATHBRINGER, NPC_SS_FROSTBINDER, NPC_SS_HUNTRESS,
                            NPC_SS_WARLORD, NPC_SS_CORRUPTED_YMIRJAR, NPC_SS_WARHAWK>(botAI);
}

GauntletState* LiveGauntlet(PlayerbotAI* botAI)
{
    return IsSsGauntletLive(botAI) ? gauntletStates.Find(botAI->GetBot()->GetInstanceId()) : nullptr;
}

bool IsMeleeYmirjar(Unit* unit)
{
    switch (unit ? unit->GetEntry() : 0)
    {
        case NPC_SS_WARLORD:
        case NPC_SS_BATTLE_MAIDEN:
        case NPC_SS_CORRUPTED_YMIRJAR:
            return true;
        default:
            return false;
    }
}

// Ranged damage dealers open on the Deathbringers, melee on the Frostbinders, as the gauntlet blog has it
int8 GauntletTier(Unit* unit, bool melee)
{
    switch (unit ? unit->GetEntry() : 0)
    {
        case NPC_SS_DEATHBRINGER:
            return melee ? 1 : 0;
        case NPC_SS_FROSTBINDER:
            return melee ? 0 : 1;
        case NPC_SS_HUNTRESS:
            return 2;
        case NPC_SS_BATTLE_MAIDEN:
            return 3;
        case NPC_SS_WARLORD:
            return 4;
        case NPC_SS_CORRUPTED_YMIRJAR:
        case NPC_SS_WARHAWK:
            return 5;
        default:
            return -1;
    }
}

bool IsGauntletUnit(Unit* unit) { return GauntletTier(unit, false) >= 0; }

char const* GauntletName(Unit* unit)
{
    switch (unit->GetEntry())
    {
        case NPC_SS_DEATHBRINGER:
            return "deathbringer";
        case NPC_SS_FROSTBINDER:
            return "frostbinder";
        case NPC_SS_HUNTRESS:
            return "huntress";
        case NPC_SS_BATTLE_MAIDEN:
            return "battlemaiden";
        case NPC_SS_WARLORD:
            return "warlord";
        case NPC_SS_CORRUPTED_YMIRJAR:
            return "corrupted";
        default:
            return "warhawk";
    }
}

bool Engaged(Player* bot, Creature* add)
{
    return add && add->IsAlive() && add->IsInCombat() &&
           (InGroup(bot, add->GetVictim()) || bot->IsInCombatWith(add)) && bot->IsValidAttackTarget(add);
}

std::vector<Creature*> EngagedAdds(GauntletState const& st, Player* bot)
{
    std::vector<Creature*> adds;
    for (ObjectGuid const& guid : st.adds)
        if (Creature* add = bot->GetMap()->GetCreature(guid))
            if (Engaged(bot, add))
                adds.push_back(add);

    return adds;
}

void ScanGauntlet(GauntletState& st, Creature* source, Player* bot, uint32 now)
{
    if (st.scanned && getMSTimeDiff(st.scanMs, now) < SCAN_MS)
        return;

    st.scanned = true;
    st.scanMs = now;
    st.adds.clear();
    static std::vector<uint32> const entries = {NPC_SS_BATTLE_MAIDEN,     NPC_SS_DEATHBRINGER, NPC_SS_FROSTBINDER,
                                                NPC_SS_HUNTRESS,          NPC_SS_WARLORD,
                                                NPC_SS_CORRUPTED_YMIRJAR, NPC_SS_WARHAWK};
    std::list<Creature*> found;
    source->GetCreatureListWithEntryInGrid(found, entries, GAUNTLET_SCAN_RANGE);
    for (Creature* add : found)
        if (add->IsAlive() && add->IsInCombat())
            st.adds.push_back(add->GetGUID());

    // Summon Undead raises five Corrupted Ymirjar, so it goes before the bolts
    std::vector<std::pair<uint8, Creature*>> casting;
    for (ObjectGuid const& guid : st.adds)
    {
        Creature* add = source->GetMap()->GetCreature(guid);
        if (!add || add->GetEntry() != NPC_SS_DEATHBRINGER)
            continue;

        if (Casting(add, SPELL_SS_SUMMON_UNDEAD))
            casting.emplace_back(0, add);
        else if (CastingShadowBolt(add))
            casting.emplace_back(1, add);
    }
    std::sort(casting.begin(), casting.end(),
              [](auto const& a, auto const& b)
              {
                  return std::make_tuple(a.first, a.second->GetGUID()) <
                         std::make_tuple(b.first, b.second->GetGUID());
              });

    std::vector<Creature*> casters;
    for (auto const& [order, add] : casting)
        casters.push_back(add);
    AssignKicks(st.kicks, casters, MembersOnMap(bot));
}

// A melee add hitting a player who is no tank, which the duty tank nearest it takes. For a taunt, the
// nearest with its taunt ready, skipping one a taunt just landed on: it still shows its old victim.
Creature* RescueFor(GauntletState const& st, Player* bot, bool forTaunt)
{
    std::vector<Player*> tanks;
    for (Player* member : MembersOnMap(bot))
    {
        PlayerbotAI* memberAI = IsDutyBot(member) && IsTankPlayer(member) ? GET_PLAYERBOT_AI(member) : nullptr;
        if (memberAI && (!forTaunt || ClassTauntReady(memberAI)))
            tanks.push_back(member);
    }

    Creature* pick = nullptr;
    float pickDistance = 0.0f;
    for (Creature* add : EngagedAdds(st, bot))
    {
        if (!IsMeleeYmirjar(add) || (forTaunt && add->HasAuraType(SPELL_AURA_MOD_TAUNT)))
            continue;

        Player* hit = add->GetVictim() ? add->GetVictim()->ToPlayer() : nullptr;
        if (!hit || IsTankPlayer(hit))
            continue;

        Player* elected = nullptr;
        std::tuple<float, ObjectGuid> electedKey;
        for (Player* tank : tanks)
        {
            auto const key = std::make_tuple(tank->GetExactDist2d(add), tank->GetGUID());
            if (!elected || key < electedKey)
            {
                elected = tank;
                electedKey = key;
            }
        }

        float const distance = bot->GetExactDist2d(add);
        if (elected == bot && (!pick || distance < pickDistance))
        {
            pick = add;
            pickDistance = distance;
        }
    }

    return pick;
}

Unit* ResolveGauntletTarget(PlayerbotAI* botAI, GauntletState const& st)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if (!tank && !IsRaidDpsBot(bot))
        return nullptr;

    if (Unit* skull = GetRaidHumanSkull(botAI, &IsGauntletUnit))
    {
        NoteDerived(bot, "ss.gauntlet.target", "skull");
        return skull;
    }

    std::vector<Creature*> const adds = EngagedAdds(st, bot);
    if (tank)
    {
        if (IsDutyBot(bot))
            if (Creature* rescue = RescueFor(st, bot, false))
            {
                NoteDerived(bot, "ss.gauntlet.target", "rescue");
                return rescue;
            }

        // The melee adds this tank holds, else the nearest no other tank holds; the casters keep their range
        Creature* pick = nullptr;
        std::tuple<bool, float, ObjectGuid> pickKey;
        for (Creature* add : adds)
        {
            Player* victim = add->GetVictim() ? add->GetVictim()->ToPlayer() : nullptr;
            if (!IsMeleeYmirjar(add) || (victim && victim != bot && IsTankPlayer(victim)))
                continue;

            auto const key = std::make_tuple(victim != bot, bot->GetExactDist2d(add), add->GetGUID());
            if (!pick || key < pickKey)
            {
                pick = add;
                pickKey = key;
            }
        }

        NoteDerived(bot, "ss.gauntlet.target", pick ? "held" : "none");
        return pick;
    }

    bool const melee = !PlayerbotAI::IsRanged(bot);
    Creature* best = nullptr;
    std::tuple<int8, float, ObjectGuid> bestKey;
    for (Creature* add : adds)
    {
        auto const key = std::make_tuple(GauntletTier(add, melee), bot->GetExactDist2d(add), add->GetGUID());
        if (!best || key < bestKey)
        {
            best = add;
            bestKey = key;
        }
    }

    // The current pick stays while nothing of a better tier fights, or bots flip between adds walking in
    Unit* current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    Creature* held = current ? current->ToCreature() : nullptr;
    if (best && held && held != best && GauntletTier(held, melee) == GauntletTier(best, melee) &&
        std::find(adds.begin(), adds.end(), held) != adds.end())
        best = held;

    NoteDerived(bot, "ss.gauntlet.target", best ? GauntletName(best) : "none");
    return best;
}

// Hazards about to land round the bot; a tank the Warlord hits takes its Whirlwind like any swing
std::vector<HazardCircle> GauntletHazards(GauntletState const& st, Player* bot, float pad)
{
    std::vector<HazardCircle> hazards;
    bool const tank = IsTankPlayer(bot);
    for (ObjectGuid const& guid : st.adds)
    {
        Creature* add = bot->GetMap()->GetCreature(guid);
        if (!add || !add->IsAlive())
            continue;

        if (add->GetEntry() == NPC_SS_WARLORD && !tank && Casting(add, SPELL_SS_WHIRLWIND))
            hazards.emplace_back(add->GetPosition(), WHIRLWIND_RADIUS + pad);
        else if (add->GetEntry() == NPC_SS_FROSTBINDER)
            if (Aura* orb = add->GetAura(SPELL_SS_FROZEN_ORB))
                if (orb->GetDuration() <= ORB_LEAD_MS)
                    hazards.emplace_back(add->GetPosition(), ORB_RADIUS + pad);
    }

    return hazards;
}

char const* GauntletDanger(GauntletState const& st, Player* bot)
{
    bool const tank = IsTankPlayer(bot);
    for (ObjectGuid const& guid : st.adds)
    {
        Creature* add = bot->GetMap()->GetCreature(guid);
        if (!add || !add->IsAlive())
            continue;

        float const distance = bot->GetExactDist2d(add);
        if (add->GetEntry() == NPC_SS_WARLORD && !tank && distance < WHIRLWIND_RADIUS + DANGER_PAD &&
            Casting(add, SPELL_SS_WHIRLWIND))
            return "whirlwind";

        if (add->GetEntry() == NPC_SS_FROSTBINDER && distance < ORB_RADIUS + DANGER_PAD)
            if (Aura* orb = add->GetAura(SPELL_SS_FROZEN_ORB))
                if (orb->GetDuration() <= ORB_LEAD_MS)
                    return "orb";
    }

    return nullptr;
}

void CallGauntletHumanTank(GauntletState& st, Player* sender, uint32 now)
{
    if (st.humanCallRan && getMSTimeDiff(st.humanCallMs, now) < HUMAN_CALL_CHECK_MS)
        return;

    st.humanCallRan = true;
    st.humanCallMs = now;
    std::vector<Creature*> const adds = EngagedAdds(st, sender);
    for (Player* member : MembersOnMap(sender))
    {
        if (IsDutyBot(member) || !IsTankPlayer(member) || member->GetHealthPct() >= SS_TANK_DEFENSIVE_PCT ||
            std::none_of(adds.begin(), adds.end(), [member](Creature* add) { return add->GetVictim() == member; }))
            continue;

        if (uint32 const spellId = ReadyTankDefensiveSpell(member, true, 0, 0))
            CallHumanTankDefensive(sender, member, spellId, 0, "you are low", "ss.gauntlet.humancall");
    }
}
}  // namespace

Creature* GetSsSvalna(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Creature* svalna = SlotCreature(bot, SS_DATA_SVALNA_GUID);
    return svalna && svalna->IsAlive() && bot->GetExactDist2d(svalna) <= LIVE_RANGE ? svalna : nullptr;
}

bool IsSsLive(PlayerbotAI* botAI)
{
    Creature* svalna = GetSsSvalna(botAI);
    if (!svalna)
        return false;

    return svalna->IsInCombat() || AnyCaptainFighting(botAI->GetBot());
}

bool IsSsShielded(PlayerbotAI* botAI, Unit* svalna)
{
    bool const shielded = Shielded(svalna);
    NoteDerived(botAI->GetBot(), "ss.shield", shielded ? "up" : "down");
    return shielded;
}

Unit* GetSsTarget(PlayerbotAI* botAI)
{
    SsState* st = LiveState(botAI);
    if (!st)
        return nullptr;

    TargetMemo& memo = st->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveTarget(botAI, *st);
    }

    return memo.pick;
}

Unit* GetSsCaptainToTaunt(PlayerbotAI* botAI)
{
    SsState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    Creature* captain = st ? OwnedCaptain(*st, bot) : nullptr;
    if (!captain || captain->GetVictim() == bot || !ClassTauntReady(botAI))
        return nullptr;

    return bot->GetExactDist(captain) <= TAUNT_RANGE && bot->IsWithinLOSInMap(captain) ? captain : nullptr;
}

Creature* GetSsSpearToPull(PlayerbotAI* botAI)
{
    SsState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st)
        return nullptr;

    auto const itr = st->spear.find(bot->GetGUID());
    return itr != st->spear.end() ? LiveSpear(bot, itr->second) : nullptr;
}

Item* GetSsSpearToThrow(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* svalna = IsSsLive(botAI) ? GetSsSvalna(botAI) : nullptr;
    if (!svalna || !IsDutyBot(bot) || !bot->HasItemCount(ITEM_SPEAR, 1))
        return nullptr;

    if (!IsFighting(svalna) || !Shielded(svalna))
    {
        NoteDerived(bot, "ss.hurl", "hold");
        return nullptr;
    }

    if (ElectedThrower(bot, svalna) != bot)
    {
        NoteDerived(bot, "ss.hurl", "wait");
        return nullptr;
    }

    bool const inRange = bot->GetExactDist(svalna) <= HURL_RANGE && bot->IsWithinLOSInMap(svalna);
    NoteDerived(bot, "ss.hurl", inRange ? "throw" : "walk");
    for (Item* item : botAI->GetInventoryItems())
        if (item->GetEntry() == ITEM_SPEAR)
            return item;

    return nullptr;
}

Unit* GetSsInterrupt(PlayerbotAI* botAI, char const*& spell)
{
    SsState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    spell = nullptr;
    if (!st || !IsDutyBot(bot))
        return nullptr;

    Creature* arnath = KickFor(st->kicks, bot);
    spell = arnath && ArnathKickable(arnath) ? ReadyKick(bot, arnath, true) : nullptr;
    if (!spell)
    {
        NoteDerived(bot, "ss.interrupt", "none");
        return nullptr;
    }

    NoteDerived(bot, "ss.interrupt", Casting(arnath, SPELL_SS_DOMINATE_MIND) ? "dominate" : "heal");
    return arnath;
}

uint32 GetSsTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    SsState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !bot->IsAlive() || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return 0;

    if (bot->GetHealthPct() >= SS_TANK_DEFENSIVE_PCT || !CaptainHits(bot, bot))
    {
        // Change-latched, so the way out of a window is written too
        if (noteKind)
            NoteDerived(bot, noteKind, "idle");

        return 0;
    }

    // The captains' swings and strikes are physical
    return NextTankDefensive(botAI, bot, noteKind, true);
}

bool IsSsTankDefensiveHeld(PlayerbotAI* botAI)
{
    return LiveState(botAI) && botAI->GetBot()->GetHealthPct() >= SS_TANK_LAST_RESORT_PCT;
}

void TickSsEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    // A wipe or a kill ends the fight: the next one rebuilds every duty. Her death kills the raised captains.
    Creature* slot = SlotCreature(bot, SS_DATA_SVALNA_GUID);
    if (!slot || !slot->IsAlive() || (!slot->IsInCombat() && !AnyCaptainFighting(bot)))
    {
        if (ssStates.Find(bot->GetInstanceId()))
            ssStates.Reset(bot->GetInstanceId());
        return;
    }

    if (!IsSsLive(botAI))
        return;

    SsState& st = ssStates.For(bot->GetInstanceId());
    if (Creature* svalna = GetSsSvalna(botAI))
        if (bot->GetExactDist2d(svalna) <= SHIELD_NOTE_RANGE)
            IsSsShielded(botAI, svalna);

    uint32 const now = getMSTime();
    CallHumanTank(st, bot, now);
    if (st.scanned && getMSTimeDiff(st.scanMs, now) < SCAN_MS)
        return;

    st.scanned = true;
    st.scanMs = now;
    Scan(st, bot);
    std::vector<Player*> const members = MembersOnMap(bot);
    AssignSpears(st, bot, members);
    AssignCaptainTanks(st, bot, members);
    AssignArnathKicks(st, bot, members);
}

bool IsSsGauntletLive(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    return bot && bot->IsAlive() && bot->GetMapId() == ICC_MAP_ID && GauntletLiveUnit(botAI);
}

Unit* GetSsGauntletTarget(PlayerbotAI* botAI)
{
    GauntletState* st = LiveGauntlet(botAI);
    if (!st)
        return nullptr;

    TargetMemo& memo = st->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveGauntletTarget(botAI, *st);
    }

    return memo.pick;
}

Unit* GetSsGauntletTauntTarget(PlayerbotAI* botAI)
{
    GauntletState* st = LiveGauntlet(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return nullptr;

    Creature* add = RescueFor(*st, bot, true);
    bool const ready = add && ClassTauntReady(botAI) && bot->GetExactDist(add) <= TAUNT_RANGE &&
                       bot->IsWithinLOSInMap(add);
    NoteDerived(bot, "ss.gauntlet.taunt", ready ? GauntletName(add) : "none");
    return ready ? add : nullptr;
}

Unit* GetSsGauntletInterrupt(PlayerbotAI* botAI, char const*& spell)
{
    GauntletState* st = LiveGauntlet(botAI);
    Player* bot = botAI->GetBot();
    spell = nullptr;
    if (!st || !IsDutyBot(bot))
        return nullptr;

    Creature* caster = KickFor(st->kicks, bot);
    bool const summon = caster && Casting(caster, SPELL_SS_SUMMON_UNDEAD);
    bool const bolt = caster && CastingShadowBolt(caster);
    spell = summon || bolt ? ReadyKick(bot, caster, true) : nullptr;
    if (!spell)
    {
        NoteDerived(bot, "ss.gauntlet.interrupt", "none");
        return nullptr;
    }

    NoteDerived(bot, "ss.gauntlet.interrupt", summon ? "summon" : "bolt");
    return caster;
}

bool IsSsGauntletInDanger(PlayerbotAI* botAI)
{
    GauntletState* st = LiveGauntlet(botAI);
    if (!st)
        return false;

    Player* bot = botAI->GetBot();
    bool const danger = GauntletDanger(*st, bot) != nullptr;
    if (!danger)
        NoteDerived(bot, "ss.gauntlet.dodge", "none");

    return danger;
}

bool FindSsGauntletDodgeSpot(PlayerbotAI* botAI, Position& spot)
{
    GauntletState* st = LiveGauntlet(botAI);
    Player* bot = botAI->GetBot();
    char const* danger = st ? GauntletDanger(*st, bot) : nullptr;
    if (!danger)
        return false;

    std::vector<HazardCircle> const hazards = GauntletHazards(*st, bot, CLEAR_PAD);
    Position const from = bot->GetPosition();
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    Position found;
    if (target && !PlayerbotAI::IsRanged(bot))
        found = FindNearestPositionClearOfHazards(bot, hazards, DODGE_SWEEP, 2.0f, static_cast<float>(M_PI) / 8.0f,
                                                  &from,
                                                  [target](float x, float y)
                                                  {
                                                      return target->GetExactDist2d(x, y) <=
                                                             target->GetCombatReach() + MELEE_KEEP;
                                                  });

    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
        found = FindNearestPositionClearOfHazards(bot, hazards, DODGE_SWEEP, 2.0f, static_cast<float>(M_PI) / 8.0f,
                                                  &from);

    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
    {
        NoteDerived(bot, "ss.gauntlet.dodge", "blocked");
        return false;
    }

    NoteDerived(bot, "ss.gauntlet.dodge", danger);
    spot = found;
    return true;
}

bool IsSsGauntletReachHeld(PlayerbotAI* botAI)
{
    GauntletState* st = LiveGauntlet(botAI);
    Player* bot = botAI->GetBot();
    if (!st || PlayerbotAI::IsRanged(bot))
        return false;

    uint32 const now = getMSTime();
    auto const memo = st->reachHeld.find(bot->GetGUID());
    if (memo != st->reachHeld.end() && memo->second.first == now)
        return memo->second.second;

    bool held = false;
    if (Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get())
        for (HazardCircle const& hazard : GauntletHazards(*st, bot, CLEAR_PAD))
            held = held || target->GetExactDist2d(hazard.first) < hazard.second;

    st->reachHeld[bot->GetGUID()] = {now, held};
    return held;
}

uint32 GetSsGauntletTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    GauntletState* st = LiveGauntlet(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return 0;

    std::vector<Creature*> const adds = EngagedAdds(*st, bot);
    bool const hit = std::any_of(adds.begin(), adds.end(), [bot](Creature* add) { return add->GetVictim() == bot; });
    if (bot->GetHealthPct() >= SS_TANK_DEFENSIVE_PCT || !hit)
    {
        if (noteKind)
            NoteDerived(bot, noteKind, "idle");

        return 0;
    }

    // The melee Ymirjar and the Huntresses' shots are physical
    return NextTankDefensive(botAI, bot, noteKind, true);
}

bool IsSsGauntletTankDefensiveHeld(PlayerbotAI* botAI)
{
    return LiveGauntlet(botAI) && botAI->GetBot()->GetHealthPct() >= SS_TANK_LAST_RESORT_PCT;
}

void TickSsGauntletEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    // Between packs nothing fights the raid: the next pack rebuilds the scan and the kicks
    Creature* unit = GauntletLiveUnit(botAI);
    if (!unit)
    {
        if (gauntletStates.Find(bot->GetInstanceId()))
            gauntletStates.Reset(bot->GetInstanceId());
        return;
    }

    IccMarkPull(bot, unit, "svalna-gauntlet");
    if (!bot->IsAlive())
        return;

    GauntletState& st = gauntletStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    ScanGauntlet(st, unit, bot, now);
    CallGauntletHumanTank(st, bot, now);
}

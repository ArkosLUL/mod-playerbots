#include "ToCHelpers_Jaraxxus.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <unordered_set>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Map.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "Unit.h"

using namespace EncounterHelpers;

namespace TrialOfTheCrusaderHelpers
{

namespace
{

// The arena floor reaches ~80 yd from ARENA_CENTER. A bot further out than scan radius minus that
// would miss part of the floor, so it reads the last scan instead of refreshing it.
constexpr float JARAXXUS_ARENA_REACH = 80.0f;
constexpr float JARAXXUS_FLAME_CARRIER_RAID_SCAN = 40.0f;
constexpr float JARAXXUS_MELEE_INTERRUPT_RANGE = 5.0f;

std::vector<uint32> const JARAXXUS_SCAN_ENTRIES = {
    static_cast<uint32>(ToCNpcs::NPC_JARAXXUS),        static_cast<uint32>(ToCNpcs::NPC_NETHER_PORTAL),
    static_cast<uint32>(ToCNpcs::NPC_INFERNAL_VOLCANO), static_cast<uint32>(ToCNpcs::NPC_MISTRESS_OF_PAIN),
    static_cast<uint32>(ToCNpcs::NPC_FEL_INFERNAL),     static_cast<uint32>(ToCNpcs::NPC_LEGION_FLAME),
};

// Guids, never pointers: a unit can despawn between two refreshes.
struct JaraxxusScan
{
    uint32 stampMs = 0;
    bool valid = false;  // 0 is a real getMSTime value
    std::vector<ObjectGuid> bosses;
    std::vector<ObjectGuid> portals;
    std::vector<ObjectGuid> volcanoes;
    std::vector<ObjectGuid> mistresses;
    std::vector<ObjectGuid> infernals;
    std::vector<Position> flames;
};

struct JaraxxusState
{
    JaraxxusScan scan;
    RaidObs::ObsValue<uint32> netherPower{"jaraxxus.netherpower"};
    RaidObs::ObsValue<ObjectGuid> focus{"jaraxxus.focus"};
    RaidObs::ObsGuidMap<ObjectGuid> addTank{"jaraxxus.addtank"};
    std::unordered_set<ObjectGuid> rtiOwners;
};

RaidInstanceState<JaraxxusState> jaraxxusStates;

JaraxxusState* StateFor(Player* bot)
{
    if (!bot || !bot->IsInWorld() || bot->GetMapId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return nullptr;

    return &jaraxxusStates.For(bot->GetInstanceId());
}

JaraxxusScan const& ScanOf(Player* bot, JaraxxusState& state)
{
    JaraxxusScan& scan = state.scan;
    uint32 const now = getMSTime();
    if (scan.valid && getMSTimeDiff(scan.stampMs, now) < JARAXXUS_SCAN_MS)
        return scan;

    if (bot->GetExactDist(ARENA_CENTER) > JARAXXUS_SCAN_RADIUS - JARAXXUS_ARENA_REACH)
        return scan;

    scan.stampMs = now;
    scan.valid = true;
    scan.bosses.clear();
    scan.portals.clear();
    scan.volcanoes.clear();
    scan.mistresses.clear();
    scan.infernals.clear();
    scan.flames.clear();

    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, JARAXXUS_SCAN_ENTRIES, JARAXXUS_SCAN_RADIUS);
    for (Creature* creature : creatures)
    {
        if (!creature || !creature->IsAlive())
            continue;

        switch (static_cast<ToCNpcs>(creature->GetEntry()))
        {
            case ToCNpcs::NPC_JARAXXUS:
                scan.bosses.push_back(creature->GetGUID());
                break;
            case ToCNpcs::NPC_NETHER_PORTAL:
                scan.portals.push_back(creature->GetGUID());
                break;
            case ToCNpcs::NPC_INFERNAL_VOLCANO:
                scan.volcanoes.push_back(creature->GetGUID());
                break;
            case ToCNpcs::NPC_MISTRESS_OF_PAIN:
                scan.mistresses.push_back(creature->GetGUID());
                break;
            case ToCNpcs::NPC_FEL_INFERNAL:
                scan.infernals.push_back(creature->GetGUID());
                break;
            case ToCNpcs::NPC_LEGION_FLAME:
                scan.flames.push_back(creature->GetPosition());
                break;
            default:
                break;
        }
    }

    for (std::vector<ObjectGuid>* guids :
         {&scan.bosses, &scan.portals, &scan.volcanoes, &scan.mistresses, &scan.infernals})
        std::sort(guids->begin(), guids->end());

    return scan;
}

JaraxxusScan const* ScanFor(Player* bot)
{
    JaraxxusState* state = StateFor(bot);
    return state ? &ScanOf(bot, *state) : nullptr;
}

Creature* ResolveAlive(Player* bot, ObjectGuid guid)
{
    Map* map = bot->FindMap();
    Creature* creature = map ? map->GetCreature(guid) : nullptr;
    return creature && creature->IsAlive() ? creature : nullptr;
}

Creature* FirstAlive(Player* bot, std::vector<ObjectGuid> const& guids)
{
    for (ObjectGuid guid : guids)
        if (Creature* creature = ResolveAlive(bot, guid))
            return creature;

    return nullptr;
}

// Normal portals and volcanoes are NOT_SELECTABLE and gone after 15 s, heroic ones have to be killed.
Creature* FirstSelectable(Player* bot, std::vector<ObjectGuid> const& guids)
{
    for (ObjectGuid guid : guids)
    {
        Creature* creature = ResolveAlive(bot, guid);
        if (creature && !creature->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            return creature;
    }

    return nullptr;
}

Creature* SecondAlive(Player* bot, std::vector<ObjectGuid> const& guids)
{
    bool skipped = false;
    for (ObjectGuid guid : guids)
    {
        Creature* creature = ResolveAlive(bot, guid);
        if (!creature)
            continue;

        if (skipped)
            return creature;

        skipped = true;
    }

    return nullptr;
}

Unit* FindJaraxxus(Player* bot)
{
    JaraxxusScan const* scan = ScanFor(bot);
    if (!scan)
        return nullptr;

    for (ObjectGuid guid : scan->bosses)
    {
        Creature* boss = ResolveAlive(bot, guid);
        if (boss && !boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
            return boss;
    }

    return nullptr;
}

std::vector<Player*> AliveMembersOnMap(Player* bot)
{
    std::vector<Player*> members;
    Group* group = bot->GetGroup();
    if (!group)
    {
        if (bot->IsAlive())
            members.push_back(bot);

        return members;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->IsInMap(bot))
            members.push_back(member);
    }

    return members;
}

Unit* IncinerateFleshTargetFor(Player* bot)
{
    if (!StateFor(bot))
        return nullptr;

    Player* target = nullptr;
    for (Player* member : AliveMembersOnMap(bot))
        if (HasIncinerateFlesh(member) && (!target || member->GetGUID() < target->GetGUID()))
            target = member;

    return target;
}

bool IsNonHealingRemover(Player* player)
{
    switch (player->getClass())
    {
        case CLASS_MAGE:
            return true;
        case CLASS_PRIEST:
        case CLASS_SHAMAN:
            return !PlayerbotAI::IsHeal(player);
        default:
            return false;
    }
}

// One on another player first, tanks included: Fel Streak can land on any of them and its 50,000
// threat keeps it there. Then the one already on this bot, so a tank doesn't drop its own for another.
Unit* PickInfernal(Player* bot, JaraxxusScan const& scan)
{
    Unit* lowest = nullptr;
    Unit* onBot = nullptr;
    for (ObjectGuid guid : scan.infernals)
    {
        Creature* infernal = ResolveAlive(bot, guid);
        if (!infernal)
            continue;

        if (!lowest)
            lowest = infernal;

        Unit* victim = infernal->GetVictim();
        if (victim && victim != bot && victim->ToPlayer())
            return infernal;

        if (!onBot && victim == bot)
            onBot = infernal;
    }

    return onBot ? onBot : lowest;
}

bool AnyLivingAssistTankOne(Player* bot)
{
    for (Player* member : AliveMembersOnMap(bot))
        if (PlayerbotAI::IsAssistTankOfIndex(member, 1, true))
            return true;

    return false;
}

bool InInterruptRange(Player* bot, Unit* boss, SpellInfo const* spellInfo)
{
    float const maxRange = bot->GetSpellMaxRangeForTarget(boss, spellInfo);
    if (maxRange <= JARAXXUS_MELEE_INTERRUPT_RANGE)
        return bot->IsWithinMeleeRange(boss);

    return bot->IsWithinCombatRange(boss, maxRange);
}

// CanCastSpell probes with TRIGGERED_IGNORE_POWER_AND_REAGENT_COST, so energy, rage and runic power
// get checked here. POWER_HEALTH is -2 and Powers is signed, hence the cast.
bool CanPayFor(Player* bot, SpellInfo const* spellInfo)
{
    if (spellInfo->PowerType >= static_cast<uint32>(MAX_POWERS))
        return true;

    return static_cast<int32>(bot->GetPower(Powers(spellInfo->PowerType))) >=
           spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
}

// the sweep's answer when nothing inside its radius is clear
bool IsEmptyPosition(Position const& position) { return !position.GetPositionX() && !position.GetPositionY(); }

float DistanceToSegment2d(Position const& point, Position const& from, Position const& to)
{
    float const dx = to.GetPositionX() - from.GetPositionX();
    float const dy = to.GetPositionY() - from.GetPositionY();
    float const lengthSq = dx * dx + dy * dy;

    float t = 0.0f;
    if (lengthSq > 0.0f)
    {
        t = ((point.GetPositionX() - from.GetPositionX()) * dx + (point.GetPositionY() - from.GetPositionY()) * dy) /
            lengthSq;
        t = std::clamp(t, 0.0f, 1.0f);
    }

    return point.GetExactDist2d(from.GetPositionX() + t * dx, from.GetPositionY() + t * dy);
}

// Only flames that can reach a spot inside the sweep radius
void AddFlameHazards(Player* bot, JaraxxusScan const& scan, std::vector<HazardCircle>& hazards)
{
    for (Position const& flame : scan.flames)
        if (bot->GetExactDist2d(flame) <= JARAXXUS_LEGION_FLAME_SEARCH + JARAXXUS_LEGION_FLAME_CLEAR)
            hazards.emplace_back(flame, JARAXXUS_LEGION_FLAME_CLEAR);
}

bool DeriveCarrierSpot(Player* bot, JaraxxusScan const& scan, float heading, Position& spot)
{
    std::vector<HazardCircle> flames;
    AddFlameHazards(bot, scan, flames);
    // next flame lands where the carrier stands. Also keeps the list non-empty before the first
    // flame, since the sweep answers nothing for an empty one
    flames.emplace_back(bot->GetPosition(), JARAXXUS_LEGION_FLAME_CLEAR);

    Position const preferNear(bot->GetPositionX() + std::cos(heading) * 2.0f * JARAXXUS_FLAME_CARRIER_MIN_LEG,
                              bot->GetPositionY() + std::sin(heading) * 2.0f * JARAXXUS_FLAME_CARRIER_MIN_LEG,
                              bot->GetPositionZ());
    auto const fullLeg = [bot](float x, float y)
    { return bot->GetExactDist2d(x, y) >= JARAXXUS_FLAME_CARRIER_MIN_LEG; };

    HazardSweepCache cache;
    auto const sweep = [&](std::vector<HazardCircle> const& hazards)
    {
        return FindNearestPositionClearOfHazards(bot, hazards, JARAXXUS_LEGION_FLAME_SEARCH, 2.0f,
                                                 static_cast<float>(M_PI) / 8.0f, &preferNear, fullLeg, &cache);
    };

    Unit* boss = FindJaraxxus(bot);
    char const* branch = "carrier";

    // a tank carrier drags him along, so it can't keep clear of him or of the melee on him
    if (boss && boss->GetVictim() == bot)
    {
        spot = sweep(flames);
        branch = "tank";
    }
    else
    {
        std::vector<HazardCircle> hazards = flames;
        for (Player* member : AliveMembersOnMap(bot))
            if (member != bot && bot->GetExactDist2d(member) <= JARAXXUS_FLAME_CARRIER_RAID_SCAN)
                hazards.emplace_back(member->GetPosition(), JARAXXUS_FLAME_CARRIER_RAID_CLEAR);

        if (boss)
            hazards.emplace_back(boss->GetPosition(), JARAXXUS_FLAME_CARRIER_BOSS_CLEAR);

        spot = sweep(hazards);
        if (IsEmptyPosition(spot))
        {
            spot = sweep(flames);
            branch = "relaxed";
        }
    }

    bool const found = !IsEmptyPosition(spot);
    RaidObs::NoteDerived(bot, "jaraxxus.flame", found ? branch : "none");
    return found;
}

// A bot already inside CLEAR of a flame may walk on as long as it gets no nearer to it
bool FlameCrossesPath(JaraxxusScan const& scan, Position const& from, Position const& to)
{
    for (Position const& flame : scan.flames)
    {
        float const limit = std::min(JARAXXUS_LEGION_FLAME_CLEAR, flame.GetExactDist2d(from) - 0.1f);
        if (DistanceToSegment2d(flame, from, to) < limit)
            return true;
    }

    return false;
}

bool DeriveDodgeSpot(Player* bot, JaraxxusScan const& scan, Position& spot)
{
    std::vector<HazardCircle> hazards;
    AddFlameHazards(bot, scan, hazards);

    Position bossSpot;
    Position const* preferNear = nullptr;
    if (!PlayerbotAI::IsRanged(bot))
    {
        if (Unit* boss = FindJaraxxus(bot))
        {
            bossSpot = boss->GetPosition();
            preferNear = &bossSpot;
        }
    }

    spot = FindNearestPositionClearOfHazards(bot, hazards, JARAXXUS_LEGION_FLAME_SEARCH, 2.0f,
                                             static_cast<float>(M_PI) / 8.0f, preferNear);
    RaidObs::NoteDerived(bot, "jaraxxus.flame", IsEmptyPosition(spot) ? "none" : "dodge");
    return !IsEmptyPosition(spot);
}

}  // namespace

Unit* GetJaraxxus(PlayerbotAI* botAI) { return botAI ? FindJaraxxus(botAI->GetBot()) : nullptr; }

Unit* GetJaraxxusInIntro(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    JaraxxusScan const* scan = ScanFor(bot);
    if (!scan)
        return nullptr;

    for (ObjectGuid guid : scan->bosses)
    {
        Creature* boss = ResolveAlive(bot, guid);
        if (boss && boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE) && !boss->IsInCombat())
            return boss;
    }

    return nullptr;
}

bool JaraxxusHasNetherPower(Unit* jaraxxus)
{
    if (!jaraxxus)
        return false;

    return jaraxxus->HasAura(SPELL_NETHER_POWER_10N) ||
           jaraxxus->HasAura(SPELL_NETHER_POWER_10H) ||
           jaraxxus->HasAura(SPELL_NETHER_POWER_25N) ||
           jaraxxus->HasAura(SPELL_NETHER_POWER_25H);
}

uint32 JaraxxusNetherPowerStacks(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    JaraxxusState* state = StateFor(bot);
    if (!state)
        return 0;

    uint32 stacks = 0;
    if (Unit* boss = FindJaraxxus(bot))
    {
        for (uint32 spellId :
             {SPELL_NETHER_POWER_10N, SPELL_NETHER_POWER_25N, SPELL_NETHER_POWER_10H, SPELL_NETHER_POWER_25H})
        {
            if (Aura* aura = boss->GetAura(spellId))
            {
                stacks = aura->GetStackAmount();
                break;
            }
        }
    }

    state->netherPower = stacks;
    return stacks;
}

bool JaraxxusIsNetherPowerRemover(Player* bot)
{
    if (!StateFor(bot))
        return false;

    if (IsNonHealingRemover(bot))
        return true;

    uint8 const botClass = bot->getClass();
    if (botClass != CLASS_PRIEST && botClass != CLASS_SHAMAN)
        return false;

    // a healer only fills in when nobody else can, and never while Incinerate Flesh needs its heals
    for (Player* member : AliveMembersOnMap(bot))
        if (member != bot && IsNonHealingRemover(member))
            return false;

    return !IncinerateFleshTargetFor(bot);
}

bool HasIncinerateFlesh(Unit* unit)
{
    return unit && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_INCINERATE_FLESH, unit));
}

Unit* GetIncinerateFleshTarget(PlayerbotAI* botAI)
{
    return botAI ? IncinerateFleshTargetFor(botAI->GetBot()) : nullptr;
}

bool IsLegionFlameCarrier(Unit* unit)
{
    return unit && (unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LEGION_FLAME, unit)) ||
                    unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LEGION_FLAME_TRAIL, unit)));
}

bool HasMistressKiss(Unit* unit)
{
    return unit && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_MISTRESS_KISS, unit));
}

bool IsJaraxxusAdd(Unit* unit)
{
    Creature* creature = unit ? unit->ToCreature() : nullptr;
    if (!creature)
        return false;

    switch (static_cast<ToCNpcs>(creature->GetEntry()))
    {
        case ToCNpcs::NPC_NETHER_PORTAL:
        case ToCNpcs::NPC_INFERNAL_VOLCANO:
        case ToCNpcs::NPC_MISTRESS_OF_PAIN:
        case ToCNpcs::NPC_FEL_INFERNAL:
            return true;
        default:
            return false;
    }
}

bool JaraxxusAnyAddAlive(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    JaraxxusScan const* scan = ScanFor(bot);
    if (!scan)
        return false;

    return FirstAlive(bot, scan->mistresses) || FirstAlive(bot, scan->infernals) ||
           FirstSelectable(bot, scan->portals) || FirstSelectable(bot, scan->volcanoes);
}

Unit* GetJaraxxusFocusAdd(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    JaraxxusState* state = StateFor(bot);
    if (!state)
        return nullptr;

    JaraxxusScan const& scan = ScanOf(bot, *state);

    Unit* focus = FirstSelectable(bot, scan.portals);
    if (!focus)
        focus = FirstSelectable(bot, scan.volcanoes);
    if (!focus)
        focus = FirstAlive(bot, scan.mistresses);
    if (!focus)
        focus = FirstAlive(bot, scan.infernals);

    state->focus = focus ? focus->GetGUID() : ObjectGuid::Empty;
    return focus;
}

Unit* GetJaraxxusAssistTankAdd(PlayerbotAI* botAI, uint8 index)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    JaraxxusState* state = StateFor(bot);
    if (!state)
        return nullptr;

    JaraxxusScan const& scan = ScanOf(bot, *state);

    // a tank that died or left stops calling this, so its entry would outlive it in the trace
    std::vector<Player*> const members = AliveMembersOnMap(bot);
    std::vector<ObjectGuid> gone;
    for (auto const& entry : state->addTank)
    {
        ObjectGuid const tank = entry.first;
        if (std::none_of(members.begin(), members.end(), [tank](Player* member) { return member->GetGUID() == tank; }))
            gone.push_back(tank);
    }
    for (ObjectGuid tank : gone)
        state->addTank.erase(tank);

    Unit* add = nullptr;
    if (index == 0)
    {
        add = FirstAlive(bot, scan.mistresses);
        if (!add && !AnyLivingAssistTankOne(bot))
            add = PickInfernal(bot, scan);
    }
    else if (index == 1)
    {
        add = PickInfernal(bot, scan);
        if (!add)
            add = SecondAlive(bot, scan.mistresses);
    }

    if (add)
        state->addTank.Set(bot->GetGUID(), add->GetGUID());
    else
        state->addTank.erase(bot->GetGUID());

    return add;
}

bool JaraxxusIsFelFireballCasting(Unit* boss)
{
    Spell* spell = boss ? boss->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
    return spell && spell->GetSpellInfo() &&
           spell->GetSpellInfo()->Id == sSpellMgr->GetSpellIdForDifficulty(SPELL_FEL_FIREBALL, boss);
}

char const* JaraxxusReadyInterrupt(Player* bot, Unit* boss)
{
    if (!bot || !boss)
        return nullptr;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI || bot->HasUnitState(UNIT_STATE_CASTING))
        return nullptr;

    // He is immune to silence, so Silencing Shot and Silence never land. Spell Lock is left out
    // because CanCastSpell answers any pet spell true without looking at its cooldown.
    static char const* const interrupts[] = {"kick",         "pummel",       "shield bash",
                                             "mind freeze",  "counterspell", "wind shear"};

    for (char const* interrupt : interrupts)
    {
        uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", interrupt)->Get();
        SpellInfo const* spellInfo = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!spellInfo || !CanPayFor(bot, spellInfo) || !InInterruptRange(bot, boss, spellInfo))
            continue;

        if (botAI->CanCastSpell(spellId, boss))
            return interrupt;
    }

    return nullptr;
}

bool JaraxxusIsFelFireballInterrupter(Player* bot, Unit* boss)
{
    if (!StateFor(bot) || !boss)
        return false;

    bool interrupter = JaraxxusIsFelFireballCasting(boss) && JaraxxusReadyInterrupt(bot, boss);
    if (interrupter)
    {
        for (Player* member : AliveMembersOnMap(bot))
        {
            if (member->GetGUID() < bot->GetGUID() && JaraxxusReadyInterrupt(member, boss))
            {
                interrupter = false;
                break;
            }
        }
    }

    RaidObs::NoteDerived(bot, "jaraxxus.interrupter", interrupter ? "1" : "0");
    return interrupter;
}

bool JaraxxusSpellHasCastTime(PlayerbotAI* botAI, std::string const& spell, bool withChannels)
{
    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info)
        return false;

    if (info->IsChanneled())
        return withChannels;

    return info->CalcCastTime(botAI->GetBot()) > 0;
}

std::vector<Player*> GetJaraxxusPinnedCasters(Player* bot)
{
    std::vector<Player*> pinned;
    if (!StateFor(bot))
        return pinned;

    for (Player* member : AliveMembersOnMap(bot))
    {
        if (!member->HasUnitState(UNIT_STATE_CASTING) || !GET_PLAYERBOT_AI(member))
            continue;

        if (HasMistressKiss(member) || IsLegionFlameCarrier(member))
            pinned.push_back(member);
    }

    return pinned;
}

JaraxxusFlameRole GetJaraxxusFlameRole(Player* bot)
{
    JaraxxusScan const* scan = ScanFor(bot);
    if (!scan || !bot->IsAlive())
        return JaraxxusFlameRole::None;

    if (IsLegionFlameCarrier(bot))
        return JaraxxusFlameRole::Carrier;

    for (Position const& flame : scan->flames)
        if (bot->GetExactDist2d(flame) < JARAXXUS_LEGION_FLAME_TRIGGER)
            return JaraxxusFlameRole::Dodge;

    return JaraxxusFlameRole::None;
}

float GetLegionFlameCarrierHeading(Player* bot)
{
    if (!bot)
        return 0.0f;

    Unit* boss = FindJaraxxus(bot);
    Position const from = boss ? boss->GetPosition() : ARENA_CENTER;
    return from.GetAngle(bot);
}

bool DeriveLegionFlameSpot(Player* bot, JaraxxusFlameRole role, float heading, Position& spot)
{
    JaraxxusScan const* scan = ScanFor(bot);
    if (!scan)
        return false;

    switch (role)
    {
        case JaraxxusFlameRole::Carrier:
            return DeriveCarrierSpot(bot, *scan, heading, spot);
        case JaraxxusFlameRole::Dodge:
            return DeriveDodgeSpot(bot, *scan, spot);
        default:
            return false;
    }
}

bool IsLegionFlameSpotClear(PlayerbotAI* botAI, Position const& spot)
{
    JaraxxusScan const* scan = botAI ? ScanFor(botAI->GetBot()) : nullptr;
    if (!scan)
        return true;

    for (Position const& flame : scan->flames)
        if (flame.GetExactDist2d(spot) < JARAXXUS_LEGION_FLAME_CLEAR)
            return false;

    return true;
}

bool LegionFlameCrossesPath(Player* bot, Position const& to)
{
    JaraxxusScan const* scan = ScanFor(bot);
    return scan && FlameCrossesPath(*scan, bot->GetPosition(), to);
}

bool DeriveLegionFlameDetour(Player* bot, Position const& anchor, float maxStep, Position& spot)
{
    JaraxxusScan const* scan = ScanFor(bot);
    if (!scan)
        return false;

    std::vector<HazardCircle> hazards;
    AddFlameHazards(bot, *scan, hazards);
    if (hazards.empty())
        return false;

    Position const from = bot->GetPosition();
    auto const reachable = [scan, &from](float x, float y)
    { return !FlameCrossesPath(*scan, from, Position(x, y, from.GetPositionZ())); };

    spot = FindNearestPositionClearOfHazards(bot, hazards, maxStep, 2.0f, static_cast<float>(M_PI) / 8.0f, &anchor,
                                             reachable);
    return !IsEmptyPosition(spot);
}

bool JaraxxusAnyLegionFlame(PlayerbotAI* botAI)
{
    JaraxxusScan const* scan = botAI ? ScanFor(botAI->GetBot()) : nullptr;
    return scan && !scan->flames.empty();
}

void JaraxxusClaimRti(Player* bot)
{
    if (JaraxxusState* state = StateFor(bot))
        state->rtiOwners.insert(bot->GetGUID());
}

bool JaraxxusOwnsRti(Player* bot)
{
    JaraxxusState* state = StateFor(bot);
    return state && state->rtiOwners.count(bot->GetGUID());
}

void JaraxxusReleaseRti(Player* bot)
{
    if (JaraxxusState* state = StateFor(bot))
        state->rtiOwners.erase(bot->GetGUID());
}

}

#include "ToCHelpers_TwinValkyr.h"

#include "BossAuraTriggers.h"
#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Defensive.h"
#include "ToCHelpers_Shared.h"
#include "Unit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

using namespace EncounterHelpers;

namespace TrialOfTheCrusaderHelpers
{
namespace
{

// The script swaps an orb to this model the moment it explodes and despawns it 1.5 s later. The
// blast is instant, so what's left is harmless.
constexpr uint32 TWIN_ORB_SPENT_DISPLAY_ID = 11686;
// Orbs stay inside a 47 yd circle round ARENA_CENTER and the portals sit 31.8 yd out, so this reaches
// all of them from a twin held at the door, about 48 yd out
constexpr float TWIN_ARENA_SCAN_RADIUS = 100.0f;
constexpr float TWIN_COUNTERSPELL_RANGE = 30.0f;
constexpr float TWIN_WIND_SHEAR_RANGE = 25.0f;
constexpr float TWIN_SPELL_LOCK_RANGE = 30.0f;

// The entrance tunnel. The traces read its floor as a flat 395.2, and every stand here is 51+ yd from
// ARENA_CENTER, where no orb gets.
constexpr float TWIN_DOOR_Z = 395.2f;

struct DoorPoint
{
    float x;
    float y;
};

// 5 yd apart, so cleaves hit both twins
constexpr DoorPoint FJOLA_DOOR{560.5f, 88.2f};
constexpr DoorPoint EYDIS_DOOR{565.5f, 88.2f};
constexpr DoorPoint LONE_DOOR{563.5f, 88.2f};
// Front first. A twin hovers about 6 yd over her tank, so melee reach her from about 10 yd in 2D.
constexpr DoorPoint MELEE_SLOTS[] = {{563.25f, 87.0f}, {561.5f, 87.0f}, {565.0f, 87.0f},
                                     {563.25f, 85.5f}, {561.5f, 85.5f}, {565.0f, 85.5f}};
constexpr DoorPoint HEAL_SLOTS[] = {{563.25f, 84.0f}, {561.5f, 84.0f}, {565.0f, 84.0f},
                                    {563.25f, 82.5f}, {561.5f, 82.5f}, {565.0f, 82.5f}};
constexpr DoorPoint RANGED_SLOTS[] = {{563.25f, 81.0f}, {561.5f, 81.0f}, {565.0f, 81.0f},
                                      {563.25f, 79.8f}, {561.5f, 79.8f}, {565.0f, 79.8f}};
constexpr uint32 TWIN_DOOR_DEAL_MS = 1000;

constexpr float TWIN_TAUNT_RANGE = 30.0f;
constexpr uint32 TWIN_FETCH_STALL_MS = 2000;
// less than this closer isn't closing
constexpr float TWIN_FETCH_CLOSING = 1.0f;

constexpr uint8 TWIN_LUST_SPECIALS = 3;
constexpr uint32 TWIN_NORMAL_LUST_MS = 20000;
constexpr uint32 TWIN_LUST_GRACE_MS = 10000;

constexpr uint32 SPELL_AURA_MASTERY = 31821;
constexpr uint32 SPELL_BLOODLUST = 2825;
constexpr uint32 SPELL_HEROISM = 32182;
constexpr uint32 SPELL_SATED = 57724;
constexpr uint32 SPELL_EXHAUSTION = 57723;

std::vector<uint32> const TWIN_ARENA_ENTRIES = {NPC_CONCENTRATED_LIGHT, NPC_CONCENTRATED_DARK,
                                                static_cast<uint32>(ToCNpcs::NPC_LIGHT_ESSENCE),
                                                static_cast<uint32>(ToCNpcs::NPC_DARK_ESSENCE)};

struct TwinOrb
{
    ObjectGuid guid;
    TwinColour colour = TwinColour::None;
    Position position;
    Position destination;
};

struct TwinPortal
{
    ObjectGuid guid;
    TwinColour colour = TwinColour::None;
    Position position;
};

// How long a twin has been no closer to her door spot
struct TwinApproach
{
    bool valid = false;
    float mark = 0.0f;
    uint32 sinceMs = 0;
};

// Reset whenever neither twin is in combat
struct TwinPull
{
    bool engaged = false;
    uint32 startMs = 0;
    // a Vortex, shield or Pact up at the last read
    bool special = false;
    uint8 specials = 0;
    bool eydisPact = false;
    TwinColour lastVortex = TwinColour::None;
    // latched at a normal Light Vortex's start
    bool raidHold = false;
    bool lustOpen = false;
    uint32 lustOpenMs = 0;
    // Fjola, Eydis
    std::array<TwinApproach, 2> approach{};
};

// One read per instance per ms: every bot asks several of these a tick, and the arena sweep walks a
// 100 yd grid. Guids only, a pointer can die before the next read.
struct TwinValkyrState
{
    RaidObs::ObsValue<TwinColour> shield{"tv.shield"};

    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value
    ObjectGuid fjola;
    ObjectGuid eydis;
    ObjectGuid pactTwin;
    ObjectGuid shieldedTwin;
    TwinColour vortex = TwinColour::None;
    std::vector<TwinOrb> orbs;
    std::vector<TwinPortal> portals;
    // Resolved from the group of the bot that refreshed; every bot in a raid instance shares it
    ObjectGuid tankGroup;
    std::vector<ObjectGuid> tanks;
    TwinPull pull;

    bool doorDealValid = false;
    uint32 doorDealMs = 0;
    ObjectGuid doorGroup;
    std::unordered_map<ObjectGuid, TwinDoorStand> doorStands;
    std::unordered_map<ObjectGuid, bool> doorParked;
};

RaidInstanceState<TwinValkyrState> twinStates;

enum class OrbRule : uint8
{
    Wrong,
    Splash
};

struct OrbHazard
{
    Position point;
    OrbRule rule;
};

Creature* AliveTwin(Map* map, InstanceScript* instance, uint32 type)
{
    ObjectGuid const guid = instance->GetGuidData(type);
    if (guid.IsEmpty())
        return nullptr;

    Creature* twin = map->GetCreature(guid);
    return twin && twin->IsAlive() ? twin : nullptr;
}

ObjectGuid GuidOf(Creature* creature) { return creature ? creature->GetGUID() : ObjectGuid::Empty; }

// From a twin rather than the bot: every bot reads these lists until the next ms, and a bot at the
// gate would miss the far side of the circle
void ScanArena(Creature* twin, TwinValkyrState& state)
{
    state.orbs.clear();
    state.portals.clear();

    std::list<Creature*> creatures;
    twin->GetCreatureListWithEntryInGrid(creatures, TWIN_ARENA_ENTRIES, TWIN_ARENA_SCAN_RADIUS);
    for (Creature* creature : creatures)
    {
        if (!creature->IsAlive())
            continue;

        uint32 const entry = creature->GetEntry();
        if (entry == static_cast<uint32>(ToCNpcs::NPC_LIGHT_ESSENCE) ||
            entry == static_cast<uint32>(ToCNpcs::NPC_DARK_ESSENCE))
        {
            TwinColour const colour =
                entry == static_cast<uint32>(ToCNpcs::NPC_LIGHT_ESSENCE) ? TwinColour::Light : TwinColour::Dark;
            state.portals.push_back({creature->GetGUID(), colour, creature->GetPosition()});
            continue;
        }

        if (creature->GetDisplayId() == TWIN_ORB_SPENT_DISPLAY_ID)
            continue;

        TwinOrb orb;
        orb.guid = creature->GetGUID();
        orb.colour = entry == NPC_CONCENTRATED_LIGHT ? TwinColour::Light : TwinColour::Dark;
        orb.position = creature->GetPosition();

        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        orb.destination = creature->GetMotionMaster()->GetDestination(x, y, z) ? Position(x, y, z) : orb.position;

        state.orbs.push_back(orb);
    }
}

// Main tank, then the assists with assistants first, as GetGroupMainTank and GetGroupAssistTank rank
// them. IsTank reads the tank strategy, which can drop out for a tick, so the spec counts as well.
std::vector<ObjectGuid> ResolveTwinTanks(Group* group)
{
    auto const isTank = [](Player* member)
    { return PlayerbotAI::IsTank(member) || PlayerbotAI::IsTank(member, true); };

    ObjectGuid flagged;
    for (Group::MemberSlot const& slot : group->GetMemberSlots())
    {
        if (slot.flags & MEMBER_FLAG_MAINTANK)
        {
            flagged = slot.guid;
            break;
        }
    }

    Player* mainTank = nullptr;
    std::vector<Player*> assistants;
    std::vector<Player*> others;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive())
            continue;

        if (member->GetGUID() == flagged)
        {
            mainTank = member;
            continue;
        }

        if (!isTank(member))
            continue;

        if (flagged.IsEmpty() && !mainTank)
            mainTank = member;
        else
            (group->IsAssistant(member->GetGUID()) ? assistants : others).push_back(member);
    }

    std::vector<ObjectGuid> tanks;
    if (mainTank)
        tanks.push_back(mainTank->GetGUID());

    for (std::vector<Player*> const* list : {&assistants, &others})
        for (Player* tank : *list)
            if (tanks.size() < 2)
                tanks.push_back(tank->GetGUID());

    return tanks;
}

Position DoorPosition(DoorPoint point) { return Position(point.x, point.y, TWIN_DOOR_Z); }

Position DoorOf(TwinColour colour, size_t tankCount)
{
    if (tankCount == 1)
        return DoorPosition(LONE_DOOR);

    return DoorPosition(colour == TwinColour::Light ? FJOLA_DOOR : EYDIS_DOOR);
}

// The raid cover deal only hands Aura Mastery for a fire hit to a bot paladin running its own Fire
// Resistance Aura
bool FireMasteryReady(Group* group, Map* map)
{
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsInWorld() && member->IsAlive() && member->FindMap() == map &&
            member->getClass() == CLASS_PALADIN && GET_PLAYERBOT_AI(member) &&
            member->GetAuraOfRankedSpell(SPELL_FIRE_RESISTANCE_AURA_RANK_1, member->GetGUID()) &&
            member->HasSpell(SPELL_AURA_MASTERY) && !member->HasSpellCooldown(SPELL_AURA_MASTERY))
            return true;
    }

    return false;
}

void TrackApproach(TwinApproach& approach, Creature* twin, Position const& door, uint32 now)
{
    if (!twin)
    {
        approach = TwinApproach();
        return;
    }

    float const distance = twin->GetExactDist2d(door);
    if (!approach.valid || distance < approach.mark - TWIN_FETCH_CLOSING)
    {
        approach.valid = true;
        approach.mark = distance;
        approach.sinceMs = now;
        return;
    }

    // walking off keeps the clock, a close is measured from the furthest point
    approach.mark = std::max(approach.mark, distance);
}

bool AtDoor(TwinValkyrState const& state, Creature* twin)
{
    Unit* victim = twin ? twin->GetVictim() : nullptr;
    if (!victim || std::find(state.tanks.begin(), state.tanks.end(), victim->GetGUID()) == state.tanks.end())
        return false;

    return twin->GetExactDist2d(DoorOf(TwinColourOf(twin), state.tanks.size())) <= TWIN_DOOR_MELEE_REACH;
}

bool LustWindowReached(TwinValkyrState const& state, Creature* fjola, Creature* eydis, bool eydisShield,
                       bool heroic, uint32 now)
{
    if (heroic)
        return eydisShield || (state.pull.specials >= TWIN_LUST_SPECIALS && !state.pull.eydisPact);

    return getMSTimeDiff(state.pull.startMs, now) >= TWIN_NORMAL_LUST_MS ||
           (AtDoor(state, fjola) && AtDoor(state, eydis));
}

void UpdatePull(TwinValkyrState& state, Creature* fjola, Creature* eydis, bool eydisShield, bool eydisPact,
                Map* map, Group* group, uint32 now)
{
    TwinPull& pull = state.pull;
    if (!(fjola && fjola->IsInCombat()) && !(eydis && eydis->IsInCombat()))
    {
        pull = TwinPull();
        return;
    }

    if (!pull.engaged)
    {
        pull.engaged = true;
        pull.startMs = now;
    }

    bool const special = state.vortex != TwinColour::None || !state.pactTwin.IsEmpty() ||
                         !state.shieldedTwin.IsEmpty();
    if (special && !pull.special && pull.specials < TWIN_LUST_SPECIALS)
        ++pull.specials;

    pull.special = special;
    pull.eydisPact = pull.eydisPact || eydisShield || eydisPact;

    if (state.vortex != TwinColour::None && pull.lastVortex == TwinColour::None)
        pull.raidHold = state.vortex == TwinColour::Light && !map->IsHeroic() && FireMasteryReady(group, map);

    pull.lastVortex = state.vortex;

    TrackApproach(pull.approach[0], fjola, DoorOf(TwinColour::Light, state.tanks.size()), now);
    TrackApproach(pull.approach[1], eydis, DoorOf(TwinColour::Dark, state.tanks.size()), now);

    if (!pull.lustOpen && LustWindowReached(state, fjola, eydis, eydisShield, map->IsHeroic(), now))
    {
        pull.lustOpen = true;
        pull.lustOpenMs = now;
    }
}

// Null off a ToC instance
TwinValkyrState* ReadTwins(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Map* map = bot ? bot->FindMap() : nullptr;
    if (!map || map->GetId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return nullptr;

    InstanceScript* instance = bot->GetInstanceScript();
    if (!instance)
        return nullptr;

    TwinValkyrState& state = twinStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.memoValid && state.memoMs == now)
        return &state;

    state.memoMs = now;
    state.memoValid = true;

    Creature* fjola = AliveTwin(map, instance, TOC_DATA_FJOLA);
    Creature* eydis = AliveTwin(map, instance, TOC_DATA_EYDIS);
    state.fjola = GuidOf(fjola);
    state.eydis = GuidOf(eydis);

    // Each twin casts only her own colour's Vortex, Shield and Pact
    bool const lightVortex =
        fjola && fjola->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_LIGHT_VORTEX, fjola));
    bool const darkVortex =
        eydis && eydis->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_DARK_VORTEX, eydis));
    state.vortex = lightVortex ? TwinColour::Light : darkVortex ? TwinColour::Dark : TwinColour::None;

    bool const lightPact =
        fjola && fjola->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_LIGHT_TWIN_PACT, fjola));
    bool const darkPact =
        eydis && eydis->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_DARK_TWIN_PACT, eydis));
    state.pactTwin = lightPact ? GuidOf(fjola) : darkPact ? GuidOf(eydis) : ObjectGuid::Empty;

    bool const lightShield = fjola && fjola->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LIGHT_SHIELD, fjola));
    bool const darkShield = eydis && eydis->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_DARK_SHIELD, eydis));
    state.shieldedTwin = lightShield ? GuidOf(fjola) : darkShield ? GuidOf(eydis) : ObjectGuid::Empty;
    state.shield = lightShield ? TwinColour::Light : darkShield ? TwinColour::Dark : TwinColour::None;

    if (fjola || eydis)
    {
        ScanArena(fjola ? fjola : eydis, state);
    }
    else
    {
        state.orbs.clear();
        state.portals.clear();
    }

    Group* group = bot->GetGroup();
    state.tankGroup = group ? group->GetGUID() : ObjectGuid::Empty;
    state.tanks = group ? ResolveTwinTanks(group) : std::vector<ObjectGuid>();

    UpdatePull(state, fjola, eydis, darkShield, darkPact, map, group, now);

    return &state;
}

Creature* ResolveAlive(PlayerbotAI* botAI, ObjectGuid guid)
{
    if (guid.IsEmpty())
        return nullptr;

    Map* map = botAI->GetBot()->FindMap();
    Creature* creature = map ? map->GetCreature(guid) : nullptr;
    return creature && creature->IsAlive() ? creature : nullptr;
}

char const* ColourName(TwinColour colour)
{
    switch (colour)
    {
        case TwinColour::Light:
            return "light";
        case TwinColour::Dark:
            return "dark";
        default:
            return "none";
    }
}

char const* ReasonName(TwinEssenceReason reason)
{
    switch (reason)
    {
        case TwinEssenceReason::Base:
            return "base";
        case TwinEssenceReason::Hold:
            return "hold";
        case TwinEssenceReason::Vortex:
            return "vortex";
        case TwinEssenceReason::Touch:
            return "touch";
        default:
            return "none";
    }
}

// Main tank first, so a dead main tank hands Fjola to the first assist
std::vector<ObjectGuid> TwinTankGuids(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return {};

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    TwinValkyrState const* state = botAI ? ReadTwins(botAI) : nullptr;
    if (state && state->tankGroup == group->GetGUID())
        return state->tanks;

    return ResolveTwinTanks(group);
}

TwinTankRole TankRoleOf(Player* bot)
{
    std::vector<ObjectGuid> const tanks = TwinTankGuids(bot);
    if (tanks.empty())
        return TwinTankRole::None;

    if (tanks[0] == bot->GetGUID())
        return tanks.size() == 1 ? TwinTankRole::Both : TwinTankRole::Fjola;

    return tanks.size() > 1 && tanks[1] == bot->GetGUID() ? TwinTankRole::Eydis : TwinTankRole::None;
}

// CanCastSpell tests with power costs switched off, so an empty rogue or a death knight short on
// runic power reads ready and then fails the real cast
bool CanAffordSpell(Player* bot, uint32 spellId)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info || info->PowerType >= MAX_POWERS)
        return true;

    return info->CalcPowerCost(bot, info->GetSchoolMask()) <= int32(bot->GetPower(Powers(info->PowerType)));
}

char const* TankRoleName(TwinTankRole role)
{
    switch (role)
    {
        case TwinTankRole::Fjola:
            return "fjola";
        case TwinTankRole::Eydis:
            return "eydis";
        case TwinTankRole::Both:
            return "both";
        default:
            return "none";
    }
}

void NoteTankRole(Player* bot, TwinTankRole role)
{
    if (PlayerbotAI::IsTank(bot) || PlayerbotAI::IsTank(bot, true))
        RaidObs::NoteDerived(bot, "tv.tank", TankRoleName(role));
}

// Eydis's Touch only picks a Light holder no twin has as victim, so with only her tank on Light it
// finds nobody
TwinColour BaseColourOf(Player* bot)
{
    return TankRoleOf(bot) == TwinTankRole::Eydis ? TwinColour::Light : TwinColour::Dark;
}

// No probe: the interrupt duty asks this about every other bot too
TwinEssenceWant WantedEssence(PlayerbotAI* botAI)
{
    TwinValkyrState const* state = ReadTwins(botAI);
    if (!state || (state->fjola.IsEmpty() && state->eydis.IsEmpty()))
        return {};

    // Over a Touch: the Touch's other colour would eat the whole Vortex, up to 80k
    Player* bot = botAI->GetBot();
    TwinColour const held = EssenceOf(bot);
    if (state->vortex != TwinColour::None)
    {
        // a tank walking off would drag its twin away from the door
        bool const keeps = TankRoleOf(bot) != TwinTankRole::None ||
                           (state->pull.raidHold && state->vortex == TwinColour::Light);
        if (keeps && held != TwinColour::None && held != state->vortex)
            return {held, TwinEssenceReason::Hold};

        return {state->vortex, TwinEssenceReason::Vortex};
    }

    if (HasLightTouch(bot))
        return {TwinColour::Light, TwinEssenceReason::Touch};

    if (HasDarkTouch(bot))
        return {TwinColour::Dark, TwinEssenceReason::Touch};

    return {BaseColourOf(bot), TwinEssenceReason::Base};
}

bool MustSwap(Player* bot, TwinEssenceWant const& want, bool urgentOnly)
{
    if (want.colour == TwinColour::None)
        return false;

    if (urgentOnly && want.reason != TwinEssenceReason::Touch && want.reason != TwinEssenceReason::Vortex)
        return false;

    return want.colour != EssenceOf(bot);
}

bool OtherColourAllyNear(Player* bot, TwinColour colour, float radius)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || !member->IsInMap(bot))
            continue;

        if (EssenceOf(member) != colour && bot->GetExactDist2d(member) <= radius)
            return true;
    }

    return false;
}

void AppendOrbPath(TwinOrb const& orb, OrbRule rule, std::vector<OrbHazard>& hazards)
{
    float const dx = orb.destination.GetPositionX() - orb.position.GetPositionX();
    float const dy = orb.destination.GetPositionY() - orb.position.GetPositionY();
    float const length = std::sqrt(dx * dx + dy * dy);
    float const reach = std::min(length, TWIN_ORB_HORIZON);

    auto const pointAt = [&](float along)
    {
        float const scale = length > 0.0f ? along / length : 0.0f;
        return Position(orb.position.GetPositionX() + dx * scale, orb.position.GetPositionY() + dy * scale,
                        orb.position.GetPositionZ());
    };

    for (float along = 0.0f; along < reach; along += 1.0f)
        hazards.push_back({pointAt(along), rule});

    hazards.push_back({pointAt(reach), rule});
}

std::vector<OrbHazard> OrbHazards(PlayerbotAI* botAI)
{
    std::vector<OrbHazard> hazards;
    TwinValkyrState const* state = ReadTwins(botAI);
    if (!state || state->orbs.empty())
        return hazards;

    Player* bot = botAI->GetBot();
    TwinColour const held = EssenceOf(bot);
    bool splashChecked = false;
    bool splash = false;
    for (TwinOrb const& orb : state->orbs)
    {
        if (held == TwinColour::None || orb.colour != held)
        {
            AppendOrbPath(orb, OrbRule::Wrong, hazards);
            continue;
        }

        if (!splashChecked)
        {
            splash = OtherColourAllyNear(bot, held, TWIN_ORB_SPLASH_ALLY_RADIUS);
            splashChecked = true;
        }

        if (splash)
            AppendOrbPath(orb, OrbRule::Splash, hazards);
    }

    return hazards;
}

template <size_t N>
Position SlotOf(DoorPoint const (&slots)[N], uint32& next)
{
    return DoorPosition(slots[next++ % N]);
}

// Alive or not, so a death doesn't move anyone else. A tank spec out of the tank list goes last, so
// a dead tank dropping off it doesn't shift the melee either.
std::vector<Player*> DoorBots(Player* bot, Group* group)
{
    std::vector<Player*> bots;
    if (!group)
    {
        bots.push_back(bot);
        return bots;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsInWorld() && member->GetMapId() == bot->GetMapId() &&
            member->GetInstanceId() == bot->GetInstanceId() && GET_PLAYERBOT_AI(member))
            bots.push_back(member);
    }

    auto const key = [](Player* member)
    { return std::make_pair(PlayerbotAI::IsTank(member, true), member->GetGUID()); };
    std::sort(bots.begin(), bots.end(), [&key](Player* lhs, Player* rhs) { return key(lhs) < key(rhs); });
    return bots;
}

void DealDoorStands(TwinValkyrState& state, Player* bot, uint32 now)
{
    Group* group = bot->GetGroup();
    ObjectGuid const groupGuid = group ? group->GetGUID() : ObjectGuid::Empty;
    if (state.doorDealValid && state.doorGroup == groupGuid && getMSTimeDiff(state.doorDealMs, now) < TWIN_DOOR_DEAL_MS)
        return;

    state.doorDealValid = true;
    state.doorDealMs = now;
    state.doorGroup = groupGuid;
    state.doorStands.clear();

    std::vector<ObjectGuid> const tanks = TwinTankGuids(bot);
    uint32 melee = 0;
    uint32 heal = 0;
    uint32 ranged = 0;
    for (Player* member : DoorBots(bot, group))
    {
        TwinDoorStand stand;
        auto const tank = std::find(tanks.begin(), tanks.end(), member->GetGUID());
        if (tank != tanks.end())
        {
            stand.role = TwinDoorRole::Tank;
            if (tanks.size() == 1)
            {
                stand.label = "both";
                stand.spot = DoorPosition(LONE_DOOR);
            }
            else if (tank == tanks.begin())
            {
                stand.label = "fjola";
                stand.spot = DoorPosition(FJOLA_DOOR);
            }
            else
            {
                stand.label = "eydis";
                stand.spot = DoorPosition(EYDIS_DOOR);
            }
        }
        else if (PlayerbotAI::IsHeal(member))
        {
            stand.role = TwinDoorRole::Heal;
            stand.label = "healer";
            stand.spot = SlotOf(HEAL_SLOTS, heal);
        }
        else if (PlayerbotAI::IsRangedDps(member))
        {
            stand.role = TwinDoorRole::Ranged;
            stand.label = "ranged";
            stand.spot = SlotOf(RANGED_SLOTS, ranged);
        }
        else
        {
            stand.role = TwinDoorRole::Melee;
            stand.label = "melee";
            stand.spot = SlotOf(MELEE_SLOTS, melee);
        }

        state.doorStands[member->GetGUID()] = stand;
    }
}

char const* DoorRoleName(TwinDoorRole role)
{
    switch (role)
    {
        case TwinDoorRole::Tank:
            return "tank";
        case TwinDoorRole::Melee:
            return "melee";
        case TwinDoorRole::Ranged:
            return "ranged";
        case TwinDoorRole::Heal:
            return "heal";
        default:
            return "none";
    }
}

// What the running Vortex still has to deal each player, before resistance. Its first tick lands a
// period into the channel.
bool VortexHit(PlayerbotAI* botAI, TwinValkyrState const& state, ToCPredictedHit& hit)
{
    if (state.vortex == TwinColour::None)
        return false;

    bool const light = state.vortex == TwinColour::Light;
    Creature* twin = ResolveAlive(botAI, light ? state.fjola : state.eydis);
    Spell* spell =
        twin ? twin->FindCurrentSpellBySpellId(
                   sSpellMgr->GetSpellIdForDifficulty(light ? SPELL_LIGHT_VORTEX : SPELL_DARK_VORTEX, twin))
             : nullptr;
    SpellInfo const* info = spell ? spell->GetSpellInfo() : nullptr;
    if (!info)
        return false;

    SpellEffectInfo const& periodic = info->Effects[EFFECT_0];
    int32 const period = periodic.Amplitude;
    int32 const channelMs = info->GetDuration();
    SpellInfo const* tick =
        sSpellMgr->GetSpellInfo(sSpellMgr->GetSpellIdForDifficulty(periodic.TriggerSpell, twin));
    if (period <= 0 || channelMs < period || !tick)
        return false;

    uint32 const ticks = static_cast<uint32>(channelMs / period);
    uint32 const perTick =
        static_cast<uint32>(std::max(tick->Effects[EFFECT_0].BasePoints + (tick->Effects[EFFECT_0].DieSides + 1) / 2, 0));

    uint32 const now = getMSTime();
    int32 const left = std::max(spell->GetCastTimeRemaining(), 0);
    uint32 channelStart = now + static_cast<uint32>(left);
    uint32 ticksLeft = ticks;
    if (spell->getState() != SPELL_STATE_PREPARING)
    {
        int32 const elapsed = std::max(channelMs - left, 0);
        channelStart = now - static_cast<uint32>(elapsed);
        ticksLeft -= std::min(ticks, static_cast<uint32>(elapsed / period));
    }

    hit = ToCPredictedHit();
    hit.startMs = channelStart + static_cast<uint32>(period);
    hit.durationMs = (ticks - 1) * static_cast<uint32>(period);
    hit.damage = perTick * ticksLeft;
    hit.school = info->GetSchoolMask();
    return true;
}

bool HasLust(Player* bot) { return bot->HasAura(SPELL_BLOODLUST) || bot->HasAura(SPELL_HEROISM); }

enum class SatedSince : uint8
{
    None,
    ThisPull,
    Before
};

SatedSince SatedOf(Player* bot, uint32 pullMs, uint32 now)
{
    for (uint32 const spellId : {SPELL_SATED, SPELL_EXHAUSTION})
    {
        Aura const* aura = bot->GetAura(spellId);
        if (!aura)
            continue;

        uint32 const elapsed = static_cast<uint32>(std::max(aura->GetMaxDuration() - aura->GetDuration(), 0));
        return elapsed <= getMSTimeDiff(pullMs, now) ? SatedSince::ThisPull : SatedSince::Before;
    }

    return SatedSince::None;
}

// Humans too: a player's lust lands on the bots all the same
bool LustReady(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !member->IsInMap(bot) || member->getClass() != CLASS_SHAMAN)
            continue;

        for (uint32 const spellId : {SPELL_BLOODLUST, SPELL_HEROISM})
            if (member->HasSpell(spellId) && !member->HasSpellCooldown(spellId))
                return true;
    }

    return false;
}

}

Unit* GetFjola(PlayerbotAI* botAI)
{
    TwinValkyrState const* state = ReadTwins(botAI);
    return state ? ResolveAlive(botAI, state->fjola) : nullptr;
}

Unit* GetEydis(PlayerbotAI* botAI)
{
    TwinValkyrState const* state = ReadTwins(botAI);
    return state ? ResolveAlive(botAI, state->eydis) : nullptr;
}

bool IsTwinAttackable(Unit* twin) { return twin && !twin->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE); }

TwinColour TwinColourOf(Unit* twin)
{
    if (!twin)
        return TwinColour::None;

    switch (twin->GetEntry())
    {
        case static_cast<uint32>(ToCNpcs::NPC_FJOLA_LIGHTBANE):
            return TwinColour::Light;
        case static_cast<uint32>(ToCNpcs::NPC_EYDIS_DARKBANE):
            return TwinColour::Dark;
        default:
            return TwinColour::None;
    }
}

TwinColour EssenceOf(Unit* unit)
{
    if (HasLightEssence(unit))
        return TwinColour::Light;

    return HasDarkEssence(unit) ? TwinColour::Dark : TwinColour::None;
}

bool HasLightEssence(Unit* unit)
{
    return unit && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LIGHT_ESSENCE, unit));
}

bool HasDarkEssence(Unit* unit)
{
    return unit && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_DARK_ESSENCE, unit));
}

bool HasAnyEssence(Unit* unit)
{
    return HasLightEssence(unit) || HasDarkEssence(unit);
}

bool HasLightTouch(Unit* unit)
{
    return unit && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LIGHT_TOUCH, unit));
}

bool HasDarkTouch(Unit* unit)
{
    return unit && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_DARK_TOUCH, unit));
}

TwinColour ActiveVortexColour(PlayerbotAI* botAI)
{
    TwinValkyrState const* state = ReadTwins(botAI);
    return state ? state->vortex : TwinColour::None;
}

Unit* GetTwinCastingPact(PlayerbotAI* botAI)
{
    TwinValkyrState const* state = ReadTwins(botAI);
    return state ? ResolveAlive(botAI, state->pactTwin) : nullptr;
}

Unit* GetShieldedTwin(PlayerbotAI* botAI)
{
    TwinValkyrState const* state = ReadTwins(botAI);
    return state ? ResolveAlive(botAI, state->shieldedTwin) : nullptr;
}

Player* GetTwinTank(Player* bot, Unit* twin)
{
    TwinColour const colour = TwinColourOf(twin);
    if (!bot || colour == TwinColour::None)
        return nullptr;

    std::vector<ObjectGuid> const tanks = TwinTankGuids(bot);
    if (tanks.empty())
        return nullptr;

    return ObjectAccessor::GetPlayer(*bot, colour == TwinColour::Light || tanks.size() == 1 ? tanks[0] : tanks[1]);
}

bool IsTwinTank(Player* bot, Unit* twin)
{
    TwinColour const colour = TwinColourOf(twin);
    if (!bot || colour == TwinColour::None)
        return false;

    TwinTankRole const role = TankRoleOf(bot);
    NoteTankRole(bot, role);
    return role == TwinTankRole::Both ||
           role == (colour == TwinColour::Light ? TwinTankRole::Fjola : TwinTankRole::Eydis);
}

TwinEssenceWant GetWantedEssence(PlayerbotAI* botAI)
{
    if (!botAI)
        return {};

    TwinEssenceWant const want = WantedEssence(botAI);
    RaidObs::NoteDerived(botAI->GetBot(), "tv.essence",
                         std::string(ReasonName(want.reason)) + ":" + ColourName(want.colour));
    return want;
}

bool TwinValkyrMustSwapEssence(PlayerbotAI* botAI, bool urgentOnly)
{
    return botAI && MustSwap(botAI->GetBot(), GetWantedEssence(botAI), urgentOnly);
}

Creature* GetEssencePortal(Player* bot, TwinColour colour)
{
    if (!bot || colour == TwinColour::None)
        return nullptr;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    TwinValkyrState const* state = botAI ? ReadTwins(botAI) : nullptr;
    if (!state)
        return nullptr;

    // From Light 1 the nearest Dark portal is Dark 2, at the north end, 80 yd from the door
    TwinDoorStand const stand = GetTwinDoorStand(bot);
    auto const walk = [&](TwinPortal const& portal)
    {
        float const there = bot->GetExactDist2d(portal.position);
        return stand.role == TwinDoorRole::None ? there : there + portal.position.GetExactDist2d(stand.spot);
    };

    TwinPortal const* best = nullptr;
    for (TwinPortal const& portal : state->portals)
        if (portal.colour == colour && (!best || walk(portal) < walk(*best)))
            best = &portal;

    return best ? ResolveAlive(botAI, best->guid) : nullptr;
}

Unit* GetTwinDpsTarget(PlayerbotAI* botAI)
{
    if (!botAI)
        return nullptr;

    Player* bot = botAI->GetBot();
    size_t const tankCount = TwinTankGuids(bot).size();
    bool const melee = !PlayerbotAI::IsRanged(bot);
    // the door hold keeps melee in the tunnel
    auto const reachable = [&](Unit* twin)
    {
        return twin && (!melee || twin->GetExactDist2d(DoorOf(TwinColourOf(twin), tankCount)) <=
                                      TWIN_DOOR_MELEE_REACH);
    };

    Unit* target = nullptr;
    char const* rule = "none";
    Unit* skull = GetToCHumanSkull(botAI, [](Unit* unit) { return TwinColourOf(unit) != TwinColour::None; });
    if (reachable(skull))
    {
        target = skull;
        rule = "skull";
    }

    Unit* pact = target ? nullptr : GetTwinCastingPact(botAI);
    // Normal: her Pact heals only 20%, not worth a Dark raid's -50% on her
    Map* map = bot->GetMap();
    if (pact && TwinColourOf(pact) == TwinColour::Dark && !(map && map->IsHeroic()))
        pact = nullptr;

    if (reachable(pact))
    {
        target = pact;
        rule = "pact";
    }

    if (!target)
    {
        // The essence gives +50% against the other colour's twin and -50% against her sister
        TwinColour const held = EssenceOf(bot);
        rule = ColourName(held);

        Unit* fjola = GetFjola(botAI);
        Unit* eydis = GetEydis(botAI);
        Unit* pick = nullptr;
        Unit* sister = nullptr;
        if (!fjola || !eydis)
        {
            pick = fjola ? fjola : eydis;
        }
        else if (tankCount == 1)
        {
            // A lone tank hits Fjola and taunts Eydis back, and taunts diminish on the twins, so Light
            // DPS on Eydis would pull her off it for good
            rule = "lone";
            pick = fjola;
        }
        else
        {
            pick = held == TwinColour::Light ? eydis : fjola;
            sister = pick == fjola ? eydis : fjola;
        }

        if (reachable(pick))
        {
            target = pick;
        }
        else if (reachable(sister))
        {
            target = sister;
            rule = "door";
        }
    }

    std::string const probe =
        target ? std::string(TwinColourOf(target) == TwinColour::Light ? "fjola" : "eydis") + ":" + rule : "none";
    RaidObs::NoteDerived(bot, "tv.target", probe);
    return target;
}

char const* TwinReadyInterrupt(Player* bot, Unit* twin)
{
    if (!bot || !twin || !bot->IsAlive())
        return nullptr;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return nullptr;

    // CanCastSpell passes any spell the pet knows without asking about cooldowns, yet the bot casts
    // Spell Lock as itself, so its cooldown lands on the bot
    auto const ready = [bot, botAI, twin](char const* spell) -> char const*
    {
        uint32 const id = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
        bool const ok = id && !bot->HasSpellCooldown(id) && CanAffordSpell(bot, id) && botAI->CanCastSpell(id, twin);
        return ok ? spell : nullptr;
    };

    switch (bot->getClass())
    {
        case CLASS_ROGUE:
            return bot->IsWithinMeleeRange(twin) ? ready("kick") : nullptr;
        case CLASS_WARRIOR:
        {
            if (!bot->IsWithinMeleeRange(twin))
                return nullptr;

            char const* pummel = ready("pummel");
            return pummel ? pummel : ready("shield bash");
        }
        case CLASS_DEATH_KNIGHT:
            return bot->IsWithinMeleeRange(twin) ? ready("mind freeze") : nullptr;
        case CLASS_MAGE:
            return bot->IsWithinCombatRange(twin, TWIN_COUNTERSPELL_RANGE) ? ready("counterspell") : nullptr;
        case CLASS_SHAMAN:
            return bot->IsWithinCombatRange(twin, TWIN_WIND_SHEAR_RANGE) ? ready("wind shear") : nullptr;
        case CLASS_WARLOCK:
        {
            // The felhunter only lends the spell: range, like the cooldown, is measured from the bot
            Pet* pet = bot->GetPet();
            if (!pet || !pet->IsAlive() || !bot->IsWithinCombatRange(twin, TWIN_SPELL_LOCK_RANGE))
                return nullptr;

            return ready("spell lock");
        }
        default:
            return nullptr;
    }
}

bool IsTwinPactInterrupter(PlayerbotAI* botAI, Unit* twin)
{
    if (!botAI || !twin)
        return false;

    if (GetTwinCastingPact(botAI) != twin || GetShieldedTwin(botAI))
        return false;

    Player* bot = botAI->GetBot();
    if (!TwinReadyInterrupt(bot, twin))
    {
        // The key is change-only, so without this a duty from an earlier Pact reads as current
        RaidObs::NoteDerived(bot, "tv.interrupt", "none");
        return false;
    }

    bool duty = !TwinValkyrMustSwapEssence(botAI, true);
    if (duty)
    {
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || member == bot || !member->IsAlive() || !member->IsInMap(bot) ||
                    !(member->GetGUID() < bot->GetGUID()))
                    continue;

                PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
                if (!memberAI || !TwinReadyInterrupt(member, twin) ||
                    MustSwap(member, WantedEssence(memberAI), true))
                    continue;

                duty = false;
                break;
            }
        }
    }

    RaidObs::NoteDerived(bot, "tv.interrupt", duty ? "duty" : "standby");
    return duty;
}

bool TwinOrbSpotClear(PlayerbotAI* botAI, Position const& spot, float clearance)
{
    for (OrbHazard const& hazard : OrbHazards(botAI))
        if (hazard.point.GetExactDist2d(spot) < clearance)
            return false;

    return true;
}

bool TwinOrbThreatens(PlayerbotAI* botAI, float clearance)
{
    return botAI && !TwinOrbSpotClear(botAI, botAI->GetBot()->GetPosition(), clearance);
}

bool FindTwinOrbDodgeSpot(PlayerbotAI* botAI, Position& spot)
{
    if (!botAI)
        return false;

    Player* bot = botAI->GetBot();
    std::vector<OrbHazard> const hazards = OrbHazards(botAI);

    std::vector<Position> points;
    points.reserve(hazards.size());
    OrbRule nearestRule = OrbRule::Wrong;
    float nearest = 0.0f;
    for (OrbHazard const& hazard : hazards)
    {
        points.push_back(hazard.point);

        float const distance = bot->GetExactDist2d(hazard.point);
        if (points.size() == 1 || distance < nearest)
        {
            nearest = distance;
            nearestRule = hazard.rule;
        }
    }

    // tv.orb is the rule that sent the bot, the move record already has where it went
    Position const candidate = FindNearestPositionClearOfHazards(bot, points, TWIN_ORB_DODGE_SEARCH_CLEARANCE,
                                                                 TWIN_ORB_DODGE_SEARCH_RADIUS);
    if (candidate == Position())
    {
        RaidObs::NoteDerived(bot, "tv.orb", "none");
        return false;
    }

    RaidObs::NoteDerived(bot, "tv.orb", nearestRule == OrbRule::Splash ? "splash" : "wrong");
    spot = candidate;
    return true;
}

TwinDoorStand GetTwinDoorStand(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    TwinValkyrState* state = botAI ? ReadTwins(botAI) : nullptr;
    if (!state)
        return {};

    DealDoorStands(*state, bot, getMSTime());
    auto const dealt = state->doorStands.find(bot->GetGUID());
    TwinDoorStand const stand = dealt != state->doorStands.end() ? dealt->second : TwinDoorStand();
    if (RaidObs::Active())
    {
        // the spot too: the reader can't redo the deal
        char note[48];
        std::snprintf(note, sizeof(note), "%s:%.1f,%.1f", DoorRoleName(stand.role), stand.spot.GetPositionX(),
                      stand.spot.GetPositionY());
        RaidObs::NoteDerived(bot, "tv.door", stand.role == TwinDoorRole::None ? "none" : note);
    }

    return stand;
}

Position GetTwinDoorSpot(Player* bot, Unit* twin)
{
    return DoorOf(TwinColourOf(twin), bot ? TwinTankGuids(bot).size() : 0);
}

bool TwinValkyrDoorHold(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || GetTwinDoorStand(bot).role == TwinDoorRole::None)
        return false;

    for (Unit* twin : {GetFjola(botAI), GetEydis(botAI)})
        if (twin && IsTwinTank(bot, twin) && TwinValkyrFetchDue(botAI, twin))
            return false;

    return true;
}

bool TwinValkyrDoorWalkDue(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    TwinValkyrState* state = bot ? ReadTwins(botAI) : nullptr;
    if (!state || !bot->IsAlive() || TankRoleOf(bot) != TwinTankRole::None)
        return false;

    TwinDoorStand const stand = GetTwinDoorStand(bot);
    if (stand.role == TwinDoorRole::None || MustSwap(bot, WantedEssence(botAI), false))
        return false;

    // It took the Vortex's colour and walks to a portal of its own once the channel ends, so the stand
    // would be a detour
    TwinColour const held = EssenceOf(bot);
    if (state->vortex != TwinColour::None && held == state->vortex && held != BaseColourOf(bot))
        return false;

    bool& parked = state->doorParked[bot->GetGUID()];
    float const distance = bot->GetExactDist2d(stand.spot);
    if (distance <= TWIN_DOOR_PARK)
    {
        parked = true;
        return false;
    }

    if (parked && distance <= TWIN_DOOR_UNPARK)
        return false;

    parked = false;
    return true;
}

bool TwinValkyrFetchDue(PlayerbotAI* botAI, Unit* twin)
{
    TwinValkyrState const* state = botAI ? ReadTwins(botAI) : nullptr;
    TwinColour const colour = TwinColourOf(twin);
    if (!state || colour == TwinColour::None || !state->pull.engaged || twin->GetVictim() == botAI->GetBot())
        return false;

    TwinApproach const& approach = state->pull.approach[colour == TwinColour::Light ? 0 : 1];
    return approach.valid && twin->GetExactDist2d(DoorOf(colour, state->tanks.size())) > TWIN_TAUNT_RANGE &&
           getMSTimeDiff(approach.sinceMs, getMSTime()) >= TWIN_FETCH_STALL_MS;
}

bool GetTwinVortexHit(PlayerbotAI* botAI, ToCPredictedHit& hit)
{
    TwinValkyrState const* state = botAI ? ReadTwins(botAI) : nullptr;
    return state && WantedEssence(botAI).reason == TwinEssenceReason::Hold && VortexHit(botAI, *state, hit);
}

bool GetTwinVortexRaidHit(PlayerbotAI* botAI, ToCPredictedHit& hit)
{
    TwinValkyrState const* state = botAI ? ReadTwins(botAI) : nullptr;
    return state && state->pull.raidHold && state->vortex == TwinColour::Light && VortexHit(botAI, *state, hit);
}

bool TwinValkyrRaidCoverHeld(PlayerbotAI* botAI)
{
    ToCPredictedHit hit;
    return GetTwinVortexRaidHit(botAI, hit) && ToCRaidHitLeadIn(hit);
}

bool TwinValkyrLustWindowOpen(PlayerbotAI* botAI)
{
    TwinValkyrState const* state = botAI ? ReadTwins(botAI) : nullptr;
    return state && state->pull.lustOpen;
}

bool TwinValkyrBurstHeld(PlayerbotAI* botAI)
{
    TwinValkyrState const* state = botAI ? ReadTwins(botAI) : nullptr;
    if (!state || !state->pull.engaged)
        return false;

    Player* bot = botAI->GetBot();
    uint32 const now = getMSTime();
    SatedSince const sated = SatedOf(bot, state->pull.startMs, now);
    if (HasLust(bot) || sated == SatedSince::ThisPull)
        return false;

    if (!state->pull.lustOpen)
        return true;

    if (sated == SatedSince::Before || !LustReady(bot))
        return false;

    // a lust that never comes, its shaman out of range or busy
    return getMSTimeDiff(state->pull.lustOpenMs, now) < TWIN_LUST_GRACE_MS;
}

}

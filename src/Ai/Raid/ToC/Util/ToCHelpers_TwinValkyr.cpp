#include "ToCHelpers_TwinValkyr.h"

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
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Shared.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <string>
#include <vector>

using namespace EncounterHelpers;

namespace TrialOfTheCrusaderHelpers
{
namespace
{

// The script swaps an orb to this model the moment it explodes and despawns it 1.5 s later. The
// blast is instant, so what's left is harmless.
constexpr uint32 TWIN_ORB_SPENT_DISPLAY_ID = 11686;
// Orbs stay inside a 47 yd circle round ARENA_CENTER and the portals sit 31.8 yd out, so a twin
// held anywhere near the centre reaches all of them
constexpr float TWIN_ARENA_SCAN_RADIUS = 100.0f;
constexpr float TWIN_COUNTERSPELL_RANGE = 30.0f;
constexpr float TWIN_WIND_SHEAR_RANGE = 25.0f;
constexpr float TWIN_SPELL_LOCK_RANGE = 30.0f;

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
};

RaidInstanceState<TwinValkyrState> twinStates;

enum class TwinTankRole : uint8
{
    None,
    Fjola,
    Eydis,
    Both
};

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

// Null off a ToC instance
TwinValkyrState const* ReadTwins(PlayerbotAI* botAI)
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

TwinColour OtherColour(TwinColour colour)
{
    switch (colour)
    {
        case TwinColour::Light:
            return TwinColour::Dark;
        case TwinColour::Dark:
            return TwinColour::Light;
        default:
            return TwinColour::None;
    }
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
        case TwinEssenceReason::Shield:
            return "shield";
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

// No probe: the interrupt duty asks this about every other bot too
TwinEssenceWant WantedEssence(PlayerbotAI* botAI)
{
    if (!GetFjola(botAI) && !GetEydis(botAI))
        return {};

    Player* bot = botAI->GetBot();
    if (HasLightTouch(bot))
        return {TwinColour::Light, TwinEssenceReason::Touch};

    if (HasDarkTouch(bot))
        return {TwinColour::Dark, TwinEssenceReason::Touch};

    TwinColour const vortex = ActiveVortexColour(botAI);
    if (vortex != TwinColour::None)
        return {vortex, TwinEssenceReason::Vortex};

    TwinColour const held = EssenceOf(bot);
    bool const tank = PlayerbotAI::IsTank(bot) || PlayerbotAI::IsTank(bot, true);
    if (!tank && !PlayerbotAI::IsHeal(bot))
    {
        // A bot with no essence goes straight to the other colour too, rather than walking to the
        // base colour first and then back
        Unit* pact = GetTwinCastingPact(botAI);
        if (pact && pact == GetShieldedTwin(botAI))
        {
            TwinColour const wanted = OtherColour(TwinColourOf(pact));
            if (held != wanted)
                return {wanted, TwinEssenceReason::Shield};
        }
    }

    switch (TankRoleOf(bot))
    {
        case TwinTankRole::Fjola:
        case TwinTankRole::Both:
            return {TwinColour::Dark, TwinEssenceReason::Base};
        case TwinTankRole::Eydis:
            return {TwinColour::Light, TwinEssenceReason::Base};
        default:
            break;
    }

    return {held == TwinColour::None ? TwinColour::Dark : held, TwinEssenceReason::Base};
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

    TwinPortal const* nearest = nullptr;
    for (TwinPortal const& portal : state->portals)
    {
        if (portal.colour == colour &&
            (!nearest || bot->GetExactDist2d(portal.position) < bot->GetExactDist2d(nearest->position)))
        {
            nearest = &portal;
        }
    }

    return nearest ? ResolveAlive(botAI, nearest->guid) : nullptr;
}

Unit* GetTwinDpsTarget(PlayerbotAI* botAI)
{
    if (!botAI)
        return nullptr;

    Player* bot = botAI->GetBot();
    Unit* target = GetTwinCastingPact(botAI);
    char const* rule = "pact";
    if (!target)
    {
        // The essence gives +50% against the other colour's twin and -50% against her sister
        TwinColour const held = EssenceOf(bot);
        rule = ColourName(held);

        Unit* fjola = GetFjola(botAI);
        Unit* eydis = GetEydis(botAI);
        if (!fjola || !eydis)
        {
            target = fjola ? fjola : eydis;
        }
        else if (TwinTankGuids(bot).size() == 1)
        {
            // A lone tank hits Fjola and taunts Eydis back, and taunts diminish on the twins, so Light
            // DPS on Eydis would pull her off it for good
            rule = "lone";
            target = fjola;
        }
        else
        {
            target = held == TwinColour::Light ? eydis : fjola;
        }
    }

    std::string const probe =
        target ? std::string(TwinColourOf(target) == TwinColour::Light ? "fjola" : "eydis") + ":" + rule : "none";
    RaidObs::NoteDerived(bot, "tv.target", probe);
    return target;
}

char const* TwinRtiIcon(Unit* twin)
{
    return TwinColourOf(twin) == TwinColour::Dark ? "cross" : "skull";
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

}

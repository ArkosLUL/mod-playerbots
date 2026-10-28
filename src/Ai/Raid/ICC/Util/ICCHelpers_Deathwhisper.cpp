/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Deathwhisper.h"

#include <algorithm>
#include <iterator>
#include <unordered_map>

#include "CellImpl.h"
#include "DynamicObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "ICCDefinitions.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Shared.h"
#include "InstanceScript.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDifficulty.h"
#include "RaidDirectTargeting.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Timer.h"

namespace
{
constexpr float LDW_SCAN_RANGE = 100.0f;
constexpr uint32 LDW_SCAN_MS = 200;
constexpr uint32 LDW_CC_UPDATE_MS = 200;
// A duty that has landed nothing this long goes to the next caster.
constexpr uint32 LDW_CC_DUTY_TIMEOUT_MS = 4000;
// A victim held this much longer needs no caster yet. Cyclone leaves its target immune to every spell,
// so the next cast, 1.5 s at most, has to land just after it ends.
constexpr int32 LDW_CC_HELD_MS = 1000;
constexpr float LDW_CC_WALK = 10.0f;
constexpr float LDW_MELEE_INTERRUPT_RANGE = 5.0f;

// Best first: Cyclone and Hammer of Justice don't break on damage, the rest do.
struct LdwCcSpell
{
    uint8 cls;
    char const* name;
};

constexpr LdwCcSpell LDW_CC_SPELLS[] = {
    {CLASS_DRUID, "cyclone"},     {CLASS_PALADIN, "hammer of justice"}, {CLASS_SHAMAN, "hex"},
    {CLASS_MAGE, "polymorph"},    {CLASS_PALADIN, "repentance"},        {CLASS_WARLOCK, "fear"},
    {CLASS_ROGUE, "blind"},
};

constexpr uint64 LDW_CC_MECHANICS = (1ull << MECHANIC_BANISH) | (1ull << MECHANIC_STUN) |
                                    (1ull << MECHANIC_POLYMORPH) | (1ull << MECHANIC_KNOCKOUT) |
                                    (1ull << MECHANIC_FEAR) | (1ull << MECHANIC_DISORIENTED) |
                                    (1ull << MECHANIC_HORROR) | (1ull << MECHANIC_SAPPED);

struct LdwState
{
    RaidObs::ObsGuidMap<ObjectGuid> ccDuty{"ldw.cc"};
    std::unordered_map<ObjectGuid, uint32> ccSince;
    uint32 ccUpdatedMs = 0;

    uint32 scanMs = 0;
    std::vector<ObjectGuid> adds;
    std::vector<ObjectGuid> shades;
    std::vector<Position> deathAndDecay;
};

RaidInstanceState<LdwState> sLdwState;

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

// A bot of its own, not a player, selfbot included: players hold no duty.
bool IsDutyBot(Player* player) { return GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

bool IsDominated(Unit* unit) { return unit && unit->HasAura(SPELL_LDW_DOMINATE_MIND); }

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
        if (member && member->IsAlive() && member->IsInMap(bot))
            members.push_back(member);
    }

    return members;
}

LdwState& Scan(Player* bot, Creature* boss)
{
    LdwState& state = sLdwState.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.scanMs && getMSTimeDiff(state.scanMs, now) < LDW_SCAN_MS)
        return state;

    state.scanMs = now;
    state.adds.clear();
    state.shades.clear();
    state.deathAndDecay.clear();

    for (Creature* add : IccGetCreaturesByEntries(boss,
                                                  {NPC_LDW_CULT_FANATIC, NPC_LDW_DEFORMED_FANATIC,
                                                   NPC_LDW_REANIMATED_FANATIC, NPC_LDW_CULT_ADHERENT,
                                                   NPC_LDW_EMPOWERED_ADHERENT, NPC_LDW_REANIMATED_ADHERENT},
                                                  LDW_SCAN_RANGE))
        state.adds.push_back(add->GetGUID());

    for (Creature* shade : IccGetCreaturesByEntries(boss, {NPC_SHADE}, LDW_SCAN_RANGE))
        state.shades.push_back(shade->GetGUID());

    uint32 const dndId = sSpellMgr->GetSpellIdForDifficulty(SPELL_LDW_DEATH_AND_DECAY, boss);
    std::list<WorldObject*> objects;
    Acore::AllWorldObjectsInRange check(boss, LDW_SCAN_RANGE);
    Acore::WorldObjectListSearcher<Acore::AllWorldObjectsInRange> searcher(boss, objects, check,
                                                                           GRID_MAP_TYPE_MASK_DYNAMICOBJECT);
    Cell::VisitObjects(boss, searcher, LDW_SCAN_RANGE);
    for (WorldObject* object : objects)
        if (object->GetTypeId() == TYPEID_DYNAMICOBJECT && static_cast<DynamicObject*>(object)->GetSpellId() == dndId)
            state.deathAndDecay.emplace_back(object->GetPositionX(), object->GetPositionY(), object->GetPositionZ());

    return state;
}

bool Usable(Player* bot, Unit* unit)
{
    return unit && unit->IsAlive() && unit->IsInCombat() && !unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) &&
           bot->IsValidAttackTarget(unit);
}

bool IsLdwFightUnit(Unit* unit)
{
    switch (unit->GetEntry())
    {
        case NPC_LDW_CULT_FANATIC:
        case NPC_LDW_DEFORMED_FANATIC:
        case NPC_LDW_REANIMATED_FANATIC:
        case NPC_LDW_CULT_ADHERENT:
        case NPC_LDW_EMPOWERED_ADHERENT:
        case NPC_LDW_REANIMATED_ADHERENT:
        case NPC_LADY_DEATHWHISPER:
            return true;
        default:
            return false;
    }
}

// The add's place in this bot's kill order, lower first, -1 when its damage school can't touch it.
// Melee and hunters take the adherents first, casters the fanatics.
int AddTier(Unit* add, bool physical)
{
    bool const shrouded = add->HasAura(SPELL_LDW_SHROUD_OF_THE_OCCULT);
    switch (add->GetEntry())
    {
        case NPC_LDW_EMPOWERED_ADHERENT:
            return physical ? 0 : (shrouded ? -1 : 3);
        case NPC_LDW_CULT_ADHERENT:
            return physical ? 1 : (shrouded ? -1 : 4);
        case NPC_LDW_REANIMATED_ADHERENT:
            return physical ? 2 : -1;
        case NPC_LDW_DEFORMED_FANATIC:
            return physical ? 3 : 1;
        case NPC_LDW_CULT_FANATIC:
            return physical ? 4 : 2;
        case NPC_LDW_REANIMATED_FANATIC:
            return physical ? -1 : 0;
        default:
            return -1;
    }
}

char const* AddName(Unit* add)
{
    switch (add->GetEntry())
    {
        case NPC_LDW_EMPOWERED_ADHERENT:
            return "empowered";
        case NPC_LDW_CULT_ADHERENT:
            return "adherent";
        case NPC_LDW_REANIMATED_ADHERENT:
            return "reanimated adherent";
        case NPC_LDW_DEFORMED_FANATIC:
            return "deformed";
        case NPC_LDW_CULT_FANATIC:
            return "fanatic";
        case NPC_LDW_REANIMATED_FANATIC:
            return "reanimated fanatic";
        default:
            return "other";
    }
}

Unit* CurrentTarget(PlayerbotAI* botAI)
{
    return botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
}

Unit* DpsPick(Player* bot, Creature* boss, LdwState& state, Unit* current, char const*& why)
{
    bool const physical = bot->getClass() == CLASS_HUNTER || PlayerbotAI::IsMelee(bot);

    Unit* best = nullptr;
    int bestTier = 0;
    for (ObjectGuid const& guid : state.adds)
    {
        Creature* add = ObjectAccessor::GetCreature(*bot, guid);
        if (!Usable(bot, add))
            continue;

        int const tier = AddTier(add, physical);
        if (tier < 0)
            continue;

        bool better = !best || tier < bestTier;
        if (best && tier == bestTier && best != current)
            better = add == current || add->GetHealthPct() < best->GetHealthPct() ||
                     (add->GetHealthPct() == best->GetHealthPct() && add->GetGUID() < best->GetGUID());

        if (better)
        {
            best = add;
            bestTier = tier;
        }
    }

    if (best)
    {
        why = AddName(best);
        return best;
    }

    if (Usable(bot, boss))
    {
        why = "boss";
        return boss;
    }

    return nullptr;
}

// Bot tanks alive on the map and not dominated, the main tank first.
std::vector<Player*> BotTanks(Player* bot)
{
    std::vector<Player*> tanks;
    for (Player* member : MembersOnMap(bot))
        if (IsDutyBot(member) && IsTankPlayer(member) && !IsDominated(member))
            tanks.push_back(member);

    std::sort(tanks.begin(), tanks.end(),
              [](Player* a, Player* b)
              {
                  bool const mainA = PlayerbotAI::IsMainTank(a);
                  bool const mainB = PlayerbotAI::IsMainTank(b);
                  return mainA != mainB ? mainA : a->GetGUID() < b->GetGUID();
              });
    return tanks;
}

bool HeldByTank(Unit* add)
{
    Unit* victim = add->GetVictim();
    return victim && victim->IsPlayer() && IsTankPlayer(victim->ToPlayer());
}

// While the barrier holds each tank owns the adds on one side of her, the main tank the south door.
// After it, the tank she is on keeps her and the others take any loose add.
Unit* TankPick(Player* bot, Creature* boss, LdwState& state, Unit* current, char const*& why)
{
    std::vector<Player*> const tanks = BotTanks(bot);
    auto const self = std::find(tanks.begin(), tanks.end(), bot);
    if (self == tanks.end())
        return nullptr;

    bool const barrier = boss->HasAura(SPELL_LDW_MANA_BARRIER);
    if (!barrier && boss->GetVictim() == bot)
    {
        why = "boss";
        return boss;
    }

    Unit* loose = nullptr;
    Unit* held = nullptr;
    for (ObjectGuid const& guid : state.adds)
    {
        Creature* add = ObjectAccessor::GetCreature(*bot, guid);
        if (!Usable(bot, add))
            continue;

        if (add->GetVictim() == bot)
        {
            if (!held || add == current || (held != current && bot->GetExactDist2d(add) < bot->GetExactDist2d(held)))
                held = add;
            continue;
        }

        if (HeldByTank(add))
            continue;

        if (barrier)
        {
            size_t const side = add->GetPositionY() < boss->GetPositionY() ? 0 : 1;
            if (tanks[side % tanks.size()] != bot)
                continue;
        }

        if (!loose || bot->GetExactDist2d(add) < bot->GetExactDist2d(loose))
            loose = add;
    }

    if (loose)
    {
        why = "loose";
        return loose;
    }

    if (held)
    {
        why = "held";
        return held;
    }

    if (Usable(bot, boss))
    {
        why = "boss";
        return boss;
    }

    return nullptr;
}

// Per bot and ms: the rules ask these for every action the bot weighs, and a bot never ticks twice in
// one ms.
struct TargetMemo
{
    ObjectGuid bot;
    uint32 ms = 0;
    Unit* pick = nullptr;
};

struct HazardMemo
{
    ObjectGuid bot;
    uint32 ms = 0;
    std::vector<LdwHazard> hazards;
};

template <class Build>
std::vector<LdwHazard> const& Memoised(HazardMemo& memo, Player* bot, Build&& build)
{
    uint32 const now = getMSTime();
    if (memo.bot != bot->GetGUID() || memo.ms != now)
    {
        memo.bot = bot->GetGUID();
        memo.ms = now;
        memo.hazards = build();
    }

    return memo.hazards;
}

std::vector<LdwHazard> BuildHazards(PlayerbotAI* botAI);
std::vector<LdwHazard> BuildDarkReckoningHazards(PlayerbotAI* botAI);

Unit* ResolveTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* boss = LdwBoss(botAI);
    if (!boss || IsDominated(bot))
        return nullptr;

    bool const tank = IsTankPlayer(bot);
    if (!tank && !IsRaidDpsBot(bot))
        return nullptr;

    char const* why = "none";
    Unit* pick = GetRaidHumanSkull(botAI, &IsLdwFightUnit);
    if (pick)
        why = "skull";
    else
    {
        LdwState& state = Scan(bot, boss);
        Unit* const current = CurrentTarget(botAI);
        pick = tank ? TankPick(bot, boss, state, current, why) : DpsPick(bot, boss, state, current, why);
    }

    RaidObs::NoteDerived(bot, "ldw.target", why);
    return pick;
}

uint32 HeldForMs(Unit* victim)
{
    int32 longest = 0;
    for (auto const& [id, application] : victim->GetAppliedAuras())
    {
        Aura const* aura = application->GetBase();
        if (!(aura->GetSpellInfo()->GetAllEffectsMechanicMask() & LDW_CC_MECHANICS))
            continue;

        longest = std::max(longest, aura->GetDuration());
    }

    return static_cast<uint32>(longest);
}

bool NeedsCc(Unit* victim)
{
    return victim && victim->IsAlive() && IsDominated(victim) &&
           static_cast<int32>(HeldForMs(victim)) <= LDW_CC_HELD_MS;
}

float SpellRange(PlayerbotAI* memberAI, Player* member, Unit* victim, char const* spell)
{
    uint32 const spellId = memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return info ? member->GetSpellMaxRangeForTarget(victim, info) : 0.0f;
}

// The member's best CC spell that it can cast at the victim now, range aside, with its rank.
char const* ReadyCcSpell(Player* member, Unit* victim, size_t& rank)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    if (!memberAI)
        return nullptr;

    for (size_t i = 0; i < std::size(LDW_CC_SPELLS); ++i)
    {
        if (LDW_CC_SPELLS[i].cls != member->getClass() || !memberAI->CanCastSpell(LDW_CC_SPELLS[i].name, victim))
            continue;

        rank = i;
        return LDW_CC_SPELLS[i].name;
    }

    return nullptr;
}

bool CanPayFor(Player* bot, SpellInfo const* info)
{
    // POWER_HEALTH is -2 and Powers is signed, hence the cast.
    if (info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return true;

    return static_cast<int32>(bot->GetPower(Powers(info->PowerType))) >=
           info->CalcPowerCost(bot, info->GetSchoolMask());
}

bool CastingFrostbolt(Unit* boss)
{
    return boss->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_LDW_FROSTBOLT, boss)) != nullptr;
}

bool CastingMartyrdom(Unit* add)
{
    return add->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_LDW_DARK_MARTYRDOM_FANATIC, add)) ||
           add->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_LDW_DARK_MARTYRDOM_ADHERENT, add));
}

bool IsHeroic(Player* bot)
{
    Difficulty const difficulty = GetRaidMapDifficulty(bot);
    return difficulty == RAID_DIFFICULTY_10MAN_HEROIC || difficulty == RAID_DIFFICULTY_25MAN_HEROIC;
}

uint8 TouchStacks(Unit* unit)
{
    Aura const* touch = unit->GetAura(SPELL_TOUCH_OF_INSIGNIFICANCE);
    return touch ? touch->GetStackAmount() : 0;
}

bool Inside(Player* bot, LdwHazard const& hazard) { return bot->GetExactDist2d(hazard.pos) < hazard.trigger; }

char const* KindName(LdwHazardKind kind)
{
    switch (kind)
    {
        case LdwHazardKind::DeathAndDecay:
            return "dnd";
        case LdwHazardKind::Martyrdom:
            return "martyrdom";
        case LdwHazardKind::Shade:
            return "shade";
        case LdwHazardKind::Blast:
            return "blast";
        case LdwHazardKind::DarkReckoning:
            return "reckoning";
        default:
            return "none";
    }
}

// Most lethal first, the order a bot inside several reports them.
int Danger(LdwHazardKind kind)
{
    switch (kind)
    {
        case LdwHazardKind::Shade:
            return 0;
        case LdwHazardKind::Blast:
            return 1;
        case LdwHazardKind::Martyrdom:
            return 2;
        default:
            return 3;
    }
}

void HeroicBuffs(PlayerbotAI* botAI, Creature* boss)
{
    Player* bot = botAI->GetBot();
    if (!sPlayerbotAIConfig.EnableICCBuffs || !IsHeroic(bot) || !boss->IsInCombat())
        return;

    if (!bot->HasAura(SPELL_NO_THREAT) && !PlayerbotAI::IsTank(bot) && botAI->HasAggro(boss))
    {
        bot->AddAura(SPELL_NO_THREAT, bot);
        IccNoteCheat(bot, "ldw.nothreat");
    }

    if (PlayerbotAI::IsMainTank(bot) && !bot->HasAura(SPELL_SPITEFULL_FURY) && boss->GetVictim() != bot)
    {
        bot->AddAura(SPELL_SPITEFULL_FURY, bot);
        IccNoteCheat(bot, "ldw.spitefulfury");
    }
}
}  // namespace

Creature* LdwBoss(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->GetMapId() != ICC_MAP_ID || !IccDeathwhisperDefinition().Live(botAI))
        return nullptr;

    InstanceScript* instance = bot->GetInstanceScript();
    Creature* boss = instance ? ObjectAccessor::GetCreature(*bot, instance->GetGuidData(ICC_BOSS_LADY_DEATHWHISPER))
                              : nullptr;
    return boss && boss->IsAlive() ? boss : nullptr;
}

bool LdwBarrierUp(Player* bot, Unit* boss)
{
    bool const barrier = boss->HasAura(SPELL_LDW_MANA_BARRIER);
    RaidObs::NoteDerived(bot, "ldw.phase", barrier ? "1" : "2");
    return barrier;
}

Unit* LdwTarget(PlayerbotAI* botAI)
{
    // Asked by the trigger, the action and the targeting rule for every action the bot weighs.
    thread_local TargetMemo memo;
    Player* bot = botAI->GetBot();
    uint32 const now = getMSTime();
    if (memo.bot != bot->GetGUID() || memo.ms != now)
    {
        memo.bot = bot->GetGUID();
        memo.ms = now;
        memo.pick = ResolveTarget(botAI);
    }

    return memo.pick;
}

Unit* LdwLooseAddToTaunt(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsTankPlayer(bot))
        return nullptr;

    Unit* pick = LdwTarget(botAI);
    if (!pick || pick->GetEntry() == NPC_LADY_DEATHWHISPER || pick->GetVictim() == bot || HeldByTank(pick))
        return nullptr;

    return bot->GetExactDist2d(pick) <= LDW_TAUNT_RANGE && EncounterHelpers::ClassTauntReady(botAI) ? pick : nullptr;
}

bool LdwSwapTaunt(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* boss = LdwBoss(botAI);
    if (!boss || IsHeroic(bot) || boss->HasAura(SPELL_LDW_MANA_BARRIER) || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return false;

    Unit* victim = boss->GetVictim();
    if (!victim || victim == bot)
        return false;

    bool const victimIsTank = victim->IsPlayer() && IsTankPlayer(victim->ToPlayer());
    if (victimIsTank && TouchStacks(victim) < LDW_SWAP_STACKS)
        return false;

    auto canTake = [boss, victimIsTank](Player* tank)
    {
        PlayerbotAI* tankAI = GET_PLAYERBOT_AI(tank);
        return tankAI && (!victimIsTank || !TouchStacks(tank)) && tank->GetExactDist2d(boss) <= LDW_TAUNT_RANGE &&
               EncounterHelpers::ClassTauntReady(tankAI);
    };

    if (!canTake(bot))
        return false;

    for (Player* tank : BotTanks(bot))
        if (tank != bot && tank != victim && tank->GetGUID() < bot->GetGUID() && canTake(tank))
            return false;

    return true;
}

Player* LdwRedirectTank(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* boss = LdwBoss(botAI);
    if (!boss || boss->HasAura(SPELL_LDW_MANA_BARRIER))
        return nullptr;

    Unit* victim = boss->GetVictim();
    if (victim && victim != bot && victim->IsPlayer() && IsTankPlayer(victim->ToPlayer()))
        return victim->ToPlayer();

    Player* mainTank = EncounterHelpers::GetGroupMainTank(bot);
    return mainTank != bot ? mainTank : nullptr;
}

void LdwUpdateCcDuties(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* boss = LdwBoss(botAI);
    if (!boss)
        return;

    LdwState& state = sLdwState.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.ccUpdatedMs && getMSTimeDiff(state.ccUpdatedMs, now) < LDW_CC_UPDATE_MS)
        return;
    state.ccUpdatedMs = now;

    std::vector<Player*> const members = MembersOnMap(bot);
    auto member = [&members](ObjectGuid guid) -> Player*
    {
        auto const found =
            std::find_if(members.begin(), members.end(), [guid](Player* p) { return p->GetGUID() == guid; });
        return found == members.end() ? nullptr : *found;
    };

    std::vector<ObjectGuid> released;
    for (auto const& [caster, victimGuid] : state.ccDuty)
    {
        Player* holder = member(caster);
        auto const since = state.ccSince.find(caster);
        bool const stale = since == state.ccSince.end() || getMSTimeDiff(since->second, now) > LDW_CC_DUTY_TIMEOUT_MS;
        if (!holder || IsDominated(holder) || !NeedsCc(member(victimGuid)) || stale)
            released.push_back(caster);
    }

    for (ObjectGuid const& caster : released)
    {
        state.ccDuty.erase(caster);
        state.ccSince.erase(caster);
    }

    std::vector<Player*> victims;
    for (Player* candidate : members)
        if (NeedsCc(candidate))
            victims.push_back(candidate);

    std::sort(victims.begin(), victims.end(), [](Player* a, Player* b) { return a->GetGUID() < b->GetGUID(); });

    for (Player* victim : victims)
    {
        bool covered = false;
        for (auto const& duty : state.ccDuty)
            covered = covered || duty.second == victim->GetGUID();
        if (covered)
            continue;

        Player* best = nullptr;
        size_t bestRank = 0;
        float bestDist = 0.0f;
        for (Player* caster : members)
        {
            if (caster == victim || !IsDutyBot(caster) || IsTankPlayer(caster) || IsDominated(caster) ||
                state.ccDuty.count(caster->GetGUID()))
                continue;

            size_t rank = 0;
            char const* spell = ReadyCcSpell(caster, victim, rank);
            if (!spell)
                continue;

            float const dist = caster->GetExactDist2d(victim);
            if (dist > SpellRange(GET_PLAYERBOT_AI(caster), caster, victim, spell) + LDW_CC_WALK)
                continue;

            if (!best || rank < bestRank || (rank == bestRank && dist < bestDist))
            {
                best = caster;
                bestRank = rank;
                bestDist = dist;
            }
        }

        if (best)
        {
            state.ccDuty[best->GetGUID()] = victim->GetGUID();
            state.ccSince[best->GetGUID()] = now;
        }
    }
}

Unit* LdwCcDuty(PlayerbotAI* botAI, char const*& spell)
{
    Player* bot = botAI->GetBot();
    if (!LdwBoss(botAI))
        return nullptr;

    LdwState* state = sLdwState.Find(bot->GetInstanceId());
    if (!state)
        return nullptr;

    auto const duty = state->ccDuty.find(bot->GetGUID());
    if (duty == state->ccDuty.end())
        return nullptr;

    Player* victim = ObjectAccessor::GetPlayer(*bot, duty->second);
    if (!NeedsCc(victim) || !victim->IsInMap(bot))
        return nullptr;

    size_t rank = 0;
    spell = ReadyCcSpell(bot, victim, rank);
    return spell ? victim : nullptr;
}

bool LdwVictimInSplash(PlayerbotAI* botAI, float range)
{
    Player* bot = botAI->GetBot();
    Unit* target = CurrentTarget(botAI);
    for (Player* member : MembersOnMap(bot))
    {
        if (member == bot || !IsDominated(member) || member->HasAura(SPELL_CYCLONE))
            continue;

        if (bot->GetExactDist2d(member) <= range || (target && target->GetExactDist2d(member) <= range))
            return true;
    }

    return false;
}

char const* LdwReadyInterrupt(Player* bot, Unit* boss)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI || bot->HasUnitState(UNIT_STATE_CASTING))
        return nullptr;

    // She is immune to silence, so Silence and Silencing Shot never land.
    static char const* const interrupts[] = {"kick",        "pummel",       "shield bash",
                                             "mind freeze", "counterspell", "wind shear"};

    for (char const* interrupt : interrupts)
    {
        uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", interrupt)->Get();
        SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!info || !CanPayFor(bot, info))
            continue;

        float const range = bot->GetSpellMaxRangeForTarget(boss, info);
        bool const inReach =
            range <= LDW_MELEE_INTERRUPT_RANGE ? bot->IsWithinMeleeRange(boss) : bot->IsWithinCombatRange(boss, range);
        if (inReach && botAI->CanCastSpell(spellId, boss))
            return interrupt;
    }

    return nullptr;
}

bool LdwIsFrostboltInterrupter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* boss = LdwBoss(botAI);
    if (!boss)
        return false;

    bool interrupter = IsDutyBot(bot) && CastingFrostbolt(boss) && LdwReadyInterrupt(bot, boss);
    if (interrupter)
    {
        for (Player* member : MembersOnMap(bot))
        {
            if (member->GetGUID() < bot->GetGUID() && IsDutyBot(member) && LdwReadyInterrupt(member, boss))
            {
                interrupter = false;
                break;
            }
        }
    }

    RaidObs::NoteDerived(bot, "ldw.interrupter", interrupter ? "1" : "0");
    return interrupter;
}

std::vector<LdwHazard> LdwHazardsFor(PlayerbotAI* botAI)
{
    thread_local HazardMemo memo;
    return Memoised(memo, botAI->GetBot(), [botAI] { return BuildHazards(botAI); });
}

namespace
{
std::vector<LdwHazard> BuildHazards(PlayerbotAI* botAI)
{
    std::vector<LdwHazard> hazards;
    Player* bot = botAI->GetBot();
    Creature* boss = LdwBoss(botAI);
    if (!boss || IsDominated(bot))
        return hazards;

    LdwState& state = Scan(bot, boss);
    for (Position const& pool : state.deathAndDecay)
        hazards.push_back({pool, LDW_DND_REACH, LDW_DND_REACH + LDW_DODGE_MARGIN, LdwHazardKind::DeathAndDecay});

    for (ObjectGuid const& guid : state.adds)
    {
        Creature* add = ObjectAccessor::GetCreature(*bot, guid);
        if (add && add->IsAlive() && CastingMartyrdom(add))
            hazards.push_back({add->GetPosition(), LDW_MARTYRDOM_REACH, LDW_MARTYRDOM_REACH + LDW_DODGE_MARGIN,
                               LdwHazardKind::Martyrdom});
    }

    bool const heroic = IsHeroic(bot);
    float const blast = IsRaid25Man(bot) ? LDW_BLAST_REACH_25H : LDW_BLAST_REACH_10H;
    for (ObjectGuid const& guid : state.shades)
    {
        Creature* shade = ObjectAccessor::GetCreature(*bot, guid);
        Unit* victim = shade && shade->IsAlive() ? shade->GetVictim() : nullptr;
        if (!victim)
            continue;

        if (victim == bot)
            hazards.push_back({shade->GetPosition(), LDW_SHADE_TRIGGER, LDW_SHADE_CLEAR, LdwHazardKind::Shade});
        else if (heroic && victim->IsPlayer() && shade->GetExactDist2d(victim) < LDW_BLAST_IMMINENT)
            hazards.push_back({victim->GetPosition(), blast, blast + LDW_DODGE_MARGIN, LdwHazardKind::Blast});
    }

    return hazards;
}
}  // namespace

LdwHazardKind LdwDodgeKind(PlayerbotAI* botAI)
{
    LdwHazardKind kind = LdwHazardKind::None;
    Player* bot = botAI->GetBot();
    for (LdwHazard const& hazard : LdwHazardsFor(botAI))
        if (Inside(bot, hazard) && (kind == LdwHazardKind::None || Danger(hazard.kind) < Danger(kind)))
            kind = hazard.kind;

    RaidObs::NoteDerived(bot, "ldw.dodge", KindName(kind));
    return kind;
}

bool LdwNearHazard(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Unit* target = PlayerbotAI::IsMelee(bot) ? CurrentTarget(botAI) : nullptr;
    for (LdwHazard const& hazard : LdwHazardsFor(botAI))
    {
        if (bot->GetExactDist2d(hazard.pos) < hazard.clear)
            return true;

        // A melee bot's reach would walk it into the pool its target stands in.
        if (target && hazard.kind == LdwHazardKind::DeathAndDecay &&
            target->GetExactDist2d(hazard.pos) < hazard.trigger)
            return true;
    }

    return false;
}

std::vector<LdwHazard> LdwDarkReckoningHazards(PlayerbotAI* botAI)
{
    thread_local HazardMemo memo;
    return Memoised(memo, botAI->GetBot(), [botAI] { return BuildDarkReckoningHazards(botAI); });
}

namespace
{
std::vector<LdwHazard> BuildDarkReckoningHazards(PlayerbotAI* botAI)
{
    std::vector<LdwHazard> hazards;
    Player* bot = botAI->GetBot();
    if (bot->GetMapId() != ICC_MAP_ID)
        return hazards;

    std::vector<Player*> const members = MembersOnMap(bot);
    bool const marked = bot->HasAura(SPELL_DARK_RECKONING);
    if (!marked && std::none_of(members.begin(), members.end(),
                                [](Player* member) { return member->HasAura(SPELL_DARK_RECKONING); }))
        return hazards;

    // A marked tank keeps the priest where he is and a player decides for himself, so the raid clears them.
    bool const walksAway = marked && !IsTankPlayer(bot);
    for (Player* member : members)
    {
        if (member == bot)
            continue;

        bool const holdsStill =
            member->HasAura(SPELL_DARK_RECKONING) && (IsTankPlayer(member) || !IsDutyBot(member));
        if (walksAway || holdsStill)
            hazards.push_back({member->GetPosition(), LDW_DARK_RECKONING_REACH,
                               LDW_DARK_RECKONING_REACH + LDW_DODGE_MARGIN, LdwHazardKind::DarkReckoning});
    }

    return hazards;
}
}  // namespace

bool LdwDarkReckoningDodging(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    for (LdwHazard const& hazard : LdwDarkReckoningHazards(botAI))
        if (Inside(bot, hazard))
            return true;

    return false;
}

RaidBurstWindow IccDeathwhisperBurstWindow(PlayerbotAI* botAI)
{
    RaidBurstWindow window;
    Creature* boss = LdwBoss(botAI);
    window.allowLust = boss && !boss->HasAura(SPELL_LDW_MANA_BARRIER);
    return window;
}

void LdwTick(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    Creature* boss = LdwBoss(botAI);
    if (!boss)
    {
        if (sLdwState.Find(bot->GetInstanceId()))
            sLdwState.Reset(bot->GetInstanceId());
        return;
    }

    LdwBarrierUp(bot, boss);
    HeroicBuffs(botAI, boss);
    LdwUpdateCcDuties(botAI);
}

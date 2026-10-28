/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Dogs.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "Creature.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCHelpers_Shared.h"
#include "ICCSeams.h"
#include "Map.h"
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
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

namespace
{
constexpr uint32 HUMAN_CALL_CHECK_MS = 500;
constexpr float TAUNT_RANGE = 30.0f;
constexpr float ZOMBIE_RANGE = 30.0f;

struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
};

struct DogsState
{
    uint32 humanCallMs = 0;
    bool humanCallRan = false;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
};

RaidInstanceState<DogsState> dogsStates;

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

// A bot of its own, not a player, selfbot included: players hold no duty.
bool IsDutyBot(Player* player) { return GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

bool IsDog(Unit* unit) { return unit && (unit->GetEntry() == NPC_STINKY || unit->GetEntry() == NPC_PRECIOUS); }

bool IsZombie(Unit* unit) { return unit && unit->GetEntry() == NPC_DOGS_PLAGUED_ZOMBIE; }

bool IsDogsUnit(Unit* unit) { return IsDog(unit) || IsZombie(unit); }

void Note(Player* bot, char const* key, char const* value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

uint32 Wounds(Unit* unit)
{
    Aura const* aura = unit->GetAura(SPELL_DOGS_MORTAL_WOUND);
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

// Off cooldown and castable in its current form: Growl needs bear form and Taunt Defensive Stance
bool TauntUsable(Player* member)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    char const* taunt = memberAI ? TauntName(member) : nullptr;
    uint32 const spellId = taunt ? memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", taunt)->Get() : 0;
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    return info && !member->HasSpellCooldown(spellId) &&
           info->CheckShapeshift(member->GetShapeshiftForm()) == SPELL_CAST_OK;
}

bool Yields(Player* bot, Creature* dog)
{
    return IsDutyBot(bot) && IsTankPlayer(bot) && dog->GetVictim() != bot && bot->HasAura(SPELL_DOGS_MORTAL_WOUND);
}

// The tank to take the dog now. A taunt aura on it means one just landed and its victim catches up next
// update, so nobody else spends one.
Player* SwapTaunter(Player* bot, Creature* dog)
{
    Unit* victim = dog->GetVictim();
    if (!victim || dog->HasAuraType(SPELL_AURA_MOD_TAUNT))
        return nullptr;

    Player* holder = victim->ToPlayer();
    bool const tankHolds = holder && IsTankPlayer(holder);
    uint32 const held = tankHolds ? Wounds(holder) : 0;
    if (tankHolds && held < DOGS_SWAP_STACKS)
        return nullptr;

    Player* pick = nullptr;
    uint32 pickWounds = 0;
    for (Player* member : MembersOnMap(bot))
    {
        if (member == victim || !IsDutyBot(member) || !IsTankPlayer(member) ||
            member->GetExactDist2d(dog) > TAUNT_RANGE || !TauntUsable(member))
            continue;

        uint32 const wounds = Wounds(member);
        if (tankHolds && wounds + DOGS_SWAP_LEAD > held)
            continue;

        if (!pick || wounds < pickWounds || (wounds == pickWounds && member->GetGUID() < pick->GetGUID()))
        {
            pick = member;
            pickWounds = wounds;
        }
    }

    return pick;
}

// A zombie on a player who is no tank, nearest the bot
Unit* LooseZombie(Player* bot)
{
    Unit* pick = nullptr;
    float pickDistance = 0.0f;
    for (Creature* zombie : IccGetCreaturesByEntries(bot, {NPC_DOGS_PLAGUED_ZOMBIE}, ZOMBIE_RANGE))
    {
        Unit* victim = zombie->IsInCombat() ? zombie->GetVictim() : nullptr;
        Player* player = victim ? victim->ToPlayer() : nullptr;
        if (!player || IsTankPlayer(player))
            continue;

        float const distance = bot->GetExactDist2d(zombie);
        if (!pick || distance < pickDistance || (distance == pickDistance && zombie->GetGUID() < pick->GetGUID()))
        {
            pick = zombie;
            pickDistance = distance;
        }
    }

    return pick;
}

Unit* ResolveTarget(PlayerbotAI* botAI, Creature* dog)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if (!tank && !IsRaidDpsBot(bot))
        return nullptr;

    if (tank && dog->GetVictim() == bot)
    {
        Note(bot, "dogs.target", "dog");
        return dog;
    }

    bool const yielding = tank && Yields(bot, dog);
    char const* why = "skull";
    Unit* pick = GetRaidHumanSkull(botAI, yielding ? &IsZombie : &IsDogsUnit);
    if (!pick && tank)
    {
        why = "zombie";
        pick = LooseZombie(bot);
    }

    if (!pick && !yielding)
    {
        why = "dog";
        pick = dog;
    }

    Note(bot, "dogs.target", pick ? why : "none");
    return pick;
}

bool CastingDecimate(Creature* dog)
{
    Spell* spell = dog->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    return spell && spell->GetSpellInfo() && spell->GetSpellInfo()->Id == SPELL_DOGS_DECIMATE;
}

// A human tank has no trigger, so it gets told: its taunt when the bot holding the dog is due a swap with no
// bot to take it, a defensive through Decimate or when low
void CallHumanTank(DogsState& state, Player* sender, Creature* dog, uint32 now)
{
    if (state.humanCallRan && getMSTimeDiff(state.humanCallMs, now) < HUMAN_CALL_CHECK_MS)
        return;

    state.humanCallRan = true;
    state.humanCallMs = now;

    Unit* victim = dog->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (!holder || !holder->IsAlive() || !IsTankPlayer(holder))
        return;

    if (IsDutyBot(holder))
    {
        uint32 const held = Wounds(holder);
        if (held < DOGS_SWAP_STACKS || dog->HasAuraType(SPELL_AURA_MOD_TAUNT) || SwapTaunter(sender, dog))
            return;

        for (Player* member : MembersOnMap(sender))
        {
            uint32 const taunt = IsDutyBot(member) ? 0 : TauntSpellId(member);
            if (taunt && member != holder && IsTankPlayer(member) && Wounds(member) + DOGS_SWAP_LEAD <= held &&
                member->HasSpell(taunt) && !member->HasSpellCooldown(taunt))
            {
                CallHumanTankDefensive(sender, member, taunt, 0, "Mortal Wound is stacking on your partner",
                                       "dogs.humancall");
                return;
            }
        }

        return;
    }

    bool const decimate = CastingDecimate(dog);
    bool const low = holder->GetHealthPct() < DOGS_TANK_DEFENSIVE_PCT;
    if (!decimate && !low)
        return;

    Spell* spell = dog->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    uint32 const hitIn = decimate && spell ? static_cast<uint32>(std::max(spell->GetCastTimeRemaining(), 0)) : 0;
    CallHumanTankDefensive(sender, holder, ReadyTankDefensiveSpell(holder, true, 0, 0), hitIn,
                           decimate ? "Decimate" : "you are low", "dogs.humancall");
}
}  // namespace

Creature* GetDogsBoss(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->IsAlive())
        return nullptr;

    Creature* dog = UnitGateLiveUnit<NPC_STINKY, NPC_PRECIOUS>(botAI);
    return dog && dog->IsAlive() && dog->IsInCombat() ? dog : nullptr;
}

bool IsDogsLive(PlayerbotAI* botAI) { return GetDogsBoss(botAI) != nullptr; }

Unit* GetDogsTarget(PlayerbotAI* botAI)
{
    Creature* dog = GetDogsBoss(botAI);
    Player* bot = botAI->GetBot();
    DogsState* state = dog && bot->GetInstanceId() ? dogsStates.Find(bot->GetInstanceId()) : nullptr;
    if (!state)
        return nullptr;

    // Asked by the trigger, the action and the targeting rule for every action a bot weighs
    TargetMemo& memo = state->targets[bot->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveTarget(botAI, dog);
    }

    return memo.pick;
}

bool IsDogsSwapTaunter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* dog = GetDogsBoss(botAI);
    if (!dog || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return false;

    bool const elected = SwapTaunter(bot, dog) == bot;
    Note(bot, "dogs.swap", elected ? "1" : "0");
    return elected;
}

bool IsDogsYielding(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* dog = GetDogsBoss(botAI);
    if (!dog || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return false;

    bool const yielding = Yields(bot, dog);
    Note(bot, "dogs.yield", yielding ? "1" : "0");
    return yielding;
}

uint32 GetDogsTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    Creature* dog = GetDogsBoss(botAI);
    Player* bot = botAI->GetBot();
    if (!dog || !IsDutyBot(bot) || !IsTankPlayer(bot))
        return 0;

    bool const window =
        bot->GetHealthPct() < DOGS_TANK_DEFENSIVE_PCT || (dog->GetVictim() == bot && CastingDecimate(dog));
    if (!window)
    {
        // Change-latched, so the way out of a window is written too
        if (noteKind)
            Note(bot, noteKind, "idle");

        return 0;
    }

    return NextTankDefensive(botAI, bot, noteKind, true);
}

bool IsDogsTankDefensiveHeld(PlayerbotAI* botAI)
{
    return IsDogsLive(botAI) && botAI->GetBot()->GetHealthPct() >= DOGS_TANK_LAST_RESORT_PCT;
}

bool GetDogsDecimateHit(PlayerbotAI* botAI, RaidPredictedHit& hit)
{
    Creature* dog = GetDogsBoss(botAI);
    Aura const* stun = dog ? dog->GetAura(SPELL_DOGS_DECIMATE) : nullptr;
    if (!stun)
        return false;

    // Stamped on the landing, so every read of one Decimate is the same hit
    uint32 const now = getMSTime();
    int32 const since = std::max(stun->GetMaxDuration() - stun->GetDuration(), 0);
    hit = RaidPredictedHit();
    hit.startMs = now - static_cast<uint32>(since);
    hit.durationMs = DOGS_DECIMATE_HIT_MS;
    hit.school = SPELL_SCHOOL_MASK_NORMAL;

    // Everyone sits at 15%, under the cover floor whatever the damage: Precious's zombies count as nothing
    if (dog->GetEntry() == NPC_STINKY)
    {
        SpellInfo const* tick =
            sSpellMgr->GetSpellInfo(sSpellMgr->GetSpellIdForDifficulty(SPELL_DOGS_PLAGUE_STENCH_TICK, dog));
        if (tick)
        {
            SpellEffectInfo const& effect = tick->Effects[EFFECT_0];
            hit.damage = 2 * static_cast<uint32>(std::max(effect.BasePoints + (effect.DieSides + 1) / 2, 0));
            hit.school = SPELL_SCHOOL_MASK_NATURE;
        }
    }

    return true;
}

RaidCover GetDogsDecimateCover(PlayerbotAI* botAI)
{
    if (!IsDogsLive(botAI))
        return RaidCover::None;

    RaidPredictedHit hit;
    bool const hits = GetDogsDecimateHit(botAI, hit);
    RaidCover const cover = GetRaidCover(botAI, ICC_DEFENSIVE_SCOPE, hits ? &hit : nullptr);
    // Precious's zombies hit physical, where Anti-Magic Zone takes nothing off
    return cover == RaidCover::AntiMagicZone && hit.school == SPELL_SCHOOL_MASK_NORMAL ? RaidCover::None : cover;
}

void TickDogsEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    Creature* dog = UnitGateLiveUnit<NPC_STINKY, NPC_PRECIOUS>(botAI);
    if (!dog)
    {
        if (dogsStates.Find(bot->GetInstanceId()))
            dogsStates.Reset(bot->GetInstanceId());
        return;
    }

    IccMarkPull(bot, dog, "stinky-and-precious");

    DogsState& state = dogsStates.For(bot->GetInstanceId());
    if (bot->IsAlive())
        CallHumanTank(state, bot, dog, getMSTime());
}

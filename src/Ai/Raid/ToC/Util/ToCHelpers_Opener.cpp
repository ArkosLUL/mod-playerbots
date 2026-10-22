#include "ToCHelpers_Opener.h"

#include <list>
#include <vector>

#include "Creature.h"
#include "Group.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidRedirectThreat.h"
#include "Timer.h"
#include "ToCData.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Shared.h"

namespace TrialOfTheCrusaderHelpers
{
namespace
{
constexpr uint32 SPELL_MISDIRECTION = 34477;
constexpr uint32 SPELL_TRICKS_OF_THE_TRADE = 57934;
// the redirect after the rogue's first attack
constexpr uint32 SPELL_TRICKS_OF_THE_TRADE_PROC = 59628;

// From anywhere on the arena floor this sees all of it
constexpr float BOSS_SEARCH_RADIUS = 200.0f;

struct OpenerBoss
{
    ObjectGuid guid;
    uint32 engagedMs = 0;
    uint32 tankSinceMs = 0;
    bool tankHeld = false;
    bool released = false;
};

// One read per instance per ms: every DPS bot's multiplier asks per action
struct OpenerState
{
    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value
    std::vector<OpenerBoss> bosses;
};

RaidInstanceState<OpenerState> openerStates;

std::vector<uint32> const& BossEntriesOf(ToCEncounter encounter)
{
    static std::vector<uint32> const beasts = {
        static_cast<uint32>(ToCNpcs::NPC_GORMOK), static_cast<uint32>(ToCNpcs::NPC_ACIDMAW),
        static_cast<uint32>(ToCNpcs::NPC_DREADSCALE), static_cast<uint32>(ToCNpcs::NPC_ICEHOWL)};
    static std::vector<uint32> const jaraxxus = {static_cast<uint32>(ToCNpcs::NPC_JARAXXUS)};
    static std::vector<uint32> const twins = {static_cast<uint32>(ToCNpcs::NPC_FJOLA_LIGHTBANE),
                                              static_cast<uint32>(ToCNpcs::NPC_EYDIS_DARKBANE)};
    static std::vector<uint32> const anubarak = {static_cast<uint32>(ToCNpcs::NPC_ANUBARAK)};
    static std::vector<uint32> const none;

    switch (encounter)
    {
        case ToCEncounter::NorthrendBeasts:
            return beasts;
        case ToCEncounter::Jaraxxus:
            return jaraxxus;
        case ToCEncounter::TwinValkyr:
            return twins;
        case ToCEncounter::Anubarak:
            return anubarak;
        default:
            return none;
    }
}

Player* GroupTankHolding(Unit* boss, Group* group)
{
    Unit* victim = boss ? boss->GetVictim() : nullptr;
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (!holder || !group || holder->GetGroup() != group)
        return nullptr;

    return PlayerbotAI::IsTank(holder) || PlayerbotAI::IsTank(holder, true) ? holder : nullptr;
}

OpenerState const* ReadOpener(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Map* map = bot ? bot->FindMap() : nullptr;
    if (!map || map->GetId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return nullptr;

    OpenerState& state = openerStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.memoValid && state.memoMs == now)
        return &state;

    state.memoMs = now;
    state.memoValid = true;

    // A boss out of combat drops out, so a wipe or a reset starts its opener over
    std::vector<OpenerBoss> next;
    Group* group = bot->GetGroup();
    for (uint32 entry : BossEntriesOf(ToCLiveEncounter(botAI)))
    {
        std::list<Creature*> creatures;
        bot->GetCreatureListWithEntryInGrid(creatures, entry, BOSS_SEARCH_RADIUS);
        for (Creature* creature : creatures)
        {
            if (!creature->IsAlive() || !creature->IsInCombat())
                continue;

            OpenerBoss boss;
            boss.guid = creature->GetGUID();
            boss.engagedMs = now;
            for (OpenerBoss const& known : state.bosses)
            {
                if (known.guid == boss.guid)
                {
                    boss = known;
                    break;
                }
            }

            if (!boss.released)
            {
                if (!GroupTankHolding(creature, group))
                    boss.tankHeld = false;
                else if (!boss.tankHeld)
                {
                    boss.tankHeld = true;
                    boss.tankSinceMs = now;
                }

                boss.released = (boss.tankHeld && getMSTimeDiff(boss.tankSinceMs, now) >= TOC_OPENER_DWELL_MS) ||
                                getMSTimeDiff(boss.engagedMs, now) >= TOC_OPENER_CAP_MS;
            }

            next.push_back(boss);
        }
    }

    state.bosses = std::move(next);
    return &state;
}

bool RedirectRunning(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_HUNTER:
            return bot->HasAura(SPELL_MISDIRECTION) || bot->HasAura(SPELL_MISDIRECTION_PROC);
        case CLASS_ROGUE:
            return bot->HasAura(SPELL_TRICKS_OF_THE_TRADE) || bot->HasAura(SPELL_TRICKS_OF_THE_TRADE_PROC);
        default:
            return false;
    }
}

// null when this bot holds on nothing. Not gated on the bot's own combat: its first hit is the one
// that matters.
OpenerState const* HoldingState(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !IsToCDpsBot(bot) || RedirectRunning(bot))
        return nullptr;

    OpenerState const* state = ReadOpener(botAI);
    if (!state)
        return nullptr;

    for (OpenerBoss const& boss : state->bosses)
        if (!boss.released)
            return state;

    return nullptr;
}

bool Holds(OpenerState const& state, ObjectGuid guid)
{
    for (OpenerBoss const& boss : state.bosses)
        if (!boss.released && boss.guid == guid)
            return true;

    return false;
}
}  // namespace

bool IsToCOpenerHeld(PlayerbotAI* botAI, Unit* unit)
{
    if (!unit)
        return false;

    OpenerState const* state = HoldingState(botAI);
    return state && Holds(*state, unit->GetGUID());
}

Unit* GetToCOpenerHeldBossNear(PlayerbotAI* botAI, float reach)
{
    OpenerState const* state = HoldingState(botAI);
    if (!state)
        return nullptr;

    Player* bot = botAI->GetBot();
    for (OpenerBoss const& boss : state->bosses)
    {
        if (boss.released)
            continue;

        Unit* unit = botAI->GetUnit(boss.guid);
        if (unit && bot->GetExactDist2d(unit) <= reach)
            return unit;
    }

    return nullptr;
}

bool ToCOpenerHolds(PlayerbotAI* botAI) { return HoldingState(botAI) != nullptr; }

bool IsToCOpenerHolding(PlayerbotAI* botAI)
{
    bool const holding = ToCOpenerHolds(botAI);

    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (RaidObs::Active() && bot && bot->GetMapId() == TRIAL_OF_THE_CRUSADER_MAP_ID && IsToCDpsBot(bot))
        RaidObs::NoteDerived(bot, "toc.opener", holding ? "hold" : "free");

    return holding;
}

Player* GetToCOpenerRedirectTank(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE))
        return nullptr;

    OpenerState const* state = HoldingState(botAI);
    if (!state)
        return nullptr;

    Group* group = bot->GetGroup();
    Unit* current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (current && Holds(*state, current->GetGUID()))
        if (Player* tank = GroupTankHolding(current, group))
            return tank;

    for (OpenerBoss const& boss : state->bosses)
    {
        if (boss.released)
            continue;

        if (Player* tank = GroupTankHolding(botAI->GetUnit(boss.guid), group))
            return tank;
    }

    return nullptr;
}

}

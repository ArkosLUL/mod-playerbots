/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCData.h"
#include "ICCEncounterGate.h"
#include "InstanceScript.h"
#include "Map.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "Timer.h"

namespace
{
// Every fight that hands out Experienced or 1% Threat, through Playerbots.EnableICCBuffs or a raid cheat.
constexpr IccEncounterId BUFFING_ENCOUNTERS[] = {ICC_BOSS_LADY_DEATHWHISPER, ICC_BOSS_PROFESSOR_PUTRICIDE,
                                                  ICC_BOSS_SINDRAGOSA, ICC_BOSS_THE_LICH_KING};

constexpr uint32 HUMAN_SWEEP_MS = 1000;

struct HumanSweep
{
    uint32 atMs = 0;
    bool swept = false;  // 0 is a real getMSTime value
};

RaidInstanceState<HumanSweep> humanSweeps;

bool BuffingEncounterInProgress(InstanceScript* instance)
{
    for (IccEncounterId const id : BUFFING_ENCOUNTERS)
        if (instance->GetBossState(id) == IN_PROGRESS)
            return true;

    return false;
}

// Neither aura ever expires.
bool HasPermanentBuff(Player* player)
{
    return player->HasAura(SPELL_EXPERIENCED) || player->HasAura(SPELL_NO_THREAT);
}

void StripPermanentBuffs(Player* player)
{
    if (player->HasAura(SPELL_EXPERIENCED))
        player->RemoveAura(SPELL_EXPERIENCED);

    if (player->HasAura(SPELL_NO_THREAT))
        player->RemoveAura(SPELL_NO_THREAT);
}

// Each bot strips itself. Humans run no strategy, so the first bot past the throttle sweeps them.
void TickHeroicBuffs(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->GetMapId() != ICC_MAP_ID)
        return;

    InstanceScript* instance = bot->GetInstanceScript();
    if (!instance || BuffingEncounterInProgress(instance))
        return;

    if (HasPermanentBuff(bot))
        StripPermanentBuffs(bot);

    Map* map = bot->GetMap();
    uint32 const now = getMSTime();
    HumanSweep& sweep = humanSweeps.For(map->GetInstanceId());
    if (sweep.swept && getMSTimeDiff(sweep.atMs, now) < HUMAN_SWEEP_MS)
        return;

    sweep.atMs = now;
    sweep.swept = true;

    Map::PlayerList const& players = map->GetPlayers();
    for (Map::PlayerList::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        Player* player = it->GetSource();
        if (player && player->IsInWorld() && !GET_PLAYERBOT_AI(player) && HasPermanentBuff(player))
            StripPermanentBuffs(player);
    }
}

void DefineHeroicBuffs(EncounterBuilder& e) { e.Tick(&TickHeroicBuffs); }
}  // namespace

EncounterDefinition const& IccHeroicBuffsDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_NONE, IccRaidWideGate, nullptr, &DefineHeroicBuffs);
    return definition;
}

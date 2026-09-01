/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_YoggSaron.h"

#include "Creature.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

const Position ULDUAR_YOGG_SARON_MIDDLE = Position(1980.28f, -25.5868f, 329.397f);
const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE = Position(1927.1511f, 68.507256f, 242.37657f);
const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE = Position(1925.6553f, -121.59296f, 239.98965f);
const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE = Position(2104.5667f, -25.509348f, 242.64679f);
const Position ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE = Position(1980.1971f, -27.854689f, 236.06789f);
const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE = Position(1954.06f, 21.66f, 239.71f);
const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE = Position(1950.11f, -79.284f, 239.98982f);
const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE = Position(2048.63f, -25.5f, 239.72f);
const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT = Position(1998.5377f, -22.90317f, 324.8895f);
const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT = Position(2018.7628f, -18.896868f, 327.07245f);

bool YoggSaronInPhase2(PlayerbotAI* botAI)
{
    Creature* yogg = botAI->GetBot()->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);

    return yogg && yogg->IsAlive() && yogg->HasAura(SPELL_SHADOW_BARRIER);
}

bool YoggSaronInPhase3(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Creature* yogg = bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true);
    Creature* guardian = bot->FindNearestCreature(NPC_GUARDIAN_OF_YS, 200.0f, true);

    return yogg && yogg->IsAlive() && !yogg->HasAura(SPELL_SHADOW_BARRIER) && !guardian;
}

bool YoggSaronFearWindowActive(PlayerbotAI* botAI) { return YoggSaronInPhase2(botAI) || YoggSaronInPhase3(botAI); }

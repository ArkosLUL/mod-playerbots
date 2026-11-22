/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDSCRIPTS_H
#define PLAYERBOTS_ULDSCRIPTS_H

enum UlduarNPCs
{
    NPC_RAZORSCALE                   = 33186,

    NPC_BRUNDIR                      = 32857,
    NPC_STEELBREAKER                 = 32867,
    NPC_MOLGEIM                      = 32927,

    NPC_KOLOGARN                     = 32930,

    NPC_AURIAYA                      = 33515,

    NPC_XT002                        = 33293,
    NPC_HEART_OF_DECONSTRUCTOR       = 33329,
    NPC_XT_TOY_PILE                  = 33337,
    NPC_XS013_SCRAPBOT               = 33343,

    NPC_THORIM                       = 32865,

    NPC_FREYA                        = 32906,

    NPC_HODIR                        = 32845,

    NPC_MIMIRON                      = 33350,

    NPC_FLAME_LEVIATHAN              = 33113,

    NPC_SALVAGED_SIEGE_ENGINE        = 33060,
    NPC_VEHICLE_CHOPPER              = 33062,
    NPC_SALVAGED_SIEGE_ENGINE_TURRET = 33067,
    NPC_SALVAGED_DEMOLISHER          = 33109,
    NPC_SALVAGED_DEMOLISHER_TURRET   = 33167
};

enum UlduarBossData
{
    ULD_DATA_VEZAX = 11
};

enum UlduarPersistentData
{
    PERSISTENT_DATA_WATCHERS_MASK = 0
};

enum UlduarKeepers
{
    KEEPER_FREYA   = 0,
    KEEPER_HODIR   = 1,
    KEEPER_MIMIRON = 2,
    KEEPER_THORIM  = 3
};

#endif

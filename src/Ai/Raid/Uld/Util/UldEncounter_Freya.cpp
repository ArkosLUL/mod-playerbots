/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Freya.h"

#include "Creature.h"
#include "EncounterHelpers.h"
#include "GameObject.h"
#include "Group.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldScripts.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <vector>

using namespace EncounterHelpers;

const Position ULDUAR_FREYA_TANK_ANCHOR = Position(2360.0847f, -43.1235f, 425.333f);

std::vector<Unit*> FreyaWaveState::LivingTrio() const
{
    std::vector<Unit*> living;
    for (Unit* member : {snaplasher, stormLasher, waterSpirit})
    {
        if (member && member->IsAlive())
            living.push_back(member);
    }

    return living;
}

bool FreyaWaveState::TrioLocked() const
{
    for (Unit* member : LivingTrio())
    {
        if (member->GetHealthPct() < ULDUAR_FREYA_TRIO_SYNC_WINDOW_PCT)
            return true;
    }

    return false;
}

bool FreyaWaveState::TrioReleased() const
{
    std::vector<Unit*> const living = LivingTrio();
    if (living.empty())
        return false;

    for (Unit* member : living)
    {
        if (member->GetHealthPct() > ULDUAR_FREYA_TRIO_FLOOR_RELEASE_PCT)
            return false;
    }

    return true;
}

void GatherFreyaWaveState(PlayerbotAI* botAI, FreyaWaveState& state)
{
    // Waves spawn on a fixed 60s timer whether or not the last one died, so two sets of the same add
    // can be up at once. Keeping the more damaged one finishes the older wave first, and - because
    // the scan order is not stable - it is also what stops the split target flipping between two
    // identical adds from tick to tick.
    auto const keepMoreDamaged = [](Unit*& slot, Unit* candidate)
    {
        if (!slot || candidate->GetHealth() < slot->GetHealth())
            slot = candidate;
    };

    // "possible targets" enforces line of sight, which drops adds behind Freya's tree trunks out of
    // the scan and makes the split disagree between bots standing on opposite sides.
    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        switch (unit->GetEntry())
        {
            case NPC_EONARS_GIFT:
                keepMoreDamaged(state.eonarsGift, unit);
                break;
            case NPC_ANCIENT_CONSERVATOR:
                keepMoreDamaged(state.conservator, unit);
                break;
            case NPC_SNAPLASHER:
                keepMoreDamaged(state.snaplasher, unit);
                break;
            case NPC_STORM_LASHER:
                keepMoreDamaged(state.stormLasher, unit);
                break;
            case NPC_ANCIENT_WATER_SPIRIT:
                keepMoreDamaged(state.waterSpirit, unit);
                break;
            case NPC_DETONATING_LASHER:
                state.detonatingLashers.push_back(unit);
                break;
            default:
                break;
        }
    }
}

bool FreyaTrioSyncSuppress(FreyaWaveState const& state, Unit* target)
{
    if (!target)
        return false;

    if (target != state.snaplasher && target != state.stormLasher && target != state.waterSpirit)
        return false;

    std::vector<Unit*> const living = state.LivingTrio();

    // Below three the window is already open and counting down; holding damage back now only lets the
    // ones already dead come back.
    if (living.size() < 3)
        return false;

    if (state.TrioReleased())
        return false;

    if (target->GetHealthPct() > ULDUAR_FREYA_TRIO_HARD_FLOOR_PCT)
        return false;

    for (Unit* member : living)
    {
        if (member != target && member->GetHealthPct() > ULDUAR_FREYA_TRIO_FLOOR_RELEASE_PCT)
            return true;
    }

    return false;
}

Unit* GetFreyaTrioAssignment(PlayerbotAI* botAI, FreyaWaveState const& state)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    std::vector<Unit*> candidates;
    for (Unit* member : state.LivingTrio())
    {
        if (!FreyaTrioSyncSuppress(state, member))
            candidates.push_back(member);
    }

    // Everything floored at once should be impossible - the release check clears the floor as soon as
    // the last member joins the band - but a bot with nothing to hit would fall through to the boss.
    if (candidates.empty())
        candidates = state.LivingTrio();

    if (candidates.empty())
        return nullptr;

    std::vector<uint32> assigned(candidates.size(), 0);
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsDps(member))
            continue;

        // Whichever member would carry the most remaining health per attacker if this bot joined it.
        // Over the whole group that lands a split proportional to remaining health, which is the
        // quantity that has to reach zero at the same time.
        size_t pick = 0;
        float best = -1.0f;
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            float const share = float(candidates[i]->GetHealth()) / float(assigned[i] + 1);
            if (share > best)
            {
                best = share;
                pick = i;
            }
        }

        ++assigned[pick];

        if (member == bot)
            return candidates[pick];
    }

    return nullptr;
}

Unit* GetFreyaRangedLasherFocus(FreyaWaveState const& state)
{
    Unit* best = nullptr;
    for (Unit* lasher : state.detonatingLashers)
    {
        if (!lasher || !lasher->IsAlive())
            continue;

        // GUID breaks the tie so a wave of untouched lashers does not resolve differently per bot.
        if (!best || lasher->GetHealth() < best->GetHealth() ||
            (lasher->GetHealth() == best->GetHealth() && lasher->GetGUID() < best->GetGUID()))
        {
            best = lasher;
        }
    }

    return best;
}

Unit* GetFreyaLocalLasherTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget, float range)
{
    Player* bot = botAI->GetBot();

    Unit* selected = nullptr;
    if (currentTarget && currentTarget->IsAlive() && currentTarget->GetEntry() == NPC_DETONATING_LASHER &&
        bot->GetExactDist2d(currentTarget) <= range)
    {
        selected = currentTarget;
    }

    // The margin stops two lashers at similar range from trading the bot back and forth every tick.
    constexpr float switchMargin = 10.0f;
    for (Unit* candidate : state.detonatingLashers)
    {
        if (!candidate || !candidate->IsAlive() || candidate == selected)
            continue;

        if (bot->GetExactDist2d(candidate) > range)
            continue;

        if (!selected)
        {
            selected = candidate;
            continue;
        }

        if (candidate->GetExactDist2d(bot) + switchMargin < selected->GetExactDist2d(bot))
            selected = candidate;
    }

    return selected;
}

Unit* GetFreyaConservatorSpore(PlayerbotAI* botAI, Unit* conservator)
{
    if (!conservator || !conservator->IsAlive())
        return nullptr;

    std::list<Creature*> found;
    conservator->GetCreatureListWithEntryInGrid(found, NPC_HEALTHY_SPORE, ULDUAR_FREYA_SPORE_SEARCH_RADIUS);

    // Nearest the Conservator, not the caller: the tank drags the boss to this spore and the melee
    // shelter on it, and two derivations of "which spore" would disagree and oscillate. It is also
    // self-stabilising - once parked, the spore is at distance ~0 and stays nearest until it despawns,
    // while every new one spawns 20 yd out.
    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* spore : found)
    {
        if (!spore || !spore->IsAlive())
            continue;

        float const dist = spore->GetExactDist2d(conservator);
        if (!best || dist < bestDist)
        {
            best = spore;
            bestDist = dist;
        }
    }

    return best;
}

std::vector<Unit*> GetFreyaSpores(PlayerbotAI* botAI)
{
    std::vector<Unit*> spores;
    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == NPC_HEALTHY_SPORE)
            spores.push_back(unit);
    }

    return spores;
}

Unit* GetFreyaTargetSpore(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    // Melee take the spore the Conservator is parked on, not their own nearest - anywhere else and the
    // DPS node drags them back out of the aura to reach the boss, and the two nodes fight all wave.
    if (PlayerbotAI::IsMelee(bot))
        if (Unit* parked = GetFreyaConservatorSpore(botAI, GetFirstAliveUnitByEntry(botAI, NPC_ANCIENT_CONSERVATOR)))
            return parked;

    // Ranged and healers only need the aura, not melee range, so they take the nearest spore that is
    // not already full rather than the nearest one outright. Nearest outright is what piles them up:
    // they start the wave in one ball, so it resolves to the same spore for every one of them, and the
    // melee group is already standing on it.
    //
    // No latch and no shared state: the trigger stands down the moment the bot holds Potent Pheromones,
    // so the choice is only ever made on the way in, and the crowd count settles it the same way for
    // every bot that asks.
    //
    // A spore with an Unstable Sun Beam standing in it is not shelter either. The pool is 5 yd of the
    // spore's 6, so the two overlap over most of their area, and a bot sent there parks in it for the
    // pool's full ten seconds - which is how a healer died, rooted on a spore holding two of them.
    std::vector<Position> const beams = GetFreyaSunBeamPositions(botAI, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);
    auto const pooled = [&beams](Unit* spore)
    {
        for (Position const& beam : beams)
            if (spore->GetExactDist2d(beam.GetPositionX(), beam.GetPositionY()) < ULDUAR_FREYA_SPORE_RADIUS)
                return true;

        return false;
    };

    Unit* nearest = nullptr;
    Unit* clean = nullptr;
    Unit* roomy = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    float cleanDistance = std::numeric_limits<float>::max();
    float roomyDistance = std::numeric_limits<float>::max();
    for (Unit* spore : GetFreyaSpores(botAI))
    {
        float const distance = bot->GetDistance2d(spore);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = spore;
        }

        if (pooled(spore))
            continue;

        if (distance < cleanDistance)
        {
            cleanDistance = distance;
            clean = spore;
        }

        if (distance < roomyDistance &&
            CountFreyaRaidNear(botAI, spore->GetPosition(), ULDUAR_FREYA_SPORE_RADIUS, bot) <
                ULDUAR_FREYA_SPORE_CROWD)
        {
            roomyDistance = distance;
            roomy = spore;
        }
    }

    // Every spore full, or every one of them pooled - three alive at a time against fifteen bots that
    // need one, so both happen. Sheltered and stacked, or sheltered and burning, still beats pacified.
    if (roomy)
        return roomy;

    return clean ? clean : nearest;
}

uint32 CountFreyaRaidNear(PlayerbotAI* botAI, Position const& centre, float radius, Player* except)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    uint32 count = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == except || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (member->GetExactDist2d(centre.GetPositionX(), centre.GetPositionY()) <= radius)
            ++count;
    }

    return count;
}

Unit* GetFreyaNaturesFuryShelter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    Unit* best = nullptr;
    float bestDistance = std::numeric_limits<float>::max();
    for (Unit* spore : GetFreyaSpores(botAI))
    {
        // The carrier itself does not count against the spore it is running to.
        if (CountFreyaRaidNear(botAI, spore->GetPosition(), ULDUAR_FREYA_NATURES_FURY_RADIUS, bot))
            continue;

        float const distance = bot->GetDistance2d(spore);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = spore;
        }
    }

    return best;
}

// Highest health first, and never a suppressed member - see the header for why the tank goes to the
// member furthest from the floor rather than the nearest one.
//
// Percent, not absolute: the floor and release thresholds are percentages, and the three members differ
// by nearly 2x in max health, so absolute health would call the Snaplasher the furthest from dying even
// when it is the closest.
static Unit* GetFreyaTankTrioTarget(FreyaWaveState const& state, Unit* currentTarget)
{
    Unit* best = nullptr;
    for (Unit* member : state.LivingTrio())
    {
        if (FreyaTrioSyncSuppress(state, member))
            continue;

        if (!best || member->GetHealthPct() > best->GetHealthPct())
            best = member;
    }

    if (!best || !currentTarget || currentTarget == best)
        return best;

    // Hold the member the tank is already on until another is clear of it by the margin, or its own
    // damage closing the gap makes it swap every few ticks.
    for (Unit* member : state.LivingTrio())
    {
        if (member != currentTarget || FreyaTrioSyncSuppress(state, member))
            continue;

        if (best->GetHealthPct() - member->GetHealthPct() < ULDUAR_FREYA_TANK_TRIO_SWITCH_PCT)
            return member;

        break;
    }

    return best;
}

Unit* GetFreyaTankTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget)
{
    Player* bot = botAI->GetBot();
    Unit* freya = GetFirstAliveUnitByEntry(botAI, NPC_FREYA);

    if (PlayerbotAI::IsMainTank(bot))
        return freya;

    if (!PlayerbotAI::IsAssistTankOfIndex(bot, 0, true))
        return nullptr;

    if (state.snaplasher && state.snaplasher->IsAlive())
        return state.snaplasher;

    if (state.conservator && state.conservator->IsAlive())
        return state.conservator;

    if (Unit* member = GetFreyaTankTrioTarget(state, currentTarget))
        return member;

    // Same local rule and leash as a melee DPS bot: hit the one standing next to it, never walk one
    // anywhere. Collecting lashers and towing them into the raid is what killed the raid before.
    if (Unit* lasher = GetFreyaLocalLasherTarget(botAI, state, currentTarget, ULDUAR_FREYA_MELEE_LASHER_RANGE))
        return lasher;

    return freya;
}

bool FreyaHasLivingRangedDps(PlayerbotAI* botAI)
{
    Group* group = botAI->GetBot()->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && PlayerbotAI::IsRangedDps(member))
            return true;
    }

    return false;
}

std::vector<Position> GetFreyaNatureBombPositions(Player* bot, float searchRadius)
{
    std::list<GameObject*> bombs;
    bot->GetGameObjectListWithEntryInGrid(bombs, GOBJECT_NATURE_BOMB, searchRadius);

    std::vector<Position> positions;
    positions.reserve(bombs.size());
    for (GameObject* bomb : bombs)
    {
        if (bomb)
            positions.push_back(bomb->GetPosition());
    }

    return positions;
}

std::vector<Position> GetFreyaSunBeamPositions(PlayerbotAI* botAI, float searchRadius)
{
    Player* bot = botAI->GetBot();

    std::vector<Position> beams;
    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_FREYA_SUN_BEAM && unit->GetEntry() != NPC_FREYA_UNSTABLE_SUN_BEAM)
            continue;

        if (bot->GetExactDist2d(unit) < searchRadius)
            beams.push_back(unit->GetPosition());
    }

    return beams;
}

std::vector<EncounterHelpers::HazardCircle> GetFreyaEscapeHazards(PlayerbotAI* botAI, float searchRadius)
{
    Player* bot = botAI->GetBot();

    std::vector<EncounterHelpers::HazardCircle> hazards;
    for (Position const& bomb : GetFreyaNatureBombPositions(bot, searchRadius))
        hazards.emplace_back(bomb, ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS);

    for (Position const& beam : GetFreyaSunBeamPositions(botAI, searchRadius))
        hazards.emplace_back(beam, ULDUAR_FREYA_SUN_BEAM_CLEARANCE);

    return hazards;
}

Player* GetFreyaRangedCampAnchor(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return PlayerbotAI::IsRangedDps(bot) ? bot : nullptr;

    Player* anchor = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (!GET_PLAYERBOT_AI(member) || !PlayerbotAI::IsRangedDps(member))
            continue;

        if (!anchor || member->GetGUID() < anchor->GetGUID())
            anchor = member;
    }

    return anchor;
}

uint32 GetFreyaRangedDpsRank(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsRangedDps(bot))
        return std::numeric_limits<uint32>::max();

    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    uint32 rank = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (!GET_PLAYERBOT_AI(member) || !PlayerbotAI::IsRangedDps(member))
            continue;

        if (member->GetGUID() < bot->GetGUID())
            ++rank;
    }

    return rank;
}

uint32 CountFreyaLashersNear(Position const& centre, FreyaWaveState const& state, float radius)
{
    uint32 count = 0;
    for (Unit* lasher : state.detonatingLashers)
    {
        if (lasher && lasher->IsAlive() && centre.GetExactDist2d(lasher->GetPosition()) <= radius)
            ++count;
    }

    return count;
}

std::vector<Position> GetFreyaLowLasherPositions(PlayerbotAI* botAI, FreyaWaveState const& state, float maxPct,
                                                 float radius)
{
    Player* bot = botAI->GetBot();
    std::vector<Position> low;

    for (Unit* lasher : state.detonatingLashers)
    {
        if (!lasher || !lasher->IsAlive() || lasher->GetHealthPct() >= maxPct)
            continue;

        if (bot->GetExactDist2d(lasher) <= radius)
            low.push_back(lasher->GetPosition());
    }

    return low;
}

Position GetFreyaLasherCampSpot(PlayerbotAI* botAI, FreyaWaveState const& state)
{
    Player* bot = botAI->GetBot();

    // The anchor bot is what defines where the raid already is: every bot picks the same one, so every
    // bot derives the same spot without shared state.
    Player* anchor = GetFreyaRangedCampAnchor(botAI);

    // Only the lashers that are about to detonate. A Detonating Lasher runs 8.0 yd/s (speed_run
    // 1.14286) against a player's 7.0, so clearance owed to the whole pack is a retreat that cannot
    // end: over one wave the back line walked 256 yd to net 19, still lost ground - median distance to
    // the nearest lasher fell 20.0 to 14.3 - spent 62% of the wave moving, and did half the damage of
    // the wave it cleared standing still. Detonate only lands when one dies, so a healthy lasher is
    // not a blast to walk away from.
    std::vector<Unit*> low;
    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumZ = 0.0f;
    for (Unit* lasher : state.detonatingLashers)
    {
        if (!lasher || !lasher->IsAlive() || lasher->GetHealthPct() >= ULDUAR_FREYA_LASHER_BAIL_PCT)
            continue;

        low.push_back(lasher);
        sumX += lasher->GetPositionX();
        sumY += lasher->GetPositionY();
        sumZ += lasher->GetPositionZ();
    }

    // Nothing is about to blow, so there is nowhere the camp has to be - but it still has to be one
    // place, or GetFreyaLasherPackFocus has no ball behind it and class AoE never fires. Standing on
    // the anchor is that place, and the trigger's tolerance is what keeps it from churning.
    if (low.empty())
        return anchor ? anchor->GetPosition() : Position();

    Position centre;
    centre.Relocate(sumX / low.size(), sumY / low.size(), sumZ / low.size());

    // Bearing from the pile toward where the raid already is, so the walk to the camp never crosses
    // the pack.
    float bearing = anchor && anchor->GetExactDist2d(&centre) > CONTACT_DISTANCE
                        ? centre.GetAngle(anchor)
                        : centre.GetAngle(bot);

    // The first bearing is the one that keeps the raid where it stands; the sweep only runs when a
    // tree trunk or the room edge is in the way.
    for (float delta = 0.0f; delta <= static_cast<float>(M_PI); delta += static_cast<float>(M_PI) / 8.0f)
    {
        for (float sign : {1.0f, -1.0f})
        {
            float const angle = bearing + sign * delta;

            // Walked outward rather than fixed, because the clearance is owed to the nearest of them
            // and they are wider than their centre: a wave spreads over a 17 yd radius, so the first
            // candidate off the middle usually has one standing on it.
            for (float radius = ULDUAR_FREYA_LASHER_CAMP_STANDOFF; radius <= ULDUAR_FREYA_LASHER_CAMP_MAX_STANDOFF;
                 radius += ULDUAR_FREYA_LASHER_CAMP_STEP)
            {
                float x = centre.GetPositionX() + std::cos(angle) * radius;
                float y = centre.GetPositionY() + std::sin(angle) * radius;
                float z = centre.GetPositionZ();

                if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, centre.GetPositionX(), centre.GetPositionY(),
                                                                    centre.GetPositionZ(), x, y, z))
                    continue;

                // Collision can pull the spot back toward the pile, which would park the camp inside the
                // blasts it exists to stay out of. Counted over the low set rather than through
                // CountFreyaLashersNear, which the nova, the trap and the pack focus need to keep
                // reading every living lasher.
                Position const candidate(x, y, z, 0.0f);
                bool clear = true;
                for (Unit* lasher : low)
                    clear = clear && candidate.GetExactDist2d(lasher->GetPosition()) >
                                         ULDUAR_FREYA_LASHER_CAMP_STANDOFF;

                if (!clear)
                    continue;

                return candidate;
            }
        }
    }

    // Trees and the room edge can box in every bearing. A live bot is always on the mesh, which is the
    // whole reason the anchor is a bot and not a fixed point, so fall back to standing on it.
    return anchor ? anchor->GetPosition() : Position();
}

Unit* GetFreyaLasherPackFocus(FreyaWaveState const& state)
{
    Unit* best = nullptr;
    uint32 bestCount = 0;

    for (Unit* lasher : state.detonatingLashers)
    {
        if (!lasher || !lasher->IsAlive())
            continue;

        uint32 const count = CountFreyaLashersNear(lasher->GetPosition(), state, ULDUAR_FREYA_LASHER_PACK_RADIUS);

        // GUID breaks the tie so a wave of evenly spread lashers does not resolve differently per bot,
        // which would split the raid's AoE across two piles and fire neither.
        if (!best || count > bestCount || (count == bestCount && lasher->GetGUID() < best->GetGUID()))
        {
            best = lasher;
            bestCount = count;
        }
    }

    return best;
}

bool IsFreyaLasherPackFinishing(FreyaWaveState const& state, Position const& centre)
{
    uint32 count = 0;
    for (Unit* lasher : state.detonatingLashers)
    {
        if (!lasher || !lasher->IsAlive())
            continue;

        if (centre.GetExactDist2d(lasher->GetPosition()) > ULDUAR_FREYA_LASHER_PACK_RADIUS)
            continue;

        // One healthy lasher in the pile means the raid is still in the AoE phase: novaing and walking
        // out now would leave it alive behind the snare with nobody near enough to finish it.
        if (lasher->GetHealthPct() > ULDUAR_FREYA_LASHER_FINISH_PCT)
            return false;

        ++count;
    }

    return count >= ULDUAR_FREYA_LASHER_PACK_MIN_COUNT;
}

Unit* GetFreyaFinishingPackNear(PlayerbotAI* botAI, FreyaWaveState const& state, float radius)
{
    Unit* focus = GetFreyaLasherPackFocus(state);
    if (!focus || botAI->GetBot()->GetExactDist2d(focus) > radius)
        return nullptr;

    return IsFreyaLasherPackFinishing(state, focus->GetPosition()) ? focus : nullptr;
}

bool IsFreyaGroundTremorCasting(Unit* boss)
{
    if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
        return false;

    Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!spell)
        return false;

    return spell->m_spellInfo->Id == SPELL_FREYA_GROUND_TREMOR_10 ||
           spell->m_spellInfo->Id == SPELL_FREYA_GROUND_TREMOR_25;
}

Unit* GetFreyaSunbeamTarget(Unit* boss)
{
    if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
        return nullptr;

    Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!spell)
        return nullptr;

    if (spell->m_spellInfo->Id != SPELL_FREYA_SUNBEAM_10 && spell->m_spellInfo->Id != SPELL_FREYA_SUNBEAM_25)
        return nullptr;

    return spell->m_targets.GetUnitTarget();
}

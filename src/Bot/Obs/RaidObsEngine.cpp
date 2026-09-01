/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidObsSession.h"

#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Timer.h"

#include <cmath>
#include <cstdio>
#include <utility>

namespace RaidObs
{
namespace
{
// Who the engine is currently ticking, and which action is executing inside that tick. Scalar latches
// carry no guid of their own and resolve their trace through these.
thread_local char const* t_currentAction = nullptr;
thread_local Player* t_currentBot = nullptr;
}  // namespace

// --- verdict ticks ------------------------------------------------------------

// Relevance is deliberately left out: it drifts by thousandths between passes, and comparing it would
// make every pass look new - which is the bug a per-verdict comparison had.
bool SameTick(std::vector<TickEntry> const& left, std::vector<TickEntry> const& right)
{
    if (left.size() != right.size())
        return false;

    for (std::size_t i = 0; i < left.size(); ++i)
        if (left[i].veto != right[i].veto || left[i].action != right[i].action ||
            left[i].verdict != right[i].verdict)
            return false;

    return true;
}

void ObsSession::EmitTickEntry(uint64 key, TickEntry const& entry)
{
    std::string fields = "\"g\":" + std::to_string(key);
    if (entry.veto)
    {
        fields += ",\"m\":" + Quoted(entry.verdict);
        fields += ",\"a\":" + Quoted(entry.action);
        Emit(entry.ms, "veto", fields);
        return;
    }

    fields += ",\"a\":" + Quoted(entry.action);
    fields += ",\"rel\":" + Num(entry.relevance);
    fields += ",\"vd\":" + Quoted(entry.verdict);
    Emit(entry.ms, "act", fields);
}

// Measured on the first v4 Hodir trace: 40% of the file was act records repeating the previous pass'
// answer for the same action. A pass repeating verbatim is rare; a verdict repeating is the norm.
bool ShouldEmit(BotTrace& trace, TickEntry const& entry)
{
    ActionLatch& latch = trace.emitted[(entry.veto ? "v" : "a") + entry.action];
    if (latch.seen && latch.verdict == entry.verdict &&
        getMSTimeDiff(latch.ms, entry.ms) < OBS_ACTION_REPEAT_MS)
        return false;

    latch.seen = true;
    latch.verdict = entry.verdict;
    latch.ms = entry.ms;
    return true;
}

// Closes the pass being buffered. Only a pass whose verdict set differs from the one before is written
// out; the rest become a repeat count on the record already held, which is what a death record replays.
void ObsSession::FlushTick(uint64 key, BotTrace& trace)
{
    if (trace.tick.empty())
        return;

    uint32 const firstMs = trace.tick.front().ms;

    // A pass that decided exactly what the last one decided is not news - but a bot that has been
    // losing the same tick for minutes still has to leave a trail, or its timeline reads as a gap.
    if (!trace.ticks.empty() && SameTick(trace.ticks.back().entries, trace.tick) &&
        getMSTimeDiff(trace.ticks.back().firstMs, firstMs) < OBS_ACTION_REPEAT_MS)
    {
        ++trace.ticks.back().repeats;
        trace.ticks.back().lastMs = firstMs;
        trace.tick.clear();
        return;
    }

    for (TickEntry const& entry : trace.tick)
        if (ShouldEmit(trace, entry))
            EmitTickEntry(key, entry);

    trace.ticks.push_back({firstMs, firstMs, 1, std::move(trace.tick)});
    trace.tick.clear();

    while (!trace.ticks.empty() && getMSTimeDiff(trace.ticks.front().lastMs, firstMs) > g_cfg.deathVerdictMs)
        trace.ticks.pop_front();
}

// --- engine probes ------------------------------------------------------------

void BeginTick(Player* bot)
{
    ProbeTarget probe(bot);
    if (!probe)
        return;

    probe.Session().FlushTick(probe.Key(), probe.Trace());
}

// Buffered rather than emitted. What a pass decided is only news as a whole ordered set - the engine
// walks several nodes and reports a verdict for each, in the same order every tick - and the set is
// not complete until BeginTick closes it.
void NoteAction(Player* bot, char const* action, float relevance, char const* verdict)
{
    if (!action || !verdict)
        return;

    ProbeTarget probe(bot);
    if (!probe)
        return;

    BotTrace& trace = probe.Trace();
    if (trace.tick.size() >= OBS_MAX_TICK_ENTRIES)
        return;

    trace.tick.push_back({getMSTime(), false, action, relevance, verdict});
}

void NoteVeto(Player* bot, char const* multiplier, char const* action)
{
    if (!multiplier || !action)
        return;

    ProbeTarget probe(bot);
    if (!probe)
        return;

    BotTrace& trace = probe.Trace();
    if (trace.tick.size() >= OBS_MAX_TICK_ENTRIES)
        return;

    trace.tick.push_back({getMSTime(), true, action, 0.0f, multiplier});
}

void NoteMove(Player* bot, MoveKind kind, float x, float y, float z, ObjectGuid target, MoveOutcome outcome,
              MovePriority priority, MovePriority holder, uint32 holdMs)
{
    ProbeTarget probe(bot);
    if (!probe)
        return;

    ObsSession& s = probe.Session();
    uint32 const now = getMSTime();
    BotTrace& trace = probe.Trace();
    char const* by = t_currentAction ? t_currentAction : "";
    bool const issued = outcome == MoveOutcome::Issued;
    // Follow and Chase steer at a unit that keeps moving, so their coordinates change every tick and
    // only a changed target is news. A point destination re-stamped every tick is the opposite - that
    // repetition is the bug the trace exists to show.
    bool const tracking = kind == MoveKind::Follow || kind == MoveKind::Chase;
    uint64 const targetKey = GuidKey(target);

    if (!issued)
    {
        // A destination that did not reach the MotionMaster is a candidate, not a command - MoveNear
        // sweeps eight angles a tick and MoveToLOS retries. Latching one would make the death record
        // measure `arrived` against a position no MotionMaster ever saw, so these are throttled and
        // never touch the latch.
        if (trace.hasReject && trace.lastRejectBy == by && trace.lastRejectKind == kind &&
            getMSTimeDiff(trace.lastRejectMs, now) < OBS_MOVE_REJECT_THROTTLE_MS)
            return;

        trace.hasReject = true;
        trace.lastRejectMs = now;
        trace.lastRejectBy = by;
        trace.lastRejectKind = kind;
    }
    else if (tracking)
    {
        if (trace.hasTrack && trace.lastTrackBy == by && trace.lastTrackKind == kind &&
            trace.lastTrackTarget == targetKey)
            return;

        trace.hasTrack = true;
        trace.lastTrackBy = by;
        trace.lastTrackKind = kind;
        trace.lastTrackTarget = targetKey;
    }
    else
    {
        if (trace.hasMove && trace.lastMoveBy == by && std::fabs(trace.lastMoveX - x) < 0.5f &&
            std::fabs(trace.lastMoveY - y) < 0.5f && std::fabs(trace.lastMoveZ - z) < 0.5f)
            return;

        trace.hasMove = true;
        trace.lastMoveX = x;
        trace.lastMoveY = y;
        trace.lastMoveZ = z;
        trace.lastMoveBy = by;
    }

    std::string fields = "\"g\":" + std::to_string(GuidKey(bot->GetGUID()));
    fields += ",\"k\":\"" + std::string(MoveKindName(kind)) + "\"";
    fields += ",\"x\":" + Num(x);
    fields += ",\"y\":" + Num(y);
    fields += ",\"z\":" + Num(z);
    fields += ",\"tgt\":" + std::to_string(targetKey);
    fields += ",\"ok\":" + std::string(issued ? "1" : "0");
    fields += ",\"r\":\"" + std::string(MoveReason(outcome)) + "\"";
    fields += ",\"by\":" + Quoted(by);
    fields += ",\"pr\":\"" + std::string(MovePriorityName(priority)) + "\"";

    // Only on a refusal the gate actually made. Everywhere else there is no walk in flight to name.
    if (outcome == MoveOutcome::Waiting)
    {
        fields += ",\"hpr\":\"" + std::string(MovePriorityName(holder)) + "\"";
        fields += ",\"hms\":" + std::to_string(holdMs);
    }

    s.Emit(now, "move", fields);
}

ActionScope::ActionScope(std::string action) : _action(std::move(action)), _previous(t_currentAction)
{
    t_currentAction = _action.c_str();
}

ActionScope::~ActionScope() { t_currentAction = _previous; }

BotContext::BotContext(Player* bot) : _previous(t_currentBot) { t_currentBot = bot; }
BotContext::~BotContext() { t_currentBot = _previous; }

void Note(Player* bot, char const* kind, std::string const& text)
{
    if (!Active() || !kind)
        return;

    // A latch with no bot of its own belongs to whichever bot's tick flipped it, which is also the
    // most useful attribution: it names who made the call.
    if (!bot)
        bot = t_currentBot;

    ObsSession* session = bot ? SessionFor(bot) : nullptr;
    if (!session)
        return;

    std::string fields = "\"g\":" + std::to_string(GuidKey(bot->GetGUID()));
    fields += ",\"k\":\"" + std::string(kind) + "\"";
    fields += ",\"txt\":" + Quoted(text);

    session->Emit(getMSTime(), "note", fields);
}

void NoteAssignment(ObjectGuid guid, char const* kind, std::string const& value)
{
    if (!Active() || !kind)
        return;

    Player* player = ObjectAccessor::FindPlayer(guid);
    if (!player)
        return;

    ObsSession* session = SessionFor(player);
    if (!session)
        return;

    std::string fields = "\"g\":" + std::to_string(GuidKey(guid));
    fields += ",\"k\":\"" + std::string(kind) + "\"";
    fields += ",\"txt\":" + Quoted(value);

    session->Emit(getMSTime(), "note", fields);
}

void NoteDerived(Player* bot, char const* key, std::string const& value)
{
    if (!key)
        return;

    if (!bot)
        bot = t_currentBot;

    ProbeTarget probe(bot);
    if (!probe)
        return;

    std::string& held = probe.Trace().derived[key];
    if (held == value)
        return;

    held = value;

    std::string fields = "\"g\":" + std::to_string(probe.Key());
    fields += ",\"k\":\"" + std::string(key) + "\"";
    fields += ",\"txt\":" + Quoted(value);

    probe.Session().Emit(getMSTime(), "note", fields);
}

void NoteHazard(Map* map, uint32 spellId, Position const& origin, char const* shape, std::string const& params,
                uint32 ttlMs)
{
    if (!Active() || !map || !shape)
        return;

    ObsSession* session = FindSession(map->GetInstanceId());
    if (!session)
        return;

    // A hazard with no world object is the one spell nothing else in the trace has to mention, so this
    // is the only chance to name it.
    session->EnsureSpell(spellId);

    std::string fields = "\"sp\":" + std::to_string(spellId);
    fields += ",\"shape\":\"" + std::string(shape) + "\"";
    fields += ",\"x\":" + Num(origin.GetPositionX());
    fields += ",\"y\":" + Num(origin.GetPositionY());
    fields += ",\"z\":" + Num(origin.GetPositionZ());
    fields += ",\"ttl\":" + std::to_string(ttlMs);
    if (!params.empty())
        fields += "," + params;

    session->Emit(getMSTime(), "haz", fields);
}

void NoteHazardCircle(Map* map, uint32 spellId, Position const& pos, float radius, uint32 ttlMs)
{
    NoteHazard(map, spellId, pos, "circle", "\"rad\":" + Num(radius), ttlMs);
}

std::string DescribeAssignment(bool value) { return value ? "1" : "0"; }
std::string DescribeAssignment(ObjectGuid const& value) { return std::to_string(GuidKey(value)); }

std::string DescribeAssignment(Position const& value)
{
    return Num(value.GetPositionX()) + "," + Num(value.GetPositionY()) + "," + Num(value.GetPositionZ());
}

std::string DescribeDerived(Position const& value)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%.0f,%.0f,%.0f", value.GetPositionX(), value.GetPositionY(),
             value.GetPositionZ());
    return buf;
}
}  // namespace RaidObs

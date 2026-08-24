/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDOBS_H
#define PLAYERBOTS_RAIDOBS_H

#include "ObjectGuid.h"
#include "Position.h"

#include <atomic>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

class Aura;
class Map;
class Player;
class SpellInfo;
class Unit;

// Per-pull NDJSON traces of what every raid member saw, decided and did, so a wipe can be diagnosed
// without video. Records position, action verdicts, movement commands, damage, heals, auras, hazards
// and deaths. Schema and probe guide: docs/systems/observability.md
//
// Threading: every probe must be called from the map thread that owns the unit or map it names. All
// of the hooks that feed this run there; anything driven from the world thread would not.
namespace RaidObs
{
// Bumped whenever a record's field layout changes, so the analyzer can still read older traces.
constexpr uint32 SCHEMA_VERSION = 6;

// True only while at least one trace is open. Probes on shared hot paths test this before doing
// anything else, so with nothing recording the framework costs one predictable branch.
//
// Written by whichever map thread opens or closes a trace and read by every other one, so the store is
// a release and the load an acquire. Without that pair a thread can keep observing the stale `false`
// after a raid on its own map has opened a trace, and the pull records nothing but snapshots.
extern std::atomic<bool> g_active;

inline bool Active() { return g_active.load(std::memory_order_acquire); }

void LoadConfig();
void Shutdown();

// --- Session lifecycle, driven from RaidObsScripts ---

void OnMapUpdate(Map* map, uint32 diff);
void OnMapDestroyed(Map* map);
// An instance script *attempted* a boss state change. The core has not decided yet at that point and
// rejects some of them, so this only queues the boss id; the state that actually stuck is read on the
// next map update. That delay is also what lets the engage hook name the trace first.
void OnBossState(uint32 bossId, Map* map);
void OnCreatureEngage(Unit* creature, Unit* victim);

// Opens a trace for a pull no instance script reports: gauntlets, trash, a mid-phase re-engage.
// `source` names the file and may be null, in which case the map's own name is used.
void MarkPull(Map* map, Unit* source);

// --- Combat events ---

void NoteDamage(Unit* attacker, Unit* victim, SpellInfo const* spell, uint32 amount, int32 overkill,
                uint32 schoolMask, uint32 absorb, uint32 resist);
void NoteHeal(Unit* healer, Unit* target, SpellInfo const* spell, uint32 amount, uint32 overheal);
// Which shield ate the hit, so "the bubble was up but too small" reads differently from "no bubble".
void NoteAbsorb(Unit* victim, Unit* absorbCaster, SpellInfo const* absorbSpell, uint32 amount);
void NoteAura(Unit* target, Aura* aura, bool removed);
void NoteCast(Unit* caster, SpellInfo const* spell, Unit* target, uint32 castTimeMs);
// Every other damage hook here is a combat-log hook, so damage that sends no log packet - a fall, a
// script kill - is invisible to them and a bot dying to one leaves a death record with an empty
// rewind. Fed instead from Unit::DealDamage, which all of them pass through, and only for the blow
// that takes the bot under: emitting the rest from there would double every hit the log hooks see.
void NoteKillingBlow(Unit* attacker, Unit* victim, uint32 amount);
void NoteDeath(Unit* unit, Unit* killer);

// --- Engine probes ---

// Closes the engine pass being buffered and opens the next. A pass emits the same ordered set of
// verdicts every tick while nothing changes, so the set - not the individual verdict - is what is news,
// and it is not complete until the pass ends.
void BeginTick(Player* bot);

void NoteAction(Player* bot, char const* action, float relevance, char const* verdict);
void NoteVeto(Player* bot, char const* multiplier, char const* action);

// Which MotionMaster generator a command drives. Follow and Chase track a unit that keeps moving, so
// they have no destination of their own and carry the target instead.
enum class MoveKind : uint8
{
    Point,
    Jump,
    Follow,
    Chase,
};

// Why a command did or did not reach the MotionMaster. Duplicate and Waiting are the ordinary state
// while a bot walks to a destination its action re-offers every tick - reading either as a refusal
// blames the movement layer for rejecting what it accepted a moment earlier.
enum class MoveOutcome : uint8
{
    Issued,
    Duplicate,
    Waiting,
    NotAllowed,
    NoPath,
    // Nothing wrong with the destination - the bot is already standing on it. Kept apart from Issued
    // because MoveTo has always returned false here and its callers branch on that.
    AlreadyThere,
};

// Mirrors MovementPriority in Ai/Base/Value/LastMovementValue.h. Mirrored rather than included
// because Bot/Obs must not depend on Ai; MovementActions.cpp static_asserts that the two agree.
//
// None is not a tier. Follow and Chase steer the MotionMaster without passing the priority gate at
// all, and recording that is the point: a bot can be held in place by a walk it never had to outrank.
enum class MovePriority : uint8
{
    None,
    Idle,
    Wander,
    Normal,
    Combat,
    Forced,
};

// `holder` and `holdMs` describe the walk already in flight, and only mean anything when the outcome
// is Waiting: the gate yields to a strictly higher priority, so without them a refusal says a command
// lost without saying to what or for how long.
void NoteMove(Player* bot, MoveKind kind, float x, float y, float z, ObjectGuid target, MoveOutcome outcome,
              MovePriority priority = MovePriority::None, MovePriority holder = MovePriority::None,
              uint32 holdMs = 0);

// Publishes the action the engine is currently executing. A movement command carries no hint of who
// issued it, and "two actions steering the same MotionMaster" is only visible once it does.
//
// Restores the previous action rather than clearing it: a nested DoSpecificAction executes a second
// action inside the first, and clearing would strip the outer action's attribution for the rest of
// its own tick. Holds its own copy of the name because Action::getName() returns by value.
class ActionScope
{
public:
    explicit ActionScope(std::string action);
    ~ActionScope();

    ActionScope(ActionScope const&) = delete;
    ActionScope& operator=(ActionScope const&) = delete;

private:
    std::string _action;
    char const* _previous;
};

// Publishes the bot for a whole engine pass, restoring the previous one on the way out - a nested
// DoSpecificAction ticks a second bot inside the first. Scalar latches hold no guid of their own and
// resolve their trace through this, so it has to cover trigger and value evaluation too, not just
// action execution.
class BotContext
{
public:
    explicit BotContext(Player* bot);
    ~BotContext();

    BotContext(BotContext const&) = delete;
    BotContext& operator=(BotContext const&) = delete;

private:
    Player* _previous;
};

// --- Encounter helper probes ---

void Note(Player* bot, char const* kind, std::string const& text);
void NoteAssignment(ObjectGuid guid, char const* kind, std::string const& value);

// For encounter state that is derived fresh on every call rather than stored, which the traced
// containers below cannot see. Emits only when the derived answer changes, so the probe belongs
// inside the helper that derives it - trigger and action both route through there, and two probes at
// two call sites can disagree about what was decided.
void NoteDerived(Player* bot, char const* key, std::string const& value);

// A hazard with no world object behind it, so nothing can sweep for it: a cone, a rolling wave, a
// rotating sweep. The helper that derives the geometry is the only thing that knows it exists.
//
// Ulduar's invisible hazards are not all circles, so the general form carries the shape's own fields
// in `params` as compact JSON - for a Barrage sweep, "\"lead\":1.2,\"sweep\":3.1,\"rate\":0.5".
void NoteHazard(Map* map, uint32 spellId, Position const& origin, char const* shape, std::string const& params,
                uint32 ttlMs);
void NoteHazardCircle(Map* map, uint32 spellId, Position const& pos, float radius, uint32 ttlMs);

std::string Status();

// ---------------------------------------------------------------------------
// Assignment containers
//
// Encounter state that decides "which bot does what" is stored in these instead of the bare
// std::unordered_map/set, so a changed assignment writes itself into the trace. Instrumentation then
// follows the data rather than every call site that touches it, and a new boss that stores its slots
// here is traced without further work.
//
// The emit path resolves the owning session from the guid, which costs a player lookup - but only on
// an actual change, which is rare next to the reads.
// ---------------------------------------------------------------------------

std::string DescribeAssignment(bool value);
std::string DescribeAssignment(ObjectGuid const& value);
std::string DescribeAssignment(Position const& value);

// For NoteDerived. Rounds to the yard, because a position derived from the bot's own coordinates drifts
// by centimetres every tick - latched at full precision it would emit on every one of them, which is
// the repetition the change-only rule exists to suppress. Stored assignments do not drift and keep
// DescribeAssignment's full precision.
std::string DescribeDerived(Position const& value);

// One template rather than an overload per width: uint8 is unsigned char, which converts to uint32,
// uint64 and bool at the same rank, so overloads would make every uint8 slot map ambiguous. Scoped
// enums are written as their raw number - read them against the enum in the boss's own header.
template <typename T, typename = std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>>>
std::string DescribeAssignment(T value)
{
    if constexpr (std::is_enum_v<T>)
        return std::to_string(static_cast<uint64>(value));
    else
        return std::to_string(value);
}

// Scalar assignment. Emits when the value actually changes, not on every write.
template <typename T>
class ObsValue
{
public:
    explicit ObsValue(char const* kind) : _kind(kind) {}
    ObsValue(char const* kind, T value) : _kind(kind), _value(std::move(value)) {}

    ObsValue& operator=(T value)
    {
        if (!(_value == value))
        {
            _value = std::move(value);
            if (Active())
                Note(nullptr, _kind, DescribeAssignment(_value));
        }

        return *this;
    }

    operator T const&() const { return _value; }
    T const& Get() const { return _value; }

private:
    char const* _kind;
    T _value{};
};

// Guid-keyed assignment map. operator[] hands back a proxy so an assignment through it is seen;
// direct reads and iteration behave like the underlying map.
template <typename V>
class ObsGuidMap
{
public:
    using Container = std::unordered_map<ObjectGuid, V>;

    explicit ObsGuidMap(char const* kind) : _kind(kind) {}

    class Ref
    {
    public:
        Ref(ObsGuidMap& owner, ObjectGuid guid) : _owner(owner), _guid(guid) {}

        Ref& operator=(V value)
        {
            _owner.Set(_guid, std::move(value));
            return *this;
        }

        operator V const&() const { return _owner._values[_guid]; }

    private:
        ObsGuidMap& _owner;
        ObjectGuid _guid;
    };

    Ref operator[](ObjectGuid guid) { return Ref(*this, guid); }

    void Set(ObjectGuid guid, V value)
    {
        auto it = _values.find(guid);
        if (it != _values.end() && it->second == value)
            return;

        _values[guid] = std::move(value);
        if (Active())
            NoteAssignment(guid, _kind, DescribeAssignment(_values[guid]));
    }

    typename Container::const_iterator find(ObjectGuid guid) const { return _values.find(guid); }
    typename Container::const_iterator begin() const { return _values.begin(); }
    typename Container::const_iterator end() const { return _values.end(); }

    // Non-const iteration and iterator-erase exist for the prune loops that drop members who left the
    // instance. Dropping a stale entry is not an assignment, so those deliberately emit nothing.
    // Erase by guid is not one of those - see below.
    typename Container::iterator begin() { return _values.begin(); }
    typename Container::iterator end() { return _values.end(); }
    typename Container::iterator erase(typename Container::const_iterator it) { return _values.erase(it); }

    // Inserts only when the key is absent, which is exactly when it is news. Returns what std's
    // try_emplace returns, because call sites read the iterator back out of it.
    std::pair<typename Container::iterator, bool> try_emplace(ObjectGuid guid, V value)
    {
        auto it = _values.find(guid);
        if (it != _values.end())
            return {it, false};

        it = _values.emplace(guid, std::move(value)).first;
        if (Active())
            NoteAssignment(guid, _kind, DescribeAssignment(it->second));

        return {it, true};
    }
    std::size_t count(ObjectGuid guid) const { return _values.count(guid); }
    std::size_t size() const { return _values.size(); }
    bool empty() const { return _values.empty(); }

    // Erasing a named guid is a real unassignment - a wipe reset, or a boss that is no longer present -
    // so unlike the prune loops' iterator-erase it is news.
    std::size_t erase(ObjectGuid guid)
    {
        std::size_t const removed = _values.erase(guid);
        if (removed && Active())
            NoteAssignment(guid, _kind, "0");

        return removed;
    }

    void clear() { _values.clear(); }

    Container& Raw() { return _values; }
    Container const& Raw() const { return _values; }

private:
    friend class Ref;

    char const* _kind;
    Container _values;
};

// Guid set for membership latches - arrived, bailing, stripped. Emits on the transition only.
class ObsGuidSet
{
public:
    explicit ObsGuidSet(char const* kind) : _kind(kind) {}

    void insert(ObjectGuid guid)
    {
        if (!_values.insert(guid).second)
            return;

        if (Active())
            NoteAssignment(guid, _kind, "1");
    }

    std::size_t erase(ObjectGuid guid)
    {
        std::size_t const removed = _values.erase(guid);
        if (!removed)
            return 0;

        if (Active())
            NoteAssignment(guid, _kind, "0");

        return removed;
    }

    std::size_t count(ObjectGuid guid) const { return _values.count(guid); }
    std::size_t size() const { return _values.size(); }
    bool empty() const { return _values.empty(); }
    void clear() { _values.clear(); }

    std::unordered_set<ObjectGuid>::const_iterator begin() const { return _values.begin(); }
    std::unordered_set<ObjectGuid>::const_iterator end() const { return _values.end(); }

private:
    char const* _kind;
    std::unordered_set<ObjectGuid> _values;
};
}  // namespace RaidObs

#endif

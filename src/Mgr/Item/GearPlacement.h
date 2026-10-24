/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_GEARPLACEMENT_H
#define PLAYERBOTS_GEARPLACEMENT_H

#include <array>
#include <cstdint>
#include <vector>

// Where a new item goes, decided over scores the caller already has. std only, so tools/nativetest
// can build it.
namespace GearPlacement
{
// One slot whose occupant changes.
struct Change
{
    float newScore = 0.0f;
    // 0 for an empty slot
    float oldScore = 0.0f;
    // the bar oldScore is multiplied by
    float threshold = 1.0f;
};

struct Option
{
    bool legal = true;
    // > 0 the sim ranks the new item higher, < 0 lower; 0 leaves it to the scores
    int8_t rankOrder = 0;
    // the item it displaces is broken and the new one isn't: wanted whatever the scores say
    bool replacesBroken = false;
    // between wanted options, higher first: an empty slot, or the worn item the sim ranks lower
    int8_t preference = 0;
    uint8_t count = 0;
    std::array<Change, 2> changes{};

    void Add(Change change) { changes[count++] = change; }

    float NewSum() const
    {
        float sum = 0.0f;
        for (uint8_t i = 0; i < count; ++i)
            sum += changes[i].newScore;
        return sum;
    }

    float OldSum() const
    {
        float sum = 0.0f;
        for (uint8_t i = 0; i < count; ++i)
            sum += changes[i].oldScore;
        return sum;
    }

    // the old side with every slot's threshold applied
    float Bar() const
    {
        float sum = 0.0f;
        for (uint8_t i = 0; i < count; ++i)
            sum += changes[i].oldScore * changes[i].threshold;
        return sum;
    }

    bool Wanted() const
    {
        if (!legal)
            return false;
        if (replacesBroken)
            return true;
        if (rankOrder)
            return rankOrder > 0;
        return NewSum() > Bar();
    }
};

// Index of the option to take, -1 to keep what's worn. force takes the best legal option even
// without a gain.
inline int Choose(std::vector<Option> const& options, bool force)
{
    int best = -1;
    for (int i = 0; i < int(options.size()); ++i)
    {
        Option const& option = options[i];
        if (!option.legal || (!force && !option.Wanted()))
            continue;

        if (best < 0)
        {
            best = i;
            continue;
        }

        Option const& current = options[best];
        if (option.Wanted() != current.Wanted())
        {
            if (option.Wanted())
                best = i;
            continue;
        }

        if (option.preference != current.preference)
        {
            if (option.preference > current.preference)
                best = i;
            continue;
        }

        if (option.NewSum() - option.OldSum() > current.NewSum() - current.OldSum())
            best = i;
    }

    return best;
}

// Weapon hands.

enum class Kind : uint8_t
{
    None,
    OneHand,
    MainHandOnly,
    OffHandOnly,
    TwoHand,
    // shield or holdable
    OffHandItem
};

struct Hand
{
    Kind kind = Kind::None;
    // a 2H axe, mace or sword
    bool titanGripType = false;
    // a one-hand polearm or a fishing pole
    bool mainHandOnly = false;
};

struct Bot
{
    bool canDualWield = false;
    bool canTitanGrip = false;
    // a tank wearing a shield keeps it
    bool keepsShield = false;
};

enum class Layout : uint8_t
{
    NewToMain,
    NewToMainOldToOff,
    NewToOff,
    NewToOffOldToMain,
    Count
};

// a 2H that leaves no room for an off hand
inline bool LoneTwoHand(Hand const& hand, Bot const& bot)
{
    return hand.kind == Kind::TwoHand && !(bot.canTitanGrip && hand.titanGripType);
}

inline bool FitsMainHand(Hand const& hand, Bot const& bot)
{
    switch (hand.kind)
    {
        case Kind::OneHand:
        case Kind::MainHandOnly:
            return true;
        case Kind::TwoHand:
            return !bot.canTitanGrip || hand.titanGripType;
        default:
            return false;
    }
}

inline bool FitsOffHand(Hand const& hand, Bot const& bot)
{
    if (!bot.canDualWield)
        return false;

    switch (hand.kind)
    {
        case Kind::OneHand:
            return !hand.mainHandOnly;
        case Kind::OffHandOnly:
            return true;
        case Kind::TwoHand:
            return bot.canTitanGrip && hand.titanGripType;
        default:
            return false;
    }
}

// Whether the layout can be worn at all. A lone 2H only ever goes to the main hand.
inline bool LayoutLegal(Layout layout, Hand const& item, Hand const& main, Hand const& off, Bot const& bot)
{
    bool const hasMain = main.kind != Kind::None;
    bool const hasOff = off.kind != Kind::None;

    switch (layout)
    {
        case Layout::NewToMain:
            if (!FitsMainHand(item, bot))
                return false;
            // pushes the shield out
            return !(LoneTwoHand(item, bot) && hasOff && bot.keepsShield);
        case Layout::NewToMainOldToOff:
            return FitsMainHand(item, bot) && !LoneTwoHand(item, bot) && hasMain && FitsOffHand(main, bot) &&
                   !bot.keepsShield;
        case Layout::NewToOff:
            if (!FitsOffHand(item, bot) || bot.keepsShield)
                return false;
            // a weapon that fits the empty main hand goes there
            return hasMain ? !LoneTwoHand(main, bot) : !FitsMainHand(item, bot);
        case Layout::NewToOffOldToMain:
            return FitsOffHand(item, bot) && hasOff && FitsMainHand(off, bot) && !LoneTwoHand(off, bot);
        default:
            return false;
    }
}
}  // namespace GearPlacement

#endif

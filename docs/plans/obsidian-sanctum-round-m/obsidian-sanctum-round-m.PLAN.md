# Obsidian Sanctum — round M

## Context

Round M of in-game defects on the Sartharion 3-drake strategy
(`modules/mod-playerbots/src/Ai/Raid/OS/`). The user builds and raids, then reports. Rounds A–K are
committed; round L (Twilight Realm exit) is implemented and uncommitted in the working tree. The
docs rewrite covering E–L is drafted and still unapplied.

Six reports, in the order they were raised.

### 1 — the off-tank dodges Shadow Fissure on X; it has to be Y

A drake walks to melee range of whoever holds it and faces them, so the off-tank's position *is* the
drake's facing, and the drake carries a 15yd frontal Shadow Breath. Every drake spot was chosen to
aim that cone at empty platform, and a 10yd X sidestep re-aims it.

Shadron is the case that kills people. It lands at `(3230.50, 533.00)` and is held from
`(3228.94, 534.66)`, so it faces west. The raid line is `RaidLineX` 3248.58 on an 8yd tolerance
holding Y 535.5 — its **west end sits at (3240.58, 535.5), 10.2yd east of the landing on the same Y**,
well inside 15yd. Step the off-tank 10yd east and the drake turns to face him: the cone now points
straight down the line. Vesperon fails the same way against the raid's left-wave hold at Y 551. A Y
step turns the drake across the line instead of down it.

The main tank already dodges on Y for the mirror-image reason, and that branch is untouched.

### 2 — what actually kills the main tank

Each drake casts its Power aura on itself in `JustEngagedWith` (`boss_sartharion.cpp:385-397`). All
three are `SPELL_EFFECT_APPLY_AREA_AURA_ENEMY`, radius index 28 = **50000yd**, duration index 21 =
**permanent**, so they sit on the whole raid from the pull until that drake dies:

| spell | effect |
|---|---|
| Power of Shadron 58105 | `MOD_DAMAGE_PERCENT_TAKEN` +100%, school mask 4 (**fire**) |
| Power of Vesperon 61251 | `MOD_INCREASE_HEALTH_PERCENT` **−25% max health** |
| Power of Tenebron 61248 | `MOD_DAMAGE_PERCENT_TAKEN` +100%, school mask 32 (shadow) |

So for the whole stretch before Shadron dies the main tank eats doubled Flame Breath and Lava Strike
on a 75% health pool. The class nodes spend his cooldowns on whatever health trigger fires first —
`TankWarriorStrategy.cpp:237` hangs Shield Wall off a plain health trigger — so a 5-minute button
goes on the first dip and is gone when the fight is at its worst.

### 3 — Tricks on trash

`OsRedirectThreatTrigger` is live from the pull, and `RedirectTarget` hands back the off-tank whenever
`OFFTANK_PICKUP_ENTRIES` (Lava Blazes, Twilight Whelps) is anywhere in the room. Tricks is a 30s
cooldown redirecting **everything** for 6s and a Lava Blaze dies to one Fan of Knives, so it goes out
on trash and is down when a drake lands. Hunters keep today's behaviour — Misdirection is three shots
rather than a blanket window, and the off-tank pulling a whelp off a healer is worth one.

### 4 — the main tank's left-wave hold is further south than it needs to be

He holds `(3221.3743, 511.08917)` under left waves, which is also his between-waves home; he leaves it
only for right waves. Two things cap how far north it can go: the left wave line at Y 524 against the
8.5yd lethal half-width, and **melee on the boss share this exact lane** (`SafeCorridorY`,
`CorridorGroup::Melee`) at the looser 2.0yd tolerance rather than his 1.0. That puts the ceiling at
513.5, not 514.5.

### 5 — burst opens on the wrong signal

`SartharionBurstWindowOpen` latches at `BURST_WINDOW_TENEBRON_PCT` = 70%. It should latch when Shadron
is on the ground. Shadron is called at 60s, and the Gift of Twilight interlock in
`SartharionBurstWindowMultiplier` already parks Bloodlust while its acolyte lives, so burst lands where
the raid can actually damage the boss.

### 6 — both drakes end up piled at Tenebron's spot

The anchor itself is already right: `NewestLandedDrake` ranks Vesperon > Shadron > Tenebron, drakes
clear `UNIT_FLAG_NOT_SELECTABLE` on `POINT_LANDING` (`boss_sartharion.cpp:741`), so the tick Shadron
touches down `OffTankAnchor` becomes `SHADRON_TANK_SPOT`. The `&TENEBRON_TANK_SPOT` fallback needs
**zero** landed drakes and cannot fire while Shadron is down.

What is missing is the handover. `OffTankChargeFor` returns only the newest drake, so the off-tank
attacks Shadron and **never re-taunts Tenebron**, which follows him only while he is still its top
threat. The walk is 35.5yd — 5.1s, and a tsunami freezes it mid-way for another ~11s — and
`PriorityTarget` ranks Tenebron *above* Shadron, so the whole raid is still hitting the drake he is
walking away from. When it peels it is 35.5yd behind him, outside his 30yd taunt range, and nothing
brings it back: it stays north with the melee killing it and the pile looks like it never moved.

---

## Change

All under `modules/mod-playerbots/src/Ai/Raid/OS/`.

### A. Off-tank dodges on Y — `OSActions.cpp`

In `OsAvoidTwilightFissureAction::Execute`, hoist the wave classification into a local and give the
off-tank his own candidate list after the existing main-tank branch:

```cpp
    else if (IsOffTank(bot))
    {
        // His position is the drake's facing, and the 15yd Shadow Breath points at empty platform only
        // while he stands where the spot puts him. Shadron's is the one that decides it: held from the
        // west at (3228.94, 534.66), a 10yd step east turns the drake to face the raid line, whose west
        // end is 10.2yd away on the same Y. A Y step turns it across the line instead.
        //
        // The X pair is kept as the tail, for a fissure that pins him against a rim. Both Y candidates
        // have to clamp or fall in a lane before it is reached.
        candidates = { {
            { bot->GetPositionX(), fy - step },
            { bot->GetPositionX(), fy + step },
            { fx + step, stepY },
            { fx - step, stepY },
        } };
    }
```

and filter every candidate on the wave, inside the existing loop right after `ClampDestination`:

```cpp
        // Only the off-tank's Y pair can fail this. The X candidates carry stepY, which is wave-clear by
        // construction, and the main tank does not dodge at all while a wave is up.
        if (!WaveClearsY(y, wave))
            continue;
```

The existing `bestClearance < 0.0f` guard already covers "everything was filtered". Ranged and melee
keep the X pair they have today.

### B. Held tank cooldowns

**`OSHelpers.h`** — add `SpellId::PowerOfShadron = 58105` beside the other Sartharion ids, with the
effect decoded and Vesperon's 61251 named as the other half of what kills him, plus:

```cpp
// Where the main tank's cooldowns are worth more than anywhere else in the fight. Power of Shadron
// doubles every point of fire he takes and Power of Vesperon costs him a quarter of his health pool,
// both from the pull, both permanent until their drake dies - so the class nodes, which fire on a bare
// health trigger, spend a 5-minute Shield Wall on the first Flame Breath and have nothing left for the
// stretch that actually kills him. Held until Shadron is nearly down instead, then spent one at a time.
constexpr float MAIN_TANK_COOLDOWN_SHADRON_PCT = 50.0f;
// Not latched, unlike the Shadron gate: one dip this low buys one cooldown, not the rest of the fight.
constexpr float MAIN_TANK_COOLDOWN_PANIC_PCT = 25.0f;

bool MainTankCooldownWindowOpen(Player* bot);
// The weakest cooldown the bot can cast right now, or nullptr - either because none is off cooldown or
// because one is already running. Ordered by cooldown length, which is what "weakest" means here.
char const* NextTankDefensive(PlayerbotAI* botAI, Player* bot);
// True for the action names in that table, so the class nodes can be held off them.
bool IsHeldTankDefensive(std::string const& actionName);
```

**`OSHelpers.cpp`** — the table, in the anonymous namespace. Cast names are the strings the class
strategies already register, so one entry serves both the cast and the multiplier; auras are matched by
id because `PlayerbotAI::HasAura` compares the DBC string exactly and Anti-Magic Shell's cast name has
no hyphen:

```cpp
// Weakest first, which here is shortest cooldown first: the strongest button stays in hand longest.
// Bubbles are deliberately absent - a prot paladin under Divine Shield does no damage and holds
// nothing. So is short rotational mitigation (shield block, bone shield, spell reflection), which keeps
// running on its own class logic; only the real cooldowns are sequenced.
struct TankDefensive
{
    uint8 playerClass;
    char const* castName;
    uint32 auraId;
};

std::array<TankDefensive, 9> const TANK_DEFENSIVES = { {
    { CLASS_WARRIOR,      "last stand",            12975 },  // 180s
    { CLASS_WARRIOR,      "shield wall",             871 },  // 300s
    { CLASS_PALADIN,      "divine protection",       498 },  // 180s
    { CLASS_DRUID,        "barkskin",              22812 },  //  60s
    { CLASS_DRUID,        "frenzied regeneration", 22842 },  // 180s
    { CLASS_DRUID,        "survival instincts",    61336 },  // 180s
    { CLASS_DEATH_KNIGHT, "anti magic shell",      48707 },  //  45s
    { CLASS_DEATH_KNIGHT, "vampiric blood",        55233 },  //  60s
    { CLASS_DEATH_KNIGHT, "icebound fortitude",    48792 },  // 120s
} };
```

`NextTankDefensive` walks the bot's own class rows in order, returns `nullptr` the moment any row's
`auraId` is already on the bot — that is the whole of "one at a time" — and otherwise returns the first
`castName` that `botAI->CanCastSpell(name, bot)` accepts.

`MainTankCooldownWindowOpen` latches on the encounter state, next to `burstWindowOpen`, on a new
`bool tankCooldownWindowOpen` in `EncounterState`:

```cpp
bool MainTankCooldownWindowOpen(Player* bot)
{
    Unit* boss = GetSartharion(bot);
    if (!boss)
        return false;

    EncounterState& state = StateFor(boss);
    if (!state.tankCooldownWindowOpen)
    {
        if (Unit* shadron = LandedShadron(bot))
            state.tankCooldownWindowOpen = shadron->GetHealthPct() <= MAIN_TANK_COOLDOWN_SHADRON_PCT;
        else if (ShadronGone(bot))
            state.tankCooldownWindowOpen = true;
    }

    return state.tankCooldownWindowOpen || bot->GetHealthPct() < MAIN_TANK_COOLDOWN_PANIC_PCT;
}
```

**`OSTriggers.{h,cpp}`** — `OsMainTankCooldownTrigger`, named `"os main tank cooldown"`:

```cpp
bool OsMainTankCooldownTrigger::IsActive()
{
    if (!botAI->IsMainTank(bot) || !OnThePlatform(bot) || !SartharionEncounterActive(bot))
        return false;

    return MainTankCooldownWindowOpen(bot) && NextTankDefensive(botAI, bot);
}
```

**`OSActions.{h,cpp}`** — `OsMainTankCooldownAction`, a bare `Action`:

```cpp
bool OsMainTankCooldownAction::Execute(Event /*event*/)
{
    char const* spell = NextTankDefensive(botAI, bot);
    return spell && botAI->CastSpell(spell, bot);
}
```

**`OSStrategy.cpp`** — `ACTION_RAID + 7`, above the shapeshift so a hold that keeps returning true
cannot starve it, and below every dodge.

**`OSMultipliers.cpp`** — with the OS node casting these directly, the class nodes have to be off them
for the whole encounter or the held ones go out anyway. After the existing Tricks rule:

```cpp
    // The class nodes fire these on a bare health trigger, which in this fight spends a 5-minute Shield
    // Wall on the first Flame Breath. "os main tank cooldown" owns the order instead, and it can only
    // own it if nothing else casts them.
    if (botAI->IsMainTank(bot) && IsHeldTankDefensive(action->getName()))
        return 0.0f;
```

### C. Rogue Tricks — `OSHelpers.{h,cpp}`, `OSTriggers.cpp`, `OSActions.cpp`

```cpp
// Rogues redirect off their own target rather than off the raid-wide RedirectTarget: Tricks moves
// everything for 6s, so it has to land on the tank of the thing the rogue is actually hitting - and
// never on a Lava Blaze, which dies to one Fan of Knives and takes the 30s cooldown with it. Sartharion
// needs no pull-window test of its own; his tank is the main tank for the whole fight.
Player* RedirectTankFor(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || bot->getClass() != CLASS_ROGUE)
        return RedirectTarget(botAI, bot);

    Unit* victim = bot->GetVictim();
    if (!victim)
        return nullptr;

    if (IsDrakeEntry(victim->GetEntry()))
        return GetOffTank(botAI, bot);

    Unit* boss = GetSartharion(bot);
    return boss && victim == boss ? GetGroupMainTank(botAI, bot) : nullptr;
}
```

`OsRedirectThreatTrigger::IsActive` and `OsRedirectThreatAction::GetRedirectTank` both swap
`RedirectTarget` for `RedirectTankFor`. Hunters land on the code path they have today.

### D. Main tank's left-wave hold — `OSHelpers.h`

`TANK_CORRIDOR_LEFT_Y` 511.08917 → **513.0**. Clearance to the line at 524 goes 12.91 → 11.00; worst
case with his 1.0yd tolerance 11.91 → 10.00, and 9.00 for the melee who share the lane at 2.0 — both
still over the 8.5yd kill width, where 513.5 would put melee exactly on it. He comes to 35.3yd from the
raid's home hold (was 36.6) and 46.7 from its left one (was 48.3). The comment block above the constant
carries those four figures and the header's heal-range paragraph carries two of them; all need updating,
along with the note that the tank holds are hand-measured — this one is derived.

Two things it does not break, worth stating in the comment: Sartharion stays parked, since 10.9yd is
well inside his 20.83yd reach, and his facing rotates only 138° → 131°, leaving the raid line 74° off
against a 41°+8° half-cone. And `PlatformMinX` still returns 3220 below Y 520, so `TANK_HOLD_LEFT_X`
3221.37 survives the clamp.

### E. Burst window — `OSHelpers.{h,cpp}`

Two new helpers, shared with B:

```cpp
// Shadron once it is on the ground, or nullptr while it is still circling: drakes carry
// UNIT_FLAG_NOT_SELECTABLE for their whole flight and clear it on POINT_LANDING.
Unit* LandedShadron(Player* bot);
// True once Shadron is down for good - dead, or never called at all. Power of Shadron is on the raid
// from the pull and permanent, so "no drake and no aura" is what separates gone from still in the air.
bool ShadronGone(Player* bot);
```

`ShadronGone` is `EncounterElapsedMs(bot) > PULL_WINDOW_MS && !LandedShadron(bot) &&
!bot->HasAura(SpellId::PowerOfShadron)` — the elapsed guard covers the tick or two before the drake's
own cast lands.

`SartharionBurstWindowOpen` then latches on `LandedShadron(bot) || ShadronGone(bot)`, and
`BURST_WINDOW_TENEBRON_PCT` is deleted along with its comment.

### F. Off-tank re-taunts the drake he walked away from — `OSHelpers.{h,cpp}`, `OSActions.cpp`

```cpp
// A landed drake that is not on the off-tank, nearest first. He attacks only the newest one, so without
// this nothing pulls back the drake he walks away from - and by the time it peels it is 35.5yd behind
// him at the far spot, outside the 30yd taunt this reaches with.
Unit* OffTankTauntTarget(Player* bot);
```

Implemented over `LandedDrakes`, filtered on `drake->GetVictim() != bot` and
`bot->GetExactDist2d(drake) <= LAVA_BLAZE_TAUNT_RANGE`, nearest first. `OsOffTankHoldAction::Execute`
tries it before it moves, and only if the taunt actually goes out does it take the tick:

```cpp
    // Ahead of the walk, not after it: he starts the handover standing on the old drake and ends it
    // 35.5yd away, so the one tick this is in range is the tick he sets off.
    if (Unit* stray = OffTankTauntTarget(bot))
    {
        if (botAI->CanCastSpell("taunt", stray) && botAI->CastSpell("taunt", stray))
            return true;
    }
```

using the class's own taunt name per `bot->getClass()` — `taunt`, `growl`, `dark command`,
`hand of reckoning` — the same four the multiplier already names one by one.

---

## Files

| file | change |
|---|---|
| `OSHelpers.h` | `SpellId::PowerOfShadron`; two cooldown-gate constants; `TANK_CORRIDOR_LEFT_Y` → 513.0 and its comment figures; drop `BURST_WINDOW_TENEBRON_PCT`; declare `MainTankCooldownWindowOpen`, `NextTankDefensive`, `IsHeldTankDefensive`, `RedirectTankFor`, `LandedShadron`, `ShadronGone`, `OffTankTauntTarget` |
| `OSHelpers.cpp` | `TANK_DEFENSIVES` and its accessors; `tankCooldownWindowOpen` in `EncounterState`; `RedirectTankFor`; the two Shadron helpers; `SartharionBurstWindowOpen` relatched; `OffTankTauntTarget` |
| `OSTriggers.{h,cpp}` | `OsMainTankCooldownTrigger`; `OsRedirectThreatTrigger` routes through `RedirectTankFor` |
| `OSActions.{h,cpp}` | off-tank Y candidates and the wave filter in the fissure dodge; `OsMainTankCooldownAction`; `GetRedirectTank` routes through `RedirectTankFor`; the re-taunt in `OsOffTankHoldAction` |
| `OSTriggerContext.h`, `OSActionContext.h` | register the new trigger and action |
| `OSStrategy.cpp` | the new node at `ACTION_RAID + 7` |
| `OSMultipliers.cpp` | hold the class defensive nodes off the main tank |

## Docs

The E–L rewrite is still unapplied, so round M folds into the same pass: the Power auras and what they
cost the tank, the held-cooldown order and its two gates, the off-tank's dodge axis and why Shadron's
spot decides it, the new left-hold figures, burst opening on Shadron's landing, and the drake handover.
Presented as a unified diff with the word delta measured from the files, applied only on approval.

## Verification

**Static**

1. `python apps/codestyle/codestyle-cpp.py` — no new findings under `src/Ai/Raid/OS/`, no line over 110.
2. Name parity: 17 trigger nodes / 17 next-actions / 17 trigger creators against 16 action creators plus
   the documented `rear flank` borrow.
3. Grep `BURST_WINDOW_TENEBRON_PCT` — gone from header, source and docs.
4. Grep `RedirectTarget` — reached only through `RedirectTankFor` inside OS. The VoA helper of the same
   name is a separate namespace and must be untouched.
5. Every `castName` in `TANK_DEFENSIVES` is a string an existing class context registers, so
   `CanCastSpell` resolves it: `last stand`, `shield wall` (`WarriorAiObjectContext.cpp:220,230`),
   `divine protection` (`PaladinAiObjectContext.cpp:296`), `barkskin`, `survival instincts`,
   `frenzied regeneration` (`DruidAiObjectContext.cpp:242,243,266`), `anti magic shell`,
   `vampiric blood`, `icebound fortitude` (`DKAiObjectContext.cpp:177,194,210`).
6. Re-check the four clearance figures in the `TANK_CORRIDOR_LEFT_Y` comment block against 513.0, and
   that no other constant or comment still quotes 511.089, 36.6 or 48.3.

**In-game**

- Off-tank on Shadron: a fissure steps him **north or south** keeping his X, the drake keeps facing west,
  the raid line takes no Shadow Breath. Under a wave he steps only to a Y the wave cannot reach and falls
  back to an X step when neither works.
- Main tank casts nothing defensive while Shadron is above 50% and he is above 25% — no Shield Wall on
  the pull, no Last Stand on the first Flame Breath. Under 25% he spends exactly one, the weakest he has,
  and no second until the first has run out. Past Shadron 50% he chains them weakest first, never
  stacked. The gate stays open after Shadron dies and opens on its own in a run where it is never called.
- Main tank holds Y **513.0** between waves and under left waves, melee on the boss stand with him, and
  both still clear the line at 524. He walks to 490.0 for right waves as before.
- Bloodlust does **not** go out at Tenebron 70%. It goes when Shadron touches down, or as soon as Gift of
  Twilight drops if Shadron's acolyte is up.
- Rogues cast Tricks on the main tank while on Sartharion and on the off-tank while on a drake, never
  while hitting a Lava Blaze or a whelp. Hunters unchanged.
- When Shadron lands with Tenebron alive the off-tank takes **both to Shadron's spot** and re-taunts
  Tenebron if it peels on the way, rather than arriving alone with Tenebron left north on the melee.
- Rounds E–L still hold: pull drag and settle, off-tank never swinging at Sartharion, melee tracking a
  drake on X while the corridor owns Y, the Twilight Realm exit waiting out a wave.

**Not in scope:** no build (the module cannot be compiled headless here) and no git operation — nothing
is committed without an explicit instruction naming the command.

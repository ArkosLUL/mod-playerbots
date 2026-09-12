# Hunter mana management rework — Viper instead of mana potions

## Context

Hunter bots run dry on Thorim and Mimiron and never swap to Aspect of the Viper. Verified from RaidObs
traces, not inferred. In `603_1_thorim_1789155415.ndjson`, hunter `Trueshot` (guid 5141) sits between
**0.03% and 2.6% mana at 100% health for ~110 seconds**, climbs to ~25%, drains to 0.7%, dies. Zero
Viper casts in that trace or the Mimiron one.

Three separate faults stack up:

**1. A leaked strategy kills Aspect of the Viper outright.**
[UldStrategy.cpp:469-471](../../../src/Ai/Raid/Uld/UldStrategy.cpp) pushes `thorim nature resistance
action`; [BossAuraActions.cpp:51-57](../../../src/Ai/Base/Actions/BossAuraActions.cpp) executes
`ChangeStrategy("+rnature", BOT_STATE_COMBAT)`. The Viper trigger
([HunterTriggers.cpp:95-105](../../../src/Ai/Class/Hunter/HunterTriggers.cpp)) hard-returns `false`
whenever `rnature` is present, so the mana check is never reached. **Nothing removes `rnature`** — the
only `-rnature` in the repo is [RaidAq40Actions.cpp:32-33](../../../src/Ai/Raid/Aq40/RaidAq40Actions.cpp),
and `ApplyInstanceStrategies` strips only *instance* strategies. The trigger picks "the first alive
hunter", so as hunters die or pulls restart, a different hunter each time acquires `rnature`
permanently — until every hunter has it. Proof: both hunters cast `aspect of the wild` at `rel 20.00`
during the **Mimiron** pull, which is `HunterNatureResistanceStrategy`'s own node (`ACTION_HIGH` = 20),
not the boss action (`ACTION_RAID` = 60) — and Mimiron has no nature-resistance trigger at all.
Side effect: `rnature`/`bdps`/`bspeed` are siblings
([HunterAiObjectContext.cpp:45-59](../../../src/Ai/Class/Hunter/HunterAiObjectContext.cpp)), so
`+rnature` also evicts `bdps` from the combat engine.

**2. The mana potion steals the hunter's one potion slot.** `MediumManaTrigger` (mana < `mediumMana` =
40%) pushes `mana potion` at **`ACTION_EMERGENCY` (90)**, the top band in the engine, against the
offensive potion's `ACTION_HIGH` (20)
([UsePotionsStrategy.cpp:29-39](../../../src/Ai/Base/Strategy/UsePotionsStrategy.cpp)). WotLK shares one
potion cooldown per fight, so this costs the hunter its Potion of Speed. `Trueshot` drank Runic Mana
Potion (43186) at t=107s on Thorim and got no offensive potion; melee bots got Potion of Speed and
casters Potion of Wild Magic in the same pull.

**3. Viper's band cannot sustain anything.** Entry is `mana < lowMana / 2` — `lowMana` is 15, integer
division gives **7%**. A hunter is long past useful before it fires.

Intended outcome: hunters stop drinking mana potions entirely and carry their in-combat mana on Aspect
of the Viper, freeing the potion slot for Potion of Speed. Hunters are **already stocked correctly** —
`InitPotions` puts them on the agility ladder `{POTION_OF_SPEED, HASTE_POTION}`
([PlayerbotFactory.cpp:4260-4299](../../../src/Bot/Factory/PlayerbotFactory.cpp)) and they already
pass `IsDps`. Hunter `Nightwarrior` did take Potion of Speed on Mimiron. Nothing needs adding there;
the slot just has to be free.

### Decisions already taken

- Viper band: **enter < 30%, exit ≥ 60%** (exit is the value already hard-coded today).
- Mana potions: **block use and stop stocking**, for hunters that know Aspect of the Viper.
- Aspect of the Wild: keep it — one designated hunter holds it, every other hunter stays free to Viper,
  and the role hands off when the holder itself runs low.
- The Viper trigger's `rnature`/`bspeed` early-return **stays as written**; it becomes harmless once no
  hunter carries a leaked `rnature`.
- **Viper Sting is out of scope** — left exactly as it is.
- Paladin `+rfire`/`+rfrost`/`+rshadow` have the identical leak but are **out of scope**; record as a gap.

Aspect of the Viper is learned at level 20 (verified in `spell.reference.csv`, spell 34074
`SpellLevel` 20), so hunters below 20 must keep the old mana-potion behaviour.

## Part A — Unblock Aspect of the Viper

Hodir already solves this exact problem correctly for paladin frost aura, without `ChangeStrategy`, and
`docs/raids/README.md:156-168` prescribes that shape ("Cast directly and suppress the competition; no
`ChangeStrategy`, no persistent state, self-reverting"). Read all four before editing:
`src/Ai/Raid/Uld/Util/UldEncounter_Hodir.cpp:829` (the holder walk),
`src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.cpp:85-95`,
`src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp:75-78`,
`src/Ai/Raid/Uld/Multiplier/UldMultipliers_Hodir.cpp:133-150`.

### A1. Shared helpers — `src/Ai/Base/Util/GenericBuffUtils.{h,cpp}` (namespace `ai::buff`)

`FindBossByName` is `static` in `BossAuraTriggers.cpp:18`. Move it here verbatim (drop `static`, keep
its comment) so trigger and multiplier resolve the boss identically; update the call sites in
`BossAuraTriggers.cpp` (lines 48, 99, 150, 176 and the shadow trigger).

Add `Player* GetNatureResistanceHunter(PlayerbotAI* botAI, Player* bot);`. Walk `bot->GetGroup()`:

- **Candidate**: alive, `CLASS_HUNTER`, on `bot->GetMapId()`, knows any of the four
  `SPELL_ASPECT_OF_THE_WILD_RANK_*` constants already declared for `BossNatureResistanceTrigger`.
- **Eligible**: candidate, not currently in Viper (`botAI->HasAura("aspect of the viper", member)`),
  and `member->GetPowerPct(POWER_MANA) >= HUNTER_VIPER_ENTER_MANA_PCT`.
- Return the eligible candidate that already holds the `aspect of the wild` aura; else the first
  eligible in group order; else **the first candidate in group order, eligibility ignored**.

Reusing the Viper band for eligibility is what stops the role flapping — a hunter that hands off at 30%
does not become eligible again until Viper drops at 60%, so a handback cannot race a handoff. Say that
in a comment; it is the non-obvious part.

**The last rule is an unconditional fallback, and that is deliberate**: the raid-wide nature resistance
is worth more than one hunter's mana, so the aura must never lapse. Two consequences to write down in
the comment, because both look like bugs otherwise:

- **A raid with exactly one hunter pins that hunter into Wild for the whole encounter**, and it will run
  low exactly as it does today. Accepted trade, not a regression to chase.
- **During a raid-wide mana trough** every hunter is ineligible, so the fallback holds the first one in
  Wild while the others refill on Viper. The first hunter back over 60% takes the role by rule 2, which
  frees the pinned one — self-correcting, one transition, no flapping.

The handoff costs a brief gap: the outgoing holder's `bdps` Dragonhawk node (`ACTION_HIGH`, 20) stops
being vetoed the same tick the incoming holder's Wild cast is queued (`ACTION_RAID`, 60). The incoming
cast is far higher priority so the gap is about a GCD. Acceptable; do not add cross-bot sequencing for it.

`GenericBuffUtils.cpp` may include `HunterTriggers.h` for the constants; `BossAuraTriggers.cpp` already
includes `HunterBuffStrategies.h`, so Base→Hunter includes are established here.

### A2. `BossNatureResistanceTrigger::IsActive` — `src/Ai/Base/Trigger/BossAuraTriggers.cpp:165-217`

- Delete the `HasStrategy("rnature", ...)` early-out (lines 184-187) and its now-unused local.
- Replace the "first alive hunter" loop (196-216) with
  `return ai::buff::GetNatureResistanceHunter(botAI, bot) == bot;`.
- Keep the cheap class check first, the alive check, the boss lookup and the
  `HasAura("aspect of the wild", bot)` early-out. Drop the knows-the-spell and raid-group duplicates now
  living in the helper.

### A3. `BossNatureResistanceAction::Execute` — `src/Ai/Base/Actions/BossAuraActions.cpp:51-57`

Reduce to the Hodir form. **This single deletion is the leak.** Leave the paladin actions alone.

```cpp
bool BossNatureResistanceAction::Execute(Event /*event*/)
{
    return botAI->DoSpecificAction("aspect of the wild", Event(), true);
}
```

### A4. New multiplier — `src/Ai/Base/Strategy/BossResistanceMultipliers.{h,cpp}`

`BossNatureAspectHoldMultiplier(PlayerbotAI*, std::string const bossName)`, named
`bossName + " nature aspect hold multiplier"`. Check order matters — copy `HodirPaladinAuraMultiplier`:

1. `!action || bot->getClass() != CLASS_HUNTER` → `1.0f`.
2. Name not in `{"aspect of the viper", "aspect of the dragonhawk", "aspect of the hawk",
   "aspect of the monkey", "aspect of the cheetah", "aspect of the pack"}` → `1.0f`. **Name first** —
   this runs for every queued action and the group walk is not free.
3. Boss gate via `ai::buff::FindBossByName`: alive and not friendly. **Must match A2's gate exactly** —
   if the multiplier stops vetoing while the trigger still fires, the `bdps` Dragonhawk node at
   `ACTION_HIGH` and the Wild cast at `ACTION_RAID` thrash every tick. Put that in a comment.
4. `ai::buff::GetNatureResistanceHunter(botAI, bot) == bot ? 0.0f : 1.0f`.

List the whole aspect chain, not just Dragonhawk: a vetoed node takes the **IMPOSSIBLE** branch, which
still pushes its `/*A*/` alternatives, and
`src/Ai/Class/Hunter/Strategy/HunterBuffStrategies.cpp:21-34` chains dragonhawk → hawk → monkey. Monkey
has no alternative, so the chain terminates.

### A5. Register in `InitMultipliers`

- `src/Ai/Raid/Uld/UldStrategy.cpp` beside `HodirPaladinAuraMultiplier` (line 954):
  `"kologarn"`, `"freya"`, `"thorim"`.
- `src/Ai/Raid/VoA/VoAStrategy.cpp`: `"emalon the storm watcher"`, `"archavon the stone watcher"` — the
  same strings the existing actions use (`src/Ai/Raid/VoA/VoAActionContext.h:54-58`).

No build files to touch: the module has no `CMakeLists.txt`, sources are globbed by the core.

## Part B — Retune the Viper band

In `src/Ai/Class/Hunter/HunterTriggers.h`, define the pair so the two halves are visibly one band (today
the exit is a bare `60` in a different function from the entry):

```cpp
// Hunters skip mana potions and carry their in-combat mana on Viper, so it has to take over well
// before the hunter is dry. Leaving at 60% keeps the band wide enough not to flip aspects mid-fight.
constexpr uint8 HUNTER_VIPER_ENTER_MANA_PCT = 30;
constexpr uint8 HUNTER_VIPER_LEAVE_MANA_PCT = 60;
```

- `HunterTriggers.cpp:103-104` — replace `< (sPlayerbotAIConfig.lowMana / 2)` with
  `< HUNTER_VIPER_ENTER_MANA_PCT`.
- `HunterTriggers.cpp:40-41` — replace the literal `60` with `HUNTER_VIPER_LEAVE_MANA_PCT`. Behaviour
  unchanged.

Leave the `rnature`/`bspeed` early-return at lines 97-101 alone.

## Part C — Hunters off mana potions

### C1. Block the use — `src/Ai/Base/Actions/UseItemAction.cpp:402`

```cpp
bool UseManaPotion::isUseful()
{
    // WotLK shares one potion cooldown per fight, so a mana potion here costs the hunter its Potion of
    // Speed. Hunters carry their mana on Aspect of the Viper instead, which they learn at level 20.
    if (bot->getClass() == CLASS_HUNTER && botAI->HasSpell("aspect of the viper"))
        return false;

    return AI_VALUE2(bool, "combat", "self target");
}
```

The `medium mana` node still queues at relevance 90 and is then skipped as USELESS — one wasted queue
iteration, the same shape already accepted as gap D6 in `docs/classes/hunter.md`. `isUseful` is the
right seam; do not make `MediumManaTrigger` class-aware, it is shared by every class.

### C2. Stop stocking — `PlayerbotFactory::InitPotions()`, `src/Bot/Factory/PlayerbotFactory.cpp:4230-4258`

Skip `SPELL_EFFECT_ENERGIZE` for hunters alongside the existing has-no-mana skip.

**Use a level check, not `HasSpell`.** `InitPotions` runs at line 1154 but `InitTalentsTree` runs at
1167, so spell state is not settled when the factory runs — which is exactly why the offensive ladder
immediately below uses `RequiredLevel > level` and `IsDps(bot, true)` (see its comment at line 4260).
So: `bot->getClass() == CLASS_HUNTER && level >= 20`.

`CleanupConsumables()` (`PlayerbotFactory.cpp:4605-4653`) already deletes every `ITEM_SUBCLASS_POTION` on
restock, so mana potions already in hunter bags clear themselves at the next maintenance. No migration
needed.

### C3. Stop buying — `ItemUsageValue::GetConsumableType`, `src/Ai/Base/Value/ItemUsageValue.cpp:1965-2000`

Today this classifies looted/vendor mana potions as `"mana potion"`, so bots buy up to 2 stacks and keep
3. Return no consumable type for mana potions when the bot is a hunter of level ≥ 20, so they read as
vendor trash. Without this, C2 is undone by the next vendor run.

## Verification

Bots hold strategies in memory only — no strategy persistence exists in the repo — so restarting the
worldserver clears every leaked `rnature`. Rebuild, restart, then run **one Thorim pull followed by one
Mimiron pull without an intervening restart**; the second pull is what catches a leak.

Read effective config first; never trust the `.conf`:

```
docker exec ac-worldserver env | grep ^AC_
docker exec ac-worldserver sh -lc 'ls -t /azerothcore/env/dist/logs/botobs/ | head'
```

Against the newest Thorim trace, then the Mimiron one (hunter guids are the roster entries with `"c":3`):

1. **One Wild holder, no leak.** `grep -o '"a":"aspect of the wild","rel":[0-9.]*' <file>` — every hit
   must be `rel 60.00`. Any `rel 20.00` means `rnature` is still being added. The **Mimiron** trace
   should show no Wild casts at all.
2. **Viper fires.** `grep -c '"a":"aspect of the viper"' <file>` > 0 on Thorim, for a hunter that is not
   the Wild holder.
3. **No mana potion.** `grep -c '"sp":43186' <file>` — zero casts by either hunter guid.
4. **Offensive potion instead.** `grep -o '"e":"cast","s":<hunter guid>,"sp":53908' <file>` — present.
   53908 is Potion of Speed; the item is 40211.
5. **No mana pinning.** Re-run the extraction used in the investigation; hunter mana must not sit in the
   0-3% band, and should oscillate roughly between 30% and 60%:
   ```
   grep -o "\[<guid>,[-0-9.]*,[-0-9.]*,[-0-9.]*,[-0-9.]*,[0-9.]*,[0-9.]*" <file> | awk -F, '{print $7}'
   ```
6. **No aspect thrash.** Dragonhawk and Wild casts by the same guid must not alternate within a pull.
7. `tools/botobs/postmortem.py <file> --verify` exits zero.

Check the hand-off by letting the designated hunter drain: the Wild-casting guid should change once, and
the drained hunter should then show a Viper cast. This needs a second hunter above 60% at that moment —
if both are low the fallback correctly keeps the first one in Wild, so a *missing* handoff is only a
defect when another hunter was actually eligible. The roster in these traces has two hunters (`"c":3`).
Baseline to beat — in `603_1_thorim_1789155415.ndjson` the hunters ended the pull at 0.7% and 19.8% mana
with one mana potion and no offensive potion between them.

## Docs

`docs/classes/` is reachable from the module `CLAUDE.md` via `docs/engine/pitfalls.md`, so these edits
fall under the compact-governing-docs rule: **invoke `/compact-docs-writer` before touching them**, not
as cleanup afterwards.

- `docs/classes/hunter.md` — close gap **D3** with the new 30/60 band; record that the band was never the
  reason Viper went uncast, the `rnature` leak was; document that hunters no longer use mana potions and
  why (shared potion cooldown).
- `docs/systems/consumables-and-burst.md` — record the hunter carve-out from the defensive-potion path.
- `docs/raids/ulduar/thorim.md` — the designated-holder behaviour.
- Add as a known gap: the paladin `+rfire`/`+rfrost`/`+rshadow` leak, same add-only pattern, still live
  at Razorscale, Ignis, Freya, Thorim, Mimiron, Sara, Yogg-Saron, Koralon and Toravon.

Delete this plan once the findings land in the docs above.

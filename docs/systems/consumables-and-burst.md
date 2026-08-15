# Burst windows and consumables

One gate times every offensive cooldown, potion and tinker a bot owns. Per-boss windows layered on
top of it live in the raid docs.

## The burst gate

`BurstWindowStrategy` (`src/Ai/Base/Strategy/BurstWindowStrategy.cpp`) registers `getName() == "burst"`,
has **no triggers**, and installs one `HoldBurstUntilTankEngagedMultiplier`. It is in the default
combat list at `AiFactory.cpp:289`, so every non-battleground bot has it, and `strategy -burst`
switches it off per bot.

**`IsBurstCooldownAction(name)` (`src/Ai/Base/Combat/BurstCooldowns.cpp:22-52`) is the single
registry.** A new cooldown only ever needs adding there — the per-boss multipliers all early-out on
the same predicate, so they pick it up for free.

Identification is by **action name, not `dynamic_cast`**. `CastSpellAction` passes the spell name to
`Action(botAI, spell)`, so `getName()` is a reliable identifier, and racials and trinkets follow the
same convention. `dynamic_cast` does not scale to ~25 cooldowns across 10 classes without including
every class header — the two pre-existing per-boss lust gates use it and are the reason to stop.

| Source | Names |
|---|---|
| raid-wide / racial / item | `bloodlust`, `heroism`, `berserking`, `blood fury`, `use trinket`, `use tinker`, `offensive potion` |
| Warrior | `recklessness`, `death wish` |
| Rogue | `adrenaline rush`, `blade flurry` |
| Mage | `arcane power`, `icy veins`, `combustion`, `mirror image`, `presence of mind` |
| Warlock | `metamorphosis` |
| Hunter | `rapid fire`, `bestial wrath`, `readiness` |
| Priest | `shadowfiend` |
| Druid | `berserk`, `force of nature` |
| Paladin | `avenging wrath` |
| Death Knight | `killing machine`, `army of the dead`, `summon gargoyle` |
| Shaman | `fire elemental totem`, `elemental mastery` |

Deliberately **excluded**: `unbreakable armor` and `dancing rune weapon` (tank mitigation) and
`inner focus` (healer mana). Two names carry explicit exemptions inside the multiplier:
`use trinket` is the *generic* trinket action, so for a healer or off-tank it also covers survival
and mana trinkets and is left alone for non-DPS; `shadowfiend` doubles as a mana return, so it stays
available on trash.

### Behaviour by target

| Case | Result |
|---|---|
| Not a burst cooldown | `1.0f` — leaves rotation, mitigation and mana casts untouched |
| Non-boss target, grouped, `BurstOnBossOnly` on | `0.0f` — save it for the boss |
| Non-boss target, solo or config off | `1.0f` — solo bots still burst tough elites |
| Boss target, bot is the main tank or ungrouped | `1.0f` — never gated on itself |
| Boss target otherwise | `1.0f` once a tank has held the boss for the dwell, else `0.0f` |

"Tank has hold" means the boss's victim has been **any tank in the bot's group** — not only the main
tank — **continuously** for the dwell; `TankHasHeldBoss` zeroes the timer whenever it is not, so a
tank swap or tank death re-arms the gate. The boss predicate is `IsDungeonBoss() || isWorldBoss()`,
the de-facto is-boss check used everywhere (also `ShamanTriggers.cpp:488`).

**Any tank counts, deliberately.** A raid that hands a boss-flagged add to its off-tank has
established threat exactly as well as one that gave it to the main tank. Main-tank-only held every
burst cooldown for whole fights — Obsidian Sanctum's drake phase, where the drakes are boss-flagged
and the off-tank holds them, is the case that forced it.

**For `bloodlust` / `heroism` only, the boss is resolved from the main tank's victim** when the bot's
own target is not boss-flagged. Lust is raid-wide and a healer often has nothing selected.
Deliberately **not** extended to personal cooldowns, which would let DPS burn them on trash while a
tank holds a boss somewhere else.

**`holdState` carries the boss GUID and must be `Reset()` on every path that skips
`TankHasHeldBoss`** — without that, the previous boss's timer satisfies the dwell instantly on
the next pull.

Because every bot's gate opens on the same tick, bursts stack with no cross-bot coordination.
Multipliers multiply, so the per-boss lust gates that already existed (Maulgar, Gruul, Anub'arak) AND
with this one correctly.

### Dwell constants and casting order

| Constant | Value | Applies to |
|---|---|---|
| `LUST_DWELL_MS` | 3000 | `bloodlust` / `heroism` |
| `BURST_DWELL_MS` | 5000 | every other burst cooldown |
| `POTION_HOLD_MS` | 5000 | the offensive potion's own independent gate |

The stagger exists to get lust out **first**, so personal cooldowns land inside the haste window.
It has to be 2s, not 1s, because of a subtlety that priority alone cannot fix: **the potion is an
item (off-GCD, instant the moment its gate opens) while heroism is a GCD spell**, so a shaman
mid-cast when a 1s-apart gate opened lost the race every time. Heroism/Bloodlust also sit at
relevance 50 (raised from 30) — clearly above the rotation, below the emergency band.

`POTION_HOLD_MS` is the potion's gate when the `burst` strategy is *not* loaded, which is why it is
tuned alongside `BURST_DWELL_MS` rather than left to the multiplier.

## Offensive potions

Before this, bots only ever used defensive consumables. Stocking lives inside
`PlayerbotFactory::InitPotions()`, so it inherits every existing call site — `MaintenanceAction`
(including the `altMaintenancePotions` path) and `AutoMaintenanceOnLevelupAction` — with no new
plumbing.

| Level band | Melee / physical | Caster |
|---|---|---|
| 45-60 | Haste Potion 22838 | Haste Potion 22838 |
| 61-70 | Insane Strength Potion 22828 | Destruction Potion 22839 |
| 71-80 | Potion of Speed 40211 | Potion of Wild Magic 40212 |

Nothing offensive exists below required level 45, so a DPS bot under 45 stocks none — expected, not
a bug. The caster/physical split reuses the same predicate `InitConsumables` uses to choose wizard
oil vs sharpening stone, so the two stay consistent.

Adding `"offensive potion"` to `burstCooldownNames` is what buys the tank-hold dwell **and** every
per-boss window for free. The `OffensivePotionTrigger` only expresses "want to" (DPS, in combat,
boss target); the multiplier decides "not yet".

**Expansion gating.** The ladder originally gated only on `RequiredLevel`, so a level-68-70 bot drank
WotLK potions. `RandomItemMgr::IsAllowedForLevelExpansion(itemId, level)` is the shared rule,
extracted from three inlined copies:

```
level <= 60 -> itemId < 23728    // first TBC content item
level <= 70 -> itemId < 35570    // first WotLK content item
else        -> no cap
```

Gated on `AiPlayerbot.LimitGearExpansion`, **not** the server's `CONFIG_EXPANSION`, by decision. In
the ladder loop this must be a `continue`, not a `break` — that is what makes it fall through to the
previous expansion's entry.

**Known limitation, stated not fixed:** 22828/22838/22839 sit *below* the 23728 cutoff, so a level-60
bot still receives these TBC-recipe potions. Fixing that needs a per-item expansion tag rather than
an id threshold, and it matches how gear and ammo already behave.

No use-time gate is needed: `CleanupConsumables()` wipes all potion-subclass items immediately before
`InitPotions()` runs, so stale potions clear themselves on the next restock.

WotLK allows one combat potion per fight, so a DPS that pops an offensive potion cannot also use a
healing potion that fight. Accepted for a DPS role. Role is read at stock time and at use time via
`IsDps`; a spec change reconciles on the next restock.

## Engineering tinkers

Two gaps had to close together, and the ordering matters — this is why USE_SPELL scoring was
implemented, reverted, then reinstated.

`UseTinkerAction` walks the equipped gear slots, reads `ITEM_ENCHANTMENT_TYPE_USE_SPELL` off each
enchantment, and fires it with the same `CMSG_USE_ITEM` packet `UseTrinketAction` uses. The reason
that works at all:

- **`WorldSession::HandleUseItemOpcode` does not validate the packet's `spellId` against
  `proto->Spells[]`** (`SpellHandler.cpp:58-202`). It only requires the spell to exist and the item
  to be equipped and usable.
- `Player::CastItemUseSpell` (`Player.cpp:7671-7719`) then iterates `MAX_ENCHANTMENT_SLOT`, finds the
  `USE_SPELL` entry, checks `HasSpellCooldown` itself, and casts with `m_CastItem = item`. It is
  **not** unreachable for bots, contrary to an earlier write-up — `UseTrinketAction` already reaches
  it.

Both the action and `StatsCollector` share one predicate, `IsUsableTinkerSpell`, so **only tinkers
granting a combat-stat aura count**: positive spell, at least one `SPELL_EFFECT_APPLY_AURA` whose
`ApplyAuraName` is `MOD_RATING`, `MOD_STAT`, `MOD_ATTACK_POWER`, `MOD_RANGED_ATTACK_POWER`,
`MOD_DAMAGE_DONE`, `MOD_HEALING_DONE`, `MOD_INCREASE_HEALTH` or `MOD_RESISTANCE`. Grenades,
parachutes and sprints score 0 and are never pressed.

**Both user-facing ranking rules fall out structurally, with no hardcoded ids:**

- Hyperspeed Accelerators always beats Hand-Mounted Pyro Rocket on gloves, because Pyro Rocket is
  damage-only and fails the predicate.
- Tailoring embroidery always beats an engineering cloak tinker, because the enchant loop skips
  `enchant->requiredSkill == SKILL_ENGINEERING` on `INVTYPE_CLOAK` outright.

Reference ids (not needed by the code): Hyperspeed 54758 (item 41093), Pyro Rocket 54998 (41091),
Flexweave 55002 (41111), Nitro Boosts 55016 (41118), EMP Generator 54736 (40776).

**Watch in testing:** if an enchant spell carries no `RecoveryTime`/`CategoryRecoveryTime`,
`GetSpellCooldownDelay` returns 0 and the bot retries every tick — the fallback is a fixed cooldown
constant in the action.

Out of scope and deliberately unused: Frag Belt, Personal EMP Generator, Mind Amplification Dish and
the parachute tinkers — all targeted or utility effects needing their own targeting logic.

## Ammo

Thori'dal, the Stars' Fury is `ITEM_SUBCLASS_WEAPON_BOW`, so every ammo path treated it as
arrow-consuming. Its equip aura **46699 ("Requires No Ammo")** makes `Player::CanUseAmmo` return
`EQUIP_ERR_BAG_FULL6`, which maps to the same string as `EQUIP_ERR_BAG_FULL` — hence endless
"My bags are full". The aura also permanently clears `PLAYER_AMMO_ID`, so the bot never converges:
restock, `SetAmmo`, fail, repeat.

`RangedWeaponNeedsAmmo(bot)` = `!bot->HasAura(46699)` guards five entry points: `FindAmmo`,
`InitAmmo`, `QueryItemUsageForAmmo`, `EquipAction`'s `INVTYPE_AMMO` branch, and
`AmmoCountTrigger::IsActive`. **Use the aura, not `Player::CanUseAmmo`** — the latter also fails
while the bot is dead, which would silently disable ammo restocking on corpses.

`FindAmmoVisitor` is left alone: "how much ammo is in the bags" is still a truthful answer, and the
trigger guard is what stops it mattering.

## Ritual of Souls

The mechanic itself (ritual channel, participant count, soulwell spawn, healthstone hand-out) is
server-side. The module supplies three behaviours: the warlock casts, helpers walk to the portal and
click, and anyone lacking a healthstone walks to the soulwell and clicks.

**The interesting part is the strategy-override architecture.** One toggle,
`nc +ritualofsouls` (off by default), registers the *same* name `"ritualofsouls"` in two places: a
base `RitualOfSoulsStrategy` (join + soulwell, all classes) and a `WarlockRitualOfSoulsStrategy`
(base plus the cast trigger) in the warlock context. Strategy lookup resolves through the class
context first, so the warlock override wins for warlocks and the base serves everyone else — no
duplicated triggers.

Reusable patterns this established:

- **Move-to-GO then click**: `FindNearestGameObject(entry, range)` → `MoveTo` if not at interact
  distance, else `WorldPacket(CMSG_GAMEOBJ_USE) << go->GetGUID()` →
  `HandleGameObjectUseOpcode`. Copied from `EnterTwilightPortalAction` (`OSActions.cpp:182-199`).
- **Cross-bot reservation** to stop a stampede: static map + mutex + expiry cleanup, keyed on the GO
  GUID for helper slots and on the group GUID so only one warlock casts. Shape taken from
  `soulstoneReservations` (`WarlockActions.cpp:250-266`).

**Documented UX gotcha:** the chat command invokes the action directly and works with the toggle off,
but helpers will not respond unless *their* toggle is on — so a command-cast is only useful when the
group has the strategy enabled.

Ids to confirm against the DB rather than trust: Soulwell GO ~181621, Ritual of Souls spells 29893
(R1) / 58887 (R2), and the participant requirement, which lives in the ritual GO's
`gameobject_template` row (`type = 18`, `GAMEOBJECT_TYPE_RITUAL`).

## Config

| Key | Default | Meaning |
|---|---|---|
| `AiPlayerbot.BurstOnBossOnly` | 1 | Grouped bots hold burst for dungeon/world bosses instead of trash |
| `AiPlayerbot.OffensivePotions` | 1 | Stock and use DPS offensive potions |
| `AiPlayerbot.LimitGearExpansion` | 1 | Also gates the potion ladder by expansion |

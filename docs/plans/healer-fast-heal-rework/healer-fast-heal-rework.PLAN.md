# Healer fast-heal rework: Disc Priest + Holy Paladin

## Context

Two healer specs are leaning on their cheap fast heal in situations where a better spell exists.

**Discipline Priest** currently casts Flash Heal in four of its five health bands. Flash Heal is
not a Disc spell in any meaningful sense — the spec's throughput comes from Power Word: Shield
(Divine Aegis, Borrowed Time), Penance, Prayer of Mending and tank Renew. Flash Heal wins globals
that those spells should have. Decision: remove Flash Heal from the Disc ladder entirely, with no
replacement filler. The bot idling a global when PW:S / Penance / PoM are all on cooldown is the
accepted trade.

**Holy Paladin** has the Infusion of Light proc wired backwards. Verified 3.3.5 behaviour:
Infusion of Light (talent 53569/53576, proc aura 53672/54149) reduces the cast time of the next
**Flash of Light** by 0.75/1.5s and increases the crit chance of the next **Holy Light** by
10/20%. At 2/2 that makes Flash of Light instant (base cast 1.5s). The existing
`infusion of light` trigger node spends the proc on `holy light on party`, which only ever
collects the crit half and throws away the free instant.

Two consequences of getting this right:

1. `SpellInfo::CalcCastTime(caster)` runs `Unit::ModSpellCastTime` →
   `ApplySpellMod(SPELLMOD_CASTING_TIME)`
   ([Unit.cpp:11726-11737](../../../../../src/server/game/Entities/Unit/Unit.cpp#L11726-L11737)),
   and `PlayerbotAI::CanCastSpell` gates the moving-veto on that computed value
   ([PlayerbotAI.cpp:3357-3366](../../../src/Bot/PlayerbotAI.cpp#L3357-L3366)). So with a 2/2 proc up the
   bot sees Flash of Light at 0 cast time and **can cast it while moving** — the only heal that
   qualifies besides Holy Shock.
2. With Sacred Shield on the target, Flash of Light additionally procs a 12s HoT via
   `spell_pal_infusion_of_light::HandleProc`
   ([spell_paladin.cpp:1514-1546](../../../../../src/server/scripts/Spells/spell_paladin.cpp#L1514-L1546)).
   The Holy strategy already keeps Sacred Shield on the tank, so this comes for free.

Outside that proc window, Flash of Light should not be cast at all — Holy Light stays the main
heal, which is what the comment at
[HealPaladinStrategy.cpp:124-125](../../../src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp#L124-L125)
already claims but the ladder does not enforce.

Third item from the request — Beacon of Light showing as spell IDs 53652 / 53653 / 53654 in Skada
— needs **no change**. Beacon is single-rank 53563; those three IDs are the server-side beacon
copy heals for Holy Light / Flash of Light / Holy Shock respectively
([spell_paladin.cpp:131-135](../../../../../src/server/scripts/Spells/spell_paladin.cpp#L131-L135),
dispatched in `spell_pal_light_s_beacon::HandleProc`). Three entries means the beacon is working.

## Change 1 — Discipline Priest: drop Flash Heal

### `src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp`

Delete the `flash heal on party` `NextAction` from four trigger nodes. Leave every other entry at
its current relevance — the point is removal, not re-pricing.

| Node | Line | Delete |
|---|---|---|
| `party member critical health` | [:58](../../../src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp#L58) | `flash heal on party`, `ACTION_CRITICAL_HEAL + 4.5f` |
| `weakened soul on party member` | [:98](../../../src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp#L98) | `flash heal on party`, `ACTION_MEDIUM_HEAL + 7` |
| `party member low health` | [:110](../../../src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp#L110) | `flash heal on party`, `ACTION_MEDIUM_HEAL + 4.5f` |
| `party member medium health` | [:158](../../../src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp#L158) | `flash heal on party`, `ACTION_LIGHT_HEAL + 7.5f` |

Remember to drop the trailing comma on the entry that becomes last in each list.

The `weakened soul on party member` node is left with `penance on party` alone. Its comment at
[:90-92](../../../src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp#L90-L92) says "top it off with a
direct heal instead of retrying PW:S" — that no longer describes the node. Rewrite it to say
Penance is the direct heal on an already-shielded target, and that it leads here because PW:S
sits on top of every band so Weakened Soul is up on the heal target almost permanently.

### `src/Ai/Class/Priest/Strategy/GenericPriestStrategyActionNodeFactory.h`

`penance_on_party` at
[:204-212](../../../src/Ai/Class/Priest/Strategy/GenericPriestStrategyActionNodeFactory.h#L204-L212)
carries `/*A*/ { NextAction("flash heal on party") }`. That edge re-introduces Flash Heal on every
Penance cooldown, so the removal above would be cosmetic without cutting it. Empty the alternative
list to `/*A*/ {}`.

Keep the node itself — its `remove shadowform` prerequisite is still needed.

Do **not** touch `flash_heal` / `flash_heal_on_party` at
[:186-203](../../../src/Ai/Class/Priest/Strategy/GenericPriestStrategyActionNodeFactory.h#L186-L203), or
their context registrations at
[PriestAiObjectContext.cpp:200-201](../../../src/Ai/Class/Priest/PriestAiObjectContext.cpp#L200-L201) —
Holy still uses Flash Heal at
[HolyPriestStrategy.cpp:116,155](../../../src/Ai/Class/Priest/Strategy/HolyPriestStrategy.cpp#L116).

### Known side effect (accept, do not fix)

`HolyPriestStrategy` (`"holy dps"`) derives from `HealPriestStrategy` and calls
`HealPriestStrategy::InitTriggers`
([HolyPriestStrategy.cpp:27,43](../../../src/Ai/Class/Priest/Strategy/HolyPriestStrategy.cpp#L27)). That
strategy is added to **ungrouped** non-shadow priests
([AiFactory.cpp:428-432](../../../src/Bot/Factory/AiFactory.cpp#L428-L432)), so a solo Holy priest also
loses Flash Heal. This is pre-existing structure — a solo Holy priest already runs the Disc
ladder. Grouped Holy is unaffected: it runs `HolyHealPriestStrategy`, which is a sibling of
`GenericPriestStrategy`, not a subclass of `HealPriestStrategy`
([HolyPriestStrategy.cpp:71-74](../../../src/Ai/Class/Priest/Strategy/HolyPriestStrategy.cpp#L71-L74)).

`PriestNonCombatStrategy` never lists Flash Heal, so out-of-combat healing is untouched.

## Change 2 — Holy Paladin: Flash of Light only on Infusion of Light

### `src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp`

**a) Flip the Infusion of Light node**
([:76-83](../../../src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp#L76-L83)) from
`holy light on party` to `flash of light on party`, keeping the relevance at
`ACTION_MEDIUM_HEAL + 6.5f` (26.5). Add a comment covering the two things a reader cannot see from
the code: Infusion of Light cuts the *Flash of Light* cast time (instant at 2/2, so it is the one
heal that lands while the bot is moving), and on a Sacred-Shielded target it also drops a 12s HoT.

Position matters and is deliberate: at 26.5 the node sits under the critical band, so
Holy Shock (36) / Divine Favor (35.5) / Holy Light (34) still lead on a dying target. The instant
Flash only takes the global when those fail — which is exactly the moving case, since
`CanCastSpell` vetoes every cast-time spell while `bot->isMoving()`.

**b) Remove `flash of light on party` from all four health bands:**

| Node | Line | Delete |
|---|---|---|
| `party member critical health` | [:42](../../../src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp#L42) | `ACTION_CRITICAL_HEAL + 3` |
| `party member low health` | [:90](../../../src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp#L90) | `ACTION_MEDIUM_HEAL + 3.5f` |
| `party member medium health` | [:120](../../../src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp#L120) | `ACTION_LIGHT_HEAL + 8` |
| `party member almost full health` | [:131](../../../src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp#L131) | `ACTION_LIGHT_HEAL + 3` |

Fix trailing commas. The comment at
[:124-125](../../../src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp#L124-L125) ends with "Flash is
what is left when the mana saver vetoes it" — that sentence is now false; drop it and keep the
glyphed-Holy-Light rationale.

Everything else in the ladder stays put. Holy Shock keeps leading each band, so the bot still has
an instant available whenever it is off cooldown.

### `src/Ai/Class/Paladin/Strategy/GenericPaladinStrategyActionNodeFactory.h`

Delete the `holy_light_on_party` ActionNode entirely — the creator registration at
[:33](../../../src/Ai/Class/Paladin/Strategy/GenericPaladinStrategyActionNodeFactory.h#L33) and the
function plus its comment at
[:161-169](../../../src/Ai/Class/Paladin/Strategy/GenericPaladinStrategyActionNodeFactory.h#L161-L169).

Its only content is `/*A*/ { NextAction("flash of light on party") }`, the fallback that would
otherwise put Flash of Light straight back into every band cleaned out above (mana veto, out of
range, moving). With the alternative gone the node has no prerequisite, no alternative and no
continuer, so removing the creator is equivalent — the engine resolves `holy light on party` to
the plain action from
[PaladinAiObjectContext.cpp:300](../../../src/Ai/Class/Paladin/PaladinAiObjectContext.cpp#L300).

Leave the self-target `flash_of_light` node at
[:154-160](../../../src/Ai/Class/Paladin/Strategy/GenericPaladinStrategyActionNodeFactory.h#L154-L160)
alone.

### Known side effects of dropping that fallback (accept, do not fix)

The factory is shared via `GenericPaladinStrategy` and `GenericPaladinNonCombatStrategy`, so two
other strategies lose the Holy-Light-to-Flash fallback:

- `OffhealRetPaladinStrategy` ([:177](../../../src/Ai/Class/Paladin/Strategy/OffhealRetPaladinStrategy.cpp#L177),
  [:185](../../../src/Ai/Class/Paladin/Strategy/OffhealRetPaladinStrategy.cpp#L185)) — harmless, it already
  lists `flash of light on party` explicitly at :193 and :201.
- `GenericPaladinNonCombatStrategy` ([:24-25](../../../src/Ai/Class/Paladin/Strategy/GenericPaladinNonCombatStrategy.cpp#L24-L25))
  — out-of-combat Holy Light failures no longer fall through to Flash. Out of combat there is no
  time pressure and the strategy already has its own Flash nodes at :22-23.

Beacon of Light and Sacred Shield wiring is untouched.

## Change 3 — docs

Both spec write-ups carry ladder tables that this change invalidates.

- `docs/classes/priest.md` — the Disc ladder table, and the "Second pass — Penance was still being
  suppressed" section, which reasons about Flash Heal outranking Penance. Replace with a short
  note that Flash Heal was removed from Disc outright and why (PW:S / Penance / PoM / Renew carry
  the spec; an idle global is preferred over a Flash Heal).
- `docs/classes/paladin.md` — the ladder table at ~:66-89 and the Infusion/Holy Shock rationale at
  ~:13-17, which states the proc is spent on Holy Light. Correct the mechanic (cast-time cut is on
  Flash of Light; Holy Light gets the crit) and note the moving-cast and Sacred-Shield-HoT
  consequences. The "Holy Light over Flash second pass" section at ~:106-119 needs the same
  treatment.

While in `docs/classes/paladin.md`, the note claiming Lay on Hands has no Forbearance guard is
stale — guards exist at
[PaladinActions.cpp:590,595](../../../src/Ai/Class/Paladin/Actions/PaladinActions.cpp#L590). Fix it in
passing.

## Verification

The module cannot be compiled headless in this environment, so verification is static plus a
hand-off.

**Static, done here:**

1. `grep -rn "flash heal" src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp` → no hits.
2. `grep -rn "flash heal on party" src/Ai/Class/Priest/` → hits only in `HolyPriestStrategy.cpp`,
   `PriestAiObjectContext.cpp`, and the two node factories. No hit in the `penance_on_party`
   alternative.
3. `grep -rn "flash of light on party" src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp` →
   exactly one hit, inside the `infusion of light` node.
4. `grep -rn "holy light on party" src/Ai/Class/Paladin/Strategy/GenericPaladinStrategyActionNodeFactory.h`
   → no hits.
5. Every `NextAction` list still compiles as a valid braced initialiser — no dangling commas, no
   empty `{}` where a list is required.
6. Every action name still referenced by a strategy is still registered in its
   `*AiObjectContext.cpp`.

**Hand-off to the user (needs a build + a running realm):**

1. Build the server with the module.
2. Disc priest bot in a raid group: pull a boss and watch the combat log / Skada. Expect zero
   Flash Heal casts, Penance on cooldown, PW:S rotating across targets, Renew on the main tank.
   Expect visible idle globals when all three are down — that is the intended trade, not a bug.
3. Holy paladin bot: Skada should show Flash of Light casts only in bursts that follow a Holy
   Shock crit, and `Flash of Light` (66922, the Sacred Shield HoT) appearing on the tank.
4. Move the paladin bot while a group member is hurt and Infusion of Light is up (2/2 talent):
   it should land an instant Flash of Light mid-move instead of standing there doing nothing.
5. Confirm the beacon mirrors still show up as 53652 / 53653 / 53654 — 53653 (the Flash mirror)
   should now be rarer and 53652 (the Holy Light mirror) more common.

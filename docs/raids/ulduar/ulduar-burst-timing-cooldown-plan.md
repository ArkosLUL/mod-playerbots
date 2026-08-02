# Ulduar burst/lust timing windows

## Context

Ulduar bot raids fire Bloodlust/Heroism (and every other burst cooldown) ~3s after the main tank
engages, on every boss, regardless of phase. That is correct for a patchwerk fight and wrong for the
several Ulduar encounters whose real DPS check arrives minutes after the pull — a 10-minute lust
spent on Razorscale's air phase, Mimiron's Leviathan MK II, or Yogg-Saron's Sara phase is gone for
the part of the fight that actually wipes bot raids.

The mechanism to fix this already exists and is used by **ten other raids** — Kara, SSC, TK, BT, SWP,
Hyjal, ZA, Gruul, ToC and Naxxramas all register a per-raid `Multiplier` that returns `0.0f` for
burst-cooldown actions outside the encounter's DPS window. **Ulduar and ICC are the only raids with
no burst gating at all**: [UldMultipliers.h](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.h)
contains exactly one multiplier (Algalon Dispersion reserve).

This plan adds the Ulduar equivalent, following the Naxxramas implementation, which is the current
best-practice version of the pattern.

### How lust currently reaches the bot

1. `HeroismTrigger` / `BloodlustTrigger` → `ShamanBoostTrigger::IsActive()`
   ([ShamanTriggers.cpp:475-493](modules/mod-playerbots/src/Ai/Class/Shaman/Trigger/../ShamanTriggers.cpp#L475-L493)):
   bot lacks the aura **and** at level 60+ the current target is `IsDungeonBoss() || isWorldBoss()`.
2. `NextAction("heroism", 50.0f)` in
   [GenericShamanStrategy.cpp:136-137](modules/mod-playerbots/src/Ai/Class/Shaman/Strategy/GenericShamanStrategy.cpp#L136-L137).
3. `HoldBurstUntilTankEngagedMultiplier`
   ([BurstWindowStrategy.cpp:12-57](modules/mod-playerbots/src/Ai/Base/Strategy/BurstWindowStrategy.cpp#L12-L57))
   holds it for `LUST_DWELL_MS = 3000` after the main tank has the boss, and blocks all burst on trash.

There is no Sated/Exhaustion check anywhere, and no Drums/Time Warp support — lust is shaman-only.
The boss-only target gate matters for the design: **burst can only ever fire while the bot's current
target is a boss-flagged creature**, so adds-only phases are already covered and need no gate.

## Per-boss verdict

Confirmed against the core scripts in `src/server/scripts/Northrend/Ulduar/Ulduar/`.

| Boss | Verdict | Window | Why |
|---|---|---|---|
| Razorscale | **Gate** | Hold all burst while airborne; hold lust until the permanent ground phase | `boss_razorscale.cpp:254` — Razorscale takes **zero damage** while `Z > 440`. Permanent ground phase at `<50%` HP (`:451,465`). |
| Mimiron | **Gate (lust)** | Hold lust until phase 4 | Damage in P1-P3 counts, so only lust is held. P4 (all three mechs up) is the enrage-risk burn and the fight outlasts a 10-min lust. |
| Yogg-Saron | **Gate** | Hold all burst until P2; hold lust until P3 | P1 damage on Sara is wasted. P3 (no Shadow Barrier, no Guardian) is the body burn. |
| Assembly of Iron | **Gate (lust)** | Hold lust until one council member is left | Also covers the hard mode: Steelbreaker-last means the empowered phase-3 Steelbreaker is the survivor. |
| Freya | **Gate (lust)** | Hold lust until the final phase | Freya carries `SPELL_ATTUNED_TO_NATURE` (62519) for the whole wave phase, reducing damage taken; the core removes it after wave 6 and enters `EVENT_PHASE_FINAL` (`boss_freya.cpp:446,521-529`). |
| Thorim | **Gate (cheap)** | Hold all burst until he is on the arena floor | Largely redundant — the gauntlet adds are not boss-flagged — but a one-line safety against a stray lust while he is immune on the balcony. |
| Hodir | No change | — | Hard mode is a 3-minute race; any hold risks the timer. |
| General Vezax | No change | — | The hard-mode burn is the Saronite Animus, which is not boss-flagged, so lust cannot fire there anyway. |
| Algalon | No change | — | 6-minute hard enrage, burn from the pull. |
| Flame Leviathan | No change | — | Vehicle fight; bots cannot cast lust from a seat. |
| Ignis, Auriaya, Kologarn | No change | — | No phases; pull is the right window. |
| XT-002 Deconstructor | Out of scope | — | The only Ulduar boss with **zero** playerbot code (no triggers, actions or enums). Gating it would mean writing the encounter first. |

## The change

### 1. Findings doc

Write `docs/raids/ulduar/ulduar-lust-timing-findings.md` with the verdict table above plus the core
script evidence, matching the existing `ulduar-*-findings.md` files.

### 2. `UlduarBurstWindowMultiplier`

New multiplier in
[UldMultipliers.h](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.h) /
[UldMultipliers.cpp](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.cpp), modelled directly on
`NaxxBurstWindowMultiplier`
([NaxxMultipliers.h:141-174](modules/mod-playerbots/src/Ai/Raid/Naxx/NaxxMultipliers.h#L141-L174),
[NaxxMultipliers.cpp:596-672](modules/mod-playerbots/src/Ai/Raid/Naxx/NaxxMultipliers.cpp#L596-L672)):

- `GetValue`: bail on `!IsBurstCooldownAction(action->getName())`
  ([BurstCooldowns.cpp:49-52](modules/mod-playerbots/src/Ai/Base/Combat/BurstCooldowns.cpp#L49-L52)),
  then bail on `!bot->IsInCombat()`, then return the memoised verdict for this tick.
- Two tiers instead of Naxx's one. `EvaluateWindow()` returns a small `{bool allowAll; bool allowLust;}`
  struct, cached with the `cachedAtMs`/`cachedValue` idiom Naxx uses. `GetValue` picks the lust tier
  when the action name is `"bloodlust"` or `"heroism"`, the general tier otherwise.
- `EvaluateWindow()` does **one** pass over `AI_VALUE(GuidVector, "possible targets no los")`
  collecting the entries it cares about (Razorscale, the three Mimiron mechs, the three council
  members, Freya, Thorim), then branches per boss. Falls back to
  `bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true)` for Yogg, which is what the existing Yogg
  triggers use because Yogg is not reliably in the threat list. Default `{true, true}`.

Per-boss logic, all of it reusing detection that already exists:

| Boss | `allowAll` | `allowLust` |
|---|---|---|
| Razorscale | `Z <= RAZORSCALE_FLYING_Z_THRESHOLD` | that **and** `HP < 50%` **and** `!HasAura(SPELL_STUN_AURA)` — i.e. the existing `RazorscaleBossHelper::IsGroundPhase()` condition ([UldBossHelper.cpp:82-88](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L82-L88)) |
| Mimiron | `true` | all three of `NPC_LEVIATHAN_MKII` / `NPC_VX001` / `NPC_AERIAL_COMMAND_UNIT` alive, the phase-4 test from [UldTriggers_Mimiron.cpp:298-325](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.cpp#L298-L325) |
| Yogg-Saron | in P2 or P3 | in P3 |
| Assembly of Iron | `true` | exactly one of Steelbreaker / Molgeim / Brundir alive |
| Freya | `true` | `!HasAura(SPELL_ATTUNED_TO_NATURE)` **or** `HP <= 25%` (fallback) |
| Thorim | `Z <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD` | same |

Freya is the only boss needing a fallback: her final phase is six minutes of waves away, so a
`FREYA_LUST_FALLBACK_PCT = 25.0f` release keeps a lust from being held forever if the aura check ever
misses. Every other window is on the mandatory path to the kill.

**Do not call `RazorscaleBossHelper::UpdateBossAI()` from the multiplier** — it side-effects into
`AssignRolesBasedOnHealth()` ([UldBossHelper.cpp:62-75](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L62-L75)).
Read the boss straight off the target sweep instead.

### 3. Yogg phase helpers

`IsPhase2()` / `IsPhase3()` currently live as methods on `YoggSaronTrigger`
([UldTriggers_YoggSaron.cpp:48-61](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_YoggSaron.cpp#L48-L61)),
so the multiplier cannot reach them. Promote the bodies to free functions
`YoggSaronInPhase2(PlayerbotAI*)` / `YoggSaronInPhase3(PlayerbotAI*)` in
`Util/UldBossHelper.{h,cpp}` and have the two trigger methods delegate. No behaviour change, no
duplicated aura logic.

### 4. Enum + registration

- `Util/UldBossHelper.h` — add `SPELL_ATTUNED_TO_NATURE = 62519` to the Freya block (~line 70). Every
  other id and threshold this needs already exists: `NPC_YOGG_SARON`, `SPELL_SHADOW_BARRIER`,
  `NPC_GUARDIAN_OF_YS`, the Mimiron mech entries, `RAZORSCALE_FLYING_Z_THRESHOLD`,
  `SPELL_STUN_AURA`, `ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD`; `NPC_FREYA`, `NPC_THORIM` and the
  council entries come from core `ulduar.h` via `UldScripts.h`.
- `UldStrategy.cpp:478-482` — `multipliers.push_back(new UlduarBurstWindowMultiplier(botAI));`
  alongside the existing `AlgalonMultiplier`. No other wiring: the raid strategy, its trigger/action
  contexts and the map-603 auto-activation are already in place, and AzerothCore globs `src/**` so
  there is no build file to touch.

No config key — per the decision this is always active inside Ulduar, matching `AlgalonMultiplier`
and every other raid's burst gating.

## Verification

The module cannot be compiled in this environment, so this is static verification plus a hand-off.

Static:
- `UlduarBurstWindowMultiplier` is the only new registration in `RaidUlduarStrategy::InitMultipliers`,
  and `IsBurstCooldownAction` stays the single gate for which actions are affected.
- Every boss branch has a terminating condition on the path to the kill; Freya is the only one with a
  timed/HP fallback.
- No `GetData()` reads and no `EventMap` timer reads — both are documented as fragile in
  [UldHardMode.h:23-25,71-72](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldHardMode.h#L23-L25).
- Run `python apps/codestyle/codestyle-cpp.py`.

In-game (user, needs a build), per boss with a shaman in the bot raid:
1. **Razorscale** — no lust or personal cooldowns during air phases; lust fires once she is grounded
   below 50%.
2. **Mimiron** — no lust on MK II / VX-001 / Aerial Command Unit; lust fires when all three are up in
   P4. Personal cooldowns fire normally throughout.
3. **Yogg-Saron** — nothing during Sara; personal cooldowns from P2; lust when the Shadow Barrier
   drops and no Guardian is up.
4. **Assembly of Iron** — lust only on the last council member alive; verify with the hard-mode
   kill order (Steelbreaker last) that it lands on empowered Steelbreaker.
5. **Freya** — lust fires when the wave phase ends, or by 25% HP if the raid gets there first.
6. **Thorim** — nothing while he is on the balcony; burst from the moment he lands.
7. **Control**: Hodir, Vezax, Algalon, Ignis, Auriaya, Kologarn, Flame Leviathan — lust still fires
   ~3s after the tank engages, exactly as today.

Also worth confirming in-game: the Mimiron mechs and the Assembly council members are boss-flagged
(`IsDungeonBoss()`), otherwise lust never fires on them today and those two gates are inert. This
cannot be checked from the source tree.

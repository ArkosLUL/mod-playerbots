# Ulduar Burst / Lust Timing — Server-Truth Findings

Backing evidence for `UlduarBurstWindowMultiplier`
([UldMultipliers.cpp](../../../src/Ai/Raid/Uld/UldMultipliers.cpp)), registered in
`RaidUlduarStrategy::InitMultipliers`. Plan: [ulduar-burst-timing-cooldown-plan.md](ulduar-burst-timing-cooldown-plan.md).

Source: `src/server/scripts/Northrend/Ulduar/Ulduar/`. No config key — always active inside Ulduar,
matching `AlgalonMultiplier` and every other raid's burst gating.

---

## How burst reaches the bot

1. `HeroismTrigger` / `BloodlustTrigger` → `ShamanBoostTrigger::IsActive()`: bot lacks the aura **and**
   at level 60+ the current target is `IsDungeonBoss() || isWorldBoss()`.
2. `NextAction("heroism", 50.0f)` in `GenericShamanStrategy`.
3. `HoldBurstUntilTankEngagedMultiplier` holds it for `LUST_DWELL_MS = 3000` after the main tank has
   the boss, and blocks all burst on trash.

Consequences for the design:

- **Burst only ever fires while the bot's current target is a boss-flagged creature**, so adds-only
  phases need no gate of their own.
- There is no Sated/Exhaustion check anywhere, and no Drums / Time Warp support — lust is shaman-only.
- `IsBurstCooldownAction` ([BurstCooldowns.cpp](../../../src/Ai/Base/Combat/BurstCooldowns.cpp)) stays
  the single list of what the multiplier can affect.

Two tiers are gated separately. `allowAll` covers every burst cooldown; `allowLust` covers
`"bloodlust"` / `"heroism"` only, because a 10-minute raid cooldown wants a later window than
personal cooldowns that come back within a phase.

## Per-boss verdict

| Boss | `allowAll` | `allowLust` | Server truth |
|---|---|---|---|
| Razorscale | grounded (`Z <= 440`) | grounded **and** HP < 50% **and** no Stun Aura (62794) | `boss_razorscale.cpp:254` — zero damage taken while `Z > 440`. Permanent ground phase at <50% (`:451,465`); earlier harpoon knockdowns end on a timer. |
| Mimiron | always | all of Leviathan MK II (33432) / VX-001 (33651) / Aerial Command Unit (33670) alive | P1-P3 damage counts, so only lust waits. All three up at once is P4, the enrage-risk burn, and it outlasts a 10-min lust. |
| Yogg-Saron | P2 or P3 | P3 | P1 damage lands on Sara and is wasted. P3 = no `SPELL_SHADOW_BARRIER` (63894) on Yogg and no Guardian of Yogg-Saron (33136) up. |
| Assembly of Iron | always | exactly one of Steelbreaker / Molgeim / Brundir alive | They resurrect each other until one is left. Also covers the hard mode: Steelbreaker-last means the survivor is the empowered phase-3 Steelbreaker. |
| Freya | always | no `SPELL_ATTUNED_TO_NATURE` (62519), **or** HP <= 25% | She carries the aura for the whole wave phase, reducing damage taken; the core removes it after wave 6 and enters `EVENT_PHASE_FINAL` (`boss_freya.cpp:446,521-529`). |
| Thorim | on the arena floor (`Z <= 429.6094`) | same | Largely redundant — the gauntlet adds are not boss-flagged — but a cheap safety against a stray lust while he is immune on the balcony. |
| Hodir | — | — | No change. Hard mode is a 3-minute race; any hold risks the timer. |
| General Vezax | — | — | No change. The hard-mode burn is the Saronite Animus, which is not boss-flagged, so lust cannot fire there anyway. |
| Algalon | — | — | No change. 6-minute hard enrage, burn from the pull. |
| Flame Leviathan | — | — | No change. Vehicle fight; bots cannot cast lust from a seat. |
| Ignis, Auriaya, Kologarn | — | — | No change. No phases; the pull is the right window. |
| XT-002 | — | — | Handled separately by `XT002BurstWindowMultiplier`, which gates on the exposed Heart per mode. |

Freya is the only boss with a fallback release. Her final phase is six minutes of waves away, so
`FREYA_LUST_FALLBACK_PCT = 25.0f` keeps lust from being held forever if the aura read ever misses.
Every other window is on the mandatory path to the kill.

## Implementation notes

- One pass over `AI_VALUE(GuidVector, "possible targets no los")` collects every boss the multiplier
  cares about, then it branches. The verdict is memoised per tick with the `cachedAtMs`/`cachedValue`
  idiom, since the multiplier runs against every candidate action.
- Yogg is resolved with `bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true)` instead, which is
  what the existing Yogg triggers do — he is not reliably on a bot's threat list. `IsPhase2()` /
  `IsPhase3()` moved out of `YoggSaronTrigger` into free `YoggSaronInPhase2()` / `YoggSaronInPhase3()`
  in `Util/UldBossHelper.{h,cpp}` so the multiplier can reach them; the trigger methods delegate.
- **Razorscale is read straight off the sweep.** `RazorscaleBossHelper::UpdateBossAI()` side-effects
  into `AssignRolesBasedOnHealth()`, which reassigns the raid's main tank — never call it from a
  multiplier.
- No `GetData()` reads and no `EventMap` timer reads; both are documented as fragile in
  [UldHardMode.h](../../../src/Ai/Raid/Uld/Util/UldHardMode.h).

## In-game verification (needs a build, shaman in the bot raid)

1. **Razorscale** — no lust or personal cooldowns while airborne; lust once she is grounded below 50%.
2. **Mimiron** — no lust on MK II / VX-001 / Aerial Command Unit; lust when all three are up in P4.
   Personal cooldowns fire normally throughout.
3. **Yogg-Saron** — nothing during Sara; personal cooldowns from P2; lust when the Shadow Barrier
   drops with no Guardian up.
4. **Assembly of Iron** — lust only on the last council member alive; with the hard-mode kill order
   (Steelbreaker last) confirm it lands on empowered Steelbreaker.
5. **Freya** — lust when the wave phase ends, or by 25% HP if the raid gets there first.
6. **Thorim** — nothing while he is on the balcony; burst from the moment he lands.
7. **Control** — Hodir, Vezax, Algalon, Ignis, Auriaya, Kologarn, Flame Leviathan: lust still fires
   ~3s after the tank engages, exactly as before.

Also worth confirming: the Mimiron mechs and the Assembly council members are boss-flagged
(`IsDungeonBoss()`). If they are not, lust never fires on them today either and those two gates are
inert. This cannot be checked from the source tree.

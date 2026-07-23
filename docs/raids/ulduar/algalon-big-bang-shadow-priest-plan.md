# Algalon — Big Bang soak tactic

## Correction (2026-07-22)
Big Bang is **unavoidable raid-wide damage** — full immunity (Paladin **Divine Shield**) does
**not** prevent it. Only mitigation survives it. Soaker role must move to a **Shadow Priest**
using **Dispersion** (90% damage reduction). The prior Protection-Paladin bubble approach is wrong
and must be replaced. Dispersion must also be **reserved**: the designated soaker priest must not
spend it on normal low-mana / critical-health triggers during the fight, so it is up every Big Bang.

## Current implementation (to be replaced)
- `GetAlgalonBigBangSoakerPaladin(Player*)` — `UldBossHelper.cpp:265`. First alive Protection
  Paladin in raid. `UldBossHelper.h:181`.
- `AlgalonBigBangSoakTrigger::IsActive()` — fires only for that paladin while Algalon channels
  `SPELL_ALGALON_BIG_BANG` (64443) and paladin not already phased.
- `AlgalonBigBangSoakAction::Execute()` — `UldActions_Algalon.cpp:78`. Casts `divine shield`, else
  `divine protection`, else falls back to `AlgalonBigBangHideAction` (hide in hole).
- `AlgalonBigBangTrigger` (the hide trigger) excludes the designated soaker (`UldTriggers_Algalon.cpp:53`)
  so the soaker stays out while everyone else hides in a Black/Worm Hole (safe-phase aura
  `SPELL_ALGALON_BLACK_HOLE_DAMAGE`).
- Wired `UldStrategy.cpp:369-374` (hide + soak trigger nodes).

## Dispersion plumbing (available)
- `CastDispersionAction` (self-cast, action name `"dispersion"`) — `PriestActions.h:175`,
  registered `PriestAiObjectContext.cpp:202`. Shadow spec = `AiFactory::GetPlayerSpecTab(bot) == PRIEST_TAB_SHADOW`.
- Normal usage that burns the cooldown: `ShadowPriestStrategy.cpp:42,50` — `"low mana"` and
  `"critical health"` triggers → `NextAction("dispersion", ACTION_HIGH + 5)`.

## Plan
1. **Soaker selection** — replace `GetAlgalonBigBangSoakerPaladin` with
   `GetAlgalonBigBangSoakerPriest(Player*)`: first alive **Shadow Priest** in the Ulduar raid.
   Update helper name + doc comment in `UldBossHelper.h/.cpp`.
2. **Soak action** — `AlgalonBigBangSoakAction::Execute()` casts `"dispersion"` (via
   `botAI->CastSpell("dispersion", bot)` / `DoSpecificAction`). If Dispersion unavailable, fall
   back to `AlgalonBigBangHideAction` (hide in hole).
3. **Reserve Dispersion** — new `UldMultipliers` file with `AlgalonMultiplier`: returns `0.0f` for
   `CastDispersionAction` when the bot is the designated soaker priest, Algalon is engaged, and
   Big Bang is **not** currently channeling. Register via new `RaidUlduarStrategy::InitMultipliers`.
   (Matches ICC multiplier pattern; Uld currently has no multipliers — small new infra.)
4. Rename trigger references (`GetAlgalonBigBangSoakerPaladin` → priest) in
   `UldTriggers_Algalon.cpp` (hide-exclusion + soak trigger).
5. Difficulty: Big Bang id 64443 is difficulty-agnostic; Dispersion is a bot spell — no heroic split.

## Answers (confirmed)
- **Q1:** Reserve Dispersion for **only the designated soaker priest**; other shadow priests disperse normally.
- **Q2:** No living Shadow Priest → **everyone hides, no soaker** (`GetAlgalonBigBangSoakerPriest` returns
  nullptr, so no bot is exempt from the hide trigger).
- **Q3:** Non-soaker bots keep hiding in a Black/Worm Hole; only the designated priest stays out and Disperses.

## Implemented
- `GetAlgalonBigBangSoakerPriest` — `UldBossHelper.cpp` / `.h` (was `...Paladin`).
- Soak action casts `"dispersion"`, falls back to hide — `UldActions_Algalon.cpp:78`.
- `AlgalonMultiplier` reserves Dispersion — `UldMultipliers.cpp/.h`, registered via
  `RaidUlduarStrategy::InitMultipliers` (`UldStrategy.cpp`). Returns 0 for `CastDispersionAction`
  when bot is the soaker priest, Algalon is engaged, and Big Bang isn't currently channeling.
- Trigger refs renamed — `UldTriggers_Algalon.cpp`.

## Note
Boss `find target` qualifier is inconsistent in existing code: hide/soak triggers use
`"algalon observer"`, `AlgalonPhasePunchSwapAction` uses `"algalon the observer"`
(`UldActions_Algalon.cpp:99`). `find target` requires an exact full-name match, so only one can be
correct — possible latent bug in the mismatched one. Multiplier matches the triggers
(`"algalon observer"`). Not touched here.

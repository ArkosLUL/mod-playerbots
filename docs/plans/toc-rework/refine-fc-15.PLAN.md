# refine-fc-15: kites stay healable, Hand of Protection for melee, wall grip keeps its Chains, Reincarnation waits

Wave 4x, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4w `108783a74`, which the
pull below ran.

## Why

2026-10-25 25H kill, 6:43: `env/dist/logs/botobs/649_39_liandra-suncaller_1792947097.ndjson`. 7 deaths of 6 bots,
all before 3:16 (the wipes before it had 30 each).

- **Caster focus, 4:** every caster picks the lowest current health within 35 yd, all the same one.
  - Totemist (Enh, 6-7 champions, 47 s): no Hand of Protection, since `HandOfProtectionFits` took healers and ranged
    only, though Holylight's was ready 23 yd off.
  - Totemist again: Reincarnation brought it back on its corpse at 32% among 4 casters; dead 3 s later.
  - Hellflame (40 s): kites had left it 29-37 yd from every healer; the only ready Hand was 31-49 yd off (30 yd range).
  - Tree (17 s): every external already on cooldown.
- **Kites leave heal reach, 2** (Justice, Malediction): the kite takes the hop farthest from melee, nothing ties it to
  the healers. 30 of 53 kites ended past 30 yd from every healer, most at the wall; the two got 13-29k healing in
  their last 10 s. Nitro runs ended 41-51 yd off in 4 of 12.
- **Two melee champions on one bot, 2** (Agony, Justice): Narrhok's Charge plus a holder's champion. Out of scope.
- **Wall grip race:** `FactionChampionsNoteWallGrip` called `Refresh`, whose `UpdateWalls` on a new millisecond saw
  Death Grip on cooldown and could release the duty before it was marked. Chains of Ice followed 1 of 5 grips, but the
  other 4 found Gorgrim already slowed (Chains of Ice or Frost Shock within 3 s before), the designed release.

## Scope (the user's calls, 2026-10-25)

1. **Kite and Nitro in heal reach:** a hop or run ends within 30 yd (`KITE_HEAL_REACH`, the Hands' reach) of a living
   healer other than the bot, or nearer one than the bot is now; with none such, the clear hop nearest a healer.
   Nitro no longer runs "as far as it can".
2. **Hand of Protection** on any focused non-tank, non-holder, melee included.
3. **Wall grip:** `FactionChampionsNoteWallGrip` marks the duty without a refresh (Obliteration has no Chilblains).
4. **Reincarnation** waits while 2+ champions stand within 8 yd of the corpse (`SelfResurrectTrigger`).
5. **Reader:** healer distance after each kite and Nitro run. An Enh shaman never holds already (`HoldTier`).

## Targets (next 25H)

Kites ending past 30 yd of every healer well under 30 of 53; no kiter dying 35+ yd from every healer; Hand of
Protection on a focused melee DPS; wall grips on an unslowed champion followed by Chains of Ice; no Reincarnation into the pack; fewer than 7
deaths; 25N still kills.

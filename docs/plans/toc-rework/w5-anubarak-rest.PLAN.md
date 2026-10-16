# w5-anubarak-rest — Anub'arak: drag oscillation, pull threat, phase-2 patches, then w5's carry-over

Wave 4d, a refine lane. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics and the shipped design:
[anubarak.md](../../raids/trial-of-the-crusader/anubarak.md). Guide:
`anubarak-master-strategy-guide-toc-25/`. Read `tools/botobs/bosses/anub_arak.py` output before
designing each item. Line numbers are as of `1846fc944`.

Trace (25N kill 4:11, 5 deaths, raid DPS 125k, human tank "Dragon", build carrying waves 3-4c):
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/649_3_anub-arak_1792178435.ndjson`.
Read it with `bosses/anub_arak.py <f>` (sections `--pull --threat --spike --spheres`),
`postmortem.py <f> --probes move.by` (who owned each move) and `--probes anub.pickup`.

## Findings from the 2026-10-16 25N pull

**Drag oscillation.** The bot main tank Bulwark walks the boss about 25 yd every 8-9 s in phase 1
(x 711 ↔ 736) and about 14 yd every 10 s after the emerge; the boss's median distance from the
anchor is 7.8 yd.

- `DragBossOnto` (`ToCActions_Anubarak.cpp:261-284`) stops only with the boss inside
  `ANUBARAK_DRAG_ARRIVE` = 2 yd of the anchor and aims the tank "his current gap" past it.
- He steps only once the tank is about 12 yd away (his melee reach), then closes to about 5 yd, so
  one drag moves him at least about 7 yd: the 2 yd target can't be hit. He overshoots, the next
  tick sees him past the anchor, and the direction flips.
- Knock-on: `reach melee` ↔ `set behind` flipped 216 times; the tank loses swing time.

**Pull death.** Bulwark dragged from the first tick and landed no threat ability for 7 s (first Holy
Shield 7.8 s). Angry (Intercept 1.2 s, Bloodthirst 4.9 s) pulled the boss at 5.4 s and died to one
32.6k hit. Until the patches land (9-12 s) the anchor is the room centre (`GetAnubarakBossAnchor`,
`ToCHelpers_Anubarak.cpp:1170`), so he is dragged twice.

**Phase-2 deaths.**

- The four patches made in phase 1 went to the first four spikes (none left at 1:43).
- Agony held the only `kite` sphere duty from 1:46 to 2:09 and never shot: he spent it all in
  `anubarak avoid spike` (duties: `ComputeDuties`, `ToCHelpers_Anubarak.cpp:856`).
- With no Permafrost, Mighty kited 16.8 s and died; the spike then ran through the ranged:
  Stormweaver, Trueshot and Druidica died at 2:08, two Impales each.
- `anubarak avoid spike` flipped with `reach spell` 130 times and `reach melee` 107 times; Trueshot
  died while `reach spell` walked him back. Same release-on-arrival pattern `refine-beasts` fixed on
  the worms.

Phase 3 and the stretch after the emerge were clean: no deaths, lust at phase 3 as designed.

## Scope

New items first; then w5's carry-over, in its order, as far as the lane holds.

1. **Drag with a deadband.**
   - Start a drag only past a start radius; end it inside a smaller done radius, both measured on
     the boss. Done radius at least half the minimum step (about 4 yd).
   - Plan the tank's spot from his real chase threshold and stop gap, read live (his melee range),
     not constants. Never re-plan while he moves.
   - `ANUBARAK_SUBMERGE_PATCH_CLEARANCE` (`ToCHelpers_Anubarak.h:62`) adds the arrive radius: keep
     it satisfiable, checked against the room with `navprobe` and the trace's positions.
2. **Pull threat.** No drag until both tank patches are latched, which also removes the room-centre
   detour; the tank holds him where it picks him up. The investigator picks and logs the fallback if
   patches never land. Optional: a DPS opener hold on him (no threat brake exists; check
   `docs/engine/pitfalls.md` first).
3. **Phase-2 patch supply.** Shoot spheres before patches run out (e.g. always one patch per
   remaining kite); more than one shooter; never give the duty to the spike's target or a bot
   dodging it, unless it can shoot from where it stands. Item 5 is the backstop.
4. **Spike dodge hold.** Hold the dodge spot until the spike has passed, with a Block rule zeroing
   `reach melee` and `reach spell` by name while the hold is up: the worm pattern,
   `e.Block("northrend worms spot hold", ...)` in `ToCDefinition_NorthrendBeasts.cpp:35-38`.
5. **Kiter immunity.** A kiter with no patch dies once the spike reaches 11.4 yd/s. Impale is
   physical (school 1), so Hand of Protection voids it; the guide's heroic phase 2 has a paladin
   HoP the kiter just outside a patch and, for the last kite, stand still under HoP where he should
   emerge. Also self-immunities (Divine Shield, Ice Block). `anub.kite` `ring` rows and Impale hits
   on kiters (`spike` section) say how often it is needed.
6. **Phase 3 raid-health economy.** Leeching Swarm takes 10/10/20/30% of *current* health a second
   and heals him by it, so the guide keeps the raid low: no healing for melee, hunters and
   affliction warlocks, fast healing only for Penetrating Cold carriers, a cancelaura on stamina
   buffs at phase 3. Needs a heal hold that cannot starve a real emergency; `swarm` section first.
7. **Freezing Slash defensives.** The guide wants one per slash. It is a non-triggered instant with
   no warning: first 7-15 s after the engage or emerge, then every 15-20 s, so only a timer model
   anchored on those edges can place one.
8. **Burrower pacing.** Guide: throttle AoE so each wave dies just before the next (45 s cadence),
   and once he is low, ignore burrowers for him (heroic keeps them coming in phase 3).
9. **Landing and regroup.** The floor break drops the raid into what is likely water under the
   arena; navprobe cannot path out of it. The Web Door (661.6, 144.7) shuts at the engage. Act only
   if the `pull` section shows bots stuck or locked out.
10. **Scarabs.** 20000 threat on a random player each; Acid-Drenched Mandibles stacks (guide: limit
    stacks going into phase 3). Consider a DPS threat brake (none exists, see pitfalls) and stack
    management.
11. **Spider Frenzy.** Burrowers within 12 yd haste each other; 25H brings four a wave to two
    patches, 10H two to one tank. A third patch, or spacing on the patch.
12. **Phase 2 raid positioning.** Guide: raid to one corner for the first spike, back to the centre,
    then stacked where he should emerge; w5 ships only the spike dodge and the boss drag off the
    patches 15 s before each submerge.

**Reader.** `--threat` reports drag reversals per phase and the boss's distance from the anchor;
`--spheres` shows each phase-2 kite's patch supply.

**Wave 4e** (`w8-prepull-staging`) reads this lane's anchor and side points for the pre-pull
stands: keep them reachable through a helper.

## Owns

As `w5-anubarak`: stem `Anubarak`, `docs/raids/trial-of-the-crusader/anubarak.md`,
`tools/botobs/bosses/anub_arak.py`, `tools/botobs/tests/test_anub_arak.py`.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked

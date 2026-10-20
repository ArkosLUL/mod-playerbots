# refine-fc-7: CC that holds, a Cyclone stand, a controlled rogue, defensives where they count

Wave 4l, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4k `11f944062`.

## Why

25H, 2026-10-20, `env/dist/logs/botobs/649_2_caiphus-the-stern_1792487576.ndjson` (wipe 2:14) and
`…_1792487875.ndjson` (wipe 2:01). Grips whole (26-30 yd per link), Thrakgar died both pulls (0:50, 1:05);
kill targets carry 3.7-4.6M health, ~80k/s net with 13 bots on it.

- **CC broken by its own caster.** Cyclone, Polymorph, Fear, Banish, Hex, Entangling Roots carry
  `SPELL_ATTR5_LIMIT_N` (one target per caster per spell, `Aura::IsSingleTargetWith`). The duty freed a bot
  once its cast landed and dealt it the next champion: 29/39 and 39/43 of Druidica's Cyclones ended as its
  next landed (~1.3 s held), warlock Fears 9 times. Druidica did no damage to the kill target for 80 s.
- **Recast duty flapped.** The caster's own old aura in its last 2 s counted as landed: Power 62 duties for
  10 casts.
- **Lay on Hands uncoordinated** (generic `lay on hands on party`): three on Totemist in 8 s, who died at
  0:20; two on Justice.
- **Personal cooldowns late** (generic critical-health): Divine Protection at 20%, Shamanistic Rage at
  13-27%, Enraged Regeneration at 1%; no Divine Shield from Justice or Holylight. A focused bot loses
  40-60% in 1-2 s.
- **Battle res** 4 of 6 on frontline DPS who died again in 7-28 s; three healers then stayed dead.
- **Healers walked** (cast 13-36%): heal spokes re-dealt by armour on every healer death (Tree's 40+ yd
  jumps match the deaths). 13 ranged and healers spent Nitro Boosts on the opening walk: 0 chased runs.
- **The rogue ran free.** Damage to the raid: pull 1 DK 20%, rogue 17%; pull 2 rogue 19% (8 kills), Ret
  17%; the four melee 52-65%. Rogue held 18.5 s and 9 s, disarmed once. Peel only covers melee on healers or
  ranged; melee came after casters in CC. Disarm halves a champion's weapon damage
  (`StatSystem.cpp:1156`).
- **Balance druid stand** was 20-26 yd past the kill target; pack healers 35-60 yd off it; every Cyclone a
  walk in and out.

User's calls: Balance druid hits the gripped healer and Cyclones the healer in the blob; every healer heals
the frontline (the gripped healer can't kill the kill squad); the rogue kept controlled or disarmed.

## Scope

1. **CC tie.** A caster's spell holding a champion with more than `CC_RECAST_LEAD_MS` left is skipped for
   any other champion, unless moving it from a non-healer to a healer; recast on the same one allowed.
   Landed only counts an aura above that lead.
2. **Cyclone stand.** A ranged DPS druid with Cyclone, kill target out and a pack healer left: the spot on
   the 26/20 yd rings round the kill target (all round, `Clear()`) within Cyclone reach (spell range with
   mods + both reaches − 2) of the pack healer nearest the kill target, farthest from the pack centre, then
   nearest the druid; else nearest that healer. Latched until a guid changes, either moves 6 yd, or the spot
   fails `Clear()`. No walk-in CC duty meanwhile. Probe `fc.cyclone`.
3. **Danger champion** (`fc.danger`): first alive of Rogue, DK, Ret, Warrior, not kill target, pulled or
   suspended. Always peeled first, frontline included, unless held (> 1 s) or disarmed; Hammer of Justice
   joins its peels (DR > 2.5 s); in CC it comes right after the healers.
4. **Lay on Hands** dealt as an external: member ≤ 20% with a champion on it, not covered, none in 15 s;
   healers first. Generic `lay on hands on party` blocked in FC.
5. **Personal defensives when focused**: 2+ champions on it and its 2 s health loss projected over 4 s
   ends under 25%: `GetToCPersonalDefensive` with a measured hit, physical pick when a physical champion is
   on it. Node at `ACTION_EMERGENCY + 8.4`.
6. **Battle res**: healers, tanks, then others only while 2+ ready druids are untaken.
7. **Healers**: all on the frontline spokes, each keeping its spoke while alive. Ranged and healers keep
   Nitro Boosts for the chased run.
8. **Reader**: `--cc` own breaks and duties per cast; `--stands` Cyclone stand and chased runs; `--peel`
   Lay on Hands and repeat deaths after Rebirth; new `--damage` and `--personal`. Tests, docs.

## Targets (next 25H)

No CC broken by its own caster, Cyclone median hold over 3 s, pack healers off the kill target controlled
50%+; Cyclone stand both in reach 70%+ of samples, Druidica 40%+ damage casts on the kill target; danger
champion held or disarmed 50%+, its damage share under 12%; one Lay on Hands per member per 15 s; a Rebirth
left for a healer or tank; first personal at 50%+ health on focused deaths; healers cast 50%+, chased runs
over 0; first frontline DPS death after 0:40; 25N still kills.

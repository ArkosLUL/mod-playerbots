# refine-fc-9: chain the next healer, free the casters for CC, externals on any focus

Wave 4n, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4m `d745c4399`, not
yet built: both pulls below ran the 4l binary, so 4m and 4n get tested together.

## Why

25H, 2026-10-21, `env/dist/logs/botobs/649_2_liandra-suncaller_1792602074.ndjson` (wipe 2:41) and
`649_2_liandra-suncaller_1792602582.ndjson` (wipe 2:57); Thrakgar killed at 0:53 and 0:56 in each, nothing
after.

- **No second chain.** No `pull` after the kill; the human gripped the next healer alone (21-22 yd, inside
  heal reach). Pull 2: Liandra latched by `dead` in the pack, 2-22 yd from Erin for 120 s, 98 → 11% at
  1.2%/s (Thrakgar pulled: 2.5%/s).
- **Casters kill healers and ranged from full in 1-2 s** (Hunter, Mage, Warlock, Shadow Priest on one
  victim). `UpdateProtect` counts physical champions only and needs 60% health: no external on any of them.
- **Raid-wide cooldowns unused**: class nodes wait for `medium group heal setting`, and at most 2 members
  had 3+ champions on them at once. One Divine Sacrifice (Holylight, Divine Guardian on 7) at 2:03.
- **Danger stuck on the rogue**; the DK dealt the most both pulls (21%), held 17 s and 0.1 s.
- **Breakable CC can't reach the casters**: the frontline held the Mage 63-85 s, raid members targeted the
  casters 10-52% of their time; 13 and 10 Polymorphs a pull.

## Scope

1. **Next chain on a kill.** When the kill target dies, the first champion healer in kill order (in combat,
   touchable, not pulled, grip can land) gets a chain once `ChainOpener`'s grip is within `CHAIN_WAIT_MS`
   (no DK tank: the bot DK's), for up to 20 s. The opener tells the master. A `pull` still overrides.
2. **Casters left to CC.** Frontline deals melee champions only. Spread DoTs (`… on attacker`) only on the
   kill target.
3. **Externals.** Every champion not CC'd counts (melee within 8 yd). Pain Suppression, Hand of Sacrifice,
   Guardian Spirit at 4+ on a non-tank at any health, else 2+ at ≤ 60%. Hand of Protection still needs a
   physical one.
4. **Divine Sacrifice.** A non-tank with 4+ on it and no external gets it from a paladin with Divine
   Guardian (50%+ health, member in its reach, none up); the paladin drops Divine Sacrifice once Divine
   Guardian is on. Class node blocked while live.
5. **Danger passes on** while the current one is CC'd, stunned or disarmed.
6. **Reader**: `--chain` `after a kill`; `--cc` targeted share; `--peel` champions on each external's
   target, Divine Sacrifice reach, raid-wide casts; `--damage` seconds as danger. Tests, docs.

## Targets (next 25H, with 4m)

A chain within 2 s of each `dead` switch while a champion healer lives, and a second kill; casters
targeted under 10%, Polymorphs and Fears up, casters held 25%+; externals on healers under caster focus,
a Divine Sacrifice before 1:00; danger moves to the DK while the rogue is held, DK share under 15%; no
healer death before 1:00; 25N still kills.

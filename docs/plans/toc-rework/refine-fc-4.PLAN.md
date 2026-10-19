# refine-fc-4: a grip chain started by the player's `pull`, Fire Resistance Aura

Wave 4i, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4h `149a88b6f`.

## Why

25H, 2026-10-19, `env/dist/logs/botobs/649_2_thrakgar_1792361472.ndjson`, `…_1792361718.ndjson`,
`649_2_caiphus-the-stern_1792361982.ndjson`, all wipes. Deathsong (DK) the only human; Felesta bot DK
tank, Bulwark bot paladin tank, Obliteration bot DK.

- **Bot DK late.** Deathsong gripped Liandra at 8 s from the tank line; Obliteration was elected 0.2 s
  later while she was mid-jump (~1 s), 53 yd off, so `GripsFromHere` failed and it walked 3.4-4.5 s to a
  stand derived from her mid-air spot. Landed, she was 25 yd from its post. Pull 2's grip moved her 10 yd.
- **No DK tank opener** in the code; Felesta spent her grip on peels.
- **Pull 1**: Deathsong's ghoul Leap started the fight, 10/10 first picks on melee bots.
- Fire was 7-8% of champion damage on the raid (Hunter 67985, Warlock 68150/68144, Enhancement 65974).

User: the chain is bot DK tank → bot DK → the human, last and farthest; every chain, later healers
included, starts when the user targets a champion and types `pull`; a paladin runs Fire Resistance Aura.

## Scope

1. **Takeover.** `pull` reaches tanks as the reaction `pull my target` (`PullRequestAction`), which arms
   the generic pull (`PullMultiplier` then zeroes everything else). `RaidTrialOfTheCrusaderStrategy::OnTick`
   runs before triggers each tick: at the Faction Champions stage, a generic pull at a champion opens a
   chain request and ends that pull as `PullEndAction` does (pet react state, `pull` position,
   `current target`, `OnPullEnded`), for every tank, so Bulwark drops his.
2. **Chain request** (instance state, readable before the pull, probe `fc.chain`): target, opener (bot
   tank DK knowing Death Grip, in control, lowest guid), link 1 time and spot. Closes on link 2's grip,
   target death, a new request, the pull's end, 15 s without link 1 or 10 s after it without link 2.
3. **Link 1**, `faction champions chain grip` (`ACTION_RAID + 4.6f`): grip from here when in reach and
   LOS and (in combat) 12+ yd away from the pack, else walk to 27 yd from the target toward the chain
   post. Grip more than 3 s off cooldown: reply once and close. Immunity checked without the in-combat
   test. Staging releases the opener; its Death Grip goes only to chains (no peel, class node blocked).
4. **Link 2**: the bot DK walks to the chain post while link 1 is pending, then grips the landed target
   from here. Never elected mid-jump (`EFFECT_MOTION_TYPE` in the controlled slot). Without a chain it
   only continues a pulled kill target still wanting a pull; the next-healer pre-walk and 20% grip are
   gone. CC skips the chain target.
5. **Positions**: bot DK post 50 → 54 yd on the centroid-to-centre ray, the human ~24 yd past it,
   floor-checked with navprobe.
6. **Fire Resistance Aura**: `ResistAura` wants fire on Faction Champions.
7. **Reader** `--chain`: per request the links, gaps, carry, distance off the pack, order verdict, and
   bot grips outside a chain. Tests and docs.

## Targets (next 25H, the user types `pull` on a healer)

Felesta grips within 2 s; Obliteration within 1 s of the landing, no walk; the user last. Healer 50+ yd
off the pack after link 2, 70+ after the user's. Felesta's grip never on a peel; mid-fight out and back
in ~10 s. No bot grip without a `pull` except to continue one. `toc.aura` `fire` on one paladin. 25N
still kills.

# refine-fc-3: player-started pull, instant grip chain, CC pairs, melee peel

Wave 4h, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4g `f4a819fc0`.

## Why

25H, 2026-10-18, `env/dist/logs/botobs/649_2_thrakgar_1792355947.ndjson` (wipe 1:20, no kill) and
`…_1792356238.ndjson` (wipe 1:55, Resto Druid at 55.9 s). Deathsong (DK) is the only human.

- **Tanks started the fight.** Champions go aggressive 4 s after the jump (`EVENT_CHAMPIONS_ATTACK`),
  aggro radius 23 yd (`detection_range` 20 + 3 levels, 3D); 4g staged bot tanks on the landing
  centroid. The first engage (`TYPE_FACTION_CHAMPIONS_START`) makes every idle champion attack
  `SelectNearestTarget(200)`; with nobody inside 23 yd they idle until a player engages.
- **Bot DK slow.** Human grip 1.69 s, bot grip 9.52 s: its new walk was refused (`wait`) 2.6-5.1 s
  by its stale walk's COMBAT booking, then 31 yd of walking. Pull 1: 50 yd from staging, first grip
  at 7 s, 15 yd moved. Death Grip cooldown 35 s.
- **Tanks held one re-pick.** All 10 on tanks at 0.2 s; at the 2 s recount 8 took melee bots 20-28 yd
  off (healing threat: Healing Stream, Divine Storm). Casters ignore distance inside 35 yd and favour
  health under 40k, so a tank loses to anyone with threat within ~48 (30k hp) to 57 yd (22k hp).
- **Melee champions roamed.** Malithas (Ret) killed 5 casters 54-78 yd from the tanks, Narrhok
  (Warrior) likewise; 30 snares, no root, Nova or Disarm.
- **CC nearly absent.** Only Cyclone druids are CC bots: 2 Cyclones in 80 s; 2 mages, 3 warlocks idle.

User: fix the start and the grip, and add the pasted guide's Mage+Warlock CC pairs (Polymorph ×2,
Fear ×2, repeat; Banish/Fear the Resto Druid) and melee peel (Chains of Ice, Death Grip, Hamstring,
Disarm, roots, Frost Nova), all this round.

Verified: Resto Druid champions (34459/34469, heroic 35688/35716) are Elemental, so Banish lands;
Banish is stun + school immunity (never breaks, heroic trinket breaks the first); a disarmed Warrior
champion skips Bladestorm and rechecks every 5 s; Death Grip reaches ~34 yd centre to centre.

## Scope

1. **Staging** (`LayoutChampions`): bot tanks on the ray centroid → `LOC_CENTER`, first 0.5 yd step
   with every landing slot ≥ 25 yd (~29 yd out), extra tanks ±3 yd perpendicular, still forced;
   non-tank bot DKs knowing Death Grip get duty `grip` on the ray at 50 yd (extras ±3 yd); lattice
   points must also be farther from every slot than the nearest tank stand + 3 yd.
2. **Grip chain**: the grip action clears `last movement` when replacing a walk to another spot;
   `UpdateGrip` grips from the DK's own spot when the target is in reach and LOS and the DK is ≥ 12 yd
   farther from the pack centre; for 1.5 s after live only a `pulled` target; once the kill target is
   pulled and ≤ 40% the DK pre-walks to the next healer's stand if Death Grip is ready within the walk
   + 1 s (`walkOnly`), casting at ≤ 20%.
3. **CC duty** replaces the icon system (icons, `savedRtiCc`, nodes `faction champions clear cc
   icons`, `faction champions set cc icon`, `toc restore rti cc`). CC bots: mages (Polymorph), warlocks
   (Fear; Banish on Elemental/Demon), ranged druids (Cyclone). Candidates other than kill, suspended
   and skulled: healers off the tanks nearest the kill target, other healers, champions off the tanks
   (casters first), the rest in kill order. One duty per candidate: skip if CC'd past cast time +
   0.5 s or a CC cast is in flight; caster in range and LOS whose next application lasts > 2.5 s,
   unbreakable first, then longest hold, then nearest. Polymorph and Fear only on a clear candidate
   (≥ 12 yd from pack tanks, no group attacker, no group DoT). Latched to cast end or 3 s; probe
   `fc.cc`. Node `faction champions crowd control` (`ACTION_RAID + 3.4f`); the CC guard zeroes every
   class CC node while live.
4. **Peel duty** (`fc.peel`, `faction champions peel`, `ACTION_RAID + 3.3f`): melee champions not CC'd,
   rooted or immune, with a non-tank victim within 10 yd, or the Warrior champion with ≥ 2 group units
   within 8 yd. Warrior first, then healer victims, then lowest victim health. Nearest free bot (not
   grip DK, not on CC): Disarm, Dismantle (not already disarmed), Frost Nova within 10 yd, Entangling
   Roots, Psychic Horror (no Nova or roots within 12 yd of a pack tank), Death Grip by a pack tank or
   by a non-grip DK ≥ 12 yd nearer the pack centre.
5. **Reader**: `--ball` start line (release to first engage, first picks by role), `--pack` grips
   `since prev`, `--cc` per-cast duties, `--peel` `fc.peel`, Disarm uptime, Bladestorms. Tests and docs.

## Targets (next 25H, Deathsong opens with a grip from ~30 yd)

Start only on the player's engage, first picks ≥ 80% on tanks; bot grip ≤ 1.5 s after a human grip
in its reach; kill target ≥ 45 yd off the pack within 5 s; ≥ 1 CC landed per 10 s; Warrior champion
disarmed before its first Bladestorm; no ranged or healer death in 45 s. 25N still kills.

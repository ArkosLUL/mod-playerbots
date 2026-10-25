# refine-fc-14: every bot kites, Nitro runs far, DKs grip to the wall, kiters get freed

Wave 4w, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4t `bdca025a9`, which the
pulls below ran.

## Why

2026-10-25, two 25H wipes: `env/dist/logs/botobs/649_39_caiphus-the-stern_1792935004.ndjson` (3:03) and
`649_39_caiphus-the-stern_1792935326.ndjson` (3:42), 30 deaths each.

- **Ranged kite works:** median 21-22 yd off the nearest melee champion (was 8-15); moving halves that champion's
  damage (3.7-4.5 vs 7.9-8.2k/s), in contact and unsnared too (2.9-3.6 vs 5.0-8.7k/s), so `KITE_CONTACT` is wrong.
- **Melee bots never kite:** the melee champions' main victims (~1,100 samples a pull vs 80-250 per other role),
  moving 2-7%.
- **Nitro runs 25 yd** of ~87 (median travel ~24); one melee chaser never triggers it.
- **Re-grip:** A 1:20.8 Felesta (holder) gripped Maz'dinah back toward the raid 1.1 s after the user's wall grip;
  `FactionChampionsHoldControl` grips a DK holder's champion in whenever it's on someone else.
- **Ranged deaths** (22: melee 14, casters 8): the DK champion's Chains of Ice 66020 rooted 6 kiters and none of 21
  got Hand of Freedom (which strips it: mechanic 11, purge-on-immunity); the contact gate (2); Dazed (4) and Charge
  Stun (2); AoE dodges into or with melee (2); a CC duty on a chased bot (1); burst from full (2).

## Scope (the user's calls, 2026-10-25)

1. **Kite:** every bot but tanks, holders not from their own champion; no contact gate; `reach melee`,
   `reach spell`, `reach party member to heal` and charges held while chased within 45 yd. Snares go on any kited
   chaser. No CC duty for a chased bot. AoE dodge spots 8+ yd off melee champions; a Bladestorm chasing a kiter left
   to the kite.
2. **Nitro:** also on one free melee champion within 8 yd; runs to the floor spot farthest from melee champions, up
   to the boost's ~87 yd.
3. **Wall grip:** bot DKs (holder: own champion when on someone else; others: one on a non-tank non-holder raider,
   healer victims first) walk to a stand 34+ yd off `ARENA_CENTER`, in healer reach, landing it 12+ yd farther from
   its victim; never one in the air in the last 8 s; then Chains of Ice; back to the normal job after. Hold control
   and peel lose Death Grip. Probe `fc.wall`.
4. **Hand of Freedom:** a paladin bot frees a kiter rooted or snared by a champion with a melee champion on it within
   15 yd; the class node is zeroed.
5. **Reader:** per-role chase spacing, Nitro travel and chaser after, wall grips with `regrip`, death causes, Chains
   of Ice freed. Tests, docs.
6. **Speed boost** (follow-up commit): rogues Sprint and Cat Form druids Dash with a free melee champion they kite
   from within 8 yd, under Nitro and over the kite; the class `sprint` and `dash` zeroed all fight. Ghost Wolf left
   out: no shaman has Improved Ghost Wolf. Reader: Sprint and Dash casts in `--personal` (today: 3 and 4, all on the
   opening run-in but one).

## Targets (next 25H)

Melee and healers moving well above 2-7% / 50-56% with a melee champion on them; Nitro travel 50+ yd; bot grips land
34+ yd off centre with Chains of Ice and no `regrip`; most DK champion roots freed; fewer melee-damage deaths than 18
and 13, ranged than 8 and 6; 25N still kills.

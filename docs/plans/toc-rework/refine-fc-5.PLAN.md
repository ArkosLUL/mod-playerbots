# refine-fc-5: a frontline holds the blob, the kill squad kills the pulled healer

Wave 4j, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4i `e8f3163dd`.

## Why

25H, 2026-10-19, `env/dist/logs/botobs/649_2_thrakgar_1792435005.ndjson` and `…_1792435329.ndjson`,
both wipes. The grip chain ran in order, the healer landing 49-69 yd out, but another champion healer
was always within 40 yd of it (clear 0% and 11%):

- **The blob followed the melee.** Every non-healer bot but the tanks focuses the kill target, so all
  melee ran to the pulled healer; champions re-pick every ~9 s by distance, health and armour and walk
  to their pick, so the blob's centroid drifted 42-66 yd toward it (25-40 in pull 1).
- **Tanks idle.** Pack tanks hit only champions within 12 yd and return within 8 yd of the landing
  spot; from 0:20 both stood there with no target.

Melee score: tank ~0.72, plate DPS in melee ~1.4-1.6, which beats cloth past ~23-27 yd and leather or
mail past ~13. User: tanks plus every plate melee DPS bot hold the blob where it landed, healed by all
healers but one; the rest kill the pulled healer; with the next healer still in the blob the kill squad
waits outside (melee hold, ranged from 30+ yd).

## Scope

1. **Frontline** (`FactionChampionsIsFrontline`): bot tanks plus Warrior, Paladin and DK melee DPS bots,
   minus the chain DK (lowest-guid non-tank DK bot knowing Death Grip, now public
   `FactionChampionsChainDk`). `FactionChampionsIsPackTank` stays for tank AoE rules.
2. **Assignment** per instance per tick, probe `fc.front` (member → champion): champions within 20 yd
   of the landing anchor, not pulled, touchable, not under breakable CC. Tiers: untagged (no non-tank
   frontline member has threat on it), straying (victim not frontline), rest; melee first, then nearest
   the anchor. Latched while valid and no higher tier is unheld; each candidate gets the nearest free
   member, extras take a second round. Nodes `faction champions frontline return` (past 24 yd, or 8 yd
   with no champion) and `… frontline attack`; rule `faction champions frontline hold`.
3. **Other duties**: frontline off focus and the physical switch; escapee and CC ordering read "victim is
   frontline" (`onFront`); peel skips frontline victims (Bladestorm rule stays); `ElectGripper` and the
   avoid-AoE branch skip the frontline.
4. **Healers**: all but one heal the frontline, most armour first, 28 yd off the anchor at ±60°, ±45°,
   ±75° off the line toward the pulled healer (to `ARENA_CENTER` with none), pulled in onto the floor.
5. **Kill squad wait** (`faction champions kill squad wait`, was grip wait): non-frontline melee hold
   while a grip is pending or a champion healer lives and the kill target isn't pulled; ranged slots
   then sit 30-32 yd off the pack centre.
6. **Reader** `--front`: blob off the landing spot, frontline spot and targets, champion picks,
   kill target clear of healers; summary shares. Tests and docs.

## Targets (next 25H, each healer pulled with `pull`)

Blob within 15 yd of the landing spot 80%+ of samples; 60%+ of melee champion picks on the frontline;
pulled healer clear of other champion healers 70%+; frontline within 24 yd, tanks with a target 90%+;
frontline deaths reported; `--chain` in order; 25N still kills.

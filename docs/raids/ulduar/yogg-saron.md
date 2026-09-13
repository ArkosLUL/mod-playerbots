# Yogg-Saron
## Phase 1: Sara is friendly, and only Guardians can hurt her

Sara is `FACTION_FRIENDLY` all of P1, and her `DamageTaken` zeroes anything whose attacker is not a
Guardian of Yogg-Saron (33136). The raid's job is to kill Guardians **on top of her** — their death
explosion Shadow Nova Sara (65719, 15 yd) is her only damage source.

So no bot ever holds threat on her, and `AI_VALUE2(Unit*, "find target", "sara")` walks
`GetThreatenedByMeList()`. It returned null every tick and took **all 21 Yogg nodes** with it: two
wipes on 2026-09-13, ~6,900 checks per trigger, zero fires. Resolve her with
`FindNearestCreature(NPC_SARA_PHASE_1, …)`. She lives into P2/P3 at 1 health, so "Sara is alive" is no
phase test — `YoggSaronInPhase1` also demands neither P2 nor P3.

`IsBotMainTank` is false for **every** bot while a human holds main tank, which silently disabled the
cloud-cheat and mark-target nodes. `IsDesignatedBotTank` falls back to the first living bot tank.

## The two spells that wipe P1

| Spell | Shape | Answer |
|---|---|---|
| Shadow Nova 62714 / 65209 | instant, uninterruptible, 15 yd, on Guardian **death** | ranged and healers stay out; melee and tanks must eat it |
| Dark Volley 63038 / 65330 | 1500 ms cast, 35 yd, `InterruptFlags` 0xF | interrupt it — distance is no answer |

~97% of raid damage across both wipes. 25-man **normal** casts 63038, so the Dark Volley ids do not
split 10/25; test both. The always-on class interrupts only look at the bot's current target and
caught under a third of the volleys, hence `yogg-saron dark volley`, which offsets each interrupter by
its index so the raid does not spend every cooldown on one cast.

The nova's kill zone and the raid's camp are the same place: ranged and healers sat at a median 11 yd
from Sara while every burst caught 20-25 of 25 raiders. **Do not gate the retreat on Guardian health**
— the median Guardian is below 15% for 1.05 s, about 7 yd of travel. Spacing is a standing rule.

## Ominous Clouds

Six clouds orbit Sara at fixed **11 / 21 / 31 / 41 / 51 / 61 yd** radii, constant **3.0 yd/s**. One
summons a Guardian whenever a player comes within 6 yd, which is how bots spawned ~23 Guardians against
a scripted ~11-12. `InformCloud` skips clouds closer than 20 yd to Sara, so **the innermost orbit only
ever fires from player contact** - and it sweeps straight through where melee stand.

Avoiding them starves nothing: `EVENT_SARA_P1_SUMMON` feeds Guardians every 20 s, shrinking to a 10 s
floor, wherever the raid stands.

No fixed radius is safe. Orbit gaps are ~10 yd, so the best clearance any ring holds is 5 yd against
its two neighbours, inside the 6 yd summon check. Bots sidestep as a cloud comes round.

## `yogg-saron phase 1 spacing`

Clouds and Guardians in one node: two nodes at one relevance trade ticks and walk the bot down the line
between their destinations. Three things keep it from oscillating against hazards that never stop
moving.

- **Trigger and clear radii kept apart** (cloud 10 → 14, nova 17 → 20). One radius parks the bot on the
  boundary and re-fires the moment a cloud drifts a yard in.
- **The destination is held** until the bot arrives or a cloud drifts onto it, and the in-flight tick is
  claimed by returning true without touching the motion master.
- **`preferNear` is `ULDUAR_YOGG_SARON_MIDDLE`, not the bot.** Every candidate in a ring is the same
  walk away, so a bot-position bias is a no-op tie-break; biasing at Sara makes the dodge a sidestep
  rather than a run for the rim. The `accept` cap holds it inside spell range.

`MoveAwayFromCreatureAction` was rejected for this: no throttle, no latch, and it *maximises* distance
recomputed from the bot's new position every tick, so against six rotating rings the best answer rotates
with them.

## Reduced Keepers

The raid frees fewer than 4 Keepers, losing that Keeper's support. Tuned for the hardest single-Keeper
case, **Thorim only**. The bitmask lives in `PERSISTENT_DATA_WATCHERS_MASK` — preferred over Sara's
`GetData(DATA_GET_KEEPERS_COUNT)`, which gives a count only, because reading the mask confirms
**which** Keeper.

What Thorim-only removes: no Freya means **no Sanity Wells, so Sanity (63050, 100 stacks) is a
one-way drain** — nothing restores it; no Hodir means no Protective Gaze absorb; no Mimiron means no
haste clouds and therefore a slower kill and more total drain.

Sanity drains, and whether anything can be done:

| Source | Spell | Loss | Avoidable |
|---|---|---|---|
| Psychosis | 63795 / 65301 | −9 / −12 | **No** — random target every 3.5s in P2 |
| Malady of the Mind | 63830 / 63881 | −3 | Yes |
| Brain Link | 63803 | −2 | Yes, if the pair stays within 20 yd |
| Lunatic Gaze (P2 skull) | 64168 | −2 | Yes — only players *facing* the caster |
| Lunatic Gaze (P3 Yogg) | 64164 | −4 | Yes |
| Induce Madness | 64059 | −100 | Yes — leave the brain room |

**Thorim's Titanic Storm auto-kills anything carrying `SPELL_WEAKENED` (64162)**, and a guardian
drops Empowered at ~<10% HP. So **melee only need to burn a guardian to Weakened; Thorim finishes
it.** With no Thorim, guardians are effectively unkillable without the cheat — which is why the P3
cheat instakill is suppressed **only when Thorim is a Keeper**, leaving other reduced-Keeper combos
winnable.

Crusher Tentacle (33966) is **stationary** and nobody tanks it — with no one in melee range its Crush
cannot land, so ranged nuke it in place. Its Diminish Power (64145) is a raid-wide DPS debuff, so it
must die fast.

Open risks: the removed P3 cheat existed because Lunatic Gaze "freezes" bots, so guardian DPS may
stall between gazes; a guardian spawns up to 48 yd out, so confirm taunt pickup is prompt; and the
sanity-conservation behaviour (stand behind Yogg facing away below 15 stacks) **nearly benches a bot**
— sanity never recovers Thorim-only, so a bot that drops to 15 stays there.

## Squeeze breaks on immunity

Removing the Squeeze aura (64125 / 64126) kills the Constrictor Tentacle and drops the passenger, so
`yogg-saron squeeze escape` at `ACTION_RAID + 1` has a grabbed mage cast Ice Block and a paladin cast
Divine Shield. Hunter Feign Death and rogue Vanish are deliberately not used — neither removes a
periodic damage aura.

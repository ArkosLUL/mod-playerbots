# refine-fc-6: whole grips, a frontline that follows the pack, steady stands, interrupts, CC, purges

Wave 4k, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4j `46a6a7131`.

## Why

25H, 2026-10-19, `env/dist/logs/botobs/649_2_thrakgar_1792440191.ndjson` (wipe 1:12) and
`…_1792440423.ndjson` (wipe 1:49, Caiphus held 104 s, never died):

- **Grips cut.** Core: `Spell::CheckCast` fails JUMP_DEST on a rooted caster, so 57604 never starts on a
  rooted champion; a root or stun applied mid-flight runs `SendMoveRoot` → `StopMoving` and ends the jump;
  a stun already on doesn't block it. Link 2 carried 3-4 yd both pulls: a hunter pet's Charge (61685 →
  root 53148) 11 ms after the grip, or already on. The player's link 3 in pull 1: a bot's generic Frost
  Nova as the healer flew past. Also HoJ on snare target, Intimidation 19577 → 24394.
- **Frontline died, tanks idled.** Justice and Mighty died 0:24-0:27 (four melee champions on them,
  103-112k taken vs 73-83k healed, externals spent at 0:05-0:12). The pack followed its picks 32-40 yd
  off the landing spot; candidates only within 20 yd of it, so members held nothing and stood there.
- **Kill squad deadlock.** Pack drifted onto the pulled healer, `pulled` turned false, the wait rule held
  melee and stands flipped to the pack rings for the rest of the fight.
- **Ranged oscillation.** Path 163-374 yd for net 7-72 in 55 s, 29-56 stand moves each: slots rebuilt and
  dealt by index every refresh, so any change reshuffled everyone 20-45 yd.
- **Interrupts.** Tranquility stopped every time; Hellfire never (negative channel, only bots targeting
  the warlock kick it); 9 of 26 cast-time heals followed by an interrupt; Counterspell duty 0 times.
- **CC.** Felesta's Pestilence and Death and Decay diseased almost every pack champion, so Fear and
  Polymorph had no candidates (pull 1: 0 Fears, 1 Polymorph); the CC bots stood 40-58 yd from the champion
  healers (no duty at all 15-36 s in pull 2); healers off the kill target controlled 9-31%. The Resto Druid
  is an Elemental (`creature_template` type 4): Banish lands, Polymorph and Hex don't.
- **Externals.** Hand of Protection went to melee DPS at 7-36 s; Prayer took 66k from melee with none left.
- **Purges.** Only the kill target is purged; champion Bloodlust and Avenging Wrath never were.
- **Dispels** fine (CC off in 0.1-1.2 s, DoTs and curses in ~0.1-2 s, UA left on purpose): no change.

User's calls: frontline is plate plus mail melee (adds Enhancement Shaman); cast-time spells kicked in
their last moments, channels at once; Nitro Boosts to get into position and when chased; pet stuns kept
outside grips.

## Scope

1. **Grip guard.** Guarded champion: the open chain's target until 6 s after the bot DK's grip, a grip
   duty's target while pending, any champion mid-jump. Multiplier zeroes spells that root, stun, fear,
   confuse, charge or knock back (one trigger or linked level down) at a guarded champion, and area or
   self ones while any guard is active. Pet autocasts that hold are off during a guard, back on after.
2. **Frontline** adds melee shamans; the pack centre follows the pack (mean of champions within 20 yd of
   the last centre); frontline candidates, return and escapee read the centre, not the landing spot.
3. **Externals.** HoP only for healers and ranged, on one physical champion on them; Pain Suppression,
   Guardian Spirit, Hand of Sacrifice need 2+ and health ≤ 60%.
4. **Kill squad** commits once the kill target has been pulled out (`FactionChampionsKillTargetOut`).
5. **Stands** latched: kept while the layout hasn't moved 6+ yd, mode is unchanged and the slot stays on
   the floor out of hazards; free members take the nearest free slot.
6. **Interrupts.** One bot interrupts each Hellfire at once (`fc.kick`, ranged first). Interrupts and
   every `<spell> on enemy healer` action wait for a champion's cast-time spell's last 600 ms; channels
   at once; the due Counterspell mage starts no cast-time spell meanwhile.
7. **Nitro Boosts** before FC walks of 15+ yd; a ranged bot or healer a melee champion is on within 8 yd
   boosts 25 yd away (`faction champions chased`).
8. **CC.** No raid splash on champions (`SpellSplashes`); Polymorph and Fear lose the pack tank
   clearance; Hex for DPS shamans; a duty reaches casters up to 15 yd out of range, which walk in; the
   next CC on a healer prefers another DR group.
9. **Purge.** A second purger takes Bloodlust, Heroism, Avenging Wrath, Hand of Freedom off pack
   champions, melee on the frontline first.
10. **Reader**: `--chain` cut by / no pull; `--stands` (moves, slot jumps, path vs net, Nitro); `--heals`
    channels and interrupt timing; `--front` deaths; `--purge` pack buffs; `--aoe` own splash. Tests, docs.

## Targets (next 25H, each healer pulled with `pull`)

No grip cut by a bot or pet, link 2 carries 15+ yd; frontline armed 90%+ with a champion in the pack,
first frontline DPS death after 0:40; ranged path within 3× net; Hellfire stopped within 1.5 s, heal
kicks in the last 0.6 s; no 100 s kill target holds; champion healers off the kill target controlled
50%+, no 10 s CC gap; HoP only on healers and ranged; no raid splash on champions; champion Bloodlust and
Avenging Wrath purged within 8 s; 25N still kills.

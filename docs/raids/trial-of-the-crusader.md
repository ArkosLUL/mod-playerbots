# Trial of the Crusader (map 649)

Cross-raid conventions are in [README.md](README.md).

## The key discovery

**`wotlk-toc` is Trial of the *Champion*** — the 5-man dungeon, map 650 — **not the raid.** There
was no handler for map 649 anywhere. The raid strategy uses the key **`trialofthecrusader`**.

## Encounter mechanics

Source of truth: `src/server/scripts/Northrend/CrusadersColiseum/TrialOfTheCrusader/`.

1. **Northrend Beasts** — one continuous fight, three stages. *Gormok*: avoid Staggering Stomp zones,
   Fire Bombs leave ground hazards, spread. *Acidmaw & Dreadscale*: submerge/emerge form swap, avoid
   slime pools and churning ground, focus the mobile worm, spread for sprays. *Icehowl*: recognise
   the jump-to-centre charge phase and intercept or dodge it — a wall crash stuns the boss and opens
   a free DPS window — then move out of Whirl and Arctic Breath.
2. **Lord Jaraxxus** — interrupt/dispel Incinerate Flesh and Touch of Jaraxxus, spread for Legion
   Flame, kill Mistress of Pain adds, avoid Infernal Volcano AoE.
3. **Faction Champions** — PvP-style NPCs. Focus-fire kill order (healers → casters → melee),
   interrupt heals, avoid clumping.
4. **Twin Val'kyr** — shared health pool. Essence-colour assignment, collect matching-colour orbs,
   keep opposite-colour immunity against Light/Dark Touch, spread for Vortex.
5. **Anub'arak** — P1 manage Scarabs and Burrowers and spread for Penetrating Cold; submerge phase
   kite Pursuing Spikes into Frost Spheres; P3 burst through the permanent Leeching Swarm.

**Bot-tractable** (avoidance, positioning, kill order, interrupts): Beasts, Jaraxxus, Faction
Champions, Anub'arak P1/P3. **Harder, needs new state tracking**: the Val'kyr essence system and
Anub'arak spike-kiting.

## Contributed to shared code

`TrialOfTheCrusaderHelpers::IsBotInFrontalCone` was promoted into the shared `RaidBossHelpers` during
the Koralon work — see [vault-of-archavon.md](vault-of-archavon.md).

`AnubarakDelayBloodlustUntilLeechingSwarmMultiplier` is one of the two pre-existing per-boss lust
gates that the shared burst gate had to compose with rather than replace.

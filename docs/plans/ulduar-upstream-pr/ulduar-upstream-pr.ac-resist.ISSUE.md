# Draft: AzerothCore issue (Game issues template)

Repo: azerothcore/azerothcore-wotlk. Each section below is one field of the form. Not filed yet.

## Title

[Ulduar] Cosmic Smash, Water Spray, Dark Volley and Malady of the Mind can't be partially resisted

## Current Behaviour

These four Ulduar spells never take partial resists. Each one pairs its damage with a knockback or a debuff, so `SpellMgr::LoadSpellInfoCustomAttributes` flags it as a binary spell and `CalcAbsorbResist` skips the partial resist bands. Resistance gear and auras do nothing against them.

- Cosmic Smash (Algalon): 62311, 64596, fire
- Water Spray (Mimiron's Emergency Fire Bot): 64619, frost
- Dark Volley (Yogg-Saron): 63038, shadow
- Malady of the Mind (Yogg-Saron): 63830, shadow

Water Spray and Malady of the Mind have no damage class either, so they never get the full resist roll that binary spells get instead. For those two the flag only takes resistance away.

## Expected Behaviour

They take partial resists in 10% steps like other magic damage, same as the Thorim and Hodir spells that got `SPELL_ATTR0_CU_IGNORE_BINARY` in #27927.

## Source

WoW Web Stats parses of retail pulls from 2009. The partial resists show when you hover a target's Avoid column.

- Malady of the Mind, Aug 2009: http://files.shefki.org/sws/sws-yoggsaron-1249958799/spell_63830.html. Its damage is a flat 5000, and the log has hits of 4500, 4000 and 3500 with nothing fully resisted.
- Dark Volley, same pull: http://files.shefki.org/sws/sws-yoggsaron-1249958799/spell_63038.html
- Water Spray, 25 man hard mode, Aug 2009: http://wws.piechart-guild.com/col25/sws-mimiron-1251329839/spell_64619.html. Every resisted amount is exactly 10, 20 or 30% of a roll between 18850 and 21150, and nobody fully resisted it.
- Cosmic Smash, 10 man, May 2009: http://wws.piechart-guild.com/uld10/sws-algalon-1243304605/spell_62311.html. Every hit on every target was partly resisted. 64596 is the 25 man version with the same effects. The 25 man logs I found have no full resists either, those raids just didn't run fire resistance.

## Steps to reproduce the problem

1. `.spellinfo attributes 63830` lists `SPELL_ATTR0_CU_BINARY_SPELL`. Same for 62311, 64596, 64619 and 63038.
2. Get some shadow resistance and fight Yogg-Saron. Every Malady of the Mind hit lands for the full 5000.
3. Same with frost resistance against Water Spray in Mimiron's hard mode: no hit is ever resisted.

## Extra Notes

The fix is a few `spell_custom_attr` rows like in #27927. I run these on my server:

```sql
UPDATE `spell_custom_attr` SET `attributes` = `attributes` | 512 WHERE `spell_id` = 64619; -- Water Spray (Mimiron's Emergency Fire Bot)
DELETE FROM `spell_custom_attr` WHERE `spell_id` IN (62311, 63038, 63830, 64596);
INSERT INTO `spell_custom_attr` (`spell_id`, `attributes`) VALUES
(62311, 512), -- Cosmic Smash (Algalon)
(63038, 512), -- Dark Volley (Yogg-Saron)
(63830, 512), -- Malady of the Mind (Yogg-Saron)
(64596, 512); -- Cosmic Smash (Algalon)
```

## AC rev. hash/commit

```
AzerothCore rev. 62fb09c68d5e+ 2026-10-17 14:29:37 +0300 (Custom branch) (Unix, RelWithDebInfo, Static) (worldserver-daemon)
OpenSSL 3.0.13, Boost 1.83.0, MySQL 8.4.8, ACDB 335.17-dev
```

## Operating system

Ubuntu 24.04 in Docker, on Windows 11 x64

## Custom changes or Modules

mod-playerbots on its azerothcore-wotlk fork, plus my own changes. That includes the SQL above, so my server doesn't show the bug anymore. The binary spell code in `SpellMgr.cpp` is the same as on master.

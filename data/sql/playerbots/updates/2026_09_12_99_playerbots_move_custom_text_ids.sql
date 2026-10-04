-- Upstream numbers new ai_playerbot_texts rows straight up from its last id, so fork-only texts
-- sitting in that range break the next upstream insert with a duplicate key. Fork-only texts go to
-- 30001 and up (id is a signed smallint), and the autogear texts go back to the ids upstream gave
-- them (1901-1912). Texts load by name, so the id itself means nothing. Runs before upstream's
-- 2026_09_13_00 file.

UPDATE ai_playerbot_texts SET id = 30001 WHERE name = 'kalecgos_tank_sent_to_spectral_realm';
UPDATE ai_playerbot_texts SET id = 30002 WHERE name = 'kalecgos_tank_should_enter_spectral_realm';
UPDATE ai_playerbot_texts SET id = 30003 WHERE name = 'kiljaeden_designated_dragon_orb_user';
UPDATE ai_playerbot_texts SET id = 30004 WHERE name = 'kiljaeden_no_designated_dragon_orb_user';

-- Ascending, so each target id is already free when its row moves.
UPDATE ai_playerbot_texts SET id = 1901 WHERE name = 'autogear_command_disabled_error';
UPDATE ai_playerbot_texts SET id = 1902 WHERE name = 'autogear_altbot_refused_error';
UPDATE ai_playerbot_texts SET id = 1903 WHERE name = 'autogear_no_master_error';
UPDATE ai_playerbot_texts SET id = 1904 WHERE name = 'autogear_master_no_gear_error';
UPDATE ai_playerbot_texts SET id = 1905 WHERE name = 'autogear_quality_capped_msg';
UPDATE ai_playerbot_texts SET id = 1906 WHERE name = 'autogear_ilvl_capped_msg';
UPDATE ai_playerbot_texts SET id = 1907 WHERE name = 'autogear_invalid_arg_error';
UPDATE ai_playerbot_texts SET id = 1908 WHERE name = 'autogear_quality_as_number_error';
UPDATE ai_playerbot_texts SET id = 1909 WHERE name = 'autogear_start_msg';
UPDATE ai_playerbot_texts SET id = 1910 WHERE name = 'autogear_start_any_ilvl_msg';
UPDATE ai_playerbot_texts SET id = 1911 WHERE name = 'autogear_reset_start_msg';
UPDATE ai_playerbot_texts SET id = 1912 WHERE name = 'autogear_reset_start_any_ilvl_msg';

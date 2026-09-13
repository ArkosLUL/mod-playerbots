-- Aq40UseResistanceBuffsAction added "+rnature" and only removed it again while the aq40 instance
-- strategy was loaded, so every hunter that fought Viscidus or Princess Huhuran and left kept it.
-- Saved strategies outlive the session, so the leak survived logouts and restarts.
--
-- Restore "+bdps" rather than just dropping "+rnature": the two are siblings, so adding one evicted
-- the other, and PlayerbotRepository::Load clears the engine and replays the stored string, which
-- means the AiFactory default never comes back. A hunter left with neither has no aspect node in
-- combat at all. Siblings cannot coexist in one engine, so no stored value holds both.
UPDATE `playerbots_db_store`
SET `value` = TRIM(BOTH ',' FROM REPLACE(CONCAT(',', `value`, ','), ',+rnature,', ',+bdps,'))
WHERE `key` IN ('co', 'nc', 'dead')
  AND CONCAT(',', `value`, ',') LIKE '%,+rnature,%';

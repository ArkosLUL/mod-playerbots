Bots show "ssc" strategy instead of "tempestkeep" on Void Reaver
Context
While fighting Void Reaver in The Eye (Tempest Keep, map 550), most bots had the ssc (Serpentshrine Cavern) combat strategy instead of tempestkeep. Because Void Reaver's behavior (spread on arcane orb, aggro dump on knock-away, tank positioning) lives entirely inside the tempestkeep strategy, bots with ssc never fired any Void Reaver triggers.

Root cause
Instance combat strategies are chosen purely by map id in PlayerbotAI::ApplyInstanceStrategies(mapId) — PlayerbotAI.cpp:1623: it strips every instance strategy in allInstanceStrategies, then adds the one for the current map (548→ssc, 550→tempestkeep). That switch is correct.

The problem is persistence clobbering it. Bot strategies are saved to / loaded from the playerbots_db_store table:

PlayerbotRepository::Save — PlayerbotRepository.cpp:65 — persists the entire combat strategy list, including whatever instance strategy is active (ssc, tempestkeep, …).
PlayerbotRepository::Load — PlayerbotRepository.cpp:26-31 — does ClearStrategies(BOT_STATE_COMBAT) (wipes all strategies, incl. the map-derived one) then re-applies the saved string verbatim.
On the bot login path — PlayerbotMgr.cpp:527-536:

ResetStrategies();                              // ApplyInstanceStrategies(550) → adds "tempestkeep"
PlayerbotRepository::instance().Load(botAI);    // ClearStrategies wipes it, re-applies stale "+...,+ssc"
Load runs after the map-based apply and overwrites it. No ApplyInstanceStrategies runs afterward on this path, so a bot last saved in SSC (map 548) logs into The Eye already carrying ssc and keeps it for the whole run. This matches "most of the bots": the ones previously grouped/saved in a Serpentshrine session. (The far-teleport ACK path PlayerbotAI.cpp:797 does re-apply, which is why bots that zoned in via portal after login were correct — hence only most, not all.)

Instance strategies are location-derived and must never be restored from persisted per-bot data.

Fix (recommended)
Make the current map authoritative for instance strategies, in two parts:

Re-apply after Load. In PlayerbotRepository::Load (PlayerbotRepository.cpp:9), after the saved strategies are applied, call botAI->ApplyInstanceStrategies(botAI->GetBot()->GetMapId()) (gated by sPlayerbotAIConfig.applyInstanceStrategies, as every other call site is). This strips any stale instance strategy and adds the correct one for the bot's current map. Fixes existing bad DB rows too, and covers every Load caller, not just login.

Stop persisting instance strategies. In PlayerbotRepository::Save (PlayerbotRepository.cpp:65-67), filter instance-strategy names out of the "co"/"nc" lists before writing, so they never leak into the DB again. This needs the allInstanceStrategies list, currently a local static inside ApplyInstanceStrategies — promote it to a shared accessor (e.g. a static method / free function returning the list, or a PlayerbotAI::IsInstanceStrategy(name) helper) reused by both Save and ApplyInstanceStrategies.

Part 1 alone fully fixes the reported bug; part 2 is hardening that keeps the DB clean and prevents any future re-introduction.

Files touched
src/Db/PlayerbotRepository.cpp — re-apply in Load; filter in Save.
src/Bot/PlayerbotAI.cpp / PlayerbotAI.h — expose the instance-strategy name set / IsInstanceStrategy helper (extract from the local static list at PlayerbotAI.cpp:1625-1633).
Verification
Reproduce: with a bot whose playerbots_db_store co row contains +ssc, log it in while on map 550 (The Eye). Before the fix it shows ssc; after, it shows tempestkeep. Inspect via the in-game strategy list command (whisper co / the debug strategy print) or log GetStrategies(BOT_STATE_COMBAT).
Cross-check the SSC direction still works: on map 548 the bot must show ssc, not a stale tempestkeep.
In The Eye, pull Void Reaver and confirm bots now spread on arcane orb and tanks position the boss (the tempestkeep Void Reaver triggers fire).
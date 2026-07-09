/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "PlayerbotRepository.h"
#include "AiObjectContext.h"
#include "PlayerbotAIConfig.h"

#include <algorithm>

void PlayerbotRepository::Load(PlayerbotAI* botAI)
{
    ObjectGuid::LowType guid = botAI->GetBot()->GetGUID().GetCounter();

    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_DB_STORE);
    stmt->SetData(0, guid);
    if (PreparedQueryResult result = PlayerbotsDatabase.Query(stmt))
    {
        std::vector<std::string> values;
        do
        {
            Field* fields = result->Fetch();
            std::string const key = fields[0].Get<std::string>();
            std::string const value = fields[1].Get<std::string>();

            if (key == "value")
                values.push_back(value);
            else if (key == "co")
            {
                botAI->ClearStrategies(BOT_STATE_COMBAT);
                botAI->ChangeStrategy("+chat", BOT_STATE_COMBAT);
                botAI->ChangeStrategy(value, BOT_STATE_COMBAT);
            }
            else if (key == "nc")
            {
                botAI->ClearStrategies(BOT_STATE_NON_COMBAT);
                botAI->ChangeStrategy("+chat", BOT_STATE_NON_COMBAT);
                botAI->ChangeStrategy(value, BOT_STATE_NON_COMBAT);
            }
            else if (key == "dead")
                botAI->ChangeStrategy(value, BOT_STATE_DEAD);
        } while (result->NextRow());

        botAI->GetAiObjectContext()->GetUntypedValue("outfit list");

        botAI->GetAiObjectContext()->Load(values);
    }

    // The saved combat strategy string can carry a stale instance strategy (e.g. a bot last saved
    // in Serpentshrine Cavern keeps "ssc" when logging into The Eye). Re-derive the instance
    // strategy from the bot's current map so it always matches where the bot actually is.
    if (sPlayerbotAIConfig.applyInstanceStrategies)
        botAI->ApplyInstanceStrategies(botAI->GetBot()->GetMapId());
}

void PlayerbotRepository::Save(PlayerbotAI* botAI)
{
    ObjectGuid::LowType guid = botAI->GetBot()->GetGUID().GetCounter();

    Reset(botAI);

    PlayerbotsDatabasePreparedStatement* deleteStatement =
        PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_DEL_DB_STORE);
    deleteStatement->SetData(0, guid);
    PlayerbotsDatabase.Execute(deleteStatement);

    std::vector<std::string> data = botAI->GetAiObjectContext()->Save();
    for (std::vector<std::string>::iterator i = data.begin(); i != data.end(); ++i)
    {
        SaveValue(guid, "value", *i);
    }

    // Instance strategies are map-derived at login (see PlayerbotAI::ApplyInstanceStrategies), so
    // they must never be persisted — otherwise a stale one leaks back in on Load. Filter them out.
    SaveValue(guid, "co", FormatStrategies("co", FilterInstanceStrategies(botAI->GetStrategies(BOT_STATE_COMBAT))));
    SaveValue(guid, "nc", FormatStrategies("nc", FilterInstanceStrategies(botAI->GetStrategies(BOT_STATE_NON_COMBAT))));
    SaveValue(guid, "dead", FormatStrategies("dead", FilterInstanceStrategies(botAI->GetStrategies(BOT_STATE_DEAD))));
}

std::vector<std::string> PlayerbotRepository::FilterInstanceStrategies(std::vector<std::string> strategies)
{
    strategies.erase(
        std::remove_if(strategies.begin(), strategies.end(),
            [](std::string const& name) { return PlayerbotAI::IsInstanceStrategy(name); }),
        strategies.end());
    return strategies;
}

std::string const PlayerbotRepository::FormatStrategies(std::string const /*type*/, std::vector<std::string> strategies)
{
    std::ostringstream out;
    for (std::vector<std::string>::iterator i = strategies.begin(); i != strategies.end(); ++i)
        out << "+" << (*i).c_str() << ",";

    std::string const res = out.str();
    return res.substr(0, res.size() - 1);
}

void PlayerbotRepository::Reset(PlayerbotAI* botAI)
{
    ObjectGuid::LowType guid = botAI->GetBot()->GetGUID().GetCounter();

    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_DEL_DB_STORE);
    stmt->SetData(0, guid);
    PlayerbotsDatabase.Execute(stmt);
}

void PlayerbotRepository::SaveValue(uint32 guid, std::string const key, std::string const value)
{
    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_INS_DB_STORE);
    stmt->SetData(0, guid);
    stmt->SetData(1, key);
    stmt->SetData(2, value);
    PlayerbotsDatabase.Execute(stmt);
}

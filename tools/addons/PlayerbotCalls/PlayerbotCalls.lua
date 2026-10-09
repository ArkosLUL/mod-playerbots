-- Server side: src/Ai/Raid/RaidHumanTankCall.cpp. It whispers "PBCALL" addon messages,
-- "DEF<tab>spellId<tab>hitInMs<tab>reason", plus a raid boss whisper as a fallback for clients
-- without this add-on, which this add-on hides.

local PREFIX = "PBCALL"

local function IsGroupMember(name)
    if not name then
        return false
    end

    local raidSize = GetNumRaidMembers()
    if raidSize > 0 then
        for i = 1, raidSize do
            if UnitName("raid" .. i) == name then
                return true
            end
        end
        return false
    end

    for i = 1, GetNumPartyMembers() do
        if UnitName("party" .. i) == name then
            return true
        end
    end
    return false
end

-- Same switches DBM's own special warnings check.
local function ShowDefensive(spellId, hitInMs, why)
    local name, _, icon = GetSpellInfo(spellId)
    if not name then
        return
    end

    local options = DBM.Options
    if not options.DontShowSpecialWarningText then
        local iconCode = options.SpecialWarningIcon and (" |T%s:12:12|t "):format(icon) or ""
        DBM:AddSpecialWarning(iconCode .. name .. " now: " .. why .. iconCode)
    end

    if not options.DontShowSpecialWarningFlash and options.SpecialWarningFlash1 then
        local color = options.SpecialWarningFlashCol1
        DBM.Flash:Show(color[1], color[2], color[3], options.SpecialWarningFlashDura1,
            options.SpecialWarningFlashAlph1, (options.SpecialWarningFlashCount1 or 1) - 1)
    end

    if not options.DontPlaySpecialWarningSound then
        DBM:PlaySpecialWarningSound(1)
    end

    if hitInMs >= 1000 then
        DBT:CreateBar(hitInMs / 1000, why, icon)
    end
end

local frame = CreateFrame("Frame")
frame:RegisterEvent("CHAT_MSG_ADDON")
frame:SetScript("OnEvent", function(_, _, prefix, message, channel, sender)
    if prefix ~= PREFIX or channel ~= "WHISPER" or not IsGroupMember(sender) then
        return
    end

    local kind, spellId, hitInMs, why = strsplit("\t", message, 4)
    if kind == "DEF" and tonumber(spellId) then
        ShowDefensive(tonumber(spellId), tonumber(hitInMs) or 0, why or "")
    end
end)

-- Real bosses are never in the group, so a boss whisper from a group member is always the fallback copy.
ChatFrame_AddMessageEventFilter("CHAT_MSG_RAID_BOSS_WHISPER", function(_, _, _, sender)
    return IsGroupMember(sender)
end)

local bossEmoteOnEvent = RaidBossEmoteFrame:GetScript("OnEvent")
RaidBossEmoteFrame:SetScript("OnEvent", function(self, event, message, sender, ...)
    if event == "CHAT_MSG_RAID_BOSS_WHISPER" and IsGroupMember(sender) then
        return
    end
    return bossEmoteOnEvent(self, event, message, sender, ...)
end)

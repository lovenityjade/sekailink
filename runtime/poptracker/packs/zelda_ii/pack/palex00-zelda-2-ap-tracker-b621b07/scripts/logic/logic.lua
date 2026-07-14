function candle()
    if has("Candle") or Tracker:FindObjectForCode("opt_candle").CurrentStage == 0 then
        return AccessibilityLevel.Normal
    else
        return AccessibilityLevel.SequenceBreak
    end
end

function cross()
    if has("Cross") or Tracker:FindObjectForCode("opt_cross").CurrentStage == 0 then
        return AccessibilityLevel.Normal
    else
        return AccessibilityLevel.SequenceBreak
    end
end

function high()
    return has("jumpspell") or has("fairyspell")
end

function boulder()
    return has("hammer") or Tracker:FindObjectForCode("opt_boulder").CurrentStage == 1
end

function better_boots()
    return has("boots") and Tracker:FindObjectForCode("opt_boots").CurrentStage == 1
end

function crystals()
    return true
end

function key(palace, ool, il)
    local ool_n = tonumber(ool)
    local il_n = tonumber(il)
    local count = Tracker:ProviderCountForCode(palace.."palacekey")
    
    if has("magicalkey") then
        return AccessibilityLevel.Normal
    elseif count >= il_n then
        return AccessibilityLevel.Normal
    elseif count >= ool_n then
        return AccessibilityLevel.SequenceBreak
    elseif has("fairyspell") then
        return AccessibilityLevel.SequenceBreak        
    end
end

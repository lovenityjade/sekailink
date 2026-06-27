function GtCrystalCount()
    local reqCount = Tracker:ProviderCountForCode("open_tower")
    local count = Tracker:ProviderCountForCode("allcrystals")

    if count >= reqCount then
        return 1
    else
        return 0
    end
end

function GanonCrystalCount()
    local reqCount = Tracker:ProviderCountForCode("ganon_vulnerable")
    local count = Tracker:ProviderCountForCode("allcrystals")

    if count >= reqCount then
        return 1
    else
        return 0
    end
end

function MotherBrainBossesCount()
    local reqCount = Tracker:ProviderCountForCode("open_tourian")
    local count = Tracker:ProviderCountForCode("g4")

    if count >= reqCount then
        return 1
    else
        return 0
    end
end

function MotherBrainAmmo()
    local missiles = Tracker:ProviderCountForCode("missile")
    local supers = Tracker:ProviderCountForCode("super")

    -- 40 for phase 1, 180 for phase 2
    if (missiles * 3 + supers) >= 220 then
        return 1
    else
        return 0
    end
end

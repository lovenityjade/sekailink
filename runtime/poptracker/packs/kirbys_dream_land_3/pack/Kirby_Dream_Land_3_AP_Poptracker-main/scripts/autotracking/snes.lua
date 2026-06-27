-- this is an example file for SNES memory autotracking 
-- more info: https://github.com/black-sliver/PopTracker/blob/master/doc/AUTOTRACKING.md#memory-interface-usb2snes
BASEADDR = 0xE00000
U16 = 2
U8 = 1

HALKEN = BASEADDR + 0x80F0
UNLOCKED_WORLD = BASEADDR + 0x53CB
LEVELS = BASEADDR + 0x9020
CURRENT_SAVE = BASEADDR + 0x3617

worlds = []

function update_available_worlds(segment)
    local halken = segment:ReadUInt32(HALKEN)
    if halken ~= 0x1802264936 then
        return
    end
    -- this only checks "halk", but it's better than nothing?
    for i=0,4 do
        for j = 0,6 do
            worlds[i,j] = segment:ReadUInt16(LEVELS+ (i*7)+j)
        end
    end
    local unlocked_world = segment:ReadUInt16(UNLOCKED_WORLD)
    local unlocked_level = segment:ReadUInt16(UNLOCKED_WORLD + 2)
    
    if AUTOTRACKER_ENABLE_DEBUG_LOGGING_SNES then
        print(string.format("update_example: readResult: %x, readResult2: %x", unlocked_world, unlocked_level))
    end
end

ScriptHost:AddMemoryWatch('WorldUnlock', UNLOCKED_WORLD, U16 * 2, update_available_worlds)

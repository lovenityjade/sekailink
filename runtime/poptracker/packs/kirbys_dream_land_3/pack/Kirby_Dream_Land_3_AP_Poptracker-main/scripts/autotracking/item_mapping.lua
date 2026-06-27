-- use this file to map the AP item ids to your items
-- first value is the code of the target item and the second is the item type (currently only "toggle", "progressive" and "consumable" but feel free to expand for your needs!)
-- here are the SM items as an example: https://github.com/Cyb3RGER/sm_ap_tracker/blob/main/scripts/autotracking/item_mapping.lua
ITEM_MAPPING = {
    [0x770001] = {"Burning", "toggle"},
    [0x770002] = {"Stone", "toggle"},
    [0x770003] = {"Ice", "toggle"},
    [0x770004] = {"Needle", "toggle"},
    [0x770005] = {"Clean", "toggle"},
    [0x770006] = {"Parasol", "toggle"},
    [0x770007] = {"Spark", "toggle"},
    [0x770008] = {"Cutter", "toggle"},
    [0x770010] = {"Rick", "toggle"},
    [0x770011] = {"Kine", "toggle"},
    [0x770012] = {"Coo", "toggle"},
    [0x770013] = {"Nago", "toggle"},
    [0x770014] = {"ChuChu", "toggle"},
    [0x770015] = {"Pitch", "toggle"},
    [0x770020] = {"HeartStar", "consumable"},
}
from random import choices
from typing import NamedTuple, Optional
from BaseClasses import Item, ItemClassification, MultiWorld
from .names import ItemName, Chati

class ChatipelagoItem(Item):
    game: str = Chati.name

class ChatipelagoItemData(NamedTuple):
    code: Optional[int] = None
    classification: ItemClassification = ItemClassification.progression

item_table = [
    ItemName.ItemNum0,
    ItemName.ItemNum1,
    ItemName.ItemNum2,
    ItemName.ItemNum3,
    ItemName.ItemNum4,
    ItemName.ItemNum5,
    ItemName.ItemNum6,
    ItemName.ItemNum7,
    ItemName.ItemNum8,
    ItemName.ItemNum9,
    ItemName.ItemNum10,
    ItemName.ItemNum11,
    ItemName.ItemNum12,
    ItemName.ItemNum13,
    ItemName.ItemNum14,
    ItemName.ItemNum15,
    ItemName.ItemNum16,
    ItemName.ItemNum17,
    ItemName.ItemNum18,
    ItemName.ItemNum19,
    ItemName.ItemNum20,
    ItemName.ItemNum21,
    ItemName.ItemNum22,
    ItemName.ItemNum23,
    ItemName.ItemNum24,
    ItemName.ItemNum25,
    ItemName.ItemNum26,
    ItemName.ItemNum27,
    ItemName.ItemNum28,
    ItemName.ItemNum29,
    ItemName.ItemNum30,
    ItemName.ItemNum31,
    ItemName.ItemNum32,
    ItemName.ItemNum33,
    ItemName.ItemNum34,
    ItemName.ItemNum35,
    ItemName.ItemNum36,
    ItemName.ItemNum37,
    ItemName.ItemNum38,
    ItemName.ItemNum39,
    ItemName.ItemNum40,
    ItemName.ItemNum41,
    ItemName.ItemNum42,
    ItemName.ItemNum43,
    ItemName.ItemNum44,
    ItemName.ItemNum45,
    ItemName.ItemNum46,
    ItemName.ItemNum47,
    ItemName.ItemNum48,
    ItemName.ItemNum49,
    ItemName.ItemNum50,
    ItemName.ItemNum51,
    ItemName.ItemNum52,
    ItemName.ItemNum53,
    ItemName.ItemNum54,
    ItemName.ItemNum55,
    ItemName.ItemNum56,
    ItemName.ItemNum57,
    ItemName.ItemNum58,
    ItemName.ItemNum59,
    ItemName.ItemNum60,
    ItemName.ItemNum61,
]
trap_item_table = [
    ItemName.ItemNum197,
    ItemName.ItemNum198,
    ItemName.ItemNum199, 
]
# stuff that can be duplicated to fill in extras
filler_table = [
    ItemName.ItemNum200,
    ItemName.ItemNum201,
    ItemName.ItemNum202,
]
# progression
prog_item_table = [
    ItemName.ItemNum300,
    ItemName.ItemNum301,
    ItemName.ItemNum302,
]

item_data_table: dict[str, ChatipelagoItemData] = {}
random_item_table = choices(item_table,k=25)
count = 0
for item in random_item_table:
    item_data_table[item] = ChatipelagoItemData(
        code=10490+count
    )
    count+=1
count = 0
for item in filler_table:
    item_data_table[item] = ChatipelagoItemData(
        code=12490+count
    )
    count+=1
count = 0
for item in trap_item_table:
    item_data_table[item] = ChatipelagoItemData(
        code=13490+count
    )
    count+=1
count = 0
for item in prog_item_table:  #Progressive Items have a code starting with 12
    item_data_table[item] = ChatipelagoItemData(
        code=11490+count
    )
    count+=1

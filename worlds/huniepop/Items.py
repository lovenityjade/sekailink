from BaseClasses import Item


class HPItem(Item):
    game = "Hunie Pop"

girl_gift = {
    "tiffany": ("academy", "rave", "scuba"),
    "aiko": ("toys", "artist", "garden"),
    "kyanna": ("fitness", "yoga", "dancer"),
    "audrey": ("rave", "toys", "aquarium"),
    "lola": ("sports", "baking", "scuba"),
    "nikki": ("artist", "academy", "aquarium"),
    "jessie": ("baking", "fitness", "dancer"),
    "beli": ("yoga", "sports", "garden"),
    "kyu": ("dancer", "rave", "artist"),
    "momo": ("aquarium", "toys", "sports"),
    "celeste": ("scuba", "academy", "fitness"),
    "venus": ("garden", "baking", "yoga")
}


panties_item_table = {
    "tiffany's panties": 42069001,
    "aiko's panties": 42069002,
    "kyanna's panties": 42069003,
    "audrey's panties": 42069004,
    "lola's panties": 42069005,
    "nikki's panties": 42069006,
    "jessie's panties": 42069007,
    "beli's panties": 42069008,
    "kyu's panties": 42069009,
    "momo's panties": 42069010,
    "celeste's panties": 42069011,
    "venus's panties": 42069012
}

girl_unlock_table = {
    "Unlock Girl(tiffany)": 42069013,
    "Unlock Girl(aiko)": 42069014,
    "Unlock Girl(kyanna)": 42069015,
    "Unlock Girl(audrey)": 42069016,
    "Unlock Girl(lola)": 42069017,
    "Unlock Girl(nikki)": 42069018,
    "Unlock Girl(jessie)": 42069019,
    "Unlock Girl(beli)": 42069020,
    "Unlock Girl(kyu)": 42069021,
    "Unlock Girl(momo)": 42069022,
    "Unlock Girl(celeste)": 42069023,
    "Unlock Girl(venus)": 42069024
}

gift_item_table = {
    "Decorative Pens": 42069025, #"academy gift item 1"
    "Glossy Notebook": 42069026, #"academy gift item 2"
    "Graduation Cap": 42069027, #"academy gift item 3"
    "Textbooks": 42069028, #"academy gift item 4"
    "Girly Backpack": 42069029, #"academy gift item 5"
    "Laptop Pro": 42069030, #"academy gift item 6"
    "Old Fashioned Yoyo": 42069031, #"toys gift item 1"
    "Puzzle Cube": 42069032, #"toys gift item 2"
    "Sudoku Books": 42069033, #"toys gift item 3"
    "Dart Board": 42069034, #"toys gift item 4"
    "Board Game": 42069035, #"toys gift item 5"
    "Chess Set": 42069036, #"toys gift item 6"
    "Water Bottle": 42069037, #"fitness gift item 1"
    "Cardio Weights": 42069038, #"fitness gift item 2"
    "Skipping Rope": 42069039, #"fitness gift item 3"
    "Kettle Bell": 42069040, #"fitness gift item 4"
    "Boxing Gloves": 42069041, #"fitness gift item 5"
    "Punching Bag": 42069042, #"fitness gift item 6"
    "Baby Binky": 42069043, #"rave gift item 1"
    "Bead Bracelet": 42069044, #"rave gift item 2"
    "Glow Sticks": 42069045, #"rave gift item 3"
    "Rainbow Wig": 42069046, #"rave gift item 4"
    "Fuzzy Boots": 42069047, #"rave gift item 5"
    "Fairy Wings": 42069048, #"rave gift item 6"
    "Tennis Balls": 42069049, #"sports gift item 1"
    "Tennis Racket": 42069050, #"sports gift item 2"
    "Flying Disc": 42069051, #"sports gift item 3"
    "Basket Ball": 42069052, #"sports gift item 4"
    "Volley Ball": 42069053, #"sports gift item 5"
    "Soccer Ball": 42069054, #"sports gift item 6"
    "Sketching Pencils": 42069055, #"artist gift item 1"
    "Paint Brushes": 42069056, #"artist gift item 2"
    "Drawing Mannequin": 42069057, #"artist gift item 3"
    "Sketch Pad": 42069058, #"artist gift item 4"
    "Paint Palette": 42069059, #"artist gift item 5"
    "Canvas & Easel": 42069060, #"artist gift item 6"
    "Baking Utensils": 42069061, #"baking gift item 1"
    "Measuring Cup": 42069062, #"baking gift item 2"
    "Rolling Pin": 42069063, #"baking gift item 3"
    "Oven Timer": 42069064, #"baking gift item 4"
    "Mixing Bowl": 42069065, #"baking gift item 5"
    "Oven Mitts": 42069066, #"baking gift item 6"
    "Yoga Belt": 42069067, #"yoga gift item 1"
    "Yoga Blocks": 42069068, #"yoga gift item 2"
    "Yoga Bag": 42069069, #"yoga gift item 3"
    "Yoga Mat": 42069070, #"yoga gift item 4"
    "Yoga Ball": 42069071, #"yoga gift item 5"
    "Yoga Outfit": 42069072, #"yoga gift item 6"
    "Tango Rose": 42069073, #"dancer gift item 1"
    "Sweatbands": 42069074, #"dancer gift item 2"
    "Leg Warmers": 42069075, #"dancer gift item 3"
    "Dancing Fan": 42069076, #"dancer gift item 4"
    "Pink Tutu": 42069077, #"dancer gift item 5"
    "Stripper Pole": 42069078, #"dancer gift item 6"
    "Synthetic Seaweed": 42069079, #"aquarium gift item 1"
    "Synthetic Coral": 42069080, #"aquarium gift item 2"
    "Tank Gravel": 42069081, #"aquarium gift item 3"
    "Bag of Goldfish": 42069082, #"aquarium gift item 4"
    "Fishy Castle": 42069083, #"aquarium gift item 5"
    "Fish Tank": 42069084, #"aquarium gift item 6"
    "Swimmers Cap": 42069085, #"scuba gift item 1"
    "Goggles": 42069086, #"scuba gift item 2"
    "Snorkel": 42069087, #"scuba gift item 3"
    "Flippers": 42069088, #"scuba gift item 4"
    "Lifesaver": 42069089, #"scuba gift item 5"
    "Diving Tank": 42069090, #"scuba gift item 6"
    "Flower Seeds": 42069091, #"garden gift item 1"
    "Garden Shovel": 42069092, #"garden gift item 2"
    "Flower Pots": 42069093, #"garden gift item 3"
    "Watering Can": 42069094, #"garden gift item 4"
    "Garden Gnome": 42069095, #"garden gift item 5"
    "Wooden Birdhouse": 42069096 #"garden gift item 6"
}

unique_item_table = {
    "Double Hair Bow": 42069097,  # "tiffany unique item 1"
    "Glitter Bottles": 42069098,  # "tiffany unique item 2"
    "Twirly Baton": 42069099,  # "tiffany unique item 3"
    "Megaphone": 42069100,  # "tiffany unique item 4"
    "Pom-poms": 42069101,  # "tiffany unique item 5"
    "Cheerleading Uniform": 42069102,  # "tiffany unique item 6"
    "Chopsticks": 42069103,  # "aiko unique item 1"
    "Riceballs": 42069104,  # "aiko unique item 2"
    "Bonsai Tree": 42069105,  # "aiko unique item 3"
    "Wooden Sandals": 42069106,  # "aiko unique item 4"
    "Kimono": 42069107,  # "aiko unique item 5"
    "Samurai Helmet": 42069108,  # "aiko unique item 6"
    "Maracas": 42069109,  # "kyanna unique item 1"
    "Sombrero": 42069110,  # "kyanna unique item 2"
    "Poncho": 42069111,  # "kyanna unique item 3"
    "Luchador Mask": 42069112,  # "kyanna unique item 4"
    "Pinata": 42069113,  # "kyanna unique item 5"
    "Vinuela": 42069114,  # "kyanna unique item 6"
    "Cigarette Pack": 42069115,  # "audrey unique item 1"
    "Lighter": 42069116,  # "audrey unique item 2"
    "Glass Pipe": 42069117,  # "audrey unique item 3"
    "Glass Bong": 42069118,  # "audrey unique item 4"
    "Blotter Tabs": 42069119,  # "audrey unique item 5"
    "Happy Pills": 42069120,  # "audrey unique item 6"
    "Wing Pin": 42069121,  # "lola unique item 1"
    "Compass": 42069122,  # "lola unique item 2"
    "Pilot's Cap": 42069123,  # "lola unique item 3"
    "Travel Suitcase": 42069124,  # "lola unique item 4"
    "Rolling Luggage": 42069125,  # "lola unique item 5"
    "High Def Camera": 42069126,  # "lola unique item 6"
    "Retro Controller": 42069127,  # "nikki unique item 1"
    "Arcade Joystick": 42069128,  # "nikki unique item 2"
    "Zappy Gun": 42069129,  # "nikki unique item 3"
    "Gamer Glove": 42069130,  # "nikki unique item 4"
    "Handheld Game": 42069131,  # "nikki unique item 5"
    "Arcade Cabinet": 42069132,  # "nikki unique item 6"
    "Mistletoe": 42069133,  # "jessie unique item 1"
    "Gingerbread Man": 42069134,  # "jessie unique item 2"
    "Round Ornament": 42069135,  # "jessie unique item 3"
    "Ribbon Wreath": 42069136,  # "jessie unique item 4"
    "Fuzzy Stocking": 42069137,  # "jessie unique item 5"
    "Jolly Old Cap": 42069138,  # "jessie unique item 6"
    "Acorns": 42069139,  # "beli unique item 1"
    "Maple Leaf": 42069140,  # "beli unique item 2"
    "Pinecone": 42069141,  # "beli unique item 3"
    "Mushrooms": 42069142,  # "beli unique item 4"
    "Seashell": 42069143,  # "beli unique item 5"
    "Four Leaf Clover": 42069144,  # "beli unique item 6"
    "Endurance Ring": 42069145,  # "kyu unique item 1"
    "Pocket Vibe": 42069146,  # "kyu unique item 2"
    "Fairy's Tail": 42069147,  # "kyu unique item 3"
    "Bliss Beads": 42069148,  # "kyu unique item 4"
    "Magic Wand": 42069149,  # "kyu unique item 5"
    "Royal Scepter": 42069150,  # "kyu unique item 6"
    "Ball of Yarn": 42069151,  # "momo unique item 1"
    "Lattice Ball": 42069152,  # "momo unique item 2"
    "Squeaky Mouse": 42069153,  # "momo unique item 3"
    "Feather Pole": 42069154,  # "momo unique item 4"
    "Laser Pointer": 42069155,  # "momo unique item 5"
    "Scratch Post": 42069156,  # "momo unique item 6"
    "Model Rocket": 42069157,  # "celeste unique item 1"
    "Miniature UFO": 42069158,  # "celeste unique item 2"
    "Armillary Sphere": 42069159,  # "celeste unique item 3"
    "Telescope": 42069160,  # "celeste unique item 4"
    "Space Helmet": 42069161,  # "celeste unique item 5"
    "Moonrock": 42069162,  # "celeste unique item 6"
    "Sapphire": 42069163,  # "venus unique item 1"
    "Ruby": 42069164,  # "venus unique item 2"
    "Emerald": 42069165,  # "venus unique item 3"
    "Topaz": 42069166,  # "venus unique item 4"
    "Amethyst": 42069167,  # "venus unique item 5"
    "Diamond": 42069168,  # "venus unique item 6"
}

token_item_table = {
    "talent lv-up 1": 42069169,
    "talent lv-up 2": 42069170,
    "talent lv-up 3": 42069171,
    "talent lv-up 4": 42069172,
    "talent lv-up 5": 42069173,
    "talent lv-up 6": 42069174,
    "flirtation lv-up 1": 42069175,
    "flirtation lv-up 2": 42069176,
    "flirtation lv-up 3": 42069177,
    "flirtation lv-up 4": 42069178,
    "flirtation lv-up 5": 42069179,
    "flirtation lv-up 6": 42069180,
    "romance lv-up 1": 42069181,
    "romance lv-up 2": 42069182,
    "romance lv-up 3": 42069183,
    "romance lv-up 4": 42069184,
    "romance lv-up 5": 42069185,
    "romance lv-up 6": 42069186,
    "sexuality lv-up 1": 42069187,
    "sexuality lv-up 2": 42069188,
    "sexuality lv-up 3": 42069189,
    "sexuality lv-up 4": 42069190,
    "sexuality lv-up 5": 42069191,
    "sexuality lv-up 6": 42069192,
    "passion lv-up 1": 42069193,
    "passion lv-up 2": 42069194,
    "passion lv-up 3": 42069195,
    "passion lv-up 4": 42069196,
    "passion lv-up 5": 42069197,
    "passion lv-up 6": 42069198,
    "sensitivity lv-up 1": 42069199,
    "sensitivity lv-up 2": 42069200,
    "sensitivity lv-up 3": 42069201,
    "sensitivity lv-up 4": 42069202,
    "sensitivity lv-up 5": 42069203,
    "sensitivity lv-up 6": 42069204,
    "charisma lv-up 1": 42069205,
    "charisma lv-up 2": 42069206,
    "charisma lv-up 3": 42069207,
    "charisma lv-up 4": 42069208,
    "charisma lv-up 5": 42069209,
    "charisma lv-up 6": 42069210,
    "luck lv-up 1": 42069211,
    "luck lv-up 2": 42069212,
    "luck lv-up 3": 42069213,
    "luck lv-up 4": 42069214,
    "luck lv-up 5": 42069215,
    "luck lv-up 6": 42069216,
}

progressive_token_item_table = {
    "progressive talent lv-up": 42069169,
    "progressive flirtation lv-up": 42069170,
    "progressive romance lv-up": 42069171,
    "progressive sexuality lv-up": 42069172,
    "progressive passion lv-up": 42069173,
    "progressive sensitivity lv-up": 42069174,
    "progressive charisma lv-up": 42069175,
    "progressive luck lv-up": 42069176,
}

junk_table = {
    #FOODS
    "Coffee": 42069177,
    "Orange Juice": 42069178,
    "Bagel": 42069179,
    "Muffin": 42069180,
    "Omelette": 42069181,
    "Pancakes": 42069182,
    "Cookies": 42069183,
    "Cupcakes": 42069184,
    "Sundae": 42069185,
    "Pumpkin Pie": 42069186,
    "Fruit Tart Pie": 42069187,
    "Wedding Cake": 42069188,
    "Orange": 42069189,
    "Lemon": 42069190,
    "Mango": 42069191,
    "Pinapple": 42069192,
    "Coconut": 42069193,
    "Watermelon": 42069194,
    "Carrot": 42069195,
    "Cucumber": 42069196,
    "Tomatos": 42069197,
    "Bell Peppers": 42069198,
    "Eggplant": 42069199,
    "Cabbage": 42069200,#
    "Heart Candies": 42069201,
    "Jelly Beans": 42069202,
    "Bubble Gum": 42069203,
    "Lollipop": 42069204,
    "Cotton Candy": 42069205,
    "Chocolate": 42069206,
    "Soda": 42069207,
    "Popcorn": 42069208,
    "French Fries": 42069209,
    "Corndog": 42069210,
    "Hamburger": 42069211,
    "Pizza": 42069212,
    #DRINKS
    "Beer": 42069213,
    "Sake": 42069214,
    "Wine": 42069215,
    "Champagne": 42069216,
    "Pina Colada": 42069217,
    "Daiquiri": 42069218,
    "Mojito": 42069219,
    "Lime Margarita": 42069220,
    "Martini": 42069221,
    "Cocktail": 42069222,
    "Lemon Drop": 42069223,
    "Whisky": 42069224,
    #DATE GIFTS
    "Hoop Earrings": 42069225,
    "Gold Earrings": 42069226,
    "Heart Necklace": 42069227,
    "Pearl Necklace": 42069228,
    "Silver Ring": 42069229,
    "Lovely Ring": 42069230,
    "Nail Polish": 42069231,
    "Shiny Lipstick": 42069232,
    "Hair Brush": 42069233,
    "Makeup Kit": 42069234,
    "Eyelash Curler": 42069235,
    "Compact Mirror": 42069236,
    "Peep Toe Heels": 42069237,
    "Cork Wedge Sandals": 42069238,
    "Vintage Platforms": 42069239,
    "Leopard Print Pumps": 42069240,
    "Pink Mary Janes": 42069241,
    "Suede Ankle Booties": 42069242,
    "Blue Orchid": 42069243,
    "White Pansy": 42069244,
    "Orange Cosmos": 42069245,
    "Red Tulip": 42069246,
    "Pink Lily": 42069247,
    "Sunflower": 42069248,
    "Stuffed Bear": 42069249,
    "Stuffed Cat": 42069250,
    "Stuffed Sheep": 42069251,
    "Stuffed Monkey": 42069252,
    "Stuffed Penguin": 42069253,
    "Stuffed Whale": 42069254,
    "Sea Breeze Perfume": 42069255,
    "Green Tea Perfume": 42069256,
    "Peach Perfume": 42069257,
    "Cinnamon Perfume": 42069258,
    "Rose Perfume": 42069259,
    "Lilac Perfume": 42069260,
    #ACCESSORYS not used in game only shown on trait upgrade screen
    #"Flute": 42069261,
    #"Drums": 42069262,
    #"Trumpet": 42069263,
    #"Banjo": 42069264,
    #"Violin": 42069265,
    #"Keyboard": 42069266,
    #"Sun Lotion": 42069267,
    #"Stylish Shades": 42069268,
    #"Flip Flops": 42069269,
    #"Beach Ball": 42069270,
    #"Big Beach Towel": 42069271,
    #"Surfboard": 42069272,
    #"Buttery Croissant": 42069273,
    #"Fresh Baguette": 42069274,
    #"Fancy Cheese": 42069275,
    #"French Beret": 42069276,
    #"Accordion": 42069277,
    #"Wine Bottle": 42069278,
    #"Hot Wax Candles": 42069279,
    #"Silk Blindfold": 42069280,
    #"Spiked Choker": 42069281,
    #"Fuzzy Handcuffs": 42069282,
    #"Leather Whip": 42069283,
    #"Ball Gag": 42069284,
    #"Ear Muffs": 42069285,
    #"Warm Mittens": 42069286,
    #"Snow Globe": 42069287,
    #"Ice Skates": 42069288,
    #"Snow Sled": 42069289,
    #"Snowboard": 42069290,
    #"Bandages": 42069291,
    #"Stethoscope": 42069292,
    #"Medical Clipboard": 42069293,
    #"Medicine Bottle": 42069294,
    #"First-Aid Kit": 42069295,
    #"Nurse Uniform": 42069296,
    #"Snake Flute": 42069297,
    #"Jeweled Turban": 42069298,
    #"Feather Fan": 42069299,
    #"Scarab Pendant": 42069300,
    #"Antique Vase": 42069301,
    #"Golden Cat Statue": 42069302,
    #"Poker Chips": 42069303,
    #"Lucky Dice": 42069304,
    #"Playing Cards": 42069305,
    #"Dealer's Cap": 42069306,
    #"Roulette Wheel": 42069307,
    #"Slot Machine": 42069308,
}

item_table = {
    "nothing": 42069998,
    "victory": 42069999,
    **panties_item_table,
    **girl_unlock_table,
    **gift_item_table,
    **unique_item_table,
    **progressive_token_item_table,
    **junk_table
}

itemgen_to_name = {
    #REGULAR GIFTS
    "academy gift item 1": "Decorative Pens",
    "academy gift item 2": "Glossy Notebook",
    "academy gift item 3": "Graduation Cap",
    "academy gift item 4": "Textbooks",
    "academy gift item 5": "Girly Backpack",
    "academy gift item 6": "Laptop Pro",
    "toys gift item 1": "Old Fashioned Yoyo",
    "toys gift item 2": "Puzzle Cube",
    "toys gift item 3": "Sudoku Books",
    "toys gift item 4": "Dart Board",
    "toys gift item 5": "Board Game",
    "toys gift item 6": "Chess Set",
    "fitness gift item 1": "Water Bottle",
    "fitness gift item 2": "Cardio Weights",
    "fitness gift item 3": "Skipping Rope",
    "fitness gift item 4": "Kettle Bell",
    "fitness gift item 5": "Boxing Gloves",
    "fitness gift item 6": "Punching Bag",
    "rave gift item 1": "Baby Binky",
    "rave gift item 2": "Bead Bracelet",
    "rave gift item 3": "Glow Sticks",
    "rave gift item 4": "Rainbow Wig",
    "rave gift item 5": "Fuzzy Boots",
    "rave gift item 6": "Fairy Wings",
    "sports gift item 1": "Tennis Balls",
    "sports gift item 2": "Tennis Racket",
    "sports gift item 3": "Flying Disc",
    "sports gift item 4": "Basket Ball",
    "sports gift item 5": "Volley Ball",
    "sports gift item 6": "Soccer Ball",
    "artist gift item 1": "Sketching Pencils",
    "artist gift item 2": "Paint Brushes",
    "artist gift item 3": "Drawing Mannequin",
    "artist gift item 4": "Sketch Pad",
    "artist gift item 5": "Paint Palette",
    "artist gift item 6": "Canvas & Easel",
    "baking gift item 1": "Baking Utensils",
    "baking gift item 2": "Measuring Cup",
    "baking gift item 3": "Rolling Pin",
    "baking gift item 4": "Oven Timer",
    "baking gift item 5": "Mixing Bowl",
    "baking gift item 6": "Oven Mitts",
    "yoga gift item 1": "Yoga Belt",
    "yoga gift item 2": "Yoga Blocks",
    "yoga gift item 3": "Yoga Bag",
    "yoga gift item 4": "Yoga Mat",
    "yoga gift item 5": "Yoga Ball",
    "yoga gift item 6": "Yoga Outfit",
    "dancer gift item 1": "Tango Rose",
    "dancer gift item 2": "Sweatbands",
    "dancer gift item 3": "Leg Warmers",
    "dancer gift item 4": "Dancing Fan",
    "dancer gift item 5": "Pink Tutu",
    "dancer gift item 6": "Stripper Pole",
    "aquarium gift item 1": "Synthetic Seaweed",
    "aquarium gift item 2": "Synthetic Coral",
    "aquarium gift item 3": "Tank Gravel",
    "aquarium gift item 4": "Bag of Goldfish",
    "aquarium gift item 5": "Fishy Castle",
    "aquarium gift item 6": "Fish Tank",
    "scuba gift item 1": "Swimmers Cap",
    "scuba gift item 2": "Goggles",
    "scuba gift item 3": "Snorkel",
    "scuba gift item 4": "Flippers",
    "scuba gift item 5": "Lifesaver",
    "scuba gift item 6": "Diving Tank",
    "garden gift item 1": "Flower Seeds",
    "garden gift item 2": "Garden Shovel",
    "garden gift item 3": "Flower Pots",
    "garden gift item 4": "Watering Can",
    "garden gift item 5": "Garden Gnome",
    "garden gift item 6": "Wooden Birdhouse",
    #UNIQUE GIFTS
    "tiffany unique item 1": "Double Hair Bow",
    "tiffany unique item 2": "Glitter Bottles",
    "tiffany unique item 3": "Twirly Baton",
    "tiffany unique item 4": "Megaphone",
    "tiffany unique item 5": "Pom-poms",
    "tiffany unique item 6": "Cheerleading Uniform",
    "aiko unique item 1": "Chopsticks",
    "aiko unique item 2": "Riceballs",
    "aiko unique item 3": "Bonsai Tree",
    "aiko unique item 4": "Wooden Sandals",
    "aiko unique item 5": "Kimono",
    "aiko unique item 6": "Samurai Helmet",
    "kyanna unique item 1": "Maracas",
    "kyanna unique item 2": "Sombrero",
    "kyanna unique item 3": "Poncho",
    "kyanna unique item 4": "Luchador Mask",
    "kyanna unique item 5": "Pinata",
    "kyanna unique item 6": "Vinuela",
    "audrey unique item 1": "Cigarette Pack",
    "audrey unique item 2": "Lighter",
    "audrey unique item 3": "Glass Pipe",
    "audrey unique item 4": "Glass Bong",
    "audrey unique item 5": "Blotter Tabs",
    "audrey unique item 6": "Happy Pills",
    "lola unique item 1": "Wing Pin",
    "lola unique item 2": "Compass",
    "lola unique item 3": "Pilot's Cap",
    "lola unique item 4": "Travel Suitcase",
    "lola unique item 5": "Rolling Luggage",
    "lola unique item 6": "High Def Camera",
    "nikki unique item 1": "Retro Controller",
    "nikki unique item 2": "Arcade Joystick",
    "nikki unique item 3": "Zappy Gun",
    "nikki unique item 4": "Gamer Glove",
    "nikki unique item 5": "Handheld Game",
    "nikki unique item 6": "Arcade Cabinet",
    "jessie unique item 1": "Mistletoe",
    "jessie unique item 2": "Gingerbread Man",
    "jessie unique item 3": "Round Ornament",
    "jessie unique item 4": "Ribbon Wreath",
    "jessie unique item 5": "Fuzzy Stocking",
    "jessie unique item 6": "Jolly Old Cap",
    "beli unique item 1": "Acorns",
    "beli unique item 2": "Maple Leaf",
    "beli unique item 3": "Pinecone",
    "beli unique item 4": "Mushrooms",
    "beli unique item 5": "Seashell",
    "beli unique item 6": "Four Leaf Clover",
    "kyu unique item 1": "Endurance Ring",
    "kyu unique item 2": "Pocket Vibe",
    "kyu unique item 3": "Fairy's Tail",
    "kyu unique item 4": "Bliss Beads",
    "kyu unique item 5": "Magic Wand",
    "kyu unique item 6": "Royal Scepter",
    "momo unique item 1": "Ball of Yarn",
    "momo unique item 2": "Lattice Ball",
    "momo unique item 3": "Squeaky Mouse",
    "momo unique item 4": "Feather Pole",
    "momo unique item 5": "Laser Pointer",
    "momo unique item 6": "Scratch Post",
    "celeste unique item 1": "Model Rocket",
    "celeste unique item 2": "Miniature UFO",
    "celeste unique item 3": "Armillary Sphere",
    "celeste unique item 4": "Telescope",
    "celeste unique item 5": "Space Helmet",
    "celeste unique item 6": "Moonrock",
    "venus unique item 1": "Sapphire",
    "venus unique item 2": "Ruby",
    "venus unique item 3": "Emerald",
    "venus unique item 4": "Topaz",
    "venus unique item 5": "Amethyst",
    "venus unique item 6": "Diamond",
}
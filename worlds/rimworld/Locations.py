from BaseClasses import Location

# -----------------------------------------------

base_location_id = 1
location_id_gap = 1000

generic_victory_requirements = [["Advanced Fabrication"],
                                ["Fabrication"],
                                ["Electricity"]]
ship_launch_victory_requirements = [["Starflight Basics"],
                                    ["Vacuum Cryptosleep Casket"],
                                    ["Starship Reactor"],
                                    ["Machine Persuasion"],
                                    ["Starflight Sensors"],
                                    ["Starflight Basics"]]
royalty_victory_requirements = [["Complex Furniture"],
                                ["Carpet-Making", "Stonecutting"],
                                ["Piano"],
                                ["Royal Apparel"]]
# For standardization - nothing is strictly required here. We
#    could include high-value stuff like Devilstrand or drugs
#    but... it's not necessary.
archonexus_victory_requirements = []
anomaly_victory_requirements = [["Void Provocation"],
                                ["Bioferrite Extraction"],
                                ["Entity Containment"],
                                ["Proximity Detector"],
                                ["Bioferrite Harvester"]]

class RimworldLocation(Location):
    game: str = "Rimworld"
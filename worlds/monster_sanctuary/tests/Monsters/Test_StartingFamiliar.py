# from test.bases import WorldTestBase
# from worlds.monster_sanctuary import encounters as ENCOUNTERS
#
#
# class TestFamiliar(WorldTestBase):
#     game = "Monster Sanctuary"
#     player: int = 1
#
#     familiar = ""
#
#     def test_starting_familiar(self):
#         encounter = self.multiworld.worlds[1].encounters["Menu_0"]
#         if self.familiar == "":
#             self.assertEqual(len(encounter.monsters), 0)
#         else:
#             self.assertEqual(len(encounter.monsters), 1)
#             self.assertEqual(encounter.monsters[0].name, self.familiar)
#
#
# class TestSpectralWolf(TestFamiliar):
#     options = {
#         "spectral_familiar": 0
#     }
#     familiar = "Spectral Wolf"
#
#
# class TestSpectralEagle(TestFamiliar):
#     options = {
#         "spectral_familiar": 1
#     }
#     familiar = "Spectral Eagle"
#
#
# class TestSpectralToad(TestFamiliar):
#     options = {
#         "spectral_familiar": 2
#     }
#     familiar = "Spectral Toad"
#
#
# class TestSpectralLion(TestFamiliar):
#     options = {
#         "spectral_familiar": 3
#     }
#     familiar = "Spectral Lion"

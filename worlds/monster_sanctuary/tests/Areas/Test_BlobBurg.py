from worlds.monster_sanctuary.tests.Areas.TestArea import TestArea


class BlobBurgTests(TestArea):
    def test_access_1(self):
        self.assertAccessible("BlobBurg_East1", "blob_burg_access_1", [])

    def test_access_2(self):
        self.assertNotAccessible("BlobBurg_East1", "blob_burg_access_2",
                                 ["Double Jump Boots", "Kongamato"])
        self.assertAccessible("BlobBurg_East1", "blob_burg_access_2",
                              ["Blob Burg Access", "Double Jump Boots", "Kongamato"])

    def test_access_3(self):
        self.assertNotAccessible("BlobBurg_East1", "blob_burg_access_3",
                                 ["Blob Burg Access", "Double Jump Boots", "Kongamato"])
        self.assertAccessible("BlobBurg_East1", "blob_burg_access_3",
                              ["Blob Burg Access", "Blob Burg Access", "Double Jump Boots", "Kongamato"])

    def test_access_4(self):
        self.assertNotAccessible("BlobBurg_East1", "blob_burg_access_4",
                                 ["Blob Burg Access", "Blob Burg Access", "Double Jump Boots", "Kongamato"])
        self.assertAccessible("BlobBurg_East1", "blob_burg_access_4",
                              ["Blob Burg Access", "Blob Burg Access", "Blob Burg Access",
                               "Double Jump Boots", "Kongamato"])

    def test_access_5(self):
        self.assertNotAccessible("BlobBurg_East1", "blob_burg_access_5",
                                 ["Blob Burg Access", "Blob Burg Access", "Blob Burg Access",
                                  "Double Jump Boots", "Kongamato"])
        self.assertAccessible("BlobBurg_East1", "blob_burg_access_5",
                              ["Blob Burg Access", "Blob Burg Access", "Blob Burg Access", "Blob Burg Access",
                               "Double Jump Boots", "Kongamato", "Koi"])

    def test_access_6(self):
        self.assertNotAccessible("BlobBurg_East1", "blob_burg_access_6",
                                 ["Blob Burg Access", "Blob Burg Access", "Blob Burg Access", "Blob Burg Access",
                                  "Double Jump Boots", "Kongamato"])
        self.assertAccessible("BlobBurg_East1", "blob_burg_access_6",
                              ["Blob Burg Access", "Blob Burg Access", "Blob Burg Access", "Blob Burg Access",
                               "Blob Burg Access", "Double Jump Boots", "Kongamato"])

    def test_access_7(self):
        self.assertNotAccessible("BlobBurg_East1", "BlobBurg_Worms_3",
                                 ["Blob Burg Access", "Blob Burg Access", "Blob Burg Access", "Blob Burg Access",
                                  "Blob Burg Access", "Double Jump Boots", "Kongamato"])
        self.assertAccessible("BlobBurg_East1", "BlobBurg_Worms_3",
                              ["Blob Burg Access", "Blob Burg Access", "Blob Burg Access", "Blob Burg Access",
                               "Blob Burg Access", "Blob Burg Access", "Double Jump Boots", "Kongamato"])


class BlobBurgWithOpenEntrances(TestArea):
    options = {
        "open_blob_burg": "entrances"
    }

    def test_blob_burg_accessible_without_key(self):
        self.assertAccessible("MagmaChamber_West7", "BlobBurg_East1_1_0", [])


class BlobBurgWithOpenRooms(TestArea):
    options = {
        "open_blob_burg": "open_walls"
    }

    def test_access_1(self):
        self.assertAccessible("BlobBurg_East1", "blob_burg_access_1", [])

    def test_access_2(self):
        self.assertAccessible("BlobBurg_East1", "blob_burg_access_2",
                              ["Double Jump Boots", "Kongamato"])

    def test_access_3(self):
        self.assertAccessible("BlobBurg_East1", "blob_burg_access_3",
                              ["Double Jump Boots", "Kongamato"])

    def test_access_4(self):
        self.assertAccessible("BlobBurg_East1", "blob_burg_access_4",
                              ["Double Jump Boots", "Kongamato"])

    def test_access_5(self):
        self.assertAccessible("BlobBurg_East1", "blob_burg_access_5",
                              ["Double Jump Boots", "Kongamato", "Koi"])

    def test_access_6(self):
        self.assertAccessible("BlobBurg_East1", "blob_burg_access_6",
                              ["Double Jump Boots", "Kongamato"])

    def test_access_7(self):
        self.assertAccessible("BlobBurg_East1", "BlobBurg_Worms_3",
                              ["Double Jump Boots", "Kongamato"])

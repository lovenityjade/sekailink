# External Patchers & App Flow

This file lists games that require external patchers/assets beyond standard AP patch files and how the client
should route through them.

## Twilight Princess
External assets/tools:
- REL Loader + seed file are provided by the Twilight Princess Randomizer site and its generator.
- The MultiworldGG Twilight Princess setup guide also requires these files in Dolphin save data.

Integrated source:
- `third_party/tools/TP_SaveFileHacker` (GCI hack tool for loading REL)
- `third_party/tools/TP_Randomizer`
- `third_party/tools/TP_Randomizer_Web_Generator`
- `third_party/tools/TP_GeckoPatcher`

Client flow (automation plan):
1) Collect user ISO path (no distribution of ISOs).
2) Download REL loader and seed file from the Randomizer downloads/generator endpoints.
3) Ensure Dolphin save directory contains:
   - `RELoader` (from REL loader download)
   - `aptest.gci` seed (from generator)
   - `RandomizerAP.US.gci` (from APWorld zip or latest release)
4) Use SaveFileHacker to inject the REL payload into the GCI if needed.
5) Launch Dolphin with the user ISO and auto-connect the TP client.

Notes:
- Seed/REL loader assets should be fetched at runtime to stay current.
- If the randomizer site changes, update the fetch URLs in the client.

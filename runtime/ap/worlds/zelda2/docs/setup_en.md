# Zelda II MultiworldGG Randomizer Setup Guide

## Required Software

- The BizHawk emulator. Versions 2.3.1 and higher are supported.
    - [BizHawk at TASVideos](https://tasvideos.org/BizHawk)
- Your legally acquired Zelda 2: The Adventure of Link USA copy


## Joining a MultiWorld Game

### Obtain your patch file and create your ROM

When you join a multiworld game, you will be asked to provide your config file to whoever is hosting. Once that is done,
the host will provide you with either a link to download your patch file, or with a zip file containing everyone's patch
files. Your patch file should have a `.apz2` extension.

Put your patch file on your desktop or somewhere convenient, and double click it. This should automatically launch the
client, and will also create your ROM in the same place as your patch file.


## Running the Client Program and Connecting to the Server

By default, opening a patch file will do steps 1-5 below for you automatically. Even so, keep them in your memory just
in case you have to close and reopen a window mid-game for some reason.

1. Zelda II use MultiworldGG's BizHawk Client. If the client isn't still open from when you patched your
game, you can re-open it from the launcher.
2. Ensure EmuHawk is running the patched ROM.
3. In EmuHawk, go to `Tools > Lua Console`. This window must stay open while playing.
4. In the Lua Console window, go to `Script > Open Script…`.
5. Navigate to your MultiworldGG install folder and open `data/lua/connector_bizhawk_generic.lua`.
6. The emulator may freeze every few seconds until it manages to connect to the client. This is expected. The BizHawk
Client window should indicate that it connected and recognized Pokémon Red/Blue.
7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
top text field of the client and click Connect.

To connect the client to the multiserver simply put `<address>:<port>` on the textfield on top and press enter (if the
server uses password, type in the bottom textfield `/connect <address>:<port> [password]`)

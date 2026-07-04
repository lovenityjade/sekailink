# Mega Man X3 Archipelago Tracker for PopTracker

This is a PopTracker pack for Mega Man X3 Archipelago. Created by MeridianBC.

![](https://raw.githubusercontent.com/BrianCumminger/megamanx3-ap-poptracker/master/images/screenshot.png)

## Installation

Just download the lastest build or source and put in your PopTracker packs folder.

## Recent Changes
### v1.0.3 (Jine 16, 2024)
- Fixed bug in how total armor upgrades is calculated

### v1.0.2 (June 15, 2024)
- Fixed oversight with dash lemon weakness

### v1.0.1 (June 12, 2024)
- Fixed logic regarding access to items past minibosses

### v1.0.0 (June 7, 2024)
- Version 1.0 because all game logic is now tracked and therefore should be feature complete for now.
- Reworked boss weaknesses. Weaknesses are now detected and used for logic calculations, with vanilla weaknesses as a fallback if not autotracking
- Bosses that can be reached and damaged but not required by logic are shown in yellow
- Refights are now hidden if 0 are required
- Added game-themed map icons to replace the default chest location marker
- Replaced maverick medal icon
- Fixed bug in maximum number of upgrades
  
### v0.1.1 (May 31, 2024)
- Fixed bug in armor upgrade count logic

## Features
Detects and tracks all logic as of Mega Man X3 AP 1.1.2 including:

- Pickupsanity
- Jammed buster
- Doppler and Vile access conditions (medals/weapons/upgrades/heart/sub/access codes)
- Boss weaknesses and appropriate "yellow" shading when a boss can be damaged but is not in logic (following strictness setting)
  
Includes individual stage maps showing item locations, with the option to automatically switch tabs (on by default).

## Usage
When using Archipelago auto tracking, logic settings will all be set automatically.  For manual operation (or to check which settings are active), click on the "Open Pack Settings" button at the top of PopTracker while this pack is loaded.

Brief notes for various settings when not using autotracker:
- Dr. Doppler and Vile options: sets the access requirements for Dr. Doppler Labs and Vile's Stage  If all of these are blank or set to 0, the associated stage will unlock when access codes are acquired.
- Bosses Require Weaknesses: yaml option (`logic_boss_weakness`).  Bosses will be shaded yellow if you can reach and damage them but destroying them is not in logic. When not autotracking, bosses are assumed to have unshuffled weaknesses.
- Jammed Buster: yaml option `jammed_buster` - adds an extra arm upgrade to the pool.  A jammed buster is indicated by a grayed out arms icon with a blue down arrow.
- There are a few access requirements (notably number of boss refights completed) which only update when an item changes.


## More Info

Check out PopTrackers Documentation on packs [here](https://github.com/black-sliver/PopTracker/blob/master/doc/PACKS.md)

Still having trouble realizing your pack and looking for help or just want more information about everything PopTracker? Check out the ['Unofficial' PopTracker Discord Server](https://discord.com/invite/gwThqMCPgK)!

## License

Public Domain
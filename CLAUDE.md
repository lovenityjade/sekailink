# SekaiLink - Archipelago Multiworld Hosting Platform

## Architecture Overview
- **Backend:** Flask + SQLAlchemy + PostgreSQL
- **Real-time:** Redis + Celery (background tasks)
- **Frontend:** Vanilla JS + ACE Editor
- **Auth:** Discord OAuth2
- **Deployment:** Docker Compose
- **VPS:** Ubuntu (Hostinger)

## Project Structure
```
sekailink/
├── backend/
│   ├── main.py              # Flask API principale
│   ├── tasks.py             # Celery tasks (génération)
│   ├── static/
│   │   ├── uploads/roms/    # ROMs uploadées (À CHANGER en temporaire)
│   │   └── boxarts/         # Images des jeux
│   ├── dashboard.html       # Dashboard UI
│   └── index.html           # Landing page
├── discord_bot/
│   └── bot.py               # Discord bot (placeholder)
├── archipelago_core/        # Clone du repo Archipelago
├── docker-compose.yml       # Docker setup
└── .env                     # SECRETS (NEVER COMMIT)
```

## Database Schema
- **users**: Discord auth, profiles, badges
- **yaml_files**: User YAML configs
- **rom_files**: ROM uploads avec SHA-1 validation
- **lobbies**: Multiworld sessions

## Current State
✅ Discord OAuth fonctionnel
✅ User system complet
✅ YAML Manager avec validation
✅ ROM Vault avec SHA-1 verification
✅ Basic lobby creation
✅ Docker setup (PostgreSQL + Redis + Celery)

⚠️ NEEDS WORK:
- WebSocket pour real-time lobby updates
- ROM temporaire (pas permanent storage)
- Custom worlds management
- Generation integration avec WebhostLib
- Time limit system
- Discord bot implementation

## Security Notes
- **CRITICAL:** All passwords/tokens sont dans .env (NOT in code)
- ROM uploads doivent être temporaires (/tmp/generation/)
- Rate limiting sur uploads
- Session management via Flask sessions

## Development Approach
1. Fix security issues first
2. Implement WebSocket real-time
3. Complete lobby system
4. Generation integration
5. Testing & polish

## Known Issues
- Passwords étaient hardcodés (FIXED avec .env)
- ROM storage est permanent (needs to be temporary)
- No WebSocket yet (Flask-SocketIO needed)
- Generation task needs progress updates
- No rate limiting on uploads

## Conventions
- French comments OK in code (Jade is québécoise)
- Use `logger` for debugging
- All API routes prefixed with `/api/`
- Docker containers: sekailink_db, sekailink_api, sekailink_bot

## Goals
Build a better Archipelago multiworld hosting platform than Discord-based coordination.
Replace the chaos of manual YAML collection, ROM sharing, and seed generation.

## Blueprint
INDEX
Page principale.
- Doit avoir la liste des jeux compatible avec boxart classé en ordre de sync faites
- Doit avoir la liste des gameroom active et une fonction search
- En entête doit avoir logo et doit mentionner powered by Archipelago (le mot archipleago est un lien vers archipelago.gg), nom d'utilisateur connecter, bouton setting, bouton logout
- Dans le bas de la page, liens pour help, faq, about, règlements, docs, discord, GitHub, donation.

DASHBOARD
User dashboard.
- Profile & Settings
-- Email (For any important messages; Won't be shared with third parties)
-- Pronouns
-- Profile Bio
-- Favorite Games
-- Bouttons: Update Account - Delete Account (avec popup de warning pour éviter deletion accidentelle)
- YAML Manager
-- Upload YAML (avec vérification)
-- Create YAML (voir: YAML Creation)
-- List les YAML avec bouton Delete, Edit
- ROM Manager
-- Important de mentionner assez clairement en évidence: "ROMs are for storage online for use on this website. No download links will be given in any circumstances and you must provide your own ROMs. We'll be doing SHA verifications to make sure your ROM is compatible with the selected games. Note that all ROMs are cleared every 1st of the month"
-- Liste des jeux demandant des ROMs pour la generation SEULEMENT ceux qui ne demande pas de ROM ne sont pas inclus avec upload (ca upload dans le répertoire du UUID de la personne)
- Twitch and Boardcasting
-- Connect with Twitch (Oauth)
-- Disconnect account
- Friends & Blacklist
-- Friendlist (Online, Offline)
-- Blacklist
-- Bouton: Add Friend, Remove Friend, Add Blacklist, Remove Blacklist

GAMEPAGE (une page par jeu)
- Boxart
- Gamename
- Favorite (active/inactive)
- Game description
- Bouton: Create YAML, Create Lobby, 
- Current Lobbies (List des lobby ouvert)
-- Nom du Lobby
-- Nom du créateur
-- Nombre de joueurs
-- Timer
-- Time Limit
-- Open/Private/Closed
-- Boardcast (Yes/No)
Note: Avoir des nom lisible pour les lobby name links (motsrandom-motsrandom-nombre4chiffres)

- Past Lobbies (Liste des lobbies fermer)
-- Nom du Lobby
-- Nom du créateur
-- Nombre de joueurs
-- Timer
-- Time Limit
-- Finished/DNF

LOBBY PAGE
- Nom de la room (avec lien et un bouton copy)
- Nombre d'utilisateur / Limite d'utilisateurs
- Règle de la sync
- Lien sekailink.xyz
- Started at
- Ended at
- Chatbox
- Timer
- Time Limit
- List des noms (pronoms) avec: Ready, Active/Finished/DNF, Timer, Broadcast (active/inactive), Host (or not), Game, Patch
- Bouton: Ready/Unready, Start Sync, Stop Sync, Settings, Generate
- Console de la room (uniquement visible pour le host pour envoyer des commande administratrice)
-- Notes: Ready sera disponible si le YAML et/ou la ROM sont uploader et confirmer fonctionnel; Start Sync sera disponible une fois que la seed est générer correctement, Stop Sync arrête le timer, et force release tous les jeux sur leur serveur. Start Sync/Stop Sync/Settings/Generate sont disponible seulement au host de la room. Si le host de la room quitte, alors un nouvel host sera attribuer, et les bouton deviennent disponible. Une fois la seed généré le bouton Settings deviens indisponible.
-- Chaque room a un liens vers leur serveur (sekailink.xyz:[port]) pour les client archipelago et les tracker
-- Si le jeu fournit une patch pour l'utilisateur, une fois la generation terminer, un bouton "Download Patch" sera alors disponible et fera le download de .appatch ou peux importe. 
- Feature Eventuelle: Creation d'un canal vocal sur le discord avec un bouton Join Voice Chat lier a un bot
discord sur le discord.
- A Voir: Certaines personne aiment avoir plus qu'un jeu; Voir une facon de faire fonctionner ca ici; Comment intégrer le tracker, un simple liens ? intégrer dans la page ?)

CREATE ROOM
- Nom de la room 
- Limite de joueur
- Règle de la sync (personalisé donc textbox)
- Settings de la Sync (trouvable dans WebhostLib)
- Time Limit (1h, 2h, 3h, 4h, etc...)
- Allow multigame
- Disallow ROM games
- Allow Broadcast
- Disallow use of supported custom worlds
- Deactivate Voice Chat
- Restrict Time limit
- Blacklisted Games (on liste les jeux compatible, et le host choisi de rejeter certain jeu)
- NOTES: Une fois al room créer, ca créer un liens copiable, dans les room settings du lobby, la possibilité de fermer un room, qui la rend en state fermée plus personne ne peux entrer mais elle est active, et de delete la room (on désactive la room, elle reste dans liste, elle deviens fermer et tout le monde est kick out).

YAML CREATION
- Utiliser ce qui est dans WebhostLib pour s'en inspirer
- Chaque personne dois avoir son YAML de créer pour le selectionner dans un lobby

CHATBOX
- Garder les logs des chatbox a des fin de moderation
- Chatbox rapporte ce qui se passe lors de la génération
- Chatbox dis quand un user entre et sort
- Dis quand tout le monde est prêt et que le compte a rebours commence
- Dis quand quelqu'un a fini ou DNF
- Dis si la sync est terminer au complet

BOT DISCORD
- Ajuste les roles (avoir un role pour playing, not playing serais le fun aussi)
- Rapporte les nouveau lobby ajouter
- Gere les canaux vocaux
- Annonce les status serveur (Online/Offline/Maintenance)

TIME LIMIT
- Une limite de temps est établit, si elle est en restrict mode, donc le monde doivent s'y tenir, une fois que la limite de temps est atteint, le serveur release les items de tout le monde, la room deviens fermer a tous. 

COMMENT LA GENERATION FONCTIONNE
- On utilise les outils fournit par WebdevHost. 

LINKS ET INFOS
- Il va nous falloir un set de règle. Important a noter et en GROSSE ÉVIDENCE: Archipelago.gg et leur communauté NE FONT PAS DE SOUTIENS TECHNIQUE FACE A SEKAILINK, si il y a un problème qui se passe sur SekaiLink, de se rapporter au notre discord dans Support ou Bug Report.
- Discord: https://discord.gg/XvvcBxrRsk
- GitHub: https://github.com/lovenityjade/sekailink
- Les donations font pour payer le serveur, il faut une donation page, mais mon paypal est un business paypal et ne peux pas accepter de donations direct a paypal, suggère moi un service de donation sécuritaire (j'avais penser a patreon, mais on offrent pas de service supplémentaire, je ne veux pas faire de compétition a archipelago, mais si SekaiWorld deviens plus gros et on a besoin d'un plus gros VPS, alors je vais devoir pouvoir le payer, si tu crois que patreon serait une bonne idée, alors dis moi comment)

CUSTOM WORLDS
- Nous allons supporter les custom worlds
- Cependant on averti les utilisateurs que les custom worlds sont en dévelopement et peuvent être instables
- Si un problème se produit avec un custom world, d'ouvrir un bug report SUR LE DISCORD DE SEKAILINK

THEME (CSS)
- Dark mode, gamer style, facile d'accès, multilingue (Français, Anglais pour commencer, on ajoutera des traduction), stylé et pro mais qui ne fais pas "page généré par l'IA", le tout modulable et facilement mis a jour

MODERATION PAGE
- Close lobby
- Join any lobbies
- Kick/Ban
- Warnings
- Edit lobbies
- Administrative powers over game servers (des fois que l'hote n'est pas disponible) (Release items, etc...)

ADMINISTRATION PAGE
- User list with user management (kick, ban, mod, admin) and search
- Same as moderation page
- Server logs
- Server Status
- Ban Appeals
- Link modifications
- Start/Stop/Restart Docker
- Reboot VPS
- Purge ROMS
- Purge YAMLs
- Activate maintenance mode
- Custom world file management
- Update archipelago

BANNISSEMENT
- Raison du bannissement (donc si un utilisateur se log, il va savoir si il est bani et va pouvoir faire un ban appeal)
- Date du ban
- Durée du ban
- Nombre de ban
- Appeal
- Si un ban est dépasser sa durée on l'neleve mais on garde un compte des bans
- NOTE: Faire la différence en SUSPENDED ACCOUNT et BANNED ACCOUNT

USER RATING
- Deux type de rating: Rating par users, Rating du serveur
- Rating par users, les users rate eux meme la personnage pour sous les critère: Ponctualité, Respect des autres, Respect des règles, Si release avec raison valable, etc...
- Rating du serveur: Le serveur se bases sur le nombre de kick, bannissement, suspension, nombre de sync finie, nombre de sync rage quit (release sans raison)
- Rating sont les deux sur 5 étoiles (avec une moyenne)
- Possibilité de faire un player review (l'utilisateur qui recoit un review peux faire un report)

USER REVIEWS
- Like, Dislike, Report
- Disponible a tous le monde
- Envoyer aux modérateurs/admin les nouveau review pour moderation (Approuve, disapprouve)

CREDITS
- VPS Hosting: Hostinger (avec liens)
- Archipelago.gg (code, backend, etc.. avec liens)

CONTACT
- sekailink@themiareproject.com


# 15 - Reference commandes

Chaque commande indique le role minimal. Les actions marquees Approval peuvent
etre preparees par Service mais executees seulement apres approbation Admin.

| Commande | Role | Confirmation | Description |
| --- | --- | --- | --- |
| `auth whoami` | Service | Non | Affiche l'identite Linux/SekaiLink et capabilities. |
| `dashboard` | Service | Non | Affiche la matrice locale Core Access MVP. |
| `commands list` | Service | Non | Liste le registre de commandes connu du binaire. |
| `commands search <query>` | Service | Non | Recherche dans le registre de commandes. |
| `server status <server|all> --execute` | Service | Non | Statut CPU/RAM/disk/uptime/services; live read-only si gate env active. |
| `server services <server>` | Service | Non | Liste services systemd connus. |
| `server logs <server> <service> --follow --execute` | Service | Non | Suit les logs d'un service; execution distante bloquee sans gate env. |
| `server restart <server> <service>` | Admin | Oui | Redemarre un service allowlist. |
| `server update plan <server>` | Admin | Non | Prepare un plan d'update. |
| `server update apply <server>` | Admin | Oui | Applique un update apres backup gate. |
| `ssh open <server>` | Admin | Oui | Ouvre une session SSH explicite. |
| `health probe <server|all> --execute` | Service | Non | Lance/verifie une sonde health; dry-run par defaut. |
| `logs list` | Service | Non | Liste les sources logs. |
| `logs list --by-server` | Service | Non | Vue logs par serveur/service. |
| `logs list --by-incident` | Service | Non | Vue logs correlees incident. |
| `logs tail <source> --execute` | Service | Non | Suit une source log; dry-run par defaut. |
| `logs search <query>` | Service | Non | Recherche logs. |
| `logs filter <expr>` | Service | Non | Filtre par user/lobby/room/correlation. |
| `logs pin` | Service | Non | Epingle la selection courante. |
| `logs note` | Service | Non | Cree une note liee. |
| `logs export --format <md|json|txt>` | Service | Non | Exporte logs/notes. |
| `audit export [query] [file-name]` | Service | Non | Exporte des lignes d'audit locales dans le dossier exports Core Access. |
| `note add <target> <text>` | Service | Non | Ajoute une note locale liee a un incident, log, user, lobby ou room. |
| `note list [query]` | Service | Non | Liste les notes locales, optionnellement filtrees. |
| `note export [query] [file-name]` | Service | Non | Exporte des notes locales dans le dossier exports Core Access. |
| `approval request <command> <reason>` | Service | Non | Demande une approbation Admin pour une commande sensible. |
| `approval list` | Service | Non | Liste les demandes et decisions locales. |
| `approval approve <id> [reason]` | Admin | Oui | Approuve localement une demande existante, sans execution serveur en MVP. |
| `ops snapshot [label]` | Service | Non | Cree un snapshot Markdown local avec dashboard, logs, services, audit, notes et approvals. |
| `user search <query>` | Service | Non | Recherche utilisateur. |
| `user open <user>` | Service | Non | Ouvre fiche utilisateur. |
| `user create` | Admin | Oui | Cree un utilisateur. |
| `user edit <user>` | Admin | Oui | Modifie un utilisateur. |
| `user disable <user>` | Admin | Oui | Desactive un utilisateur et revoke sessions. |
| `user sessions <user>` | Service | Non | Liste sessions. |
| `user devices <user>` | Service | Non | Liste devices. |
| `user audit <user>` | Service | Non | Audit utilisateur. |
| `user revoke-sessions <user>` | Admin | Oui | Revoque sessions. |
| `user force-password-reset <user>` | Admin | Oui | Force reset password. |
| `user configs <user>` | Service | Non | Liste configs utilisateur. |
| `user config open <user> <config>` | Service | Non | Ouvre config. |
| `user config diff <user> <config> <version>` | Service | Non | Compare versions. |
| `user config export <user> <config>` | Service | Non | Exporte YAML/JSON. |
| `user config edit <user> <config>` | Admin | Oui | Edite config source Nexus. |
| `lobby list` | Service | Non | Liste lobbies. |
| `lobby open <lobby>` | Service | Non | Ouvre lobby. |
| `lobby create` | Admin | Oui | Cree lobby manuel. |
| `lobby edit <lobby>` | Admin | Oui | Modifie lobby. |
| `lobby close <lobby>` | Service/Admin | Oui/Approval | Ferme lobby selon policy. |
| `lobby lock <lobby>` | Service | Oui | Verrouille lobby. |
| `lobby unlock <lobby>` | Service | Oui | Deverrouille lobby. |
| `lobby chat <lobby>` | Service | Non | Ouvre chat. |
| `lobby join-secret <lobby>` | Admin | Oui | Observe lobby invisiblement pour joueurs. |
| `lobby broadcast <lobby> <message>` | Service | Oui | Broadcast lobby. |
| `room list` | Service | Non | Liste rooms. |
| `room open <room>` | Service | Non | Ouvre room. |
| `room summary <room>` | Service | Non | Resume room. |
| `room events <room>` | Service | Non | Evenements room. |
| `room logs <room> --follow` | Service | Non | Logs room/AP. |
| `room snapshot <room>` | Service | Non | Snapshot et archive etat. |
| `room sync <room>` | Admin | Oui | Force sync payload. |
| `room pending-items <room>` | Service | Non | Items en attente. |
| `room client-reports <room>` | Service | Non | Rapports clients. |
| `room request-sklmi-reconnect <room> <player>` | Service | Oui | Demande reconnect SKLMI. |
| `room disconnect-runtime <room> <player>` | Admin | Oui | Deconnecte runtime. |
| `room give-item <room> <target> <item>` | Admin | Oui | Injecte item live. |
| `client broadcast` | Service | Oui | Message client realtime. |
| `client maintenance-banner` | Admin | Oui | Banniere maintenance client. |
| `client force-update-prompt` | Admin | Oui | Invite update. |
| `client request-relogin` | Service | Oui | Demande relogin. |
| `client refresh-lobby` | Service | Non | Refresh lobby. |
| `client refresh-room` | Service | Non | Refresh room. |
| `client request-sklmi-reconnect` | Service | Oui | Signal reconnect SKLMI. |
| `client restart-runtime` | Admin | Oui | Demande restart runtime client. |
| `client clear-cache-request` | Service | Approval | Demande clear cache. |
| `client diagnostics-request` | Service | Consentement | Demande diagnostics. |
| `broadcast global <message>` | Admin | Oui | Broadcast global. |
| `broadcast server <server> <message>` | Admin | Oui | Broadcast cible serveur. |
| `broadcast lobby <lobby> <message>` | Service | Oui | Broadcast lobby. |
| `broadcast room <room> <message>` | Service | Oui | Broadcast room. |
| `broadcast role <role> <message>` | Admin | Oui | Broadcast par role. |
| `broadcast version <version> <message>` | Admin | Oui | Broadcast par client version. |
| `broadcast game <game> <message>` | Service | Oui | Broadcast par jeu. |
| `maintenance status` | Service | Non | Etat maintenance. |
| `maintenance enable <scope>` | Admin | Oui | Active maintenance. |
| `maintenance disable` | Admin | Oui | Desactive maintenance. |
| `maintenance schedule <scope> <start> <end>` | Admin | Oui | Planifie maintenance. |
| `maintenance broadcast` | Admin | Oui | Annonce maintenance. |
| `maintenance history` | Service | Non | Historique. |
| `release current` | Service | Non | Release actuelle. |
| `release list` | Service | Non | Releases connues. |
| `release verify` | Service | Non | Verifie manifest. |
| `release verify-cdn` | Service | Non | Verifie CDN/SHA. |
| `release compare <a> <b>` | Service | Non | Compare releases. |
| `release publish <manifest>` | Admin | Oui | Publie manifest. |
| `release rollback <version>` | Admin | Oui | Rollback release. |
| `release schedule <version> <datetime>` | Admin | Oui | Planifie release. |
| `release notes <version>` | Service | Non | Notes release. |
| `release audit <version>` | Service | Non | Audit release. |
| `client-banner list` | Service | Non | Liste 3 slots. |
| `client-banner edit <slot>` | Admin | Oui | Edite slot. |
| `client-banner preview <slot>` | Service | Non | Preview. |
| `client-banner publish <slot>` | Admin | Oui | Publie slot. |
| `client-banner rollback <slot>` | Admin | Oui | Rollback slot. |
| `client-banner disable <slot>` | Admin | Oui | Desactive slot. |
| `pack repo list` | Service | Non | Liste repos packs. |
| `pack repo add` | Admin | Oui | Ajoute repo pack. |
| `pack repo edit <id>` | Admin | Oui | Edite repo pack. |
| `pack repo disable <id>` | Admin | Oui | Desactive repo. |
| `pack repo delete <id>` | Admin | Oui | Supprime repo. |
| `pack check <id>` | Service | Non | Check repo. |
| `pack validate <id>` | Service | Non | Valide pack. |
| `pack stage <id>` | Admin | Oui | Stage pack. |
| `pack publish <id>` | Admin | Oui | Publie pack. |
| `pack rollback <id> <version>` | Admin | Oui | Rollback pack. |
| `pack schedule-check <id> <cron|interval>` | Admin | Oui | Planifie checks. |
| `schedule list` | Service | Non | Liste jobs. |
| `schedule calendar` | Service | Non | Vue calendrier. |
| `schedule add` | Admin | Oui | Ajoute job. |
| `schedule edit <job>` | Admin | Oui | Edite job. |
| `schedule pause <job>` | Admin | Oui | Pause job. |
| `schedule resume <job>` | Admin | Oui | Resume job. |
| `schedule run-now <job>` | Admin | Oui | Lance job. |
| `schedule history <job>` | Service | Non | Historique job. |
| `discord status` | Service | Non | Statut bot Discord. |
| `discord reload` | Admin | Oui | Reload bot. |
| `discord announce <channel> <message>` | Service | Oui | Annonce Discord. |
| `discord sync-roles` | Admin | Oui | Sync roles. |
| `discord command list` | Service | Non | Liste commandes. |
| `discord command enable <name>` | Admin | Oui | Active commande. |
| `discord command disable <name>` | Admin | Oui | Desactive commande. |
| `discord command edit <name>` | Admin | Oui | Edite commande. |
| `discord timer list` | Service | Non | Liste timers. |
| `discord timer edit <id>` | Admin | Oui | Edite timer. |
| `discord incident-post <incident>` | Service | Oui | Poste incident. |
| `discord logs` | Service | Non | Logs Discord. |
| `twitch status` | Service | Non | Statut bot Twitch. |
| `twitch connect <channel>` | Admin | Oui | Connecte channel. |
| `twitch disconnect <channel>` | Admin | Oui | Deconnecte channel. |
| `twitch announce <channel> <message>` | Service | Oui | Annonce Twitch. |
| `twitch command list` | Service | Non | Liste commandes. |
| `twitch command enable <name>` | Admin | Oui | Active commande. |
| `twitch command disable <name>` | Admin | Oui | Desactive commande. |
| `twitch command edit <name>` | Admin | Oui | Edite commande. |
| `twitch timer list` | Service | Non | Liste timers. |
| `twitch timer edit <id>` | Admin | Oui | Edite timer. |
| `twitch lobby announce <lobby>` | Service | Oui | Annonce lobby. |
| `twitch stream set-title-hint` | Service | Non | Aide titre stream. |
| `twitch logs` | Service | Non | Logs Twitch. |
| `cleanup plan logs` | Admin | Non | Dry-run nettoyage logs. |
| `cleanup plan db` | Admin | Non | Dry-run nettoyage DB. |
| `cleanup plan spool` | Admin | Non | Dry-run spool. |
| `cleanup plan all` | Admin | Non | Dry-run global. |
| `cleanup apply <plan_id>` | Admin | Oui | Applique nettoyage. |
| `cleanup history` | Service | Non | Historique cleanup. |
| `cleanup rollback <id>` | Admin | Oui | Rollback si possible. |
| `audit search` | Service | Non | Recherche audit. |

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
| `server agent-health [server|all] --execute` | Service | Non | Lit `/health` des admin-agents loopback via SSH; gate read-only requis pour execution. |
| `server agent-system [server|all] --execute` | Service | Non | Lit `/system` des admin-agents avec token serveur et gate read-only. |
| `server agent-services [server|all] --execute` | Service | Non | Lit `/services` des admin-agents avec token serveur et gate read-only. |
| `server agent-service <server> <service> --execute` | Service | Non | Lit etat/state d'un service declare dans l'admin-agent. |
| `server agent-logs <server> <service> --execute` | Service | Non | Lit le tail JSON expose par l'admin-agent pour un service declare. |
| `nexus services --execute` | Service | Non | Liste les services connus par le private Nexus admin-agent; dry-run par defaut. |
| `server logs <server> <service> --follow --execute` | Service | Non | Suit les logs d'un service; execution distante bloquee sans gate env. |
| `server restart <server> <service> --confirm <server>:<service>:restart --execute` | Admin | Oui | Redemarre un service declare via admin-agent; gate mutation et confirmation exacte requis. |
| `server start <server> <service> --confirm <server>:<service>:start --execute` | Admin | Oui | Demarre un service declare via admin-agent; gate mutation et confirmation exacte requis. |
| `server stop <server> <service> --confirm <server>:<service>:stop --execute` | Admin | Oui | Arrete un service declare via admin-agent; gate mutation et confirmation exacte requis. |
| `server update plan <server>` | Admin | Non | Prepare un plan d'update. |
| `server update apply <server>` | Admin | Oui | Applique un update apres backup gate. |
| `ssh open <server>` | Admin | Oui | Ouvre une session SSH explicite. |
| `health probe <server|all> --execute` | Service | Non | Lance/verifie une sonde health; dry-run par defaut. |
| `logs list` | Service | Non | Liste les sources logs. |
| `logs list --by-server` | Service | Non | Vue logs par serveur/service. |
| `logs list --by-incident` | Service | Non | Vue logs correlees incident. |
| `logs tail <source> --execute` | Service | Non | Suit une source log; dry-run par defaut. |
| `logs search <query> [source|all] --execute` | Service | Non | Recherche read-only dans les logs allowlistes; dry-run par defaut. |
| `logs filter <term...> source:<source|all> --execute` | Service | Non | Filtre read-only par user/lobby/room/correlation; dry-run par defaut. |
| `logs pin <source> <text>` | Service | Non | Epingle une ligne ou un extrait comme preuve locale. |
| `logs note <source> <text>` | Service | Non | Cree une note locale ciblee `log:<source>`. |
| `logs export [query] --format <md|jsonl|txt> --file <name>` | Service | Non | Exporte pins et notes logs locaux. |
| `audit export [query] [file-name]` | Service | Non | Exporte des lignes d'audit locales dans le dossier exports Core Access. |
| `note add <target> <text>` | Service | Non | Ajoute une note locale liee a un incident, log, user, lobby ou room. |
| `note list [query]` | Service | Non | Liste les notes locales, optionnellement filtrees. |
| `note export [query] [file-name]` | Service | Non | Exporte des notes locales dans le dossier exports Core Access. |
| `approval request <command> <reason>` | Service | Non | Demande une approbation Admin pour une commande sensible. |
| `approval list` | Service | Non | Liste les demandes et decisions locales. |
| `approval approve <id> [reason]` | Admin | Oui | Approuve localement une demande existante, sans execution serveur en MVP. |
| `incident open <label> <severity> <summary>` | Service | Non | Ouvre un workspace incident local append-only. |
| `incident list [query]` | Service | Non | Liste les events incidents locaux, optionnellement filtres. |
| `incident status <label>` | Service | Non | Affiche etat, events, notes et pins locaux d'un incident. |
| `incident note <label> <text>` | Service | Non | Ajoute une note locale ciblee `incident:<label>`. |
| `incident pin <label> <source> <text>` | Service | Non | Attache une preuve log locale a un incident. |
| `incident export <label> --file <name>` | Service | Non | Exporte le workspace incident local en Markdown. |
| `incident close <label> <resolution>` | Service | Non | Ferme localement le workspace incident. |
| `ops snapshot [label]` | Service | Non | Cree un snapshot Markdown local avec dashboard, logs, services, audit, notes et approvals. |
| `ops timeline [query]` | Service | Non | Affiche une timeline locale audit/incidents/notes/pins/approvals/drafts. |
| `ops handoff [label] --file <name>` | Service | Non | Exporte un rapport Markdown local de releve de shift. |
| `ops doctor [--verbose]` | Service | Non | Verifie localement l'etat du cockpit, outils, env gates, stores, docs et PDFs sans exposer les secrets. |
| `ops paths` | Service | Non | Liste les chemins locaux Core Access utiles pour audit, notes, incidents, exports et docs. |
| `ops exports [query]` | Service | Non | Liste les exports locaux Core Access par date, taille et fichier. |
| `user search <query> --execute` | Service | Non | Recherche utilisateur via Nexus Identity read-only; dry-run par defaut. |
| `user open <user> --execute` | Service | Non | Ouvre fiche utilisateur via Nexus Identity read-only; ajoute une trace audit serveur. |
| `user create <user> <email> <role> --password-env ENV --confirm user:<user>:create --execute` | Admin | Oui | Cree un utilisateur via Nexus Identity; mot de passe lu depuis env local, body redacted en dry-run, gate mutation requis. |
| `user edit <user> key=value --confirm user:<user>:edit --execute` | Admin | Oui | Modifie un utilisateur via Nexus Identity; champs supportes: email, display_name, avatar_url, bio, locale, role, email_verified, disabled, permissions. |
| `user disable <user> --confirm user:<user>:disable --execute` | Admin | Oui | Desactive un utilisateur et revoke sessions via Nexus Identity; gate mutation requis. |
| `user sessions <user> --execute` | Service | Non | Liste sessions via Nexus Identity read-only; ajoute une trace audit serveur. |
| `user devices <user> --execute` | Service | Non | Liste devices via Nexus Identity read-only; ajoute une trace audit serveur. |
| `user audit <user> --execute` | Service | Non | Liste audit utilisateur via Nexus Identity read-only; ajoute une trace audit serveur. |
| `user revoke-sessions <user> --confirm user:<user>:revoke-sessions --execute` | Admin | Oui | Revoque toutes les sessions du compte cible via Nexus Identity; gate mutation requis. |
| `user force-password-reset <user> --confirm user:<user>:force-password-reset --execute` | Admin | Oui | Declenche un reset password via Nexus Identity; gate mutation requis. |
| `user configs <user>` | Service | Non | Liste configs utilisateur. |
| `user config open <user> <config>` | Service | Non | Ouvre config. |
| `user config diff <user> <config> <version>` | Service | Non | Compare versions. |
| `user config export <user> <config>` | Service | Non | Exporte YAML/JSON. |
| `user config edit <user> <config>` | Admin | Oui | Edite config source Nexus. |
| `lobby list --execute` | Service | Non | Liste les lobbies via Nexus lobby-admin read-only; dry-run par defaut. |
| `lobby open <lobby> --execute` | Service | Non | Ouvre un detail lobby via Nexus lobby-admin read-only; dry-run par defaut. |
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
| `client broadcast <user|all> <message> --confirm client:broadcast:<target>` | Service | Oui | Cree un draft audite de message client; aucun signal envoye. |
| `client maintenance-banner <user|all> <message> --confirm client:maintenance-banner:<target>` | Admin | Oui | Cree un draft audite de banniere maintenance client; aucun signal envoye. |
| `client force-update-prompt <user|all> <version|message> --confirm client:force-update-prompt:<target>` | Admin | Oui | Cree un draft audite d'invite update; aucun signal envoye. |
| `client request-relogin <user|all> <reason> --confirm client:request-relogin:<target>` | Service | Oui | Cree un draft audite de relogin; aucun signal envoye. |
| `client refresh-lobby <user|all> [lobby]` | Service | Non | Cree un draft audite de refresh lobby; aucun signal envoye. |
| `client refresh-room <user|all> [room]` | Service | Non | Cree un draft audite de refresh room; aucun signal envoye. |
| `client request-sklmi-reconnect <user|all> <reason> --confirm client:request-sklmi-reconnect:<target>` | Service | Oui | Cree un draft audite de reconnect SKLMI; aucun changement SKLMI. |
| `client restart-runtime <user|all> <reason> --confirm client:restart-runtime:<target>` | Admin | Oui | Cree un draft audite de restart runtime client; aucun signal envoye. |
| `client clear-cache-request <user|all> <reason> --confirm client:clear-cache-request:<target>` | Service | Approval | Cree un draft audite approval-required; aucun signal envoye. |
| `client diagnostics-request <user> <incident> <reason> --include <set>` | Service | Consentement | Prepare une demande locale de bundle diagnostics client. |
| `client diagnostics-list [query]` | Service | Non | Liste les demandes locales de diagnostics client. |
| `client diagnostics-export [query] --file <name>` | Service | Non | Exporte les demandes diagnostics et le contrat de bundle attendu. |
| `broadcast global <message> --confirm broadcast:global:all` | Admin | Oui | Cree un draft audite global; aucun broadcast envoye. |
| `broadcast server <server> <message> --confirm broadcast:server:<server>` | Admin | Oui | Cree un draft audite cible serveur; aucun broadcast envoye. |
| `broadcast lobby <lobby> <message> --confirm broadcast:lobby:<lobby>` | Service | Oui | Cree un draft audite lobby; aucun broadcast envoye. |
| `broadcast room <room> <message> --confirm broadcast:room:<room>` | Service | Oui | Cree un draft audite room; aucun broadcast envoye. |
| `broadcast role <role> <message> --confirm broadcast:role:<role>` | Admin | Oui | Cree un draft audite role; aucun broadcast envoye. |
| `broadcast version <version> <message> --confirm broadcast:version:<version>` | Admin | Oui | Cree un draft audite version client; aucun broadcast envoye. |
| `broadcast game <game> <message> --confirm broadcast:game:<game>` | Service | Oui | Cree un draft audite jeu; aucun broadcast envoye. |
| `maintenance status` | Service | Non | Etat maintenance. |
| `maintenance enable <scope> <message> --confirm maintenance:<scope>:enable` | Admin | Oui | Cree un draft audite d'activation maintenance; aucun mode serveur change. |
| `maintenance disable [scope] [reason] --confirm maintenance:<scope>:disable` | Admin | Oui | Cree un draft audite de desactivation maintenance; aucun mode serveur change. |
| `maintenance schedule <scope> <start> <end>` | Admin | Oui | Planifie maintenance. |
| `maintenance broadcast <scope> <message> --confirm maintenance:<scope>:broadcast` | Admin | Oui | Cree un draft audite d'annonce maintenance; aucun broadcast envoye. |
| `maintenance history` | Service | Non | Historique. |
| `release current` | Service | Non | Affiche le dernier manifeste local client update-bundle et ses URLs/fallbacks. |
| `release list` | Service | Non | Liste les manifestes locaux dedupliques par date/version/channel/build. |
| `release verify [latest|version|date|manifest] [--fast]` | Service | Non | Verifie existence, taille et SHA-256 des artefacts locaux; `--fast` saute le hash. |
| `release verify-cdn [channel] [platform|all] --execute` | Service | Non | Probe l'API publique `release-latest`; dry-run par defaut. |
| `release compare <a> <b>` | Service | Non | Compare deux manifestes locaux par version, URL, artefacts, tailles et hash. |
| `release publish <manifest|version|date> --confirm release:<version>:publish` | Admin | Oui | Cree un draft audite de publication; ne modifie pas le CDN. |
| `release rollback <version> --confirm release:<version>:rollback` | Admin | Oui | Cree un draft audite de rollback; ne modifie pas le CDN. |
| `release schedule <manifest|version|date> <datetime> --confirm release:<version>:schedule` | Admin | Oui | Cree un draft release et un draft scheduler; non arme. |
| `release notes [version|date|manifest]` | Service | Non | Affiche le manifeste local et les drafts release lies. |
| `release audit [version|date|manifest]` | Service | Non | Affiche le manifeste local et les drafts release lies. |
| `client-banner list` | Service | Non | Liste 3 slots. |
| `client-banner edit <slot>` | Admin | Oui | Edite slot. |
| `client-banner preview <slot>` | Service | Non | Preview. |
| `client-banner publish <slot> --confirm banner:<slot>:publish` | Admin | Oui | Cree un draft audite de publication banner; aucun dashboard client modifie. |
| `client-banner rollback <slot> --confirm banner:<slot>:rollback` | Admin | Oui | Cree un draft audite rollback banner; aucun dashboard client modifie. |
| `client-banner disable <slot> --confirm banner:<slot>:disable` | Admin | Oui | Cree un draft audite disable banner; aucun dashboard client modifie. |
| `pack repo list` | Service | Non | Liste repos packs. |
| `pack repo add <id> <url> <game> [notes] --confirm pack-repo:<id>:add` | Admin | Oui | Cree un draft repo pack; aucun fetch/publish. |
| `pack repo edit <id> key=value --confirm pack-repo:<id>:edit` | Admin | Oui | Cree un draft edition repo pack; aucun publish. |
| `pack repo disable <id> [reason] --confirm pack-repo:<id>:disable` | Admin | Oui | Cree un draft disable repo pack; aucun publish. |
| `pack repo delete <id> [reason] --confirm pack-repo:<id>:delete` | Admin | Oui | Cree un draft delete repo pack; aucun publish. |
| `pack check <id> [notes]` | Service | Non | Cree un draft check pack; aucun fetch reseau. |
| `pack validate <id> [notes]` | Service | Non | Cree un draft validate pack; aucune logique Lua executee. |
| `pack stage <id> [notes] --confirm pack:<id>:stage` | Admin | Oui | Cree un draft stage pack; aucun artifact CDN stage. |
| `pack publish <id> [notes] --confirm pack:<id>:publish` | Admin | Oui | Cree un draft publish pack; aucun artifact CDN publie. |
| `pack rollback <id> <version> --confirm pack:<id>:rollback:<version>` | Admin | Oui | Cree un draft rollback pack; aucun artifact CDN modifie. |
| `pack schedule-check <id> <cron|interval> --confirm pack:<id>:schedule-check` | Admin | Oui | Cree un draft scheduler de check pack; job non arme. |
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

use crate::admin_agent::{
    AgentRequestPlan, agent_control_plan, agent_health_plan, agent_logs_plan, agent_servers,
    agent_service_plan, agent_services_plan, agent_system_plan, execute_agent_request,
    find_agent_server,
};
use crate::audit::{
    Session, append_approval_decision, append_approval_request, append_audit,
    append_client_diagnostics_request, append_history, append_incident_event, append_log_pin,
    append_note, read_file_to_string, write_client_banner_draft, write_export,
    write_export_with_extension, write_maintenance_draft, write_ops_draft, write_pack_repo,
    write_release_draft, write_schedule_job,
};
use crate::commands::{COMMANDS, Confirmation, command_names, find_command, search_commands};
use crate::line_editor::LineEditor;
use crate::nexus::{
    LobbyListFilter, NEXUS_MUTATION_ENV, ProtectedGetPlan, admin_agent_services_plan,
    execute_protected_get, identity_user_create_plan, identity_user_disable_plan,
    identity_user_edit_plan, identity_user_force_password_reset_plan, identity_user_plan,
    identity_user_revoke_sessions_plan, lobby_list_plan, lobby_open_plan, non_flag_args,
    user_config_export_plan, user_config_open_plan, user_configs_plan,
};
use crate::rbac::Role;
use crate::release_ops::{
    dedupe_manifests, discover_manifests, latest_manifest, public_release_urls,
    render_manifest_compare, render_manifest_list, render_manifest_summary, render_verification,
    resolve_manifest, verify_manifest,
};
use crate::system::{
    known_services, log_catalog, render_dashboard, render_health_probe_plan,
    render_log_filter_plan, render_log_search_plan, render_log_tail_plan, render_server_logs_plan,
};
use crate::tui::TuiExit;
use crate::util::{home_dir, split_command_line};
use std::env;
use std::fs;
use std::io::{self, IsTerminal};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

pub fn run() -> Result<(), Box<dyn std::error::Error>> {
    let options = Options::parse(env::args().skip(1).collect())?;
    let linux_user = env::var("USER").unwrap_or_else(|_| "unknown-linux-user".to_string());
    let sekailink_user = options
        .user
        .or_else(|| env::var("SEKAILINK_CORE_ACCESS_USER").ok())
        .unwrap_or_else(|| linux_user.clone());
    let role = options
        .role
        .or_else(|| {
            env::var("SEKAILINK_CORE_ACCESS_ROLE")
                .ok()
                .and_then(|value| Role::parse(&value))
        })
        .unwrap_or(Role::Service);
    let data_dir = options
        .data_dir
        .unwrap_or_else(|| home_dir().join(".sekailink").join("core-access"));
    let mut app = App::new(Session::new(linux_user, sekailink_user, role, data_dir))?;

    if let Some(command) = options.command {
        app.execute(&command)?;
        return Ok(());
    }

    if options.shell || !io::stdin().is_terminal() || !io::stdout().is_terminal() {
        return app.run_interactive();
    }

    match crate::tui::run(app.session.clone())? {
        TuiExit::Quit => Ok(()),
        TuiExit::Shell => app.run_interactive(),
    }
}

struct Options {
    command: Option<String>,
    role: Option<Role>,
    user: Option<String>,
    data_dir: Option<PathBuf>,
    shell: bool,
}

impl Options {
    fn parse(args: Vec<String>) -> Result<Self, String> {
        let mut command = None;
        let mut role = None;
        let mut user = None;
        let mut data_dir = None;
        let mut shell = false;
        let mut i = 0;
        while i < args.len() {
            match args[i].as_str() {
                "--command" | "-c" => {
                    i += 1;
                    command = Some(args.get(i).ok_or("--command requires a value")?.clone());
                }
                "--role" => {
                    i += 1;
                    let value = args.get(i).ok_or("--role requires a value")?;
                    role = Role::parse(value);
                    if role.is_none() {
                        return Err(format!("unknown role: {value}"));
                    }
                }
                "--user" => {
                    i += 1;
                    user = Some(args.get(i).ok_or("--user requires a value")?.clone());
                }
                "--data-dir" => {
                    i += 1;
                    data_dir = Some(PathBuf::from(
                        args.get(i).ok_or("--data-dir requires a value")?,
                    ));
                }
                "--shell" => {
                    shell = true;
                }
                "--help" | "-h" => {
                    print_usage();
                    std::process::exit(0);
                }
                other => return Err(format!("unknown argument: {other}")),
            }
            i += 1;
        }
        Ok(Self {
            command,
            role,
            user,
            data_dir,
            shell,
        })
    }
}

struct App {
    session: Session,
}

impl App {
    fn new(session: Session) -> io::Result<Self> {
        session.ensure_dirs()?;
        Ok(Self { session })
    }

    fn run_interactive(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        print_banner(&self.session);
        println!("{}", render_dashboard());
        let completions = command_names().into_iter().map(str::to_string).collect();
        let history = self.load_history()?;
        let mut editor = LineEditor::new(completions, history);
        loop {
            let prompt = format!("skl:{}> ", self.session.role.as_str());
            let Some(line) = editor.read_line(&prompt)? else {
                break;
            };
            let trimmed = line.trim();
            if trimmed.is_empty() {
                continue;
            }
            append_history(&self.session, trimmed)?;
            let keep_running = self.execute(trimmed)?;
            if !keep_running {
                break;
            }
        }
        Ok(())
    }

    fn execute(&mut self, line: &str) -> Result<bool, Box<dyn std::error::Error>> {
        let parsed = match split_command_line(line) {
            Ok(parts) => parts,
            Err(err) => {
                println!("parse error: {err}");
                append_audit(&self.session, line, "parse_error", &err)?;
                return Ok(true);
            }
        };
        if parsed.is_empty() {
            return Ok(true);
        }

        let spec = find_command(line);
        if let Some(spec) = spec {
            if !self.session.role.allows(spec.role) {
                let detail = format!(
                    "blocked: role {} does not allow {} command",
                    self.session.role.as_str(),
                    spec.role.as_str()
                );
                println!("{detail}");
                println!("Use: approval request \"{line}\" \"reason\"");
                append_audit(&self.session, line, "rbac_denied", &detail)?;
                return Ok(true);
            }
        }

        let keep_running = match parsed[0].as_str() {
            "exit" | "quit" => {
                append_audit(&self.session, line, "ok", "exit")?;
                false
            }
            "help" => {
                print_help();
                append_audit(&self.session, line, "ok", "help")?;
                true
            }
            "dashboard" => {
                println!("{}", render_dashboard());
                append_audit(&self.session, line, "ok", "dashboard")?;
                true
            }
            "auth" if parsed.get(1).map(String::as_str) == Some("whoami") => {
                self.auth_whoami();
                append_audit(&self.session, line, "ok", "auth whoami")?;
                true
            }
            "commands" => {
                self.commands(&parsed);
                append_audit(&self.session, line, "ok", "commands")?;
                true
            }
            "server" if parsed.get(1).map(String::as_str) == Some("status") => {
                if parsed.iter().any(|part| part == "--execute") {
                    self.health_probe_target(first_non_flag(&parsed, 2, "all"), true)?;
                    append_audit(&self.session, line, "ok", "server status execute")?;
                } else {
                    println!("{}", render_dashboard());
                    append_audit(&self.session, line, "ok", "server status")?;
                }
                true
            }
            "server" if parsed.get(1).map(String::as_str) == Some("services") => {
                self.server_services(parsed.get(2).map(String::as_str));
                append_audit(&self.session, line, "ok", "server services")?;
                true
            }
            "server"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("agent-health")
                        | Some("agent-system")
                        | Some("agent-services")
                        | Some("agent-service")
                        | Some("agent-logs")
                ) =>
            {
                self.server_agent(&parsed)?;
                append_audit(&self.session, line, "ok", "server admin-agent read-only")?;
                true
            }
            "server"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("restart") | Some("start") | Some("stop")
                ) =>
            {
                self.server_agent_control(&parsed)?;
                append_audit(&self.session, line, "ok", "server admin-agent control")?;
                true
            }
            "nexus" if parsed.get(1).map(String::as_str) == Some("services") => {
                self.nexus_services(&parsed)?;
                append_audit(&self.session, line, "ok", "nexus services")?;
                true
            }
            "server" if parsed.get(1).map(String::as_str) == Some("logs") => {
                self.server_logs(&parsed)?;
                append_audit(&self.session, line, "ok", "server logs dry-run")?;
                true
            }
            "logs" if parsed.get(1).map(String::as_str) == Some("list") => {
                print_log_catalog(parsed.get(2).map(String::as_str));
                append_audit(&self.session, line, "ok", "logs list")?;
                true
            }
            "logs" if parsed.get(1).map(String::as_str) == Some("tail") => {
                self.logs_tail(&parsed)?;
                append_audit(&self.session, line, "ok", "logs tail dry-run")?;
                true
            }
            "logs"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("search" | "filter" | "pin" | "note" | "export")
                ) =>
            {
                self.logs_workflow(&parsed)?;
                append_audit(&self.session, line, "ok", "logs workflow")?;
                true
            }
            "health" if parsed.get(1).map(String::as_str) == Some("probe") => {
                self.health_probe(&parsed)?;
                append_audit(&self.session, line, "ok", "health probe dry-run")?;
                true
            }
            "user"
                if parsed.get(1).map(String::as_str) == Some("configs")
                    || parsed.get(1).map(String::as_str) == Some("config") =>
            {
                self.user_config(&parsed)?;
                append_audit(&self.session, line, "ok", "user config")?;
                true
            }
            "user"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("search")
                        | Some("open")
                        | Some("sessions")
                        | Some("devices")
                        | Some("audit")
                ) =>
            {
                self.user_readonly_plan(&parsed)?;
                append_audit(&self.session, line, "ok", "user read-only nexus plan")?;
                true
            }
            "user"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("create")
                        | Some("edit")
                        | Some("disable")
                        | Some("revoke-sessions")
                        | Some("force-password-reset")
                ) =>
            {
                self.user_admin_mutation(&parsed)?;
                append_audit(&self.session, line, "ok", "user admin nexus mutation")?;
                true
            }
            "lobby"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("list") | Some("open")
                ) =>
            {
                self.lobby_readonly(&parsed)?;
                append_audit(&self.session, line, "ok", "lobby read-only nexus")?;
                true
            }
            "audit" if parsed.get(1).map(String::as_str) == Some("search") => {
                let query = parsed.get(2).map(String::as_str).unwrap_or("");
                self.audit_search(query)?;
                append_audit(&self.session, line, "ok", "audit search")?;
                true
            }
            "audit" if parsed.get(1).map(String::as_str) == Some("export") => {
                let query = parsed.get(2).map(String::as_str).unwrap_or("");
                let requested_name = parsed.get(3).map(String::as_str);
                self.export_jsonl(
                    "audit",
                    &self.session.audit_dir().join("core-access.jsonl"),
                    query,
                    requested_name,
                )?;
                append_audit(&self.session, line, "ok", "audit export")?;
                true
            }
            "note" if parsed.get(1).map(String::as_str) == Some("add") => {
                self.note_add(&parsed)?;
                append_audit(&self.session, line, "ok", "note add")?;
                true
            }
            "note" if parsed.get(1).map(String::as_str) == Some("list") => {
                self.note_list(parsed.get(2).map(String::as_str).unwrap_or(""))?;
                append_audit(&self.session, line, "ok", "note list")?;
                true
            }
            "note" if parsed.get(1).map(String::as_str) == Some("export") => {
                let query = parsed.get(2).map(String::as_str).unwrap_or("");
                let requested_name = parsed.get(3).map(String::as_str);
                self.export_jsonl("notes", &self.session.notes_path(), query, requested_name)?;
                append_audit(&self.session, line, "ok", "note export")?;
                true
            }
            "approval" => {
                self.approval(&parsed, line)?;
                append_audit(&self.session, line, "ok", "approval")?;
                true
            }
            "incident" => {
                self.incident(&parsed)?;
                append_audit(&self.session, line, "ok", "incident local")?;
                true
            }
            "ops" => {
                self.ops(&parsed)?;
                append_audit(&self.session, line, "ok", "ops local")?;
                true
            }
            "client"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("diagnostics-request" | "diagnostics-list" | "diagnostics-export")
                ) =>
            {
                self.client_diagnostics(&parsed)?;
                append_audit(&self.session, line, "ok", "client diagnostics local")?;
                true
            }
            "client"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some(
                        "broadcast"
                            | "maintenance-banner"
                            | "force-update-prompt"
                            | "request-relogin"
                            | "refresh-lobby"
                            | "refresh-room"
                            | "request-sklmi-reconnect"
                            | "restart-runtime"
                            | "clear-cache-request"
                    )
                ) =>
            {
                self.client_signal_draft(&parsed)?;
                append_audit(&self.session, line, "ok", "client signal draft")?;
                true
            }
            "client-banner"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("list")
                        | Some("preview")
                        | Some("edit")
                        | Some("publish" | "rollback" | "disable")
                ) =>
            {
                self.client_banner(&parsed)?;
                append_audit(&self.session, line, "ok", "client-banner")?;
                true
            }
            "broadcast"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("global" | "server" | "lobby" | "room" | "role" | "version" | "game")
                ) =>
            {
                self.broadcast_draft(&parsed)?;
                append_audit(&self.session, line, "ok", "broadcast draft")?;
                true
            }
            "maintenance"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("enable" | "disable" | "status" | "schedule" | "broadcast" | "history")
                ) =>
            {
                self.maintenance(&parsed)?;
                append_audit(&self.session, line, "ok", "maintenance local")?;
                true
            }
            "release"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some(
                        "current"
                            | "list"
                            | "verify"
                            | "verify-cdn"
                            | "compare"
                            | "publish"
                            | "rollback"
                            | "schedule"
                            | "notes"
                            | "audit"
                    )
                ) =>
            {
                self.release(&parsed)?;
                append_audit(&self.session, line, "ok", "release workflow")?;
                true
            }
            "schedule"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some(
                        "list"
                            | "calendar"
                            | "add"
                            | "edit"
                            | "pause"
                            | "resume"
                            | "run-now"
                            | "history"
                    )
                ) =>
            {
                self.schedule(&parsed)?;
                append_audit(&self.session, line, "ok", "schedule local")?;
                true
            }
            "cleanup"
                if parsed.get(1).map(String::as_str) == Some("plan")
                    && matches!(
                        parsed.get(2).map(String::as_str),
                        Some("logs" | "db" | "spool" | "all")
                    )
                    || matches!(
                        parsed.get(1).map(String::as_str),
                        Some("apply" | "history" | "rollback")
                    ) =>
            {
                self.cleanup(&parsed)?;
                append_audit(&self.session, line, "ok", "cleanup local")?;
                true
            }
            "pack"
                if parsed.get(1).map(String::as_str) == Some("repo")
                    && matches!(
                        parsed.get(2).map(String::as_str),
                        Some("list" | "add" | "edit" | "disable" | "delete")
                    )
                    || matches!(
                        parsed.get(1).map(String::as_str),
                        Some(
                            "check"
                                | "validate"
                                | "stage"
                                | "publish"
                                | "rollback"
                                | "schedule-check"
                        )
                    ) =>
            {
                self.pack(&parsed)?;
                append_audit(&self.session, line, "ok", "pack local")?;
                true
            }
            "discord" | "twitch" => {
                self.bot_ops(&parsed)?;
                append_audit(&self.session, line, "ok", "bot ops")?;
                true
            }
            _ => {
                self.stub_or_unknown(line, spec)?;
                true
            }
        };
        Ok(keep_running)
    }

    fn load_history(&self) -> io::Result<Vec<String>> {
        let text = read_file_to_string(&self.session.history_path())?;
        Ok(text
            .lines()
            .rev()
            .filter(|line| !line.trim().is_empty())
            .take(500)
            .map(str::to_string)
            .collect::<Vec<_>>()
            .into_iter()
            .rev()
            .collect())
    }

    fn auth_whoami(&self) {
        println!("Linux user      : {}", self.session.linux_user);
        println!("SekaiLink user  : {}", self.session.sekailink_user);
        println!("Role            : {}", self.session.role.as_str());
        println!("Session         : {}", self.session.session_id);
        println!("Data dir        : {}", self.session.data_dir.display());
        println!("Auth mode       : MVP bootstrap (Nexus login pending)");
    }

    fn commands(&self, parsed: &[String]) {
        match parsed.get(1).map(String::as_str) {
            Some("list") | None => {
                for cmd in COMMANDS {
                    let implemented = if cmd.implemented { "ready" } else { "planned" };
                    println!(
                        "{:<36} {:<7} {:<8} {:?}  {}",
                        cmd.name,
                        cmd.role.as_str(),
                        implemented,
                        cmd.confirmation,
                        cmd.description
                    );
                }
            }
            Some("search") => {
                let query = parsed.get(2).map(String::as_str).unwrap_or("");
                for cmd in search_commands(query) {
                    println!("{:<36} {}", cmd.name, cmd.description);
                }
            }
            _ => println!("usage: commands list | commands search <query>"),
        }
    }

    fn audit_search(&self, query: &str) -> io::Result<()> {
        let path = self.session.audit_dir().join("core-access.jsonl");
        let text = read_file_to_string(&path)?;
        if text.is_empty() {
            println!("audit log is empty: {}", path.display());
            return Ok(());
        }
        for line in text
            .lines()
            .filter(|line| query.is_empty() || line.contains(query))
            .take(200)
        {
            println!("{line}");
        }
        Ok(())
    }

    fn export_jsonl(
        &self,
        prefix: &str,
        source_path: &std::path::Path,
        query: &str,
        requested_name: Option<&str>,
    ) -> io::Result<()> {
        let text = read_file_to_string(source_path)?;
        if text.is_empty() {
            println!("nothing to export from {}", source_path.display());
            return Ok(());
        }
        let mut body = String::new();
        let mut count = 0_usize;
        for line in text
            .lines()
            .filter(|line| query.is_empty() || line.contains(query))
        {
            body.push_str(line);
            body.push('\n');
            count += 1;
        }
        if count == 0 {
            println!("no lines matched export query: {query}");
            return Ok(());
        }
        let path = write_export(&self.session, prefix, requested_name, &body)?;
        println!("exported {count} lines to {}", path.display());
        println!("MVP note: exports are local Core Access files, not Nexus DB records yet.");
        Ok(())
    }

    fn server_services(&self, requested_server: Option<&str>) {
        let requested = requested_server.unwrap_or("all");
        println!("{:<12} SERVICES", "SERVER");
        println!("{}", "-".repeat(76));
        let mut found = false;
        for (server, services) in known_services() {
            if requested != "all" && requested != *server {
                continue;
            }
            found = true;
            println!("{server:<12} {}", services.join(", "));
        }
        if !found {
            println!("unknown server: {requested}");
            println!("known servers: nexus, link, worlds, evolution, pulse");
        }
        println!();
        println!("MVP note: service allowlist only; no systemd or SSH call was made.");
    }

    fn server_agent(&self, parsed: &[String]) -> io::Result<()> {
        let action = parsed.get(1).map(String::as_str).unwrap_or("");
        let execute = parsed.iter().any(|part| part == "--execute");
        match action {
            "agent-health" | "agent-system" | "agent-services" => {
                let target = parsed
                    .iter()
                    .skip(2)
                    .find(|part| !part.starts_with("--"))
                    .map(String::as_str)
                    .unwrap_or("all");
                let servers = if target == "all" {
                    agent_servers().to_vec()
                } else {
                    match find_agent_server(target) {
                        Ok(server) => vec![server],
                        Err(err) => {
                            println!("{err}");
                            return Ok(());
                        }
                    }
                };
                for server in servers {
                    let plan = match action {
                        "agent-health" => agent_health_plan(server),
                        "agent-system" => agent_system_plan(server),
                        _ => agent_services_plan(server),
                    };
                    self.render_or_execute_agent_plan(&plan, execute, None)?;
                }
                Ok(())
            }
            "agent-service" | "agent-logs" => {
                let args = option_positionals(parsed, 2, &[]);
                if args.len() != 2 {
                    println!("usage: server {action} <server> <service> [--execute]");
                    return Ok(());
                }
                let server = match find_agent_server(&args[0]) {
                    Ok(server) => server,
                    Err(err) => {
                        println!("{err}");
                        return Ok(());
                    }
                };
                let plan = if action == "agent-service" {
                    agent_service_plan(server, &args[1])
                } else {
                    agent_logs_plan(server, &args[1])
                };
                match plan {
                    Ok(plan) => self.render_or_execute_agent_plan(&plan, execute, None),
                    Err(err) => {
                        println!("{err}");
                        Ok(())
                    }
                }
            }
            _ => {
                println!(
                    "usage: server agent-health|agent-system|agent-services [server|all] [--execute]"
                );
                println!("       server agent-service|agent-logs <server> <service> [--execute]");
                Ok(())
            }
        }
    }

    fn server_agent_control(&self, parsed: &[String]) -> io::Result<()> {
        let action = parsed.get(1).map(String::as_str).unwrap_or("");
        let args = option_positionals(parsed, 2, &["--confirm"]);
        if args.len() != 2 {
            println!(
                "usage: server {action} <server> <service> --confirm <server>:<service>:{action} [--execute]"
            );
            return Ok(());
        }
        let server = match find_agent_server(&args[0]) {
            Ok(server) => server,
            Err(err) => {
                println!("{err}");
                return Ok(());
            }
        };
        let execute = parsed.iter().any(|part| part == "--execute");
        let confirm = flag_value(parsed, "--confirm");
        match agent_control_plan(server, &args[1], action) {
            Ok(plan) => self.render_or_execute_agent_plan(&plan, execute, confirm),
            Err(err) => {
                println!("{err}");
                Ok(())
            }
        }
    }

    fn render_or_execute_agent_plan(
        &self,
        plan: &AgentRequestPlan,
        execute: bool,
        confirm: Option<&str>,
    ) -> io::Result<()> {
        if !execute {
            println!("dry-run admin-agent request:");
            println!("{}", plan.render_dry_run());
            if let Some(expected) = plan.mutation_confirm.as_deref() {
                println!(
                    "MVP note: command was not executed. Add --execute, set SEKAILINK_CORE_ACCESS_REMOTE_MUTATION=1, and pass --confirm {expected} to run it."
                );
            } else {
                println!(
                    "MVP note: command was not executed. Add --execute and set SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1 to run it."
                );
            }
            println!();
            return Ok(());
        }
        execute_agent_request(plan, confirm)
    }

    fn server_logs(&self, parsed: &[String]) -> io::Result<()> {
        let Some(server) = parsed.get(2) else {
            println!("usage: server logs <server> <service> [--follow]");
            return Ok(());
        };
        let Some(service) = parsed.get(3) else {
            println!("usage: server logs <server> <service> [--follow]");
            return Ok(());
        };
        let follow = parsed.iter().any(|part| part == "--follow" || part == "-f");
        let execute = parsed.iter().any(|part| part == "--execute");
        match render_server_logs_plan(server, service, follow) {
            Ok(plan) => {
                self.render_or_execute_remote_plan("remote log command", &plan, execute)?;
            }
            Err(err) => println!("{err}"),
        }
        Ok(())
    }

    fn logs_tail(&self, parsed: &[String]) -> io::Result<()> {
        let Some(source) = parsed.get(2) else {
            println!("usage: logs tail <source> [--follow]");
            println!("try: logs list");
            return Ok(());
        };
        let follow = parsed.iter().any(|part| part == "--follow" || part == "-f");
        let execute = parsed.iter().any(|part| part == "--execute");
        match render_log_tail_plan(source, follow) {
            Ok(plan) => {
                self.render_or_execute_remote_plan("remote log command", &plan, execute)?;
            }
            Err(err) => println!("{err}"),
        }
        Ok(())
    }

    fn logs_workflow(&self, parsed: &[String]) -> io::Result<()> {
        match parsed.get(1).map(String::as_str) {
            Some("search") => self.logs_search(parsed),
            Some("filter") => self.logs_filter(parsed),
            Some("pin") => self.logs_pin(parsed),
            Some("note") => self.logs_note(parsed),
            Some("export") => self.logs_export(parsed),
            _ => {
                println!("usage: logs search|filter|pin|note|export");
                Ok(())
            }
        }
    }

    fn logs_search(&self, parsed: &[String]) -> io::Result<()> {
        let execute = parsed.iter().any(|part| part == "--execute");
        let args = non_flag_args(parsed, 2);
        let Some(query) = args.first() else {
            println!("usage: logs search <query> [source|all] [--execute]");
            return Ok(());
        };
        let source = args.get(1).map(String::as_str).unwrap_or("all");
        match render_log_search_plan(source, query) {
            Ok(plan) => self.render_or_execute_remote_plan("log search", &plan, execute),
            Err(err) => {
                println!("{err}");
                Ok(())
            }
        }
    }

    fn logs_filter(&self, parsed: &[String]) -> io::Result<()> {
        let execute = parsed.iter().any(|part| part == "--execute");
        let args = non_flag_args(parsed, 2);
        if args.is_empty() {
            println!("usage: logs filter <term...> [source:<source|all>] [--execute]");
            return Ok(());
        }
        let source = args
            .iter()
            .find_map(|arg| arg.strip_prefix("source:"))
            .unwrap_or("all");
        let filters = args
            .iter()
            .filter(|arg| !arg.starts_with("source:"))
            .map(|arg| log_filter_search_term(arg))
            .collect::<Vec<_>>();
        match render_log_filter_plan(source, &filters) {
            Ok(plan) => self.render_or_execute_remote_plan("log filter", &plan, execute),
            Err(err) => {
                println!("{err}");
                Ok(())
            }
        }
    }

    fn logs_pin(&self, parsed: &[String]) -> io::Result<()> {
        let Some(source) = parsed.get(2) else {
            println!("usage: logs pin <source> <text>");
            return Ok(());
        };
        let text = if parsed.len() > 3 {
            parsed[3..].join(" ")
        } else {
            String::new()
        };
        if text.trim().is_empty() {
            println!("usage: logs pin <source> <text>");
            return Ok(());
        }
        let id = append_log_pin(&self.session, source, &text)?;
        println!("log pin created: {id}");
        println!("source: {source}");
        Ok(())
    }

    fn logs_note(&self, parsed: &[String]) -> io::Result<()> {
        let Some(source) = parsed.get(2) else {
            println!("usage: logs note <source> <text>");
            return Ok(());
        };
        let text = if parsed.len() > 3 {
            parsed[3..].join(" ")
        } else {
            String::new()
        };
        if text.trim().is_empty() {
            println!("usage: logs note <source> <text>");
            return Ok(());
        }
        let id = append_note(&self.session, &format!("log:{source}"), &text)?;
        println!("log note created: {id}");
        println!("target: log:{source}");
        Ok(())
    }

    fn logs_export(&self, parsed: &[String]) -> io::Result<()> {
        let options = LogExportOptions::from_parts(parsed);
        let pins = read_file_to_string(&self.session.log_pins_path())?;
        let notes = read_file_to_string(&self.session.notes_path())?;
        let body = render_logs_export(&pins, &notes, &options);
        if body.trim().is_empty() {
            println!("nothing to export from local log pins/notes");
            return Ok(());
        }
        let path = write_export_with_extension(
            &self.session,
            "logs-evidence",
            options.file_name.as_deref(),
            options.format.extension(),
            &body,
        )?;
        println!("logs evidence exported to {}", path.display());
        println!("format: {}", options.format.as_str());
        println!("MVP note: export is local; no Nexus DB incident record was changed.");
        Ok(())
    }

    fn health_probe(&self, parsed: &[String]) -> io::Result<()> {
        let execute = parsed.iter().any(|part| part == "--execute");
        self.health_probe_target(first_non_flag(parsed, 2, "all"), execute)
    }

    fn health_probe_target(&self, target: &str, execute: bool) -> io::Result<()> {
        match render_health_probe_plan(target) {
            Ok(plan) => {
                self.render_or_execute_remote_plan("health probe", &plan, execute)?;
            }
            Err(err) => println!("{err}"),
        }
        Ok(())
    }

    fn render_or_execute_remote_plan(
        &self,
        label: &str,
        plan: &str,
        execute: bool,
    ) -> io::Result<()> {
        if !execute {
            println!("dry-run {label}:");
            println!("{plan}");
            println!(
                "MVP note: command was not executed. Add --execute and set SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1 to run it."
            );
            return Ok(());
        }
        if env::var("SEKAILINK_CORE_ACCESS_REMOTE_READONLY")
            .ok()
            .as_deref()
            != Some("1")
        {
            println!("remote read-only execution blocked by environment gate");
            println!("set SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1 and rerun with --execute");
            println!("planned command:");
            println!("{plan}");
            return Ok(());
        }
        println!("executing read-only {label}:");
        println!("{plan}");
        let term = env::var("TERM")
            .ok()
            .filter(|value| value != "dumb")
            .unwrap_or_else(|| "xterm-256color".to_string());
        let status = Command::new("sh")
            .arg("-lc")
            .arg(plan)
            .env("TERM", term)
            .status()?;
        println!("remote command exit status: {status}");
        Ok(())
    }

    fn nexus_services(&self, parsed: &[String]) -> io::Result<()> {
        if parsed.len() > 2 && parsed.iter().filter(|part| !part.starts_with("--")).count() > 2 {
            println!("usage: nexus services [--execute]");
            return Ok(());
        }
        let execute = parsed.iter().any(|part| part == "--execute");
        let plan = admin_agent_services_plan();
        self.render_or_execute_nexus_get(&plan, execute, None)
    }

    fn lobby_readonly(&self, parsed: &[String]) -> io::Result<()> {
        let execute = parsed.iter().any(|part| part == "--execute");
        match parsed.get(1).map(String::as_str) {
            Some("list") => {
                let args = non_flag_args(parsed, 2);
                if args.len() > 5 {
                    println!(
                        "usage: lobby list [limit] [query] [visibility] [status] [offset] [--execute]"
                    );
                    return Ok(());
                }
                let filter = LobbyListFilter::from_positionals(&args);
                let plan = lobby_list_plan(&filter);
                self.render_or_execute_nexus_get(&plan, execute, None)
            }
            Some("open") => {
                let args = non_flag_args(parsed, 2);
                if args.len() != 1 {
                    println!("usage: lobby open <lobby_id> [--execute]");
                    return Ok(());
                }
                let Some(lobby_id) = args.first() else {
                    println!("usage: lobby open <lobby_id> [--execute]");
                    return Ok(());
                };
                match lobby_open_plan(lobby_id) {
                    Ok(plan) => self.render_or_execute_nexus_get(&plan, execute, None),
                    Err(err) => {
                        println!("{err}");
                        Ok(())
                    }
                }
            }
            _ => {
                println!("usage: lobby list|open");
                Ok(())
            }
        }
    }

    fn render_or_execute_nexus_get(
        &self,
        plan: &ProtectedGetPlan,
        execute: bool,
        confirm: Option<&str>,
    ) -> io::Result<()> {
        let mutation_confirm = plan.mutation_confirm.as_deref();
        if !execute {
            println!("dry-run Nexus protected request:");
            println!("{}", plan.render_dry_run());
            if let Some(expected) = mutation_confirm {
                println!(
                    "MVP note: command was not executed. Add --execute, set {NEXUS_MUTATION_ENV}=1, pass --confirm {expected}, and set {}{} to run it.",
                    plan.token_env,
                    plan.fallback_token_env
                        .map(|env| format!(" or {env}"))
                        .unwrap_or_default()
                );
            } else {
                println!(
                    "MVP note: command was not executed. Add --execute, set SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1, and set {}{} to run it.",
                    plan.token_env,
                    plan.fallback_token_env
                        .map(|env| format!(" or {env}"))
                        .unwrap_or_default()
                );
            }
            return Ok(());
        }
        if let Some(expected) = mutation_confirm {
            if env::var(NEXUS_MUTATION_ENV).ok().as_deref() != Some("1") {
                println!("protected Nexus mutation blocked by environment gate");
                println!("set {NEXUS_MUTATION_ENV}=1 and rerun with --execute");
                println!("dry-run command:");
                println!("{}", plan.render_dry_run());
                return Ok(());
            }
            if confirm != Some(expected) {
                println!("protected Nexus mutation blocked by confirmation guard");
                println!("required confirmation: --confirm {expected}");
                println!("dry-run command:");
                println!("{}", plan.render_dry_run());
                return Ok(());
            }
        } else {
            if env::var("SEKAILINK_CORE_ACCESS_REMOTE_READONLY")
                .ok()
                .as_deref()
                != Some("1")
            {
                println!("protected Nexus read-only execution blocked by environment gate");
                println!("set SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1 and rerun with --execute");
                println!("dry-run command:");
                println!("{}", plan.render_dry_run());
                return Ok(());
            }
        }
        execute_protected_get(plan)
    }

    fn user_readonly_plan(&self, parsed: &[String]) -> io::Result<()> {
        let action = parsed.get(1).map(String::as_str).unwrap_or("");
        let execute = parsed.iter().any(|part| part == "--execute");
        let args = non_flag_args(parsed, 2);
        match identity_user_plan(action, &args) {
            Ok(plan) => {
                self.render_or_execute_nexus_get(&plan, execute, None)?;
            }
            Err(err) => println!("{err}"),
        }
        Ok(())
    }

    fn user_admin_mutation(&self, parsed: &[String]) -> io::Result<()> {
        let action = parsed.get(1).map(String::as_str).unwrap_or("");
        let execute = parsed.iter().any(|part| part == "--execute");
        let confirm = flag_value(parsed, "--confirm");
        let plan = match action {
            "create" => {
                let args = option_positionals(
                    parsed,
                    2,
                    &[
                        "--password-env",
                        "--display-name",
                        "--locale",
                        "--permissions",
                    ],
                );
                if args.len() < 3 {
                    println!(
                        "usage: user create <username> <email> <role> --password-env ENV [--display-name name] [--locale fr-CA] [--permissions a,b] [--email-verified] --confirm user:<username>:create [--execute]"
                    );
                    return Ok(());
                }
                let Some(password_env) = flag_value(parsed, "--password-env") else {
                    println!("user create requires --password-env ENV");
                    return Ok(());
                };
                identity_user_create_plan(
                    &args[0],
                    &args[1],
                    &args[2],
                    flag_value(parsed, "--display-name"),
                    flag_value(parsed, "--locale"),
                    flag_value(parsed, "--permissions"),
                    parsed.iter().any(|part| part == "--email-verified"),
                    password_env,
                )
            }
            "edit" => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                if args.len() < 2 {
                    println!(
                        "usage: user edit <username> key=value [key=value...] --confirm user:<username>:edit [--execute]"
                    );
                    return Ok(());
                }
                identity_user_edit_plan(&args[0], &args[1..])
            }
            "disable" => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                if args.len() != 1 {
                    println!(
                        "usage: user disable <username> --confirm user:<username>:disable [--execute]"
                    );
                    return Ok(());
                }
                identity_user_disable_plan(&args[0])
            }
            "revoke-sessions" => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                if args.len() != 1 {
                    println!(
                        "usage: user revoke-sessions <username> --confirm user:<username>:revoke-sessions [--execute]"
                    );
                    return Ok(());
                }
                identity_user_revoke_sessions_plan(&args[0])
            }
            "force-password-reset" => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                if args.len() != 1 {
                    println!(
                        "usage: user force-password-reset <username> --confirm user:<username>:force-password-reset [--execute]"
                    );
                    return Ok(());
                }
                identity_user_force_password_reset_plan(&args[0])
            }
            _ => Err(format!("unsupported user admin command: {action}")),
        };

        match plan {
            Ok(plan) => self.render_or_execute_nexus_get(&plan, execute, confirm)?,
            Err(err) => println!("{err}"),
        }
        Ok(())
    }

    fn user_config(&self, parsed: &[String]) -> io::Result<()> {
        let execute = parsed.iter().any(|part| part == "--execute");
        match (
            parsed.get(1).map(String::as_str),
            parsed.get(2).map(String::as_str),
        ) {
            (Some("configs"), Some(user_id)) => {
                let args = non_flag_args(parsed, 2);
                let game_key = args.get(1).map(String::as_str);
                match user_configs_plan(user_id, game_key) {
                    Ok(plan) => self.render_or_execute_nexus_get(&plan, execute, None),
                    Err(err) => {
                        println!("{err}");
                        Ok(())
                    }
                }
            }
            (Some("configs"), None) => {
                println!("usage: user configs <user_id> [game_key] [--execute]");
                Ok(())
            }
            (Some("config"), Some("open")) => {
                let args = non_flag_args(parsed, 3);
                if args.len() != 2 {
                    println!("usage: user config open <user_id> <config_id> [--execute]");
                    return Ok(());
                }
                match user_config_open_plan(&args[0], &args[1]) {
                    Ok(plan) => self.render_or_execute_nexus_get(&plan, execute, None),
                    Err(err) => {
                        println!("{err}");
                        Ok(())
                    }
                }
            }
            (Some("config"), Some("export")) => {
                let args = non_flag_args(parsed, 3);
                if args.len() < 2 {
                    println!(
                        "usage: user config export <user_id> <config_id> [--format yaml] [--execute]"
                    );
                    return Ok(());
                }
                let format = flag_value(parsed, "--format").unwrap_or("yaml");
                if format != "yaml" && format != "yml" {
                    println!("seed-config API currently supports YAML export only");
                    return Ok(());
                }
                match user_config_export_plan(&args[0], &args[1]) {
                    Ok(plan) => self.render_or_execute_nexus_get(&plan, execute, None),
                    Err(err) => {
                        println!("{err}");
                        Ok(())
                    }
                }
            }
            (Some("config"), Some("diff")) => {
                let args = non_flag_args(parsed, 3);
                if args.len() != 3 {
                    println!("usage: user config diff <user_id> <config_id> <version>");
                    return Ok(());
                }
                let detail = format!("config_id={} version={}", args[1], args[2]);
                let id = write_ops_draft(&self.session, "user-config", "diff", &args[0], &detail)?;
                println!("user config diff draft saved: {id}");
                println!("user_id: {}", args[0]);
                println!("config_id: {}", args[1]);
                println!("version: {}", args[2]);
                println!(
                    "MVP note: seed-config version diff route is not confirmed; no Nexus request was made."
                );
                Ok(())
            }
            (Some("config"), Some("edit")) => {
                let args = option_positionals(parsed, 3, &["--confirm"]);
                if args.len() < 3 {
                    println!(
                        "usage: user config edit <user_id> <config_id> key=value [key=value...] --confirm user-config:<user_id>:<config_id>:edit"
                    );
                    return Ok(());
                }
                let expected = format!("user-config:{}:{}:edit", args[0], args[1]);
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let detail = args[2..].join(" ");
                let target = format!("{}/{}", args[0], args[1]);
                let id = write_ops_draft(&self.session, "user-config", "edit", &target, &detail)?;
                println!("user config edit draft saved: {id}");
                println!("target: {target}");
                println!("detail: {detail}");
                println!("MVP note: canonical Nexus seed-config values were not changed.");
                Ok(())
            }
            _ => {
                println!("usage: user configs <user_id> [game_key]");
                println!("       user config open|diff|export|edit");
                Ok(())
            }
        }
    }

    fn note_add(&self, parsed: &[String]) -> io::Result<()> {
        let Some(target) = parsed.get(2) else {
            println!("usage: note add <target> <text>");
            return Ok(());
        };
        let text = if parsed.len() > 3 {
            parsed[3..].join(" ")
        } else {
            String::new()
        };
        if text.trim().is_empty() {
            println!("usage: note add <target> <text>");
            return Ok(());
        }
        let id = append_note(&self.session, target, &text)?;
        println!("note created: {id}");
        Ok(())
    }

    fn note_list(&self, query: &str) -> io::Result<()> {
        let text = read_file_to_string(&self.session.notes_path())?;
        if text.is_empty() {
            println!("notes are empty");
            return Ok(());
        }
        let mut count = 0_usize;
        for line in text
            .lines()
            .filter(|line| query.is_empty() || line.contains(query))
            .take(200)
        {
            println!("{line}");
            count += 1;
        }
        if count == 0 {
            println!("no notes matched: {query}");
        }
        Ok(())
    }

    fn approval(&self, parsed: &[String], raw: &str) -> io::Result<()> {
        match parsed.get(1).map(String::as_str) {
            Some("request") => {
                let Some(command) = parsed.get(2) else {
                    println!("usage: approval request <command> <reason>");
                    return Ok(());
                };
                let reason = if parsed.len() > 3 {
                    parsed[3..].join(" ")
                } else {
                    "no reason provided".to_string()
                };
                let id = append_approval_request(&self.session, command, &reason)?;
                println!("approval requested: {id}");
            }
            Some("list") => {
                let text = read_file_to_string(&self.session.approvals_path())?;
                if text.is_empty() {
                    println!("approval queue is empty");
                } else {
                    for line in text.lines().take(200) {
                        println!("{line}");
                    }
                }
            }
            Some("approve") => {
                if self.session.role != Role::Admin {
                    println!("approval approve requires Admin");
                    append_audit(
                        &self.session,
                        raw,
                        "rbac_denied",
                        "approval approve requires Admin",
                    )?;
                    return Ok(());
                }
                let Some(id) = parsed.get(2) else {
                    println!("usage: approval approve <id>");
                    return Ok(());
                };
                let (pending, approved) = self.approval_state(id)?;
                if !pending {
                    println!("approval not found in local pending queue: {id}");
                    println!("Use: approval list");
                    return Ok(());
                }
                if approved {
                    println!("approval already marked approved locally: {id}");
                    return Ok(());
                }
                let reason = if parsed.len() > 3 {
                    parsed[3..].join(" ")
                } else {
                    "approved locally".to_string()
                };
                append_approval_decision(&self.session, id, "approved", &reason)?;
                println!("approval marked approved locally: {id}");
                println!("MVP note: execution handoff to Nexus approval service is pending.");
            }
            _ => println!("usage: approval request|list|approve"),
        }
        Ok(())
    }

    fn approval_state(&self, id: &str) -> io::Result<(bool, bool)> {
        let needle = format!("\"id\":\"{id}\"");
        let text = read_file_to_string(&self.session.approvals_path())?;
        let mut pending = false;
        let mut approved = false;
        for line in text.lines().filter(|line| line.contains(&needle)) {
            if line.contains("\"state\":\"pending\"") {
                pending = true;
            }
            if line.contains("\"state\":\"approved\"") {
                approved = true;
            }
        }
        Ok((pending, approved))
    }

    fn incident(&self, parsed: &[String]) -> io::Result<()> {
        match parsed.get(1).map(String::as_str) {
            Some("open") => self.incident_open(parsed),
            Some("list") => self.incident_list(parsed.get(2).map(String::as_str).unwrap_or("")),
            Some("status") => self.incident_status(parsed),
            Some("note") => self.incident_note(parsed),
            Some("pin") => self.incident_pin(parsed),
            Some("export") => self.incident_export(parsed),
            Some("close") => self.incident_close(parsed),
            _ => {
                println!("usage: incident open|list|status|note|pin|export|close");
                Ok(())
            }
        }
    }

    fn incident_open(&self, parsed: &[String]) -> io::Result<()> {
        let Some(label) = parsed.get(2) else {
            println!("usage: incident open <label> <severity> <summary>");
            return Ok(());
        };
        let label = match normalize_incident_label(label) {
            Ok(label) => label,
            Err(err) => {
                println!("{err}");
                return Ok(());
            }
        };
        let severity = parsed.get(3).map(String::as_str).unwrap_or("sev3");
        let summary = if parsed.len() > 4 {
            parsed[4..].join(" ")
        } else {
            "opened".to_string()
        };
        let id = append_incident_event(&self.session, &label, "open", severity, &summary)?;
        println!("incident opened: {label}");
        println!("event: {id}");
        println!("severity: {severity}");
        Ok(())
    }

    fn incident_list(&self, query: &str) -> io::Result<()> {
        let text = read_file_to_string(&self.session.incident_events_path())?;
        if text.trim().is_empty() {
            println!("incident event log is empty");
            return Ok(());
        }
        let mut count = 0_usize;
        for line in text
            .lines()
            .filter(|line| query.is_empty() || line.contains(query))
            .take(300)
        {
            println!("{line}");
            count += 1;
        }
        if count == 0 {
            println!("no incident events matched: {query}");
        }
        Ok(())
    }

    fn incident_status(&self, parsed: &[String]) -> io::Result<()> {
        let Some(label) = parsed.get(2) else {
            println!("usage: incident status <label>");
            return Ok(());
        };
        let label = match normalize_incident_label(label) {
            Ok(label) => label,
            Err(err) => {
                println!("{err}");
                return Ok(());
            }
        };
        let events = read_file_to_string(&self.session.incident_events_path())?;
        let notes = read_file_to_string(&self.session.notes_path())?;
        let pins = read_file_to_string(&self.session.log_pins_path())?;
        let event_lines = incident_event_lines(&events, &label).collect::<Vec<_>>();
        if event_lines.is_empty() {
            println!("incident not found locally: {label}");
            println!("Use: incident open {label} sev3 <summary>");
            return Ok(());
        }
        println!("incident: {label}");
        println!("state: {}", incident_state(&event_lines));
        println!();
        println!("Recent events:");
        for line in event_lines.iter().rev().take(12).rev() {
            println!("{line}");
        }
        print_related_section(
            "Related notes",
            incident_note_lines(&notes, &label).collect(),
        );
        print_related_section("Related pins", incident_pin_lines(&pins, &label).collect());
        Ok(())
    }

    fn incident_note(&self, parsed: &[String]) -> io::Result<()> {
        let Some(label) = parsed.get(2) else {
            println!("usage: incident note <label> <text>");
            return Ok(());
        };
        let label = match normalize_incident_label(label) {
            Ok(label) => label,
            Err(err) => {
                println!("{err}");
                return Ok(());
            }
        };
        let text = if parsed.len() > 3 {
            parsed[3..].join(" ")
        } else {
            String::new()
        };
        if text.trim().is_empty() {
            println!("usage: incident note <label> <text>");
            return Ok(());
        }
        let note_id = append_note(&self.session, &format!("incident:{label}"), &text)?;
        let event_id = append_incident_event(&self.session, &label, "note", "", &text)?;
        println!("incident note created: {note_id}");
        println!("event: {event_id}");
        Ok(())
    }

    fn incident_pin(&self, parsed: &[String]) -> io::Result<()> {
        let Some(label) = parsed.get(2) else {
            println!("usage: incident pin <label> <source> <text>");
            return Ok(());
        };
        let label = match normalize_incident_label(label) {
            Ok(label) => label,
            Err(err) => {
                println!("{err}");
                return Ok(());
            }
        };
        let Some(source) = parsed.get(3) else {
            println!("usage: incident pin <label> <source> <text>");
            return Ok(());
        };
        let text = if parsed.len() > 4 {
            parsed[4..].join(" ")
        } else {
            String::new()
        };
        if text.trim().is_empty() {
            println!("usage: incident pin <label> <source> <text>");
            return Ok(());
        }
        let pin_text = format!("incident:{label} {text}");
        let pin_id = append_log_pin(&self.session, source, &pin_text)?;
        let event_detail = format!("source={source} text={text}");
        let event_id = append_incident_event(&self.session, &label, "pin", "", &event_detail)?;
        println!("incident pin created: {pin_id}");
        println!("event: {event_id}");
        Ok(())
    }

    fn incident_export(&self, parsed: &[String]) -> io::Result<()> {
        let Some(label) = parsed.get(2) else {
            println!("usage: incident export <label> [--file name]");
            return Ok(());
        };
        let label = match normalize_incident_label(label) {
            Ok(label) => label,
            Err(err) => {
                println!("{err}");
                return Ok(());
            }
        };
        let events = read_file_to_string(&self.session.incident_events_path())?;
        let notes = read_file_to_string(&self.session.notes_path())?;
        let pins = read_file_to_string(&self.session.log_pins_path())?;
        let event_lines = incident_event_lines(&events, &label).collect::<Vec<_>>();
        if event_lines.is_empty() {
            println!("incident not found locally: {label}");
            return Ok(());
        }
        let file_name = incident_export_file_name(parsed, &label);
        let body = render_incident_export(&label, &events, &notes, &pins);
        let path =
            write_export_with_extension(&self.session, "incident", Some(&file_name), "md", &body)?;
        println!("incident exported to {}", path.display());
        println!("MVP note: export is local; no Nexus DB incident record was changed.");
        Ok(())
    }

    fn incident_close(&self, parsed: &[String]) -> io::Result<()> {
        let Some(label) = parsed.get(2) else {
            println!("usage: incident close <label> <resolution>");
            return Ok(());
        };
        let label = match normalize_incident_label(label) {
            Ok(label) => label,
            Err(err) => {
                println!("{err}");
                return Ok(());
            }
        };
        let resolution = if parsed.len() > 3 {
            parsed[3..].join(" ")
        } else {
            "closed".to_string()
        };
        let id = append_incident_event(&self.session, &label, "close", "", &resolution)?;
        println!("incident closed locally: {label}");
        println!("event: {id}");
        Ok(())
    }

    fn ops(&self, parsed: &[String]) -> io::Result<()> {
        match parsed.get(1).map(String::as_str) {
            Some("snapshot") => self.ops_snapshot(parsed.get(2).map(String::as_str)),
            Some("timeline") => self.ops_timeline(parsed.get(2).map(String::as_str).unwrap_or("")),
            Some("handoff") => self.ops_handoff(parsed),
            Some("doctor") => self.ops_doctor(parsed),
            Some("paths") => {
                self.ops_paths();
                Ok(())
            }
            Some("exports") => self.ops_exports(parsed.get(2).map(String::as_str).unwrap_or("")),
            _ => {
                println!("usage: ops snapshot|timeline|handoff|doctor|paths|exports");
                Ok(())
            }
        }
    }

    fn ops_doctor(&self, parsed: &[String]) -> io::Result<()> {
        let verbose = parsed
            .iter()
            .any(|part| part == "--verbose" || part == "-v");
        let checks = self.doctor_checks(verbose);
        let ok = checks
            .iter()
            .filter(|check| check.level == DoctorLevel::Ok)
            .count();
        let warn = checks
            .iter()
            .filter(|check| check.level == DoctorLevel::Warn)
            .count();
        let fail = checks
            .iter()
            .filter(|check| check.level == DoctorLevel::Fail)
            .count();

        println!("SekaiLink Core Access doctor");
        println!("session: {}", self.session.session_id);
        println!(
            "role: {} | user: {}",
            self.session.role.as_str(),
            self.session.sekailink_user
        );
        println!("summary: ok={ok} warn={warn} fail={fail}");
        println!();
        println!("{:<7} {:<18} DETAIL", "STATUS", "CHECK");
        println!("{}", "-".repeat(92));
        for check in checks {
            if verbose || check.level != DoctorLevel::Ok {
                println!(
                    "{:<7} {:<18} {}",
                    check.level.as_str(),
                    check.name,
                    check.detail
                );
            }
        }
        if !verbose {
            println!();
            println!("Use `ops doctor --verbose` to show passing checks too.");
        }
        println!("No server mutation was performed.");
        Ok(())
    }

    fn doctor_checks(&self, verbose: bool) -> Vec<DoctorCheck> {
        let mut checks = Vec::new();
        let repo_root = repo_root_path();
        let docs_root = docs_root_path();
        let pdf_root = docs_root.join("dist").join("pdf");

        checks.push(path_check("repo-root", &repo_root, true));
        checks.push(path_check("data-dir", &self.session.data_dir, true));
        checks.push(path_check("audit-dir", &self.session.audit_dir(), true));
        checks.push(path_check("exports-dir", &self.session.exports_dir(), true));
        checks.push(path_check("docs-root", &docs_root, true));
        checks.push(path_check("pdf-root", &pdf_root, true));

        for (label, path) in [
            (
                "audit-store",
                self.session.audit_dir().join("core-access.jsonl"),
            ),
            ("notes-store", self.session.notes_path()),
            ("pins-store", self.session.log_pins_path()),
            ("incidents-store", self.session.incident_events_path()),
            ("diagnostics-store", self.session.client_diagnostics_path()),
            ("approvals-store", self.session.approvals_path()),
        ] {
            checks.push(file_store_check(label, &path));
        }

        for name in [
            "git",
            "cargo",
            "ssh",
            "stty",
            "less",
            "journalctl",
            "xelatex",
            "pandoc",
            "pdftotext",
            "pdfinfo",
        ] {
            checks.push(tool_check(name));
        }

        for file in [
            "sekailink-core-access-full.pdf",
            "sekailink-core-access-service-training.pdf",
            "sekailink-core-access-command-reference.pdf",
            "sekailink-core-access-incident-playbooks.pdf",
            "sekailink-core-access-quick-reference.pdf",
        ] {
            checks.push(pdf_check(file, &pdf_root.join(file)));
        }

        for (name, sensitive) in [
            ("SEKAILINK_CORE_ACCESS_USER", false),
            ("SEKAILINK_CORE_ACCESS_ROLE", false),
            ("SEKAILINK_CORE_ACCESS_REMOTE_READONLY", false),
            ("SEKAILINK_CORE_ACCESS_REMOTE_MUTATION", false),
            (NEXUS_MUTATION_ENV, false),
            ("SEKAILINK_CORE_ACCESS_AGENT_ADMIN_TOKEN", true),
            ("SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN", true),
            ("SEKAILINK_CORE_ACCESS_NEXUS_AGENT_ADMIN_TOKEN", true),
            ("SEKAILINK_CORE_ACCESS_LINK_AGENT_ADMIN_TOKEN", true),
            ("SEKAILINK_CORE_ACCESS_WORLDS_AGENT_ADMIN_TOKEN", true),
            ("SEKAILINK_CORE_ACCESS_EVOLUTION_AGENT_ADMIN_TOKEN", true),
            ("SEKAILINK_CORE_ACCESS_NEXUS_LOBBY_ADMIN_TOKEN", true),
            ("SEKAILINK_CORE_ACCESS_NEXUS_IDENTITY_ADMIN_TOKEN", true),
        ] {
            checks.push(env_check(name, sensitive));
        }

        if verbose {
            checks.push(DoctorCheck::ok(
                "log-sources",
                &format!("{} source(s)", log_catalog().len()),
            ));
            checks.push(DoctorCheck::ok(
                "service-targets",
                &format!("{} server(s)", known_services().len()),
            ));
            checks.push(DoctorCheck::ok(
                "exports-count",
                &format!(
                    "{} file(s)",
                    count_export_files(&self.session.exports_dir())
                ),
            ));
        }

        checks
    }

    fn ops_paths(&self) {
        println!("{:<22} PATH", "NAME");
        println!("{}", "-".repeat(96));
        for (name, path) in self.core_access_paths() {
            println!("{:<22} {}", name, path.display());
        }
        println!("Secrets are not printed by this command.");
    }

    fn core_access_paths(&self) -> Vec<(&'static str, PathBuf)> {
        vec![
            ("data-dir", self.session.data_dir.clone()),
            ("audit", self.session.audit_dir().join("core-access.jsonl")),
            ("notes", self.session.notes_path()),
            ("log-pins", self.session.log_pins_path()),
            ("incidents", self.session.incident_events_path()),
            ("client-diagnostics", self.session.client_diagnostics_path()),
            ("approvals", self.session.approvals_path()),
            ("history", self.session.history_path()),
            ("exports", self.session.exports_dir()),
            ("client-banners", self.session.client_banners_dir()),
            ("maintenance", self.session.maintenance_dir()),
            ("scheduler", self.session.scheduler_dir()),
            ("pack-repos", self.session.pack_repos_dir()),
            ("releases", self.session.releases_dir()),
            ("drafts", self.session.drafts_dir()),
            ("docs", docs_root_path()),
            ("pdfs", docs_root_path().join("dist").join("pdf")),
        ]
    }

    fn ops_exports(&self, query: &str) -> io::Result<()> {
        let mut exports = export_entries(&self.session.exports_dir())?;
        if !query.trim().is_empty() {
            let clean = query.to_ascii_lowercase();
            exports.retain(|entry| entry.name.to_ascii_lowercase().contains(&clean));
        }
        if exports.is_empty() {
            if query.trim().is_empty() {
                println!("no local exports found");
            } else {
                println!("no local exports matched: {query}");
            }
            return Ok(());
        }
        exports.sort_by_key(|entry| std::cmp::Reverse(entry.modified));
        println!("{:<12} {:>10} FILE", "MODIFIED", "SIZE");
        println!("{}", "-".repeat(96));
        for entry in exports.iter().take(200) {
            println!(
                "{:<12} {:>10} {}",
                entry.modified,
                format_bytes(entry.size),
                entry.path.display()
            );
        }
        if exports.len() > 200 {
            println!("... {} more export(s)", exports.len() - 200);
        }
        Ok(())
    }

    fn ops_timeline(&self, query: &str) -> io::Result<()> {
        let mut entries = self.ops_timeline_entries()?;
        if !query.trim().is_empty() {
            entries.retain(|entry| entry.line.contains(query));
        }
        if entries.is_empty() {
            if query.trim().is_empty() {
                println!("ops timeline is empty");
            } else {
                println!("no ops timeline entries matched: {query}");
            }
            return Ok(());
        }
        entries.sort_by_key(|entry| entry.ts);
        let start = entries.len().saturating_sub(200);
        println!("{:<12} {:<10} ENTRY", "TS", "SOURCE");
        println!("{}", "-".repeat(96));
        for entry in &entries[start..] {
            println!("{:<12} {:<10} {}", entry.ts, entry.source, entry.line);
        }
        Ok(())
    }

    fn ops_handoff(&self, parsed: &[String]) -> io::Result<()> {
        let label = parsed
            .iter()
            .skip(2)
            .find(|part| !part.starts_with("--") && !looks_like_export_file(part))
            .map(String::as_str)
            .unwrap_or("shift-handoff");
        let requested_name = handoff_file_name(parsed, label);
        let body = self.render_handoff_report(label)?;
        let path = write_export_with_extension(
            &self.session,
            "handoff",
            Some(&requested_name),
            "md",
            &body,
        )?;
        println!("handoff written to {}", path.display());
        println!("MVP note: handoff is local; no Nexus DB record was changed.");
        Ok(())
    }

    fn render_handoff_report(&self, label: &str) -> io::Result<String> {
        let audit = read_file_to_string(&self.session.audit_dir().join("core-access.jsonl"))?;
        let notes = read_file_to_string(&self.session.notes_path())?;
        let pins = read_file_to_string(&self.session.log_pins_path())?;
        let incidents = read_file_to_string(&self.session.incident_events_path())?;
        let approvals = read_file_to_string(&self.session.approvals_path())?;
        let maintenance = read_file_to_string(&self.session.maintenance_dir().join("current.txt"))?;
        let maintenance_history =
            read_file_to_string(&self.session.maintenance_dir().join("history.jsonl"))?;
        let schedule = read_file_to_string(&self.session.scheduler_dir().join("jobs.jsonl"))?;
        let pack_repos = read_file_to_string(&self.session.pack_repos_dir().join("repos.jsonl"))?;
        let release_drafts =
            read_file_to_string(&self.session.releases_dir().join("drafts.jsonl"))?;
        let ops_drafts = read_draft_files(&self.session.drafts_dir())?;
        let diagnostics = read_file_to_string(&self.session.client_diagnostics_path())?;
        let mut timeline = self.ops_timeline_entries()?;
        timeline.sort_by_key(|entry| entry.ts);

        let mut body = String::new();
        body.push_str(&format!("# SekaiLink Core Access Handoff - {label}\n\n"));
        body.push_str("## Session\n\n");
        body.push_str(&format!("- Linux user: {}\n", self.session.linux_user));
        body.push_str(&format!(
            "- SekaiLink user: {}\n",
            self.session.sekailink_user
        ));
        body.push_str(&format!("- Role: {}\n", self.session.role.as_str()));
        body.push_str(&format!("- Session: {}\n\n", self.session.session_id));

        body.push_str("## Dashboard\n\n```text\n");
        body.push_str(&render_dashboard());
        body.push_str("```\n\n");

        body.push_str("## Open Incident Labels\n\n");
        push_open_incident_labels(&mut body, &incidents);

        body.push_str("\n## Recent Ops Timeline\n\n```text\n");
        let start = timeline.len().saturating_sub(80);
        for entry in &timeline[start..] {
            body.push_str(&format!(
                "{:<12} {:<10} {}\n",
                entry.ts, entry.source, entry.line
            ));
        }
        body.push_str("```\n\n");

        push_json_section_from_text(&mut body, "Recent Incident Events", &incidents, 80);
        push_json_section_from_text(&mut body, "Recent Notes", &notes, 60);
        push_json_section_from_text(&mut body, "Recent Log Pins", &pins, 60);
        push_json_section_from_text(&mut body, "Approval Queue", &approvals, 80);

        body.push_str("## Maintenance Draft\n\n```text\n");
        if maintenance.trim().is_empty() {
            body.push_str("(none)\n");
        } else {
            body.push_str(&maintenance);
            if !maintenance.ends_with('\n') {
                body.push('\n');
            }
        }
        body.push_str("```\n\n");

        push_json_section_from_text(&mut body, "Maintenance History", &maintenance_history, 40);
        push_json_section_from_text(&mut body, "Schedule Drafts", &schedule, 60);
        push_json_section_from_text(&mut body, "Pack Repo Drafts", &pack_repos, 60);
        push_json_section_from_text(&mut body, "Release Drafts", &release_drafts, 60);
        push_json_section_from_text(&mut body, "Ops Drafts", &ops_drafts, 80);
        push_json_section_from_text(&mut body, "Client Diagnostics Requests", &diagnostics, 60);
        body.push_str("## Client Banner Drafts\n\n```text\n");
        for slot in 1..=3 {
            let text = self.read_client_banner_slot(slot)?;
            let display = if text.trim().is_empty() {
                "(empty)"
            } else {
                text.trim()
            };
            body.push_str(&format!("slot {slot}: {display}\n"));
        }
        body.push_str("```\n\n");

        push_json_section_from_text(&mut body, "Recent Audit", &audit, 80);
        Ok(body)
    }

    fn ops_timeline_entries(&self) -> io::Result<Vec<TimelineEntry>> {
        let audit = read_file_to_string(&self.session.audit_dir().join("core-access.jsonl"))?;
        let notes = read_file_to_string(&self.session.notes_path())?;
        let pins = read_file_to_string(&self.session.log_pins_path())?;
        let incidents = read_file_to_string(&self.session.incident_events_path())?;
        let approvals = read_file_to_string(&self.session.approvals_path())?;
        let maintenance =
            read_file_to_string(&self.session.maintenance_dir().join("history.jsonl"))?;
        let schedule = read_file_to_string(&self.session.scheduler_dir().join("jobs.jsonl"))?;
        let pack_repos = read_file_to_string(&self.session.pack_repos_dir().join("repos.jsonl"))?;
        let release_drafts =
            read_file_to_string(&self.session.releases_dir().join("drafts.jsonl"))?;
        let ops_drafts = read_draft_files(&self.session.drafts_dir())?;
        let diagnostics = read_file_to_string(&self.session.client_diagnostics_path())?;
        let mut entries = Vec::new();
        push_timeline_entries(&mut entries, "audit", &audit);
        push_timeline_entries(&mut entries, "note", &notes);
        push_timeline_entries(&mut entries, "pin", &pins);
        push_timeline_entries(&mut entries, "incident", &incidents);
        push_timeline_entries(&mut entries, "approval", &approvals);
        push_timeline_entries(&mut entries, "maintenance", &maintenance);
        push_timeline_entries(&mut entries, "schedule", &schedule);
        push_timeline_entries(&mut entries, "pack", &pack_repos);
        push_timeline_entries(&mut entries, "release", &release_drafts);
        push_timeline_entries(&mut entries, "draft", &ops_drafts);
        push_timeline_entries(&mut entries, "diagnostics", &diagnostics);
        Ok(entries)
    }

    fn ops_snapshot(&self, label: Option<&str>) -> io::Result<()> {
        let title = label.unwrap_or("incident").trim();
        let file_name = format!("{title}.md");
        let audit = read_file_to_string(&self.session.audit_dir().join("core-access.jsonl"))?;
        let notes = read_file_to_string(&self.session.notes_path())?;
        let approvals = read_file_to_string(&self.session.approvals_path())?;
        let mut body = String::new();

        body.push_str(&format!("# SekaiLink Core Access Snapshot - {title}\n\n"));
        body.push_str("## Session\n\n");
        body.push_str(&format!("- Linux user: {}\n", self.session.linux_user));
        body.push_str(&format!(
            "- SekaiLink user: {}\n",
            self.session.sekailink_user
        ));
        body.push_str(&format!("- Role: {}\n", self.session.role.as_str()));
        body.push_str(&format!("- Session: {}\n\n", self.session.session_id));

        body.push_str("## Dashboard\n\n```text\n");
        body.push_str(&render_dashboard());
        body.push_str("```\n\n");

        body.push_str("## Log Sources\n\n");
        for (source, server, description) in log_catalog() {
            body.push_str(&format!("- `{source}` on {server}: {description}\n"));
        }

        body.push_str("\n## Services\n\n");
        for (server, services) in known_services() {
            body.push_str(&format!("- `{server}`: {}\n", services.join(", ")));
        }

        body.push_str("\n## Recent Audit\n\n```json\n");
        push_recent_lines(&mut body, &audit, 50);
        body.push_str("```\n\n## Recent Notes\n\n```json\n");
        push_recent_lines(&mut body, &notes, 50);
        body.push_str("```\n\n## Approval Queue\n\n```json\n");
        push_recent_lines(&mut body, &approvals, 50);
        body.push_str("```\n");

        let path = write_export(&self.session, "snapshot", Some(&file_name), &body)?;
        println!("snapshot written to {}", path.display());
        println!("MVP note: snapshot is local, not a Nexus DB incident record yet.");
        Ok(())
    }

    fn client_diagnostics(&self, parsed: &[String]) -> io::Result<()> {
        match parsed.get(1).map(String::as_str) {
            Some("diagnostics-request") => self.client_diagnostics_request(parsed),
            Some("diagnostics-list") => {
                self.client_diagnostics_list(parsed.get(2).map(String::as_str).unwrap_or(""))
            }
            Some("diagnostics-export") => self.client_diagnostics_export(parsed),
            _ => {
                println!("usage: client diagnostics-request|diagnostics-list|diagnostics-export");
                Ok(())
            }
        }
    }

    fn client_diagnostics_request(&self, parsed: &[String]) -> io::Result<()> {
        let args = DiagnosticsArgs::from_parts(parsed);
        let Some(user) = args.positionals.first() else {
            println!(
                "usage: client diagnostics-request <user> <incident> <reason> [--include client-core,sekaiemu,sklmi,configs,system]"
            );
            return Ok(());
        };
        let Some(incident) = args.positionals.get(1) else {
            println!(
                "usage: client diagnostics-request <user> <incident> <reason> [--include client-core,sekaiemu,sklmi,configs,system]"
            );
            return Ok(());
        };
        let reason = if args.positionals.len() > 2 {
            args.positionals[2..].join(" ")
        } else {
            String::new()
        };
        if reason.trim().is_empty() {
            println!(
                "usage: client diagnostics-request <user> <incident> <reason> [--include client-core,sekaiemu,sklmi,configs,system]"
            );
            return Ok(());
        }
        let id = append_client_diagnostics_request(
            &self.session,
            user,
            incident,
            &reason,
            &args.include,
        )?;
        println!("client diagnostics request drafted: {id}");
        println!("target user: {user}");
        println!("incident: {incident}");
        println!("include: {}", args.include.join(", "));
        println!("consent required: user must approve before any client-side upload");
        println!(
            "MVP note: draft only; no client signal was sent and no Client Core/Sekaiemu/SKLMI code changed."
        );
        println!();
        print_client_diagnostics_contract();
        Ok(())
    }

    fn client_diagnostics_list(&self, query: &str) -> io::Result<()> {
        let text = read_file_to_string(&self.session.client_diagnostics_path())?;
        if text.trim().is_empty() {
            println!("client diagnostics request queue is empty");
            return Ok(());
        }
        let mut count = 0_usize;
        for line in text
            .lines()
            .filter(|line| query.is_empty() || line.contains(query))
            .take(200)
        {
            println!("{line}");
            count += 1;
        }
        if count == 0 {
            println!("no client diagnostics requests matched: {query}");
        }
        Ok(())
    }

    fn client_diagnostics_export(&self, parsed: &[String]) -> io::Result<()> {
        let options = LogExportOptions::from_parts(parsed);
        let requests = read_file_to_string(&self.session.client_diagnostics_path())?;
        let matching = matching_lines(&requests, &options.query).collect::<Vec<_>>();
        if matching.is_empty() {
            println!("nothing to export from local client diagnostics requests");
            return Ok(());
        }
        let mut body = String::new();
        body.push_str("# SekaiLink Client Diagnostics Requests\n\n");
        if !options.query.trim().is_empty() {
            body.push_str(&format!("Filter: `{}`\n\n", options.query));
        }
        push_json_section(&mut body, "Requests", &matching);
        body.push_str("## Expected Client Bundle Contract\n\n");
        body.push_str(client_diagnostics_contract_markdown());
        let path = write_export_with_extension(
            &self.session,
            "client-diagnostics",
            options.file_name.as_deref(),
            "md",
            &body,
        )?;
        println!("client diagnostics requests exported to {}", path.display());
        println!("MVP note: export is local; no client upload was fetched.");
        Ok(())
    }

    fn client_signal_draft(&self, parsed: &[String]) -> io::Result<()> {
        let action = parsed.get(1).map(String::as_str).unwrap_or("");
        let args = option_positionals(parsed, 2, &["--confirm"]);
        let requires_confirm = !matches!(action, "refresh-lobby" | "refresh-room");
        let usage = match action {
            "broadcast" => {
                "usage: client broadcast <user|all> <message> --confirm client:broadcast:<target>"
            }
            "maintenance-banner" => {
                "usage: client maintenance-banner <user|all> <message> --confirm client:maintenance-banner:<target>"
            }
            "force-update-prompt" => {
                "usage: client force-update-prompt <user|all> <version|message> --confirm client:force-update-prompt:<target>"
            }
            "request-relogin" => {
                "usage: client request-relogin <user|all> <reason> --confirm client:request-relogin:<target>"
            }
            "refresh-lobby" => "usage: client refresh-lobby <user|all> [lobby-id]",
            "refresh-room" => "usage: client refresh-room <user|all> [room-id]",
            "request-sklmi-reconnect" => {
                "usage: client request-sklmi-reconnect <user|all> <reason> --confirm client:request-sklmi-reconnect:<target>"
            }
            "restart-runtime" => {
                "usage: client restart-runtime <user|all> <reason> --confirm client:restart-runtime:<target>"
            }
            "clear-cache-request" => {
                "usage: client clear-cache-request <user|all> <reason> --confirm client:clear-cache-request:<target>"
            }
            _ => "usage: client <signal> <target> [detail]",
        };
        let Some(target) = args.first().map(String::as_str) else {
            println!("{usage}");
            return Ok(());
        };
        let detail = if args.len() > 1 {
            args[1..].join(" ")
        } else if matches!(action, "refresh-lobby" | "refresh-room") {
            "refresh requested".to_string()
        } else {
            String::new()
        };
        if detail.trim().is_empty() {
            println!("{usage}");
            return Ok(());
        }
        if requires_confirm {
            let expected = format!("client:{action}:{target}");
            if !confirmation_matches(parsed, &expected) {
                return Ok(());
            }
        }
        let mut stored_detail = detail;
        if action == "clear-cache-request" {
            stored_detail = format!("approval_required=true {stored_detail}");
        }
        let id = write_ops_draft(
            &self.session,
            "client-signal",
            action,
            target,
            &stored_detail,
        )?;
        println!("client signal draft saved: {id}");
        println!("action: {action}");
        println!("target: {target}");
        println!("detail: {stored_detail}");
        println!("MVP note: no realtime client signal was sent.");
        if matches!(action, "request-sklmi-reconnect" | "restart-runtime") {
            println!("SKLMI/Sekaiemu note: no SKLMI, Sekaiemu, or Client Core code changed.");
        }
        Ok(())
    }

    fn broadcast_draft(&self, parsed: &[String]) -> io::Result<()> {
        let scope = parsed.get(1).map(String::as_str).unwrap_or("");
        let args = option_positionals(parsed, 2, &["--confirm"]);
        let (target, message_start) = match scope {
            "global" => ("all", 0),
            "server" | "lobby" | "room" | "role" | "version" | "game" => {
                let Some(target) = args.first().map(String::as_str) else {
                    println!(
                        "usage: broadcast {scope} <target> <message> --confirm broadcast:{scope}:<target>"
                    );
                    return Ok(());
                };
                (target, 1)
            }
            _ => {
                println!("usage: broadcast global|server|lobby|room|role|version|game");
                return Ok(());
            }
        };
        let message = if args.len() > message_start {
            args[message_start..].join(" ")
        } else {
            String::new()
        };
        if message.trim().is_empty() {
            if scope == "global" {
                println!("usage: broadcast global <message> --confirm broadcast:global:all");
            } else {
                println!(
                    "usage: broadcast {scope} <target> <message> --confirm broadcast:{scope}:<target>"
                );
            }
            return Ok(());
        }
        let expected = format!("broadcast:{scope}:{target}");
        if !confirmation_matches(parsed, &expected) {
            return Ok(());
        }
        let id = write_ops_draft(&self.session, "broadcast", scope, target, &message)?;
        println!("broadcast draft saved: {id}");
        println!("scope: {scope}");
        println!("target: {target}");
        println!("message: {message}");
        println!("MVP note: no broadcast was sent to clients, lobbies, rooms, Discord, or Twitch.");
        Ok(())
    }

    fn client_banner(&self, parsed: &[String]) -> io::Result<()> {
        match parsed.get(1).map(String::as_str) {
            Some("list") => self.client_banner_list(),
            Some("preview") => {
                let Some(slot) = parsed.get(2) else {
                    println!("usage: client-banner preview <1|2|3>");
                    return Ok(());
                };
                let slot = match parse_banner_slot(slot) {
                    Ok(slot) => slot,
                    Err(err) => {
                        println!("{err}");
                        return Ok(());
                    }
                };
                self.client_banner_preview(slot)
            }
            Some("edit") => {
                let Some(slot) = parsed.get(2) else {
                    println!("usage: client-banner edit <1|2|3> <text>");
                    return Ok(());
                };
                let slot = match parse_banner_slot(slot) {
                    Ok(slot) => slot,
                    Err(err) => {
                        println!("{err}");
                        return Ok(());
                    }
                };
                let text = if parsed.len() > 3 {
                    parsed[3..].join(" ")
                } else {
                    String::new()
                };
                if text.trim().is_empty() {
                    println!("usage: client-banner edit <1|2|3> <text>");
                    return Ok(());
                }
                let id = write_client_banner_draft(&self.session, slot, &text)?;
                println!("client banner draft saved: {id}");
                println!("slot {slot}: {text}");
                println!("MVP note: draft only; client-banner publish is not implemented.");
                Ok(())
            }
            Some("publish" | "rollback" | "disable") => {
                let action = parsed.get(1).map(String::as_str).unwrap_or("");
                let Some(slot) = parsed.get(2) else {
                    println!(
                        "usage: client-banner {action} <1|2|3> --confirm banner:<slot>:{action}"
                    );
                    return Ok(());
                };
                let slot = match parse_banner_slot(slot) {
                    Ok(slot) => slot,
                    Err(err) => {
                        println!("{err}");
                        return Ok(());
                    }
                };
                let expected = format!("banner:{slot}:{action}");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let text = self.read_client_banner_slot(slot)?;
                let detail = if text.trim().is_empty() {
                    "(empty local slot)".to_string()
                } else {
                    text.trim().to_string()
                };
                let id = write_ops_draft(
                    &self.session,
                    "client-banner",
                    action,
                    &slot.to_string(),
                    &detail,
                )?;
                println!("client banner {action} draft saved: {id}");
                println!("slot {slot}: {detail}");
                println!("MVP note: client dashboard banner config was not published.");
                Ok(())
            }
            _ => {
                println!("usage: client-banner list|preview|edit|publish|rollback|disable");
                Ok(())
            }
        }
    }

    fn client_banner_list(&self) -> io::Result<()> {
        println!("{:<6} Draft", "SLOT");
        println!("{}", "-".repeat(76));
        for slot in 1..=3 {
            let text = self.read_client_banner_slot(slot)?;
            let display = if text.trim().is_empty() {
                "(empty local draft)".to_string()
            } else {
                text
            };
            println!("{slot:<6} {display}");
        }
        println!("MVP note: local drafts only; publish/rollback remain planned.");
        Ok(())
    }

    fn client_banner_preview(&self, slot: u8) -> io::Result<()> {
        let text = self.read_client_banner_slot(slot)?;
        if text.trim().is_empty() {
            println!("slot {slot} has no local draft");
        } else {
            println!("slot {slot} preview:");
            println!("{text}");
        }
        Ok(())
    }

    fn read_client_banner_slot(&self, slot: u8) -> io::Result<String> {
        read_file_to_string(
            &self
                .session
                .client_banners_dir()
                .join(format!("slot-{slot}.txt")),
        )
    }

    fn maintenance(&self, parsed: &[String]) -> io::Result<()> {
        match parsed.get(1).map(String::as_str) {
            Some("enable") => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let Some(scope) = args.first().map(String::as_str) else {
                    println!(
                        "usage: maintenance enable <scope> <message> --confirm maintenance:<scope>:enable"
                    );
                    return Ok(());
                };
                let message = if args.len() > 1 {
                    args[1..].join(" ")
                } else {
                    String::new()
                };
                if message.trim().is_empty() {
                    println!(
                        "usage: maintenance enable <scope> <message> --confirm maintenance:<scope>:enable"
                    );
                    return Ok(());
                }
                let expected = format!("maintenance:{scope}:enable");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let id = write_ops_draft(&self.session, "maintenance", "enable", scope, &message)?;
                println!("maintenance enable draft saved: {id}");
                println!("scope: {scope}");
                println!("message: {message}");
                println!("MVP note: production maintenance mode was not changed.");
                Ok(())
            }
            Some("disable") => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let scope = args.first().map(String::as_str).unwrap_or("all");
                let reason = if args.len() > 1 {
                    args[1..].join(" ")
                } else {
                    "maintenance disable requested".to_string()
                };
                let expected = format!("maintenance:{scope}:disable");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let id = write_ops_draft(&self.session, "maintenance", "disable", scope, &reason)?;
                println!("maintenance disable draft saved: {id}");
                println!("scope: {scope}");
                println!("reason: {reason}");
                println!("MVP note: production maintenance mode was not changed.");
                Ok(())
            }
            Some("status") => {
                let current =
                    read_file_to_string(&self.session.maintenance_dir().join("current.txt"))?;
                if current.trim().is_empty() {
                    println!("maintenance: no local draft");
                } else {
                    println!("local maintenance draft:");
                    println!("{current}");
                }
                println!("MVP note: production maintenance mode is not connected yet.");
                Ok(())
            }
            Some("history") => {
                let history =
                    read_file_to_string(&self.session.maintenance_dir().join("history.jsonl"))?;
                if history.trim().is_empty() {
                    println!("maintenance history is empty");
                } else {
                    for line in history.lines().take(200) {
                        println!("{line}");
                    }
                }
                Ok(())
            }
            Some("schedule") => {
                let Some(scope) = parsed.get(2) else {
                    println!("usage: maintenance schedule <scope> <start> <end> <message>");
                    return Ok(());
                };
                let Some(start) = parsed.get(3) else {
                    println!("usage: maintenance schedule <scope> <start> <end> <message>");
                    return Ok(());
                };
                let Some(end) = parsed.get(4) else {
                    println!("usage: maintenance schedule <scope> <start> <end> <message>");
                    return Ok(());
                };
                let message = if parsed.len() > 5 {
                    parsed[5..].join(" ")
                } else {
                    String::new()
                };
                if message.trim().is_empty() {
                    println!("usage: maintenance schedule <scope> <start> <end> <message>");
                    return Ok(());
                }
                let id = write_maintenance_draft(&self.session, scope, start, end, &message)?;
                println!("maintenance draft scheduled: {id}");
                println!("MVP note: draft only; no client/server maintenance flag changed.");
                Ok(())
            }
            Some("broadcast") => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let Some(scope) = args.first().map(String::as_str) else {
                    println!(
                        "usage: maintenance broadcast <scope> <message> --confirm maintenance:<scope>:broadcast"
                    );
                    return Ok(());
                };
                let message = if args.len() > 1 {
                    args[1..].join(" ")
                } else {
                    String::new()
                };
                if message.trim().is_empty() {
                    println!(
                        "usage: maintenance broadcast <scope> <message> --confirm maintenance:<scope>:broadcast"
                    );
                    return Ok(());
                }
                let expected = format!("maintenance:{scope}:broadcast");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let id =
                    write_ops_draft(&self.session, "maintenance", "broadcast", scope, &message)?;
                println!("maintenance broadcast draft saved: {id}");
                println!("scope: {scope}");
                println!("message: {message}");
                println!("MVP note: no broadcast was sent.");
                Ok(())
            }
            _ => {
                println!("usage: maintenance enable|disable|status|schedule|broadcast|history");
                Ok(())
            }
        }
    }

    fn release(&self, parsed: &[String]) -> io::Result<()> {
        let repo_root = repo_root_path();
        match parsed.get(1).map(String::as_str) {
            Some("current") => match latest_manifest(&repo_root)? {
                Some(manifest) => {
                    println!("{}", render_manifest_summary(&manifest));
                }
                None => {
                    println!("no local release manifest found");
                    println!(
                        "expected: apps/client-core/release/update-bundles/YYYYMMDD/sekailink-client-release-YYYYMMDD.json"
                    );
                }
            },
            Some("list") => {
                let manifests = dedupe_manifests(&discover_manifests(&repo_root)?);
                println!("{}", render_manifest_list(&manifests));
            }
            Some("verify") => {
                let args = non_flag_args(parsed, 2);
                let selector = args.first().map(String::as_str);
                let check_sha = !parsed.iter().any(|part| part == "--fast");
                match resolve_manifest(&repo_root, selector)? {
                    Some(manifest) => {
                        println!("release verify: {}", manifest.version);
                        println!("manifest: {}", manifest.path.display());
                        if check_sha {
                            println!("sha256: enabled");
                        } else {
                            println!("sha256: skipped by --fast");
                        }
                        let verification = verify_manifest(&manifest, check_sha);
                        println!("{}", render_verification(&verification, check_sha));
                    }
                    None => println!("release manifest not found"),
                }
            }
            Some("verify-cdn") => {
                let args = non_flag_args(parsed, 2);
                let channel = args.first().map(String::as_str).unwrap_or("test");
                let platform = args.get(1).map(String::as_str).unwrap_or("all");
                let execute = parsed.iter().any(|part| part == "--execute");
                let urls = public_release_urls(channel, platform);
                println!("release CDN probe");
                for url in &urls {
                    println!("GET {url}");
                }
                if !execute {
                    println!("dry-run only. Add --execute to call the public release-latest API.");
                    return Ok(());
                }
                for url in urls {
                    println!();
                    println!("== {url}");
                    let status = Command::new("curl")
                        .arg("-fsS")
                        .arg("--connect-timeout")
                        .arg("5")
                        .arg("--max-time")
                        .arg("20")
                        .arg(&url)
                        .status()?;
                    println!();
                    println!("curl exit status: {status}");
                }
            }
            Some("compare") => {
                let args = non_flag_args(parsed, 2);
                if args.len() != 2 {
                    println!(
                        "usage: release compare <manifest|version|date> <manifest|version|date>"
                    );
                    return Ok(());
                }
                let left = resolve_manifest(&repo_root, Some(&args[0]))?;
                let right = resolve_manifest(&repo_root, Some(&args[1]))?;
                match (left, right) {
                    (Some(left), Some(right)) => {
                        println!("{}", render_manifest_compare(&left, &right));
                    }
                    (None, _) => println!("left release manifest not found: {}", args[0]),
                    (_, None) => println!("right release manifest not found: {}", args[1]),
                }
            }
            Some("notes") | Some("audit") => {
                let action = parsed.get(1).map(String::as_str).unwrap_or("notes");
                let args = non_flag_args(parsed, 2);
                let selector = args.first().map(String::as_str);
                match resolve_manifest(&repo_root, selector)? {
                    Some(manifest) => {
                        println!("release {action}: {}", manifest.version);
                        println!("{}", render_manifest_summary(&manifest));
                        let drafts =
                            read_file_to_string(&self.session.releases_dir().join("drafts.jsonl"))?;
                        if drafts.trim().is_empty() {
                            println!("local release drafts: empty");
                        } else {
                            println!("local release drafts:");
                            for line in drafts
                                .lines()
                                .filter(|line| {
                                    selector.map(|value| line.contains(value)).unwrap_or(true)
                                })
                                .take(100)
                            {
                                println!("{line}");
                            }
                        }
                    }
                    None => println!("release manifest not found"),
                }
            }
            Some("publish") => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let Some(selector) = args.first() else {
                    println!(
                        "usage: release publish <manifest|version|date> --confirm release:<version>:publish"
                    );
                    return Ok(());
                };
                match resolve_manifest(&repo_root, Some(selector))? {
                    Some(manifest) => {
                        let expected = format!("release:{}:publish", manifest.version);
                        if flag_value(parsed, "--confirm") != Some(expected.as_str()) {
                            println!("release publish draft blocked by confirmation guard");
                            println!("required confirmation: --confirm {expected}");
                            return Ok(());
                        }
                        let detail = format!(
                            "manifest={} channel={} build={} artifacts={}",
                            manifest.path.display(),
                            manifest.channel,
                            manifest.build,
                            manifest.artifacts.len()
                        );
                        let id = write_release_draft(
                            &self.session,
                            "publish",
                            &manifest.version,
                            &detail,
                        )?;
                        println!("release publish draft saved: {id}");
                        println!(
                            "MVP note: no CDN file was uploaded and no service was restarted."
                        );
                    }
                    None => println!("release manifest not found: {selector}"),
                }
            }
            Some("rollback") => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let Some(version) = args.first() else {
                    println!(
                        "usage: release rollback <version> --confirm release:<version>:rollback"
                    );
                    return Ok(());
                };
                let expected = format!("release:{version}:rollback");
                if flag_value(parsed, "--confirm") != Some(expected.as_str()) {
                    println!("release rollback draft blocked by confirmation guard");
                    println!("required confirmation: --confirm {expected}");
                    return Ok(());
                }
                let id = write_release_draft(
                    &self.session,
                    "rollback",
                    version,
                    "rollback requested; manual CDN manifest replacement still required",
                )?;
                println!("release rollback draft saved: {id}");
                println!("MVP note: no CDN manifest changed.");
            }
            Some("schedule") => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                if args.len() < 2 {
                    println!(
                        "usage: release schedule <version|manifest|date> <datetime> --confirm release:<version>:schedule"
                    );
                    return Ok(());
                }
                let selector = &args[0];
                let when = &args[1];
                let version = resolve_manifest(&repo_root, Some(selector))?
                    .map(|manifest| manifest.version)
                    .unwrap_or_else(|| selector.clone());
                let expected = format!("release:{version}:schedule");
                if flag_value(parsed, "--confirm") != Some(expected.as_str()) {
                    println!("release schedule draft blocked by confirmation guard");
                    println!("required confirmation: --confirm {expected}");
                    return Ok(());
                }
                let release_id = write_release_draft(
                    &self.session,
                    "schedule",
                    &version,
                    &format!("when={when}"),
                )?;
                let job_id = write_schedule_job(
                    &self.session,
                    &format!("release-publish-{version}"),
                    when,
                    &format!("release publish {version} --confirm release:{version}:publish"),
                )?;
                println!("release schedule draft saved: {release_id}");
                println!("schedule draft added: {job_id}");
                println!("MVP note: schedule is not armed for automatic CDN mutation.");
            }
            _ => println!(
                "usage: release current|list|verify|verify-cdn|compare|publish|rollback|schedule|notes|audit"
            ),
        }
        Ok(())
    }

    fn schedule(&self, parsed: &[String]) -> io::Result<()> {
        match parsed.get(1).map(String::as_str) {
            Some("list") | Some("calendar") | Some("history") => {
                let jobs = read_file_to_string(&self.session.scheduler_dir().join("jobs.jsonl"))?;
                if jobs.trim().is_empty() {
                    println!("schedule is empty");
                } else {
                    for line in jobs.lines().take(200) {
                        println!("{line}");
                    }
                }
                println!("MVP note: local draft schedule only; no job execution is enabled.");
                Ok(())
            }
            Some("add") => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let Some(name) = args.first().map(String::as_str) else {
                    println!(
                        "usage: schedule add <name> <when> <command> --confirm schedule:<name>:add"
                    );
                    return Ok(());
                };
                let Some(when) = args.get(1).map(String::as_str) else {
                    println!(
                        "usage: schedule add <name> <when> <command> --confirm schedule:<name>:add"
                    );
                    return Ok(());
                };
                let command = if args.len() > 2 {
                    args[2..].join(" ")
                } else {
                    String::new()
                };
                if command.trim().is_empty() {
                    println!(
                        "usage: schedule add <name> <when> <command> --confirm schedule:<name>:add"
                    );
                    return Ok(());
                }
                let expected = format!("schedule:{name}:add");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let id = write_schedule_job(&self.session, name, when, &command)?;
                println!("schedule draft added: {id}");
                println!("MVP note: job is not armed; schedule run-now remains planned.");
                Ok(())
            }
            Some("edit") => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let Some(job) = args.first().map(String::as_str) else {
                    println!(
                        "usage: schedule edit <job> key=value [key=value...] --confirm schedule:<job>:edit"
                    );
                    return Ok(());
                };
                if args.len() < 2 {
                    println!(
                        "usage: schedule edit <job> key=value [key=value...] --confirm schedule:<job>:edit"
                    );
                    return Ok(());
                }
                let expected = format!("schedule:{job}:edit");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let detail = args[1..].join(" ");
                let id = write_ops_draft(&self.session, "schedule", "edit", job, &detail)?;
                println!("schedule edit draft saved: {id}");
                println!("job: {job}");
                println!("detail: {detail}");
                println!("MVP note: no scheduler job was modified.");
                Ok(())
            }
            Some("pause" | "resume" | "run-now") => {
                let action = parsed.get(1).map(String::as_str).unwrap_or("");
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let Some(job) = args.first().map(String::as_str) else {
                    println!(
                        "usage: schedule {action} <job> [reason] --confirm schedule:<job>:{action}"
                    );
                    return Ok(());
                };
                let expected = format!("schedule:{job}:{action}");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let reason = if args.len() > 1 {
                    args[1..].join(" ")
                } else {
                    format!("schedule {action} requested")
                };
                let id = write_ops_draft(&self.session, "schedule", action, job, &reason)?;
                println!("schedule {action} draft saved: {id}");
                println!("job: {job}");
                println!("reason: {reason}");
                println!("MVP note: no scheduler job was changed or executed.");
                Ok(())
            }
            _ => {
                println!("usage: schedule list|calendar|add|edit|pause|resume|run-now|history");
                Ok(())
            }
        }
    }

    fn cleanup(&self, parsed: &[String]) -> io::Result<()> {
        match parsed.get(1).map(String::as_str) {
            Some("plan") => {
                let Some(scope) = parsed.get(2).map(String::as_str) else {
                    println!("usage: cleanup plan <logs|db|spool|all> [notes]");
                    return Ok(());
                };
                if !matches!(scope, "logs" | "db" | "spool" | "all") {
                    println!("usage: cleanup plan <logs|db|spool|all> [notes]");
                    return Ok(());
                }
                let notes = if parsed.len() > 3 {
                    parsed[3..].join(" ")
                } else {
                    "cleanup dry-run plan requested".to_string()
                };
                let id = write_ops_draft(
                    &self.session,
                    "cleanup",
                    &format!("plan-{scope}"),
                    scope,
                    &notes,
                )?;
                println!("cleanup plan draft saved: {id}");
                println!("scope: {scope}");
                println!("notes: {notes}");
                println!("MVP note: no cleanup scan or deletion was executed.");
                Ok(())
            }
            Some("history") => {
                let text = read_file_to_string(&self.session.drafts_dir().join("cleanup.jsonl"))?;
                if text.trim().is_empty() {
                    println!("cleanup history is empty");
                } else {
                    for line in text.lines().take(200) {
                        println!("{line}");
                    }
                }
                Ok(())
            }
            Some("apply" | "rollback") => {
                let action = parsed.get(1).map(String::as_str).unwrap_or("");
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let Some(id_or_plan) = args.first().map(String::as_str) else {
                    println!(
                        "usage: cleanup {action} <plan_id|id> [reason] --confirm cleanup:<id>:{action}"
                    );
                    return Ok(());
                };
                let expected = format!("cleanup:{id_or_plan}:{action}");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let reason = if args.len() > 1 {
                    args[1..].join(" ")
                } else {
                    format!("cleanup {action} requested")
                };
                let draft_id =
                    write_ops_draft(&self.session, "cleanup", action, id_or_plan, &reason)?;
                println!("cleanup {action} draft saved: {draft_id}");
                println!("target: {id_or_plan}");
                println!("reason: {reason}");
                println!("MVP note: no cleanup mutation was executed.");
                Ok(())
            }
            _ => {
                println!("usage: cleanup plan|apply|history|rollback");
                Ok(())
            }
        }
    }

    fn pack(&self, parsed: &[String]) -> io::Result<()> {
        match (
            parsed.get(1).map(String::as_str),
            parsed.get(2).map(String::as_str),
        ) {
            (Some("repo"), Some("list")) => {
                let repos =
                    read_file_to_string(&self.session.pack_repos_dir().join("repos.jsonl"))?;
                if repos.trim().is_empty() {
                    println!("pack repo drafts are empty");
                } else {
                    for line in repos.lines().take(200) {
                        println!("{line}");
                    }
                }
                println!("MVP note: local repo drafts only; CDN publish is not connected.");
                Ok(())
            }
            (Some("repo"), Some("add")) => {
                let args = option_positionals(parsed, 3, &["--confirm"]);
                let Some(id) = args.first().map(String::as_str) else {
                    println!(
                        "usage: pack repo add <id> <url> <game> [notes] --confirm pack-repo:<id>:add"
                    );
                    return Ok(());
                };
                let Some(url) = args.get(1).map(String::as_str) else {
                    println!(
                        "usage: pack repo add <id> <url> <game> [notes] --confirm pack-repo:<id>:add"
                    );
                    return Ok(());
                };
                let Some(game) = args.get(2).map(String::as_str) else {
                    println!(
                        "usage: pack repo add <id> <url> <game> [notes] --confirm pack-repo:<id>:add"
                    );
                    return Ok(());
                };
                let expected = format!("pack-repo:{id}:add");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let notes = if args.len() > 3 {
                    args[3..].join(" ")
                } else {
                    String::new()
                };
                let record_id = write_pack_repo(&self.session, id, url, game, &notes)?;
                println!("pack repo draft added: {record_id}");
                println!("MVP note: draft only; no repo was fetched or published.");
                Ok(())
            }
            (Some("repo"), Some("edit")) => {
                let args = option_positionals(parsed, 3, &["--confirm"]);
                let Some(id) = args.first().map(String::as_str) else {
                    println!(
                        "usage: pack repo edit <id> key=value [key=value...] --confirm pack-repo:<id>:edit"
                    );
                    return Ok(());
                };
                if args.len() < 2 {
                    println!(
                        "usage: pack repo edit <id> key=value [key=value...] --confirm pack-repo:<id>:edit"
                    );
                    return Ok(());
                }
                let expected = format!("pack-repo:{id}:edit");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let detail = args[1..].join(" ");
                let draft_id = write_ops_draft(&self.session, "pack-repo", "edit", id, &detail)?;
                println!("pack repo edit draft saved: {draft_id}");
                println!("repo: {id}");
                println!("detail: {detail}");
                println!("MVP note: no pack repo config was published.");
                Ok(())
            }
            (Some("repo"), Some("disable" | "delete")) => {
                let action = parsed.get(2).map(String::as_str).unwrap_or("");
                let args = option_positionals(parsed, 3, &["--confirm"]);
                let Some(id) = args.first().map(String::as_str) else {
                    println!(
                        "usage: pack repo {action} <id> [reason] --confirm pack-repo:<id>:{action}"
                    );
                    return Ok(());
                };
                let expected = format!("pack-repo:{id}:{action}");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let reason = if args.len() > 1 {
                    args[1..].join(" ")
                } else {
                    format!("pack repo {action} requested")
                };
                let draft_id = write_ops_draft(&self.session, "pack-repo", action, id, &reason)?;
                println!("pack repo {action} draft saved: {draft_id}");
                println!("repo: {id}");
                println!("reason: {reason}");
                println!("MVP note: no pack repo config was published.");
                Ok(())
            }
            (Some("check" | "validate"), _) => {
                let action = parsed.get(1).map(String::as_str).unwrap_or("");
                let args = non_flag_args(parsed, 2);
                let Some(id) = args.first().map(String::as_str) else {
                    println!("usage: pack {action} <repo-id> [notes]");
                    return Ok(());
                };
                let detail = if args.len() > 1 {
                    args[1..].join(" ")
                } else {
                    format!("{action} requested")
                };
                let draft_id = write_ops_draft(&self.session, "pack", action, id, &detail)?;
                println!("pack {action} draft saved: {draft_id}");
                println!("repo: {id}");
                println!("detail: {detail}");
                println!("MVP note: no pack repo was fetched and no Lua logic was evaluated.");
                Ok(())
            }
            (Some("stage" | "publish"), _) => {
                let action = parsed.get(1).map(String::as_str).unwrap_or("");
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let Some(id) = args.first().map(String::as_str) else {
                    println!(
                        "usage: pack {action} <repo-id> [notes] --confirm pack:<repo-id>:{action}"
                    );
                    return Ok(());
                };
                let expected = format!("pack:{id}:{action}");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let detail = if args.len() > 1 {
                    args[1..].join(" ")
                } else {
                    format!("{action} requested")
                };
                let draft_id = write_ops_draft(&self.session, "pack", action, id, &detail)?;
                println!("pack {action} draft saved: {draft_id}");
                println!("repo: {id}");
                println!("detail: {detail}");
                println!("MVP note: no CDN pack artifact was staged or published.");
                Ok(())
            }
            (Some("rollback"), _) => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let Some(id) = args.first().map(String::as_str) else {
                    println!(
                        "usage: pack rollback <repo-id> <version> --confirm pack:<repo-id>:rollback:<version>"
                    );
                    return Ok(());
                };
                let Some(version) = args.get(1).map(String::as_str) else {
                    println!(
                        "usage: pack rollback <repo-id> <version> --confirm pack:<repo-id>:rollback:<version>"
                    );
                    return Ok(());
                };
                let expected = format!("pack:{id}:rollback:{version}");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let draft_id = write_ops_draft(&self.session, "pack", "rollback", id, version)?;
                println!("pack rollback draft saved: {draft_id}");
                println!("repo: {id}");
                println!("version: {version}");
                println!("MVP note: no CDN pack artifact was rolled back.");
                Ok(())
            }
            (Some("schedule-check"), _) => {
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let Some(id) = args.first().map(String::as_str) else {
                    println!(
                        "usage: pack schedule-check <repo-id> <when-or-interval> --confirm pack:<repo-id>:schedule-check"
                    );
                    return Ok(());
                };
                let Some(when) = args.get(1).map(String::as_str) else {
                    println!(
                        "usage: pack schedule-check <repo-id> <when-or-interval> --confirm pack:<repo-id>:schedule-check"
                    );
                    return Ok(());
                };
                let expected = format!("pack:{id}:schedule-check");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                };
                let job_id = write_schedule_job(
                    &self.session,
                    &format!("pack-check-{id}"),
                    when,
                    &format!("pack check {id}"),
                )?;
                println!("pack check schedule draft added: {job_id}");
                println!("MVP note: job is not armed; no pack repo was fetched.");
                Ok(())
            }
            _ => {
                println!(
                    "usage: pack repo list|add|edit|disable|delete | pack check|validate|stage|publish|rollback|schedule-check"
                );
                Ok(())
            }
        }
    }

    fn bot_ops(&self, parsed: &[String]) -> io::Result<()> {
        let platform = parsed.first().map(String::as_str).unwrap_or("");
        let service = match platform {
            "discord" => "sekailink-social-bots",
            "twitch" => "sekailink-twitch-assistant",
            _ => {
                println!("usage: discord|twitch <command>");
                return Ok(());
            }
        };
        match parsed.get(1).map(String::as_str) {
            Some("status") => {
                let execute = parsed.iter().any(|part| part == "--execute");
                let server = match find_agent_server("link") {
                    Ok(server) => server,
                    Err(err) => {
                        println!("{err}");
                        return Ok(());
                    }
                };
                match agent_service_plan(server, service) {
                    Ok(plan) => self.render_or_execute_agent_plan(&plan, execute, None)?,
                    Err(err) => println!("{err}"),
                }
                let drafts = read_file_to_string(
                    &self.session.drafts_dir().join(format!("{platform}.jsonl")),
                )?;
                let count = drafts
                    .lines()
                    .filter(|line| !line.trim().is_empty())
                    .count();
                println!("{platform} local drafts: {count}");
                println!(
                    "MVP note: bot config API is not connected; status uses service/log probes only."
                );
                Ok(())
            }
            Some("logs") => {
                let execute = parsed.iter().any(|part| part == "--execute");
                let source = if platform == "discord" {
                    "discord:bot"
                } else {
                    "twitch:assistant"
                };
                let follow = parsed.iter().any(|part| part == "--follow" || part == "-f");
                match render_log_tail_plan(source, follow) {
                    Ok(plan) => self.render_or_execute_remote_plan("bot logs", &plan, execute)?,
                    Err(err) => println!("{err}"),
                }
                Ok(())
            }
            Some("reload") if platform == "discord" => {
                self.bot_simple_confirmed_draft(platform, "reload", "bot", parsed, 2)
            }
            Some("sync-roles") if platform == "discord" => {
                self.bot_simple_confirmed_draft(platform, "sync-roles", "all", parsed, 2)
            }
            Some("announce") => self.bot_announce(platform, parsed),
            Some("command") => self.bot_command(platform, parsed),
            Some("timer") => self.bot_timer(platform, parsed),
            Some("incident-post") if platform == "discord" => self.discord_incident_post(parsed),
            Some("connect" | "disconnect") if platform == "twitch" => {
                let action = parsed.get(1).map(String::as_str).unwrap_or("");
                let args = option_positionals(parsed, 2, &["--confirm"]);
                let Some(channel) = args.first().map(String::as_str) else {
                    println!(
                        "usage: twitch {action} <channel> [reason] --confirm twitch:{action}:<channel>"
                    );
                    return Ok(());
                };
                let expected = format!("twitch:{action}:{channel}");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let detail = if args.len() > 1 {
                    args[1..].join(" ")
                } else {
                    format!("twitch {action} requested")
                };
                let id = write_ops_draft(&self.session, "twitch", action, channel, &detail)?;
                println!("twitch {action} draft saved: {id}");
                println!("channel: {channel}");
                println!("detail: {detail}");
                println!("MVP note: Twitch API was not called.");
                Ok(())
            }
            Some("lobby") if platform == "twitch" => self.twitch_lobby(parsed),
            Some("stream") if platform == "twitch" => self.twitch_stream(parsed),
            _ => {
                if platform == "discord" {
                    println!(
                        "usage: discord status|reload|announce|sync-roles|command|timer|incident-post|logs"
                    );
                } else {
                    println!(
                        "usage: twitch status|connect|disconnect|announce|command|timer|lobby|stream|logs"
                    );
                }
                Ok(())
            }
        }
    }

    fn bot_simple_confirmed_draft(
        &self,
        platform: &str,
        action: &str,
        target: &str,
        parsed: &[String],
        detail_start: usize,
    ) -> io::Result<()> {
        let expected = format!("{platform}:{action}:{target}");
        if !confirmation_matches(parsed, &expected) {
            return Ok(());
        }
        let args = option_positionals(parsed, detail_start, &["--confirm"]);
        let detail = if args.is_empty() {
            format!("{platform} {action} requested")
        } else {
            args.join(" ")
        };
        let id = write_ops_draft(&self.session, platform, action, target, &detail)?;
        println!("{platform} {action} draft saved: {id}");
        println!("target: {target}");
        println!("detail: {detail}");
        println!("MVP note: bot process/API was not changed.");
        Ok(())
    }

    fn bot_announce(&self, platform: &str, parsed: &[String]) -> io::Result<()> {
        let args = option_positionals(parsed, 2, &["--confirm"]);
        let Some(channel) = args.first().map(String::as_str) else {
            println!(
                "usage: {platform} announce <channel> <message> --confirm {platform}:announce:<channel>"
            );
            return Ok(());
        };
        let message = if args.len() > 1 {
            args[1..].join(" ")
        } else {
            String::new()
        };
        if message.trim().is_empty() {
            println!(
                "usage: {platform} announce <channel> <message> --confirm {platform}:announce:<channel>"
            );
            return Ok(());
        }
        let expected = format!("{platform}:announce:{channel}");
        if !confirmation_matches(parsed, &expected) {
            return Ok(());
        }
        let id = write_ops_draft(&self.session, platform, "announce", channel, &message)?;
        println!("{platform} announce draft saved: {id}");
        println!("channel: {channel}");
        println!("message: {message}");
        println!("MVP note: announcement was not sent.");
        Ok(())
    }

    fn bot_command(&self, platform: &str, parsed: &[String]) -> io::Result<()> {
        match parsed.get(2).map(String::as_str) {
            Some("list") => {
                self.bot_history(platform, "command")?;
                println!("MVP note: live bot command registry is not connected yet.");
                Ok(())
            }
            Some("enable" | "disable" | "edit") => {
                let action = parsed.get(2).map(String::as_str).unwrap_or("");
                let args = option_positionals(parsed, 3, &["--confirm"]);
                let Some(name) = args.first().map(String::as_str) else {
                    println!(
                        "usage: {platform} command {action} <name> [detail] --confirm {platform}:command:<name>:{action}"
                    );
                    return Ok(());
                };
                let expected = format!("{platform}:command:{name}:{action}");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let detail = if args.len() > 1 {
                    args[1..].join(" ")
                } else {
                    format!("command {action} requested")
                };
                let id = write_ops_draft(
                    &self.session,
                    platform,
                    &format!("command-{action}"),
                    name,
                    &detail,
                )?;
                println!("{platform} command {action} draft saved: {id}");
                println!("command: {name}");
                println!("detail: {detail}");
                println!("MVP note: bot command config was not changed.");
                Ok(())
            }
            _ => {
                println!("usage: {platform} command list|enable|disable|edit");
                Ok(())
            }
        }
    }

    fn bot_timer(&self, platform: &str, parsed: &[String]) -> io::Result<()> {
        match parsed.get(2).map(String::as_str) {
            Some("list") => {
                self.bot_history(platform, "timer")?;
                println!("MVP note: live bot timer registry is not connected yet.");
                Ok(())
            }
            Some("edit") => {
                let args = option_positionals(parsed, 3, &["--confirm"]);
                let Some(timer_id) = args.first().map(String::as_str) else {
                    println!(
                        "usage: {platform} timer edit <id> key=value [key=value...] --confirm {platform}:timer:<id>:edit"
                    );
                    return Ok(());
                };
                if args.len() < 2 {
                    println!(
                        "usage: {platform} timer edit <id> key=value [key=value...] --confirm {platform}:timer:<id>:edit"
                    );
                    return Ok(());
                }
                let expected = format!("{platform}:timer:{timer_id}:edit");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let detail = args[1..].join(" ");
                let id = write_ops_draft(&self.session, platform, "timer-edit", timer_id, &detail)?;
                println!("{platform} timer edit draft saved: {id}");
                println!("timer: {timer_id}");
                println!("detail: {detail}");
                println!("MVP note: bot timer config was not changed.");
                Ok(())
            }
            _ => {
                println!("usage: {platform} timer list|edit");
                Ok(())
            }
        }
    }

    fn discord_incident_post(&self, parsed: &[String]) -> io::Result<()> {
        let args = option_positionals(parsed, 2, &["--confirm"]);
        let Some(incident) = args.first().map(String::as_str) else {
            println!(
                "usage: discord incident-post <incident> [channel] [message] --confirm discord:incident-post:<incident>"
            );
            return Ok(());
        };
        let expected = format!("discord:incident-post:{incident}");
        if !confirmation_matches(parsed, &expected) {
            return Ok(());
        }
        let detail = if args.len() > 1 {
            args[1..].join(" ")
        } else {
            "incident post requested".to_string()
        };
        let id = write_ops_draft(&self.session, "discord", "incident-post", incident, &detail)?;
        println!("discord incident-post draft saved: {id}");
        println!("incident: {incident}");
        println!("detail: {detail}");
        println!("MVP note: Discord API was not called.");
        Ok(())
    }

    fn twitch_lobby(&self, parsed: &[String]) -> io::Result<()> {
        match parsed.get(2).map(String::as_str) {
            Some("announce") => {
                let args = option_positionals(parsed, 3, &["--confirm"]);
                let Some(lobby) = args.first().map(String::as_str) else {
                    println!(
                        "usage: twitch lobby announce <lobby> [message] --confirm twitch:lobby:<lobby>:announce"
                    );
                    return Ok(());
                };
                let expected = format!("twitch:lobby:{lobby}:announce");
                if !confirmation_matches(parsed, &expected) {
                    return Ok(());
                }
                let detail = if args.len() > 1 {
                    args[1..].join(" ")
                } else {
                    "lobby announcement requested".to_string()
                };
                let id =
                    write_ops_draft(&self.session, "twitch", "lobby-announce", lobby, &detail)?;
                println!("twitch lobby announce draft saved: {id}");
                println!("lobby: {lobby}");
                println!("detail: {detail}");
                println!("MVP note: Twitch API was not called.");
                Ok(())
            }
            _ => {
                println!("usage: twitch lobby announce <lobby>");
                Ok(())
            }
        }
    }

    fn twitch_stream(&self, parsed: &[String]) -> io::Result<()> {
        match parsed.get(2).map(String::as_str) {
            Some("set-title-hint") => {
                let args = non_flag_args(parsed, 3);
                if args.is_empty() {
                    println!("usage: twitch stream set-title-hint [channel] <title>");
                    return Ok(());
                }
                let (target, title) = if args.len() == 1 {
                    ("default", args[0].clone())
                } else {
                    (args[0].as_str(), args[1..].join(" "))
                };
                let id =
                    write_ops_draft(&self.session, "twitch", "stream-title-hint", target, &title)?;
                println!("twitch stream title hint draft saved: {id}");
                println!("target: {target}");
                println!("title: {title}");
                println!("MVP note: title hint is local; Twitch API was not called.");
                Ok(())
            }
            _ => {
                println!("usage: twitch stream set-title-hint [channel] <title>");
                Ok(())
            }
        }
    }

    fn bot_history(&self, platform: &str, filter: &str) -> io::Result<()> {
        let text =
            read_file_to_string(&self.session.drafts_dir().join(format!("{platform}.jsonl")))?;
        if text.trim().is_empty() {
            println!("{platform} draft history is empty");
            return Ok(());
        }
        let mut shown = 0_usize;
        for line in text.lines().filter(|line| line.contains(filter)).take(200) {
            println!("{line}");
            shown += 1;
        }
        if shown == 0 {
            println!("no {platform} drafts matched: {filter}");
        }
        Ok(())
    }

    fn stub_or_unknown(
        &self,
        line: &str,
        spec: Option<&crate::commands::CommandSpec>,
    ) -> io::Result<()> {
        if let Some(spec) = spec {
            println!("command recognized: {}", spec.name);
            println!("status: planned integration; no server mutation performed");
            println!(
                "role: {} | confirmation: {:?}",
                spec.role.as_str(),
                spec.confirmation
            );
            if spec.confirmation == Confirmation::Required {
                println!("future confirmation: exact target + reason required");
            }
            append_audit(&self.session, line, "planned_not_implemented", spec.name)?;
        } else {
            println!("unknown command: {line}");
            println!("try: help, commands list, commands search <query>");
            append_audit(&self.session, line, "unknown_command", "no registry match")?;
        }
        Ok(())
    }
}

fn push_recent_lines(body: &mut String, source: &str, limit: usize) {
    let lines = source.lines().collect::<Vec<_>>();
    let start = lines.len().saturating_sub(limit);
    for line in &lines[start..] {
        body.push_str(line);
        body.push('\n');
    }
}

fn parse_banner_slot(value: &str) -> Result<u8, String> {
    match value.parse::<u8>() {
        Ok(slot @ 1..=3) => Ok(slot),
        _ => Err("client banner slot must be 1, 2, or 3".to_string()),
    }
}

fn first_non_flag<'a>(parts: &'a [String], start: usize, default: &'a str) -> &'a str {
    parts
        .iter()
        .skip(start)
        .find(|part| !part.starts_with("--"))
        .map(String::as_str)
        .unwrap_or(default)
}

fn flag_value<'a>(parts: &'a [String], flag: &str) -> Option<&'a str> {
    let mut i = 0;
    while i < parts.len() {
        if parts[i] == flag {
            return parts.get(i + 1).map(String::as_str);
        }
        if let Some(value) = parts[i].strip_prefix(&format!("{flag}=")) {
            return Some(value);
        }
        i += 1;
    }
    None
}

fn confirmation_matches(parts: &[String], expected: &str) -> bool {
    if flag_value(parts, "--confirm") == Some(expected) {
        return true;
    }
    println!("draft blocked by confirmation guard");
    println!("required confirmation: --confirm {expected}");
    false
}

fn option_positionals(parts: &[String], start: usize, value_flags: &[&str]) -> Vec<String> {
    let mut out = Vec::new();
    let mut i = start;
    while i < parts.len() {
        let part = &parts[i];
        if part.starts_with("--") {
            if value_flags.iter().any(|flag| part == flag) {
                i += 2;
                continue;
            }
            i += 1;
            continue;
        }
        out.push(part.clone());
        i += 1;
    }
    out
}

fn print_usage() {
    println!(
        "sekailink-core-access [--shell] [--user USER] [--role service|admin] [--data-dir PATH] [--command COMMAND]"
    );
}

fn print_banner(session: &Session) {
    println!("====================================================================");
    println!("  SekaiLink Core Access 0.1.0 :: bastion cockpit MVP");
    println!("====================================================================");
    println!(
        "linux={} sekailink={} role={} data={}",
        session.linux_user,
        session.sekailink_user,
        session.role.as_str(),
        session.data_dir.display()
    );
    println!("No production service mutation is enabled in this MVP.");
    println!();
}

fn print_help() {
    println!("Core Access MVP commands:");
    println!("  dashboard");
    println!("  auth whoami");
    println!("  commands list");
    println!("  commands search <query>");
    println!("  server status [server|all] [--execute]");
    println!("  server services [server|all]");
    println!("  server agent-health [server|all] [--execute]");
    println!("  server agent-system [server|all] [--execute]");
    println!("  server agent-services [server|all] [--execute]");
    println!("  server agent-service <server> <service> [--execute]");
    println!("  server agent-logs <server> <service> [--execute]");
    println!(
        "  server restart|start|stop <server> <service> --confirm <server>:<service>:<action> [--execute]"
    );
    println!("  nexus services [--execute]");
    println!("  server logs <server> <service> [--follow] [--execute]");
    println!("  health probe [server|all] [--execute]");
    println!("  user search <query> [--execute]");
    println!("  user open <username> [--execute]");
    println!("  user sessions <username> [--execute]");
    println!("  user devices <username> [--execute]");
    println!("  user audit <username> [limit] [event_type] [offset] [--execute]");
    println!("  user configs <user_id> [game_key] [--execute]");
    println!("  user config open <user_id> <config_id> [--execute]");
    println!("  user config diff <user_id> <config_id> <version>");
    println!("  user config export <user_id> <config_id> [--format yaml] [--execute]");
    println!(
        "  user config edit <user_id> <config_id> key=value [key=value...] --confirm user-config:<user_id>:<config_id>:edit"
    );
    println!(
        "  user create <username> <email> <role> --password-env ENV [--confirm user:<username>:create] [--execute]"
    );
    println!(
        "  user edit <username> key=value [key=value...] --confirm user:<username>:edit [--execute]"
    );
    println!("  user disable <username> --confirm user:<username>:disable [--execute]");
    println!(
        "  user revoke-sessions <username> --confirm user:<username>:revoke-sessions [--execute]"
    );
    println!(
        "  user force-password-reset <username> --confirm user:<username>:force-password-reset [--execute]"
    );
    println!("  lobby list [limit] [query] [visibility] [status] [offset] [--execute]");
    println!("  lobby open <lobby_id> [--execute]");
    println!("  logs list");
    println!("  logs list --by-server");
    println!("  logs list --by-incident");
    println!("  logs tail <source> [--follow] [--execute]");
    println!("  logs search <query> [source|all] [--execute]");
    println!("  logs filter <term...> [source:<source|all>] [--execute]");
    println!("  logs pin <source> <text>");
    println!("  logs note <source> <text>");
    println!("  logs export [query] [--format md|jsonl|txt] [--file name]");
    println!("  audit search [query]");
    println!("  audit export [query] [file-name]");
    println!("  note add <target> <text>");
    println!("  note list [query]");
    println!("  note export [query] [file-name]");
    println!("  approval request <command> <reason>");
    println!("  approval list");
    println!("  approval approve <id> [reason]");
    println!("  incident open <label> <severity> <summary>");
    println!("  incident list [query]");
    println!("  incident status <label>");
    println!("  incident note <label> <text>");
    println!("  incident pin <label> <source> <text>");
    println!("  incident export <label> [--file name]");
    println!("  incident close <label> <resolution>");
    println!("  ops snapshot [label]");
    println!("  ops timeline [query]");
    println!("  ops handoff [label] [--file name]");
    println!("  ops doctor [--verbose]");
    println!("  ops paths");
    println!("  ops exports [query]");
    println!("  client-banner list");
    println!("  client-banner preview <1|2|3>");
    println!("  client-banner edit <1|2|3> <text>");
    println!("  client-banner publish|rollback|disable <1|2|3> --confirm banner:<slot>:<action>");
    println!("  broadcast global <message> --confirm broadcast:global:all");
    println!(
        "  broadcast server|lobby|room|role|version|game <target> <message> --confirm broadcast:<scope>:<target>"
    );
    println!("  client broadcast <user|all> <message> --confirm client:broadcast:<target>");
    println!(
        "  client request-relogin <user|all> <reason> --confirm client:request-relogin:<target>"
    );
    println!("  client refresh-lobby <user|all> [lobby-id]");
    println!("  client refresh-room <user|all> [room-id]");
    println!(
        "  client request-sklmi-reconnect <user|all> <reason> --confirm client:request-sklmi-reconnect:<target>"
    );
    println!(
        "  client restart-runtime <user|all> <reason> --confirm client:restart-runtime:<target>"
    );
    println!(
        "  client diagnostics-request <user> <incident> <reason> [--include client-core,sekaiemu,sklmi,configs,system]"
    );
    println!("  client diagnostics-list [query]");
    println!("  client diagnostics-export [query] [--file name]");
    println!("  maintenance enable <scope> <message> --confirm maintenance:<scope>:enable");
    println!("  maintenance disable [scope] [reason] --confirm maintenance:<scope>:disable");
    println!("  maintenance broadcast <scope> <message> --confirm maintenance:<scope>:broadcast");
    println!("  maintenance status");
    println!("  maintenance schedule <scope> <start> <end> <message>");
    println!("  maintenance history");
    println!("  release current");
    println!("  release list");
    println!("  release verify [latest|version|date|manifest] [--fast]");
    println!("  release verify-cdn [channel] [platform|all] [--execute]");
    println!("  release compare <version|date|manifest> <version|date|manifest>");
    println!("  release publish <version|date|manifest> --confirm release:<version>:publish");
    println!("  release rollback <version> --confirm release:<version>:rollback");
    println!(
        "  release schedule <version|date|manifest> <datetime> --confirm release:<version>:schedule"
    );
    println!("  release notes [version|date|manifest]");
    println!("  release audit [version|date|manifest]");
    println!("  schedule list");
    println!("  schedule calendar");
    println!("  schedule add <name> <when> <command> --confirm schedule:<name>:add");
    println!("  schedule edit <job> key=value [key=value...] --confirm schedule:<job>:edit");
    println!("  schedule pause|resume|run-now <job> [reason] --confirm schedule:<job>:<action>");
    println!("  schedule history");
    println!("  cleanup plan <logs|db|spool|all> [notes]");
    println!("  cleanup apply <plan_id> [reason] --confirm cleanup:<plan_id>:apply");
    println!("  cleanup history");
    println!("  cleanup rollback <id> [reason] --confirm cleanup:<id>:rollback");
    println!("  pack repo list");
    println!("  pack repo add <id> <url> <game> [notes] --confirm pack-repo:<id>:add");
    println!("  pack repo edit <id> key=value [key=value...] --confirm pack-repo:<id>:edit");
    println!("  pack repo disable|delete <id> [reason] --confirm pack-repo:<id>:<action>");
    println!("  pack check|validate <repo-id> [notes]");
    println!("  pack stage|publish <repo-id> [notes] --confirm pack:<repo-id>:<action>");
    println!("  pack rollback <repo-id> <version> --confirm pack:<repo-id>:rollback:<version>");
    println!(
        "  pack schedule-check <repo-id> <when-or-interval> --confirm pack:<repo-id>:schedule-check"
    );
    println!("  discord status [--execute]");
    println!("  discord logs [--follow] [--execute]");
    println!("  discord reload --confirm discord:reload:bot");
    println!("  discord announce <channel> <message> --confirm discord:announce:<channel>");
    println!("  discord sync-roles --confirm discord:sync-roles:all");
    println!("  discord command list");
    println!(
        "  discord command enable|disable|edit <name> [detail] --confirm discord:command:<name>:<action>"
    );
    println!("  discord timer list");
    println!(
        "  discord timer edit <id> key=value [key=value...] --confirm discord:timer:<id>:edit"
    );
    println!(
        "  discord incident-post <incident> [channel] [message] --confirm discord:incident-post:<incident>"
    );
    println!("  twitch status [--execute]");
    println!("  twitch logs [--follow] [--execute]");
    println!("  twitch connect|disconnect <channel> [reason] --confirm twitch:<action>:<channel>");
    println!("  twitch announce <channel> <message> --confirm twitch:announce:<channel>");
    println!("  twitch command list");
    println!(
        "  twitch command enable|disable|edit <name> [detail] --confirm twitch:command:<name>:<action>"
    );
    println!("  twitch timer list");
    println!("  twitch timer edit <id> key=value [key=value...] --confirm twitch:timer:<id>:edit");
    println!("  twitch lobby announce <lobby> [message] --confirm twitch:lobby:<lobby>:announce");
    println!("  twitch stream set-title-hint [channel] <title>");
    println!("  exit");
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum LogExportFormat {
    Markdown,
    Jsonl,
    Text,
}

impl LogExportFormat {
    fn parse(value: &str) -> Self {
        match value.trim().to_ascii_lowercase().as_str() {
            "json" | "jsonl" => Self::Jsonl,
            "txt" | "text" => Self::Text,
            _ => Self::Markdown,
        }
    }

    fn as_str(self) -> &'static str {
        match self {
            Self::Markdown => "md",
            Self::Jsonl => "jsonl",
            Self::Text => "txt",
        }
    }

    fn extension(self) -> &'static str {
        self.as_str()
    }
}

#[derive(Debug, Clone)]
struct LogExportOptions {
    format: LogExportFormat,
    query: String,
    file_name: Option<String>,
}

impl LogExportOptions {
    fn from_parts(parts: &[String]) -> Self {
        let mut format = LogExportFormat::Markdown;
        let mut file_name = None;
        let mut query = Vec::new();
        let mut i = 2;
        while i < parts.len() {
            match parts[i].as_str() {
                "--format" => {
                    i += 1;
                    if let Some(value) = parts.get(i) {
                        format = LogExportFormat::parse(value);
                    }
                }
                value if value.starts_with("--format=") => {
                    format = LogExportFormat::parse(value.trim_start_matches("--format="));
                }
                "--file" => {
                    i += 1;
                    if let Some(value) = parts.get(i) {
                        file_name = Some(value.clone());
                    }
                }
                value if value.starts_with("--file=") => {
                    file_name = Some(value.trim_start_matches("--file=").to_string());
                }
                value if looks_like_export_file(value) && file_name.is_none() => {
                    file_name = Some(value.to_string());
                }
                value => query.push(value.to_string()),
            }
            i += 1;
        }
        Self {
            format,
            query: query.join(" "),
            file_name,
        }
    }
}

#[derive(Debug, Clone)]
struct DiagnosticsArgs {
    positionals: Vec<String>,
    include: Vec<String>,
}

impl DiagnosticsArgs {
    fn from_parts(parts: &[String]) -> Self {
        let mut positionals = Vec::new();
        let mut include = Vec::new();
        let mut i = 2;
        while i < parts.len() {
            match parts[i].as_str() {
                "--include" => {
                    i += 1;
                    if let Some(value) = parts.get(i) {
                        include.extend(parse_include_list(value));
                    }
                }
                value if value.starts_with("--include=") => {
                    include.extend(parse_include_list(value.trim_start_matches("--include=")));
                }
                value if value.starts_with("--") => {}
                value => positionals.push(value.to_string()),
            }
            i += 1;
        }
        if include.is_empty() {
            include = default_diagnostics_include();
        }
        include.sort();
        include.dedup();
        Self {
            positionals,
            include,
        }
    }
}

fn parse_include_list(value: &str) -> Vec<String> {
    value
        .split(',')
        .map(|part| part.trim().to_ascii_lowercase())
        .filter(|part| !part.is_empty())
        .map(|part| match part.as_str() {
            "client" => "client-core".to_string(),
            "config" => "configs".to_string(),
            other => other.to_string(),
        })
        .filter(|part| {
            matches!(
                part.as_str(),
                "client-core" | "sekaiemu" | "sklmi" | "configs" | "system"
            )
        })
        .collect()
}

fn default_diagnostics_include() -> Vec<String> {
    ["client-core", "configs", "sekaiemu", "sklmi", "system"]
        .iter()
        .map(|value| value.to_string())
        .collect()
}

fn print_client_diagnostics_contract() {
    println!("Expected client diagnostics bundle:");
    println!("{}", client_diagnostics_contract_markdown());
}

fn client_diagnostics_contract_markdown() -> &'static str {
    r#"- `manifest.json`: request id, user, client version, OS, timestamp, selected include set.
- `client-core/main.log`: Electron main/runtime log.
- `client-core/renderer.log`: UI/auth/lobby flow log.
- `client-core/updater.log`: bootstrap/update/install log.
- `sekaiemu/sekaiemu.log`: Sekaiemu runtime log if available.
- `sekaiemu/stdout.log` and `sekaiemu/stderr.log`: captured process streams.
- `sklmi/sklmi.log`: SKLMI companion runtime log if available.
- `sklmi/stdout.log` and `sklmi/stderr.log`: captured process streams.
- `configs/client-settings.redacted.json`: client settings with tokens/secrets removed.
- `configs/launch-context.redacted.json`: game, room, lobby, runtime paths, versions.
- `system/platform.txt`: OS, arch, install path, app data path, disk space summary.

Redaction requirements:

- remove tokens, passwords, refresh tokens, cookies and private keys;
- hash device identifiers where possible;
- keep room/lobby/user names only when needed for the incident;
- include only files approved by the user consent prompt.

Implementation note:

- Core Access currently records the request only. Client Core upload handling and
  any Sekaiemu/SKLMI log collection hook require a documented connectivity
  contract before implementation.
"#
}

fn looks_like_export_file(value: &str) -> bool {
    value.ends_with(".md")
        || value.ends_with(".txt")
        || value.ends_with(".json")
        || value.ends_with(".jsonl")
}

fn read_draft_files(dir: &Path) -> io::Result<String> {
    let mut out = String::new();
    if !dir.exists() {
        return Ok(out);
    }
    let mut paths = fs::read_dir(dir)?
        .filter_map(Result::ok)
        .map(|entry| entry.path())
        .filter(|path| path.extension().and_then(|value| value.to_str()) == Some("jsonl"))
        .collect::<Vec<_>>();
    paths.sort();
    for path in paths {
        out.push_str(&read_file_to_string(&path)?);
    }
    Ok(out)
}

#[derive(Debug, Clone)]
struct TimelineEntry {
    ts: u64,
    source: &'static str,
    line: String,
}

fn push_timeline_entries(entries: &mut Vec<TimelineEntry>, source: &'static str, text: &str) {
    for line in text.lines().filter(|line| !line.trim().is_empty()) {
        entries.push(TimelineEntry {
            ts: extract_json_u64(line, "ts").unwrap_or_default(),
            source,
            line: line.to_string(),
        });
    }
}

fn extract_json_u64(line: &str, key: &str) -> Option<u64> {
    let needle = format!("\"{key}\":");
    let start = line.find(&needle)? + needle.len();
    let value = line[start..]
        .chars()
        .take_while(|ch| ch.is_ascii_digit())
        .collect::<String>();
    value.parse().ok()
}

fn extract_json_string(line: &str, key: &str) -> Option<String> {
    let needle = format!("\"{key}\":\"");
    let start = line.find(&needle)? + needle.len();
    let rest = &line[start..];
    let mut out = String::new();
    let mut escaped = false;
    for ch in rest.chars() {
        if escaped {
            out.push(ch);
            escaped = false;
            continue;
        }
        if ch == '\\' {
            escaped = true;
            continue;
        }
        if ch == '"' {
            return Some(out);
        }
        out.push(ch);
    }
    None
}

fn push_open_incident_labels(body: &mut String, incidents: &str) {
    let mut states = std::collections::BTreeMap::<String, &'static str>::new();
    for line in incidents.lines() {
        let Some(label) = extract_json_string(line, "label") else {
            continue;
        };
        if line.contains("\"event\":\"open\"") {
            states.insert(label, "open");
        } else if line.contains("\"event\":\"close\"") {
            states.insert(label, "closed");
        }
    }
    let open = states
        .iter()
        .filter(|(_, state)| **state == "open")
        .map(|(label, _)| label)
        .collect::<Vec<_>>();
    if open.is_empty() {
        body.push_str("- none\n");
        return;
    }
    for label in open {
        body.push_str(&format!("- `{label}`\n"));
    }
}

fn push_json_section_from_text(body: &mut String, title: &str, text: &str, limit: usize) {
    body.push_str(&format!("## {title}\n\n```json\n"));
    push_recent_lines(body, text, limit);
    if text.trim().is_empty() {
        body.push_str("(empty)\n");
    }
    body.push_str("```\n\n");
}

fn handoff_file_name(parts: &[String], label: &str) -> String {
    let mut i = 2;
    while i < parts.len() {
        match parts[i].as_str() {
            "--file" => {
                if let Some(value) = parts.get(i + 1) {
                    return value.clone();
                }
            }
            value if value.starts_with("--file=") => {
                return value.trim_start_matches("--file=").to_string();
            }
            value if looks_like_export_file(value) => return value.to_string(),
            _ => {}
        }
        i += 1;
    }
    format!("{label}.md")
}

fn normalize_incident_label(value: &str) -> Result<String, String> {
    let label = value.trim();
    if label.is_empty() {
        return Err("incident label cannot be empty".to_string());
    }
    if !label
        .chars()
        .all(|ch| ch.is_ascii_alphanumeric() || matches!(ch, '_' | '-' | '.' | ':'))
    {
        return Err("incident label may only use letters, numbers, _, -, ., or :".to_string());
    }
    Ok(label.to_string())
}

fn incident_event_lines<'a>(events: &'a str, label: &str) -> impl Iterator<Item = &'a str> {
    let needle = format!("\"label\":\"{label}\"");
    events.lines().filter(move |line| line.contains(&needle))
}

fn incident_note_lines<'a>(notes: &'a str, label: &str) -> impl Iterator<Item = &'a str> {
    let needle = format!("\"target\":\"incident:{label}\"");
    notes.lines().filter(move |line| line.contains(&needle))
}

fn incident_pin_lines<'a>(pins: &'a str, label: &str) -> impl Iterator<Item = &'a str> {
    let needle = format!("incident:{label}");
    pins.lines().filter(move |line| line.contains(&needle))
}

fn incident_state(lines: &[&str]) -> &'static str {
    let mut state = "unknown";
    for line in lines {
        if line.contains("\"event\":\"open\"") {
            state = "open";
        } else if line.contains("\"event\":\"close\"") {
            state = "closed";
        }
    }
    state
}

fn print_related_section(title: &str, lines: Vec<&str>) {
    println!();
    println!("{title}:");
    if lines.is_empty() {
        println!("(none)");
        return;
    }
    for line in lines.iter().rev().take(12).rev() {
        println!("{line}");
    }
}

fn incident_export_file_name(parts: &[String], label: &str) -> String {
    let mut i = 3;
    while i < parts.len() {
        match parts[i].as_str() {
            "--file" => {
                if let Some(value) = parts.get(i + 1) {
                    return value.clone();
                }
            }
            value if value.starts_with("--file=") => {
                return value.trim_start_matches("--file=").to_string();
            }
            value if looks_like_export_file(value) => return value.to_string(),
            _ => {}
        }
        i += 1;
    }
    format!("{label}.md")
}

fn render_incident_export(label: &str, events: &str, notes: &str, pins: &str) -> String {
    let event_lines = incident_event_lines(events, label).collect::<Vec<_>>();
    let note_lines = incident_note_lines(notes, label).collect::<Vec<_>>();
    let pin_lines = incident_pin_lines(pins, label).collect::<Vec<_>>();
    let mut out = String::new();
    out.push_str(&format!("# SekaiLink Core Access Incident - {label}\n\n"));
    out.push_str(&format!("State: `{}`\n\n", incident_state(&event_lines)));
    push_json_section(&mut out, "Incident Events", &event_lines);
    push_json_section(&mut out, "Incident Notes", &note_lines);
    push_json_section(&mut out, "Incident Log Pins", &pin_lines);
    out
}

fn log_filter_search_term(value: &str) -> &str {
    let Some((prefix, term)) = value.split_once(':') else {
        return value;
    };
    match prefix {
        "user" | "lobby" | "room" | "correlation" | "item" | "runtime" => term,
        _ => value,
    }
}

fn render_logs_export(pins: &str, notes: &str, options: &LogExportOptions) -> String {
    let pins = matching_lines(pins, &options.query).collect::<Vec<_>>();
    let notes = matching_lines(notes, &options.query)
        .filter(|line| line.contains("\"target\":\"log:"))
        .collect::<Vec<_>>();
    if pins.is_empty() && notes.is_empty() {
        return String::new();
    }

    match options.format {
        LogExportFormat::Jsonl => pins
            .iter()
            .chain(notes.iter())
            .map(|line| format!("{line}\n"))
            .collect(),
        LogExportFormat::Text => {
            let mut out = String::new();
            out.push_str("SekaiLink Core Access Log Evidence Export\n");
            out.push_str("========================================\n\n");
            push_text_section(&mut out, "Pins", &pins);
            push_text_section(&mut out, "Log Notes", &notes);
            out
        }
        LogExportFormat::Markdown => {
            let mut out = String::new();
            out.push_str("# SekaiLink Core Access Log Evidence Export\n\n");
            if !options.query.trim().is_empty() {
                out.push_str(&format!("Filter: `{}`\n\n", options.query));
            }
            push_json_section(&mut out, "Pins", &pins);
            push_json_section(&mut out, "Log Notes", &notes);
            out
        }
    }
}

fn matching_lines<'a>(text: &'a str, query: &'a str) -> impl Iterator<Item = &'a str> {
    let query = query.trim().to_ascii_lowercase();
    text.lines().filter(move |line| {
        !line.trim().is_empty() && (query.is_empty() || line.to_ascii_lowercase().contains(&query))
    })
}

fn push_json_section(out: &mut String, title: &str, lines: &[&str]) {
    out.push_str(&format!("## {title}\n\n"));
    if lines.is_empty() {
        out.push_str("_No local evidence._\n\n");
        return;
    }
    out.push_str("```json\n");
    for line in lines {
        out.push_str(line);
        out.push('\n');
    }
    out.push_str("```\n\n");
}

fn push_text_section(out: &mut String, title: &str, lines: &[&str]) {
    out.push_str(title);
    out.push('\n');
    out.push_str(&"-".repeat(title.len()));
    out.push('\n');
    if lines.is_empty() {
        out.push_str("(no local evidence)\n\n");
        return;
    }
    for line in lines {
        out.push_str(line);
        out.push('\n');
    }
    out.push('\n');
}

fn repo_root_path() -> PathBuf {
    let manifest_dir = Path::new(env!("CARGO_MANIFEST_DIR"));
    manifest_dir
        .parent()
        .and_then(Path::parent)
        .map(Path::to_path_buf)
        .unwrap_or_else(|| env::current_dir().unwrap_or_else(|_| PathBuf::from(".")))
}

fn docs_root_path() -> PathBuf {
    repo_root_path().join("docs").join("sekailink-core-access")
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum DoctorLevel {
    Ok,
    Warn,
    Fail,
}

impl DoctorLevel {
    fn as_str(self) -> &'static str {
        match self {
            Self::Ok => "OK",
            Self::Warn => "WARN",
            Self::Fail => "FAIL",
        }
    }
}

#[derive(Debug, Clone)]
struct DoctorCheck {
    level: DoctorLevel,
    name: String,
    detail: String,
}

impl DoctorCheck {
    fn ok(name: &str, detail: &str) -> Self {
        Self::new(DoctorLevel::Ok, name, detail)
    }

    fn warn(name: &str, detail: &str) -> Self {
        Self::new(DoctorLevel::Warn, name, detail)
    }

    fn fail(name: &str, detail: &str) -> Self {
        Self::new(DoctorLevel::Fail, name, detail)
    }

    fn new(level: DoctorLevel, name: &str, detail: &str) -> Self {
        Self {
            level,
            name: name.to_string(),
            detail: detail.to_string(),
        }
    }
}

fn path_check(name: &str, path: &Path, expect_dir: bool) -> DoctorCheck {
    match fs::metadata(path) {
        Ok(metadata) if expect_dir && metadata.is_dir() => {
            DoctorCheck::ok(name, &format!("present directory: {}", path.display()))
        }
        Ok(metadata) if !expect_dir && metadata.is_file() => {
            DoctorCheck::ok(name, &format!("present file: {}", path.display()))
        }
        Ok(_) => DoctorCheck::fail(name, &format!("wrong type: {}", path.display())),
        Err(err) => DoctorCheck::fail(name, &format!("missing {} ({err})", path.display())),
    }
}

fn file_store_check(name: &str, path: &Path) -> DoctorCheck {
    match fs::metadata(path) {
        Ok(metadata) if metadata.is_file() => DoctorCheck::ok(
            name,
            &format!(
                "{} line(s), {}",
                count_file_lines(path),
                format_bytes(metadata.len())
            ),
        ),
        Ok(_) => DoctorCheck::warn(name, &format!("not a regular file: {}", path.display())),
        Err(err) if err.kind() == io::ErrorKind::NotFound => {
            DoctorCheck::ok(name, &format!("not created yet: {}", path.display()))
        }
        Err(err) => DoctorCheck::warn(name, &format!("unreadable {} ({err})", path.display())),
    }
}

fn count_file_lines(path: &Path) -> usize {
    fs::read_to_string(path)
        .map(|text| text.lines().count())
        .unwrap_or_default()
}

fn tool_check(name: &str) -> DoctorCheck {
    match command_path(name) {
        Some(path) => DoctorCheck::ok(&format!("tool:{name}"), &path),
        None => DoctorCheck::warn(&format!("tool:{name}"), "not found on PATH"),
    }
}

fn command_path(name: &str) -> Option<String> {
    let output = Command::new("sh")
        .args(["-lc", &format!("command -v {}", shell_safe_tool_name(name))])
        .output()
        .ok()?;
    if !output.status.success() {
        return None;
    }
    let stdout = String::from_utf8_lossy(&output.stdout);
    stdout
        .lines()
        .next()
        .map(str::trim)
        .filter(|line| !line.is_empty())
        .map(str::to_string)
}

fn shell_safe_tool_name(name: &str) -> &str {
    if name
        .chars()
        .all(|ch| ch.is_ascii_alphanumeric() || matches!(ch, '-' | '_'))
    {
        name
    } else {
        ""
    }
}

fn env_check(name: &str, sensitive: bool) -> DoctorCheck {
    match env::var(name) {
        Ok(value) if value.is_empty() => DoctorCheck::warn(&format!("env:{name}"), "set but empty"),
        Ok(value)
            if matches!(
                name,
                "SEKAILINK_CORE_ACCESS_REMOTE_READONLY"
                    | "SEKAILINK_CORE_ACCESS_REMOTE_MUTATION"
                    | NEXUS_MUTATION_ENV
            ) && value != "1" =>
        {
            DoctorCheck::warn(&format!("env:{name}"), "set but not enabled")
        }
        Ok(_) if sensitive => DoctorCheck::ok(&format!("env:{name}"), "set (value hidden)"),
        Ok(value) => DoctorCheck::ok(
            &format!("env:{name}"),
            &format!("set to {}", public_env_display(&value)),
        ),
        Err(_) if sensitive => DoctorCheck::warn(&format!("env:{name}"), "missing"),
        Err(_) => DoctorCheck::ok(&format!("env:{name}"), "not set"),
    }
}

fn public_env_display(value: &str) -> String {
    value
        .chars()
        .map(|ch| if ch.is_ascii_graphic() { ch } else { '?' })
        .take(48)
        .collect()
}

fn pdf_check(name: &str, path: &Path) -> DoctorCheck {
    match fs::metadata(path) {
        Ok(metadata) if metadata.is_file() && metadata.len() > 0 => {
            DoctorCheck::ok(&format!("pdf:{name}"), &format_bytes(metadata.len()))
        }
        Ok(_) => DoctorCheck::warn(&format!("pdf:{name}"), "missing or empty"),
        Err(err) if err.kind() == io::ErrorKind::NotFound => {
            DoctorCheck::warn(&format!("pdf:{name}"), "missing")
        }
        Err(err) => DoctorCheck::warn(&format!("pdf:{name}"), &format!("unreadable ({err})")),
    }
}

#[derive(Debug, Clone)]
struct ExportEntry {
    name: String,
    path: PathBuf,
    size: u64,
    modified: u64,
}

fn export_entries(root: &Path) -> io::Result<Vec<ExportEntry>> {
    let mut entries = Vec::new();
    if !root.exists() {
        return Ok(entries);
    }
    collect_export_entries(root, root, &mut entries)?;
    Ok(entries)
}

fn collect_export_entries(
    root: &Path,
    dir: &Path,
    entries: &mut Vec<ExportEntry>,
) -> io::Result<()> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();
        let metadata = entry.metadata()?;
        if metadata.is_dir() {
            collect_export_entries(root, &path, entries)?;
            continue;
        }
        if !metadata.is_file() {
            continue;
        }
        let name = path
            .strip_prefix(root)
            .unwrap_or(&path)
            .display()
            .to_string();
        entries.push(ExportEntry {
            name,
            path,
            size: metadata.len(),
            modified: modified_epoch(metadata.modified().unwrap_or(UNIX_EPOCH)),
        });
    }
    Ok(())
}

fn count_export_files(root: &Path) -> usize {
    export_entries(root)
        .map(|entries| entries.len())
        .unwrap_or_default()
}

fn modified_epoch(value: SystemTime) -> u64 {
    value
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_secs())
        .unwrap_or_default()
}

fn format_bytes(size: u64) -> String {
    const KB: u64 = 1024;
    const MB: u64 = KB * 1024;
    if size >= MB {
        format!("{:.1}M", size as f64 / MB as f64)
    } else if size >= KB {
        format!("{:.1}K", size as f64 / KB as f64)
    } else {
        format!("{size}B")
    }
}

#[cfg(test)]
mod ops_doctor_tests {
    use super::{format_bytes, modified_epoch};
    use std::time::UNIX_EPOCH;

    #[test]
    fn bytes_are_human_readable() {
        assert_eq!(format_bytes(0), "0B");
        assert_eq!(format_bytes(1024), "1.0K");
        assert_eq!(format_bytes(1024 * 1024), "1.0M");
    }

    #[test]
    fn unix_epoch_is_stable() {
        assert_eq!(modified_epoch(UNIX_EPOCH), 0);
    }
}

#[cfg(test)]
mod app_tests {
    use super::{
        DiagnosticsArgs, LogExportOptions, log_filter_search_term, parse_banner_slot,
        render_logs_export,
    };

    #[test]
    fn banner_slot_is_limited_to_three_slots() {
        assert_eq!(parse_banner_slot("1").unwrap(), 1);
        assert!(parse_banner_slot("0").is_err());
        assert!(parse_banner_slot("4").is_err());
    }

    #[test]
    fn logs_export_includes_only_log_notes() {
        let parts = vec![
            "logs".to_string(),
            "export".to_string(),
            "--format".to_string(),
            "md".to_string(),
        ];
        let options = LogExportOptions::from_parts(&parts);
        let notes = "{\"target\":\"log:link:room-runtime\",\"text\":\"kept\"}\n{\"target\":\"user:certo\",\"text\":\"ignored\"}\n";
        let body = render_logs_export("", notes, &options);
        assert!(body.contains("kept"));
        assert!(!body.contains("ignored"));
    }

    #[test]
    fn log_filter_terms_strip_known_labels() {
        assert_eq!(log_filter_search_term("user:certo"), "certo");
        assert_eq!(log_filter_search_term("room:abc"), "abc");
        assert_eq!(log_filter_search_term("plain"), "plain");
        assert_eq!(log_filter_search_term("unknown:value"), "unknown:value");
    }

    #[test]
    fn incident_label_rejects_spaces() {
        assert!(super::normalize_incident_label("stream-night-1").is_ok());
        assert!(super::normalize_incident_label("stream night").is_err());
    }

    #[test]
    fn incident_export_collects_related_records() {
        let events = "{\"label\":\"stream-1\",\"event\":\"open\"}\n{\"label\":\"other\",\"event\":\"open\"}\n";
        let notes = "{\"target\":\"incident:stream-1\",\"text\":\"note\"}\n";
        let pins = "{\"source\":\"link:room-runtime\",\"text\":\"incident:stream-1 pin\"}\n";
        let body = super::render_incident_export("stream-1", events, notes, pins);
        assert!(body.contains("stream-1"));
        assert!(body.contains("note"));
        assert!(body.contains("pin"));
        assert!(!body.contains("other"));
    }

    #[test]
    fn timeline_extracts_json_timestamp() {
        assert_eq!(
            super::extract_json_u64("{\"ts\":123,\"event\":\"open\"}", "ts"),
            Some(123)
        );
    }

    #[test]
    fn open_incident_labels_exclude_closed_labels() {
        let incidents = "{\"label\":\"a\",\"event\":\"open\"}\n{\"label\":\"b\",\"event\":\"open\"}\n{\"label\":\"a\",\"event\":\"close\"}\n";
        let mut body = String::new();
        super::push_open_incident_labels(&mut body, incidents);
        assert!(!body.contains("`a`"));
        assert!(body.contains("`b`"));
    }

    #[test]
    fn diagnostics_args_normalize_include_aliases() {
        let parts = vec![
            "client".to_string(),
            "diagnostics-request".to_string(),
            "player".to_string(),
            "incident".to_string(),
            "reason".to_string(),
            "--include".to_string(),
            "client,sekaiemu,sklmi,bogus".to_string(),
        ];
        let args = DiagnosticsArgs::from_parts(&parts);
        assert_eq!(args.include, vec!["client-core", "sekaiemu", "sklmi"]);
    }

    #[test]
    fn diagnostics_args_default_include_is_complete() {
        let parts = vec![
            "client".to_string(),
            "diagnostics-request".to_string(),
            "player".to_string(),
            "incident".to_string(),
            "reason".to_string(),
        ];
        let args = DiagnosticsArgs::from_parts(&parts);
        assert!(args.include.contains(&"client-core".to_string()));
        assert!(args.include.contains(&"sekaiemu".to_string()));
        assert!(args.include.contains(&"sklmi".to_string()));
        assert!(args.include.contains(&"configs".to_string()));
        assert!(args.include.contains(&"system".to_string()));
    }
}

fn print_log_catalog(mode: Option<&str>) {
    if mode == Some("--by-server") {
        for (server, services) in known_services() {
            println!("{server}:");
            for service in *services {
                println!("  service:{service}");
            }
            for (source, log_server, description) in log_catalog() {
                if log_server.eq_ignore_ascii_case(server) {
                    println!("  log:{source:<22} {description}");
                }
            }
        }
        return;
    }

    if mode == Some("--by-incident") {
        println!("{:<18} {:<24} Useful when", "INCIDENT", "PRIMARY LOGS");
        println!("{}", "-".repeat(84));
        println!(
            "{:<18} {:<24} login, token, route_not_found, session/device issues",
            "identity", "nexus:identity link:chat-api"
        );
        println!(
            "{:<18} {:<24} lobby chat, readiness, client refresh, release endpoint issues",
            "lobby", "link:chat-api client:reports"
        );
        println!(
            "{:<18} {:<24} AP item routing, room disconnects, SKLMI runtime reports",
            "room", "link:room-runtime client:reports"
        );
        println!(
            "{:<18} {:<24} generation queue, ALTTP package creation, worker failures",
            "worlds", "worlds:generation"
        );
        println!(
            "{:<18} {:<24} installer/update manifests, pack CDN, release SHA checks",
            "release", "evolution:cdn link:chat-api"
        );
        return;
    }

    println!("{:<24} {:<12} Description", "SOURCE", "SERVER");
    println!("{}", "-".repeat(76));
    for (source, server, description) in log_catalog() {
        println!("{source:<24} {server:<12} {description}");
    }
    println!();
    println!("Known service targets:");
    for (server, services) in known_services() {
        println!("  {server:<10} {}", services.join(", "));
    }
}

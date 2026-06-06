use crate::audit::{
    Session, append_approval_decision, append_approval_request, append_audit, append_history,
    append_log_pin, append_note, read_file_to_string, write_client_banner_draft, write_export,
    write_export_with_extension, write_maintenance_draft, write_pack_repo, write_schedule_job,
};
use crate::commands::{COMMANDS, Confirmation, command_names, find_command, search_commands};
use crate::line_editor::LineEditor;
use crate::nexus::{
    LobbyListFilter, ProtectedGetPlan, admin_agent_services_plan, execute_protected_get,
    identity_user_plan, lobby_list_plan, lobby_open_plan, non_flag_args,
};
use crate::rbac::Role;
use crate::system::{
    known_services, log_catalog, render_dashboard, render_health_probe_plan,
    render_log_filter_plan, render_log_search_plan, render_log_tail_plan, render_server_logs_plan,
};
use crate::tui::TuiExit;
use crate::util::{home_dir, split_command_line};
use std::env;
use std::io::{self, IsTerminal};
use std::path::PathBuf;
use std::process::Command;

pub fn run() -> Result<(), Box<dyn std::error::Error>> {
    let options = Options::parse(env::args().skip(1).collect())?;
    let linux_user = env::var("USER").unwrap_or_else(|_| "unknown-linux-user".to_string());
    let sekailink_user = options
        .user
        .or_else(|| env::var("SEKAILINK_CORE_ACCESS_USER").ok())
        .unwrap_or_else(|| linux_user.clone());
    let role = options
        .role
        .or_else(|| env::var("SEKAILINK_CORE_ACCESS_ROLE").ok().and_then(|value| Role::parse(&value)))
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
                    data_dir = Some(PathBuf::from(args.get(i).ok_or("--data-dir requires a value")?));
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
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("search") | Some("open") | Some("sessions") | Some("devices") | Some("audit")
                ) =>
            {
                self.user_readonly_plan(&parsed)?;
                append_audit(&self.session, line, "ok", "user read-only nexus plan")?;
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
                self.export_jsonl("audit", &self.session.audit_dir().join("core-access.jsonl"), query, requested_name)?;
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
            "ops" if parsed.get(1).map(String::as_str) == Some("snapshot") => {
                self.ops_snapshot(parsed.get(2).map(String::as_str))?;
                append_audit(&self.session, line, "ok", "ops snapshot")?;
                true
            }
            "client-banner"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("list") | Some("preview") | Some("edit")
                ) =>
            {
                self.client_banner(&parsed)?;
                append_audit(&self.session, line, "ok", "client-banner")?;
                true
            }
            "maintenance"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("status") | Some("schedule") | Some("history")
                ) =>
            {
                self.maintenance(&parsed)?;
                append_audit(&self.session, line, "ok", "maintenance local")?;
                true
            }
            "schedule"
                if matches!(
                    parsed.get(1).map(String::as_str),
                    Some("list") | Some("calendar") | Some("add") | Some("history")
                ) =>
            {
                self.schedule(&parsed)?;
                append_audit(&self.session, line, "ok", "schedule local")?;
                true
            }
            "pack"
                if parsed.get(1).map(String::as_str) == Some("repo")
                    && matches!(parsed.get(2).map(String::as_str), Some("list") | Some("add"))
                    || parsed.get(1).map(String::as_str) == Some("schedule-check") =>
            {
                self.pack(&parsed)?;
                append_audit(&self.session, line, "ok", "pack local")?;
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
        for line in text.lines().filter(|line| query.is_empty() || line.contains(query)).take(200) {
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
        for line in text.lines().filter(|line| query.is_empty() || line.contains(query)) {
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

    fn render_or_execute_remote_plan(&self, label: &str, plan: &str, execute: bool) -> io::Result<()> {
        if !execute {
            println!("dry-run {label}:");
            println!("{plan}");
            println!("MVP note: command was not executed. Add --execute and set SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1 to run it.");
            return Ok(());
        }
        if env::var("SEKAILINK_CORE_ACCESS_REMOTE_READONLY").ok().as_deref() != Some("1") {
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
        self.render_or_execute_nexus_get(&plan, execute)
    }

    fn lobby_readonly(&self, parsed: &[String]) -> io::Result<()> {
        let execute = parsed.iter().any(|part| part == "--execute");
        match parsed.get(1).map(String::as_str) {
            Some("list") => {
                let args = non_flag_args(parsed, 2);
                if args.len() > 5 {
                    println!("usage: lobby list [limit] [query] [visibility] [status] [offset] [--execute]");
                    return Ok(());
                }
                let filter = LobbyListFilter::from_positionals(&args);
                let plan = lobby_list_plan(&filter);
                self.render_or_execute_nexus_get(&plan, execute)
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
                    Ok(plan) => self.render_or_execute_nexus_get(&plan, execute),
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

    fn render_or_execute_nexus_get(&self, plan: &ProtectedGetPlan, execute: bool) -> io::Result<()> {
        if !execute {
            println!("dry-run Nexus protected read-only request:");
            println!("{}", plan.render_dry_run());
            println!(
                "MVP note: command was not executed. Add --execute, set SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1, and set {}{} to run it.",
                plan.token_env,
                plan.fallback_token_env
                    .map(|env| format!(" or {env}"))
                    .unwrap_or_default()
            );
            return Ok(());
        }
        if env::var("SEKAILINK_CORE_ACCESS_REMOTE_READONLY").ok().as_deref() != Some("1") {
            println!("protected Nexus read-only execution blocked by environment gate");
            println!("set SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1 and rerun with --execute");
            println!("dry-run command:");
            println!("{}", plan.render_dry_run());
            return Ok(());
        }
        execute_protected_get(plan)
    }

    fn user_readonly_plan(&self, parsed: &[String]) -> io::Result<()> {
        let action = parsed.get(1).map(String::as_str).unwrap_or("");
        let execute = parsed.iter().any(|part| part == "--execute");
        let args = non_flag_args(parsed, 2);
        match identity_user_plan(action, &args) {
            Ok(plan) => {
                self.render_or_execute_nexus_get(&plan, execute)?;
            }
            Err(err) => println!("{err}"),
        }
        Ok(())
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
        for line in text.lines().filter(|line| query.is_empty() || line.contains(query)).take(200) {
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
        body.push_str(&format!("- SekaiLink user: {}\n", self.session.sekailink_user));
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
            _ => {
                println!("usage: client-banner list|preview|edit");
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
        read_file_to_string(&self.session.client_banners_dir().join(format!("slot-{slot}.txt")))
    }

    fn maintenance(&self, parsed: &[String]) -> io::Result<()> {
        match parsed.get(1).map(String::as_str) {
            Some("status") => {
                let current = read_file_to_string(&self.session.maintenance_dir().join("current.txt"))?;
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
                let history = read_file_to_string(&self.session.maintenance_dir().join("history.jsonl"))?;
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
            _ => {
                println!("usage: maintenance status|schedule|history");
                Ok(())
            }
        }
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
                let Some(name) = parsed.get(2) else {
                    println!("usage: schedule add <name> <when> <command>");
                    return Ok(());
                };
                let Some(when) = parsed.get(3) else {
                    println!("usage: schedule add <name> <when> <command>");
                    return Ok(());
                };
                let command = if parsed.len() > 4 {
                    parsed[4..].join(" ")
                } else {
                    String::new()
                };
                if command.trim().is_empty() {
                    println!("usage: schedule add <name> <when> <command>");
                    return Ok(());
                }
                let id = write_schedule_job(&self.session, name, when, &command)?;
                println!("schedule draft added: {id}");
                println!("MVP note: job is not armed; schedule run-now remains planned.");
                Ok(())
            }
            _ => {
                println!("usage: schedule list|calendar|add|history");
                Ok(())
            }
        }
    }

    fn pack(&self, parsed: &[String]) -> io::Result<()> {
        match (parsed.get(1).map(String::as_str), parsed.get(2).map(String::as_str)) {
            (Some("repo"), Some("list")) => {
                let repos = read_file_to_string(&self.session.pack_repos_dir().join("repos.jsonl"))?;
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
                let Some(id) = parsed.get(3) else {
                    println!("usage: pack repo add <id> <url> <game> [notes]");
                    return Ok(());
                };
                let Some(url) = parsed.get(4) else {
                    println!("usage: pack repo add <id> <url> <game> [notes]");
                    return Ok(());
                };
                let Some(game) = parsed.get(5) else {
                    println!("usage: pack repo add <id> <url> <game> [notes]");
                    return Ok(());
                };
                let notes = if parsed.len() > 6 {
                    parsed[6..].join(" ")
                } else {
                    String::new()
                };
                let record_id = write_pack_repo(&self.session, id, url, game, &notes)?;
                println!("pack repo draft added: {record_id}");
                println!("MVP note: draft only; no repo was fetched or published.");
                Ok(())
            }
            (Some("schedule-check"), _) => {
                let Some(id) = parsed.get(2) else {
                    println!("usage: pack schedule-check <repo-id> <when-or-interval>");
                    return Ok(());
                };
                let Some(when) = parsed.get(3) else {
                    println!("usage: pack schedule-check <repo-id> <when-or-interval>");
                    return Ok(());
                };
                let job_id = write_schedule_job(&self.session, &format!("pack-check-{id}"), when, &format!("pack check {id}"))?;
                println!("pack check schedule draft added: {job_id}");
                println!("MVP note: job is not armed; no pack repo was fetched.");
                Ok(())
            }
            _ => {
                println!("usage: pack repo list | pack repo add | pack schedule-check");
                Ok(())
            }
        }
    }

    fn stub_or_unknown(&self, line: &str, spec: Option<&crate::commands::CommandSpec>) -> io::Result<()> {
        if let Some(spec) = spec {
            println!("command recognized: {}", spec.name);
            println!("status: planned integration; no server mutation performed");
            println!("role: {} | confirmation: {:?}", spec.role.as_str(), spec.confirmation);
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

fn print_usage() {
    println!("sekailink-core-access [--shell] [--user USER] [--role service|admin] [--data-dir PATH] [--command COMMAND]");
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
    println!("  nexus services [--execute]");
    println!("  server logs <server> <service> [--follow] [--execute]");
    println!("  health probe [server|all] [--execute]");
    println!("  user search <query> [--execute]");
    println!("  user open <username> [--execute]");
    println!("  user sessions <username> [--execute]");
    println!("  user devices <username> [--execute]");
    println!("  user audit <username> [limit] [event_type] [offset] [--execute]");
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
    println!("  ops snapshot [label]");
    println!("  client-banner list");
    println!("  client-banner preview <1|2|3>");
    println!("  client-banner edit <1|2|3> <text>");
    println!("  maintenance status");
    println!("  maintenance schedule <scope> <start> <end> <message>");
    println!("  maintenance history");
    println!("  schedule list");
    println!("  schedule calendar");
    println!("  schedule add <name> <when> <command>");
    println!("  schedule history");
    println!("  pack repo list");
    println!("  pack repo add <id> <url> <game> [notes]");
    println!("  pack schedule-check <repo-id> <when-or-interval>");
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

fn looks_like_export_file(value: &str) -> bool {
    value.ends_with(".md")
        || value.ends_with(".txt")
        || value.ends_with(".json")
        || value.ends_with(".jsonl")
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
        !line.trim().is_empty()
            && (query.is_empty() || line.to_ascii_lowercase().contains(&query))
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

#[cfg(test)]
mod app_tests {
    use super::{LogExportOptions, log_filter_search_term, parse_banner_slot, render_logs_export};

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
            "identity",
            "nexus:identity link:chat-api"
        );
        println!(
            "{:<18} {:<24} lobby chat, readiness, client refresh, release endpoint issues",
            "lobby",
            "link:chat-api client:reports"
        );
        println!(
            "{:<18} {:<24} AP item routing, room disconnects, SKLMI runtime reports",
            "room",
            "link:room-runtime client:reports"
        );
        println!(
            "{:<18} {:<24} generation queue, ALTTP package creation, worker failures",
            "worlds",
            "worlds:generation"
        );
        println!(
            "{:<18} {:<24} installer/update manifests, pack CDN, release SHA checks",
            "release",
            "evolution:cdn link:chat-api"
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

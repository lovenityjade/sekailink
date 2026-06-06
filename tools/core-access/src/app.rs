use crate::audit::{
    Session, append_approval_decision, append_approval_request, append_audit, append_history,
    append_note, read_file_to_string, write_export,
};
use crate::commands::{COMMANDS, Confirmation, command_names, find_command, search_commands};
use crate::line_editor::LineEditor;
use crate::rbac::Role;
use crate::system::{
    known_services, log_catalog, render_dashboard, render_health_probe_plan, render_log_tail_plan,
    render_server_logs_plan,
};
use crate::util::{home_dir, split_command_line};
use std::env;
use std::io;
use std::path::PathBuf;

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

    app.run_interactive()
}

struct Options {
    command: Option<String>,
    role: Option<Role>,
    user: Option<String>,
    data_dir: Option<PathBuf>,
}

impl Options {
    fn parse(args: Vec<String>) -> Result<Self, String> {
        let mut command = None;
        let mut role = None;
        let mut user = None;
        let mut data_dir = None;
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
                println!("{}", render_dashboard());
                append_audit(&self.session, line, "ok", "server status")?;
                true
            }
            "server" if parsed.get(1).map(String::as_str) == Some("services") => {
                self.server_services(parsed.get(2).map(String::as_str));
                append_audit(&self.session, line, "ok", "server services")?;
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
            "health" if parsed.get(1).map(String::as_str) == Some("probe") => {
                self.health_probe(&parsed)?;
                append_audit(&self.session, line, "ok", "health probe dry-run")?;
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
            "maintenance" if parsed.get(1).map(String::as_str) == Some("status") => {
                println!("maintenance: unknown (Nexus integration pending)");
                println!("MVP note: no server state was read or changed.");
                append_audit(&self.session, line, "ok", "maintenance status stub")?;
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
        match render_server_logs_plan(server, service, follow) {
            Ok(plan) => {
                println!("dry-run remote log command:");
                println!("{plan}");
                println!("MVP note: command was not executed.");
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
        match render_log_tail_plan(source, follow) {
            Ok(plan) => {
                println!("dry-run remote log command:");
                println!("{plan}");
                println!("MVP note: command was not executed.");
            }
            Err(err) => println!("{err}"),
        }
        Ok(())
    }

    fn health_probe(&self, parsed: &[String]) -> io::Result<()> {
        let target = parsed.get(2).map(String::as_str).unwrap_or("all");
        match render_health_probe_plan(target) {
            Ok(plan) => {
                println!("dry-run health probe:");
                println!("{plan}");
                println!("MVP note: command was not executed.");
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

fn print_usage() {
    println!("sekailink-core-access [--user USER] [--role service|admin] [--data-dir PATH] [--command COMMAND]");
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
    println!("  server status all");
    println!("  server services [server|all]");
    println!("  server logs <server> <service> [--follow]");
    println!("  health probe [server|all]");
    println!("  logs list");
    println!("  logs list --by-server");
    println!("  logs list --by-incident");
    println!("  logs tail <source> [--follow]");
    println!("  audit search [query]");
    println!("  audit export [query] [file-name]");
    println!("  note add <target> <text>");
    println!("  note list [query]");
    println!("  note export [query] [file-name]");
    println!("  approval request <command> <reason>");
    println!("  approval list");
    println!("  approval approve <id> [reason]");
    println!("  exit");
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

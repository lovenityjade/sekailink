use std::fs;
use std::process::Command;

#[derive(Debug, Clone)]
pub struct LocalHealth {
    pub uptime: String,
    pub load: String,
    pub memory: String,
    pub disk: String,
}

pub fn local_health() -> LocalHealth {
    LocalHealth {
        uptime: read_uptime(),
        load: read_loadavg(),
        memory: read_memory(),
        disk: read_disk(),
    }
}

pub fn render_dashboard() -> String {
    let health = local_health();
    let mut out = String::new();
    out.push_str("SekaiLink Core Access :: Bastion Matrix\n");
    out.push_str("====================================================================\n");
    out.push_str("SERVER      STATUS     CPU/LOAD      RAM          DISK        UPTIME\n");
    out.push_str("--------------------------------------------------------------------\n");
    out.push_str(&format!(
        "{:<11} {:<10} {:<13} {:<12} {:<11} {}\n",
        "Nexus", "LOCAL", health.load, health.memory, health.disk, health.uptime
    ));
    for server in ["Link", "Worlds", "Evolution", "Pulse"] {
        out.push_str(&format!(
            "{:<11} {:<10} {:<13} {:<12} {:<11} {}\n",
            server, "PENDING", "ops-agent", "pending", "pending", "pending"
        ));
    }
    out.push_str("--------------------------------------------------------------------\n");
    out.push_str("MVP status: local shell/audit only. Remote service mutation disabled.\n");
    out
}

pub fn log_catalog() -> &'static [(&'static str, &'static str, &'static str)] {
    &[
        ("nexus:identity", "Nexus", "Identity/auth/admin audit logs"),
        ("nexus:db", "Nexus", "DB and backup logs"),
        (
            "link:chat-api",
            "Link",
            "Public API, lobby, chat, release-latest",
        ),
        (
            "link:room-runtime",
            "Link",
            "Room runtime and AP multiserver logs",
        ),
        (
            "worlds:generation",
            "Worlds",
            "Generation queue and worker logs",
        ),
        ("evolution:cdn", "Evolution", "CDN, packs, releases, nginx"),
        (
            "pulse:assistant",
            "Pulse",
            "Pulse health/config helper logs",
        ),
        (
            "client:reports",
            "Link",
            "Uploaded client diagnostics/reports",
        ),
        (
            "discord:bot",
            "Link",
            "Discord social bot status, announcements, command changes",
        ),
        (
            "twitch:assistant",
            "Link",
            "Twitch assistant status, announcements, command changes",
        ),
    ]
}

pub fn known_services() -> &'static [(&'static str, &'static [&'static str])] {
    &[
        (
            "nexus",
            &[
                "sekailink-link-lobby-runtime-tunnel",
                "sekailink-nexus-admin-agent",
                "sekailink-nexus-identity",
                "sekailink-nexus-lobby-admin",
                "sekailink-nexus-room-query",
                "sekailink-nexus-seed-config-api",
            ],
        ),
        (
            "link",
            &[
                "sekailink-admin-agent",
                "sekailink-chat-api",
                "sekailink-chat-daemon",
                "sekailink-chat-gateway",
                "sekailink-lobby-runtime",
                "sekailink-room-server",
                "sekailink-social-bots",
                "sekailink-twitch-assistant",
                "sekailink-webhost",
                "sekailink-workers",
            ],
        ),
        (
            "worlds",
            &[
                "sekailink-generation-server",
                "sekailink-smart",
                "sekailink-worlds-admin-agent",
            ],
        ),
        (
            "evolution",
            &[
                "sekailink-evolution-admin-agent",
                "sekailink-postfix-queue-state",
                "sekailink-postfix-tail",
            ],
        ),
        ("pulse", &["sekailink-pulse-llm", "sekailink-pulse-rag-api"]),
    ]
}

pub fn render_server_logs_plan(
    server: &str,
    service: &str,
    follow: bool,
) -> Result<String, String> {
    let server = normalize_server(server)?;
    if !service_allowed(server, service) {
        return Err(format!("service {service} is not allowlisted for {server}"));
    }
    let mode = if follow { "-f -n 200" } else { "-n 300" };
    let ssh_alias = ssh_alias_for(server);
    Ok(format!(
        "ssh {ssh_alias} -- journalctl -u {} {mode} --no-pager",
        shell_word(service)
    ))
}

pub fn render_log_tail_plan(source: &str, follow: bool) -> Result<String, String> {
    let (server, service) =
        log_source_service(source).ok_or_else(|| format!("unknown log source: {source}"))?;
    render_server_logs_plan(server, service, follow)
}

pub fn render_log_search_plan(source: &str, query: &str) -> Result<String, String> {
    render_log_filter_plan(source, &[query])
}

pub fn render_log_filter_plan(source: &str, filters: &[&str]) -> Result<String, String> {
    let clean_filters = filters
        .iter()
        .map(|filter| filter.trim())
        .filter(|filter| !filter.is_empty())
        .collect::<Vec<_>>();
    if clean_filters.is_empty() {
        return Err("logs search/filter requires at least one query term".to_string());
    }

    let sources = if source.trim().is_empty() || source == "all" {
        log_catalog()
            .iter()
            .map(|(source, _, _)| *source)
            .collect::<Vec<_>>()
    } else {
        vec![source]
    };

    let mut out = String::new();
    for source in sources {
        let plan = render_log_tail_plan(source, false)?;
        let pipeline = clean_filters
            .iter()
            .map(|filter| format!("grep -i -- {}", shell_word(filter)))
            .collect::<Vec<_>>()
            .join(" | ");
        out.push_str(&format!(
            "printf '\\n### {source}\\n'; {plan} | {pipeline} || true\n"
        ));
    }
    Ok(out.trim_end().to_string())
}

pub fn render_health_probe_plan(target: &str) -> Result<String, String> {
    let target = target.trim();
    if target.is_empty() || target == "all" {
        let mut out = String::new();
        for (server, services) in known_services() {
            out.push_str(&health_probe_for(server, services));
            out.push('\n');
        }
        return Ok(out);
    }
    let server = normalize_server(target)?;
    let services = services_for(server).ok_or_else(|| format!("unknown server: {server}"))?;
    Ok(health_probe_for(server, services))
}

pub fn service_allowed(server: &str, service: &str) -> bool {
    services_for(server)
        .map(|services| services.iter().any(|candidate| *candidate == service))
        .unwrap_or(false)
}

fn health_probe_for(server: &str, services: &[&str]) -> String {
    let ssh_alias = ssh_alias_for(server);
    let service_checks = services
        .iter()
        .map(|service| {
            format!(
                "printf \"{} \"; systemctl is-active {}",
                service,
                shell_word(service)
            )
        })
        .collect::<Vec<_>>()
        .join("; ");
    format!("ssh {ssh_alias} -- 'hostname; uptime; free -m; df -h /; {service_checks}'")
}

fn log_source_service(source: &str) -> Option<(&'static str, &'static str)> {
    match source {
        "nexus:identity" => Some(("nexus", "sekailink-nexus-identity")),
        "nexus:db" => Some(("nexus", "sekailink-nexus-seed-config-api")),
        "link:chat-api" => Some(("link", "sekailink-chat-api")),
        "link:room-runtime" => Some(("link", "sekailink-room-server")),
        "worlds:generation" => Some(("worlds", "sekailink-generation-server")),
        "evolution:cdn" => Some(("evolution", "sekailink-evolution-admin-agent")),
        "pulse:assistant" => Some(("pulse", "sekailink-pulse-rag-api")),
        "client:reports" => Some(("link", "sekailink-chat-api")),
        "discord:bot" => Some(("link", "sekailink-social-bots")),
        "twitch:assistant" => Some(("link", "sekailink-twitch-assistant")),
        _ => None,
    }
}

fn normalize_server(server: &str) -> Result<&'static str, String> {
    let clean = server.trim().to_ascii_lowercase();
    for (known, _) in known_services() {
        if clean == *known {
            return Ok(*known);
        }
    }
    Err(format!("unknown server: {server}"))
}

fn services_for(server: &str) -> Option<&'static [&'static str]> {
    known_services()
        .iter()
        .find(|(known, _)| *known == server)
        .map(|(_, services)| *services)
}

fn ssh_alias_for(server: &str) -> &'static str {
    match server {
        "nexus" => "nexus-vps",
        "link" => "link-vps",
        "worlds" => "worlds-vps",
        "evolution" => "evolution-vps",
        "pulse" => "pulse-vps",
        _ => "unknown-vps",
    }
}

fn shell_word(value: &str) -> String {
    if value
        .chars()
        .all(|ch| ch.is_ascii_alphanumeric() || matches!(ch, '-' | '_' | '.' | ':'))
    {
        value.to_string()
    } else {
        format!("'{}'", value.replace('\'', "'\\''"))
    }
}

fn read_uptime() -> String {
    let Ok(raw) = fs::read_to_string("/proc/uptime") else {
        return "unknown".to_string();
    };
    let secs = raw
        .split_whitespace()
        .next()
        .and_then(|value| value.parse::<f64>().ok())
        .unwrap_or_default() as u64;
    let days = secs / 86_400;
    let hours = (secs % 86_400) / 3_600;
    let minutes = (secs % 3_600) / 60;
    format!("{days}d {hours}h {minutes}m")
}

fn read_loadavg() -> String {
    fs::read_to_string("/proc/loadavg")
        .ok()
        .and_then(|raw| {
            let mut parts = raw.split_whitespace();
            Some(format!(
                "{}/{}/{}",
                parts.next()?,
                parts.next()?,
                parts.next()?
            ))
        })
        .unwrap_or_else(|| "unknown".to_string())
}

fn read_memory() -> String {
    let Ok(raw) = fs::read_to_string("/proc/meminfo") else {
        return "unknown".to_string();
    };
    let mut total = 0_u64;
    let mut available = 0_u64;
    for line in raw.lines() {
        if line.starts_with("MemTotal:") {
            total = parse_meminfo_kb(line);
        } else if line.starts_with("MemAvailable:") {
            available = parse_meminfo_kb(line);
        }
    }
    if total == 0 {
        return "unknown".to_string();
    }
    let used = total.saturating_sub(available);
    let pct = used.saturating_mul(100) / total;
    format!("{pct}%")
}

fn parse_meminfo_kb(line: &str) -> u64 {
    line.split_whitespace()
        .nth(1)
        .and_then(|value| value.parse().ok())
        .unwrap_or_default()
}

fn read_disk() -> String {
    let output = Command::new("df").args(["-P", "/"]).output();
    let Ok(output) = output else {
        return "unknown".to_string();
    };
    if !output.status.success() {
        return "unknown".to_string();
    }
    let text = String::from_utf8_lossy(&output.stdout);
    text.lines()
        .nth(1)
        .and_then(|line| line.split_whitespace().nth(4))
        .unwrap_or("unknown")
        .to_string()
}

#[cfg(test)]
mod tests {
    use super::{render_log_search_plan, render_server_logs_plan, service_allowed};

    #[test]
    fn server_logs_plan_uses_allowlisted_service() {
        let plan = render_server_logs_plan("link", "sekailink-chat-api", true).unwrap();
        assert!(plan.contains("ssh link-vps"));
        assert!(plan.contains("journalctl -u sekailink-chat-api"));
        assert!(service_allowed("link", "sekailink-room-server"));
    }

    #[test]
    fn server_logs_plan_rejects_unknown_service() {
        let err = render_server_logs_plan("link", "postgresql", false).unwrap_err();
        assert!(err.contains("not allowlisted"));
    }

    #[test]
    fn log_search_plan_greps_allowlisted_source() {
        let plan = render_log_search_plan("link:room-runtime", "sklmi runtime").unwrap();
        assert!(plan.contains("ssh link-vps"));
        assert!(plan.contains("journalctl -u sekailink-room-server"));
        assert!(plan.contains("grep -i -- 'sklmi runtime'"));
    }
}

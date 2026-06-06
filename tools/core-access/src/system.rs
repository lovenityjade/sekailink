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
        "Nexus",
        "LOCAL",
        health.load,
        health.memory,
        health.disk,
        health.uptime
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
        ("link:chat-api", "Link", "Public API, lobby, chat, release-latest"),
        ("link:room-runtime", "Link", "Room runtime and AP multiserver logs"),
        ("worlds:generation", "Worlds", "Generation queue and worker logs"),
        ("evolution:cdn", "Evolution", "CDN, packs, releases, nginx"),
        ("pulse:assistant", "Pulse", "Pulse health/config helper logs"),
        ("client:reports", "Link", "Uploaded client diagnostics/reports"),
    ]
}

pub fn known_services() -> &'static [(&'static str, &'static [&'static str])] {
    &[
        ("nexus", &["sekailink-identity", "sekailink-seed-config"]),
        ("link", &["sekailink-chat-api", "sekailink-room-runtime", "nginx"]),
        ("worlds", &["sekailink-worlds", "generation-worker"]),
        ("evolution", &["nginx", "pack-publisher", "release-publisher"]),
        ("pulse", &["sekailink-pulse"]),
    ]
}

pub fn render_server_logs_plan(server: &str, service: &str, follow: bool) -> Result<String, String> {
    let server = normalize_server(server)?;
    if !service_allowed(server, service) {
        return Err(format!("service {service} is not allowlisted for {server}"));
    }
    let mode = if follow { "-f -n 200" } else { "-n 300" };
    Ok(format!(
        "ssh {server} -- journalctl -u {} {mode} --no-pager",
        shell_word(service)
    ))
}

pub fn render_log_tail_plan(source: &str, follow: bool) -> Result<String, String> {
    let (server, service) = log_source_service(source)
        .ok_or_else(|| format!("unknown log source: {source}"))?;
    render_server_logs_plan(server, service, follow)
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
    let service_checks = services
        .iter()
        .map(|service| format!("systemctl is-active {}", shell_word(service)))
        .collect::<Vec<_>>()
        .join("; ");
    format!(
        "ssh {server} -- 'hostname; uptime; free -m; df -h /; {service_checks}'"
    )
}

fn log_source_service(source: &str) -> Option<(&'static str, &'static str)> {
    match source {
        "nexus:identity" => Some(("nexus", "sekailink-identity")),
        "nexus:db" => Some(("nexus", "sekailink-seed-config")),
        "link:chat-api" => Some(("link", "sekailink-chat-api")),
        "link:room-runtime" => Some(("link", "sekailink-room-runtime")),
        "worlds:generation" => Some(("worlds", "sekailink-worlds")),
        "evolution:cdn" => Some(("evolution", "nginx")),
        "pulse:assistant" => Some(("pulse", "sekailink-pulse")),
        "client:reports" => Some(("link", "sekailink-chat-api")),
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
    use super::{render_server_logs_plan, service_allowed};

    #[test]
    fn server_logs_plan_uses_allowlisted_service() {
        let plan = render_server_logs_plan("link", "sekailink-chat-api", true).unwrap();
        assert!(plan.contains("ssh link"));
        assert!(plan.contains("journalctl -u sekailink-chat-api"));
        assert!(service_allowed("link", "sekailink-room-runtime"));
    }

    #[test]
    fn server_logs_plan_rejects_unknown_service() {
        let err = render_server_logs_plan("link", "postgresql", false).unwrap_err();
        assert!(err.contains("not allowlisted"));
    }
}

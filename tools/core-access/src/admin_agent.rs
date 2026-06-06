use std::env;
use std::io::{self, Write};
use std::process::{Command, Stdio};

const AGENT_BASE: &str = "http://127.0.0.1:19091";
const GENERIC_AGENT_TOKEN_ENV: &str = "SEKAILINK_CORE_ACCESS_AGENT_ADMIN_TOKEN";
const REMOTE_READONLY_ENV: &str = "SEKAILINK_CORE_ACCESS_REMOTE_READONLY";
const REMOTE_MUTATION_ENV: &str = "SEKAILINK_CORE_ACCESS_REMOTE_MUTATION";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AgentMethod {
    Get,
    Post,
}

impl AgentMethod {
    fn as_str(self) -> &'static str {
        match self {
            Self::Get => "GET",
            Self::Post => "POST",
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct AgentServer {
    pub id: &'static str,
    pub display: &'static str,
    pub ssh_alias: &'static str,
    pub token_env: &'static str,
    pub fallback_env: Option<&'static str>,
}

#[derive(Debug, Clone)]
pub struct AgentRequestPlan {
    pub title: String,
    pub detail: String,
    pub server: AgentServer,
    pub method: AgentMethod,
    pub path: String,
    pub token_required: bool,
    pub mutation_confirm: Option<String>,
}

impl AgentRequestPlan {
    pub fn render_dry_run(&self) -> String {
        let token_line = if self.token_required {
            format!(
                "headers: Authorization: Bearer ${}{}; X-SekaiLink-Client: core-access",
                self.server.token_env,
                self.server
                    .fallback_env
                    .map(|env| format!("; fallback ${env}"))
                    .unwrap_or_default()
            )
        } else {
            "headers: public loopback health request; no bearer token required".to_string()
        };
        format!(
            "{}\n{}\ntransport: ssh {} -- sh -s\nrequest: {} {}\n{}",
            self.title,
            self.detail,
            self.server.ssh_alias,
            self.method.as_str(),
            shell_word(&format!("{AGENT_BASE}{}", self.path)),
            token_line,
        )
    }
}

pub fn agent_servers() -> &'static [AgentServer] {
    &[
        AgentServer {
            id: "nexus",
            display: "Nexus",
            ssh_alias: "nexus-vps",
            token_env: "SEKAILINK_CORE_ACCESS_NEXUS_AGENT_ADMIN_TOKEN",
            fallback_env: Some("SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN"),
        },
        AgentServer {
            id: "link",
            display: "Link",
            ssh_alias: "link-vps",
            token_env: "SEKAILINK_CORE_ACCESS_LINK_AGENT_ADMIN_TOKEN",
            fallback_env: Some(GENERIC_AGENT_TOKEN_ENV),
        },
        AgentServer {
            id: "worlds",
            display: "Worlds",
            ssh_alias: "worlds-vps",
            token_env: "SEKAILINK_CORE_ACCESS_WORLDS_AGENT_ADMIN_TOKEN",
            fallback_env: Some(GENERIC_AGENT_TOKEN_ENV),
        },
        AgentServer {
            id: "evolution",
            display: "Evolution",
            ssh_alias: "evolution-vps",
            token_env: "SEKAILINK_CORE_ACCESS_EVOLUTION_AGENT_ADMIN_TOKEN",
            fallback_env: Some(GENERIC_AGENT_TOKEN_ENV),
        },
    ]
}

pub fn find_agent_server(server: &str) -> Result<AgentServer, String> {
    let clean = server.trim().to_ascii_lowercase();
    agent_servers()
        .iter()
        .copied()
        .find(|candidate| clean == candidate.id || clean == candidate.display.to_ascii_lowercase())
        .ok_or_else(|| format!("admin-agent is not configured for server: {server}"))
}

pub fn agent_health_plan(server: AgentServer) -> AgentRequestPlan {
    AgentRequestPlan {
        title: format!("{} admin-agent health", server.display),
        detail: "GET /health on the loopback admin-agent through SSH.".to_string(),
        server,
        method: AgentMethod::Get,
        path: "/health".to_string(),
        token_required: false,
        mutation_confirm: None,
    }
}

pub fn agent_system_plan(server: AgentServer) -> AgentRequestPlan {
    AgentRequestPlan {
        title: format!("{} admin-agent system matrix", server.display),
        detail: "GET /system on the loopback admin-agent through SSH.".to_string(),
        server,
        method: AgentMethod::Get,
        path: "/system".to_string(),
        token_required: true,
        mutation_confirm: None,
    }
}

pub fn agent_services_plan(server: AgentServer) -> AgentRequestPlan {
    AgentRequestPlan {
        title: format!("{} admin-agent services", server.display),
        detail: "GET /services on the loopback admin-agent through SSH.".to_string(),
        server,
        method: AgentMethod::Get,
        path: "/services".to_string(),
        token_required: true,
        mutation_confirm: None,
    }
}

pub fn agent_service_plan(server: AgentServer, service: &str) -> Result<AgentRequestPlan, String> {
    let service = clean_segment(service, "service")?;
    Ok(AgentRequestPlan {
        title: format!("{} admin-agent service: {service}", server.display),
        detail: "GET /services/{service} on the loopback admin-agent through SSH.".to_string(),
        server,
        method: AgentMethod::Get,
        path: format!("/services/{}", percent_encode_segment(&service)),
        token_required: true,
        mutation_confirm: None,
    })
}

pub fn agent_logs_plan(server: AgentServer, service: &str) -> Result<AgentRequestPlan, String> {
    let service = clean_segment(service, "service")?;
    Ok(AgentRequestPlan {
        title: format!("{} admin-agent logs: {service}", server.display),
        detail: "GET /services/{service}/logs on the loopback admin-agent through SSH.".to_string(),
        server,
        method: AgentMethod::Get,
        path: format!("/services/{}/logs", percent_encode_segment(&service)),
        token_required: true,
        mutation_confirm: None,
    })
}

pub fn agent_control_plan(
    server: AgentServer,
    service: &str,
    action: &str,
) -> Result<AgentRequestPlan, String> {
    let service = clean_segment(service, "service")?;
    let action = match action.trim() {
        "restart" | "start" | "stop" => action.trim().to_string(),
        other => return Err(format!("unsupported service action: {other}")),
    };
    let confirm = format!("{}:{service}:{action}", server.id);
    Ok(AgentRequestPlan {
        title: format!("{} admin-agent {action}: {service}", server.display),
        detail: "POST /services/{service}/{action} on the loopback admin-agent through SSH."
            .to_string(),
        server,
        method: AgentMethod::Post,
        path: format!(
            "/services/{}/{}",
            percent_encode_segment(&service),
            percent_encode_segment(&action)
        ),
        token_required: true,
        mutation_confirm: Some(confirm),
    })
}

pub fn execute_agent_request(plan: &AgentRequestPlan, confirm: Option<&str>) -> io::Result<()> {
    if plan.method == AgentMethod::Post {
        let expected = plan.mutation_confirm.as_deref().unwrap_or("");
        if env::var(REMOTE_MUTATION_ENV).ok().as_deref() != Some("1") {
            println!("admin-agent mutation blocked by environment gate");
            println!("set {REMOTE_MUTATION_ENV}=1 and rerun with --execute");
            println!("required confirmation: --confirm {expected}");
            println!("dry-run command:");
            println!("{}", plan.render_dry_run());
            return Ok(());
        }
        if confirm != Some(expected) {
            println!("admin-agent mutation blocked by confirmation gate");
            println!("required confirmation: --confirm {expected}");
            println!("dry-run command:");
            println!("{}", plan.render_dry_run());
            return Ok(());
        }
    } else if env::var(REMOTE_READONLY_ENV).ok().as_deref() != Some("1") {
        println!("admin-agent read-only execution blocked by environment gate");
        println!("set {REMOTE_READONLY_ENV}=1 and rerun with --execute");
        println!("dry-run command:");
        println!("{}", plan.render_dry_run());
        return Ok(());
    }

    let token = if plan.token_required {
        match read_token(plan) {
            Some((source, token)) => {
                println!("token source: {source} (value hidden)");
                Some(token)
            }
            None => {
                println!(
                    "admin-agent request blocked: missing {}{}",
                    plan.server.token_env,
                    plan.server
                        .fallback_env
                        .map(|env| format!(" or {env}"))
                        .unwrap_or_default()
                );
                println!("dry-run command:");
                println!("{}", plan.render_dry_run());
                return Ok(());
            }
        }
    } else {
        None
    };

    if token
        .as_deref()
        .map(|value| value.contains('\n') || value.contains('\r'))
        .unwrap_or(false)
    {
        println!("admin-agent request blocked: token contains a newline");
        return Ok(());
    }

    println!("executing admin-agent request:");
    println!("{}", plan.title);

    let url = format!("{AGENT_BASE}{}", plan.path);
    let mut curl_config = String::new();
    if let Some(token) = token {
        curl_config.push_str(&format!(
            "header = \"{}\"\n",
            curl_config_escape(&format!("Authorization: Bearer {token}"))
        ));
    }
    curl_config.push_str("header = \"User-Agent: SekaiLinkCoreAccess/0.1\"\n");
    curl_config.push_str("header = \"X-SekaiLink-Client: core-access\"\n");
    curl_config.push_str("header = \"X-SekaiLink-Client-Version: 0.1.0\"\n");

    let script = format!(
        "set -eu\ncurl -fsS --connect-timeout 5 --max-time 20 --request {} --config - {} <<'__SEKAILINK_CURL__'\n{}__SEKAILINK_CURL__\nprintf '\\n'\n",
        plan.method.as_str(),
        shell_word(&url),
        curl_config,
    );

    let term = env::var("TERM")
        .ok()
        .filter(|value| value != "dumb")
        .unwrap_or_else(|| "xterm-256color".to_string());
    let mut child = Command::new("ssh")
        .arg(plan.server.ssh_alias)
        .arg("--")
        .arg("sh")
        .arg("-s")
        .env("TERM", term)
        .stdin(Stdio::piped())
        .spawn()?;
    if let Some(mut stdin) = child.stdin.take() {
        stdin.write_all(script.as_bytes())?;
    }
    let status = child.wait()?;
    println!("admin-agent command exit status: {status}");
    Ok(())
}

fn read_token(plan: &AgentRequestPlan) -> Option<(&'static str, String)> {
    for env_name in [Some(plan.server.token_env), plan.server.fallback_env]
        .into_iter()
        .flatten()
    {
        if let Some(token) = env::var(env_name)
            .ok()
            .map(|value| value.trim().to_string())
            .filter(|value| !value.is_empty())
        {
            return Some((env_name, token));
        }
    }
    None
}

fn clean_segment(value: &str, label: &str) -> Result<String, String> {
    let clean = value.trim();
    if clean.is_empty() {
        return Err(format!("{label} is required"));
    }
    if !clean
        .chars()
        .all(|ch| ch.is_ascii_alphanumeric() || matches!(ch, '-' | '_' | '.' | ':'))
    {
        return Err(format!(
            "{label} may only use letters, numbers, dash, underscore, dot, or colon"
        ));
    }
    Ok(clean.to_string())
}

fn percent_encode_segment(value: &str) -> String {
    let mut out = String::new();
    for byte in value.bytes() {
        match byte {
            b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'-' | b'_' | b'.' | b'~' => {
                out.push(byte as char);
            }
            _ => out.push_str(&format!("%{byte:02X}")),
        }
    }
    out
}

fn curl_config_escape(value: &str) -> String {
    value.replace('\\', "\\\\").replace('"', "\\\"")
}

fn shell_word(value: &str) -> String {
    if value
        .chars()
        .all(|ch| ch.is_ascii_alphanumeric() || matches!(ch, '-' | '_' | '.' | ':' | '/'))
    {
        value.to_string()
    } else {
        format!("'{}'", value.replace('\'', "'\\''"))
    }
}

#[cfg(test)]
mod tests {
    use super::{
        agent_control_plan, agent_logs_plan, agent_servers, agent_services_plan, find_agent_server,
    };

    #[test]
    fn server_lookup_accepts_display_name() {
        let server = find_agent_server("Nexus").unwrap();
        assert_eq!(server.id, "nexus");
        assert_eq!(server.ssh_alias, "nexus-vps");
    }

    #[test]
    fn services_plan_uses_loopback_agent() {
        let server = find_agent_server("link").unwrap();
        let plan = agent_services_plan(server);
        assert!(plan.render_dry_run().contains("ssh link-vps"));
        assert!(
            plan.render_dry_run()
                .contains("GET http://127.0.0.1:19091/services")
        );
    }

    #[test]
    fn logs_plan_encodes_service_segment() {
        let server = find_agent_server("worlds").unwrap();
        let plan = agent_logs_plan(server, "generation-server").unwrap();
        assert!(plan.path.ends_with("/generation-server/logs"));
    }

    #[test]
    fn control_plan_requires_exact_confirm() {
        let server = find_agent_server("evolution").unwrap();
        let plan = agent_control_plan(server, "postfix", "restart").unwrap();
        assert_eq!(
            plan.mutation_confirm.as_deref(),
            Some("evolution:postfix:restart")
        );
    }

    #[test]
    fn pulse_has_no_agent_profile_yet() {
        assert!(find_agent_server("pulse").is_err());
        assert_eq!(agent_servers().len(), 4);
    }
}

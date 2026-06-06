use std::env;
use std::io::{self, Write};
use std::process::{Command, Stdio};

pub const NEXUS_TOKEN_ENV: &str = "SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN";
pub const NEXUS_AGENT_TOKEN_ENV: &str = "SEKAILINK_CORE_ACCESS_NEXUS_AGENT_ADMIN_TOKEN";
pub const NEXUS_LOBBY_TOKEN_ENV: &str = "SEKAILINK_CORE_ACCESS_NEXUS_LOBBY_ADMIN_TOKEN";
pub const NEXUS_IDENTITY_TOKEN_ENV: &str = "SEKAILINK_CORE_ACCESS_NEXUS_IDENTITY_ADMIN_TOKEN";

const SSH_ALIAS: &str = "nexus-vps";
const ADMIN_AGENT_BASE: &str = "http://127.0.0.1:19091";
const LOBBY_ADMIN_BASE: &str = "http://127.0.0.1:19096";
const IDENTITY_ADMIN_BASE: &str = "http://149.202.61.90:19095";

#[derive(Debug, Clone)]
pub struct ProtectedGetPlan {
    pub title: String,
    pub url: String,
    pub detail: String,
    pub token_env: &'static str,
    pub fallback_token_env: Option<&'static str>,
}

impl ProtectedGetPlan {
    fn new(
        title: impl Into<String>,
        url: impl Into<String>,
        detail: impl Into<String>,
        token_env: &'static str,
    ) -> Self {
        Self {
            title: title.into(),
            url: url.into(),
            detail: detail.into(),
            token_env,
            fallback_token_env: (token_env != NEXUS_TOKEN_ENV).then_some(NEXUS_TOKEN_ENV),
        }
    }

    pub fn render_dry_run(&self) -> String {
        let fallback = self
            .fallback_token_env
            .map(|env| format!("; fallback ${env}"))
            .unwrap_or_default();
        format!(
            "{}\n{}\ntransport: ssh {} -- sh -s\nrequest: curl -fsS --connect-timeout 5 --max-time 20 --config - {}\nheaders: Authorization: Bearer ${}{}; X-SekaiLink-Client: core-access",
            self.title,
            self.detail,
            SSH_ALIAS,
            shell_word(&self.url),
            self.token_env,
            fallback,
        )
    }
}

#[derive(Debug, Clone, Default)]
pub struct LobbyListFilter {
    pub limit: Option<String>,
    pub query: Option<String>,
    pub visibility: Option<String>,
    pub status: Option<String>,
    pub offset: Option<String>,
}

impl LobbyListFilter {
    pub fn from_positionals(values: &[String]) -> Self {
        Self {
            limit: non_empty(values.first()),
            query: non_empty(values.get(1)),
            visibility: non_empty(values.get(2)),
            status: non_empty(values.get(3)),
            offset: non_empty(values.get(4)),
        }
    }
}

pub fn admin_agent_services_plan() -> ProtectedGetPlan {
    ProtectedGetPlan::new(
        "Nexus admin-agent service inventory",
        format!("{ADMIN_AGENT_BASE}/services"),
        "GET /services on the private Nexus admin-agent.",
        NEXUS_AGENT_TOKEN_ENV,
    )
}

pub fn lobby_list_plan(filter: &LobbyListFilter) -> ProtectedGetPlan {
    let mut params = Vec::new();
    push_query(&mut params, "limit", filter.limit.as_deref());
    push_query(&mut params, "query", filter.query.as_deref());
    push_query(&mut params, "visibility", filter.visibility.as_deref());
    push_query(&mut params, "status", filter.status.as_deref());
    push_query(&mut params, "offset", filter.offset.as_deref());
    let suffix = if params.is_empty() {
        String::new()
    } else {
        format!("?{}", params.join("&"))
    };
    ProtectedGetPlan::new(
        "Nexus lobby-admin lobby inventory",
        format!("{LOBBY_ADMIN_BASE}/admin/lobbies{suffix}"),
        "GET /admin/lobbies on the private Nexus lobby-admin service.",
        NEXUS_LOBBY_TOKEN_ENV,
    )
}

pub fn lobby_open_plan(lobby_id: &str) -> Result<ProtectedGetPlan, String> {
    let clean = lobby_id.trim();
    if clean.is_empty() {
        return Err("lobby id is required".to_string());
    }
    Ok(ProtectedGetPlan::new(
        format!("Nexus lobby-admin lobby detail: {clean}"),
        format!(
            "{LOBBY_ADMIN_BASE}/admin/lobbies/{}",
            percent_encode_segment(clean)
        ),
        "GET /admin/lobbies/{lobby_id} on the private Nexus lobby-admin service.",
        NEXUS_LOBBY_TOKEN_ENV,
    ))
}

pub fn identity_user_plan(action: &str, values: &[String]) -> Result<ProtectedGetPlan, String> {
    match action {
        "search" => {
            let query = required_arg(values, "usage: user search <query> [limit] [role] [state] [offset] [--execute]")?;
            let limit = values.get(1).map(String::as_str).unwrap_or("50");
            let role = values.get(2).map(String::as_str);
            let state = values.get(3).map(String::as_str);
            let offset = values.get(4).map(String::as_str);
            let mut params = Vec::new();
            push_query(&mut params, "limit", Some(limit));
            push_query(&mut params, "query", Some(query));
            push_query(&mut params, "role", role);
            push_query(&mut params, "state", state);
            push_query(&mut params, "offset", offset);
            Ok(ProtectedGetPlan::new(
                "Nexus Identity user search",
                format!("{IDENTITY_ADMIN_BASE}/admin/users?{}", params.join("&")),
                "GET /admin/users on the live Nexus Identity service. state accepts active/enabled/false or disabled/true.",
                NEXUS_IDENTITY_TOKEN_ENV,
            ))
        }
        "open" => {
            let username = required_arg(values, "usage: user open <username> [--execute]")?;
            Ok(ProtectedGetPlan::new(
                format!("Nexus Identity user detail: {username}"),
                format!(
                    "{IDENTITY_ADMIN_BASE}/admin/users/{}",
                    percent_encode_segment(username)
                ),
                "GET /admin/users/{username} on the live Nexus Identity service. This route records an admin audit event.",
                NEXUS_IDENTITY_TOKEN_ENV,
            ))
        }
        "sessions" => {
            let username = required_arg(values, "usage: user sessions <username> [--execute]")?;
            Ok(ProtectedGetPlan::new(
                format!("Nexus Identity session inventory: {username}"),
                format!(
                    "{IDENTITY_ADMIN_BASE}/admin/users/{}/sessions",
                    percent_encode_segment(username)
                ),
                "GET /admin/users/{username}/sessions on the live Nexus Identity service. This route records an admin audit event.",
                NEXUS_IDENTITY_TOKEN_ENV,
            ))
        }
        "devices" => {
            let username = required_arg(values, "usage: user devices <username> [--execute]")?;
            Ok(ProtectedGetPlan::new(
                format!("Nexus Identity device inventory: {username}"),
                format!(
                    "{IDENTITY_ADMIN_BASE}/admin/users/{}/devices",
                    percent_encode_segment(username)
                ),
                "GET /admin/users/{username}/devices on the live Nexus Identity service. This route records an admin audit event.",
                NEXUS_IDENTITY_TOKEN_ENV,
            ))
        }
        "audit" => {
            let username = required_arg(values, "usage: user audit <username> [limit] [event_type] [offset] [--execute]")?;
            let limit = values.get(1).map(String::as_str).unwrap_or("50");
            let event_type = values.get(2).map(String::as_str);
            let offset = values.get(3).map(String::as_str);
            let mut params = Vec::new();
            push_query(&mut params, "limit", Some(limit));
            push_query(&mut params, "event_type", event_type);
            push_query(&mut params, "offset", offset);
            let suffix = if params.is_empty() {
                String::new()
            } else {
                format!("?{}", params.join("&"))
            };
            Ok(ProtectedGetPlan::new(
                format!("Nexus Identity user audit: {username}"),
                format!(
                    "{IDENTITY_ADMIN_BASE}/admin/users/{}/audit{suffix}",
                    percent_encode_segment(username)
                ),
                "GET /admin/users/{username}/audit on the live Nexus Identity service. This route records an admin audit-view event.",
                NEXUS_IDENTITY_TOKEN_ENV,
            ))
        }
        _ => Err(format!("unsupported user command: {action}")),
    }
}

pub fn execute_protected_get(plan: &ProtectedGetPlan) -> io::Result<()> {
    let Some((token_source, token)) = read_token(plan)
    else {
        println!(
            "protected Nexus read-only execution blocked: missing {}{}",
            plan.token_env,
            plan.fallback_token_env
                .map(|env| format!(" or {env}"))
                .unwrap_or_default()
        );
        println!("dry-run command:");
        println!("{}", plan.render_dry_run());
        return Ok(());
    };
    if token.contains('\n') || token.contains('\r') {
        println!("protected Nexus read-only execution blocked: {NEXUS_TOKEN_ENV} contains a newline");
        return Ok(());
    }

    println!("executing protected Nexus read-only GET:");
    println!("{}", plan.title);
    println!("token source: {token_source} (value hidden)");

    let header = curl_config_escape(&format!("Authorization: Bearer {token}"));
    let script = format!(
        "set -eu\ncurl -fsS --connect-timeout 5 --max-time 20 --config - {} <<'__SEKAILINK_CURL__'\nheader = \"{}\"\nheader = \"User-Agent: SekaiLinkCoreAccess/0.1\"\nheader = \"X-SekaiLink-Client: core-access\"\nheader = \"X-SekaiLink-Client-Version: 0.1.0\"\nheader = \"X-SekaiLink-Device-Id: core-access-bastion\"\n__SEKAILINK_CURL__\nprintf '\\n'\n",
        shell_word(&plan.url),
        header,
    );

    let term = env::var("TERM")
        .ok()
        .filter(|value| value != "dumb")
        .unwrap_or_else(|| "xterm-256color".to_string());
    let mut child = Command::new("ssh")
        .arg(SSH_ALIAS)
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
    println!("protected Nexus command exit status: {status}");
    Ok(())
}

pub fn non_flag_args(parts: &[String], start: usize) -> Vec<String> {
    parts
        .iter()
        .skip(start)
        .filter(|part| !part.starts_with("--"))
        .cloned()
        .collect()
}

fn required_arg<'a>(values: &'a [String], usage: &str) -> Result<&'a str, String> {
    values
        .first()
        .map(String::as_str)
        .filter(|value| !value.trim().is_empty())
        .ok_or_else(|| usage.to_string())
}

fn read_token(plan: &ProtectedGetPlan) -> Option<(&'static str, String)> {
    for env_name in [Some(plan.token_env), plan.fallback_token_env]
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

fn non_empty(value: Option<&String>) -> Option<String> {
    value
        .map(String::as_str)
        .map(str::trim)
        .filter(|value| !value.is_empty() && *value != "-")
        .map(str::to_string)
}

fn push_query(params: &mut Vec<String>, key: &str, value: Option<&str>) {
    if let Some(value) = value {
        params.push(format!("{key}={}", percent_encode_query(value)));
    }
}

fn percent_encode_query(value: &str) -> String {
    percent_encode(value)
}

fn percent_encode_segment(value: &str) -> String {
    percent_encode(value)
}

fn percent_encode(value: &str) -> String {
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
    use super::{LobbyListFilter, identity_user_plan, lobby_list_plan, lobby_open_plan};

    #[test]
    fn lobby_list_builds_query_string() {
        let filter = LobbyListFilter {
            limit: Some("25".to_string()),
            query: Some("windows lobby".to_string()),
            visibility: Some("public".to_string()),
            status: Some("open".to_string()),
            offset: Some("10".to_string()),
        };
        let plan = lobby_list_plan(&filter);
        assert!(plan.url.contains("limit=25"));
        assert!(plan.url.contains("query=windows%20lobby"));
        assert!(plan.url.contains("visibility=public"));
        assert!(plan.url.contains("status=open"));
        assert!(plan.url.contains("offset=10"));
    }

    #[test]
    fn lobby_id_is_encoded_as_path_segment() {
        let plan = lobby_open_plan("name with/slash").unwrap();
        assert!(plan.url.ends_with("/name%20with%2Fslash"));
    }

    #[test]
    fn identity_plan_builds_live_user_detail_url() {
        let values = vec!["certo".to_string()];
        let plan = identity_user_plan("open", &values).unwrap();
        assert!(plan.url.ends_with("/admin/users/certo"));
        assert!(plan.render_dry_run().contains("NEXUS_IDENTITY_ADMIN_TOKEN"));
    }

    #[test]
    fn identity_search_builds_expected_filters() {
        let values = vec![
            "certo".to_string(),
            "25".to_string(),
            "admin".to_string(),
            "active".to_string(),
            "5".to_string(),
        ];
        let plan = identity_user_plan("search", &values).unwrap();
        assert!(plan.url.contains("/admin/users?"));
        assert!(plan.url.contains("query=certo"));
        assert!(plan.url.contains("limit=25"));
        assert!(plan.url.contains("role=admin"));
        assert!(plan.url.contains("state=active"));
        assert!(plan.url.contains("offset=5"));
        assert!(plan.render_dry_run().contains("'http://149.202.61.90:19095/admin/users?"));
    }
}

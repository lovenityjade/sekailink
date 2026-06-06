use std::env;
use std::io::{self, Write};
use std::process::{Command, Stdio};

pub const NEXUS_TOKEN_ENV: &str = "SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN";
pub const NEXUS_AGENT_TOKEN_ENV: &str = "SEKAILINK_CORE_ACCESS_NEXUS_AGENT_ADMIN_TOKEN";
pub const NEXUS_LOBBY_TOKEN_ENV: &str = "SEKAILINK_CORE_ACCESS_NEXUS_LOBBY_ADMIN_TOKEN";
pub const NEXUS_IDENTITY_TOKEN_ENV: &str = "SEKAILINK_CORE_ACCESS_NEXUS_IDENTITY_ADMIN_TOKEN";
pub const NEXUS_SEED_CONFIG_TOKEN_ENV: &str = "SEKAILINK_CORE_ACCESS_NEXUS_SEED_CONFIG_ADMIN_TOKEN";
pub const NEXUS_ROOM_QUERY_TOKEN_ENV: &str = "SEKAILINK_CORE_ACCESS_NEXUS_ROOM_QUERY_ADMIN_TOKEN";
pub const NEXUS_MUTATION_ENV: &str = "SEKAILINK_CORE_ACCESS_NEXUS_MUTATION";

const SSH_ALIAS: &str = "nexus-vps";
const ADMIN_AGENT_BASE: &str = "http://127.0.0.1:19091";
const LOBBY_ADMIN_BASE: &str = "http://127.0.0.1:19096";
const IDENTITY_ADMIN_BASE: &str = "http://149.202.61.90:19095";
const SEED_CONFIG_BASE: &str = "http://127.0.0.1:19106";
const ROOM_QUERY_BASE: &str = "http://127.0.0.1:19094";

#[derive(Debug, Clone)]
pub struct ProtectedGetPlan {
    pub title: String,
    pub url: String,
    pub detail: String,
    pub token_env: &'static str,
    pub fallback_token_env: Option<&'static str>,
    pub method: &'static str,
    pub body: Option<String>,
    pub redacted_body: Option<String>,
    pub mutation_confirm: Option<String>,
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
            method: "GET",
            body: None,
            redacted_body: None,
            mutation_confirm: None,
        }
    }

    fn request(
        title: impl Into<String>,
        method: &'static str,
        url: impl Into<String>,
        detail: impl Into<String>,
        token_env: &'static str,
        body: Option<String>,
        redacted_body: Option<String>,
        mutation_confirm: Option<String>,
    ) -> Self {
        Self {
            title: title.into(),
            url: url.into(),
            detail: detail.into(),
            token_env,
            fallback_token_env: (token_env != NEXUS_TOKEN_ENV).then_some(NEXUS_TOKEN_ENV),
            method,
            body,
            redacted_body,
            mutation_confirm,
        }
    }

    pub fn render_dry_run(&self) -> String {
        let fallback = self
            .fallback_token_env
            .map(|env| format!("; fallback ${env}"))
            .unwrap_or_default();
        let body = self
            .redacted_body
            .as_deref()
            .map(|body| format!("\nbody: {body}"))
            .unwrap_or_default();
        let confirm = self
            .mutation_confirm
            .as_deref()
            .map(|value| format!("\nconfirmation: --confirm {value}"))
            .unwrap_or_default();
        format!(
            "{}\n{}\ntransport: ssh {} -- sh -s\nrequest: {} {}\nheaders: Authorization: Bearer ${}{}; X-SekaiLink-Client: core-access{}{}",
            self.title,
            self.detail,
            SSH_ALIAS,
            self.method,
            shell_word(&self.url),
            self.token_env,
            fallback,
            body,
            confirm,
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

pub fn lobby_create_plan(values: &[String]) -> Result<ProtectedGetPlan, String> {
    let lobby_id = required_arg(
        values,
        "usage: lobby create <lobby_id> <name> [visibility] [owner_username] [description] --confirm lobby:<lobby_id>:create [--execute]",
    )?;
    let name = values.get(1).map(String::as_str).ok_or_else(|| {
        "usage: lobby create <lobby_id> <name> [visibility] [owner_username] [description] --confirm lobby:<lobby_id>:create [--execute]".to_string()
    })?;
    let lobby_id = clean_required_segment(lobby_id, "lobby_id")?;
    let mut fields = vec![json_field("lobby_id", &lobby_id), json_field("name", name)];
    if let Some(visibility) = values.get(2).map(String::as_str) {
        fields.push(json_field("visibility", visibility));
    }
    if let Some(owner) = values.get(3).map(String::as_str) {
        fields.push(json_field("owner_username", owner));
    }
    if values.len() > 4 {
        fields.push(json_field("description", &values[4..].join(" ")));
    }
    let body = format!("{{{}}}", fields.join(","));
    Ok(ProtectedGetPlan::request(
        format!("Nexus lobby-admin create lobby: {lobby_id}"),
        "POST",
        format!("{LOBBY_ADMIN_BASE}/admin/lobbies"),
        "POST /admin/lobbies on the private Nexus lobby-admin service. The service audits and bridges to Link runtime when configured.",
        NEXUS_LOBBY_TOKEN_ENV,
        Some(body.clone()),
        Some(body),
        Some(format!("lobby:{lobby_id}:create")),
    ))
}

pub fn lobby_edit_plan(lobby_id: &str, patches: &[String]) -> Result<ProtectedGetPlan, String> {
    let lobby_id = clean_required_segment(lobby_id, "lobby_id")?;
    let mut fields = Vec::new();
    for patch in patches {
        let Some((key, value)) = patch.split_once('=') else {
            return Err(format!(
                "invalid lobby edit patch: {patch}; expected key=value"
            ));
        };
        match key {
            "name" | "visibility" | "owner_username" | "description" | "status" => {
                fields.push(json_field(key, value.trim()));
            }
            other => {
                return Err(format!(
                    "unsupported lobby edit field: {other}; allowed name, visibility, owner_username, description, status"
                ));
            }
        }
    }
    if fields.is_empty() {
        return Err("lobby edit requires at least one key=value patch".to_string());
    }
    let body = format!("{{{}}}", fields.join(","));
    Ok(ProtectedGetPlan::request(
        format!("Nexus lobby-admin edit lobby: {lobby_id}"),
        "PATCH",
        format!(
            "{LOBBY_ADMIN_BASE}/admin/lobbies/{}",
            percent_encode_segment(&lobby_id)
        ),
        "PATCH /admin/lobbies/{lobby_id} on the private Nexus lobby-admin service. The service audits and bridges to Link runtime when configured.",
        NEXUS_LOBBY_TOKEN_ENV,
        Some(body.clone()),
        Some(body),
        Some(format!("lobby:{lobby_id}:edit")),
    ))
}

pub fn lobby_close_plan(lobby_id: &str) -> Result<ProtectedGetPlan, String> {
    let lobby_id = clean_required_segment(lobby_id, "lobby_id")?;
    Ok(ProtectedGetPlan::request(
        format!("Nexus lobby-admin close lobby: {lobby_id}"),
        "POST",
        format!(
            "{LOBBY_ADMIN_BASE}/admin/lobbies/{}/close",
            percent_encode_segment(&lobby_id)
        ),
        "POST /admin/lobbies/{lobby_id}/close on the private Nexus lobby-admin service. The service audits and bridges to Link runtime when configured.",
        NEXUS_LOBBY_TOKEN_ENV,
        None,
        None,
        Some(format!("lobby:{lobby_id}:close")),
    ))
}

#[derive(Debug, Clone, Default)]
pub struct RoomListFilter {
    pub limit: Option<String>,
    pub query: Option<String>,
    pub room_type: Option<String>,
    pub connection_state: Option<String>,
    pub offset: Option<String>,
}

impl RoomListFilter {
    pub fn from_positionals(values: &[String]) -> Self {
        Self {
            limit: non_empty(values.first()),
            query: non_empty(values.get(1)),
            room_type: non_empty(values.get(2)),
            connection_state: non_empty(values.get(3)),
            offset: non_empty(values.get(4)),
        }
    }
}

pub fn room_list_plan(filter: &RoomListFilter) -> ProtectedGetPlan {
    let mut params = Vec::new();
    push_query(&mut params, "limit", filter.limit.as_deref());
    push_query(&mut params, "query", filter.query.as_deref());
    push_query(&mut params, "room_type", filter.room_type.as_deref());
    push_query(
        &mut params,
        "connection_state",
        filter.connection_state.as_deref(),
    );
    push_query(&mut params, "offset", filter.offset.as_deref());
    let suffix = if params.is_empty() {
        String::new()
    } else {
        format!("?{}", params.join("&"))
    };
    ProtectedGetPlan::new(
        "Nexus room-query room inventory",
        format!("{ROOM_QUERY_BASE}/rooms{suffix}"),
        "GET /rooms on the private Nexus room-query service.",
        NEXUS_ROOM_QUERY_TOKEN_ENV,
    )
}

pub fn room_detail_plan(room_id: &str) -> Result<ProtectedGetPlan, String> {
    let room_id = clean_required_segment(room_id, "room_id")?;
    Ok(ProtectedGetPlan::new(
        format!("Nexus room-query room snapshot: {room_id}"),
        format!(
            "{ROOM_QUERY_BASE}/rooms/{}",
            percent_encode_segment(&room_id)
        ),
        "GET /rooms/{room_id} on the private Nexus room-query service.",
        NEXUS_ROOM_QUERY_TOKEN_ENV,
    ))
}

pub fn room_diagnostics_plan(room_id: &str) -> Result<ProtectedGetPlan, String> {
    let room_id = clean_required_segment(room_id, "room_id")?;
    Ok(ProtectedGetPlan::new(
        format!("Nexus room-query room diagnostics: {room_id}"),
        format!(
            "{ROOM_QUERY_BASE}/rooms/{}/diagnostics",
            percent_encode_segment(&room_id)
        ),
        "GET /rooms/{room_id}/diagnostics on the private Nexus room-query service.",
        NEXUS_ROOM_QUERY_TOKEN_ENV,
    ))
}

pub fn room_events_plan(room_id: &str, values: &[String]) -> Result<ProtectedGetPlan, String> {
    let room_id = clean_required_segment(room_id, "room_id")?;
    let mut params = Vec::new();
    push_query(&mut params, "limit", values.first().map(String::as_str));
    push_query(&mut params, "event_type", values.get(1).map(String::as_str));
    push_query(&mut params, "severity", values.get(2).map(String::as_str));
    push_query(&mut params, "offset", values.get(3).map(String::as_str));
    push_query(&mut params, "source", values.get(4).map(String::as_str));
    let suffix = if params.is_empty() {
        String::new()
    } else {
        format!("?{}", params.join("&"))
    };
    Ok(ProtectedGetPlan::new(
        format!("Nexus room-query room events: {room_id}"),
        format!(
            "{ROOM_QUERY_BASE}/rooms/{}/events{suffix}",
            percent_encode_segment(&room_id)
        ),
        "GET /rooms/{room_id}/events on the private Nexus room-query service.",
        NEXUS_ROOM_QUERY_TOKEN_ENV,
    ))
}

pub fn room_client_reports_plan(
    room_id: &str,
    values: &[String],
) -> Result<ProtectedGetPlan, String> {
    let room_id = clean_required_segment(room_id, "room_id")?;
    let mut params = Vec::new();
    push_query(&mut params, "limit", values.first().map(String::as_str));
    push_query(
        &mut params,
        "report_type",
        values.get(1).map(String::as_str),
    );
    push_query(&mut params, "severity", values.get(2).map(String::as_str));
    push_query(&mut params, "source", values.get(3).map(String::as_str));
    push_query(&mut params, "offset", values.get(4).map(String::as_str));
    let suffix = if params.is_empty() {
        String::new()
    } else {
        format!("?{}", params.join("&"))
    };
    Ok(ProtectedGetPlan::new(
        format!("Nexus room-query client reports: {room_id}"),
        format!(
            "{ROOM_QUERY_BASE}/rooms/{}/client-reports{suffix}",
            percent_encode_segment(&room_id)
        ),
        "GET /rooms/{room_id}/client-reports on the private Nexus room-query service.",
        NEXUS_ROOM_QUERY_TOKEN_ENV,
    ))
}

pub fn identity_user_plan(action: &str, values: &[String]) -> Result<ProtectedGetPlan, String> {
    match action {
        "search" => {
            let query = required_arg(
                values,
                "usage: user search <query> [limit] [role] [state] [offset] [--execute]",
            )?;
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
            let username = required_arg(
                values,
                "usage: user audit <username> [limit] [event_type] [offset] [--execute]",
            )?;
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

pub fn user_configs_plan(
    user_id: &str,
    game_key: Option<&str>,
) -> Result<ProtectedGetPlan, String> {
    let user_id = clean_required_segment(user_id, "user_id")?;
    let mut params = Vec::new();
    push_query(&mut params, "game_key", game_key);
    let suffix = if params.is_empty() {
        String::new()
    } else {
        format!("?{}", params.join("&"))
    };
    Ok(ProtectedGetPlan::new(
        format!("Nexus seed-config user configs: {user_id}"),
        format!(
            "{SEED_CONFIG_BASE}/users/{}/seed-configs{suffix}",
            percent_encode_segment(&user_id)
        ),
        "GET /users/{user_id}/seed-configs on the private Nexus seed-config API. The route expects a numeric Nexus user_id; resolve usernames with user open first.",
        NEXUS_SEED_CONFIG_TOKEN_ENV,
    ))
}

pub fn user_config_open_plan(user_id: &str, config_id: &str) -> Result<ProtectedGetPlan, String> {
    let user_id = clean_required_segment(user_id, "user_id")?;
    let config_id = clean_required_segment(config_id, "config_id")?;
    let mut params = Vec::new();
    push_query(&mut params, "config_id", Some(&config_id));
    Ok(ProtectedGetPlan::new(
        format!("Nexus seed-config user config: {user_id}/{config_id}"),
        format!(
            "{SEED_CONFIG_BASE}/users/{}/seed-configs?{}",
            percent_encode_segment(&user_id),
            params.join("&")
        ),
        "GET /users/{user_id}/seed-configs on the private Nexus seed-config API. The current API returns a list; Core Access includes config_id as an operator filter hint.",
        NEXUS_SEED_CONFIG_TOKEN_ENV,
    ))
}

pub fn user_config_export_plan(user_id: &str, config_id: &str) -> Result<ProtectedGetPlan, String> {
    let user_id = clean_required_segment(user_id, "user_id")?;
    let config_id = clean_required_segment(config_id, "config_id")?;
    Ok(ProtectedGetPlan::request(
        format!("Nexus seed-config YAML export: {user_id}/{config_id}"),
        "POST",
        format!(
            "{SEED_CONFIG_BASE}/users/{}/seed-configs/{}/export-yaml",
            percent_encode_segment(&user_id),
            percent_encode_segment(&config_id)
        ),
        "POST /users/{user_id}/seed-configs/{config_id}/export-yaml exports canonical config values as YAML without editing the source config.",
        NEXUS_SEED_CONFIG_TOKEN_ENV,
        None,
        None,
        None,
    ))
}

pub fn identity_user_create_plan(
    username: &str,
    email: &str,
    role: &str,
    display_name: Option<&str>,
    locale: Option<&str>,
    permissions: Option<&str>,
    email_verified: bool,
    password_env: &str,
) -> Result<ProtectedGetPlan, String> {
    let username = clean_identity(username, "username")?;
    let email = clean_email(email)?;
    let role = clean_role(role)?;
    let password = env::var(password_env)
        .map_err(|_| format!("missing password env var: {password_env}"))?
        .trim()
        .to_string();
    if password.len() < 8 {
        return Err("password env value must be at least 8 characters".to_string());
    }

    let mut fields = vec![
        json_field("username", &username),
        json_field("email", &email),
        json_field("password", &password),
        json_field("role", &role),
        format!("\"email_verified\":{email_verified}"),
    ];
    if let Some(display_name) = display_name.filter(|value| !value.trim().is_empty()) {
        fields.push(json_field("display_name", display_name.trim()));
    }
    if let Some(locale) = locale.filter(|value| !value.trim().is_empty()) {
        fields.push(json_field("locale", locale.trim()));
    }
    if let Some(permissions) = permissions.filter(|value| !value.trim().is_empty()) {
        fields.push(format!(
            "\"permissions\":{}",
            json_string_array(permissions)
        ));
    }

    let redacted_fields = fields
        .iter()
        .map(|field| {
            if field.starts_with("\"password\":") {
                "\"password\":\"<redacted>\"".to_string()
            } else {
                field.clone()
            }
        })
        .collect::<Vec<_>>();
    Ok(ProtectedGetPlan::request(
        format!("Nexus Identity create user: {username}"),
        "POST",
        format!("{IDENTITY_ADMIN_BASE}/admin/users"),
        "POST /admin/users on the live Nexus Identity service. This route records an admin audit event.",
        NEXUS_IDENTITY_TOKEN_ENV,
        Some(format!("{{{}}}", fields.join(","))),
        Some(format!("{{{}}}", redacted_fields.join(","))),
        Some(format!("user:{username}:create")),
    ))
}

pub fn identity_user_edit_plan(
    username: &str,
    patches: &[String],
) -> Result<ProtectedGetPlan, String> {
    let username = clean_identity(username, "username")?;
    let mut fields = Vec::new();
    for patch in patches {
        let Some((key, value)) = patch.split_once('=') else {
            return Err(format!("invalid patch: {patch}; expected key=value"));
        };
        match key {
            "email" => fields.push(json_field("email", &clean_email(value)?)),
            "display_name" | "avatar_url" | "bio" | "locale" | "role" => {
                fields.push(json_field(key, value.trim()));
            }
            "email_verified" | "disabled" => {
                fields.push(format!("\"{key}\":{}", parse_bool(value)?));
            }
            "permissions" => fields.push(format!("\"permissions\":{}", json_string_array(value))),
            other => return Err(format!("unsupported user edit field: {other}")),
        }
    }
    if fields.is_empty() {
        return Err("user edit requires at least one key=value patch".to_string());
    }
    Ok(ProtectedGetPlan::request(
        format!("Nexus Identity edit user: {username}"),
        "PATCH",
        format!(
            "{IDENTITY_ADMIN_BASE}/admin/users/{}",
            percent_encode_segment(&username)
        ),
        "PATCH /admin/users/{username} on the live Nexus Identity service. This route records an admin audit event.",
        NEXUS_IDENTITY_TOKEN_ENV,
        Some(format!("{{{}}}", fields.join(","))),
        Some(format!("{{{}}}", fields.join(","))),
        Some(format!("user:{username}:edit")),
    ))
}

pub fn identity_user_disable_plan(username: &str) -> Result<ProtectedGetPlan, String> {
    let username = clean_identity(username, "username")?;
    Ok(ProtectedGetPlan::request(
        format!("Nexus Identity disable user: {username}"),
        "DELETE",
        format!(
            "{IDENTITY_ADMIN_BASE}/admin/users/{}",
            percent_encode_segment(&username)
        ),
        "DELETE /admin/users/{username} soft-disables the user and revokes sessions.",
        NEXUS_IDENTITY_TOKEN_ENV,
        None,
        None,
        Some(format!("user:{username}:disable")),
    ))
}

pub fn identity_user_revoke_sessions_plan(username: &str) -> Result<ProtectedGetPlan, String> {
    let username = clean_identity(username, "username")?;
    Ok(ProtectedGetPlan::request(
        format!("Nexus Identity revoke sessions: {username}"),
        "POST",
        format!(
            "{IDENTITY_ADMIN_BASE}/admin/users/{}/sessions/revoke-others",
            percent_encode_segment(&username)
        ),
        "POST /admin/users/{username}/sessions/revoke-others revokes all sessions for the target user.",
        NEXUS_IDENTITY_TOKEN_ENV,
        None,
        None,
        Some(format!("user:{username}:revoke-sessions")),
    ))
}

pub fn identity_user_force_password_reset_plan(username: &str) -> Result<ProtectedGetPlan, String> {
    let username = clean_identity(username, "username")?;
    Ok(ProtectedGetPlan::request(
        format!("Nexus Identity force password reset: {username}"),
        "POST",
        format!(
            "{IDENTITY_ADMIN_BASE}/admin/users/{}/password-reset",
            percent_encode_segment(&username)
        ),
        "POST /admin/users/{username}/password-reset sends a reset email and records admin audit.",
        NEXUS_IDENTITY_TOKEN_ENV,
        None,
        None,
        Some(format!("user:{username}:force-password-reset")),
    ))
}

pub fn execute_protected_get(plan: &ProtectedGetPlan) -> io::Result<()> {
    let Some((token_source, token)) = read_token(plan) else {
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
        println!(
            "protected Nexus read-only execution blocked: {NEXUS_TOKEN_ENV} contains a newline"
        );
        return Ok(());
    }

    println!("executing protected Nexus request:");
    println!("{}", plan.title);
    println!("token source: {token_source} (value hidden)");

    let header = curl_config_escape(&format!("Authorization: Bearer {token}"));
    let body_config = plan
        .body
        .as_deref()
        .map(|body| {
            format!(
                "header = \"Content-Type: application/json\"\ndata = \"{}\"\n",
                curl_config_escape(body)
            )
        })
        .unwrap_or_default();
    let script = format!(
        "set -eu\ncurl -fsS --connect-timeout 5 --max-time 20 --request {} --config - {} <<'__SEKAILINK_CURL__'\nheader = \"{}\"\nheader = \"User-Agent: SekaiLinkCoreAccess/0.1\"\nheader = \"X-SekaiLink-Client: core-access\"\nheader = \"X-SekaiLink-Client-Version: 0.1.0\"\nheader = \"X-SekaiLink-Device-Id: core-access-bastion\"\n{}__SEKAILINK_CURL__\nprintf '\\n'\n",
        plan.method,
        shell_word(&plan.url),
        header,
        body_config,
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

fn clean_identity(value: &str, label: &str) -> Result<String, String> {
    let clean = value.trim();
    if clean.is_empty() {
        return Err(format!("{label} is required"));
    }
    if !clean
        .chars()
        .all(|ch| ch.is_ascii_alphanumeric() || matches!(ch, '-' | '_' | '.' | '@'))
    {
        return Err(format!(
            "{label} may only use letters, numbers, dash, underscore, dot, or @"
        ));
    }
    Ok(clean.to_string())
}

fn clean_required_segment(value: &str, label: &str) -> Result<String, String> {
    let clean = value.trim();
    if clean.is_empty() {
        return Err(format!("{label} is required"));
    }
    Ok(clean.to_string())
}

fn clean_email(value: &str) -> Result<String, String> {
    let clean = clean_identity(value, "email")?;
    if !clean.contains('@') {
        return Err("email must contain @".to_string());
    }
    Ok(clean.to_ascii_lowercase())
}

fn clean_role(value: &str) -> Result<String, String> {
    let role = value.trim().to_ascii_lowercase();
    match role.as_str() {
        "player" | "service" | "admin" | "moderator" => Ok(role),
        _ => Err("role must be player, service, moderator, or admin".to_string()),
    }
}

fn parse_bool(value: &str) -> Result<bool, String> {
    match value.trim().to_ascii_lowercase().as_str() {
        "1" | "true" | "yes" | "on" => Ok(true),
        "0" | "false" | "no" | "off" => Ok(false),
        _ => Err(format!("invalid boolean: {value}")),
    }
}

fn json_field(key: &str, value: &str) -> String {
    format!("\"{}\":\"{}\"", json_escape(key), json_escape(value))
}

fn json_string_array(value: &str) -> String {
    let items = value
        .split(',')
        .map(str::trim)
        .filter(|item| !item.is_empty())
        .map(|item| format!("\"{}\"", json_escape(item)))
        .collect::<Vec<_>>();
    format!("[{}]", items.join(","))
}

fn json_escape(value: &str) -> String {
    let mut out = String::new();
    for ch in value.chars() {
        match ch {
            '\\' => out.push_str("\\\\"),
            '"' => out.push_str("\\\""),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            ch if ch.is_control() => out.push_str(&format!("\\u{:04x}", ch as u32)),
            ch => out.push(ch),
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
        LobbyListFilter, identity_user_disable_plan, identity_user_edit_plan,
        identity_user_force_password_reset_plan, identity_user_plan,
        identity_user_revoke_sessions_plan, lobby_list_plan, lobby_open_plan,
        user_config_export_plan, user_config_open_plan, user_configs_plan,
    };

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
        assert!(
            plan.render_dry_run()
                .contains("'http://149.202.61.90:19095/admin/users?")
        );
    }

    #[test]
    fn identity_edit_plan_builds_patch_body_and_confirmation() {
        let patches = vec![
            "display_name=Certo Admin".to_string(),
            "disabled=false".to_string(),
            "permissions=release:write,rooms:control".to_string(),
        ];
        let plan = identity_user_edit_plan("certo", &patches).unwrap();
        assert_eq!(plan.method, "PATCH");
        assert!(plan.url.ends_with("/admin/users/certo"));
        assert_eq!(plan.mutation_confirm.as_deref(), Some("user:certo:edit"));
        let body = plan.body.as_deref().unwrap_or_default();
        assert!(body.contains("\"display_name\":\"Certo Admin\""));
        assert!(body.contains("\"disabled\":false"));
        assert!(body.contains("\"permissions\":[\"release:write\",\"rooms:control\"]"));
        assert!(
            plan.render_dry_run()
                .contains("confirmation: --confirm user:certo:edit")
        );
    }

    #[test]
    fn identity_disable_plan_uses_delete_and_confirmation() {
        let plan = identity_user_disable_plan("JoueurSansFromage").unwrap();
        assert_eq!(plan.method, "DELETE");
        assert!(plan.url.ends_with("/admin/users/JoueurSansFromage"));
        assert_eq!(
            plan.mutation_confirm.as_deref(),
            Some("user:JoueurSansFromage:disable")
        );
    }

    #[test]
    fn identity_session_and_password_mutations_are_post_requests() {
        let revoke = identity_user_revoke_sessions_plan("certo").unwrap();
        assert_eq!(revoke.method, "POST");
        assert!(
            revoke
                .url
                .ends_with("/admin/users/certo/sessions/revoke-others")
        );
        assert_eq!(
            revoke.mutation_confirm.as_deref(),
            Some("user:certo:revoke-sessions")
        );

        let reset = identity_user_force_password_reset_plan("certo").unwrap();
        assert_eq!(reset.method, "POST");
        assert!(reset.url.ends_with("/admin/users/certo/password-reset"));
        assert_eq!(
            reset.mutation_confirm.as_deref(),
            Some("user:certo:force-password-reset")
        );
    }

    #[test]
    fn user_configs_plan_uses_seed_config_api() {
        let plan = user_configs_plan("42", Some("a link to the past")).unwrap();
        assert!(
            plan.url
                .starts_with("http://127.0.0.1:19106/users/42/seed-configs")
        );
        assert!(plan.url.contains("game_key=a%20link%20to%20the%20past"));
        assert!(
            plan.render_dry_run()
                .contains("NEXUS_SEED_CONFIG_ADMIN_TOKEN")
        );
    }

    #[test]
    fn user_config_open_uses_list_filter_hint() {
        let plan = user_config_open_plan("42", "config/7").unwrap();
        assert!(plan.url.contains("/users/42/seed-configs?"));
        assert!(plan.url.contains("config_id=config%2F7"));
    }

    #[test]
    fn user_config_export_uses_yaml_post_without_mutation_confirm() {
        let plan = user_config_export_plan("42", "7").unwrap();
        assert_eq!(plan.method, "POST");
        assert!(plan.url.ends_with("/users/42/seed-configs/7/export-yaml"));
        assert!(plan.mutation_confirm.is_none());
    }
}

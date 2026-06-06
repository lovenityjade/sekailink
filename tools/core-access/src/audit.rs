use crate::rbac::Role;
use crate::util::{epoch_nanos, epoch_seconds, json_escape};
use std::fs::{self, OpenOptions};
use std::io::{self, Write};
use std::path::{Path, PathBuf};

#[derive(Debug, Clone)]
pub struct Session {
    pub session_id: String,
    pub linux_user: String,
    pub sekailink_user: String,
    pub role: Role,
    pub data_dir: PathBuf,
}

impl Session {
    pub fn new(linux_user: String, sekailink_user: String, role: Role, data_dir: PathBuf) -> Self {
        let session_id = format!("{}-{}", epoch_nanos(), std::process::id());
        Self {
            session_id,
            linux_user,
            sekailink_user,
            role,
            data_dir,
        }
    }

    pub fn ensure_dirs(&self) -> io::Result<()> {
        fs::create_dir_all(self.audit_dir())?;
        fs::create_dir_all(self.data_dir.join("notes"))?;
        fs::create_dir_all(self.data_dir.join("approvals"))?;
        fs::create_dir_all(self.data_dir.join("history"))?;
        fs::create_dir_all(self.exports_dir())?;
        fs::create_dir_all(self.client_banners_dir())?;
        fs::create_dir_all(self.maintenance_dir())?;
        fs::create_dir_all(self.scheduler_dir())?;
        fs::create_dir_all(self.pack_repos_dir())?;
        Ok(())
    }

    pub fn audit_dir(&self) -> PathBuf {
        self.data_dir.join("audit")
    }

    pub fn notes_path(&self) -> PathBuf {
        self.data_dir.join("notes").join("notes.jsonl")
    }

    pub fn approvals_path(&self) -> PathBuf {
        self.data_dir.join("approvals").join("queue.jsonl")
    }

    pub fn history_path(&self) -> PathBuf {
        self.data_dir.join("history").join("commands.txt")
    }

    pub fn exports_dir(&self) -> PathBuf {
        self.data_dir.join("exports")
    }

    pub fn client_banners_dir(&self) -> PathBuf {
        self.data_dir.join("client-banners")
    }

    pub fn maintenance_dir(&self) -> PathBuf {
        self.data_dir.join("maintenance")
    }

    pub fn scheduler_dir(&self) -> PathBuf {
        self.data_dir.join("scheduler")
    }

    pub fn pack_repos_dir(&self) -> PathBuf {
        self.data_dir.join("pack-repos")
    }
}

pub fn append_audit(session: &Session, command: &str, status: &str, detail: &str) -> io::Result<()> {
    append_jsonl(
        &session.audit_dir().join("core-access.jsonl"),
        &format!(
            "{{\"ts\":{},\"session_id\":\"{}\",\"linux_user\":\"{}\",\"sekailink_user\":\"{}\",\"role\":\"{}\",\"command\":\"{}\",\"status\":\"{}\",\"detail\":\"{}\"}}\n",
            epoch_seconds(),
            json_escape(&session.session_id),
            json_escape(&session.linux_user),
            json_escape(&session.sekailink_user),
            session.role.as_str(),
            json_escape(command),
            json_escape(status),
            json_escape(detail)
        ),
    )
}

pub fn append_note(session: &Session, target: &str, text: &str) -> io::Result<String> {
    let id = format!("note-{}-{}", epoch_nanos(), std::process::id());
    append_jsonl(
        &session.notes_path(),
        &format!(
            "{{\"id\":\"{}\",\"ts\":{},\"session_id\":\"{}\",\"author\":\"{}\",\"target\":\"{}\",\"text\":\"{}\"}}\n",
            json_escape(&id),
            epoch_seconds(),
            json_escape(&session.session_id),
            json_escape(&session.sekailink_user),
            json_escape(target),
            json_escape(text)
        ),
    )?;
    Ok(id)
}

pub fn append_approval_request(session: &Session, requested_command: &str, reason: &str) -> io::Result<String> {
    let id = format!("approval-{}-{}", epoch_nanos(), std::process::id());
    append_jsonl(
        &session.approvals_path(),
        &format!(
            "{{\"id\":\"{}\",\"ts\":{},\"state\":\"pending\",\"requester\":\"{}\",\"role\":\"{}\",\"command\":\"{}\",\"reason\":\"{}\"}}\n",
            json_escape(&id),
            epoch_seconds(),
            json_escape(&session.sekailink_user),
            session.role.as_str(),
            json_escape(requested_command),
            json_escape(reason)
        ),
    )?;
    Ok(id)
}

pub fn append_approval_decision(session: &Session, id: &str, state: &str, reason: &str) -> io::Result<()> {
    append_jsonl(
        &session.approvals_path(),
        &format!(
            "{{\"id\":\"{}\",\"ts\":{},\"state\":\"{}\",\"reviewer\":\"{}\",\"role\":\"{}\",\"reason\":\"{}\"}}\n",
            json_escape(id),
            epoch_seconds(),
            json_escape(state),
            json_escape(&session.sekailink_user),
            session.role.as_str(),
            json_escape(reason)
        ),
    )
}

pub fn append_history(session: &Session, line: &str) -> io::Result<()> {
    append_jsonl(&session.history_path(), &format!("{line}\n"))
}

pub fn write_client_banner_draft(session: &Session, slot: u8, text: &str) -> io::Result<String> {
    let id = format!("banner-{}-{}", epoch_nanos(), std::process::id());
    fs::write(session.client_banners_dir().join(format!("slot-{slot}.txt")), text)?;
    append_jsonl(
        &session.client_banners_dir().join("history.jsonl"),
        &format!(
            "{{\"id\":\"{}\",\"ts\":{},\"slot\":{},\"author\":\"{}\",\"text\":\"{}\"}}\n",
            json_escape(&id),
            epoch_seconds(),
            slot,
            json_escape(&session.sekailink_user),
            json_escape(text)
        ),
    )?;
    Ok(id)
}

pub fn write_maintenance_draft(
    session: &Session,
    scope: &str,
    start: &str,
    end: &str,
    message: &str,
) -> io::Result<String> {
    let id = format!("maintenance-{}-{}", epoch_nanos(), std::process::id());
    let summary = format!(
        "id={id}\nscope={scope}\nstart={start}\nend={end}\nmessage={message}\n"
    );
    fs::write(session.maintenance_dir().join("current.txt"), &summary)?;
    append_jsonl(
        &session.maintenance_dir().join("history.jsonl"),
        &format!(
            "{{\"id\":\"{}\",\"ts\":{},\"state\":\"scheduled-draft\",\"author\":\"{}\",\"scope\":\"{}\",\"start\":\"{}\",\"end\":\"{}\",\"message\":\"{}\"}}\n",
            json_escape(&id),
            epoch_seconds(),
            json_escape(&session.sekailink_user),
            json_escape(scope),
            json_escape(start),
            json_escape(end),
            json_escape(message)
        ),
    )?;
    Ok(id)
}

pub fn write_schedule_job(session: &Session, name: &str, when: &str, command: &str) -> io::Result<String> {
    let id = format!("schedule-{}-{}", epoch_nanos(), std::process::id());
    append_jsonl(
        &session.scheduler_dir().join("jobs.jsonl"),
        &format!(
            "{{\"id\":\"{}\",\"ts\":{},\"state\":\"draft\",\"author\":\"{}\",\"name\":\"{}\",\"when\":\"{}\",\"command\":\"{}\"}}\n",
            json_escape(&id),
            epoch_seconds(),
            json_escape(&session.sekailink_user),
            json_escape(name),
            json_escape(when),
            json_escape(command)
        ),
    )?;
    Ok(id)
}

pub fn write_pack_repo(
    session: &Session,
    id: &str,
    url: &str,
    game: &str,
    notes: &str,
) -> io::Result<String> {
    let record_id = format!("pack-repo-{}-{}", epoch_nanos(), std::process::id());
    append_jsonl(
        &session.pack_repos_dir().join("repos.jsonl"),
        &format!(
            "{{\"record_id\":\"{}\",\"ts\":{},\"state\":\"draft\",\"author\":\"{}\",\"id\":\"{}\",\"game\":\"{}\",\"url\":\"{}\",\"notes\":\"{}\"}}\n",
            json_escape(&record_id),
            epoch_seconds(),
            json_escape(&session.sekailink_user),
            json_escape(id),
            json_escape(game),
            json_escape(url),
            json_escape(notes)
        ),
    )?;
    Ok(record_id)
}

pub fn write_export(session: &Session, prefix: &str, requested_name: Option<&str>, body: &str) -> io::Result<PathBuf> {
    let file_name = requested_name
        .filter(|name| !name.trim().is_empty())
        .map(sanitize_export_file_name)
        .unwrap_or_else(|| format!("{prefix}-{}.jsonl", epoch_nanos()));
    let path = session.exports_dir().join(file_name);
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(&path, body)?;
    Ok(path)
}

pub fn read_file_to_string(path: &Path) -> io::Result<String> {
    match fs::read_to_string(path) {
        Ok(value) => Ok(value),
        Err(err) if err.kind() == io::ErrorKind::NotFound => Ok(String::new()),
        Err(err) => Err(err),
    }
}

fn append_jsonl(path: &Path, line: &str) -> io::Result<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let mut file = OpenOptions::new().create(true).append(true).open(path)?;
    file.write_all(line.as_bytes())
}

fn sanitize_export_file_name(name: &str) -> String {
    let mut out = String::new();
    for ch in name.trim().chars() {
        if ch.is_ascii_alphanumeric() || matches!(ch, '.' | '-' | '_') {
            out.push(ch);
        } else {
            out.push('_');
        }
    }
    if out.is_empty() {
        "export.jsonl".to_string()
    } else if out.ends_with(".jsonl") || out.ends_with(".txt") || out.ends_with(".md") {
        out
    } else {
        format!("{out}.jsonl")
    }
}

#[cfg(test)]
mod tests {
    use super::sanitize_export_file_name;

    #[test]
    fn export_file_name_is_bounded() {
        assert_eq!(
            sanitize_export_file_name("../../incident room.json"),
            ".._.._incident_room.json.jsonl"
        );
    }

    #[test]
    fn export_file_name_keeps_known_extension() {
        assert_eq!(sanitize_export_file_name("room-logs.txt"), "room-logs.txt");
    }
}

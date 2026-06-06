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

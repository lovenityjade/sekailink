use crate::audit::{Session, append_history, read_file_to_string};
use crate::commands::{COMMANDS, command_names};
use crate::system::{known_services, local_health, log_catalog};
use std::env;
use std::io::{self, IsTerminal, Read, Write};
use std::process::{Command, Stdio};
use std::thread;
use std::time::{Duration, Instant};

const RESET: &str = "\x1b[0m";
const BOLD: &str = "\x1b[1m";
const RED: &str = "\x1b[31m";
const GREEN: &str = "\x1b[32m";
const YELLOW: &str = "\x1b[33m";
const CYAN: &str = "\x1b[36m";
const MAGENTA: &str = "\x1b[35m";
const BLUE_BG: &str = "\x1b[44m";

const MAX_OUTPUT_LINES: usize = 1_200;
const COMMAND_TIMEOUT: Duration = Duration::from_secs(45);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TuiExit {
    Quit,
    Shell,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum View {
    Output,
    Help,
    Commands,
    Logs,
    Panic,
}

pub fn run(session: Session) -> io::Result<TuiExit> {
    if !io::stdin().is_terminal() || !io::stdout().is_terminal() {
        return Ok(TuiExit::Shell);
    }

    let _terminal = TerminalGuard::enter()?;
    let mut state = TuiState::new(session)?;
    state.push_output("SekaiLink Core Access cockpit ready.");
    state.push_output("F1 help | F2 status | F6 logs | F9 shell | Ctrl+Q quit");

    loop {
        draw(&state)?;
        let Some(key) = read_key()? else {
            continue;
        };
        match state.handle_key(key)? {
            TuiAction::Continue => {}
            TuiAction::Quit => return Ok(TuiExit::Quit),
            TuiAction::Shell => return Ok(TuiExit::Shell),
        }
    }
}

struct TuiState {
    session: Session,
    input: Vec<char>,
    cursor: usize,
    history: Vec<String>,
    history_cursor: usize,
    completions: Vec<String>,
    suggestions: Vec<String>,
    output: Vec<String>,
    view: View,
    last_status_refresh: Instant,
}

impl TuiState {
    fn new(session: Session) -> io::Result<Self> {
        let history_text = read_file_to_string(&session.history_path())?;
        let history = history_text
            .lines()
            .filter(|line| !line.trim().is_empty())
            .map(str::to_string)
            .collect::<Vec<_>>();
        let history_cursor = history.len();
        Ok(Self {
            session,
            input: Vec::new(),
            cursor: 0,
            history,
            history_cursor,
            completions: command_names().into_iter().map(str::to_string).collect(),
            suggestions: Vec::new(),
            output: Vec::new(),
            view: View::Output,
            last_status_refresh: Instant::now(),
        })
    }

    fn handle_key(&mut self, key: Key) -> io::Result<TuiAction> {
        self.suggestions.clear();
        match key {
            Key::Char(ch) => {
                self.input.insert(self.cursor, ch);
                self.cursor += 1;
            }
            Key::Enter => {
                let command = self.current_input();
                self.input.clear();
                self.cursor = 0;
                self.history_cursor = self.history.len();
                if command.trim().is_empty() {
                    return Ok(TuiAction::Continue);
                }
                if matches!(command.trim(), "exit" | "quit") {
                    return Ok(TuiAction::Quit);
                }
                if command.trim() == "shell" {
                    return Ok(TuiAction::Shell);
                }
                self.run_command(&command)?;
            }
            Key::Backspace => {
                if self.cursor > 0 {
                    self.input.remove(self.cursor - 1);
                    self.cursor -= 1;
                }
            }
            Key::Delete => {
                if self.cursor < self.input.len() {
                    self.input.remove(self.cursor);
                }
            }
            Key::Left => {
                if self.cursor > 0 {
                    self.cursor -= 1;
                }
            }
            Key::Right => {
                if self.cursor < self.input.len() {
                    self.cursor += 1;
                }
            }
            Key::Home | Key::Ctrl('a') => self.cursor = 0,
            Key::End | Key::Ctrl('e') => self.cursor = self.input.len(),
            Key::Ctrl('w') => self.kill_word_back(),
            Key::Ctrl('c') | Key::Ctrl('q') => return Ok(TuiAction::Quit),
            Key::Up => self.history_up(),
            Key::Down => self.history_down(),
            Key::Tab => self.complete(),
            Key::Esc => {
                self.input.clear();
                self.cursor = 0;
            }
            Key::F(1) => self.view = View::Help,
            Key::F(2) => self.run_command("server status all")?,
            Key::F(3) => self.view = View::Commands,
            Key::F(4) => self.replace_input("note add incident "),
            Key::F(5) => {
                self.last_status_refresh = Instant::now();
                self.push_output("dashboard refreshed");
            }
            Key::F(6) => self.view = View::Logs,
            Key::F(7) => self.replace_input("audit search "),
            Key::F(8) => self.replace_input("server logs "),
            Key::F(9) => return Ok(TuiAction::Shell),
            Key::F(10) => self.output.clear(),
            Key::F(11) => self.cycle_view(),
            Key::F(12) => self.view = View::Panic,
            Key::F(_) | Key::Ctrl(_) => {}
        }
        Ok(TuiAction::Continue)
    }

    fn current_input(&self) -> String {
        self.input.iter().collect::<String>()
    }

    fn replace_input(&mut self, value: &str) {
        self.input = value.chars().collect();
        self.cursor = self.input.len();
    }

    fn push_output(&mut self, line: impl Into<String>) {
        self.output.push(line.into());
        if self.output.len() > MAX_OUTPUT_LINES {
            let overflow = self.output.len() - MAX_OUTPUT_LINES;
            self.output.drain(0..overflow);
        }
    }

    fn run_command(&mut self, command: &str) -> io::Result<()> {
        self.view = View::Output;
        append_history(&self.session, command)?;
        self.history.push(command.to_string());
        self.history_cursor = self.history.len();
        self.push_output(format!("> {command}"));

        if command.contains("--execute") && (command.contains("--follow") || command.contains(" -f")) {
            self.push_output("blocked in cockpit: use F9 shell mode for live follow commands");
            return Ok(());
        }

        let lines = run_command_child(&self.session, command)?;
        for line in lines {
            self.push_output(line);
        }
        Ok(())
    }

    fn kill_word_back(&mut self) {
        while self.cursor > 0 && self.input[self.cursor - 1].is_whitespace() {
            self.input.remove(self.cursor - 1);
            self.cursor -= 1;
        }
        while self.cursor > 0 && !self.input[self.cursor - 1].is_whitespace() {
            self.input.remove(self.cursor - 1);
            self.cursor -= 1;
        }
    }

    fn history_up(&mut self) {
        if self.history_cursor > 0 {
            self.history_cursor -= 1;
            self.input = self.history[self.history_cursor].chars().collect();
            self.cursor = self.input.len();
        }
    }

    fn history_down(&mut self) {
        if self.history_cursor + 1 < self.history.len() {
            self.history_cursor += 1;
            self.input = self.history[self.history_cursor].chars().collect();
        } else {
            self.history_cursor = self.history.len();
            self.input.clear();
        }
        self.cursor = self.input.len();
    }

    fn complete(&mut self) {
        let prefix = self.input.iter().take(self.cursor).collect::<String>();
        let clean_prefix = prefix.trim_start();
        if clean_prefix.is_empty() {
            self.suggestions = self.completions.iter().take(16).cloned().collect();
            return;
        }

        let matches = self
            .completions
            .iter()
            .filter(|candidate| candidate.starts_with(clean_prefix))
            .cloned()
            .collect::<Vec<_>>();
        if matches.is_empty() {
            return;
        }

        let common = common_prefix(&matches);
        if common.len() > clean_prefix.len() {
            let leading = prefix.len() - clean_prefix.len();
            let completed = format!("{}{}", " ".repeat(leading), common);
            self.input = completed.chars().collect();
            self.cursor = self.input.len();
        } else {
            self.suggestions = matches.into_iter().take(16).collect();
        }
    }

    fn cycle_view(&mut self) {
        self.view = match self.view {
            View::Output => View::Help,
            View::Help => View::Commands,
            View::Commands => View::Logs,
            View::Logs => View::Panic,
            View::Panic => View::Output,
        };
    }
}

enum TuiAction {
    Continue,
    Quit,
    Shell,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Key {
    Char(char),
    Enter,
    Backspace,
    Delete,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    Tab,
    Esc,
    Ctrl(char),
    F(u8),
}

struct TerminalGuard {
    saved_stty: String,
}

impl TerminalGuard {
    fn enter() -> io::Result<Self> {
        let output = Command::new("stty").arg("-g").output()?;
        let saved_stty = String::from_utf8_lossy(&output.stdout).trim().to_string();
        let status = Command::new("stty")
            .args(["raw", "-echo", "min", "0", "time", "10"])
            .status()?;
        if !status.success() {
            return Err(io::Error::other("failed to enter raw terminal mode"));
        }
        write!(io::stdout(), "\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H")?;
        io::stdout().flush()?;
        Ok(Self { saved_stty })
    }
}

impl Drop for TerminalGuard {
    fn drop(&mut self) {
        let _ = write!(io::stdout(), "\x1b[?25h\x1b[?1049l{RESET}");
        let _ = io::stdout().flush();
        let _ = Command::new("stty").arg(&self.saved_stty).status();
    }
}

fn run_command_child(session: &Session, command: &str) -> io::Result<Vec<String>> {
    let exe = env::current_exe()?;
    let mut child = Command::new(exe)
        .arg("--user")
        .arg(&session.sekailink_user)
        .arg("--role")
        .arg(session.role.as_str())
        .arg("--data-dir")
        .arg(&session.data_dir)
        .arg("--command")
        .arg(command)
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()?;

    let stdout = child.stdout.take();
    let stderr = child.stderr.take();
    let stdout_reader = thread::spawn(move || read_pipe(stdout));
    let stderr_reader = thread::spawn(move || read_pipe(stderr));
    let started = Instant::now();
    let mut timed_out = false;
    let status = loop {
        if let Some(status) = child.try_wait()? {
            break Some(status);
        }
        if started.elapsed() >= COMMAND_TIMEOUT {
            timed_out = true;
            let _ = child.kill();
            break child.wait().ok();
        }
        thread::sleep(Duration::from_millis(50));
    };

    let stdout = stdout_reader.join().unwrap_or_default();
    let stderr = stderr_reader.join().unwrap_or_default();
    let mut lines = Vec::new();
    if timed_out {
        lines.push(format!(
            "command timed out after {}s and was stopped",
            COMMAND_TIMEOUT.as_secs()
        ));
    }
    if let Some(status) = status {
        lines.push(format!("exit status: {status}"));
    }
    lines.extend(stdout.lines().map(str::to_string));
    if !stderr.trim().is_empty() {
        lines.push("--- stderr ---".to_string());
        lines.extend(stderr.lines().map(str::to_string));
    }
    if lines.is_empty() {
        lines.push("(no output)".to_string());
    }
    Ok(lines)
}

fn read_pipe<T: Read + Send + 'static>(pipe: Option<T>) -> String {
    let Some(mut pipe) = pipe else {
        return String::new();
    };
    let mut text = String::new();
    let _ = pipe.read_to_string(&mut text);
    text
}

fn draw(state: &TuiState) -> io::Result<()> {
    let (cols, rows) = terminal_size();
    let cols = cols.max(80);
    let rows = rows.max(24);
    let mut out = String::new();
    out.push_str("\x1b[2J\x1b[H");
    draw_header(&mut out, state, cols);
    draw_left(&mut out, state, cols, rows);
    draw_right(&mut out, state, cols, rows);
    draw_command_line(&mut out, state, cols, rows);
    write!(io::stdout(), "{out}")?;
    io::stdout().flush()
}

fn draw_header(out: &mut String, state: &TuiState, cols: u16) {
    let title = format!(
        " SekaiLink Core Access | user={} role={} | F1 help F9 shell Ctrl+Q quit ",
        state.session.sekailink_user,
        state.session.role.as_str()
    );
    write_at(out, 1, 1, &format!("{BLUE_BG}{BOLD}{}{RESET}", fit(&title, cols as usize)));
    let local = local_health();
    let status = format!(
        "{GREEN}Nexus{RESET} local={} load={} ram={} disk={}   {YELLOW}Remote live checks require --execute gate{RESET}",
        local.uptime, local.load, local.memory, local.disk
    );
    write_at(out, 2, 1, &fit_ansi(&status, cols as usize));
    hline(out, 3, 1, cols, '-');
}

fn draw_left(out: &mut String, state: &TuiState, cols: u16, rows: u16) {
    let left_w = left_width(cols);
    let bottom = rows.saturating_sub(4);
    box_frame(out, 4, 1, bottom, left_w, "Matrix");
    let mut row = 5;
    write_at(out, row, 3, &format!("{BOLD}SERVER       SERVICES{RESET}"));
    row += 1;
    for (server, services) in known_services() {
        let service_text = format!("{} svc", services.len());
        write_at(
            out,
            row,
            3,
            &fit(
                &format!("{:<12} {}", server.to_ascii_uppercase(), service_text),
                left_w.saturating_sub(4) as usize,
            ),
        );
        row += 1;
    }

    row += 1;
    write_at(out, row, 3, &format!("{CYAN}Hotkeys{RESET}"));
    row += 1;
    for line in [
        "F1 Help       F7 Audit",
        "F2 Status     F8 Logs",
        "F3 Commands   F9 Shell",
        "F4 Note       F10 Clear",
        "F5 Refresh    F12 Panic",
    ] {
        write_at(out, row, 3, &fit(line, left_w.saturating_sub(4) as usize));
        row += 1;
    }

    row += 1;
    write_at(out, row, 3, &format!("{MAGENTA}Context{RESET}"));
    row += 1;
    let view = match state.view {
        View::Output => "output",
        View::Help => "help",
        View::Commands => "commands",
        View::Logs => "logs",
        View::Panic => "panic",
    };
    write_at(out, row, 3, &fit(&format!("view: {view}"), left_w.saturating_sub(4) as usize));
    row += 1;
    let age = state.last_status_refresh.elapsed().as_secs();
    write_at(out, row, 3, &fit(&format!("refresh: {age}s ago"), left_w.saturating_sub(4) as usize));

    if !state.suggestions.is_empty() {
        row += 2;
        write_at(out, row, 3, &format!("{YELLOW}Suggestions{RESET}"));
        for suggestion in state.suggestions.iter().take((bottom - row).saturating_sub(1) as usize) {
            row += 1;
            write_at(out, row, 3, &fit(suggestion, left_w.saturating_sub(4) as usize));
        }
    }
}

fn draw_right(out: &mut String, state: &TuiState, cols: u16, rows: u16) {
    let left_w = left_width(cols);
    let x = left_w + 2;
    let w = cols.saturating_sub(left_w + 2);
    let bottom = rows.saturating_sub(4);
    let title = match state.view {
        View::Output => "Command Output",
        View::Help => "Help",
        View::Commands => "Command Registry",
        View::Logs => "Log Sources",
        View::Panic => "Panic",
    };
    box_frame(out, 4, x, bottom, w, title);
    let height = bottom.saturating_sub(5) as usize;
    let width = w.saturating_sub(4) as usize;
    let lines = view_lines(state);
    let start = lines.len().saturating_sub(height);
    for (idx, line) in lines.iter().skip(start).take(height).enumerate() {
        write_at(out, 5 + idx as u16, x + 2, &fit_ansi(line, width));
    }
}

fn draw_command_line(out: &mut String, state: &TuiState, cols: u16, rows: u16) {
    let top = rows.saturating_sub(3);
    box_frame(out, top, 1, rows, cols, "Command");
    let prompt = "skl> ";
    let input_width = cols.saturating_sub(6 + prompt.len() as u16) as usize;
    let input = state.current_input();
    let start = if state.cursor > input_width {
        state.cursor - input_width
    } else {
        0
    };
    let visible = input.chars().skip(start).take(input_width).collect::<String>();
    write_at(out, rows - 1, 3, &format!("{CYAN}{prompt}{RESET}{visible}"));
    let cursor_col = 3 + prompt.len() as u16 + state.cursor.saturating_sub(start) as u16;
    out.push_str(&format!("\x1b[{};{}H\x1b[?25h", rows - 1, cursor_col));
}

fn view_lines(state: &TuiState) -> Vec<String> {
    match state.view {
        View::Output => state.output.clone(),
        View::Help => vec![
            "Core Access cockpit".to_string(),
            "".to_string(),
            "Type any Core Access command at the bottom prompt.".to_string(),
            "Tab completes command names; Up/Down browse command history.".to_string(),
            "Left/Right edit in the middle of the line; Ctrl+A/E jump edges.".to_string(),
            "F9 drops to the older shell for long manual sessions.".to_string(),
            "".to_string(),
            "Read-only remote execution still needs --execute plus the env gate.".to_string(),
            "Protected Nexus calls still need the matching admin token env var.".to_string(),
        ],
        View::Commands => COMMANDS
            .iter()
            .map(|cmd| {
                let implemented = if cmd.implemented { "ready" } else { "planned" };
                format!(
                    "{:<34} {:<7} {:<7} {:?}",
                    cmd.name,
                    cmd.role.as_str(),
                    implemented,
                    cmd.confirmation
                )
            })
            .collect(),
        View::Logs => log_catalog()
            .iter()
            .map(|(source, server, description)| format!("{source:<22} {server:<10} {description}"))
            .collect(),
        View::Panic => vec![
            format!("{RED}{BOLD}PANIC VIEW{RESET}"),
            "1. Stabilize the smallest scope first.".to_string(),
            "2. Use maintenance drafts before broad impact.".to_string(),
            "3. Snapshot: ops snapshot <incident-label>".to_string(),
            "4. Attach notes: note add <target> <text>".to_string(),
            "5. Use --shell for live log follow.".to_string(),
            "6. Never mutate SKLMI without explicit approval.".to_string(),
        ],
    }
}

fn read_key() -> io::Result<Option<Key>> {
    let mut input = io::stdin();
    let mut byte = [0_u8; 1];
    if input.read(&mut byte)? == 0 {
        return Ok(None);
    }
    let key = match byte[0] {
        b'\r' | b'\n' => Key::Enter,
        b'\t' => Key::Tab,
        3 => Key::Ctrl('c'),
        17 => Key::Ctrl('q'),
        1 => Key::Ctrl('a'),
        5 => Key::Ctrl('e'),
        23 => Key::Ctrl('w'),
        8 | 127 => Key::Backspace,
        27 => read_escape(&mut input)?,
        byte if byte.is_ascii_graphic() || byte == b' ' => Key::Char(byte as char),
        _ => return Ok(None),
    };
    Ok(Some(key))
}

fn read_escape(input: &mut io::Stdin) -> io::Result<Key> {
    let mut first = [0_u8; 1];
    if input.read(&mut first)? == 0 {
        return Ok(Key::Esc);
    }
    match first[0] {
        b'O' => {
            let mut code = [0_u8; 1];
            if input.read(&mut code)? == 0 {
                return Ok(Key::Esc);
            }
            Ok(match code[0] {
                b'P' => Key::F(1),
                b'Q' => Key::F(2),
                b'R' => Key::F(3),
                b'S' => Key::F(4),
                _ => Key::Esc,
            })
        }
        b'[' => read_csi(input),
        _ => Ok(Key::Esc),
    }
}

fn read_csi(input: &mut io::Stdin) -> io::Result<Key> {
    let mut seq = Vec::new();
    for _ in 0..8 {
        let mut byte = [0_u8; 1];
        if input.read(&mut byte)? == 0 {
            break;
        }
        seq.push(byte[0]);
        if byte[0].is_ascii_alphabetic() || byte[0] == b'~' {
            break;
        }
    }
    Ok(match seq.as_slice() {
        [b'A'] => Key::Up,
        [b'B'] => Key::Down,
        [b'C'] => Key::Right,
        [b'D'] => Key::Left,
        [b'H'] | [b'1', b'~'] | [b'7', b'~'] => Key::Home,
        [b'F'] | [b'4', b'~'] | [b'8', b'~'] => Key::End,
        [b'3', b'~'] => Key::Delete,
        [b'1', b'1', b'~'] => Key::F(1),
        [b'1', b'2', b'~'] => Key::F(2),
        [b'1', b'3', b'~'] => Key::F(3),
        [b'1', b'4', b'~'] => Key::F(4),
        [b'1', b'5', b'~'] => Key::F(5),
        [b'1', b'7', b'~'] => Key::F(6),
        [b'1', b'8', b'~'] => Key::F(7),
        [b'1', b'9', b'~'] => Key::F(8),
        [b'2', b'0', b'~'] => Key::F(9),
        [b'2', b'1', b'~'] => Key::F(10),
        [b'2', b'3', b'~'] => Key::F(11),
        [b'2', b'4', b'~'] => Key::F(12),
        _ => Key::Esc,
    })
}

fn terminal_size() -> (u16, u16) {
    let output = Command::new("stty").arg("size").output();
    if let Ok(output) = output {
        let text = String::from_utf8_lossy(&output.stdout);
        let mut parts = text.split_whitespace();
        if let (Some(rows), Some(cols)) = (parts.next(), parts.next()) {
            if let (Ok(rows), Ok(cols)) = (rows.parse::<u16>(), cols.parse::<u16>()) {
                return (cols, rows);
            }
        }
    }
    (120, 34)
}

fn left_width(cols: u16) -> u16 {
    (cols / 3).clamp(32, 46)
}

fn write_at(out: &mut String, row: u16, col: u16, text: &str) {
    out.push_str(&format!("\x1b[{row};{col}H{text}"));
}

fn hline(out: &mut String, row: u16, col: u16, width: u16, ch: char) {
    write_at(out, row, col, &ch.to_string().repeat(width as usize));
}

fn box_frame(out: &mut String, top: u16, left: u16, bottom: u16, width: u16, title: &str) {
    if bottom <= top + 1 || width < 4 {
        return;
    }
    let horizontal = "-".repeat(width.saturating_sub(2) as usize);
    write_at(out, top, left, &format!("{CYAN}+{horizontal}+{RESET}"));
    for row in top + 1..bottom {
        write_at(out, row, left, &format!("{CYAN}|{RESET}"));
        write_at(out, row, left + width - 1, &format!("{CYAN}|{RESET}"));
    }
    write_at(out, bottom, left, &format!("{CYAN}+{horizontal}+{RESET}"));
    let label = format!(" {title} ");
    write_at(out, top, left + 2, &format!("{BOLD}{label}{RESET}"));
}

fn fit(value: &str, width: usize) -> String {
    let mut out = value.chars().take(width).collect::<String>();
    let len = out.chars().count();
    if len < width {
        out.push_str(&" ".repeat(width - len));
    }
    out
}

fn fit_ansi(value: &str, width: usize) -> String {
    let plain = strip_ansi(value);
    if plain.chars().count() <= width {
        return value.to_string();
    }
    fit(&plain, width)
}

fn strip_ansi(value: &str) -> String {
    let mut out = String::new();
    let mut chars = value.chars().peekable();
    while let Some(ch) = chars.next() {
        if ch == '\x1b' && chars.peek() == Some(&'[') {
            chars.next();
            for code in chars.by_ref() {
                if code.is_ascii_alphabetic() {
                    break;
                }
            }
        } else {
            out.push(ch);
        }
    }
    out
}

fn common_prefix(values: &[String]) -> String {
    if values.is_empty() {
        return String::new();
    }
    let mut prefix = values[0].clone();
    for value in values.iter().skip(1) {
        while !value.starts_with(&prefix) {
            if prefix.is_empty() {
                return prefix;
            }
            prefix.pop();
        }
    }
    prefix
}

#[cfg(test)]
mod tests {
    use super::{common_prefix, strip_ansi};

    #[test]
    fn common_prefix_finds_command_stem() {
        let values = vec!["server status".to_string(), "server services".to_string()];
        assert_eq!(common_prefix(&values), "server s");
    }

    #[test]
    fn ansi_strip_removes_color_sequences() {
        assert_eq!(strip_ansi("\x1b[31mred\x1b[0m"), "red");
    }
}

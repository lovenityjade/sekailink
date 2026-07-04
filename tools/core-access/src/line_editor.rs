use std::io::{self, IsTerminal, Read, Write};
use std::process::Command;

pub struct LineEditor {
    history: Vec<String>,
    completions: Vec<String>,
}

impl LineEditor {
    pub fn new(completions: Vec<String>, history: Vec<String>) -> Self {
        Self {
            history,
            completions,
        }
    }

    pub fn read_line(&mut self, prompt: &str) -> io::Result<Option<String>> {
        if !io::stdin().is_terminal() {
            return self.read_line_plain(prompt);
        }
        self.read_line_raw(prompt)
    }

    fn read_line_plain(&mut self, prompt: &str) -> io::Result<Option<String>> {
        print!("{prompt}");
        io::stdout().flush()?;
        let mut line = String::new();
        let read = io::stdin().read_line(&mut line)?;
        if read == 0 {
            return Ok(None);
        }
        let line = line.trim_end_matches(['\r', '\n']).to_string();
        if !line.trim().is_empty() {
            self.history.push(line.clone());
        }
        Ok(Some(line))
    }

    fn read_line_raw(&mut self, prompt: &str) -> io::Result<Option<String>> {
        let _guard = RawMode::enter()?;
        let mut input = io::stdin();
        let mut stdout = io::stdout();
        let mut buffer: Vec<char> = Vec::new();
        let mut cursor = 0_usize;
        let mut history_cursor = self.history.len();

        write!(stdout, "{prompt}")?;
        stdout.flush()?;

        loop {
            let mut byte = [0_u8; 1];
            if input.read(&mut byte)? == 0 {
                return Ok(None);
            }
            match byte[0] {
                b'\r' | b'\n' => {
                    writeln!(stdout)?;
                    let line: String = buffer.iter().collect();
                    if !line.trim().is_empty() {
                        self.history.push(line.clone());
                    }
                    return Ok(Some(line));
                }
                3 => {
                    writeln!(stdout, "^C")?;
                    return Ok(Some(String::new()));
                }
                4 => {
                    writeln!(stdout)?;
                    return Ok(None);
                }
                1 => cursor = 0,
                5 => cursor = buffer.len(),
                23 => {
                    while cursor > 0 && buffer[cursor - 1].is_whitespace() {
                        buffer.remove(cursor - 1);
                        cursor -= 1;
                    }
                    while cursor > 0 && !buffer[cursor - 1].is_whitespace() {
                        buffer.remove(cursor - 1);
                        cursor -= 1;
                    }
                }
                b'\t' => {
                    self.complete_or_show(prompt, &mut buffer, &mut cursor)?;
                }
                8 | 127 => {
                    if cursor > 0 {
                        buffer.remove(cursor - 1);
                        cursor -= 1;
                    }
                }
                27 => {
                    self.handle_escape(&mut input, &mut buffer, &mut cursor, &mut history_cursor)?;
                }
                byte if byte.is_ascii_graphic() || byte == b' ' => {
                    buffer.insert(cursor, byte as char);
                    cursor += 1;
                }
                _ => {}
            }
            redraw(prompt, &buffer, cursor)?;
        }
    }

    fn handle_escape(
        &self,
        input: &mut io::Stdin,
        buffer: &mut Vec<char>,
        cursor: &mut usize,
        history_cursor: &mut usize,
    ) -> io::Result<()> {
        let mut seq = [0_u8; 2];
        if input.read(&mut seq)? != 2 || seq[0] != b'[' {
            return Ok(());
        }
        match seq[1] {
            b'A' => {
                if *history_cursor > 0 {
                    *history_cursor -= 1;
                    *buffer = self.history[*history_cursor].chars().collect();
                    *cursor = buffer.len();
                }
            }
            b'B' => {
                if *history_cursor + 1 < self.history.len() {
                    *history_cursor += 1;
                    *buffer = self.history[*history_cursor].chars().collect();
                } else {
                    *history_cursor = self.history.len();
                    buffer.clear();
                }
                *cursor = buffer.len();
            }
            b'C' => {
                if *cursor < buffer.len() {
                    *cursor += 1;
                }
            }
            b'D' => {
                if *cursor > 0 {
                    *cursor -= 1;
                }
            }
            _ => {}
        }
        Ok(())
    }

    fn complete_or_show(
        &self,
        prompt: &str,
        buffer: &mut Vec<char>,
        cursor: &mut usize,
    ) -> io::Result<()> {
        let prefix: String = buffer.iter().take(*cursor).collect();
        let matches: Vec<&String> = self
            .completions
            .iter()
            .filter(|candidate| candidate.starts_with(prefix.trim_start()))
            .collect();
        if matches.is_empty() {
            return Ok(());
        }
        let common = common_prefix(&matches);
        if common.len() > prefix.trim_start().len() {
            let leading = prefix.len() - prefix.trim_start().len();
            let completed = format!("{}{}", " ".repeat(leading), common);
            *buffer = completed.chars().collect();
            *cursor = buffer.len();
            return Ok(());
        }
        println!();
        for item in matches.iter().take(16) {
            println!("  {item}");
        }
        if matches.len() > 16 {
            println!("  ... {} more", matches.len() - 16);
        }
        redraw(prompt, buffer, *cursor)
    }
}

struct RawMode {
    saved: String,
}

impl RawMode {
    fn enter() -> io::Result<Self> {
        let output = Command::new("stty").arg("-g").output()?;
        let saved = String::from_utf8_lossy(&output.stdout).trim().to_string();
        let status = Command::new("stty")
            .args(["raw", "-echo", "min", "1", "time", "0"])
            .status()?;
        if !status.success() {
            return Err(io::Error::other("failed to enable raw terminal mode"));
        }
        Ok(Self { saved })
    }
}

impl Drop for RawMode {
    fn drop(&mut self) {
        let _ = Command::new("stty").arg(&self.saved).status();
    }
}

fn redraw(prompt: &str, buffer: &[char], cursor: usize) -> io::Result<()> {
    let mut stdout = io::stdout();
    let text: String = buffer.iter().collect();
    write!(stdout, "\r\x1b[2K{prompt}{text}")?;
    let right = buffer.len().saturating_sub(cursor);
    if right > 0 {
        write!(stdout, "\x1b[{right}D")?;
    }
    stdout.flush()
}

fn common_prefix(values: &[&String]) -> String {
    if values.is_empty() {
        return String::new();
    }
    let mut prefix = values[0].to_string();
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

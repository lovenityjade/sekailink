mod app;
mod audit;
mod commands;
mod line_editor;
mod nexus;
mod rbac;
mod system;
mod util;

fn main() {
    if let Err(err) = app::run() {
        eprintln!("core-access error: {err}");
        std::process::exit(1);
    }
}

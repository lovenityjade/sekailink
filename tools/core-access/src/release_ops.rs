use std::collections::BTreeMap;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Debug, Clone)]
pub struct ReleaseManifest {
    pub path: PathBuf,
    pub date_dir: String,
    pub generated_at: String,
    pub version: String,
    pub channel: String,
    pub build: String,
    pub base_url: String,
    pub artifacts: Vec<ReleaseArtifact>,
    pub releases: Vec<ReleaseEntry>,
}

#[derive(Debug, Clone)]
pub struct ReleaseArtifact {
    pub key: String,
    pub path: Option<String>,
    pub file_name: String,
    pub sha256: String,
    pub size: Option<u64>,
    pub url: String,
}

#[derive(Debug, Clone)]
pub struct ReleaseEntry {
    pub platform: String,
    pub artifact_type: String,
    pub download_url: String,
    pub sha256: String,
    pub fallback_download_url: String,
    pub fallback_sha256: String,
    pub fallback_artifact_type: String,
}

#[derive(Debug, Clone)]
pub struct ArtifactVerification {
    pub key: String,
    pub file_name: String,
    pub path: PathBuf,
    pub exists: bool,
    pub expected_size: Option<u64>,
    pub actual_size: Option<u64>,
    pub size_ok: Option<bool>,
    pub expected_sha256: String,
    pub actual_sha256: Option<String>,
    pub sha_ok: Option<bool>,
}

pub fn discover_manifests(repo_root: &Path) -> io::Result<Vec<ReleaseManifest>> {
    let root = update_bundle_root(repo_root);
    let mut manifests = Vec::new();
    if !root.exists() {
        return Ok(manifests);
    }
    for entry in fs::read_dir(root)? {
        let entry = entry?;
        let file_type = entry.file_type()?;
        if !file_type.is_dir() {
            continue;
        }
        for manifest in fs::read_dir(entry.path())? {
            let manifest = manifest?;
            let path = manifest.path();
            let Some(name) = path.file_name().and_then(|value| value.to_str()) else {
                continue;
            };
            if name == "client_release_manifest.fragment.json"
                || (name.starts_with("sekailink-client-release-") && name.ends_with(".json"))
            {
                if let Ok(parsed) = parse_manifest(&path) {
                    manifests.push(parsed);
                }
            }
        }
    }
    manifests.sort_by(|a, b| manifest_sort_key(a).cmp(&manifest_sort_key(b)));
    Ok(manifests)
}

pub fn dedupe_manifests(manifests: &[ReleaseManifest]) -> Vec<ReleaseManifest> {
    let mut by_key = BTreeMap::new();
    for manifest in manifests {
        let key = format!(
            "{}:{}:{}:{}:{}",
            manifest.date_dir,
            manifest.version,
            manifest.channel,
            manifest.build,
            manifest.base_url
        );
        let prefer_new = by_key
            .get(&key)
            .map(|current: &ReleaseManifest| {
                manifest_priority(manifest) > manifest_priority(current)
            })
            .unwrap_or(true);
        if prefer_new {
            by_key.insert(key, manifest.clone());
        }
    }
    by_key.into_values().collect()
}

pub fn latest_manifest(repo_root: &Path) -> io::Result<Option<ReleaseManifest>> {
    let manifests = dedupe_manifests(&discover_manifests(repo_root)?);
    Ok(manifests.into_iter().last())
}

pub fn resolve_manifest(
    repo_root: &Path,
    selector: Option<&str>,
) -> io::Result<Option<ReleaseManifest>> {
    let selector = selector
        .map(str::trim)
        .filter(|value| !value.is_empty() && *value != "latest");
    if let Some(selector) = selector {
        let path = Path::new(selector);
        if path.exists() {
            return parse_manifest(path).map(Some);
        }
    }

    let manifests = dedupe_manifests(&discover_manifests(repo_root)?);
    let Some(selector) = selector else {
        return Ok(manifests.into_iter().last());
    };
    Ok(manifests.into_iter().find(|manifest| {
        manifest.version == selector
            || manifest.date_dir == selector
            || manifest.path.to_string_lossy().contains(selector)
    }))
}

pub fn render_manifest_list(manifests: &[ReleaseManifest]) -> String {
    if manifests.is_empty() {
        return "no local release manifests found under apps/client-core/release/update-bundles"
            .to_string();
    }
    let mut out = String::new();
    out.push_str(&format!(
        "{:<10} {:<34} {:<8} {:<8} {:<9} {}\n",
        "DATE", "VERSION", "CHANNEL", "BUILD", "ARTIFACTS", "MANIFEST"
    ));
    out.push_str(&"-".repeat(112));
    out.push('\n');
    for manifest in manifests {
        out.push_str(&format!(
            "{:<10} {:<34} {:<8} {:<8} {:<9} {}\n",
            manifest.date_dir,
            truncate(&manifest.version, 34),
            manifest.channel,
            manifest.build,
            manifest.artifacts.len(),
            manifest.path.display()
        ));
    }
    out
}

pub fn render_manifest_summary(manifest: &ReleaseManifest) -> String {
    let mut out = String::new();
    out.push_str(&format!("version: {}\n", manifest.version));
    out.push_str(&format!("channel: {}\n", manifest.channel));
    out.push_str(&format!("build: {}\n", manifest.build));
    out.push_str(&format!(
        "generated_at: {}\n",
        empty_dash(&manifest.generated_at)
    ));
    out.push_str(&format!("base_url: {}\n", empty_dash(&manifest.base_url)));
    out.push_str(&format!("manifest: {}\n\n", manifest.path.display()));
    out.push_str("Artifacts\n");
    out.push_str(&format!(
        "{:<12} {:<12} {:<66} {}\n",
        "KEY", "SIZE", "SHA256", "FILE"
    ));
    out.push_str(&"-".repeat(112));
    out.push('\n');
    for artifact in &manifest.artifacts {
        out.push_str(&format!(
            "{:<12} {:<12} {:<66} {}\n",
            artifact.key,
            artifact
                .size
                .map(|value| value.to_string())
                .unwrap_or_else(|| "-".to_string()),
            truncate(&artifact.sha256, 66),
            artifact.file_name
        ));
        if !artifact.url.trim().is_empty() {
            out.push_str(&format!("  url: {}\n", artifact.url));
        }
    }
    out.push_str("\nRelease entries\n");
    out.push_str(&format!(
        "{:<12} {:<14} {:<66} {}\n",
        "PLATFORM", "TYPE", "SHA256", "DOWNLOAD"
    ));
    out.push_str(&"-".repeat(112));
    out.push('\n');
    for release in &manifest.releases {
        out.push_str(&format!(
            "{:<12} {:<14} {:<66} {}\n",
            release.platform,
            release.artifact_type,
            truncate(&release.sha256, 66),
            release.download_url
        ));
        if !release.fallback_download_url.trim().is_empty() {
            out.push_str(&format!(
                "  fallback: type={} sha256={} url={}\n",
                empty_dash(&release.fallback_artifact_type),
                empty_dash(&release.fallback_sha256),
                release.fallback_download_url
            ));
        }
    }
    out
}

pub fn verify_manifest(manifest: &ReleaseManifest, check_sha: bool) -> Vec<ArtifactVerification> {
    manifest
        .artifacts
        .iter()
        .map(|artifact| {
            let path = artifact_path(manifest, artifact);
            let metadata = fs::metadata(&path).ok();
            let actual_size = metadata.as_ref().map(|meta| meta.len());
            let size_ok = match (artifact.size, actual_size) {
                (Some(expected), Some(actual)) => Some(expected == actual),
                (Some(_), None) => Some(false),
                _ => None,
            };
            let actual_sha256 = if check_sha && metadata.is_some() {
                sha256_file(&path).ok().flatten()
            } else {
                None
            };
            let sha_ok = if check_sha {
                match (artifact.sha256.trim().is_empty(), actual_sha256.as_deref()) {
                    (false, Some(actual)) => Some(artifact.sha256.eq_ignore_ascii_case(actual)),
                    (false, None) => Some(false),
                    _ => None,
                }
            } else {
                None
            };
            ArtifactVerification {
                key: artifact.key.clone(),
                file_name: artifact.file_name.clone(),
                path,
                exists: metadata.is_some(),
                expected_size: artifact.size,
                actual_size,
                size_ok,
                expected_sha256: artifact.sha256.clone(),
                actual_sha256,
                sha_ok,
            }
        })
        .collect()
}

pub fn render_verification(items: &[ArtifactVerification], check_sha: bool) -> String {
    if items.is_empty() {
        return "manifest has no artifacts".to_string();
    }
    let mut out = String::new();
    out.push_str(&format!(
        "{:<12} {:<8} {:<10} {:<8} {}\n",
        "KEY", "EXISTS", "SIZE", "SHA", "FILE"
    ));
    out.push_str(&"-".repeat(112));
    out.push('\n');
    for item in items {
        let size_status = match item.size_ok {
            Some(true) => "ok",
            Some(false) => "mismatch",
            None => "n/a",
        };
        let sha_status = if check_sha {
            match item.sha_ok {
                Some(true) => "ok",
                Some(false) => "mismatch",
                None => "n/a",
            }
        } else {
            "skipped"
        };
        out.push_str(&format!(
            "{:<12} {:<8} {:<10} {:<8} {}\n",
            item.key,
            if item.exists { "yes" } else { "no" },
            size_status,
            sha_status,
            item.file_name
        ));
        out.push_str(&format!("  path: {}\n", item.path.display()));
        if item.expected_size.is_some() || item.actual_size.is_some() {
            out.push_str(&format!(
                "  size: expected={} actual={}\n",
                item.expected_size
                    .map(|value| value.to_string())
                    .unwrap_or_else(|| "-".to_string()),
                item.actual_size
                    .map(|value| value.to_string())
                    .unwrap_or_else(|| "-".to_string())
            ));
        }
        if check_sha {
            out.push_str(&format!(
                "  sha256: expected={} actual={}\n",
                empty_dash(&item.expected_sha256),
                item.actual_sha256.as_deref().unwrap_or("-")
            ));
        }
    }
    out
}

pub fn render_manifest_compare(left: &ReleaseManifest, right: &ReleaseManifest) -> String {
    let mut out = String::new();
    out.push_str(&format!(
        "left:  {} ({})\n",
        left.version,
        left.path.display()
    ));
    out.push_str(&format!(
        "right: {} ({})\n\n",
        right.version,
        right.path.display()
    ));
    push_compare_line(&mut out, "version", &left.version, &right.version);
    push_compare_line(&mut out, "channel", &left.channel, &right.channel);
    push_compare_line(&mut out, "build", &left.build, &right.build);
    push_compare_line(&mut out, "base_url", &left.base_url, &right.base_url);
    out.push('\n');
    out.push_str("Artifacts\n");
    let left_artifacts = left
        .artifacts
        .iter()
        .map(|artifact| (artifact.key.clone(), artifact))
        .collect::<BTreeMap<_, _>>();
    let right_artifacts = right
        .artifacts
        .iter()
        .map(|artifact| (artifact.key.clone(), artifact))
        .collect::<BTreeMap<_, _>>();
    for key in left_artifacts
        .keys()
        .chain(right_artifacts.keys())
        .collect::<std::collections::BTreeSet<_>>()
    {
        match (left_artifacts.get(key), right_artifacts.get(key)) {
            (Some(left), Some(right)) => {
                out.push_str(&format!(
                    "- {}: {} -> {}; size {} -> {}; sha {}\n",
                    key,
                    left.file_name,
                    right.file_name,
                    left.size
                        .map(|value| value.to_string())
                        .unwrap_or_else(|| "-".to_string()),
                    right
                        .size
                        .map(|value| value.to_string())
                        .unwrap_or_else(|| "-".to_string()),
                    if left.sha256 == right.sha256 {
                        "same"
                    } else {
                        "changed"
                    }
                ));
            }
            (Some(left), None) => {
                out.push_str(&format!("- {}: removed ({})\n", key, left.file_name));
            }
            (None, Some(right)) => {
                out.push_str(&format!("- {}: added ({})\n", key, right.file_name));
            }
            (None, None) => {}
        }
    }
    out
}

pub fn public_release_urls(channel: &str, platform: &str) -> Vec<String> {
    let platforms = if platform == "all" {
        vec!["linux-x64", "win32-x64"]
    } else {
        vec![platform]
    };
    platforms
        .into_iter()
        .map(|platform| {
            format!(
                "https://sekailink.com/api/client/release-latest?channel={}&platform={}",
                url_component(channel),
                url_component(platform)
            )
        })
        .collect()
}

fn parse_manifest(path: &Path) -> io::Result<ReleaseManifest> {
    let text = fs::read_to_string(path)?;
    let date_dir = path
        .parent()
        .and_then(Path::file_name)
        .and_then(|value| value.to_str())
        .unwrap_or("-")
        .to_string();
    let artifacts = json_array_objects(&text, "artifacts")
        .into_iter()
        .map(|object| ReleaseArtifact {
            key: json_string(&object, "key"),
            path: non_empty(json_string(&object, "path")),
            file_name: json_string(&object, "fileName"),
            sha256: json_string(&object, "sha256"),
            size: json_number(&object, "size"),
            url: json_string(&object, "url"),
        })
        .filter(|artifact| !artifact.file_name.is_empty())
        .collect::<Vec<_>>();
    let releases = json_array_objects(&text, "releases")
        .into_iter()
        .map(|object| ReleaseEntry {
            platform: json_string(&object, "platform"),
            artifact_type: json_string(&object, "artifact_type"),
            download_url: json_string(&object, "download_url"),
            sha256: json_string(&object, "sha256"),
            fallback_download_url: json_string(&object, "fallback_download_url"),
            fallback_sha256: json_string(&object, "fallback_sha256"),
            fallback_artifact_type: json_string(&object, "fallback_artifact_type"),
        })
        .filter(|entry| !entry.platform.is_empty())
        .collect::<Vec<_>>();
    Ok(ReleaseManifest {
        path: path.to_path_buf(),
        date_dir,
        generated_at: json_string(&text, "generated_at"),
        version: json_string(&text, "version"),
        channel: json_string(&text, "channel"),
        build: json_string(&text, "build"),
        base_url: json_string(&text, "base_url"),
        artifacts,
        releases,
    })
}

fn update_bundle_root(repo_root: &Path) -> PathBuf {
    repo_root
        .join("apps")
        .join("client-core")
        .join("release")
        .join("update-bundles")
}

fn artifact_path(manifest: &ReleaseManifest, artifact: &ReleaseArtifact) -> PathBuf {
    if let Some(path) = artifact.path.as_deref().filter(|value| !value.is_empty()) {
        return PathBuf::from(path);
    }
    manifest
        .path
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .join(&artifact.file_name)
}

fn sha256_file(path: &Path) -> io::Result<Option<String>> {
    let output = Command::new("sha256sum").arg(path).output();
    let Ok(output) = output else {
        return Ok(None);
    };
    if !output.status.success() {
        return Ok(None);
    }
    let text = String::from_utf8_lossy(&output.stdout);
    Ok(text.split_whitespace().next().map(str::to_string))
}

fn manifest_sort_key(manifest: &ReleaseManifest) -> String {
    format!(
        "{}:{}:{}",
        manifest.generated_at,
        manifest.date_dir,
        manifest.path.display()
    )
}

fn manifest_priority(manifest: &ReleaseManifest) -> u8 {
    manifest
        .path
        .file_name()
        .and_then(|value| value.to_str())
        .map(|name| u8::from(name.starts_with("sekailink-client-release-")))
        .unwrap_or(0)
}

fn push_compare_line(out: &mut String, label: &str, left: &str, right: &str) {
    if left == right {
        out.push_str(&format!("{label}: same ({})\n", empty_dash(left)));
    } else {
        out.push_str(&format!(
            "{label}: {} -> {}\n",
            empty_dash(left),
            empty_dash(right)
        ));
    }
}

fn json_array_objects(text: &str, key: &str) -> Vec<String> {
    let Some(key_pos) = text.find(&format!("\"{key}\"")) else {
        return Vec::new();
    };
    let Some(array_start_rel) = text[key_pos..].find('[') else {
        return Vec::new();
    };
    let array_start = key_pos + array_start_rel;
    let Some(array_end) = matching_bracket(text, array_start, '[', ']') else {
        return Vec::new();
    };
    let array = &text[array_start + 1..array_end];
    let mut objects = Vec::new();
    let mut depth = 0_usize;
    let mut start = None;
    let mut in_string = false;
    let mut escaped = false;
    for (index, ch) in array.char_indices() {
        if in_string {
            if escaped {
                escaped = false;
            } else if ch == '\\' {
                escaped = true;
            } else if ch == '"' {
                in_string = false;
            }
            continue;
        }
        match ch {
            '"' => in_string = true,
            '{' => {
                if depth == 0 {
                    start = Some(index);
                }
                depth += 1;
            }
            '}' => {
                depth = depth.saturating_sub(1);
                if depth == 0 {
                    if let Some(start) = start.take() {
                        objects.push(array[start..=index].to_string());
                    }
                }
            }
            _ => {}
        }
    }
    objects
}

fn matching_bracket(text: &str, start: usize, open: char, close: char) -> Option<usize> {
    let mut depth = 0_usize;
    let mut in_string = false;
    let mut escaped = false;
    for (offset, ch) in text[start..].char_indices() {
        if in_string {
            if escaped {
                escaped = false;
            } else if ch == '\\' {
                escaped = true;
            } else if ch == '"' {
                in_string = false;
            }
            continue;
        }
        match ch {
            '"' => in_string = true,
            ch if ch == open => depth += 1,
            ch if ch == close => {
                depth = depth.saturating_sub(1);
                if depth == 0 {
                    return Some(start + offset);
                }
            }
            _ => {}
        }
    }
    None
}

fn json_string(text: &str, key: &str) -> String {
    let Some(key_pos) = text.find(&format!("\"{key}\"")) else {
        return String::new();
    };
    let Some(colon_rel) = text[key_pos..].find(':') else {
        return String::new();
    };
    let value_start = key_pos + colon_rel + 1;
    let rest = text[value_start..].trim_start();
    if let Some(stripped) = rest.strip_prefix('"') {
        return read_json_string(stripped);
    }
    rest.split(|ch: char| ch == ',' || ch == '}' || ch.is_whitespace())
        .next()
        .unwrap_or("")
        .trim()
        .to_string()
}

fn read_json_string(text: &str) -> String {
    let mut out = String::new();
    let mut escaped = false;
    for ch in text.chars() {
        if escaped {
            match ch {
                '"' => out.push('"'),
                '\\' => out.push('\\'),
                '/' => out.push('/'),
                'n' => out.push('\n'),
                'r' => out.push('\r'),
                't' => out.push('\t'),
                other => out.push(other),
            }
            escaped = false;
            continue;
        }
        match ch {
            '\\' => escaped = true,
            '"' => break,
            other => out.push(other),
        }
    }
    out
}

fn json_number(text: &str, key: &str) -> Option<u64> {
    let value = json_string(text, key);
    value.parse::<u64>().ok()
}

fn non_empty(value: String) -> Option<String> {
    let trimmed = value.trim().to_string();
    (!trimmed.is_empty()).then_some(trimmed)
}

fn empty_dash(value: &str) -> &str {
    if value.trim().is_empty() { "-" } else { value }
}

fn truncate(value: &str, max: usize) -> String {
    if value.chars().count() <= max {
        return value.to_string();
    }
    let mut out = value
        .chars()
        .take(max.saturating_sub(1))
        .collect::<String>();
    out.push('~');
    out
}

fn url_component(value: &str) -> String {
    let mut out = String::new();
    for byte in value.bytes() {
        match byte {
            b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'-' | b'_' | b'.' | b'~' => {
                out.push(byte as char)
            }
            _ => out.push_str(&format!("%{byte:02X}")),
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::{json_array_objects, json_string, public_release_urls};

    #[test]
    fn json_string_reads_basic_values() {
        let text = r#"{"version":"0.3.1","size":42,"name":"Sekai\"Link"}"#;
        assert_eq!(json_string(text, "version"), "0.3.1");
        assert_eq!(json_string(text, "size"), "42");
        assert_eq!(json_string(text, "name"), "Sekai\"Link");
    }

    #[test]
    fn array_object_extractor_handles_multiple_entries() {
        let text = r#"{"artifacts":[{"key":"linux"},{"key":"win"}],"other":[]}"#;
        let objects = json_array_objects(text, "artifacts");
        assert_eq!(objects.len(), 2);
        assert!(objects[0].contains("linux"));
        assert!(objects[1].contains("win"));
    }

    #[test]
    fn public_release_urls_expands_all_platforms() {
        let urls = public_release_urls("test", "all");
        assert_eq!(urls.len(), 2);
        assert!(urls[0].contains("platform=linux-x64"));
        assert!(urls[1].contains("platform=win32-x64"));
    }
}

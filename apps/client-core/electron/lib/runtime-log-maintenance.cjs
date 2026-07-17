const DEFAULT_MAX_TRACE_BYTES = 16 * 1024 * 1024;
const DEFAULT_MAX_ENTRIES = 10000;

function pruneOversizedSklmiTraceLogs({
  rootDir,
  fs,
  path,
  maxBytes = DEFAULT_MAX_TRACE_BYTES,
  maxEntries = DEFAULT_MAX_ENTRIES,
}) {
  const result = { filesPruned: 0, bytesReclaimed: 0, entriesScanned: 0, errors: [] };
  if (!rootDir || !fs.existsSync(rootDir)) return result;

  const pending = [rootDir];
  while (pending.length > 0 && result.entriesScanned < maxEntries) {
    const directory = pending.pop();
    let entries;
    try {
      entries = fs.readdirSync(directory, { withFileTypes: true });
    } catch (error) {
      result.errors.push(String(error?.message || error || "readdir_failed"));
      continue;
    }

    for (const entry of entries) {
      if (result.entriesScanned >= maxEntries) break;
      result.entriesScanned += 1;
      const candidate = path.join(directory, entry.name);
      if (entry.isSymbolicLink()) continue;
      if (entry.isDirectory()) {
        pending.push(candidate);
        continue;
      }
      if (!entry.isFile() || (entry.name !== "trace.jsonl" && entry.name !== "trace.jsonl.1")) continue;

      try {
        const size = fs.statSync(candidate).size;
        if (size <= maxBytes) continue;
        fs.truncateSync(candidate, 0);
        result.filesPruned += 1;
        result.bytesReclaimed += size;
      } catch (error) {
        result.errors.push(String(error?.message || error || "trace_prune_failed"));
      }
    }
  }
  return result;
}

module.exports = {
  DEFAULT_MAX_TRACE_BYTES,
  pruneOversizedSklmiTraceLogs,
};

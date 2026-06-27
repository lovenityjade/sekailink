"use strict";

const createPythonRuntime = ({
  app,
  fs,
  path,
  spawn,
  spawnSync,
  https,
  processRef = process,
  getRuntimeToolsDir,
  getRuntimeFilePath,
  getBundledThirdPartyDir,
  getBundledApRootDir,
  withApPythonEnv,
  emitSessionEvent = () => {},
  downloadToFile,
  extractZip,
  ensureDir,
  findPathByBasename,
  writeLogLine = () => {},
}) => {
  let pythonRuntimePromise = null;
  let pythonBootstrapCache = null;

  const runProcess = (cmd, args, options = {}) => {
    return new Promise((resolve) => {
      let settled = false;
      const done = (result) => {
        if (settled) return;
        settled = true;
        resolve(result);
      };
      const proc = spawn(cmd, args, { stdio: ["ignore", "pipe", "pipe"], ...options });
      let stdout = "";
      let stderr = "";
      proc.stdout.on("data", (c) => (stdout += String(c)));
      proc.stderr.on("data", (c) => (stderr += String(c)));
      proc.on("error", (err) => done({ code: 1, stdout, stderr: `${stderr}${String(err || "")}` }));
      proc.on("exit", (code) => done({ code: code ?? 0, stdout, stderr }));
    });
  };

  const getPythonCommand = () => {
    const fromEnv = processRef.env.SEKAILINK_PYTHON || processRef.env.PYTHON;
    if (fromEnv) return fromEnv;
    return processRef.platform === "win32" ? "python" : "python3";
  };

  const getPythonVenvDir = () => path.join(getRuntimeToolsDir(), "python", "venv");

  const getPythonVenvPythonPath = () => {
    const venvDir = getPythonVenvDir();
    if (processRef.platform === "win32") return path.join(venvDir, "Scripts", "python.exe");
    return path.join(venvDir, "bin", "python");
  };

  const getPythonRuntimeWheelhouseTag = () => {
    if (processRef.platform === "win32" && processRef.arch === "x64") return "win-amd64-cp312";
    return "";
  };

  const getPythonRuntimeWheelhouseDir = () => {
    const tag = getPythonRuntimeWheelhouseTag();
    if (!tag) return "";
    return getRuntimeFilePath(path.join("tools", "python", "wheelhouse", tag));
  };

  const getPythonPipInstallArgs = (specs = []) => {
    const normalizedSpecs = Array.isArray(specs)
      ? specs.map((spec) => String(spec || "").trim()).filter(Boolean)
      : [];
    const wheelhouseDir = getPythonRuntimeWheelhouseDir();
    if (wheelhouseDir && fs.existsSync(wheelhouseDir)) {
      return ["-m", "pip", "install", "--no-index", "--find-links", wheelhouseDir, ...normalizedSpecs];
    }
    if (processRef.platform === "win32" && app.isPackaged) {
      throw new Error(`python_wheelhouse_missing:${wheelhouseDir || "win-amd64-cp312"}`);
    }
    return ["-m", "pip", "install", ...normalizedSpecs];
  };

  const PYTHON_SCOPED_WORLDS_SNIPPET = `
worlds_dir = os.path.join(ap_root, "worlds")
if os.path.isdir(worlds_dir):
  pkg = types.ModuleType("worlds")
  pkg.__path__ = [worlds_dir]
  for entry in os.scandir(worlds_dir):
    if entry.is_file() and entry.name.endswith(".apworld"):
      pkg.__path__.append(entry.path)
  pkg.__package__ = "worlds"
  pkg.__sekailink_scoped__ = True
  sys.modules["worlds"] = pkg
`.trim();

  const getBundledPythonVenvPythonPath = () => {
    const tag = getPythonRuntimeWheelhouseTag();
    if (!tag) return "";
    const root = getRuntimeFilePath(path.join("tools", "python", "venv", tag));
    if (processRef.platform === "win32") return path.join(root, "Scripts", "python.exe");
    return path.join(root, "bin", "python");
  };

  const getWindowsPortablePythonVersion = () => {
    const value = String(processRef.env.SEKAILINK_WIN_PYTHON_VERSION || "3.12.8").trim();
    return value || "3.12.8";
  };

  const resolveNugetPackageSha512 = async (packageId, version) => {
    const pkg = String(packageId || "").trim().toLowerCase();
    const ver = String(version || "").trim().toLowerCase();
    if (!pkg || !ver) throw new Error("missing_nuget_package");
    const shaUrl = `https://api.nuget.org/v3-flatcontainer/${pkg}/${ver}/${pkg}.${ver}.nupkg.sha512`;
    return new Promise((resolve, reject) => {
      const req = https.get(shaUrl, { headers: { "User-Agent": "SekaiLink/1.0", "Accept": "text/plain" } }, (res) => {
        if (res.statusCode !== 200) {
          res.resume();
          return reject(new Error(`nuget_hash_http_${res.statusCode || "unknown"}`));
        }
        let body = "";
        res.on("data", (chunk) => {
          body += String(chunk || "");
        });
        res.on("end", () => {
          const raw = body.trim();
          try {
            const hex = Buffer.from(raw, "base64").toString("hex").toLowerCase();
            if (!/^[a-f0-9]{128}$/.test(hex)) return reject(new Error("nuget_hash_invalid"));
            resolve(hex);
          } catch (err) {
            reject(err);
          }
        });
      });
      req.on("error", reject);
    });
  };

  const getWindowsPortablePythonRoot = () => path.join(getRuntimeToolsDir(), "python", "portable-win");

  const getWindowsPortablePythonExe = () => path.join(getWindowsPortablePythonRoot(), "tools", "python.exe");

  const getRuntimePortableWindowsPythonRoot = () => getRuntimeFilePath(path.join("tools", "python", "portable-win"));

  const pythonExeProbeOk = (pythonPath) => {
    const p = String(pythonPath || "").trim();
    if (!p) return false;
    try {
      if (!fs.existsSync(p)) return false;
      const probe = spawnSync(
        p,
        ["-c", "import sys; print(sys.executable); raise SystemExit(0 if (3, 12) <= sys.version_info[:2] < (3, 14) else 2)"],
        {
          stdio: ["ignore", "ignore", "ignore"],
          env: { ...processRef.env, PYTHONNOUSERSITE: "1" },
        }
      );
      return probe.status === 0;
    } catch (_err) {
      return false;
    }
  };

  const listExistingWindowsPortablePythonExes = () => {
    if (processRef.platform !== "win32") return [];
    const roots = [getWindowsPortablePythonRoot(), getRuntimePortableWindowsPythonRoot()];
    const seen = new Set();
    const out = [];
    const add = (candidate) => {
      const p = String(candidate || "").trim();
      if (!p) return;
      const k = p.toLowerCase();
      if (seen.has(k)) return;
      if (!fs.existsSync(p)) return;
      seen.add(k);
      out.push(p);
    };

    for (const root of roots) {
      if (!root || !fs.existsSync(root)) continue;
      add(path.join(root, "tools", "python.exe"));
      add(path.join(root, "python.exe"));
      const discovered = findPathByBasename(root, "python.exe", 8);
      if (discovered && !String(discovered).toLowerCase().includes(`${path.sep}venv${path.sep}`)) add(discovered);
    }
    return out;
  };

  const getPrivatePythonCandidates = () => {
    const candidates = [];
    const add = (candidate, label) => {
      const p = String(candidate || "").trim();
      if (!p) return;
      candidates.push({ cmd: p, args: [], label });
    };
    const arch =
      processRef.arch === "x64" ? "x86_64" : processRef.arch === "arm64" ? "arm64" : String(processRef.arch || "unknown");

    if (processRef.platform === "win32") {
      for (const portable of listExistingWindowsPortablePythonExes()) {
        add(portable, "private-python-win");
      }
    } else if (processRef.platform === "linux") {
      add(path.join(getRuntimeFilePath(path.join("tools", "python", "portable-linux")), "bin", "python3"), "private-python-linux");
      add(path.join(getRuntimeFilePath(path.join("tools", "python", `linux-${arch}`)), "bin", "python3"), "private-python-linux");
      add(path.join(getBundledThirdPartyDir(), "python", `sekailink-python-linux-${arch}`, "bin", "python3"), "private-python-linux");
    } else if (processRef.platform === "darwin") {
      add(path.join(getRuntimeFilePath(path.join("tools", "python", `macos-${arch}`)), "bin", "python3"), "private-python-macos");
      add(path.join(getBundledThirdPartyDir(), "python", `sekailink-python-macos-${arch}`, "bin", "python3"), "private-python-macos");
    }
    return candidates.filter((candidate) => fs.existsSync(candidate.cmd));
  };

  const ensurePortableWindowsPython = async () => {
    if (processRef.platform !== "win32") return "";
    const existingPortable = listExistingWindowsPortablePythonExes();
    if (existingPortable.length) {
      for (const candidate of existingPortable) {
        if (pythonExeProbeOk(candidate)) return candidate;
      }
    }

    const root = getWindowsPortablePythonRoot();

    emitSessionEvent({ event: "status", status: "Preparing SekaiLink runtime (Windows, one-time)..." });
    const version = getWindowsPortablePythonVersion();
    const url = `https://www.nuget.org/api/v2/package/python/${version}`;
    const expectedSha512 = await resolveNugetPackageSha512("python", version);
    const dlDir = path.join(getRuntimeToolsDir(), "python", "downloads");
    ensureDir(dlDir);
    const pkgPath = path.join(dlDir, `python-${version}-win64.nupkg`);
    await downloadToFile(url, pkgPath, {
      expectedSha512,
      requireHash: true,
    });

    fs.rmSync(root, { recursive: true, force: true });
    ensureDir(root);
    extractZip(pkgPath, root);

    const direct = getWindowsPortablePythonExe();
    if (fs.existsSync(direct)) return direct;
    const discovered = findPathByBasename(root, "python.exe", 8);
    if (discovered && fs.existsSync(discovered)) return discovered;
    throw new Error(`python_portable_install_failed: python.exe missing under ${root}`);
  };

  const getPythonBootstrapCandidates = () => {
    const candidates = [];
    const explicit = String(processRef.env.SEKAILINK_PYTHON || processRef.env.PYTHON || "").trim();
    if (explicit) candidates.push({ cmd: explicit, args: [], label: "env" });
    candidates.push(...getPrivatePythonCandidates());

    if (processRef.platform === "win32") {
      if (!app.isPackaged || processRef.env.SEKAILINK_ALLOW_SYSTEM_PYTHON === "1") {
        candidates.push(
          { cmd: "py", args: ["-3"], label: "py3" },
          { cmd: "py", args: [], label: "py" },
          { cmd: "python3", args: [], label: "python3" },
          { cmd: "python", args: [], label: "python" }
        );
      }
    } else if (!app.isPackaged || processRef.env.SEKAILINK_ALLOW_SYSTEM_PYTHON === "1") {
      candidates.push(
        { cmd: "python3.13", args: [], label: "python3.13" },
        { cmd: "python3.12", args: [], label: "python3.12" },
        { cmd: "python3", args: [], label: "python3" },
        { cmd: "python", args: [], label: "python" }
      );
    }
    return candidates;
  };

  const probePythonCandidate = async (candidate) => {
    const cmd = String(candidate?.cmd || "").trim();
    const args = Array.isArray(candidate?.args) ? candidate.args : [];
    if (!cmd) return false;
    const probe = await runProcess(
      cmd,
      [...args, "-c", "import sys; print(sys.executable); raise SystemExit(0 if (3, 12) <= sys.version_info[:2] < (3, 14) else 2)"],
      {
        env: { ...processRef.env, PYTHONNOUSERSITE: "1" },
      }
    );
    return probe.code === 0;
  };

  const resolveBootstrapPython = async () => {
    if (pythonBootstrapCache) return pythonBootstrapCache;
    for (const candidate of getPythonBootstrapCandidates()) {
      if (await probePythonCandidate(candidate)) {
        pythonBootstrapCache = candidate;
        return pythonBootstrapCache;
      }
    }
    if (processRef.platform === "win32" && (!app.isPackaged || processRef.env.SEKAILINK_ALLOW_RUNTIME_DOWNLOAD === "1")) {
      try {
        const portable = await ensurePortableWindowsPython();
        const candidate = { cmd: portable, args: [], label: "portable-win" };
        if (await probePythonCandidate(candidate)) {
          pythonBootstrapCache = candidate;
          return pythonBootstrapCache;
        }
      } catch (err) {
        writeLogLine("warn", "python-runtime", `portable python install failed: ${String(err || "")}`);
      }
    }
    return null;
  };

  const PYTHON_RUNTIME_IMPORT_CHECK = `
import importlib, sys
assert (3, 12) <= sys.version_info[:2] < (3, 14)
mods = [
  "pkg_resources",
  "yaml",
  "pathspec",
  "platformdirs",
  "schema",
  "jinja2",
  "colorama",
  "typing_extensions",
  "jellyfish",
  "bsdiff4",
  "websockets",
  "requests",
  "certifi",
  "orjson",
  "PIL",
  "dotenv",
  "pyaes",
  "docutils",
  "dolphin_memory_engine",
  "pyevermizer",
]
for m in mods:
  importlib.import_module(m)
print("ok")
`.trim();

  const verifyPythonRuntimeReady = async (pythonPath, env = {}) => {
    const p = String(pythonPath || "").trim();
    if (!p || !fs.existsSync(p)) return { ok: false, error: "python_missing", python: p };
    const check = await runProcess(p, ["-c", PYTHON_RUNTIME_IMPORT_CHECK], {
      env: { ...processRef.env, ...env, PYTHONNOUSERSITE: "1", SKIP_REQUIREMENTS_UPDATE: "1" },
    });
    if (check.code === 0) return { ok: true, python: p };
    return {
      ok: false,
      python: p,
      error: String(check.stderr || check.stdout || "python_runtime_import_check_failed").trim(),
    };
  };

  const resolveBundledReadyPythonRuntime = async () => {
    const candidates = [
      getBundledPythonVenvPythonPath(),
      ...(processRef.platform === "win32" ? listExistingWindowsPortablePythonExes() : []),
      ...getPrivatePythonCandidates().map((candidate) => candidate.cmd),
    ].filter(Boolean);
    const seen = new Set();
    const failures = [];
    for (const candidate of candidates) {
      const key = String(candidate).toLowerCase();
      if (seen.has(key)) continue;
      seen.add(key);
      const result = await verifyPythonRuntimeReady(candidate);
      if (result.ok) return result.python;
      failures.push({ python: candidate, error: result.error });
    }
    const detail = failures.map((entry) => `${entry.python}: ${entry.error}`).join(" | ");
    throw new Error(`python_runtime_not_prepared:${detail || "no bundled Python runtime with required modules"}`);
  };

  const parseMissingModule = (text) => {
    const s = String(text || "");
    const m =
      s.match(/MISSING_MODULE:([A-Za-z0-9_\\.\\-]+)/) ||
      s.match(/ModuleNotFoundError:\\s*No module named ['\\\"]([^'\\\"]+)['\\\"]/);
    if (!m) return "";
    return String(m[1] || "").trim();
  };

  const getPipSpecForModule = (moduleName) => {
    const name = String(moduleName || "").trim();
    if (!name) return "";
    const map = {
      yaml: "pyyaml",
      PIL: "pillow",
      dotenv: "python-dotenv",
      pkg_resources: "setuptools<70",
    };
    return map[name] || name;
  };

  const ensurePythonRuntime = async () => {
    if (pythonRuntimePromise) return pythonRuntimePromise;
    pythonRuntimePromise = (async () => {
      if (app.isPackaged && processRef.env.SEKAILINK_ALLOW_RUNTIME_BOOTSTRAP !== "1") {
        const packagedPython = await resolveBundledReadyPythonRuntime();
        writeLogLine("info", "python-runtime", `using bundled ready Python runtime: ${packagedPython}`);
        return packagedPython;
      }

      const bootstrap = await resolveBootstrapPython();
      if (!bootstrap) {
        throw new Error("python_missing: no usable Python interpreter found");
      }
      const bootstrapPython = bootstrap.cmd;
      const bootstrapArgs = bootstrap.args || [];
      const venvDir = getPythonVenvDir();
      const venvPython = getPythonVenvPythonPath();

      if (fs.existsSync(venvPython)) {
        const check = await runProcess(
          venvPython,
          [
            "-c",
            "import sys; assert (3, 12) <= sys.version_info[:2] < (3, 14); import pkg_resources, yaml, pathspec, platformdirs, schema, jinja2, colorama, typing_extensions, bsdiff4, websockets, certifi, orjson, PIL, dotenv, pyaes, docutils, dolphin_memory_engine, pyevermizer; print('ok')",
          ],
          {
            env: { ...processRef.env, PYTHONNOUSERSITE: "1" },
          }
        );
        if (check.code === 0) return venvPython;
        writeLogLine("warn", "python-runtime", `venv present but missing deps; will reinstall. stderr=${String(check.stderr || "").trim()}`);
        fs.rmSync(venvDir, { recursive: true, force: true });
      }

      emitSessionEvent({ event: "status", status: "Preparing launch runtime (one-time)..." });

      ensureDir(path.dirname(venvDir));
      if (!fs.existsSync(venvPython)) {
        let created = await runProcess(bootstrapPython, [...bootstrapArgs, "-m", "venv", venvDir], {
          env: { ...processRef.env, PYTHONNOUSERSITE: "1" },
        });
        if (created.code !== 0 && processRef.platform === "win32") {
          await runProcess(bootstrapPython, [...bootstrapArgs, "-m", "ensurepip", "--upgrade"], {
            env: { ...processRef.env, PYTHONNOUSERSITE: "1" },
          });
          created = await runProcess(bootstrapPython, [...bootstrapArgs, "-m", "venv", venvDir], {
            env: { ...processRef.env, PYTHONNOUSERSITE: "1" },
          });
        }
        if (created.code !== 0) {
          const msg = String(created.stderr || created.stdout || "venv_failed").trim();
          writeLogLine("error", "python-runtime", `failed to create venv: ${msg}`);
          throw new Error(`python_venv_failed: ${msg}`);
        }
      }

      const wheelhouseDir = getPythonRuntimeWheelhouseDir();
      const hasWheelhouse = Boolean(wheelhouseDir && fs.existsSync(wheelhouseDir));
      if (processRef.platform === "win32" && app.isPackaged && !hasWheelhouse) {
        throw new Error(`python_wheelhouse_missing:${wheelhouseDir || "win-amd64-cp312"}`);
      }
      const pipEnv = {
        ...processRef.env,
        PYTHONNOUSERSITE: "1",
        PYTHONDONTWRITEBYTECODE: "1",
        SKIP_REQUIREMENTS_UPDATE: "1",
        PIP_DISABLE_PIP_VERSION_CHECK: "1",
        PIP_NO_INPUT: "1",
        ...(hasWheelhouse ? { PIP_NO_INDEX: "1", PIP_FIND_LINKS: wheelhouseDir } : {}),
      };

      const upgraded = await runProcess(venvPython, getPythonPipInstallArgs(["setuptools<70", "wheel"]), { env: pipEnv });
      if (upgraded.code !== 0) {
        const msg = String(upgraded.stderr || upgraded.stdout || "pip_bootstrap_failed").trim();
        writeLogLine("warn", "python-runtime", `pip bootstrap failed (continuing best-effort): ${msg}`);
      }

      const deps = new Set([
        "setuptools<70",
        "wheel",
        "pyyaml",
        "pathspec",
        "platformdirs",
        "schema",
        "jinja2",
        "colorama",
        "typing_extensions",
        "jellyfish",
        "bsdiff4",
        "websockets",
        "certifi",
        "orjson",
        "pillow",
        "python-dotenv",
        "pyaes",
        "docutils",
        "dolphin-memory-engine",
        "pyevermizer",
      ]);

      const installOne = async (spec) => {
        const s = String(spec || "").trim();
        if (!s) return { ok: true };
        const res = await runProcess(venvPython, getPythonPipInstallArgs([s]), { env: pipEnv });
        if (res.code !== 0) {
          const msg = String(res.stderr || res.stdout || "pip_failed").trim();
          writeLogLine("error", "python-runtime", `pip install failed spec=${s}: ${msg}`);
          return { ok: false, error: msg };
        }
        return { ok: true };
      };

      const base = await runProcess(venvPython, getPythonPipInstallArgs(Array.from(deps)), { env: pipEnv });
      if (base.code !== 0) {
        const msg = String(base.stderr || base.stdout || "pip_failed").trim();
        writeLogLine("error", "python-runtime", `pip base install failed: ${msg}`);
        throw new Error(`python_deps_install_failed: ${msg}`);
      }

      const importCheckCode = `
import importlib, sys
mods = [
  "pkg_resources",
  "yaml",
  "pathspec",
  "platformdirs",
  "schema",
  "jinja2",
  "colorama",
  "typing_extensions",
  "jellyfish",
  "bsdiff4",
  "websockets",
  "certifi",
  "orjson",
  "PIL",
  "dotenv",
  "pyaes",
  "docutils",
  "dolphin_memory_engine",
  "pyevermizer",
]
for m in mods:
  try:
    importlib.import_module(m)
  except ModuleNotFoundError as e:
    print("MISSING_MODULE:" + (e.name or m), flush=True)
    raise
print("ok")
`.trim();

      for (let attempt = 0; attempt < 8; attempt += 1) {
        const check = await runProcess(venvPython, ["-c", importCheckCode], { env: { ...pipEnv } });
        if (check.code === 0) break;
        const missing = parseMissingModule(check.stdout) || parseMissingModule(check.stderr);
        if (!missing) {
          const msg = String(check.stderr || check.stdout || "deps_verify_failed").trim();
          throw new Error(`python_deps_verify_failed: ${msg}`);
        }
        const spec = getPipSpecForModule(missing);
        emitSessionEvent({ event: "status", status: "Preparing launch runtime component..." });
        const one = await installOne(spec);
        if (!one.ok) throw new Error(`python_deps_install_failed: ${one.error || "pip_failed"}`);
      }

      const apRoot = getBundledApRootDir();
      const apProbeModules = ["Patch", "NetUtils", "Utils", "worlds.Files"];
      const apProbeCode = `
import importlib, os, sys, warnings
import types
warnings.filterwarnings("ignore", message=r"_speedups not available.*", category=UserWarning)
ap_root = os.environ.get("SEKAILINK_AP_ROOT","")
if ap_root and ap_root not in sys.path:
  sys.path.insert(0, ap_root)
${PYTHON_SCOPED_WORLDS_SNIPPET}
mods = ${JSON.stringify(apProbeModules)}
for m in mods:
  try:
    importlib.import_module(m)
  except ModuleNotFoundError as e:
    print("MISSING_MODULE:" + (e.name or ""), flush=True)
    raise
print("ok")
`.trim();

      for (let attempt = 0; attempt < 20; attempt += 1) {
        const probe = await runProcess(venvPython, ["-c", apProbeCode], {
          env: { ...pipEnv, SEKAILINK_AP_ROOT: apRoot, PYTHONPATH: apRoot },
        });
        if (probe.code === 0) break;
        const missing = parseMissingModule(probe.stdout) || parseMissingModule(probe.stderr);
        if (!missing) {
          const msg = String(probe.stderr || probe.stdout || "ap_probe_failed").trim();
          throw new Error(`python_deps_verify_failed: ${msg}`);
        }
        const spec = getPipSpecForModule(missing);
        emitSessionEvent({ event: "status", status: "Preparing launch runtime component..." });
        const one = await installOne(spec);
        if (!one.ok) throw new Error(`python_deps_install_failed: ${one.error || "pip_failed"}`);
      }

      const verify = await runProcess(
        venvPython,
        [
          "-c",
          "import pkg_resources, yaml, pathspec, platformdirs, schema, jinja2, colorama, typing_extensions, jellyfish, bsdiff4, websockets, certifi, orjson, PIL, dotenv, pyaes, docutils, dolphin_memory_engine, pyevermizer; print('ok')",
        ],
        {
          env: { ...pipEnv },
        }
      );
      if (verify.code !== 0) {
        const msg = String(verify.stderr || verify.stdout || "verify_failed").trim();
        writeLogLine("error", "python-runtime", `deps verify failed: ${msg}`);
        throw new Error(`python_deps_verify_failed: ${msg}`);
      }

      emitSessionEvent({ event: "status", status: "Launch runtime ready." });
      return venvPython;
    })().catch((err) => {
      pythonRuntimePromise = null;
      throw err;
    });
    return pythonRuntimePromise;
  };

  const verifyPatcherPythonWorld = async (python, patchWorldModule) => {
    const world = String(patchWorldModule || "").trim().slice(0, 300);
    if (!world) return { ok: true };

    const modules = Array.from(new Set(["Patch", "worlds.Files", world]));
    const probeCode = `
import importlib, os, sys, warnings
import types
warnings.filterwarnings("ignore", message=r"_speedups not available.*", category=UserWarning)
ap_root = os.environ.get("SEKAILINK_AP_ROOT","")
if ap_root and ap_root not in sys.path:
  sys.path.insert(0, ap_root)
${PYTHON_SCOPED_WORLDS_SNIPPET}
mods = ${JSON.stringify(modules)}
for m in mods:
  try:
    importlib.import_module(m)
  except ModuleNotFoundError as e:
    print("MISSING_MODULE:" + (e.name or ""), flush=True)
    raise
print("ok")
`.trim();

    const probe = await runProcess(python, ["-c", probeCode], {
      env: withApPythonEnv(processRef.env),
    });
    if (probe.code === 0) return { ok: true };

    const msg = String(probe.stderr || probe.stdout || "world_probe_failed").trim();
    writeLogLine("error", "python-runtime", `patcher world probe failed world=${world}: ${msg}`);
    return { ok: false, error: msg };
  };

  return {
    getPythonCommand,
    ensurePythonRuntime,
    verifyPatcherPythonWorld,
    runProcess,
  };
};

module.exports = { createPythonRuntime };

const createProcessTools = ({ net, spawnSync, processRef = process, writeLogLine, withApPythonEnv }) => {
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, Math.max(0, Number(ms || 0))));

  const waitForTcpPort = (host, port, timeoutMs = 8000) => {
    const h = String(host || "127.0.0.1");
    const p = Number(port || 0);
    const deadline = Date.now() + Math.max(0, Number(timeoutMs || 0));
    return new Promise((resolve) => {
      const tick = () => {
        const sock = new net.Socket();
        let done = false;
        const finish = (ok) => {
          if (done) return;
          done = true;
          try {
            sock.destroy();
          } catch (_e) {
            // ignore close failures
          }
          if (ok) return resolve(true);
          if (Date.now() >= deadline) return resolve(false);
          setTimeout(tick, 120);
        };
        sock.setTimeout(500);
        sock.once("connect", () => finish(true));
        sock.once("timeout", () => finish(false));
        sock.once("error", () => finish(false));
        try {
          sock.connect(p, h);
        } catch (_err) {
          finish(false);
        }
      };
      tick();
    });
  };

  const waitForAnyTcpPort = async (host, ports, timeoutMs = 8000) => {
    const list = Array.isArray(ports) ? ports.map((v) => Number(v)).filter((v) => Number.isFinite(v) && v > 0) : [];
    const deadline = Date.now() + Math.max(0, Number(timeoutMs || 0));
    while (Date.now() < deadline) {
      for (const p of list) {
        const ok = await waitForTcpPort(host, p, 600);
        if (ok) return p;
      }
      await sleep(120);
    }
    return 0;
  };

  const findFreeLocalPort = (preferred = 38281) => new Promise((resolve) => {
    const server = net.createServer();
    server.once("error", () => {
      const fallback = net.createServer();
      fallback.listen(0, "127.0.0.1", () => {
        const address = fallback.address();
        const port = typeof address === "object" && address ? address.port : 0;
        fallback.close(() => resolve(port || preferred));
      });
    });
    server.listen(preferred, "127.0.0.1", () => {
      const address = server.address();
      const port = typeof address === "object" && address ? address.port : preferred;
      server.close(() => resolve(port));
    });
  });

  const findFreeLocalPortInRange = async (start, end) => {
    for (let port = start; port <= end; port += 1) {
      const available = await new Promise((resolve) => {
        const server = net.createServer();
        server.once("error", () => resolve(0));
        server.listen(port, "127.0.0.1", () => {
          server.close(() => resolve(port));
        });
      });
      if (available) return available;
    }
    return 0;
  };

  const isPidAlive = (pid) => {
    const p = Number(pid || 0);
    if (!Number.isFinite(p) || p <= 0) return false;
    try {
      processRef.kill(p, 0);
      return true;
    } catch (_err) {
      return false;
    }
  };

  const waitForChildExit = async (proc, timeoutMs = 1200) => {
    if (!proc) return true;
    if (proc.exitCode !== null) return true;
    return await new Promise((resolve) => {
      let done = false;
      const finish = (value) => {
        if (done) return;
        done = true;
        clearTimeout(timer);
        resolve(Boolean(value));
      };
      const timer = setTimeout(() => finish(false), Math.max(100, Number(timeoutMs || 0)));
      proc.once("exit", () => finish(true));
    });
  };

  const terminateChildProcess = async (proc, scope = "proc", opts = {}) => {
    if (!proc) return;
    const pid = Number(proc.pid || 0);
    const graceMs = Math.max(200, Number(opts.graceMs || 1200));
    try {
      proc.kill("SIGTERM");
    } catch (_err) {
      try {
        proc.kill();
      } catch (_err2) {
        // ignore
      }
    }
    const exited = await waitForChildExit(proc, graceMs);
    if (exited) return;
    if (pid > 0 && isPidAlive(pid)) {
      writeLogLine?.("warn", scope, `forcing SIGKILL pid=${pid}`);
      try {
        processRef.kill(pid, "SIGKILL");
      } catch (_err) {
        // ignore
      }
      await sleep(120);
    }
  };

  const getListeningPidsOnTcpPort = (port) => {
    const p = Number(port || 0);
    if (!Number.isFinite(p) || p <= 0) return [];
    try {
      if (processRef.platform === "win32") {
        const out = spawnSync("netstat", ["-ano", "-p", "tcp"], { encoding: "utf-8", windowsHide: true });
        const text = `${out.stdout || ""}\n${out.stderr || ""}`;
        const pids = new Set();
        for (const line of text.split(/\r?\n/).filter(Boolean)) {
          const row = line.trim();
          if (!/\bLISTENING\b/i.test(row)) continue;
          if (!row.match(new RegExp(`[:\\.]${p}\\s+`))) continue;
          const parts = row.split(/\s+/);
          const pid = Number(parts[parts.length - 1] || 0);
          if (pid > 0) pids.add(pid);
        }
        return Array.from(pids);
      }

      const out = spawnSync("ss", ["-ltnp", `sport = :${p}`], { encoding: "utf-8" });
      const text = `${out.stdout || ""}\n${out.stderr || ""}`;
      const pids = new Set();
      const re = /pid=(\d+)/g;
      let m;
      while ((m = re.exec(text)) !== null) {
        const pid = Number(m[1] || 0);
        if (pid > 0) pids.add(pid);
      }
      return Array.from(pids);
    } catch (_err) {
      return [];
    }
  };

  const getPidCommandLine = (pid) => {
    const p = Number(pid || 0);
    if (!Number.isFinite(p) || p <= 0) return "";
    try {
      if (processRef.platform === "win32") {
        const cmd = `(Get-CimInstance Win32_Process -Filter \"ProcessId=${p}\").CommandLine`;
        const out = spawnSync("powershell.exe", ["-NoProfile", "-Command", cmd], { encoding: "utf-8", windowsHide: true });
        return String(out.stdout || "").trim();
      }
      const out = spawnSync("ps", ["-p", String(p), "-o", "args="], { encoding: "utf-8" });
      return String(out.stdout || "").trim();
    } catch (_err) {
      return "";
    }
  };

  const isSekaiLinkBridgeCommand = (cmdline) => {
    const s = String(cmdline || "").toLowerCase();
    return s.includes("sni_bridge.py") || s.includes("sniclient_wrapper.py") || s.includes("bizhawkclient_wrapper.py") || s.includes("sekailink");
  };

  const killPidGracefully = async (pid, scope = "proc") => {
    const p = Number(pid || 0);
    if (!Number.isFinite(p) || p <= 0) return false;
    if (!isPidAlive(p)) return true;
    writeLogLine?.("warn", scope, `terminating stale pid=${p}`);
    try {
      processRef.kill(p, "SIGTERM");
    } catch (_err) {
      try {
        processRef.kill(p);
      } catch (_err2) {
        // ignore
      }
    }
    for (let i = 0; i < 12; i += 1) {
      if (!isPidAlive(p)) return true;
      await sleep(100);
    }
    try {
      processRef.kill(p, "SIGKILL");
    } catch (_err) {
      // ignore
    }
    for (let i = 0; i < 8; i += 1) {
      if (!isPidAlive(p)) return true;
      await sleep(80);
    }
    return !isPidAlive(p);
  };

  const purgeStaleSniBridgePortHolders = async (port, currentPid = 0) => {
    const keepPid = Number(currentPid || 0);
    const pids = getListeningPidsOnTcpPort(port);
    if (!pids.length) return { ok: true, killed: [], blocked: [] };
    const killed = [];
    const blocked = [];
    for (const pid of pids) {
      if (pid <= 0 || pid === processRef.pid || (keepPid > 0 && pid === keepPid)) continue;
      const cmdline = getPidCommandLine(pid);
      if (!isSekaiLinkBridgeCommand(cmdline)) {
        blocked.push({ pid, cmdline });
        continue;
      }
      const ok = await killPidGracefully(pid, "sni-bridge");
      if (ok) killed.push(pid);
      else blocked.push({ pid, cmdline });
    }
    if (blocked.length) {
      writeLogLine?.("warn", "sni-bridge", `port cleanup blocked on ${port}: ${blocked.map((b) => `${b.pid}`).join(",")}`);
    }
    if (killed.length) {
      writeLogLine?.("info", "sni-bridge", `port cleanup killed stale holders on ${port}: ${killed.join(",")}`);
    }
    return { ok: blocked.length === 0, killed, blocked };
  };

  const probeSniBridge = (pythonBin, host, port, timeoutMs = 2500) => {
    const py = String(pythonBin || "").trim();
    if (!py) return { ok: false, error: "python_missing_for_probe" };
    const script = [
      "import asyncio, json, sys",
      "import websockets",
      "async def run():",
      "  host = sys.argv[1]",
      "  port = int(sys.argv[2])",
      "  timeout = float(sys.argv[3])",
      "  uri = f'ws://{host}:{port}'",
      "  async with websockets.connect(uri, ping_interval=None, ping_timeout=None, close_timeout=1) as ws:",
      "    await ws.send(json.dumps({'Opcode':'DeviceList','Space':'SNES','Operands':[]}))",
      "    msg = await asyncio.wait_for(ws.recv(), timeout=timeout)",
      "    if isinstance(msg, (bytes, bytearray)):",
      "      raise RuntimeError('device_list_binary_reply')",
      "    data = json.loads(msg)",
      "    arr = data.get('Results') if isinstance(data, dict) else None",
      "    if not isinstance(arr, list) or not arr or arr[0] != 'SekaiLink BizHawk':",
      "      raise RuntimeError('device_list_invalid')",
      "  print('OK')",
      "asyncio.run(run())",
    ].join("\n");
    try {
      const sec = Math.max(0.4, Number(timeoutMs || 0) / 1000).toFixed(2);
      const out = spawnSync(py, ["-c", script, String(host || "127.0.0.1"), String(port || 23074), sec], {
        encoding: "utf-8",
        windowsHide: true,
        timeout: Math.max(1000, Number(timeoutMs || 0) + 1500),
        env: withApPythonEnv(processRef.env),
      });
      const stdout = String(out.stdout || "").trim();
      const stderr = String(out.stderr || "").trim();
      if (out.status === 0 && stdout.includes("OK")) return { ok: true };
      return { ok: false, error: "sni_bridge_probe_failed", detail: stderr || stdout || `status=${out.status}` };
    } catch (err) {
      return { ok: false, error: "sni_bridge_probe_exception", detail: String(err || "") };
    }
  };

  return {
    sleep,
    waitForTcpPort,
    waitForAnyTcpPort,
    findFreeLocalPort,
    findFreeLocalPortInRange,
    isPidAlive,
    waitForChildExit,
    terminateChildProcess,
    getListeningPidsOnTcpPort,
    getPidCommandLine,
    killPidGracefully,
    purgeStaleSniBridgePortHolders,
    probeSniBridge,
  };
};

module.exports = { createProcessTools };

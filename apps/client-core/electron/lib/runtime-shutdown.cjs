"use strict";

const createRuntimeShutdown = (deps = {}) => {
  const {
    writeLogLine,
    stopCommonClient,
    stopBizHawkClient,
    stopArchipelagoClient,
    stopRetroarchMemoryBridge,
    stopAllWebApClients,
    stopBizHawkClientForCoupling,
    stopSniBridge,
    sendPopTrackerRuntimeCommand,
    terminateChildProcess,
    purgeStaleSniBridgePortHolders,
    trackerProcs,
    trackerRuntimeControls,
    bizhawkProcs,
    nativeGameProcs,
    linkedWorldProcs,
    archipelagoClientProcs,
    retroarchMemoryBridgeProcs,
    sekaiemuChatBridges,
  } = deps;
  let coupledRuntimeTeardownPromise = null;

  const triggerCoupledRuntimeTeardown = (origin = "unknown", pid = null, extra = {}) => {
    if (runtimeShutdownPromise || coupledRuntimeTeardownPromise) return;
    coupledRuntimeTeardownPromise = (async () => {
      try {
        const code = extra && Object.prototype.hasOwnProperty.call(extra, "code") ? extra.code : null;
        const signal = extra && Object.prototype.hasOwnProperty.call(extra, "signal") ? extra.signal : null;
        writeLogLine(
          "warn",
          "runtime-coupling",
          `peer exited origin=${origin} pid=${pid || ""} code=${code ?? "null"} signal=${signal || "none"} -> stopping emulator/tracker/SNI`
        );
  
        for (const [otherPid, proc] of Array.from(trackerProcs.entries())) {
          if (trackerRuntimeControls.has(otherPid)) {
            sendPopTrackerRuntimeCommand(otherPid, "quit");
          }
          await terminateChildProcess(proc, "tracker", { graceMs: 900 });
          trackerProcs.delete(otherPid);
          trackerRuntimeControls.delete(otherPid);
        }
        for (const [otherPid, proc] of Array.from(bizhawkProcs.entries())) {
          await terminateChildProcess(proc, "bizhawk", { graceMs: 1100 });
          bizhawkProcs.delete(otherPid);
        }
        if (typeof stopAllWebApClients === "function") {
          await stopAllWebApClients().catch((err) => {
            writeLogLine("warn", "runtime-coupling", `web AP cleanup failed: ${String(err || "")}`);
          });
        }
        for (const clientId of Array.from(archipelagoClientProcs.keys())) {
          await stopArchipelagoClient(clientId).catch((err) => {
            writeLogLine("warn", "runtime-coupling", `AP client cleanup failed client=${clientId}: ${String(err || "")}`);
          });
        }
        for (const clientId of Array.from(retroarchMemoryBridgeProcs.keys())) {
          await stopRetroarchMemoryBridge(clientId).catch((err) => {
            writeLogLine("warn", "runtime-coupling", `memory bridge cleanup failed client=${clientId}: ${String(err || "")}`);
          });
        }
        for (const [otherPid, proc] of Array.from(nativeGameProcs.entries())) {
          if (Number(otherPid) === Number(pid)) continue;
          await terminateChildProcess(proc, "native", { graceMs: 900 }).catch((err) => {
            writeLogLine("warn", "runtime-coupling", `native cleanup failed pid=${otherPid}: ${String(err || "")}`);
          });
          nativeGameProcs.delete(otherPid);
        }
        for (const [otherPid, bridge] of Array.from(sekaiemuChatBridges.entries())) {
          try {
            bridge?.stop?.();
          } catch (_err) {}
          sekaiemuChatBridges.delete(otherPid);
        }
        if (typeof stopSniBridge === "function") {
          await stopSniBridge().catch((err) => {
            writeLogLine("warn", "runtime-coupling", `SNI bridge cleanup failed: ${String(err || "")}`);
          });
        }
        await stopBizHawkClientForCoupling();
        await purgeStaleSniBridgePortHolders(23074, 0).catch((err) => {
          writeLogLine("warn", "runtime-coupling", `stale SNI purge failed: ${String(err || "")}`);
        });
      } catch (err) {
        writeLogLine("warn", "runtime-coupling", `teardown failed: ${String(err || "")}`);
      } finally {
        coupledRuntimeTeardownPromise = null;
      }
    })();
  };

  let runtimeShutdownPromise = null;
  const shutdownRuntimeProcesses = async (reason = "shutdown") => {
    if (runtimeShutdownPromise) return runtimeShutdownPromise;
    runtimeShutdownPromise = (async () => {
      writeLogLine("info", "runtime-shutdown", `begin reason=${reason}`);
      await stopCommonClient();
      await stopBizHawkClient();
      if (typeof stopAllWebApClients === "function") {
        await stopAllWebApClients().catch((_err) => {});
      }
  	    for (const clientId of Array.from(archipelagoClientProcs.keys())) {
  	      await stopArchipelagoClient(clientId);
  	    }
  	    for (const clientId of Array.from(retroarchMemoryBridgeProcs.keys())) {
  	      await stopRetroarchMemoryBridge(clientId).catch((_err) => {});
  	    }
  
  	    for (const [pid, proc] of Array.from(trackerProcs.entries())) {
        await terminateChildProcess(proc, "tracker", { graceMs: 900 });
        trackerProcs.delete(pid);
      }
  
      for (const [pid, proc] of Array.from(bizhawkProcs.entries())) {
        await terminateChildProcess(proc, "bizhawk", { graceMs: 1100 });
        bizhawkProcs.delete(pid);
      }
  
      for (const [pid, proc] of Array.from(nativeGameProcs.entries())) {
        await terminateChildProcess(proc, "native", { graceMs: 900 });
        nativeGameProcs.delete(pid);
      }
  
      for (const [pid, session] of Array.from(linkedWorldProcs.entries())) {
        if (session?.server) await terminateChildProcess(session.server, "linkedworld-server", { graceMs: 900 });
        linkedWorldProcs.delete(pid);
      }
  
      await purgeStaleSniBridgePortHolders(23074, 0);
      writeLogLine("info", "runtime-shutdown", `complete reason=${reason}`);
    })();
    try {
      return await runtimeShutdownPromise;
    } finally {
      runtimeShutdownPromise = null;
    }
  };

  return {
    triggerCoupledRuntimeTeardown,
    shutdownRuntimeProcesses,
  };
};

module.exports = { createRuntimeShutdown };

import { afterEach, describe, expect, it } from 'vitest';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const require = createRequire(import.meta.url);
const { pruneOversizedSklmiTraceLogs } = require('../../electron/lib/runtime-log-maintenance.cjs');

const roots: string[] = [];

afterEach(() => {
  for (const root of roots.splice(0)) fs.rmSync(root, { recursive: true, force: true });
});

describe('runtime log maintenance', () => {
  it('only truncates oversized SKLMI trace logs', () => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'sekailink-trace-prune-'));
    roots.push(root);
    const bridge = path.join(root, 'zelda1', 'saves', 'seed', 'sklmi', 'zelda1-phase1');
    fs.mkdirSync(bridge, { recursive: true });
    const trace = path.join(bridge, 'trace.jsonl');
    const rotated = path.join(bridge, 'trace.jsonl.1');
    const companion = path.join(bridge, 'companion.log');
    fs.writeFileSync(trace, Buffer.alloc(2048, 1));
    fs.writeFileSync(rotated, Buffer.alloc(2048, 2));
    fs.writeFileSync(companion, Buffer.alloc(2048, 3));

    const result = pruneOversizedSklmiTraceLogs({ rootDir: root, fs, path, maxBytes: 1024 });

    expect(result.filesPruned).toBe(2);
    expect(result.bytesReclaimed).toBe(4096);
    expect(result.errors).toEqual([]);
    expect(fs.statSync(trace).size).toBe(0);
    expect(fs.statSync(rotated).size).toBe(0);
    expect(fs.statSync(companion).size).toBe(2048);
  });

  it('does not follow symbolic links outside the runtime root', () => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'sekailink-trace-prune-'));
    const outside = fs.mkdtempSync(path.join(os.tmpdir(), 'sekailink-trace-outside-'));
    roots.push(root, outside);
    const externalTrace = path.join(outside, 'trace.jsonl');
    fs.writeFileSync(externalTrace, Buffer.alloc(2048, 4));
    fs.symlinkSync(outside, path.join(root, 'linked-runtime'), 'dir');

    const result = pruneOversizedSklmiTraceLogs({ rootDir: root, fs, path, maxBytes: 1024 });

    expect(result.filesPruned).toBe(0);
    expect(fs.statSync(externalTrace).size).toBe(2048);
  });
});

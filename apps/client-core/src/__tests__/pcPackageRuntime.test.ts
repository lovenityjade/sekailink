import { createRequire } from 'node:module';
import { spawnSync as nodeSpawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { afterEach, describe, expect, it } from 'vitest';

const require = createRequire(import.meta.url);
const { createPcPackageRuntime } = require('../../electron/lib/pc-package-runtime.cjs');
const temporaryRoots: string[] = [];

const makeRuntime = (releases: Record<string, unknown>[] = [], archivePath = '') => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'sekailink-pc-packages-'));
  temporaryRoots.push(root);
  const userData = path.join(root, 'user-data');
  const publicDir = path.join(root, 'app', 'public', 'pc-packages');
  fs.mkdirSync(publicDir, { recursive: true });
  fs.writeFileSync(path.join(publicDir, 'catalog-v1.json'), JSON.stringify({
    schema: 'sekailink.pc-package-catalog/v1',
    generatedAt: null,
    packages: [{ id: 'ship-of-harkinian-sekailink', gameId: 'ship-of-harkinian', name: 'SoH', releases }],
  }));
  return createPcPackageRuntime({
    app: { getPath: () => userData, getAppPath: () => path.join(root, 'app') },
    fs,
    path,
    https: { get: (_url: string, _options: unknown, callback: (response: any) => void) => {
      const handlers: Record<string, (value?: unknown) => void> = {};
      const response = { statusCode: 503, headers: {}, resume() {}, setEncoding() {}, on(name: string, handler: () => void) { handlers[name] = handler; } };
      queueMicrotask(() => callback(response));
      return { on(name: string, handler: () => void) { handlers[name] = handler; }, destroy() {} };
    } },
    spawnSync: nodeSpawnSync,
    processRef: { pid: 1, platform: 'linux', arch: 'x64', resourcesPath: '', env: {} },
    ensureDir: (directory: string) => fs.mkdirSync(directory, { recursive: true }),
    downloadToDirWithProgress: async (_url: string, destination: string) => {
      if (!archivePath) throw new Error('not used');
      fs.mkdirSync(destination, { recursive: true });
      const downloaded = path.join(destination, path.basename(archivePath));
      fs.copyFileSync(archivePath, downloaded);
      return { path: downloaded, bytes: fs.statSync(downloaded).size };
    },
  });
};

afterEach(() => {
  for (const root of temporaryRoots.splice(0)) fs.rmSync(root, { recursive: true, force: true });
});

describe('PC package runtime', () => {
  it('falls back to the bundled catalog when the CDN is unavailable', async () => {
    const runtime = makeRuntime();
    const catalog = await runtime.getCatalog({ refresh: true });
    expect(catalog.packages).toHaveLength(1);
    expect(catalog.packages[0].id).toBe('ship-of-harkinian-sekailink');
    expect(catalog.packages[0].availableRelease).toBeNull();
    expect(catalog.packages[0].installed).toBeNull();
  });

  it('rejects unsafe package identifiers before deleting files', async () => {
    const runtime = makeRuntime();
    await expect(runtime.uninstall('../outside')).rejects.toThrow('pc_package_invalid_id');
  });

  it('installs and removes a platform-matched archive atomically', async () => {
    const fixture = fs.mkdtempSync(path.join(os.tmpdir(), 'sekailink-pc-archive-'));
    temporaryRoots.push(fixture);
    const payload = path.join(fixture, 'payload');
    const archive = path.join(fixture, 'soh.tar.gz');
    fs.mkdirSync(payload);
    fs.writeFileSync(path.join(payload, 'game.appimage'), 'ELF fixture');
    expect(nodeSpawnSync('tar', ['-czf', archive, '-C', payload, 'game.appimage']).status).toBe(0);

    const runtime = makeRuntime([{
      version: 'test.1',
      channel: 'canary',
      platform: 'linux-x64',
      url: 'https://sekailink.invalid/soh.tar.gz',
      bytes: fs.statSync(archive).size,
      sha256: 'a'.repeat(64),
      executable: 'game.appimage',
    }], archive);

    const installed = await runtime.install('ship-of-harkinian-sekailink');
    expect(installed.ok).toBe(true);
    expect(runtime.resolveInstalled('ship-of-harkinian-sekailink')?.executable).toMatch(/game\.appimage$/);
    await runtime.uninstall('ship-of-harkinian-sekailink');
    expect(runtime.resolveInstalled('ship-of-harkinian-sekailink')).toBeNull();
  });
});

import { beforeEach, describe, expect, it, vi } from 'vitest';

const { listRuntimeModules, validateRomForModule } = vi.hoisted(() => ({
  listRuntimeModules: vi.fn(),
  validateRomForModule: vi.fn(),
}));

vi.mock('../services/runtime', () => ({
  runtime: {
    listRuntimeModules,
    validateRomForModule,
  },
}));

import { checkRomReadiness } from '../services/romReadiness';

describe('ROM readiness', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    listRuntimeModules.mockResolvedValue({
      ok: true,
      modules: [{
        moduleId: 'alttp',
        manifest: {
          game_id: 'alttp',
          ap_world: 'A Link to the Past',
          display_name: 'A Link to the Past',
          required_roms: ['alttp'],
        },
      }],
    });
  });

  it('blocks lobby addition when the required ROM is missing', async () => {
    validateRomForModule.mockResolvedValue({
      ok: false,
      error: 'rom_missing',
      gameId: 'alttp',
      setupArea: 'roms',
    });

    const result = await checkRomReadiness('A Link to the Past');

    expect(result.state).toBe('missing');
    expect(result.blocksLobbyAdd).toBe(true);
    expect(result.importGameId).toBe('a_link_to_the_past');
  });

  it('allows lobby addition when the ROM checksum is verified', async () => {
    validateRomForModule.mockResolvedValue({ ok: true, required: true, setupArea: 'roms' });

    const result = await checkRomReadiness('A Link to the Past');

    expect(result.state).toBe('verified');
    expect(result.blocksLobbyAdd).toBe(false);
    expect(result.message).toBe('ROM found and verified.');
  });
});

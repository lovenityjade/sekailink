import { createServer } from 'node:http'
import { readFile, stat, mkdir, appendFile, writeFile } from 'node:fs/promises'
import { createReadStream, existsSync } from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import crypto from 'node:crypto'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const appRoot = path.resolve(__dirname, '..')
const distDir = process.env.SEKAILINK_ADMIN_DIST_DIR || path.join(appRoot, 'dist')
const dataDir = process.env.SEKAILINK_ADMIN_DATA_DIR || path.join(appRoot, 'data')

const config = {
  host: process.env.SEKAILINK_ADMIN_HOST || '127.0.0.1',
  port: Number(process.env.SEKAILINK_ADMIN_PORT || 19110),
  publicBaseUrl: process.env.SEKAILINK_ADMIN_PUBLIC_BASE_URL || 'https://admin.sekailink.com',
  webToken: process.env.SEKAILINK_ADMIN_WEB_TOKEN || process.env.SEKAILINK_ADMIN_TOKEN || '',
  allowNoToken: process.env.SEKAILINK_ADMIN_ALLOW_NO_TOKEN === '1',
  accessApprovalToken: process.env.SEKAILINK_ADMIN_ACCESS_APPROVAL_TOKEN || process.env.SEKAILINK_ADMIN_WEB_TOKEN || process.env.SEKAILINK_ADMIN_TOKEN || '',
  accessTotpSecret: process.env.SEKAILINK_ADMIN_ACCESS_TOTP_SECRET || '',
  sessionTtlMs: Number(process.env.SEKAILINK_ADMIN_SESSION_TTL_SECONDS || 8 * 60 * 60) * 1000,
  identityBase: process.env.SEKAILINK_IDENTITY_BASE || 'http://127.0.0.1:19095',
  identityToken: process.env.SEKAILINK_IDENTITY_ADMIN_TOKEN || process.env.SEKAILINK_INTERNAL_ADMIN_TOKEN || '',
  lobbyBase: process.env.SEKAILINK_LOBBY_ADMIN_BASE || 'http://127.0.0.1:19096',
  lobbyToken: process.env.SEKAILINK_LOBBY_ADMIN_TOKEN || process.env.SEKAILINK_INTERNAL_ADMIN_TOKEN || '',
  chatBase: process.env.SEKAILINK_CHAT_GATEWAY_BASE || 'http://127.0.0.1:19098',
  chatToken: process.env.SEKAILINK_CHAT_GATEWAY_TOKEN || process.env.SEKAILINK_INTERNAL_ADMIN_TOKEN || '',
  agentBase: process.env.SEKAILINK_ADMIN_AGENT_BASE || 'http://127.0.0.1:19091',
  agentToken: process.env.SEKAILINK_ADMIN_AGENT_TOKEN || process.env.SEKAILINK_INTERNAL_ADMIN_TOKEN || '',
  roomBase: process.env.SEKAILINK_ROOM_SERVER_BASE || 'http://127.0.0.1:19097',
  roomToken: process.env.SEKAILINK_ROOM_SERVER_ADMIN_TOKEN || process.env.SEKAILINK_INTERNAL_ADMIN_TOKEN || '',
  clientReportToken: process.env.SEKAILINK_USER_REPORT_TOKEN || '',
  emptyFallbacks: process.env.SEKAILINK_ADMIN_EMPTY_FALLBACKS !== '0',
}

const stores = {
  audit: path.join(dataDir, 'admin-audit.jsonl'),
  userReports: path.join(dataDir, 'user-reports.jsonl'),
  clientReports: path.join(dataDir, 'client-reports.jsonl'),
}

const pendingAccessRequests = new Map()
const adminSessions = new Map()

const contentTypes = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.ico': 'image/x-icon',
  '.json': 'application/json; charset=utf-8',
  '.txt': 'text/plain; charset=utf-8',
}

function nowIso() {
  return new Date().toISOString()
}

function requestId() {
  return crypto.randomBytes(12).toString('hex')
}

function sendJson(res, status, body, headers = {}) {
  const payload = JSON.stringify(body)
  res.writeHead(status, {
    'content-type': 'application/json; charset=utf-8',
    'content-length': Buffer.byteLength(payload),
    'cache-control': 'no-store',
    'x-content-type-options': 'nosniff',
    ...headers,
  })
  res.end(payload)
}

function sendText(res, status, body) {
  res.writeHead(status, {
    'content-type': 'text/plain; charset=utf-8',
    'content-length': Buffer.byteLength(body),
    'cache-control': 'no-store',
    'x-content-type-options': 'nosniff',
  })
  res.end(body)
}

function sendHtml(res, status, body, headers = {}) {
  res.writeHead(status, {
    ...securityHeaders(),
    'content-type': 'text/html; charset=utf-8',
    'content-length': Buffer.byteLength(body),
    'cache-control': 'no-store',
    ...headers,
  })
  res.end(body)
}

function securityHeaders() {
  return {
    'x-content-type-options': 'nosniff',
    'x-frame-options': 'DENY',
    'referrer-policy': 'no-referrer',
    'permissions-policy': 'camera=(), microphone=(), geolocation=()',
    'content-security-policy':
      "default-src 'self'; connect-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self'; base-uri 'self'; frame-ancestors 'none'",
  }
}

function bearer(req) {
  const value = req.headers.authorization || ''
  const prefix = 'Bearer '
  return value.startsWith(prefix) ? value.slice(prefix.length).trim() : ''
}

function safeEqual(a, b) {
  const left = Buffer.from(String(a || ''), 'utf8')
  const right = Buffer.from(String(b || ''), 'utf8')
  return left.length === right.length && crypto.timingSafeEqual(left, right)
}

function base32Decode(value) {
  const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567'
  const input = String(value || '').replace(/[\s=-]/g, '').toUpperCase()
  let bits = ''
  const bytes = []
  for (const char of input) {
    const index = alphabet.indexOf(char)
    if (index < 0) continue
    bits += index.toString(2).padStart(5, '0')
    while (bits.length >= 8) {
      bytes.push(parseInt(bits.slice(0, 8), 2))
      bits = bits.slice(8)
    }
  }
  return Buffer.from(bytes)
}

function totpCode(secret, counter) {
  const key = base32Decode(secret)
  if (!key.length) return ''
  const buffer = Buffer.alloc(8)
  buffer.writeBigUInt64BE(BigInt(counter))
  const hmac = crypto.createHmac('sha1', key).update(buffer).digest()
  const offset = hmac[hmac.length - 1] & 0x0f
  const value = ((hmac[offset] & 0x7f) << 24)
    | ((hmac[offset + 1] & 0xff) << 16)
    | ((hmac[offset + 2] & 0xff) << 8)
    | (hmac[offset + 3] & 0xff)
  return String(value % 1000000).padStart(6, '0')
}

function verifyTotp(secret, code) {
  const normalized = String(code || '').replace(/\s+/g, '')
  if (!secret || !/^\d{6}$/.test(normalized)) return false
  const counter = Math.floor(Date.now() / 30000)
  for (let offset = -1; offset <= 1; offset += 1) {
    if (safeEqual(totpCode(secret, counter + offset), normalized)) return true
  }
  return false
}

function cookies(req) {
  const out = {}
  const raw = req.headers.cookie || ''
  for (const part of raw.split(';')) {
    const index = part.indexOf('=')
    if (index <= 0) continue
    out[part.slice(0, index).trim()] = decodeURIComponent(part.slice(index + 1).trim())
  }
  return out
}

function setAdminSessionCookie(res, token) {
  const secure = config.publicBaseUrl.startsWith('https://') ? '; Secure' : ''
  res.setHeader('Set-Cookie', `sekailink_admin_session=${encodeURIComponent(token)}; HttpOnly; SameSite=Strict; Path=/; Max-Age=${Math.floor(config.sessionTtlMs / 1000)}${secure}`)
}

function clearAdminSessionCookie(res) {
  res.setHeader('Set-Cookie', 'sekailink_admin_session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0')
}

function hashSecret(value) {
  return crypto.createHash('sha256').update(String(value || '')).digest('hex')
}

function currentAdminSession(req) {
  const token = cookies(req).sekailink_admin_session
  if (!token) return null
  const session = adminSessions.get(hashSecret(token))
  if (!session) return null
  if (Date.now() > session.expiresAt) {
    adminSessions.delete(hashSecret(token))
    return null
  }
  return session
}

function compactAccessState() {
  const now = Date.now()
  for (const [id, request] of pendingAccessRequests) {
    if (now > request.expiresAt) pendingAccessRequests.delete(id)
  }
  for (const [key, session] of adminSessions) {
    if (now > session.expiresAt) adminSessions.delete(key)
  }
}

function accessRequestHtml() {
  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>SekaiLink Core Access</title>
  <style>
    :root { color-scheme: dark; font-family: Inter, system-ui, sans-serif; background: #050b0d; color: #f6fbfb; }
    body { min-height: 100vh; margin: 0; display: grid; place-items: center; background: radial-gradient(circle at 20% 10%, #173f3b 0, transparent 32rem), #050b0d; }
    main { width: min(560px, calc(100vw - 32px)); border: 1px solid #1f3b3e; border-radius: 12px; padding: 28px; background: rgba(9, 18, 21, .94); box-shadow: 0 24px 80px rgba(0,0,0,.45); }
    h1 { margin: 0 0 8px; font-size: 28px; }
    p { color: #9fb7ba; line-height: 1.5; }
    label { display: grid; gap: 8px; margin: 20px 0 12px; color: #9fb7ba; font-weight: 700; }
    input { border: 1px solid #24494d; background: #071113; color: white; border-radius: 8px; padding: 13px 14px; font: inherit; }
    button { border: 0; border-radius: 8px; padding: 13px 16px; font-weight: 800; color: #071113; background: linear-gradient(135deg, #ff7358, #61ddca); cursor: pointer; }
    code, .status { display: block; margin-top: 16px; padding: 12px; border-radius: 8px; background: #071113; color: #8df5e4; word-break: break-all; }
    .danger { color: #ff9388; }
  </style>
</head>
<body>
  <main>
    <strong style="color:#61ddca">SEKAILINK CORE ACCESS</strong>
    <h1>Admin access approval required</h1>
    <p>This panel is locked. Create an access request, then approve it from the trusted admin device or server console.</p>
    <form id="request-form">
      <label>Device label <input id="device" autocomplete="off" placeholder="Jade workstation" /></label>
      <button type="submit">Request Access</button>
    </form>
    <div id="status" class="status">No request yet.</div>
  </main>
  <script nonce="sekailink-access">
    const form = document.getElementById('request-form');
    const statusBox = document.getElementById('status');
    let requestId = '';
    let claimToken = '';
    let timer = null;
    async function poll() {
      if (!requestId || !claimToken) return;
      const res = await fetch('/api/admin/access/status?id=' + encodeURIComponent(requestId) + '&claim=' + encodeURIComponent(claimToken), { cache: 'no-store' });
      const data = await res.json();
      if (data.approved) {
        statusBox.textContent = 'Approved. Opening cockpit...';
        window.clearInterval(timer);
        window.location.reload();
      } else if (data.rejected || data.expired) {
        statusBox.innerHTML = '<span class="danger">' + (data.rejected ? 'Rejected.' : 'Request expired.') + '</span>';
        window.clearInterval(timer);
      } else {
        statusBox.textContent = 'Waiting for approval: ' + requestId;
      }
    }
    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      const res = await fetch('/api/admin/access/request', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ device_label: document.getElementById('device').value || 'Admin browser' })
      });
      const data = await res.json();
      requestId = data.request_id;
      claimToken = data.claim_token;
      statusBox.innerHTML = 'Request created. Approve this id:<code>' + requestId + '</code>';
      window.clearInterval(timer);
      timer = window.setInterval(poll, 2500);
      poll();
    });
  </script>
</body>
</html>`
}

function isLoopback(req) {
  const address = req.socket.remoteAddress || ''
  return address === '127.0.0.1' || address === '::1' || address === '::ffff:127.0.0.1'
}

function requireAdmin(req, res) {
  const session = currentAdminSession(req)
  if (session) {
    return { ok: true, actor: session.actor || 'approved-admin' }
  }
  if (config.allowNoToken && isLoopback(req)) {
    return { ok: true, actor: 'local-dev' }
  }
  if (!config.webToken) {
    sendJson(res, 503, {
      ok: false,
      error: 'admin_gateway_token_not_configured',
      detail: 'Set SEKAILINK_ADMIN_WEB_TOKEN before exposing admin.sekailink.com.',
    })
    return { ok: false }
  }
  const token = bearer(req)
  if (!token || !safeEqual(token, config.webToken)) {
    sendJson(res, 401, { ok: false, error: 'unauthorized' })
    return { ok: false }
  }
  return { ok: true, actor: 'admin-web' }
}

async function readBody(req, maxBytes = 1024 * 1024) {
  const chunks = []
  let size = 0
  for await (const chunk of req) {
    size += chunk.length
    if (size > maxBytes) throw new Error('payload_too_large')
    chunks.push(chunk)
  }
  const text = Buffer.concat(chunks).toString('utf8')
  if (!text) return {}
  return JSON.parse(text)
}

async function ensureDataDir() {
  await mkdir(dataDir, { recursive: true })
}

async function appendJsonl(file, value) {
  await ensureDataDir()
  await appendFile(file, `${JSON.stringify(value)}\n`, 'utf8')
}

async function readJsonl(file, limit = 200) {
  if (!existsSync(file)) return []
  const text = await readFile(file, 'utf8')
  const lines = text.split(/\r?\n/).filter(Boolean)
  return lines.slice(-limit).map((line) => {
    try {
      return JSON.parse(line)
    } catch {
      return { malformed: true, raw: line }
    }
  }).reverse()
}

async function rewriteJsonl(file, records) {
  await ensureDataDir()
  await writeFile(file, records.map((record) => JSON.stringify(record)).join('\n') + (records.length ? '\n' : ''), 'utf8')
}

async function audit(req, eventType, payload) {
  const session = currentAdminSession(req)
  await appendJsonl(stores.audit, {
    id: requestId(),
    ts: nowIso(),
    event_type: eventType,
    actor: session?.actor || (bearer(req) ? 'admin-token' : 'local-dev'),
    ip: req.socket.remoteAddress || '',
    user_agent: req.headers['user-agent'] || '',
    payload,
  })
}

function requireApprovalToken(req, res) {
  if (config.allowNoToken && isLoopback(req)) return { ok: true, actor: 'local-dev' }
  if (!config.accessApprovalToken) {
    sendJson(res, 503, { ok: false, error: 'access_approval_token_not_configured' })
    return { ok: false }
  }
  const token = bearer(req)
  if (!token || !safeEqual(token, config.accessApprovalToken)) {
    sendJson(res, 401, { ok: false, error: 'unauthorized' })
    return { ok: false }
  }
  if (config.accessTotpSecret && !verifyTotp(config.accessTotpSecret, req.headers['x-sekailink-admin-otp'])) {
    sendJson(res, 401, { ok: false, error: 'invalid_totp' })
    return { ok: false }
  }
  return { ok: true, actor: 'approval-device' }
}

function upstreamToken(kind) {
  if (kind === 'identity') return config.identityToken || config.webToken
  if (kind === 'lobby') return config.lobbyToken || config.webToken
  if (kind === 'chat') return config.chatToken || config.webToken
  if (kind === 'agent') return config.agentToken || config.webToken
  if (kind === 'room') return config.roomToken || config.webToken
  return config.webToken
}

function upstreamBase(kind) {
  if (kind === 'identity') return config.identityBase
  if (kind === 'lobby') return config.lobbyBase
  if (kind === 'chat') return config.chatBase
  if (kind === 'agent') return config.agentBase
  if (kind === 'room') return config.roomBase
  throw new Error(`unknown_upstream:${kind}`)
}

async function callUpstream(kind, method, route, body) {
  try {
    const base = upstreamBase(kind).replace(/\/+$/, '')
    const url = `${base}${route.startsWith('/') ? route : `/${route}`}`
    const token = upstreamToken(kind)
    const response = await fetch(url, {
      method,
      headers: {
        Accept: 'application/json',
        ...(body === undefined ? {} : { 'Content-Type': 'application/json' }),
        ...(token ? { Authorization: `Bearer ${token}` } : {}),
      },
      body: body === undefined ? undefined : JSON.stringify(body),
    })
    const text = await response.text()
    let parsed = {}
    try {
      parsed = text ? JSON.parse(text) : {}
    } catch {
      parsed = { ok: false, error: 'upstream_non_json', body: text }
    }
    if (!response.ok || parsed?.ok === false) {
      return {
        ok: false,
        status: response.status,
        error: parsed?.error || response.statusText,
        upstream: kind,
        body: parsed,
        url,
      }
    }
    return { ok: true, status: response.status, body: parsed, upstream: kind, url }
  } catch (error) {
    return {
      ok: false,
      status: 502,
      error: error instanceof Error ? error.message : String(error),
      upstream: kind,
      url: `${upstreamBase(kind).replace(/\/+$/, '')}${route.startsWith('/') ? route : `/${route}`}`,
    }
  }
}

async function firstUpstream(calls) {
  const errors = []
  for (const call of calls) {
    try {
      const result = await call()
      if (result.ok) return result
      errors.push(result)
    } catch (error) {
      errors.push({ ok: false, error: error instanceof Error ? error.message : String(error) })
    }
  }
  return { ok: false, status: 502, error: 'all_upstreams_failed', errors }
}

function emptyFallback(kind, payload, error) {
  if (!config.emptyFallbacks) return null
  return {
    ok: true,
    degraded: true,
    upstream: kind,
    warning: error?.error || error?.message || 'upstream_unavailable',
    ...payload,
  }
}

async function upstreamProbe(kind, route) {
  const started = Date.now()
  const result = await callUpstream(kind, 'GET', route, undefined)
  return {
    kind,
    ok: result.ok,
    status: result.status,
    base: upstreamBase(kind),
    route,
    latency_ms: Date.now() - started,
    error: result.ok ? undefined : result.error,
  }
}

function routeParts(url) {
  return url.pathname.split('/').filter(Boolean).map(decodeURIComponent)
}

async function handleUserReports(req, res, url, parts) {
  if (req.method === 'GET' && parts.length === 3) {
    const limit = Math.min(500, Math.max(1, Number(url.searchParams.get('limit') || 100)))
    const records = await readJsonl(stores.userReports, limit)
    sendJson(res, 200, { ok: true, user_reports: records })
    return true
  }

  if (req.method === 'POST' && parts.length === 3) {
    const body = await readBody(req)
    const id = body.report_id || `ur-${Date.now()}-${requestId().slice(0, 8)}`
    const record = {
      id,
      report_id: id,
      status: 'open',
      severity: String(body.severity || 'normal').slice(0, 32),
      category: String(body.category || 'conduct').slice(0, 48),
      reporter_user_id: String(body.reporter_user_id || body.reporter || '').slice(0, 120),
      reporter_name: String(body.reporter_name || '').slice(0, 120),
      target_user_id: String(body.target_user_id || body.target || '').slice(0, 120),
      target_username: String(body.target_username || '').slice(0, 120),
      reason: String(body.reason || body.message || '').slice(0, 4000),
      evidence: String(body.evidence || '').slice(0, 12000),
      channel_id: body.channel_id || null,
      room_id: body.room_id || null,
      lobby_id: body.lobby_id || null,
      created_at: nowIso(),
      updated_at: nowIso(),
    }
    if (!record.target_user_id && !record.target_username) {
      sendJson(res, 400, { ok: false, error: 'missing_target' })
      return true
    }
    if (!record.reason) {
      sendJson(res, 400, { ok: false, error: 'missing_reason' })
      return true
    }
    await appendJsonl(stores.userReports, record)
    await audit(req, 'user_report_created', { report_id: id, target_user_id: record.target_user_id, target_username: record.target_username })
    sendJson(res, 200, { ok: true, report: record })
    return true
  }

  if (req.method === 'POST' && parts.length === 5 && parts[4] === 'actions') {
    const reportId = parts[3]
    const body = await readBody(req)
    const action = String(body.action || '').trim()
    const reason = String(body.reason || '').trim()
    if (!['review', 'resolve', 'dismiss', 'reopen'].includes(action)) {
      sendJson(res, 400, { ok: false, error: 'invalid_action' })
      return true
    }
    if (reason.length < 3) {
      sendJson(res, 400, { ok: false, error: 'missing_reason' })
      return true
    }
    const records = (await readJsonl(stores.userReports, 10000)).reverse()
    const index = records.findIndex((record) => String(record.report_id || record.id) === reportId)
    if (index < 0) {
      sendJson(res, 404, { ok: false, error: 'report_not_found' })
      return true
    }
    records[index] = {
      ...records[index],
      status: action === 'review' ? 'reviewing' : action === 'resolve' ? 'resolved' : action === 'dismiss' ? 'dismissed' : 'open',
      updated_at: nowIso(),
      last_action: action,
      last_reason: reason,
    }
    await rewriteJsonl(stores.userReports, records)
    await audit(req, 'user_report_action', { report_id: reportId, action, reason })
    sendJson(res, 200, { ok: true, report: records[index] })
    return true
  }

  return false
}

async function handleClientUserReport(req, res) {
  if (config.clientReportToken) {
    const token = bearer(req)
    if (token !== config.clientReportToken) {
      sendJson(res, 401, { ok: false, error: 'unauthorized' })
      return
    }
  }
  const body = await readBody(req)
  const id = body.report_id || `ur-${Date.now()}-${requestId().slice(0, 8)}`
  const record = {
    id,
    report_id: id,
    status: 'open',
    severity: String(body.severity || 'normal').slice(0, 32),
    category: String(body.category || 'conduct').slice(0, 48),
    reporter_user_id: String(body.reporter_user_id || body.reporter || '').slice(0, 120),
    reporter_name: String(body.reporter_name || '').slice(0, 120),
    target_user_id: String(body.target_user_id || body.target || '').slice(0, 120),
    target_username: String(body.target_username || '').slice(0, 120),
    reason: String(body.reason || body.message || '').slice(0, 4000),
    evidence: String(body.evidence || '').slice(0, 12000),
    channel_id: body.channel_id || null,
    room_id: body.room_id || null,
    lobby_id: body.lobby_id || null,
    created_at: nowIso(),
    updated_at: nowIso(),
  }
  if (!record.target_user_id && !record.target_username) {
    sendJson(res, 400, { ok: false, error: 'missing_target' })
    return
  }
  if (!record.reason) {
    sendJson(res, 400, { ok: false, error: 'missing_reason' })
    return
  }
  await appendJsonl(stores.userReports, record)
  sendJson(res, 200, { ok: true, report_id: id })
}

async function handleAccessApi(req, res, url) {
  compactAccessState()
  const parts = routeParts(url)

  if (req.method === 'POST' && parts.join('/') === 'api/admin/access/request') {
    const body = await readBody(req, 64 * 1024)
    const id = `ar-${Date.now()}-${requestId().slice(0, 8)}`
    const claimToken = crypto.randomBytes(32).toString('base64url')
    const record = {
      id,
      status: 'pending',
      claimHash: hashSecret(claimToken),
      device_label: String(body.device_label || 'Admin browser').slice(0, 160),
      ip: req.headers['x-forwarded-for'] || req.socket.remoteAddress || '',
      user_agent: req.headers['user-agent'] || '',
      created_at: nowIso(),
      expiresAt: Date.now() + 10 * 60 * 1000,
      approved_at: null,
      rejected_at: null,
    }
    pendingAccessRequests.set(id, record)
    await audit(req, 'admin_access_requested', {
      request_id: id,
      device_label: record.device_label,
      ip: record.ip,
      user_agent: record.user_agent,
    })
    sendJson(res, 200, {
      ok: true,
      request_id: id,
      claim_token: claimToken,
      expires_at: new Date(record.expiresAt).toISOString(),
    })
    return true
  }

  if (req.method === 'GET' && parts.join('/') === 'api/admin/access/status') {
    const id = String(url.searchParams.get('id') || '')
    const claim = String(url.searchParams.get('claim') || '')
    const record = pendingAccessRequests.get(id)
    if (!record || Date.now() > record.expiresAt) {
      sendJson(res, 200, { ok: true, expired: true })
      return true
    }
    if (!safeEqual(record.claimHash, hashSecret(claim))) {
      sendJson(res, 401, { ok: false, error: 'invalid_claim' })
      return true
    }
    if (record.status === 'approved') {
      const sessionToken = crypto.randomBytes(32).toString('base64url')
      adminSessions.set(hashSecret(sessionToken), {
        actor: record.device_label || 'approved-admin',
        request_id: id,
        created_at: nowIso(),
        expiresAt: Date.now() + config.sessionTtlMs,
      })
      pendingAccessRequests.delete(id)
      setAdminSessionCookie(res, sessionToken)
      sendJson(res, 200, { ok: true, approved: true })
      return true
    }
    sendJson(res, 200, {
      ok: true,
      pending: record.status === 'pending',
      rejected: record.status === 'rejected',
      expires_at: new Date(record.expiresAt).toISOString(),
    })
    return true
  }

  if (req.method === 'GET' && parts.join('/') === 'api/admin/access/requests') {
    const auth = requireApprovalToken(req, res)
    if (!auth.ok) return true
    const requests = Array.from(pendingAccessRequests.values())
      .map(({ claimHash, expiresAt, ...record }) => ({ ...record, expires_at: new Date(expiresAt).toISOString() }))
      .sort((a, b) => String(b.created_at).localeCompare(String(a.created_at)))
    sendJson(res, 200, { ok: true, requests })
    return true
  }

  if (req.method === 'POST' && parts[0] === 'api' && parts[1] === 'admin' && parts[2] === 'access' && parts[3] === 'requests' && parts[4] && parts[5]) {
    const auth = requireApprovalToken(req, res)
    if (!auth.ok) return true
    const id = parts[4]
    const action = parts[5]
    const record = pendingAccessRequests.get(id)
    if (!record) {
      sendJson(res, 404, { ok: false, error: 'request_not_found' })
      return true
    }
    if (action === 'approve') {
      record.status = 'approved'
      record.approved_at = nowIso()
      await audit(req, 'admin_access_approved', { request_id: id, actor: auth.actor, device_label: record.device_label })
      sendJson(res, 200, { ok: true, request: { id, status: record.status, approved_at: record.approved_at } })
      return true
    }
    if (action === 'reject') {
      record.status = 'rejected'
      record.rejected_at = nowIso()
      await audit(req, 'admin_access_rejected', { request_id: id, actor: auth.actor, device_label: record.device_label })
      sendJson(res, 200, { ok: true, request: { id, status: record.status, rejected_at: record.rejected_at } })
      return true
    }
    sendJson(res, 400, { ok: false, error: 'invalid_action' })
    return true
  }

  if (req.method === 'POST' && parts.join('/') === 'api/admin/access/logout') {
    const token = cookies(req).sekailink_admin_session
    if (token) adminSessions.delete(hashSecret(token))
    clearAdminSessionCookie(res)
    sendJson(res, 200, { ok: true })
    return true
  }

  return false
}

async function handleAdminApi(req, res, url) {
  if (await handleAccessApi(req, res, url)) return
  const auth = requireAdmin(req, res)
  if (!auth.ok) return

  const parts = routeParts(url)

  if (req.method === 'GET' && parts.join('/') === 'api/admin/me') {
    sendJson(res, 200, {
      ok: true,
      user: { username: auth.actor, role: 'admin', permissions: ['admin.read', 'admin.write'] },
      gateway: { public_base_url: config.publicBaseUrl, token_configured: Boolean(config.webToken) },
    })
    return
  }

  if (parts[2] === 'user-reports' || (parts[2] === 'moderation' && parts[3] === 'reports')) {
    const normalized = parts[2] === 'moderation' ? ['api', 'admin', 'user-reports', ...parts.slice(4)] : parts
    if (await handleUserReports(req, res, url, normalized)) return
  }

  if (req.method === 'GET' && parts.join('/') === 'api/admin/audit') {
    const limit = Math.min(500, Math.max(1, Number(url.searchParams.get('limit') || 200)))
    sendJson(res, 200, { ok: true, audit: await readJsonl(stores.audit, limit) })
    return
  }

  if (req.method === 'GET' && parts.join('/') === 'api/admin/upstreams') {
    const probes = await Promise.all([
      upstreamProbe('identity', '/admin/users?limit=1'),
      upstreamProbe('lobby', '/admin/lobbies?limit=1'),
      upstreamProbe('chat', '/channels'),
      upstreamProbe('agent', '/services'),
      upstreamProbe('room', '/rooms'),
    ])
    sendJson(res, 200, { ok: true, upstreams: probes })
    return
  }

  if (req.method === 'GET' && parts.join('/') === 'api/admin/reports') {
    const localReports = await readJsonl(stores.clientReports, 100)
    sendJson(res, 200, { ok: true, reports: localReports })
    return
  }

  if (req.method === 'GET' && parts.join('/') === 'api/admin/users') {
    const query = url.search || '?limit=250'
    const result = await firstUpstream([
      () => callUpstream('identity', 'GET', `/admin/users${query}`, undefined),
    ])
    const fallback = emptyFallback('identity', { users: [] }, result)
    sendJson(res, result.ok || fallback ? 200 : result.status || 502, result.ok ? result.body : fallback || result)
    return
  }

  if (parts[2] === 'users' && parts[3]) {
    const userId = encodeURIComponent(parts[3])
    if (req.method === 'GET' && parts.length === 4) {
      const result = await callUpstream('identity', 'GET', `/admin/users/${userId}`, undefined)
      sendJson(res, result.ok ? 200 : result.status || 502, result.ok ? result.body : result)
      return
    }
    if (req.method === 'POST' && parts.length === 5 && parts[4] === 'actions') {
      const body = await readBody(req)
      const action = String(body.action || '')
      let result
      if (action === 'disable') {
        result = await callUpstream('identity', 'DELETE', `/admin/users/${userId}`, undefined)
      } else if (action === 'force-password-reset') {
        result = await callUpstream('identity', 'POST', `/admin/users/${userId}/force-password-reset`, undefined)
      } else {
        result = { ok: false, status: 400, error: 'invalid_action' }
      }
      await audit(req, 'admin_user_action', { user_id: parts[3], action, reason: body.reason || '' })
      sendJson(res, result.ok ? 200 : result.status || 502, result.ok ? result.body : result)
      return
    }
  }

  if (req.method === 'GET' && parts.join('/') === 'api/admin/lobbies') {
    const query = url.search || '?limit=250'
    const result = await callUpstream('lobby', 'GET', `/admin/lobbies${query}`, undefined)
    const fallback = emptyFallback('lobby', { lobbies: [] }, result)
    sendJson(res, result.ok || fallback ? 200 : result.status || 502, result.ok ? result.body : fallback || result)
    return
  }

  if (parts[2] === 'lobbies' && parts[3]) {
    const lobbyId = encodeURIComponent(parts[3])
    if (req.method === 'GET' && parts.length === 4) {
      const result = await callUpstream('lobby', 'GET', `/admin/lobbies/${lobbyId}`, undefined)
      sendJson(res, result.ok ? 200 : result.status || 502, result.ok ? result.body : result)
      return
    }
    if (req.method === 'POST' && parts.length === 5 && parts[4] === 'actions') {
      const body = await readBody(req)
      const action = String(body.action || '')
      const result = action === 'close'
        ? await callUpstream('lobby', 'POST', `/admin/lobbies/${lobbyId}/close`, undefined)
        : { ok: false, status: 400, error: 'invalid_action' }
      await audit(req, 'admin_lobby_action', { lobby_id: parts[3], action, reason: body.reason || '' })
      sendJson(res, result.ok ? 200 : result.status || 502, result.ok ? result.body : result)
      return
    }
  }

  if (req.method === 'GET' && parts.join('/') === 'api/admin/channels') {
    const result = await callUpstream('chat', 'GET', '/channels', undefined)
    const fallback = emptyFallback('chat', { channels: [] }, result)
    sendJson(res, result.ok || fallback ? 200 : result.status || 502, result.ok ? result.body : fallback || result)
    return
  }

  if (parts[2] === 'channels' && parts[3] && parts[4]) {
    const channelId = encodeURIComponent(parts[3])
    if (req.method === 'GET' && parts[4] === 'messages') {
      const result = await callUpstream('chat', 'GET', `/channels/${channelId}/messages`, undefined)
      sendJson(res, result.ok ? 200 : result.status || 502, result.ok ? result.body : result)
      return
    }
    if (req.method === 'GET' && parts[4] === 'presence') {
      const result = await callUpstream('chat', 'GET', `/channels/${channelId}/presence`, undefined)
      sendJson(res, result.ok ? 200 : result.status || 502, result.ok ? result.body : result)
      return
    }
  }

  if (req.method === 'GET' && parts.join('/') === 'api/admin/services') {
    const result = await callUpstream('agent', 'GET', '/services', undefined)
    const fallback = emptyFallback('agent', { services: [] }, result)
    sendJson(res, result.ok || fallback ? 200 : result.status || 502, result.ok ? result.body : fallback || result)
    return
  }

  if (req.method === 'GET' && parts.join('/') === 'api/admin/system') {
    const result = await callUpstream('agent', 'GET', '/system', undefined)
    const fallback = emptyFallback('agent', { system: { hostname: 'admin-gateway', uptime_seconds: process.uptime() } }, result)
    sendJson(res, result.ok || fallback ? 200 : result.status || 502, result.ok ? result.body : fallback || result)
    return
  }

  if (parts[2] === 'services' && parts[3]) {
    const serviceName = encodeURIComponent(parts[3])
    if (req.method === 'GET' && parts[4] === 'logs') {
      const result = await callUpstream('agent', 'GET', `/services/${serviceName}/logs`, undefined)
      sendJson(res, result.ok ? 200 : result.status || 502, result.ok ? result.body : result)
      return
    }
    if (req.method === 'POST' && parts[4] === 'actions') {
      const body = await readBody(req)
      const action = String(body.action || '')
      if (!['start', 'stop', 'restart'].includes(action)) {
        sendJson(res, 400, { ok: false, error: 'invalid_action' })
        return
      }
      const result = await callUpstream('agent', 'POST', `/services/${serviceName}/${action}`, undefined)
      await audit(req, 'admin_service_action', { service: parts[3], action, reason: body.reason || '' })
      sendJson(res, result.ok ? 200 : result.status || 502, result.ok ? result.body : result)
      return
    }
  }

  if (req.method === 'GET' && parts.join('/') === 'api/admin/rooms') {
    const result = await callUpstream('room', 'GET', '/rooms', undefined)
    const fallback = emptyFallback('room', { rooms: [] }, result)
    sendJson(res, result.ok || fallback ? 200 : result.status || 502, result.ok ? result.body : fallback || result)
    return
  }

  sendJson(res, 404, { ok: false, error: 'admin_route_not_found', path: url.pathname })
}

async function serveStatic(req, res, url) {
  if (!currentAdminSession(req) && !(config.allowNoToken && isLoopback(req))) {
    sendHtml(res, 200, accessRequestHtml(), {
      'content-security-policy':
        "default-src 'self'; connect-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self' 'nonce-sekailink-access'; base-uri 'self'; frame-ancestors 'none'",
    })
    return
  }
  let pathname = decodeURIComponent(url.pathname)
  if (pathname === '/') pathname = '/index.html'
  const candidate = path.normalize(path.join(distDir, pathname))
  if (!candidate.startsWith(path.normalize(distDir))) {
    sendText(res, 403, 'Forbidden')
    return
  }
  const file = existsSync(candidate) ? candidate : path.join(distDir, 'index.html')
  const info = await stat(file)
  const ext = path.extname(file)
  res.writeHead(200, {
    ...securityHeaders(),
    'content-type': contentTypes[ext] || 'application/octet-stream',
    'content-length': info.size,
    'cache-control': file.endsWith('index.html') ? 'no-store' : 'public, max-age=31536000, immutable',
  })
  createReadStream(file).pipe(res)
}

const server = createServer(async (req, res) => {
  try {
    const url = new URL(req.url || '/', `http://${req.headers.host || 'localhost'}`)

    if (url.pathname === '/health') {
      sendJson(res, 200, { ok: true, service: 'sekailink_admin_gateway', time: nowIso() })
      return
    }

    if (url.pathname === '/api/client/user-report' && req.method === 'POST') {
      await handleClientUserReport(req, res)
      return
    }

    if (url.pathname.startsWith('/api/admin/')) {
      await handleAdminApi(req, res, url)
      return
    }

    await serveStatic(req, res, url)
  } catch (error) {
    sendJson(res, 500, { ok: false, error: error instanceof Error ? error.message : String(error) })
  }
})

server.listen(config.port, config.host, () => {
  console.log(`sekailink_admin_gateway listening http://${config.host}:${config.port}`)
  if (!config.webToken && !config.allowNoToken) {
    console.warn('WARNING: SEKAILINK_ADMIN_WEB_TOKEN is not configured; admin API will fail closed.')
  }
})

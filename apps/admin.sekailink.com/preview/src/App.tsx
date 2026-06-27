import { useCallback, useEffect, useMemo, useState } from 'react'
import type { FormEvent } from 'react'
import './App.css'

type NavKey = 'dashboard' | 'users' | 'lobbies' | 'chat' | 'userReports' | 'services' | 'reports' | 'audit'
type LoadState = 'idle' | 'loading' | 'ready' | 'error'

type EndpointConfig = {
  adminBase: string
  identityBase: string
  lobbyBase: string
  chatBase: string
  agentBase: string
  roomBase: string
  token: string
}

type ApiResult<T> = {
  ok: boolean
  source: string
  data?: T
  error?: string
}

type UserRecord = {
  id?: number | string
  user_id?: number | string
  username?: string
  email?: string
  display_name?: string
  avatar_url?: string
  role?: string
  permissions?: string[]
  disabled_at?: string | null
  email_verified?: boolean
  patreon_tier?: string | null
  patreon_is_supporter?: boolean
}

type SessionRecord = {
  session_id?: number | string
  created_at?: string
  expires_at?: string
  revoked_at?: string | null
  client_name?: string | null
  client_version?: string | null
  device_id?: string | null
  user_agent?: string | null
}

type AuditRecord = {
  id?: number | string
  event_id?: number | string
  event_type?: string
  created_at?: string
  payload?: unknown
}

type UserDetail = {
  user?: UserRecord
  sessions?: SessionRecord[]
  auth_audit?: AuditRecord[]
}

type LobbyRecord = {
  id?: number | string
  lobby_id?: string
  name?: string
  visibility?: string
  status?: string
  owner_username?: string
  host?: string
  description?: string
  created_at?: string
  updated_at?: string
  closed_at?: string | null
  metadata?: Record<string, unknown>
  players?: unknown[]
}

type ServiceRecord = {
  name: string
  systemd_unit?: string | null
  state_file?: string | null
  log_file?: string | null
  state?: unknown
}

type SystemState = {
  hostname?: string
  uptime_seconds?: number
  loadavg?: { one?: number; five?: number; fifteen?: number }
  memory?: { total_bytes?: number; available_bytes?: number; free_bytes?: number }
  filesystems?: Array<{ path?: string; capacity_bytes?: number; available_bytes?: number; free_bytes?: number; error?: string }>
}

type ChannelRecord = {
  id: string
  title?: string
  irc?: string
}

type ChatMessage = {
  id?: number | string
  author?: string
  display_name?: string
  username?: string
  role?: string
  content?: string
  message?: string
  created_at?: string
  timestamp?: string
}

type PresenceUser = {
  user_id?: string
  username?: string
  display_name?: string
  name?: string
  role?: string
  ready?: boolean
}

type ReportRecord = {
  request_id?: string
  report_type?: string
  source?: string
  severity?: string
  message?: string
  timestamp?: string
  user_id?: string
  room_id?: string
  lobby_id?: string
  game?: string
  runtime?: string
}

type UserReportRecord = {
  id?: number | string
  report_id?: string
  status?: string
  severity?: string
  reporter_user_id?: string
  reporter_name?: string
  target_user_id?: string
  target_username?: string
  reason?: string
  category?: string
  message?: string
  evidence?: string
  channel_id?: string
  room_id?: string
  lobby_id?: string
  created_at?: string
  updated_at?: string
  assigned_to?: string
}

type AppData = {
  users: UserRecord[]
  selectedUser?: UserDetail
  lobbies: LobbyRecord[]
  selectedLobby?: LobbyRecord
  services: ServiceRecord[]
  system?: SystemState
  channels: ChannelRecord[]
  activeChannel?: string
  messages: ChatMessage[]
  presence: PresenceUser[]
  userReports: UserReportRecord[]
  reports: ReportRecord[]
  audit: AuditRecord[]
  serviceLogs: Record<string, string[]>
}

const navItems: Array<{ key: NavKey; label: string; hint: string }> = [
  { key: 'dashboard', label: 'Dashboard', hint: 'Live overview' },
  { key: 'users', label: 'Users', hint: 'Accounts and sessions' },
  { key: 'lobbies', label: 'Rooms & Lobbies', hint: 'Runtime operations' },
  { key: 'chat', label: 'Chat', hint: 'Community moderation' },
  { key: 'userReports', label: 'User Reports', hint: 'Player conduct' },
  { key: 'services', label: 'Services', hint: 'Health and logs' },
  { key: 'reports', label: 'Reports', hint: 'Client diagnostics' },
  { key: 'audit', label: 'Audit', hint: 'Admin timeline' },
]

const defaultConfig: EndpointConfig = {
  adminBase: import.meta.env.VITE_ADMIN_API_BASE || '/api/admin',
  identityBase: import.meta.env.VITE_IDENTITY_API_BASE || '/api/admin',
  lobbyBase: import.meta.env.VITE_LOBBY_ADMIN_API_BASE || '/api/admin',
  chatBase: import.meta.env.VITE_CHAT_GATEWAY_API_BASE || '/api/admin',
  agentBase: import.meta.env.VITE_ADMIN_AGENT_API_BASE || '/api/admin',
  roomBase: import.meta.env.VITE_ROOM_SERVER_API_BASE || '/api/admin',
  token: '',
}

const emptyData: AppData = {
  users: [],
  lobbies: [],
  services: [],
  channels: [],
  messages: [],
  presence: [],
  userReports: [],
  reports: [],
  audit: [],
  serviceLogs: {},
}

const storageKey = 'sekailink.admin.web.config'

function normalizeBase(value: string) {
  return value.trim().replace(/\/+$/, '')
}

function loadConfig(): EndpointConfig {
  try {
    const raw = window.localStorage.getItem(storageKey)
    if (!raw) return defaultConfig
    const loaded = { ...defaultConfig, ...JSON.parse(raw) }
    const legacyDevPaths = new Set(['/identity', '/lobby-admin', '/chat-gateway', '/admin-agent', '/room-server'])
    let migrated = false
    const next = { ...loaded }
    for (const key of ['identityBase', 'lobbyBase', 'chatBase', 'agentBase', 'roomBase'] as const) {
      if (legacyDevPaths.has(normalizeBase(next[key]))) {
        next[key] = defaultConfig.adminBase
        migrated = true
      }
    }
    if (migrated) saveConfig(next)
    return next
  } catch {
    return defaultConfig
  }
}

function saveConfig(config: EndpointConfig) {
  window.localStorage.setItem(storageKey, JSON.stringify(config))
}

function proxyDefaultsWithToken(token: string): EndpointConfig {
  return {
    ...defaultConfig,
    token,
  }
}

function jsonPreview(value: unknown) {
  if (value === undefined || value === null || value === '') return '-'
  if (typeof value === 'string') return value
  try {
    return JSON.stringify(value)
  } catch {
    return String(value)
  }
}

function userKey(user: UserRecord) {
  return String(user.username || user.user_id || user.id || '')
}

function userLabel(user: UserRecord) {
  return user.display_name || user.username || String(user.user_id || user.id || 'Unknown user')
}

function lobbyKey(lobby: LobbyRecord) {
  return String(lobby.lobby_id || lobby.id || lobby.name || '')
}

function lobbyLabel(lobby: LobbyRecord) {
  return lobby.name || lobby.lobby_id || 'Unnamed lobby'
}

function bytes(value?: number) {
  if (!value || value <= 0) return '-'
  const units = ['B', 'KB', 'MB', 'GB', 'TB']
  let size = value
  let unit = 0
  while (size >= 1024 && unit < units.length - 1) {
    size /= 1024
    unit += 1
  }
  return `${size.toFixed(unit === 0 ? 0 : 1)} ${units[unit]}`
}

function duration(seconds?: number) {
  if (!seconds || seconds <= 0) return '-'
  const days = Math.floor(seconds / 86400)
  const hours = Math.floor((seconds % 86400) / 3600)
  const minutes = Math.floor((seconds % 3600) / 60)
  if (days > 0) return `${days}d ${hours}h`
  if (hours > 0) return `${hours}h ${minutes}m`
  return `${minutes}m`
}

function servicePath(base: string, gatewayPath: string, directPath: string) {
  return normalizeBase(base).endsWith('/api/admin') ? gatewayPath : directPath
}

async function fetchJson<T>(base: string, path: string, token: string, init: RequestInit = {}): Promise<ApiResult<T>> {
  const url = `${normalizeBase(base)}${path.startsWith('/') ? path : `/${path}`}`
  try {
    const response = await fetch(url, {
      ...init,
      headers: {
        Accept: 'application/json',
        ...(init.body ? { 'Content-Type': 'application/json' } : {}),
        ...(token ? { Authorization: `Bearer ${token}` } : {}),
        ...(init.headers || {}),
      },
    })
    const text = await response.text()
    let data: Record<string, unknown> = {}
    try {
      data = text ? JSON.parse(text) : {}
    } catch {
      const looksHtml = text.trimStart().startsWith('<')
      return {
        ok: false,
        source: url,
        error: looksHtml ? 'endpoint_returned_html_not_json' : 'endpoint_returned_invalid_json',
      }
    }
    if (!response.ok || data?.ok === false) {
      const error = response.status === 401
        ? 'Admin token missing or invalid'
        : String(data?.error || `${response.status} ${response.statusText}`)
      return { ok: false, source: url, data: data as T, error }
    }
    return { ok: true, source: url, data: data as T }
  } catch (error) {
    return { ok: false, source: url, error: error instanceof Error ? error.message : String(error) }
  }
}

async function firstOk<T>(requests: Array<() => Promise<ApiResult<T>>>): Promise<ApiResult<T>> {
  const errors: string[] = []
  const seenErrors = new Set<string>()
  for (const request of requests) {
    const result = await request()
    if (result.ok) return result
    const message = `${result.source}: ${result.error || 'failed'}`
    if (!seenErrors.has(message)) {
      errors.push(message)
      seenErrors.add(message)
    }
  }
  return { ok: false, source: 'all', error: errors.join('\n') || 'No endpoint configured' }
}

function extractArray<T>(data: unknown, keys: string[]): T[] {
  if (Array.isArray(data)) return data as T[]
  if (!data || typeof data !== 'object') return []
  const record = data as Record<string, unknown>
  for (const key of keys) {
    const value = record[key]
    if (Array.isArray(value)) return value as T[]
  }
  return []
}

function extractObject<T>(data: unknown, keys: string[]): T | undefined {
  if (!data || typeof data !== 'object') return undefined
  const record = data as Record<string, unknown>
  for (const key of keys) {
    const value = record[key]
    if (value && typeof value === 'object') return value as T
  }
  return data as T
}

function StatusPill({ value }: { value?: string | null }) {
  const normalized = String(value || 'unknown').toLowerCase()
  return <span className={`pill ${normalized.replace(/[^a-z0-9-]/g, '-')}`}>{value || 'unknown'}</span>
}

function EmptyState({ title, detail }: { title: string; detail: string }) {
  return (
    <div className="empty-state">
      <strong>{title}</strong>
      <span>{detail}</span>
    </div>
  )
}

function App() {
  const [config, setConfig] = useState<EndpointConfig>(() => loadConfig())
  const [draftConfig, setDraftConfig] = useState<EndpointConfig>(() => loadConfig())
  const [data, setData] = useState<AppData>(emptyData)
  const [activeNav, setActiveNav] = useState<NavKey>('dashboard')
  const [loadState, setLoadState] = useState<LoadState>('idle')
  const [lastError, setLastError] = useState('')
  const [lastRefresh, setLastRefresh] = useState('')
  const [userQuery, setUserQuery] = useState('')
  const [lobbyQuery, setLobbyQuery] = useState('')
  const [reportQuery, setReportQuery] = useState('')
  const [actionBusy, setActionBusy] = useState('')

  const filteredUsers = useMemo(() => {
    const needle = userQuery.trim().toLowerCase()
    if (!needle) return data.users
    return data.users.filter((user) => jsonPreview(user).toLowerCase().includes(needle))
  }, [data.users, userQuery])

  const filteredLobbies = useMemo(() => {
    const needle = lobbyQuery.trim().toLowerCase()
    if (!needle) return data.lobbies
    return data.lobbies.filter((lobby) => jsonPreview(lobby).toLowerCase().includes(needle))
  }, [data.lobbies, lobbyQuery])

  const connectedServices = data.services.length
  const disabledUsers = data.users.filter((user) => user.disabled_at).length
  const openLobbies = data.lobbies.filter((lobby) => String(lobby.status || '').toLowerCase() !== 'closed').length
  const onlinePresence = data.presence.length
  const openUserReports = data.userReports.filter((report) => !['closed', 'resolved', 'dismissed'].includes(String(report.status || '').toLowerCase())).length

  const filteredUserReports = useMemo(() => {
    const needle = reportQuery.trim().toLowerCase()
    if (!needle) return data.userReports
    return data.userReports.filter((report) => jsonPreview(report).toLowerCase().includes(needle))
  }, [data.userReports, reportQuery])

  const refresh = useCallback(async () => {
    setLoadState('loading')
    setLastError('')
    const token = config.token.trim()
    const [usersRes, lobbiesRes, servicesRes, systemRes, channelsRes, userReportsRes, roomsReportsRes] = await Promise.all([
      firstOk<{ users?: UserRecord[] }>([
        () => fetchJson(config.adminBase, '/users?limit=250', token),
        () => fetchJson(config.identityBase, servicePath(config.identityBase, '/users?limit=250', '/admin/users?limit=250'), token),
      ]),
      firstOk<{ lobbies?: LobbyRecord[] }>([
        () => fetchJson(config.adminBase, '/lobbies?limit=250', token),
        () => fetchJson(config.lobbyBase, servicePath(config.lobbyBase, '/lobbies?limit=250', '/admin/lobbies?limit=250'), token),
      ]),
      firstOk<{ services?: ServiceRecord[] }>([
        () => fetchJson(config.adminBase, '/services', token),
        () => fetchJson(config.agentBase, '/services', token),
      ]),
      firstOk<{ system?: SystemState }>([
        () => fetchJson(config.adminBase, '/system', token),
        () => fetchJson(config.agentBase, '/system', token),
      ]),
      firstOk<{ channels?: ChannelRecord[] }>([
        () => fetchJson(config.adminBase, '/channels', token),
        () => fetchJson(config.chatBase, '/channels', token),
      ]),
      firstOk<{ user_reports?: UserReportRecord[]; reports?: UserReportRecord[] }>([
        () => fetchJson(config.adminBase, '/user-reports?limit=100', token),
        () => fetchJson(config.adminBase, '/moderation/reports?limit=100', token),
      ]),
      firstOk<{ client_reports?: ReportRecord[]; reports?: ReportRecord[] }>([
        () => fetchJson(config.adminBase, '/reports?limit=100', token),
        () => fetchJson(config.roomBase, servicePath(config.roomBase, '/reports?limit=100', '/rooms/__all__/client-reports'), token),
      ]),
    ])

    const nextChannels = extractArray<ChannelRecord>(channelsRes.data, ['channels'])
    const activeChannel = data.activeChannel || nextChannels[0]?.id
    let messages: ChatMessage[] = data.messages
    let presence: PresenceUser[] = data.presence
    if (activeChannel) {
      const [messagesRes, presenceRes] = await Promise.all([
        firstOk<{ messages?: ChatMessage[] }>([
          () => fetchJson(config.adminBase, `/channels/${encodeURIComponent(activeChannel)}/messages`, token),
          () => fetchJson(config.chatBase, `/channels/${encodeURIComponent(activeChannel)}/messages`, token),
        ]),
        firstOk<{ users?: PresenceUser[]; presence?: PresenceUser[] }>([
          () => fetchJson(config.adminBase, `/channels/${encodeURIComponent(activeChannel)}/presence`, token),
          () => fetchJson(config.chatBase, `/channels/${encodeURIComponent(activeChannel)}/presence`, token),
        ]),
      ])
      messages = extractArray<ChatMessage>(messagesRes.data, ['messages'])
      presence = extractArray<PresenceUser>(presenceRes.data, ['users', 'presence'])
    }

    const errors = [usersRes, lobbiesRes, servicesRes, systemRes, channelsRes]
      .filter((result) => !result.ok)
      .map((result) => result.error)
      .filter(Boolean)

    setData((current) => ({
      ...current,
      users: extractArray<UserRecord>(usersRes.data, ['users']),
      lobbies: extractArray<LobbyRecord>(lobbiesRes.data, ['lobbies', 'rooms']),
      services: extractArray<ServiceRecord>(servicesRes.data, ['services']),
      system: extractObject<SystemState>(systemRes.data, ['system']),
      channels: nextChannels,
      activeChannel,
      messages,
      presence,
      userReports: extractArray<UserReportRecord>(userReportsRes.data, ['user_reports', 'reports']),
      reports: extractArray<ReportRecord>(roomsReportsRes.data, ['reports', 'client_reports']),
    }))
    setLastRefresh(new Date().toLocaleTimeString())
    setLastError(errors.join('\n'))
    setLoadState(errors.length === 5 ? 'error' : 'ready')
  }, [config, data.activeChannel, data.messages, data.presence])

  useEffect(() => {
    refresh()
  }, [refresh])

  useEffect(() => {
    const id = window.setInterval(() => {
      if (activeNav === 'dashboard' || activeNav === 'chat' || activeNav === 'services') {
        refresh()
      }
    }, 8000)
    return () => window.clearInterval(id)
  }, [activeNav, refresh])

  function submitConfig(event: FormEvent) {
    event.preventDefault()
    const next = {
      ...draftConfig,
      adminBase: normalizeBase(draftConfig.adminBase),
      identityBase: normalizeBase(draftConfig.identityBase),
      lobbyBase: normalizeBase(draftConfig.lobbyBase),
      chatBase: normalizeBase(draftConfig.chatBase),
      agentBase: normalizeBase(draftConfig.agentBase),
      roomBase: normalizeBase(draftConfig.roomBase),
    }
    setConfig(next)
    saveConfig(next)
  }

  async function openUser(user: UserRecord) {
    const key = userKey(user)
    if (!key) return
    const result = await firstOk<UserDetail>([
      () => fetchJson(config.adminBase, `/users/${encodeURIComponent(key)}`, config.token),
      () => fetchJson(config.identityBase, servicePath(config.identityBase, `/users/${encodeURIComponent(key)}`, `/admin/users/${encodeURIComponent(key)}`), config.token),
    ])
    if (result.ok) {
      setData((current) => ({ ...current, selectedUser: result.data }))
    } else {
      setLastError(result.error || 'Unable to open user')
    }
  }

  async function adminAction(label: string, run: (reason: string) => Promise<ApiResult<unknown>>) {
    const reason = window.prompt(`Reason for ${label}:`)
    if (!reason || reason.trim().length < 3) return
    setActionBusy(label)
    const result = await run(reason.trim())
    setActionBusy('')
    if (!result.ok) {
      setLastError(result.error || `${label} failed`)
      return
    }
    await refresh()
  }

  async function disableUser(user: UserRecord) {
    const key = userKey(user)
    if (!key) return
    await adminAction(`disable ${key}`, (reason) =>
      firstOk([
        () => fetchJson(config.adminBase, `/users/${encodeURIComponent(key)}/actions`, config.token, {
          method: 'POST',
          body: JSON.stringify({ action: 'disable', reason, confirm: true }),
        }),
        () => fetchJson(config.identityBase, servicePath(config.identityBase, `/users/${encodeURIComponent(key)}/actions`, `/admin/users/${encodeURIComponent(key)}`), config.token, {
          method: normalizeBase(config.identityBase).endsWith('/api/admin') ? 'POST' : 'DELETE',
          body: normalizeBase(config.identityBase).endsWith('/api/admin') ? JSON.stringify({ action: 'disable', reason, confirm: true }) : undefined,
        }),
      ]),
    )
  }

  async function forcePasswordReset(user: UserRecord) {
    const key = userKey(user)
    if (!key) return
    await adminAction(`force password reset for ${key}`, (reason) =>
      firstOk([
        () => fetchJson(config.adminBase, `/users/${encodeURIComponent(key)}/actions`, config.token, {
          method: 'POST',
          body: JSON.stringify({ action: 'force-password-reset', reason, confirm: true }),
        }),
        () => fetchJson(config.identityBase, servicePath(config.identityBase, `/users/${encodeURIComponent(key)}/actions`, `/admin/users/${encodeURIComponent(key)}/force-password-reset`), config.token, {
          method: 'POST',
          body: normalizeBase(config.identityBase).endsWith('/api/admin') ? JSON.stringify({ action: 'force-password-reset', reason, confirm: true }) : undefined,
        }),
      ]),
    )
  }

  async function closeLobby(lobby: LobbyRecord) {
    const key = lobbyKey(lobby)
    if (!key) return
    await adminAction(`close lobby ${key}`, (reason) =>
      firstOk([
        () => fetchJson(config.adminBase, `/lobbies/${encodeURIComponent(key)}/actions`, config.token, {
          method: 'POST',
          body: JSON.stringify({ action: 'close', reason, confirm: true }),
        }),
        () => fetchJson(config.lobbyBase, servicePath(config.lobbyBase, `/lobbies/${encodeURIComponent(key)}/actions`, `/admin/lobbies/${encodeURIComponent(key)}/close`), config.token, {
          method: 'POST',
          body: normalizeBase(config.lobbyBase).endsWith('/api/admin') ? JSON.stringify({ action: 'close', reason, confirm: true }) : undefined,
        }),
      ]),
    )
  }

  async function serviceAction(service: ServiceRecord, action: 'start' | 'stop' | 'restart') {
    await adminAction(`${action} ${service.name}`, (reason) =>
      firstOk([
        () => fetchJson(config.adminBase, `/services/${encodeURIComponent(service.name)}/actions`, config.token, {
          method: 'POST',
          body: JSON.stringify({ action, reason, confirm: true }),
        }),
        () => fetchJson(config.agentBase, servicePath(config.agentBase, `/services/${encodeURIComponent(service.name)}/actions`, `/services/${encodeURIComponent(service.name)}/${action}`), config.token, {
          method: 'POST',
          body: normalizeBase(config.agentBase).endsWith('/api/admin') ? JSON.stringify({ action, reason, confirm: true }) : undefined,
        }),
      ]),
    )
  }

  async function userReportAction(report: UserReportRecord, action: 'review' | 'resolve' | 'dismiss') {
    const key = String(report.report_id || report.id || '')
    if (!key) return
    await adminAction(`${action} user report ${key}`, (reason) =>
      firstOk([
        () => fetchJson(config.adminBase, `/user-reports/${encodeURIComponent(key)}/actions`, config.token, {
          method: 'POST',
          body: JSON.stringify({ action, reason, confirm: true }),
        }),
        () => fetchJson(config.adminBase, `/moderation/reports/${encodeURIComponent(key)}/actions`, config.token, {
          method: 'POST',
          body: JSON.stringify({ action, reason, confirm: true }),
        }),
      ]),
    )
  }

  async function loadServiceLogs(service: ServiceRecord) {
    const result = await firstOk<{ lines?: string[] }>([
      () => fetchJson(config.adminBase, `/services/${encodeURIComponent(service.name)}/logs`, config.token),
      () => fetchJson(config.agentBase, `/services/${encodeURIComponent(service.name)}/logs`, config.token),
    ])
    if (!result.ok) {
      setLastError(result.error || 'Unable to load logs')
      return
    }
    setData((current) => ({
      ...current,
      serviceLogs: { ...current.serviceLogs, [service.name]: extractArray<string>(result.data, ['lines', 'logs']) },
    }))
  }

  async function selectChannel(channelId: string) {
    setData((current) => ({ ...current, activeChannel: channelId }))
    const [messagesRes, presenceRes] = await Promise.all([
      firstOk<{ messages?: ChatMessage[] }>([
        () => fetchJson(config.adminBase, `/channels/${encodeURIComponent(channelId)}/messages`, config.token),
        () => fetchJson(config.chatBase, `/channels/${encodeURIComponent(channelId)}/messages`, config.token),
      ]),
      firstOk<{ users?: PresenceUser[]; presence?: PresenceUser[] }>([
        () => fetchJson(config.adminBase, `/channels/${encodeURIComponent(channelId)}/presence`, config.token),
        () => fetchJson(config.chatBase, `/channels/${encodeURIComponent(channelId)}/presence`, config.token),
      ]),
    ])
    setData((current) => ({
      ...current,
      messages: extractArray<ChatMessage>(messagesRes.data, ['messages']),
      presence: extractArray<PresenceUser>(presenceRes.data, ['users', 'presence']),
    }))
  }

  return (
    <div className="admin-shell">
      <aside className="sidebar">
        <div className="brand">
          <div className="brand-mark">S</div>
          <div>
            <strong>SekaiLink</strong>
            <span>Core Access Web</span>
          </div>
        </div>
        <nav>
          {navItems.map((item) => (
            <button
              key={item.key}
              className={activeNav === item.key ? 'active' : ''}
              type="button"
              onClick={() => setActiveNav(item.key)}
            >
              <span>{item.label}</span>
              <small>{item.hint}</small>
            </button>
          ))}
        </nav>
        <div className="sidebar-card">
          <strong>Connection</strong>
          <span>{loadState === 'loading' ? 'Refreshing...' : loadState === 'ready' ? 'Connected' : 'Needs endpoints'}</span>
          <small>Last refresh: {lastRefresh || '-'}</small>
        </div>
      </aside>

      <main className="content">
        <header className="topbar">
          <div>
            <p className="eyebrow">SekaiLink administration cockpit</p>
            <h1>{navItems.find((item) => item.key === activeNav)?.label}</h1>
          </div>
          <div className="topbar-actions">
            <button type="button" onClick={refresh} disabled={loadState === 'loading'}>
              {loadState === 'loading' ? 'Refreshing' : 'Refresh'}
            </button>
          </div>
        </header>

        <section className="connection-panel">
          <form onSubmit={submitConfig}>
            <label>
              Admin gateway
              <input value={draftConfig.adminBase} onChange={(event) => setDraftConfig({ ...draftConfig, adminBase: event.target.value })} />
            </label>
            <label>
              Identity
              <input value={draftConfig.identityBase} onChange={(event) => setDraftConfig({ ...draftConfig, identityBase: event.target.value })} />
            </label>
            <label>
              Lobby admin
              <input value={draftConfig.lobbyBase} onChange={(event) => setDraftConfig({ ...draftConfig, lobbyBase: event.target.value })} />
            </label>
            <label>
              Chat gateway
              <input value={draftConfig.chatBase} onChange={(event) => setDraftConfig({ ...draftConfig, chatBase: event.target.value })} />
            </label>
            <label>
              Admin agent
              <input value={draftConfig.agentBase} onChange={(event) => setDraftConfig({ ...draftConfig, agentBase: event.target.value })} />
            </label>
            <label>
              Room server
              <input value={draftConfig.roomBase} onChange={(event) => setDraftConfig({ ...draftConfig, roomBase: event.target.value })} />
            </label>
            <label className="token-field">
              Admin token
              <input
                value={draftConfig.token}
                type="password"
                autoComplete="off"
                onChange={(event) => setDraftConfig({ ...draftConfig, token: event.target.value })}
              />
            </label>
            <button type="submit">Save endpoints</button>
            <button
              type="button"
              onClick={() => {
                const next = proxyDefaultsWithToken(draftConfig.token)
                setDraftConfig(next)
                setConfig(next)
                saveConfig(next)
              }}
            >
              Gateway defaults
            </button>
          </form>
          {lastError && <pre className="error-box">{lastError}</pre>}
        </section>

        {activeNav === 'dashboard' && (
          <section className="page-grid">
            <article className="metric-card">
              <span>Users</span>
              <strong>{data.users.length}</strong>
              <small>{disabledUsers} disabled</small>
            </article>
            <article className="metric-card">
              <span>Open lobbies</span>
              <strong>{openLobbies}</strong>
              <small>{data.lobbies.length} total listed</small>
            </article>
            <article className="metric-card">
              <span>Visible chat users</span>
              <strong>{onlinePresence}</strong>
              <small>{data.activeChannel || 'No channel selected'}</small>
            </article>
            <article className="metric-card">
              <span>User reports</span>
              <strong>{openUserReports}</strong>
              <small>{data.userReports.length} total loaded</small>
            </article>
            <article className="metric-card">
              <span>Services</span>
              <strong>{connectedServices}</strong>
              <small>{data.system?.hostname || 'agent not connected'}</small>
            </article>
            <article className="panel wide">
              <h2>Server Matrix</h2>
              {data.system ? (
                <div className="system-grid">
                  <div><span>Host</span><strong>{data.system.hostname || '-'}</strong></div>
                  <div><span>Uptime</span><strong>{duration(data.system.uptime_seconds)}</strong></div>
                  <div><span>Load</span><strong>{data.system.loadavg?.one?.toFixed(2) || '-'}</strong></div>
                  <div><span>RAM available</span><strong>{bytes(data.system.memory?.available_bytes)}</strong></div>
                  {(data.system.filesystems || []).map((fs) => (
                    <div key={fs.path || fs.error}>
                      <span>{fs.path || 'Filesystem'}</span>
                      <strong>{bytes(fs.available_bytes || fs.free_bytes)} free</strong>
                    </div>
                  ))}
                </div>
              ) : (
                <EmptyState title="Admin agent not connected" detail="Configure the admin-agent base URL and token to see system data." />
              )}
            </article>
            <article className="panel">
              <h2>Recent Reports</h2>
              {data.reports.slice(0, 6).map((report, index) => (
                <div className="stack-row" key={report.request_id || index}>
                  <strong>{report.message || report.report_type || 'Client report'}</strong>
                  <span>{report.source || report.runtime || '-'} · {report.severity || 'info'}</span>
                </div>
              ))}
              {data.reports.length === 0 && <EmptyState title="No reports loaded" detail="Bug-report aggregation is ready to connect." />}
            </article>
          </section>
        )}

        {activeNav === 'users' && (
          <section className="split-page">
            <article className="panel">
              <div className="panel-heading">
                <h2>User Search</h2>
                <input placeholder="Search username, email, id..." value={userQuery} onChange={(event) => setUserQuery(event.target.value)} />
              </div>
              <div className="table">
                {filteredUsers.map((user) => (
                  <button className="table-row" key={userKey(user)} type="button" onClick={() => openUser(user)}>
                    <span className="avatar">{userLabel(user).slice(0, 1).toUpperCase()}</span>
                    <span><strong>{userLabel(user)}</strong><small>{user.email || user.username}</small></span>
                    <StatusPill value={user.disabled_at ? 'disabled' : user.role || 'player'} />
                  </button>
                ))}
              </div>
              {filteredUsers.length === 0 && <EmptyState title="No users" detail="No users matched the current filter or endpoint." />}
            </article>
            <article className="panel detail">
              <h2>User Detail</h2>
              {data.selectedUser?.user ? (
                <>
                  <div className="detail-head">
                    <span className="avatar large">{userLabel(data.selectedUser.user).slice(0, 1).toUpperCase()}</span>
                    <div>
                      <strong>{userLabel(data.selectedUser.user)}</strong>
                      <span>{data.selectedUser.user.email || '-'}</span>
                    </div>
                  </div>
                  <div className="action-row">
                    <button type="button" onClick={() => disableUser(data.selectedUser!.user!)} disabled={Boolean(actionBusy)}>Disable</button>
                    <button type="button" onClick={() => forcePasswordReset(data.selectedUser!.user!)} disabled={Boolean(actionBusy)}>Password reset</button>
                  </div>
                  <h3>Sessions</h3>
                  {(data.selectedUser.sessions || []).map((session) => (
                    <div className="stack-row" key={String(session.session_id)}>
                      <strong>{session.client_name || session.user_agent || 'Session'}</strong>
                      <span>{session.created_at || '-'} · {session.revoked_at ? 'revoked' : 'active'}</span>
                    </div>
                  ))}
                  <h3>Audit</h3>
                  {(data.selectedUser.auth_audit || []).map((event) => (
                    <div className="stack-row" key={String(event.event_id || event.id)}>
                      <strong>{event.event_type || 'event'}</strong>
                      <span>{event.created_at || '-'}</span>
                    </div>
                  ))}
                </>
              ) : (
                <EmptyState title="Select a user" detail="Open a user to inspect sessions, audit, and moderation actions." />
              )}
            </article>
          </section>
        )}

        {activeNav === 'lobbies' && (
          <section className="panel">
            <div className="panel-heading">
              <h2>Rooms & Lobbies</h2>
              <input placeholder="Search lobby, host, game..." value={lobbyQuery} onChange={(event) => setLobbyQuery(event.target.value)} />
            </div>
            <div className="lobby-grid">
              {filteredLobbies.map((lobby) => (
                <article className="lobby-card" key={lobbyKey(lobby)}>
                  <div>
                    <strong>{lobbyLabel(lobby)}</strong>
                    <StatusPill value={lobby.status || lobby.visibility} />
                  </div>
                  <span>Host: {lobby.owner_username || lobby.host || '-'}</span>
                  <span>Updated: {lobby.updated_at || lobby.created_at || '-'}</span>
                  <div className="action-row">
                    <button type="button" onClick={() => setData((current) => ({ ...current, selectedLobby: lobby }))}>Inspect</button>
                    <button type="button" onClick={() => closeLobby(lobby)} disabled={Boolean(actionBusy)}>Close</button>
                  </div>
                </article>
              ))}
            </div>
            {data.selectedLobby && <pre className="json-box">{JSON.stringify(data.selectedLobby, null, 2)}</pre>}
          </section>
        )}

        {activeNav === 'chat' && (
          <section className="chat-page">
            <aside className="panel channel-list">
              <h2>Channels</h2>
              {data.channels.map((channel) => (
                <button
                  key={channel.id}
                  className={data.activeChannel === channel.id ? 'active' : ''}
                  type="button"
                  onClick={() => selectChannel(channel.id)}
                >
                  #{channel.title || channel.id}
                  <small>{channel.irc || channel.id}</small>
                </button>
              ))}
            </aside>
            <article className="panel chat-log">
              <h2>Live Chat {data.activeChannel ? `#${data.activeChannel}` : ''}</h2>
              <div className="messages">
                {data.messages.map((message, index) => (
                  <div className="message" key={String(message.id || index)}>
                    <strong>{message.display_name || message.author || message.username || 'Unknown'}</strong>
                    <span>{message.content || message.message || ''}</span>
                    <small>{message.created_at || message.timestamp || ''}</small>
                  </div>
                ))}
                {data.messages.length === 0 && <EmptyState title="No messages loaded" detail="Select a channel or connect the chat gateway." />}
              </div>
            </article>
            <aside className="panel presence-list">
              <h2>Presence</h2>
              {data.presence.map((user) => (
                <div className="stack-row" key={user.user_id || user.username}>
                  <strong>{user.name || user.display_name || user.username}</strong>
                  <span>{user.role || 'player'} {user.ready ? '· ready' : ''}</span>
                </div>
              ))}
            </aside>
          </section>
        )}

        {activeNav === 'userReports' && (
          <section className="panel">
            <div className="panel-heading">
              <div>
                <h2>User Reports</h2>
                <p className="muted">Community moderation queue for player conduct reports.</p>
              </div>
              <input placeholder="Search target, reporter, reason..." value={reportQuery} onChange={(event) => setReportQuery(event.target.value)} />
            </div>
            <div className="table">
              {filteredUserReports.map((report, index) => (
                <div className="moderation-report-row" key={String(report.report_id || report.id || index)}>
                  <div>
                    <StatusPill value={report.status || 'open'} />
                    <StatusPill value={report.severity || report.category || 'conduct'} />
                  </div>
                  <span>
                    <strong>{report.target_username || report.target_user_id || 'Unknown target'}</strong>
                    <small>
                      Reported by {report.reporter_name || report.reporter_user_id || 'unknown'} · {report.created_at || '-'}
                    </small>
                  </span>
                  <p>{report.reason || report.message || report.evidence || 'No report text provided.'}</p>
                  <code>{report.channel_id || report.room_id || report.lobby_id || 'no-context'}</code>
                  <div className="action-row">
                    <button type="button" onClick={() => userReportAction(report, 'review')} disabled={Boolean(actionBusy)}>Review</button>
                    <button type="button" onClick={() => userReportAction(report, 'resolve')} disabled={Boolean(actionBusy)}>Resolve</button>
                    <button type="button" onClick={() => userReportAction(report, 'dismiss')} disabled={Boolean(actionBusy)}>Dismiss</button>
                  </div>
                </div>
              ))}
            </div>
            {filteredUserReports.length === 0 && (
              <EmptyState
                title="No user reports loaded"
                detail="Expose /api/admin/user-reports or /api/admin/moderation/reports to populate the moderation queue."
              />
            )}
          </section>
        )}

        {activeNav === 'services' && (
          <section className="split-page">
            <article className="panel">
              <h2>Services</h2>
              <div className="table">
                {data.services.map((service) => (
                  <div className="service-row" key={service.name}>
                    <div>
                      <strong>{service.name}</strong>
                      <small>{service.systemd_unit || service.log_file || '-'}</small>
                    </div>
                    <div className="action-row">
                      <button type="button" onClick={() => loadServiceLogs(service)}>Logs</button>
                      <button type="button" onClick={() => serviceAction(service, 'restart')} disabled={Boolean(actionBusy)}>Restart</button>
                      <button type="button" onClick={() => serviceAction(service, 'stop')} disabled={Boolean(actionBusy)}>Stop</button>
                      <button type="button" onClick={() => serviceAction(service, 'start')} disabled={Boolean(actionBusy)}>Start</button>
                    </div>
                  </div>
                ))}
              </div>
            </article>
            <article className="panel detail">
              <h2>Service Logs</h2>
              {Object.entries(data.serviceLogs).map(([name, lines]) => (
                <div key={name}>
                  <h3>{name}</h3>
                  <pre className="log-box">{lines.join('\n')}</pre>
                </div>
              ))}
              {Object.keys(data.serviceLogs).length === 0 && <EmptyState title="No logs loaded" detail="Click Logs on an allowlisted service." />}
            </article>
          </section>
        )}

        {activeNav === 'reports' && (
          <section className="panel">
            <h2>Client Diagnostics</h2>
            <div className="table">
              {data.reports.map((report, index) => (
                <div className="report-row" key={report.request_id || index}>
                  <StatusPill value={report.severity || report.report_type || 'report'} />
                  <span><strong>{report.message || '-'}</strong><small>{report.source || report.runtime || '-'} · {report.timestamp || '-'}</small></span>
                  <code>{report.user_id || report.room_id || report.lobby_id || '-'}</code>
                </div>
              ))}
            </div>
            {data.reports.length === 0 && <EmptyState title="No diagnostics yet" detail="The view is ready for bootloader/client/Sekaiemu/SKLMI reports." />}
          </section>
        )}

        {activeNav === 'audit' && (
          <section className="panel">
            <h2>Audit Timeline</h2>
            <p className="muted">User-specific audit appears in user detail. Global admin audit will populate here when the Admin Gateway exposes it.</p>
            <div className="table">
              {data.audit.map((event) => (
                <div className="stack-row" key={String(event.event_id || event.id)}>
                  <strong>{event.event_type}</strong>
                  <span>{event.created_at || '-'}</span>
                </div>
              ))}
            </div>
          </section>
        )}
      </main>
    </div>
  )
}

export default App

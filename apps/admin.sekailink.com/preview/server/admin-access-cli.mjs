#!/usr/bin/env node
import { readFileSync } from 'node:fs'

const envPath = process.env.SEKAILINK_ADMIN_ENV_FILE || '/etc/sekailink/admin-gateway.env'

function loadEnvFile(file) {
  try {
    const text = readFileSync(file, 'utf8')
    for (const line of text.split(/\r?\n/)) {
      const trimmed = line.trim()
      if (!trimmed || trimmed.startsWith('#')) continue
      const index = trimmed.indexOf('=')
      if (index <= 0) continue
      const key = trimmed.slice(0, index).trim()
      let value = trimmed.slice(index + 1).trim()
      if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith("'") && value.endsWith("'"))) {
        value = value.slice(1, -1)
      }
      if (!process.env[key]) process.env[key] = value
    }
  } catch {
    // Environment variables can still be provided directly.
  }
}

function usage() {
  console.error('Usage:')
  console.error('  admin-access-cli.mjs list <totp_code>')
  console.error('  admin-access-cli.mjs approve <request_id> <totp_code>')
  console.error('  admin-access-cli.mjs reject <request_id> <totp_code>')
  process.exit(2)
}

loadEnvFile(envPath)

const base = (process.env.SEKAILINK_ADMIN_LOCAL_BASE || `http://${process.env.SEKAILINK_ADMIN_HOST || '127.0.0.1'}:${process.env.SEKAILINK_ADMIN_PORT || 19110}`).replace(/\/+$/, '')
const token = process.env.SEKAILINK_ADMIN_ACCESS_APPROVAL_TOKEN || process.env.SEKAILINK_ADMIN_WEB_TOKEN || process.env.SEKAILINK_ADMIN_TOKEN || ''
const [command, arg1, arg2] = process.argv.slice(2)
const requestId = command === 'list' ? '' : arg1
const otp = command === 'list' ? arg1 : arg2

if (!command || !token || !otp) usage()

const headers = {
  Accept: 'application/json',
  Authorization: `Bearer ${token}`,
}
if (otp) headers['X-SekaiLink-Admin-Otp'] = otp

const path = command === 'list'
  ? '/api/admin/access/requests'
  : requestId && ['approve', 'reject'].includes(command)
    ? `/api/admin/access/requests/${encodeURIComponent(requestId)}/${command}`
    : ''

if (!path) usage()

const response = await fetch(`${base}${path}`, {
  method: command === 'list' ? 'GET' : 'POST',
  headers,
})
const text = await response.text()
let data = null
try {
  data = text ? JSON.parse(text) : {}
} catch {
  console.error(text)
  process.exit(1)
}

if (!response.ok || data?.ok === false) {
  console.error(JSON.stringify(data, null, 2))
  process.exit(1)
}

console.log(JSON.stringify(data, null, 2))

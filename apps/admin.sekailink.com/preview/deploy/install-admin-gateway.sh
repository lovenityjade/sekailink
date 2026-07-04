#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_DIR="${SEKAILINK_ADMIN_INSTALL_DIR:-/opt/sekailink/admin-web}"
DATA_DIR="${SEKAILINK_ADMIN_DATA_DIR:-/srv/nexus-data/admin-web}"
ENV_DIR="${SEKAILINK_ADMIN_ENV_DIR:-/etc/sekailink}"
NGINX_AVAILABLE="${SEKAILINK_NGINX_AVAILABLE:-/etc/nginx/sites-available/admin.sekailink.com}"
NGINX_ENABLED="${SEKAILINK_NGINX_ENABLED:-/etc/nginx/sites-enabled/admin.sekailink.com}"

if [[ ! -d "$ROOT/dist" ]]; then
  echo "dist/ is missing. Run npm run build first." >&2
  exit 1
fi

install -d "$INSTALL_DIR"
install -d "$INSTALL_DIR/server"
install -d "$INSTALL_DIR/dist"
install -d "$DATA_DIR"
install -d "$ENV_DIR"

cp -a "$ROOT/dist/." "$INSTALL_DIR/dist/"
cp -a "$ROOT/server/admin-gateway.mjs" "$INSTALL_DIR/server/admin-gateway.mjs"
cp -a "$ROOT/package.json" "$INSTALL_DIR/package.json"

if [[ ! -f "$ENV_DIR/admin-gateway.env" ]]; then
  cp "$ROOT/deploy/admin-gateway.env.example" "$ENV_DIR/admin-gateway.env"
  chmod 600 "$ENV_DIR/admin-gateway.env"
  echo "Created $ENV_DIR/admin-gateway.env. Edit real tokens before starting."
fi

cp "$ROOT/deploy/sekailink-admin-gateway.service" /etc/systemd/system/sekailink-admin-gateway.service
cp "$ROOT/deploy/nginx-admin.sekailink.com.conf" "$NGINX_AVAILABLE"
ln -sfn "$NGINX_AVAILABLE" "$NGINX_ENABLED"

systemctl daemon-reload
echo "Installed SekaiLink Admin Gateway files."
echo "Next:"
echo "  1. Edit $ENV_DIR/admin-gateway.env"
echo "  2. nginx -t"
echo "  3. systemctl enable --now sekailink-admin-gateway"
echo "  4. systemctl reload nginx"

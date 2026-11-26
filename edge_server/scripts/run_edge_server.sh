#!/usr/bin/env bash
# 启动 Edge Server：从仓库根目录的 .env 注入 MQTT 凭据。
# 配置文件里只保留 ${AMR_MQTT_PASSWORD} 占位符，真实值不进入仓库。
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

if [[ ! -f "$PROJECT_ROOT/.env" ]]; then
    echo "[edge] missing .env, run: cp .env.example .env and set AMR_MQTT_PASSWORD" >&2
    exit 1
fi

set -a
# shellcheck disable=SC1091
source "$PROJECT_ROOT/.env"
set +a

exec "$PROJECT_ROOT/edge_server/bin/edge_server" "$@"

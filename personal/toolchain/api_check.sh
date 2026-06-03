#!/bin/bash
# Pre-build API version check (D3-05).
# Compares SDK API (from api_symbols.csv in UFBT_HOME) vs device API (from cache).
# On mismatch (major OR minor), fail loud — no hardcoded fallback.

set -u
CACHE_FILE="$HOME/.local/srcs/gitclones/flipperzero-firmware-wPlugins/personal/toolchain/last_device_api.json"
TTL_SECONDS=$((24 * 60 * 60))

# Refresh cache if missing or stale (>24h)
REFRESH_NEEDED=
if [[ -f "$CACHE_FILE" ]]; then
  CACHE_TIME=$(stat -c %Y "$CACHE_FILE" 2>/dev/null || stat -f %m "$CACHE_FILE" 2>/dev/null)
  NOW=$(date +%s)
  AGE=$((NOW - CACHE_TIME))
  if [[ $AGE -gt $TTL_SECONDS ]]; then
    echo "[*] API cache stale (>24h); refreshing..." >&2
    REFRESH_NEEDED=1
  fi
else
  REFRESH_NEEDED=1
fi

if [[ -n "$REFRESH_NEEDED" ]]; then
  DEVICE_INFO=$(python3 "$HOME/codeWS/Projects/Flipper0/.planning/tools/flipper_cli.py" device_info 2>/dev/null)
  D_MAJOR=$(echo "$DEVICE_INFO" | grep -E "^firmware_api_major[[:space:]]*:" | awk -F: '{print $2}' | tr -d ' \r')
  D_MINOR=$(echo "$DEVICE_INFO" | grep -E "^firmware_api_minor[[:space:]]*:" | awk -F: '{print $2}' | tr -d ' \r')
  if [[ -z "$D_MAJOR" || -z "$D_MINOR" ]]; then
    echo "ERROR: Could not parse device API from flipper_cli.py device_info (device not connected?)" >&2
    exit 1
  fi
  printf '{"api_major":"%s","api_minor":"%s","timestamp":%d}\n' "$D_MAJOR" "$D_MINOR" "$(date +%s)" > "$CACHE_FILE"
fi

DEVICE_MAJOR=$(python3 -c "import json; d=json.load(open('$CACHE_FILE')); print(d['api_major'])" 2>/dev/null)
DEVICE_MINOR=$(python3 -c "import json; d=json.load(open('$CACHE_FILE')); print(d['api_minor'])" 2>/dev/null)
if [[ -z "$DEVICE_MAJOR" || -z "$DEVICE_MINOR" ]]; then
  echo "ERROR: Could not determine device API from cache" >&2
  exit 1
fi

if [[ -z "${UFBT_HOME:-}" ]]; then
  echo "ERROR: UFBT_HOME not set; source personal/toolchain/ufbt-env.sh first" >&2
  exit 1
fi

# SDK API version lives at $UFBT_HOME/current/sdk_headers/f7_sdk/targets/f7/api_symbols.csv
# Format: header on row 1, row 2 = "Version,+,<major.minor>,,"
API_SYMS="$UFBT_HOME/current/sdk_headers/f7_sdk/targets/f7/api_symbols.csv"
if [[ ! -f "$API_SYMS" ]]; then
  echo "ERROR: api_symbols.csv not found at $API_SYMS" >&2
  exit 1
fi
SDK_VER=$(awk -F',' '$1=="Version"{print $3; exit}' "$API_SYMS")
SDK_MAJOR="${SDK_VER%.*}"
SDK_MINOR="${SDK_VER#*.}"

if [[ -z "$SDK_MAJOR" || -z "$SDK_MINOR" || "$SDK_MAJOR" == "$SDK_VER" ]]; then
  echo "ERROR: Could not parse SDK Version row from $API_SYMS (got '$SDK_VER')" >&2
  exit 1
fi

if [[ "$SDK_MAJOR" != "$DEVICE_MAJOR" ]] || [[ "$SDK_MINOR" != "$DEVICE_MINOR" ]]; then
  echo "API_MISMATCH: SDK=$SDK_MAJOR.$SDK_MINOR device=$DEVICE_MAJOR.$DEVICE_MINOR — re-pin SDK via fbt updater_package" >&2
  exit 1
fi

exit 0

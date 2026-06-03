#!/usr/bin/env bash
# Source this to activate Flipper0 ufbt toolchain.
# Usage: source personal/toolchain/ufbt-env.sh
FORK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [ ! -d "$FORK_DIR/.venv-ufbt" ]; then
  echo "ufbt-env: .venv-ufbt missing at $FORK_DIR — run Plan 03-01 setup."
  return 1
fi
# shellcheck disable=SC1091
. "$FORK_DIR/.venv-ufbt/bin/activate"
# UFBT_HOME currently unset; SDK lives in default ~/.ufbt (OFW 1.4.3)
# Future: when RM publishes SDK zip or we build it from source, set:
#   export UFBT_HOME="$FORK_DIR/.ufbt-rm0526"
unset UFBT_HOME 2>/dev/null
echo "ufbt-env: venv active; ufbt mode=channel/release (OFW 1.4.3). See personal/toolchain/SDK_PIN.json."

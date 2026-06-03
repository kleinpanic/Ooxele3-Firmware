#!/usr/bin/env bash
# Source this to activate Flipper0 ufbt toolchain.
# Usage: source personal/toolchain/ufbt-env.sh
# Compatible with bash and zsh (uses portable script-path detection).

# Detect script path in a shell-portable way
if [ -n "${BASH_SOURCE-}" ]; then
  __ufbt_env_self="${BASH_SOURCE[0]}"
elif [ -n "${ZSH_VERSION-}" ]; then
  # zsh: %x gives the current sourced script path
  __ufbt_env_self="${(%):-%x}"
else
  # POSIX fallback: $0 may give the script path when sourced
  __ufbt_env_self="$0"
fi

# Resolve to absolute fork dir (two levels up from personal/toolchain/)
__ufbt_env_dir="$(cd "$(dirname "$__ufbt_env_self")" && pwd -P)"
FORK_DIR="$(cd "$__ufbt_env_dir/../.." && pwd -P)"
unset __ufbt_env_self __ufbt_env_dir

if [ ! -d "$FORK_DIR/.venv-ufbt" ]; then
  echo "ufbt-env: .venv-ufbt missing at $FORK_DIR — run Plan 03-01 setup." >&2
  return 1 2>/dev/null || exit 1
fi

# shellcheck disable=SC1091
. "$FORK_DIR/.venv-ufbt/bin/activate"
export UFBT_HOME="$FORK_DIR/.ufbt-rm0526"
# Prepend personal/toolchain to PATH so the ufbt wrapper intercepts firmware-write subcommands (D3-12 Layer B)
export PATH="$FORK_DIR/personal/toolchain:$PATH"
echo "ufbt-env: venv active; mode=local; SDK=RM0526 0e7e0395; UFBT_HOME=$UFBT_HOME"

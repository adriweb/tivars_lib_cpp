#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

if ! command -v em++ >/dev/null 2>&1; then
    if [[ -n "${EMSDK_ENV:-}" && -f "$EMSDK_ENV" ]]; then
        # shellcheck disable=SC1090
        source "$EMSDK_ENV" >/dev/null 2>&1
    elif [[ -f "$HOME/emsdk/emsdk_env.sh" ]]; then
        # shellcheck disable=SC1091
        source "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1
    fi
fi

if ! command -v em++ >/dev/null 2>&1; then
    echo "em++ not found; source emsdk_env.sh first or set EMSDK_ENV=/path/to/emsdk_env.sh" >&2
    exit 127
fi

if ! command -v node >/dev/null 2>&1; then
    echo "node not found; Node.js is required to run the wasm tests" >&2
    exit 127
fi

make -C "$ROOT_DIR" -f Makefile.emscripten wasm-tests

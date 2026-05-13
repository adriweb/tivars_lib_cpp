#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TMP_DIR="${TMPDIR:-/tmp}/tivars_cli_smoke_tests"
mkdir -p "$TMP_DIR"

CLI="$ROOT_DIR/tivars_cli"

printf '\x00\x80\x10\x00\x00\x00\x00\x00\x00' > "$TMP_DIR/one_real_raw.bin"
"$CLI" -i "$TMP_DIR/one_real_raw.bin" -j raw -o "$TMP_DIR/one_real.8xn" -k varfile -t Real -n A
"$CLI" -i "$TMP_DIR/one_real.8xn" -j varfile -o "$TMP_DIR/one_real_roundtrip.bin" -k raw
cmp -s "$TMP_DIR/one_real_raw.bin" "$TMP_DIR/one_real_roundtrip.bin"

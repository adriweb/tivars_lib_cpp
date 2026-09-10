#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TMP_DIR="${TMPDIR:-/tmp}/tivars_cli_smoke_tests"
mkdir -p "$TMP_DIR"

CLI="${TIVARS_CLI:-$ROOT_DIR/tivars_cli}"

printf '\x00\x80\x10\x00\x00\x00\x00\x00\x00' > "$TMP_DIR/one_real_raw.bin"
"$CLI" -i "$TMP_DIR/one_real_raw.bin" -j raw -o "$TMP_DIR/one_real.8xn" -k varfile -t Real -n A
"$CLI" -i "$TMP_DIR/one_real.8xn" -j varfile -o "$TMP_DIR/one_real_roundtrip.bin" -k raw
cmp -s "$TMP_DIR/one_real_raw.bin" "$TMP_DIR/one_real_roundtrip.bin"

printf '1' > "$TMP_DIR/one_real.txt"
"$CLI" -i "$TMP_DIR/one_real.txt" -j readable -o "$TMP_DIR/one_real_83.83n" -k varfile -t Real -m 83 -n A
"$CLI" -i "$TMP_DIR/one_real.txt" -j readable -o "$TMP_DIR/one_real_default.8xn" -k varfile -t Real

printf '{1,2}\n' > "$TMP_DIR/list_with_newline.txt"
"$CLI" -i "$TMP_DIR/list_with_newline.txt" -j readable -o "$TMP_DIR/list_with_newline.8xl" -k varfile -t RealList -n L1

printf '{1E2}\n' > "$TMP_DIR/list_with_uppercase_exp.txt"
"$CLI" -i "$TMP_DIR/list_with_uppercase_exp.txt" -j readable -o "$TMP_DIR/list_with_uppercase_exp.8xl" -k varfile -t RealList -n L1

printf '[[1,2][3,4]]\n' > "$TMP_DIR/matrix_with_newline.txt"
"$CLI" -i "$TMP_DIR/matrix_with_newline.txt" -j readable -o "$TMP_DIR/matrix_with_newline.8xm" -k varfile -t Matrix -n A

# Header-only MPY fixture for container tests; not executable bytecode.
printf '%s\n' '{"python":{"compiledModule":true,"name":"demo","bodyHex":"4D05031F","menuDefinitionHex":"234D454E554C4142454C2044656D6F0A"}}' > "$TMP_DIR/module.txt"
"$CLI" -i "$TMP_DIR/module.txt" -o "$TMP_DIR/module.8xpy2" -t PythonAppVar -n DEMO -m 84Evo
"$CLI" -i "$TMP_DIR/module.8xpy2" -o "$TMP_DIR/module.8mp2" --python-format 8mp2
"$CLI" -i "$TMP_DIR/module.8mp2" -o "$TMP_DIR/module_resaved.8mp2"
cmp -s "$TMP_DIR/module.8mp2" "$TMP_DIR/module_resaved.8mp2"
"$CLI" -i "$TMP_DIR/module.8mp2" -o "$TMP_DIR/module_back.8xpy2" --python-format 8xpy2
"$CLI" -i "$TMP_DIR/module.8xpy2" -o "$TMP_DIR/module_raw.bin"
"$CLI" -i "$TMP_DIR/module_back.8xpy2" -o "$TMP_DIR/module_back_raw.bin"
cmp -s "$TMP_DIR/module_raw.bin" "$TMP_DIR/module_back_raw.bin"

printf 'print(42)\n' > "$TMP_DIR/source.txt"
"$CLI" -i "$TMP_DIR/source.txt" -o "$TMP_DIR/source.8xpy2" -t PythonAppVar -n SOURCE -m 84Evo
if "$CLI" -i "$TMP_DIR/source.8xpy2" -o "$TMP_DIR/source_rejected.8mp2" --python-format 8mp2; then
    echo "Source script incorrectly accepted as bytecode" >&2
    exit 1
fi
test ! -e "$TMP_DIR/source_rejected.8mp2"

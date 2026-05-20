#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TMP_DIR="${TMPDIR:-/tmp}/tivars_api_smoke_tests"
mkdir -p "$TMP_DIR"
TIVARS_GDB_DEFINE=()
if [[ -n "${TIVARS_API_GDB_SUPPORT+x}" ]]; then
    TIVARS_GDB_DEFINE=(-DTH_GDB_SUPPORT="$TIVARS_API_GDB_SUPPORT")
fi

cat > "$TMP_DIR/api_init_smoke.cpp" <<'CPP'
#include "src/TIModel.h"
#include "src/TIVarFile.h"
#include "src/TIVarType.h"

#include <cassert>

using namespace tivars;

int main()
{
    TIVarType realType{"Real"};
    TIModel ceModel{"84+CE"};
    TIVarFile realFile = TIVarFile::createNew("Real");

    assert(realType.getId() == 0x00);
    assert(ceModel.getName() == "84+CE");
    assert(realFile.getVarEntries()[0]._type.getName() == "Real");
    return 0;
}
CPP

cc -std=c2x -c "$ROOT_DIR/src/TypeHandlers/BuiltinTokensXml.c" -o "$TMP_DIR/BuiltinTokensXml.o"
COMMON_SOURCES=("$ROOT_DIR"/src/*.cpp "$ROOT_DIR"/src/TypeHandlers/*.cpp "$ROOT_DIR"/vendor/pugixml/pugixml.cpp)
c++ -std=c++2a "${TIVARS_GDB_DEFINE[@]}" -I"$ROOT_DIR" -I"$ROOT_DIR/vendor/pugixml" "${COMMON_SOURCES[@]}" "$TMP_DIR/BuiltinTokensXml.o" "$TMP_DIR/api_init_smoke.cpp" -o "$TMP_DIR/api_init_smoke"
"$TMP_DIR/api_init_smoke"

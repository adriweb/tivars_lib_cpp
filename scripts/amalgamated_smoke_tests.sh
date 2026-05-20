#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TMP_DIR="${TMPDIR:-/tmp}/tivars_amalgamated_smoke_tests"
mkdir -p "$TMP_DIR"

python3 "$ROOT_DIR/scripts/amalgamate.py"
TIVARS_GDB_DEFINE=()
if [[ -n "${TIVARS_SMOKE_GDB_SUPPORT+x}" ]]; then
    TIVARS_GDB_DEFINE=(-DTH_GDB_SUPPORT="$TIVARS_SMOKE_GDB_SUPPORT")
fi

cat > "$TMP_DIR/amalgamated_smoke.cpp" <<'CPP'
#include "tivars_lib_cpp.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

int main()
{
    auto prgm = tivars::TIVarFile::createNew("Program", "TEST");
    prgm.setContentFromString("ClrHome:Disp \"Hello World!\"");
    assert(prgm.getRawContent().size() > 2);
    assert(!prgm.getReadableContent().empty());

    auto real = tivars::TIVarFile::createNew("Real");
    real.setContentFromString("42");
    assert(real.getReadableContent() == "42");

#if TH_GDB_SUPPORT
    assert(tivars::TIVarTypes::fromName("GraphDataBase").getId() == 0x08);
#else
    bool threw = false;
    try
    {
        auto gdb = tivars::TIVarFile::createNew("GraphDataBase");
        (void)gdb.getReadableContent();
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    assert(threw);
#endif

    return 0;
}
CPP

c++ -std=c++20 "${TIVARS_GDB_DEFINE[@]}" -I"$ROOT_DIR/dist" \
    "$ROOT_DIR/dist/tivars_lib_cpp.cpp" \
    "$TMP_DIR/amalgamated_smoke.cpp" \
    -o "$TMP_DIR/amalgamated_smoke"

"$TMP_DIR/amalgamated_smoke"

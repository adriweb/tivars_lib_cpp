#!/usr/bin/env python3
#
# Generate the embeddable tivars_lib_cpp distribution files:
#   dist/tivars_lib_cpp.hpp
#   dist/tivars_lib_cpp.cpp

from __future__ import annotations

import re
import subprocess
import textwrap
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"

HEADER_FILES = [
    ROOT / "src/CommonTypes.h",
    ROOT / "src/TypeHandlers/TypeHandlers.h",
    ROOT / "src/TIVarType.h",
    ROOT / "src/TIModel.h",
    ROOT / "src/BinaryFile.h",
    ROOT / "src/TIVarFile.h",
    ROOT / "src/EvoFormat.h",
    ROOT / "src/TIModels.h",
    ROOT / "src/TIVarTypes.h",
    ROOT / "src/tivarslib_utils.h",
    ROOT / "src/TIFlashFile.h",
]

SOURCE_FILES = [
    ROOT / "src/BinaryFile.cpp",
    ROOT / "src/TIModel.cpp",
    ROOT / "src/TIModels.cpp",
    ROOT / "src/TIVarType.cpp",
    ROOT / "src/TIVarTypes.cpp",
    ROOT / "src/tivarslib_utils.cpp",
    ROOT / "src/TIFlashFile.cpp",
    ROOT / "src/EvoFormat.cpp",
    ROOT / "src/TIVarFile.cpp",
    ROOT / "src/TypeHandlers/DummyHandler.cpp",
    ROOT / "src/TypeHandlers/STH_DataAppVar.cpp",
    ROOT / "src/TypeHandlers/STH_ExactFraction.cpp",
    ROOT / "src/TypeHandlers/STH_ExactFractionPi.cpp",
    ROOT / "src/TypeHandlers/STH_ExactPi.cpp",
    ROOT / "src/TypeHandlers/STH_ExactRadical.cpp",
    ROOT / "src/TypeHandlers/STH_FP.cpp",
    ROOT / "src/TypeHandlers/STH_PythonAppVar.cpp",
    ROOT / "src/TypeHandlers/TH_Backup.cpp",
    ROOT / "src/TypeHandlers/TH_GDB.cpp",
    ROOT / "src/TypeHandlers/TH_GenericAppVar.cpp",
    ROOT / "src/TypeHandlers/TH_GenericComplex.cpp",
    ROOT / "src/TypeHandlers/TH_GenericList.cpp",
    ROOT / "src/TypeHandlers/TH_GenericReal.cpp",
    ROOT / "src/TypeHandlers/TH_Group.cpp",
    ROOT / "src/TypeHandlers/TH_Matrix.cpp",
    ROOT / "src/TypeHandlers/TH_Picture.cpp",
    ROOT / "src/TypeHandlers/TH_Settings.cpp",
    ROOT / "src/TypeHandlers/TH_StructuredAppVar.cpp",
    ROOT / "src/TypeHandlers/TH_TempEqu.cpp",
    ROOT / "src/TypeHandlers/TH_Tokenized.cpp",
    ROOT / "vendor/pugixml/pugixml.cpp",
]

VENDORED_HEADERS = [
    ROOT / "src/json.hpp",
    ROOT / "vendor/pugixml/pugiconfig.hpp",
    ROOT / "vendor/pugixml/pugixml.hpp",
]

PROJECT_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"\s*$')
PUGIXML_INCLUDE_RE = re.compile(r"^\s*#\s*include\s+<pugixml\.hpp>\s*$")
IDENTIFIER_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def banner(title: str) -> str:
    return f"\n\n// ===== {title} =====\n\n"


def relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def source_commit_sha() -> str:
    try:
        return subprocess.check_output(
            ["git", "-C", str(ROOT), "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def strip_project_includes(text: str, source_path: Path) -> str:
    output: list[str] = []
    for line in text.splitlines():
        local_include = PROJECT_INCLUDE_RE.match(line)
        if local_include:
            include_name = local_include.group(1)
            if include_name == "EvoTokens.inc":
                output.append((ROOT / "src/EvoTokens.inc").read_text())
            else:
                output.append(f"// amalgamated: removed local include {include_name!r}")
            continue
        if PUGIXML_INCLUDE_RE.match(line):
            output.append("// amalgamated: pugixml.hpp is included above")
            continue
        output.append(line)
    return "\n".join(output).rstrip() + "\n"


def count_braces_ignoring_literals(line: str) -> int:
    depth = 0
    i = 0
    in_string = False
    in_char = False
    escaped = False

    while i < len(line):
        ch = line[i]
        nxt = line[i + 1] if i + 1 < len(line) else ""

        if in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
            i += 1
            continue

        if in_char:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == "'":
                in_char = False
            i += 1
            continue

        if ch == "/" and nxt == "/":
            break
        if ch == '"':
            in_string = True
        elif ch == "'":
            in_char = True
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
        i += 1

    return depth


def collect_top_level_names(line: str) -> set[str]:
    names: set[str] = set()
    stripped = line.strip()
    if not stripped or stripped.startswith("//"):
        return names

    for pattern in (
        r"\b(?:struct|class|enum(?:\s+class)?)\s+([A-Za-z_][A-Za-z0-9_]*)",
        r"\bu8enum\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)",
    ):
        names.update(re.findall(pattern, line))

    function = re.search(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:->[^{}]+)?(?:\{|$)", line)
    if function and function.group(1) not in {"if", "for", "while", "switch", "catch", "return", "sizeof", "u8enum"}:
        names.add(function.group(1))

    static_variable = re.search(r"^\s*static\b.*\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{|;)", line)
    if static_variable:
        names.add(static_variable.group(1))

    variable = re.search(
        r"^\s*(?:(?:inline|static|const|constexpr)\s+)*(?:[A-Za-z_][A-Za-z0-9_:<>]*[\s*&]+)+([A-Za-z_][A-Za-z0-9_]*)\s*(?:=|\{|;)",
        line,
    )
    if variable:
        names.add(variable.group(1))

    return names


def anonymous_namespace_ranges(lines: list[str]) -> list[tuple[int, int]]:
    ranges: list[tuple[int, int]] = []
    index = 0
    while index < len(lines):
        line = lines[index]
        namespace_line = re.match(r"^\s*namespace\s*(?://.*)?$", line)
        namespace_brace_line = re.match(r"^\s*namespace\s*\{\s*(?://.*)?$", line)

        if namespace_line and index + 1 < len(lines) and lines[index + 1].strip() == "{":
            start = index
            index += 2
            depth = 1
            while index < len(lines):
                depth += count_braces_ignoring_literals(lines[index])
                index += 1
                if depth == 0:
                    ranges.append((start, index - 1))
                    break
            continue

        if namespace_brace_line:
            start = index
            index += 1
            depth = 1
            while index < len(lines):
                depth += count_braces_ignoring_literals(lines[index])
                index += 1
                if depth == 0:
                    ranges.append((start, index - 1))
                    break
            continue

        index += 1
    return ranges


def collect_internal_names(text: str) -> set[str]:
    lines = text.splitlines()
    names: set[str] = set()

    for start, end in anonymous_namespace_ranges(lines):
        depth = 0
        open_seen = False
        for line in lines[start : end + 1]:
            if not open_seen:
                if "{" in line:
                    open_seen = True
                    depth += count_braces_ignoring_literals(line)
                continue
            if depth == 1:
                names.update(collect_top_level_names(line))
            depth += count_braces_ignoring_literals(line)

    for line in lines:
        if re.match(r"^\s*static\s+", line):
            names.update(collect_top_level_names(line))

    return {name for name in names if name not in {"namespace", "to_json", "from_json"}}


def replace_identifiers(text: str, replacements: dict[str, str]) -> str:
    if not replacements:
        return text

    result: list[str] = []
    i = 0
    in_block_comment = False

    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if in_block_comment:
            result.append(ch)
            if ch == "*" and nxt == "/":
                result.append(nxt)
                i += 2
                in_block_comment = False
            else:
                i += 1
            continue

        if ch == "/" and nxt == "*":
            result.append(ch)
            result.append(nxt)
            i += 2
            in_block_comment = True
            continue

        if ch == "/" and nxt == "/":
            newline = text.find("\n", i)
            if newline == -1:
                result.append(text[i:])
                break
            result.append(text[i:newline])
            i = newline
            continue

        if ch in {'"', "'"}:
            quote = ch
            start = i
            i += 1
            escaped = False
            while i < len(text):
                current = text[i]
                if escaped:
                    escaped = False
                elif current == "\\":
                    escaped = True
                elif current == quote:
                    i += 1
                    break
                i += 1
            result.append(text[start:i])
            continue

        match = IDENTIFIER_RE.match(text, i)
        if match:
            name = match.group(0)
            result.append(replacements.get(name, name))
            i = match.end()
            continue

        result.append(ch)
        i += 1

    return "".join(result)


def uniquify_internal_names(text: str, source_path: Path) -> str:
    unit = re.sub(r"[^A-Za-z0-9_]", "_", relative(source_path).replace("/", "__"))
    replacements = {
        name: f"{name}_tivars_amalgamated_{unit}"
        for name in collect_internal_names(text)
    }
    return replace_identifiers(text, replacements)


def rewrite_anonymous_namespaces(text: str, source_path: Path) -> str:
    lines = text.splitlines()
    output: list[str] = []
    index = 0
    namespace_index = 0
    unit = re.sub(r"[^A-Za-z0-9_]", "_", relative(source_path).replace("/", "__"))

    while index < len(lines):
        line = lines[index]
        namespace_line = re.match(r"^(\s*)namespace\s*(?://.*)?$", line)
        namespace_brace_line = re.match(r"^(\s*)namespace\s*\{\s*(?://.*)?$", line)

        if namespace_line and index + 1 < len(lines) and lines[index + 1].strip() == "{":
            indent = namespace_line.group(1)
            unique = f"tivars_amalgamated_{unit}_{namespace_index}"
            namespace_index += 1
            output.append(f"{indent}namespace {unique}")
            output.append(lines[index + 1])
            index += 2
            depth = 1
            while index < len(lines):
                current = lines[index]
                output.append(current)
                depth += count_braces_ignoring_literals(current)
                index += 1
                if depth == 0:
                    output.append(f"{indent}using namespace {unique};")
                    break
            continue

        if namespace_brace_line:
            indent = namespace_brace_line.group(1)
            unique = f"tivars_amalgamated_{unit}_{namespace_index}"
            namespace_index += 1
            output.append(f"{indent}namespace {unique} {{")
            index += 1
            depth = 1
            while index < len(lines):
                current = lines[index]
                output.append(current)
                depth += count_braces_ignoring_literals(current)
                index += 1
                if depth == 0:
                    output.append(f"{indent}using namespace {unique};")
                    break
            continue

        output.append(line)
        index += 1

    return "\n".join(output).rstrip() + "\n"


def xml_byte_array() -> str:
    data = (ROOT / "ti-toolkit-8x-tokens.xml").read_bytes()
    rows = []
    for offset in range(0, len(data), 16):
        chunk = data[offset : offset + 16]
        rows.append("    " + ", ".join(f"0x{byte:02X}" for byte in chunk) + ",")
    return "\n".join(
        [
            'extern "C"',
            "{",
            "extern const unsigned char tivars_builtin_tokens_xml[] = {",
            *rows,
            "};",
            "extern const size_t tivars_builtin_tokens_xml_size = sizeof(tivars_builtin_tokens_xml);",
            "}",
            "",
        ]
    )


def generate_header() -> str:
    commit_sha = source_commit_sha()
    parts = [
        textwrap.dedent(
            f"""\
            /*
             * Single-include public header for tivars_lib_cpp.
             * Generated by scripts/amalgamate.py. Do not edit manually.
             * Source commit: {commit_sha}
             */

            #ifndef TIVARS_LIB_CPP_AMALGAMATED_HPP
            #define TIVARS_LIB_CPP_AMALGAMATED_HPP
            """
        )
    ]
    for header in HEADER_FILES:
        parts.append(banner(relative(header)))
        parts.append(strip_project_includes(header.read_text(), header))
    parts.append("\n#endif // TIVARS_LIB_CPP_AMALGAMATED_HPP\n")
    return "".join(parts)


def generate_source() -> str:
    commit_sha = source_commit_sha()
    parts = [
        textwrap.dedent(
            f"""\
            /*
             * Single-translation-unit implementation for tivars_lib_cpp.
             * Generated by scripts/amalgamate.py. Do not edit manually.
             * Source commit: {commit_sha}
             */

            #include "tivars_lib_cpp.hpp"
            """
        )
    ]
    for header in VENDORED_HEADERS:
        parts.append(banner(relative(header)))
        parts.append(strip_project_includes(header.read_text(), header))
    parts.append(banner("ti-toolkit-8x-tokens.xml"))
    parts.append(xml_byte_array())
    for source in SOURCE_FILES:
        parts.append(banner(relative(source)))
        source_text = strip_project_includes(source.read_text(), source)
        if "vendor/pugixml" not in relative(source):
            source_text = uniquify_internal_names(source_text, source)
            source_text = rewrite_anonymous_namespaces(source_text, source)
        parts.append(source_text)
    return "".join(parts)


def main() -> None:
    DIST.mkdir(exist_ok=True)
    (DIST / "tivars_lib_cpp.hpp").write_text(generate_header())
    (DIST / "tivars_lib_cpp.cpp").write_text(generate_source())
    print("Generated dist/tivars_lib_cpp.hpp")
    print("Generated dist/tivars_lib_cpp.cpp")


if __name__ == "__main__":
    main()

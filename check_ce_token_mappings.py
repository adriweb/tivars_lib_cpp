#!/usr/bin/env python3
import argparse
import re
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parent
TOKENS_XML = ROOT / "ti-toolkit-8x-tokens.xml"
EVO_FORMAT = ROOT / "src" / "EvoFormat.cpp"


def hex_word(value):
    return f"0x{value:04X}"


def extract_function(source, name):
    header = re.search(rf"static\s+bool\s+{name}\s*\([^)]*\)\s*\{{", source)
    if not header:
        raise RuntimeError(f"Could not find {name}")

    brace = header.end() - 1
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]

    raise RuntimeError(f"Could not find end of {name}")


def token_label(token_element):
    for lang in token_element.findall(".//lang"):
        if lang.get("code") == "en":
            display = lang.get("display")
            accessible = lang.findtext("accessible")
            if display:
                return display
            if accessible:
                return accessible
    return "(unnamed)"


def parse_ce_tokens(path):
    root = ET.fromstring(path.read_text())
    tokens = []
    for child in root:
        if child.tag == "token":
            tokens.append((int(child.get("value")[1:], 16), token_label(child)))
        elif child.tag == "two-byte":
            high = int(child.get("value")[1:], 16)
            for token in child.findall("token"):
                low = int(token.get("value")[1:], 16)
                tokens.append(((high << 8) | low, token_label(token)))
    return sorted(tokens)


def parse_direct_map(function_body):
    map_match = re.search(
        r"static\s+const\s+std::unordered_map<[^>]+>\s+direct\s*=\s*\{(?P<body>.*?)\n\s*\};",
        function_body,
        re.S,
    )
    if not map_match:
        return []

    pair_pattern = re.compile(r"\{\s*(0x[0-9A-Fa-f]+)\s*,\s*(0x[0-9A-Fa-f]+)\s*\}")
    return [
        (int(left, 16), int(right, 16))
        for left, right in pair_pattern.findall(map_match.group("body"))
    ]


def parse_range_maps(function_body, source_var, dest_var):
    range_pattern = re.compile(
        rf"if\s*\(\s*{source_var}\s*>=\s*(0x[0-9A-Fa-f]+)\s*&&\s*"
        rf"{source_var}\s*<=\s*(0x[0-9A-Fa-f]+)\s*\)\s*\{{(?P<body>.*?)return\s+true\s*;",
        re.S,
    )
    assignment_pattern = re.compile(
        rf"{dest_var}\s*=\s*static_cast<uint16_t>\(\s*"
        rf"(0x[0-9A-Fa-f]+)\s*\+\s*\(\s*{source_var}\s*-\s*"
        rf"(0x[0-9A-Fa-f]+)\s*\)\s*\)"
    )

    pairs = []
    for match in range_pattern.finditer(function_body):
        assign = assignment_pattern.search(match.group("body"))
        if not assign:
            continue

        source_first = int(match.group(1), 16)
        source_last = int(match.group(2), 16)
        dest_base = int(assign.group(1), 16)
        source_base = int(assign.group(2), 16)

        for source in range(source_first, source_last + 1):
            pairs.append((source, dest_base + (source - source_base)))
    return pairs


def build_mappings(format_path):
    source = format_path.read_text()

    evo_to_legacy_body = extract_function(source, "direct_legacy_token_for_evo")
    legacy_to_evo_body = extract_function(source, "direct_evo_token_for_legacy")

    evo_to_legacy = dict(parse_range_maps(evo_to_legacy_body, "evoToken", "legacyToken"))
    evo_to_legacy.update(parse_direct_map(evo_to_legacy_body))

    legacy_to_evo = dict(parse_range_maps(legacy_to_evo_body, "legacyToken", "evoToken"))
    legacy_to_evo.update(parse_direct_map(legacy_to_evo_body))

    return evo_to_legacy, legacy_to_evo


def print_section(title, rows):
    print(f"{title}: {len(rows)}")
    for token, name in rows:
        print(f"  {hex_word(token)} {name}")


def compact_ranges(rows):
    if not rows:
        return []

    grouped = []
    start_token, start_name = rows[0]
    prev_token, prev_name = rows[0]
    for token, name in rows[1:]:
        if token == prev_token + 1:
            prev_token, prev_name = token, name
            continue

        grouped.append((start_token, start_name, prev_token, prev_name))
        start_token, start_name = token, name
        prev_token, prev_name = token, name

    grouped.append((start_token, start_name, prev_token, prev_name))
    return grouped


def print_range_section(title, rows):
    print(f"{title}: {len(rows)} tokens in {len(compact_ranges(rows))} ranges")
    for first_token, first_name, last_token, last_name in compact_ranges(rows):
        if first_token == last_token:
            print(f"  {hex_word(first_token)} {first_name}")
        else:
            print(f"  {hex_word(first_token)}..{hex_word(last_token)} {first_name}..{last_name}")


def main():
    parser = argparse.ArgumentParser(
        description="Check ti-toolkit-8x-tokens.xml CE token coverage in CE<->Evo token conversion mappings."
    )
    parser.add_argument(
        "--ranges",
        action="store_true",
        help="Print missing tokens as compact contiguous ranges instead of one token per line.",
    )
    args = parser.parse_args()

    tokens = parse_ce_tokens(TOKENS_XML)
    evo_to_legacy, legacy_to_evo = build_mappings(EVO_FORMAT)
    evo_to_legacy_values = set(evo_to_legacy.values())

    missing_legacy_to_evo = [
        (token, name) for token, name in tokens if token not in legacy_to_evo
    ]
    missing_evo_to_legacy = [
        (token, name) for token, name in tokens if token not in evo_to_legacy_values
    ]

    print(f"XML CE tokens checked: {len(tokens)}")
    print(f"CE->Evo mapped CE tokens: {len(legacy_to_evo)}")
    print(f"Evo->CE produced CE tokens: {len(evo_to_legacy_values)}")
    print()
    section_printer = print_range_section if args.ranges else print_section
    section_printer("Missing from CE->Evo mapping", missing_legacy_to_evo)
    print()
    section_printer("Not produced by Evo->CE mapping", missing_evo_to_legacy)

    return 1 if missing_legacy_to_evo or missing_evo_to_legacy else 0


if __name__ == "__main__":
    raise SystemExit(main())

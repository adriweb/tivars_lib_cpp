#!/usr/bin/env python3
"""
Merge French token display names from programs_tokens.csv into ti-toolkit-8x-tokens.xml.

This utility reads the CSV (legacy data) to retrieve the French display strings
and inserts or updates <lang code="fr" ...> entries in the XML for the matching tokens
identified by their byte codes.

Usage:
  python scripts/merge_fr_tokens.py \
    --csv programs_tokens.csv \
    --xml-in ti-toolkit-8x-tokens.xml \
    [--xml-out ti-toolkit-8x-tokens.fr.xml] \
    [--in-place] [--backup .bak] [--only-missing] [--dry-run]

Defaults:
  --csv      defaults to ./programs_tokens.csv
  --xml-in   defaults to ./ti-toolkit-8x-tokens.xml
  If neither --xml-out nor --in-place is specified, the script prints a summary
  and performs a dry run without writing.

Notes:
  - Tokens are matched by their bytes (Byte 1 and optional Byte 2) from the CSV.
  - For each token, the script adds/updates the <lang code="fr"> element inside the last
    <version> node of the token definition, setting the display attribute to the
    CSV "Readable Name (FR)" value and adding (or updating) an <accessible> child
    with the same text.
  - If --only-missing is provided, existing French entries are left unchanged.
"""

from __future__ import annotations

import argparse
import csv
import os
import sys
import shutil
from typing import Dict, Tuple, Optional, List
import xml.etree.ElementTree as ET


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Merge FR tokens from CSV into tokens XML")
    p.add_argument("--csv", dest="csv_path", default="programs_tokens.csv",
                   help="Path to programs_tokens.csv (default: programs_tokens.csv)")
    p.add_argument("--xml-in", dest="xml_in", default="ti-toolkit-8x-tokens.xml",
                   help="Path to input tokens XML (default: ti-toolkit-8x-tokens.xml)")
    p.add_argument("--xml-out", dest="xml_out", default=None,
                   help="Path to write output XML (if omitted, use --in-place or dry-run)")
    p.add_argument("--in-place", dest="in_place", action="store_true",
                   help="Modify the input XML file in place (with optional --backup)")
    p.add_argument("--backup", dest="backup_ext", default=".bak",
                   help="Backup extension when using --in-place (default: .bak; empty to disable)")
    p.add_argument("--only-missing", dest="only_missing", action="store_true",
                   help="Only add FR entries when missing; do not update existing ones")
    p.add_argument("--dry-run", dest="dry_run", action="store_true",
                   help="Do not write any file; just report planned changes")
    p.add_argument("--no-safe-write", dest="no_safe_write", action="store_true",
                   help="Disable safe writer. WARNING: standard XML serialization may normalize entities (e.g., turn &#032; into space). Use only if you accept that risk.")
    return p.parse_args()


def hexstr_to_int(s: str) -> Optional[int]:
    s = (s or "").strip()
    if not s:
        return None
    try:
        return int(s, 16)
    except ValueError:
        return None


def load_csv_fr_tokens(csv_path: str) -> Dict[Tuple[int, Optional[int]], str]:
    """
    Returns a map: (b1, b2_or_None) -> fr_display
    Skips rows with empty FR.
    """
    mapping: Dict[Tuple[int, Optional[int]], str] = {}
    with open(csv_path, "r", encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)
        # Expected headers include: "# of bytes", "Byte 1", "Byte 2", "Readable Name (FR)"
        for row in reader:
            # IMPORTANT: do NOT trim; keep trailing/leading spaces from CSV exactly as-is
            fr = (row.get("Readable Name (FR)") or "")
            if not fr:
                continue
            # Do not attempt to map the size header row etc.
            b1 = hexstr_to_int(row.get("Byte 1", ""))
            if b1 is None:
                continue
            b2 = hexstr_to_int(row.get("Byte 2", ""))
            # Some CSV lines mark 1-byte tokens with "# of bytes" == 1; we ignore it if bytes parse correctly.
            mapping[(b1, b2)] = fr
    return mapping


def find_token_node(root: ET.Element, b1: int, b2: Optional[int]) -> Optional[ET.Element]:
    """
    Find the token XML node corresponding to the given bytes.
    - For 1-byte tokens: <tokens>/<token value="$XX">
    - For 2-byte tokens: <tokens>/<two-byte value="$B1">/<token value="$B2">
    Returns the <token> element or None if not found.
    """
    if b2 is None:
        # Ensure we only match top-level tokens, not nested under two-byte.
        value = f"${b1:02X}"
        for tok in root.findall("./token[@value]"):
            if tok.get("value", "").upper() == value:
                return tok
        return None
    else:
        val1 = f"${b1:02X}"
        val2 = f"${b2:02X}"
        for group in root.findall("./two-byte[@value]"):
            if group.get("value", "").upper() != val1:
                continue
            for tok in group.findall("./token[@value]"):
                if tok.get("value", "").upper() == val2:
                    return tok
        return None


def _pick_lang_display(token_node: ET.Element, code: str) -> str:
    """Replicates the library's heuristic to choose a display string for a language.

    Preference:
      - Iterate <version> in document order; keep last non-empty value encountered.
      - If any encountered value ends with '(', prefer that one.
      - Value source: @display if present/non-empty, else text of first <accessible> child.
    Returns empty string if none found.
    """
    last_val = ""
    preferred_paren = ""
    for ver in token_node.findall("./version"):
        for lang in ver.findall("./lang"):
            if (lang.get("code") or "").lower() != code.lower():
                continue
            # ElementTree decodes entities (e.g., &#032; -> ' '), which is fine for comparisons.
            disp = lang.get("display") or ""
            # Do NOT strip; preserve any trailing spaces.
            val = disp
            if not val:
                acc = lang.find("accessible")
                if acc is not None and acc.text:
                    # Do NOT strip; preserve trailing spaces
                    val = acc.text
            if val:
                last_val = val
                if not preferred_paren and val.endswith("("):
                    preferred_paren = val
    return preferred_paren or last_val


def ensure_fr_lang(token_node: ET.Element, fr_display: str, only_missing: bool) -> Tuple[bool, bool]:
    """
    Ensure the last <version> of token_node has a <lang code="fr"> with display and accessible.
    - Never modifies any 'en' data.
    - If the provided French display equals the English preferred display, skip entirely (added=False, updated=False),
      allowing consumers to fall back to English.
    - Additionally skip if the French display equals any English <variant> or <accessible> value already present.
    Returns (added, updated).
    """
    # Rule: skip if FR equals EN (fallback behavior desired)
    en_display = _pick_lang_display(token_node, "en")
    if en_display and fr_display == en_display:
        return False, False

    # Additional rule: skip if FR equals any English <variant> or <accessible> value
    for ver in token_node.findall("./version"):
        for lang in ver.findall("./lang"):
            if (lang.get("code") or "").lower() != "en":
                continue
            for tag in ("variant", "accessible"):
                for v in lang.findall(tag):
                    # Do NOT strip; compare raw text including any spaces
                    vtext = (v.text or "")
                    if vtext and vtext == fr_display:
                        return False, False

    versions = list(token_node.findall("./version"))
    if versions:
        ver = versions[-1]
    else:
        # Create a version if none exist (unlikely but safe)
        ver = ET.SubElement(token_node, "version")

    # Look for direct child lang with code="fr"
    fr_lang = None
    for lang in ver.findall("./lang"):
        if (lang.get("code") or "").lower() == "fr":
            fr_lang = lang
            break

    if fr_lang is None:
        fr_lang = ET.SubElement(ver, "lang", {"code": "fr", "display": fr_display})
        acc = ET.SubElement(fr_lang, "accessible")
        acc.text = fr_display
        return True, False
    else:
        # Already exists
        old_display = fr_lang.get("display", "")
        has_acc = fr_lang.find("accessible") is not None
        if only_missing and old_display:
            return False, False
        updated = False
        if old_display != fr_display:
            fr_lang.set("display", fr_display)
            updated = True
        if not has_acc:
            acc = ET.SubElement(fr_lang, "accessible")
            acc.text = fr_display
            updated = True
        else:
            acc = fr_lang.find("accessible")
            if (acc.text or "") != fr_display and not only_missing:
                acc.text = fr_display
                updated = True
        return False, updated


def _encode_trailing_spaces(s: str) -> str:
    """Replace trailing spaces with XML numeric entities (&#032;) repeated.

    Example: "RegLinTTest " -> "RegLinTTest&#032;"
    """
    if not s:
        return s
    # Count trailing spaces
    i = len(s)
    while i > 0 and s[i-1] == ' ':
        i -= 1
    if i == len(s):
        return s
    return s[:i] + ("&#032;" * (len(s) - i))


def _xml_escape_for_attr(s: str) -> str:
    """Minimal XML escaping for attribute values, without trimming and preserving trailing spaces via &#032;"""
    s = s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")
    s = _encode_trailing_spaces(s)
    return s


def _xml_escape_for_text(s: str) -> str:
    """Minimal XML escaping for text nodes, without trimming and preserving trailing spaces via &#032;"""
    s = s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    s = _encode_trailing_spaces(s)
    return s


def _insert_fr_safely(xml_text: str,
                      additions: List[Tuple[int, Optional[int], str]],
                      updates: List[Tuple[int, Optional[int], str]] = None) -> str:
    """Perform minimal, textual insertions of FR <lang> nodes without touching unrelated XML.

    additions: list of (b1, b2_or_None, fr_display) for which FR is missing and must be added
    Returns the updated XML text.

    Strategy:
      - For 2-byte tokens: locate the <two-byte value="$B1"> group, then the inner <token value="$B2"> block
        by regex, capture that token block span, and insert the <lang code="fr" ...> before the last </version>.
      - For 1-byte tokens: locate the <token value="$B1"> block similarly and insert before the last </version>.
      - Indentation: reuse the indentation of the first existing <lang> in the last <version>; if none, use a single tab more than the <version> line.
    Limitations: this function only adds missing FR entries; it does not update existing FR (by design to avoid touching existing content).
    """
    import re

    text = xml_text

    def make_fr_snippet(indent: str, fr: str) -> str:
        # Insert on a single line for stability, with proper escaping and preserved trailing spaces
        fr_attr = _xml_escape_for_attr(fr)
        fr_text = _xml_escape_for_text(fr)
        return f"{indent}<lang code=\"fr\" display=\"{fr_attr}\"><accessible>{fr_text}</accessible></lang>"

    for b1, b2, fr in additions:
        # IMPORTANT: recompute two-byte section spans for the current text state.
        # Prior insertions shift indices; stale spans can misclassify matches.
        two_byte_spans: List[Tuple[int, int]] = []
        for m in re.finditer(r"<two-byte\s+value=\"\$[0-9A-Fa-f]{2}\">[\s\S]*?</two-byte>", text):
            two_byte_spans.append((m.start(), m.end()))

        def inside_two_byte(pos: int) -> bool:
            for a, b in two_byte_spans:
                if a <= pos < b:
                    return True
            return False
        if b2 is None:
            # Single-byte token block (must be top-level, not inside <two-byte>)
            tok_pat = re.compile(r'(\n?\s*<token\s+value="\$%02X">)([\s\S]*?</token>)' % b1, re.IGNORECASE)
            # Iterate all matches and pick the first that's not inside a two-byte group
            m = None
            for candidate in tok_pat.finditer(text):
                if not inside_two_byte(candidate.start(0)):
                    m = candidate
                    break
            if not m:
                continue
            block_start = m.start(2)
            block_end = m.end(2)
            block = text[block_start:block_end]
        else:
            # Two-byte token block: find group first, then token
            group_pat = re.compile(r'(<two-byte\s+value="\$%02X">)([\s\S]*?</two-byte>)' % b1, re.IGNORECASE)
            gm = group_pat.search(text)
            if not gm:
                continue
            group_body_start = gm.start(2)
            group_body_end = gm.end(2)
            group_body = text[group_body_start:group_body_end]

            tok_pat = re.compile(r'(<token\s+value="\$%02X">)([\s\S]*?</token>)' % b2, re.IGNORECASE)
            tm = tok_pat.search(group_body)
            if not tm:
                continue
            # Map back to full text indices
            inner_block_rel_start = tm.start(2)
            inner_block_rel_end = tm.end(2)
            block_start = group_body_start + inner_block_rel_start
            block_end = group_body_start + inner_block_rel_end
            block = text[block_start:block_end]

        # Within the token block, find the last </version>
        last_ver_close = block.rfind("</version>")
        if last_ver_close == -1:
            # Fallback: append just before </token>
            insert_at = block.rfind("</token>")
            if insert_at == -1:
                continue
            # Determine indentation
            # Find preceding line's indentation
            line_start = block.rfind('\n', 0, insert_at) + 1
            indent = block[line_start:insert_at]
            indent = indent[:len(indent) - len(indent.lstrip())]
            snippet = "\n" + make_fr_snippet(indent, fr)
            new_block = block[:insert_at] + snippet + block[insert_at:]
        else:
            # Determine indentation based on existing langs within the last version
            ver_open = block.rfind("<version", 0, last_ver_close)
            ver_content = block[ver_open:last_ver_close]
            # Look for an existing <lang ...> to reuse its indent
            m_lang = re.search(r"\n(\s*)<lang\\b", ver_content)
            if m_lang:
                indent = m_lang.group(1)
            else:
                # otherwise, one level deeper than <version>
                m_ver_line = re.search(r"\n(\s*)<version\\b", block)
                base = (m_ver_line.group(1) if m_ver_line else "\t")
                indent = base + "\t"
            snippet = "\n" + make_fr_snippet(indent, fr)
            insert_at = last_ver_close
            new_block = block[:insert_at] + snippet + block[insert_at:]

        # Replace in text
        text = text[:block_start] + new_block + text[block_end:]

    # Handle updates: replace existing FR display/accessible text for matching tokens
    updates = updates or []

    def replace_fr_in_block(block_text: str, fr_new: str) -> str:
        # Replace display attribute and accessible text inside the FIRST <lang code="fr" ...> within the block
        import re
        fr_attr = _xml_escape_for_attr(fr_new)
        fr_text = _xml_escape_for_text(fr_new)
        # Replace display attribute value
        block_text = re.sub(r'(\<lang\s+[^>]*?code="fr"[^>]*?display=")([^"]*)(")',
                            lambda m: m.group(1) + fr_attr + m.group(3),
                            block_text, count=1, flags=re.IGNORECASE)
        # Replace accessible inner text for the same fr lang node; best-effort within the block
        # We try to find the first <lang code="fr" ...> ... </lang> range and then replace its first <accessible>text</accessible>
        m_lang = re.search(r'(<lang\s+[^>]*?code="fr"[^>]*>)([\s\S]*?)(</lang>)', block_text, flags=re.IGNORECASE)
        if m_lang:
            inner = m_lang.group(2)
            inner_new = re.sub(r'(<accessible>)([\s\S]*?)(</accessible>)',
                               lambda m: m.group(1) + fr_text + m.group(3),
                               inner, count=1, flags=re.IGNORECASE)
            block_text = block_text[:m_lang.start(2)] + inner_new + block_text[m_lang.end(2):]
        return block_text

    for b1, b2, fr in updates:
        # Recompute two-byte spans each iteration to keep indices correct
        two_byte_spans = []
        for m in re.finditer(r"<two-byte\s+value=\"\$[0-9A-Fa-f]{2}\">[\s\S]*?</two-byte>", text):
            two_byte_spans.append((m.start(), m.end()))

        def inside_two_byte(pos: int) -> bool:
            for a, b in two_byte_spans:
                if a <= pos < b:
                    return True
            return False

        if b2 is None:
            tok_pat = re.compile(r'(\n?\s*<token\s+value=\"\$%02X\">)([\s\S]*?</token>)' % b1, re.IGNORECASE)
            m = None
            for candidate in tok_pat.finditer(text):
                if not inside_two_byte(candidate.start(0)):
                    m = candidate
                    break
            if not m:
                continue
            block_start = m.start(2)
            block_end = m.end(2)
            block = text[block_start:block_end]
        else:
            group_pat = re.compile(r'(<two-byte\s+value=\"\$%02X\">)([\s\S]*?</two-byte>)' % b1, re.IGNORECASE)
            gm = group_pat.search(text)
            if not gm:
                continue
            group_body_start = gm.start(2)
            group_body_end = gm.end(2)
            group_body = text[group_body_start:group_body_end]

            tok_pat = re.compile(r'(<token\s+value=\"\$%02X\">)([\s\S]*?</token>)' % b2, re.IGNORECASE)
            tm = tok_pat.search(group_body)
            if not tm:
                continue
            inner_block_rel_start = tm.start(2)
            inner_block_rel_end = tm.end(2)
            block_start = group_body_start + inner_block_rel_start
            block_end = group_body_start + inner_block_rel_end
            block = text[block_start:block_end]

        new_block = replace_fr_in_block(block, fr)
        text = text[:block_start] + new_block + text[block_end:]

    return text


def main() -> int:
    args = parse_args()

    if not os.path.isfile(args.csv_path):
        print(f"[ERROR] CSV not found: {args.csv_path}", file=sys.stderr)
        return 2
    if not os.path.isfile(args.xml_in):
        print(f"[ERROR] XML not found: {args.xml_in}", file=sys.stderr)
        return 2

    fr_map = load_csv_fr_tokens(args.csv_path)
    print(f"Loaded {len(fr_map)} FR entries from CSV")

    tree = ET.parse(args.xml_in)
    root = tree.getroot()
    if root.tag != "tokens":
        print("[ERROR] Root element is not <tokens>", file=sys.stderr)
        return 2

    added = 0
    updated = 0
    missing = 0
    processed = 0

    for (b1, b2), fr in fr_map.items():
        token_node = find_token_node(root, b1, b2)
        if token_node is None:
            missing += 1
            b1_str = f"{b1:02X}" if b1 is not None else ""
            b2_str = f"{b2:02X}" if b2 is not None else ""
            print(f"[WARN] No token found for {b1_str}{b2_str} ({fr})")
            continue
        a, u = ensure_fr_lang(token_node, fr, args.only_missing)
        if a:
            added += 1
        if u:
            updated += 1
        processed += 1

    print(f"Processed: {processed} tokens; Added FR: {added}; Updated FR: {updated}; Missing in XML: {missing}")

    # Decide output
    if args.dry_run or (not args.in_place and not args.xml_out):
        print("Dry run: no files written. Use --in-place or --xml-out to write changes.")
        return 0

    out_path = args.xml_in if args.in_place and not args.xml_out else (args.xml_out or args.xml_in)

    # Backup if in-place
    if args.in_place and args.backup_ext:
        backup_path = args.xml_in + args.backup_ext
        try:
            shutil.copy2(args.xml_in, backup_path)
            print(f"Backup written: {backup_path}")
        except Exception as e:
            print(f"[WARN] Could not create backup: {e}", file=sys.stderr)

    if args.no_safe_write:
        # WARNING PATH: standard serializer can normalize entities
        tree.write(out_path, encoding="utf-8", xml_declaration=True)
        print(f"Wrote (standard serializer): {out_path}")
        return 0

    # Safe write path: only add missing FR entries via textual insertion to preserve original entities
    if not args.only_missing:
        print("[WARN] --only-missing not specified; safe writer only adds missing FR entries and will not update existing ones.")

    # Build list of additions (missing only) and updates (existing FR but value differs)
    additions: List[Tuple[int, Optional[int], str]] = []
    updates: List[Tuple[int, Optional[int], str]] = []
    # IMPORTANT: Re-parse the original XML from disk to avoid using a tree potentially mutated
    # by the earlier ensure_fr_lang() calls. This guarantees we only add FR entries that are
    # actually missing from the file on disk.
    try:
        orig_tree = ET.parse(args.xml_in)
        orig_root = orig_tree.getroot()
    except Exception as e:
        print(f"[ERROR] Could not re-read XML for safe write: {e}", file=sys.stderr)
        return 2

    for (b1, b2), fr in fr_map.items():
        token_node = find_token_node(orig_root, b1, b2)
        if token_node is None:
            continue
        # Determine if FR exists already
        has_fr = False
        fr_current = None
        for ver in token_node.findall("./version"):
            for lang in ver.findall("./lang"):
                if (lang.get("code") or "").lower() == "fr":
                    has_fr = True
                    fr_current = lang.get("display") or (lang.findtext("accessible") or "")
                    break
            if has_fr:
                break
        if not has_fr:
            # Skip if FR equals EN (fallback behavior)
            en_display = _pick_lang_display(token_node, "en")
            if en_display and fr == en_display:
                continue
            # Skip if FR equals any English <variant> or <accessible>
            equal_to_en_va = False
            for ver in token_node.findall("./version"):
                for lang in ver.findall("./lang"):
                    if (lang.get("code") or "").lower() != "en":
                        continue
                    for tag in ("variant", "accessible"):
                        for v in lang.findall(tag):
                            vtext = (v.text or "")
                            if vtext and vtext == fr:
                                equal_to_en_va = True
                                break
                        if equal_to_en_va:
                            break
                if equal_to_en_va:
                    break
            if equal_to_en_va:
                continue
            additions.append((b1, b2, fr))
        else:
            # FR exists; decide if we need an update (only if NOT only_missing)
            if not args.only_missing and fr_current is not None and fr_current != fr:
                # Apply the same skip rules: if FR equals EN or EN variant/accessible, do not update
                en_display = _pick_lang_display(token_node, "en")
                if en_display and fr == en_display:
                    pass
                else:
                    equal_to_en_va = False
                    for ver in token_node.findall("./version"):
                        for lang in ver.findall("./lang"):
                            if (lang.get("code") or "").lower() != "en":
                                continue
                            for tag in ("variant", "accessible"):
                                for v in lang.findall(tag):
                                    vtext = (v.text or "")
                                    if vtext and vtext == fr:
                                        equal_to_en_va = True
                                        break
                                if equal_to_en_va:
                                    break
                        if equal_to_en_va:
                            break
                    if not equal_to_en_va:
                        updates.append((b1, b2, fr))

    if not additions and not updates:
        print("Nothing to add or update; no file changes needed.")
        return 0

    with open(args.xml_in, "r", encoding="utf-8") as f:
        original_text = f.read()

    updated_text = _insert_fr_safely(original_text, additions, updates)

    with open(out_path, "w", encoding="utf-8") as f:
        f.write(updated_text)
    print(f"Wrote (safe writer): {out_path} (added {len(additions)} FR entries, updated {len(updates)}; existing entities preserved)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

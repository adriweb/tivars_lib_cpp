#!/usr/bin/env python3
"""Sanity checks for the generated Evo token mapping and font files."""

from __future__ import annotations

import json
import sys
from pathlib import Path

from fontTools.ttLib import TTFont


ROOT = Path(__file__).resolve().parent


def fail(message: str) -> None:
    raise AssertionError(message)


def main() -> None:
    mapping = json.loads((ROOT / "evo-token-map.json").read_text(encoding="utf-8"))
    tokens = mapping["tokens"]
    by_hex = {token["hex"]: token for token in tokens}
    expected_values = {token["value"] for token in tokens}

    if mapping["tokenCount"] != len(tokens):
        fail("tokenCount does not match the token array")
    if len(expected_values) != len(tokens):
        fail("mapping contains duplicate 16-bit values")

    expected_readable = {
        "0000": "",
        "0061": "a",
        "03C0": "π",
        "E43F": "sin(",
        "E9D6": "►ʳ",
        "EFFF": "TOK_INVALID",
        "F012": "₁₀",
    }
    for word, readable in expected_readable.items():
        if by_hex.get(word, {}).get("readable") != readable:
            fail(f"unexpected readable mapping for {word}")
    if "E3FF" in by_hex:
        fail("unknown word E3FF should not have a font mapping")

    for filename in ("EvoTokenReadable-Regular.ttf", "EvoTokenReadable-Regular.woff2"):
        font = TTFont(ROOT / filename)
        cmap = font.getBestCmap()
        if set(cmap) != expected_values:
            missing = sorted(expected_values - set(cmap))
            extra = sorted(set(cmap) - expected_values)
            fail(f"{filename} cmap mismatch; missing={missing[:5]}, extra={extra[:5]}")
        if len(font.getGlyphOrder()) != len(tokens) + 1:
            fail(f"{filename} should contain one glyph per token plus .notdef")
        for word in expected_readable:
            value = int(word, 16)
            if cmap[value] != f"evo{word}":
                fail(f"{filename} has the wrong glyph name for {word}")

    fallback_tokens = [token for token in tokens if token["fontFallbacks"]]
    if mapping["fallbackTokenCount"] != len(fallback_tokens):
        fail("fallbackTokenCount does not match the token records")

    print(
        f"OK: {len(tokens)} token words, {len(fallback_tokens)} explicit visual "
        "fallbacks, matching TTF and WOFF2 cmaps"
    )


if __name__ == "__main__":
    try:
        main()
    except AssertionError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)

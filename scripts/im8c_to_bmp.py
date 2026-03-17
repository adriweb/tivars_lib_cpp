#!/usr/bin/env python3

import argparse
import struct
from pathlib import Path


def rgb565_to_rgb888(value):
    red5 = (value >> 11) & 0x1F
    green6 = (value >> 5) & 0x3F
    blue5 = value & 0x1F
    return (
        (red5 * 255 + 15) // 31,
        (green6 * 255 + 31) // 63,
        (blue5 * 255 + 15) // 31,
    )


def parse_im8c_var(path):
    data = path.read_bytes()
    if data[:8] != b"**TI83F*":
        raise ValueError(f"{path} is not a TI var file")

    if len(data) < 74:
        raise ValueError(f"{path} is too short to contain an IM8C payload")

    data_length = data[57] | (data[58] << 8)
    payload = data[74:72 + data_length]
    if payload[:4] != b"IM8C":
        raise ValueError(f"{path} does not contain an IM8C payload")

    pos = 4
    width = payload[pos] | (payload[pos + 1] << 8) | (payload[pos + 2] << 16)
    pos += 3
    height = payload[pos] | (payload[pos + 1] << 8) | (payload[pos + 2] << 16)
    pos += 3

    palette_marker = payload[pos]
    pos += 1
    if palette_marker != 0x01:
        raise ValueError(f"Unsupported palette marker 0x{palette_marker:02X}")

    has_alpha = payload[pos] != 0
    pos += 1
    transparent_index = payload[pos]
    pos += 1

    palette_count = payload[pos]
    pos += 1
    if palette_count == 0:
        palette_count = 256

    palette = []
    for _ in range(palette_count):
        palette.append(payload[pos] | (payload[pos + 1] << 8))
        pos += 2

    return {
        "width": width,
        "height": height,
        "has_alpha": has_alpha,
        "transparent_index": transparent_index,
        "palette": palette,
        "compressed_image_data": payload[pos:],
    }


def decode_im8c_rle(width, height, compressed_image_data):
    expected_pixels = width * height
    pixels = bytearray()
    pos = 0

    while pos < len(compressed_image_data) and len(pixels) < expected_pixels:
        control = compressed_image_data[pos]
        pos += 1

        if control & 0x80:
            if pos >= len(compressed_image_data):
                raise ValueError("Truncated IM8C RLE run")

            run_length = (control & 0x7F) + 2
            palette_index = compressed_image_data[pos]
            pos += 1
            pixels.extend([palette_index] * min(run_length, expected_pixels - len(pixels)))
        else:
            literal_length = control + 1
            available = min(literal_length, len(compressed_image_data) - pos, expected_pixels - len(pixels))
            pixels.extend(compressed_image_data[pos:pos + available])
            pos += available

    if len(pixels) < expected_pixels:
        pixels.extend([0] * (expected_pixels - len(pixels)))

    return pixels


def indices_to_rgba(width, height, indices, palette, has_alpha, transparent_index):
    rgba = bytearray()
    for y in range(height):
        for x in range(width):
            palette_index = indices[y * width + x]
            if has_alpha and palette_index == transparent_index:
                rgba.extend((0, 0, 0, 0))
            else:
                rgba.extend((*rgb565_to_rgb888(palette[palette_index]), 255))
    return rgba


def make_bmp(width, height, rgba_pixels):
    row_stride = width * 4
    pixel_array_size = row_stride * height
    pixel_offset = 14 + 124
    file_size = pixel_offset + pixel_array_size

    bmp = bytearray()
    bmp.extend(b"BM")
    bmp.extend(struct.pack("<IHHI", file_size, 0, 0, pixel_offset))
    bmp.extend(struct.pack("<IIIHHIIIIII", 124, width, height, 1, 32, 3, pixel_array_size, 2835, 2835, 0, 0))
    bmp.extend(struct.pack("<IIIII", 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000, 0x73524742))
    bmp.extend(b"\x00" * 36)
    bmp.extend(struct.pack("<III", 0, 0, 0))
    bmp.extend(struct.pack("<IIII", 0, 0, 0, 0))

    for row in range(height):
        src_y = height - 1 - row
        row_start = src_y * width * 4
        for x in range(width):
            src = row_start + x * 4
            bmp.extend((rgba_pixels[src + 2], rgba_pixels[src + 1], rgba_pixels[src], rgba_pixels[src + 3]))

    return bmp


def main():
    parser = argparse.ArgumentParser(description="Decode a TI Python IM8C AppVar into a BMP preview.")
    parser.add_argument("input", type=Path, help="Input .8xv file containing an IM8C payload")
    parser.add_argument("output", type=Path, nargs="?", help="Output BMP path (defaults next to the input)")
    args = parser.parse_args()

    output = args.output if args.output is not None else args.input.with_suffix(".bmp")

    parsed = parse_im8c_var(args.input)
    indices = decode_im8c_rle(parsed["width"], parsed["height"], parsed["compressed_image_data"])
    rgba_pixels = indices_to_rgba(
        parsed["width"],
        parsed["height"],
        indices,
        parsed["palette"],
        parsed["has_alpha"],
        parsed["transparent_index"],
    )
    bmp = make_bmp(parsed["width"], parsed["height"], rgba_pixels)
    output.write_bytes(bmp)
    print(f"Wrote {output} ({parsed['width']}x{parsed['height']})")


if __name__ == "__main__":
    main()

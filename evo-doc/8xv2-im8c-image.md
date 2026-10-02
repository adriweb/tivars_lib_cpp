# Evo IM8C `.8xv2` images and sprites

IM8C is a palette-based image AppVar used by Python's
`ti_graphics.drawImage` on the Evo calculators. It stores one
image with variable dimensions and optional transparency by palette
index. The Evo payload has two known encodings: raw indexed pixels
(`format = 1`) and RLE-compressed indexed pixels (`format = 2`).

This document describes the **Evo `.8xv2` format**. The CE Python
`IM8C` `.8xv` format shares the magic and RLE packet encoding, but
has a different container and header; see the comparison below.
Evo IM8C is also separate from [`.8ca2` background images](8ca2-background-image.md)
and [`.8ci2` graph pictures](8ci2-picture.md).

The layout is confirmed by the existing tivars_lib_cpp reader/writer
and the raw-indexed fixture
[`testData/evo/TESTIM8C.8xv2`](../testData/evo/TESTIM8C.8xv2).
These are file-format and desktop-preview checks; they do not establish
the minimum supported calculator OS version.

## Evo container

The outer file uses the [common Evo CBOR container](common-container.md)
and its two-byte, big-endian checksum. It has no legacy `**TI83F*` header.

An image AppVar can use the following outer metadata:

```text
metaData.type    = 8                 AppVar
metaData.version = 1
metaData.flags   = 1
metaData.name    = tokenized name, little-endian 16-bit units, then 0000

version         = 1
size            = byte length of data, including the inner length prefix
data            = byte string containing the layout below
```

The local `TESTIM8C.8xv2` fixture instead has `metaData.flags = 0`,
which the library also reads and previews. Neither the AppVar subtype
nor transparency should be inferred from this metadata flag.
The image encoding selector is inside `data`, independently of the
two outer schema versions.

A conservative naming convention for Python use is
`[A-Z][A-Z0-9_]{0,7}`: one to eight characters, starting with an
uppercase letter. Uppercase names avoid compatibility problems with
Evo Python AppVar lookup. This is a runtime compatibility convention,
not a restriction of the generic Evo name byte string.

## Data layout

All offsets in this table are relative to the **start of the CBOR
`data` byte string**, including its two-byte AppVar length prefix.
All multibyte fields inside this byte string are little-endian.
CBOR integer/length arguments and the outer checksum retain their
own big-endian encoding.

| Offset                    | Bytes              | Field              | Meaning                                       |
|---------------------------|--------------------|--------------------|-----------------------------------------------|
| `0x00`                    | 2                  | `payloadLength`    | Number of bytes after this prefix             |
| `0x02`                    | 4                  | `magic`            | ASCII `IM8C`: `49 4D 38 43`                   |
| `0x06`                    | 2                  | `format`           | `0001` raw indexed, `0002` RLE                |
| `0x08`                    | 2                  | `width`            | Width in pixels                               |
| `0x0A`                    | 2                  | `height`           | Height in pixels                              |
| `0x0C`                    | 1                  | `hasAlpha`         | `0` opaque, `1` enables the transparent index |
| `0x0D`                    | 1                  | `transparentIndex` | Palette index to skip if transparency enabled |
| `0x0E`                    | 2                  | `paletteCount`     | Number of RGB565 entries, `1..256`            |
| `0x10`                    | `2 * paletteCount` | `palette`          | Little-endian RGB565 words                    |
| `0x10 + 2 * paletteCount` | remaining bytes    | `imageData`        | Raw indices or RLE packets                    |

The header from the `I` in `IM8C` to the palette is 14 bytes.
Let `C = paletteCount` and `D = len(imageData)`:

```text
payloadLength = 14 + 2*C + D
size          = len(data) = 2 + payloadLength
```

There is no extra palette marker, row-length table, per-image checksum,
or separate compressed-data length in the Evo IM8C payload.
In particular, 256 palette entries are stored as `00 01`, not as a
zero count.

## Palette and transparency

Each pixel uses an 8-bit index into the image's own palette.
Even an image with only two colors stores one byte per raw pixel;
it does not pack several indices into a byte.

Palette colors are RGB565:

```text
bits 15..11  red   (5 bits)
bits 10..5   green (6 bits)
bits 4..0    blue  (5 bits)

word = (red5 << 11) | (green6 << 5) | blue5
```

For example, `0x07E0` is full green and is stored as `E0 07`.
Palette words contain no alpha bits. When `hasAlpha = 1`, pixels
whose index equals `transparentIndex` leave the destination unchanged;
other indices are opaque. With `hasAlpha = 0`, the transparent-index
field is ignored. This is one transparent color index, not per-pixel
RGBA or partial opacity.

Palette order is arbitrary, provided the pixel indices refer to the
corresponding entries. Writers should map transparent pixels to a
single palette entry and set `transparentIndex` to its index: the
header cannot express several distinct transparent indices.

## Pixel order and encodings

Both encodings describe the same sequence of `width * height`
palette indices. Pixels run left-to-right, with rows top-to-bottom:

```text
index position = y * width + x
first pixel    = (0, 0), the top-left pixel
```

Unlike the bottom-to-top RGB565 payload in `.8ca2`, IM8C stores rows
top-to-bottom.
RLE packets can cross row boundaries; rows have no separators or padding.

### Format 1: raw indexed

`imageData` contains exactly `width * height` bytes, one index per
pixel. Its size is:

```text
payloadLength = 14 + 2*C + width*height
size          = 16 + 2*C + width*height
```

### Format 2: RLE

Each packet starts with a one-byte control value:

| Control  | Following bytes               | Decoded output                                               |
|----------|-------------------------------|--------------------------------------------------------------|
| `00..7F` | `control + 1` literal indices | Copy `1..128` pixels                                         |
| `80..FF` | One palette index             | Repeat it `(control & 0x7F) + 2` times, i.e. `2..129` pixels |

Examples:

```text
00 03           one literal pixel: index 3
02 00 01 02     three literal pixels: indices 0, 1, 2
80 05           two pixels of index 5
86 01           eight pixels of index 1
FF 00           129 pixels of index 0
```

Decode until exactly `width * height` indices have been produced.
A well-formed stream contains complete packets, stays within that
pixel count, and uses indices below `paletteCount`.

tivars_lib_cpp's preview decoder tolerates short literal data, clamps
output to the declared dimensions, and fills missing pixels with index
0. This is recovery behavior, not an alternate valid encoding.
Writers must flush the final literal packet and encode every
pixel. The library's own RLE encoder does so.

## Small byte-exact example

A 4 by 2 opaque green image with one palette entry has this complete
CBOR `data` value in raw-indexed format:

```text
18 00                         payloadLength = 24
49 4D 38 43                   IM8C
01 00                         format = 1
04 00 02 00                   width = 4, height = 2
00 00                         hasAlpha = 0, transparentIndex = 0
01 00                         paletteCount = 1
E0 07                         palette[0] = green
00 00 00 00 00 00 00 00       eight raw indices
```

Here `size = 26`. The equivalent RLE value is:

```text
12 00                         payloadLength = 18
49 4D 38 43                   IM8C
02 00                         format = 2
04 00 02 00                   width = 4, height = 2
00 00                         hasAlpha = 0, transparentIndex = 0
01 00                         paletteCount = 1
E0 07                         palette[0] = green
86 00                         eight pixels of index 0
```

Here `size = 20`. Both examples decode in the existing library into
eight opaque green preview pixels.

### Creating the RLE example with the CLI

From the repository root, turn the second example's hex bytes into a
binary input, then wrap them as an Evo AppVar:

```sh
printf '%s' '1200494D384302000400020000000100E0078600' | xxd -r -p > GREEN.bin
./tivars_cli -i GREEN.bin -j raw -t AppVar -n GREEN -m 84Evo -o GREEN.8xv2 -k varfile
```

`-j raw` expects binary bytes, including the inner two-byte length;
it does not accept a text hex dump. The CLI supplies the CBOR container,
tokenized name, outer sizes, and checksum. The resulting file can be
previewed with Quick Look and drawn on the calculator with
`drawImage("GREEN", 0, 30)`.

To extract and verify the same bytes:

```sh
./tivars_cli -i GREEN.8xv2 -k raw -o GREEN-roundtrip.bin
cmp GREEN.bin GREEN-roundtrip.bin
```

## Local fixture

`testData/evo/TESTIM8C.8xv2` has:

```text
file bytes       = 6962
metaData.type    = 8
metaData.version = 1
metaData.flags   = 0
body version     = 1
size             = 6876
payloadLength    = 6874
format           = 1
width            = 154
height           = 42
hasAlpha         = 0
transparentIndex = 0
paletteCount     = 196
imageData bytes  = 6468 = 154 * 42
checksum         = 7D DF
```

The prefix and header begin:

```text
DA 1A 49 4D 38 43 01 00 9A 00 2A 00 00 00 C4 00
```

All pixel indices are within `0..195`. The outer checksum verifies,
and all 6,468 library-preview pixels match an independent decode of
the fixture's RGB565 palette and indices.
Fixture SHA-256:

```text
c8e0ca7aecec01604593dc34f6f2347147629eff52ee563a009cbbdc16dc2b90
```

## Python use and size limits

To draw an image AppVar from Python:

```python
from ti_graphics import drawImage
from ti_system import disp_wait

drawImage("TESTIM8C", 0, 30)
disp_wait()
```

The argument is the on-calculator AppVar name, without `.8xv2`.
Transparency allows an image to serve as a sprite; the payload itself
contains no position, animation frames, or sprite-sheet metadata.

The 16-bit dimension fields do not prove that arbitrarily large
images are supported by the calculator. The two-byte inner payload
length must remain representable. A full 320 by 210 raw image already
needs 67,200 index bytes, before its header and palette, so it exceeds
that length field's capacity. Use a smaller image or RLE where needed.

## Distinction from CE IM8C

This comparison only identifies the layout boundary; the Evo offsets
above must not be used for CE files.

| Property       | Evo `.8xv2`                              | CE Python `.8xv`                                                                 |
|----------------|------------------------------------------|----------------------------------------------------------------------------------|
| Outer file     | Evo CBOR and XOR checksum                | Legacy `**TI83F*` variable file and additive checksum                            |
| Image selector | 16-bit `format` immediately after `IM8C` | No selector there; marker `01` follows dimensions in the supported CE RLE layout |
| Width / height | Two 16-bit little-endian values          | Two 24-bit little-endian values                                                  |
| Palette count  | 16-bit little-endian, 256 = `00 01`      | One byte, 256 = `00`                                                             |
| Pixel encoding | Raw indexed (1) or RLE (2)               | RLE in the existing CE converter/library support                                 |

Changing only the extension or outer wrapper is not enough.
Conversion must rebuild the IM8C header and, when switching between
raw and RLE, convert the pixel stream.

## Existing library support and source references

tivars_lib_cpp already implements both Evo formats in
[`TH_StructuredAppVar.cpp`](../src/TypeHandlers/TH_StructuredAppVar.cpp):
`parse_python_image_appvar`, `make_evo_python_image_payload`,
`decode_python_image_rle`, and `encode_python_image_rle`.
The structured JSON calls them `EvoRawIndexed` and `EvoRle`, and
reports dimensions, palette, `imageDataHex`, and a preview image.
The [Quick Look support](../quicklook/TIVarsQuickLookSupport.mm) finds
the generated `previewImageDataUrl` in the readable JSON and displays it.

The [image AppVar tests](../tests.cpp) cover both formats, preview
pixels, and CE/Evo conversion.

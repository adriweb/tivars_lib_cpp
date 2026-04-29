# TI-84 Evo file container notes

The files are not legacy `**TI83F*` files.
They are CBOR-like values followed by a two-byte checksum.

## Top-level layout

Most samples are an indefinite-length CBOR map:

```text
BF                         # map(*)
  68 "metaData" : BF       # nested map(*)
    64 "type"    : uint
    67 "version" : uint
    65 "flags"   : uint
    64 "name"    : byte string
  FF
  ... type-specific keys ...
FF                         # end top-level map
cc cc                      # checksum, big-endian
```

Common CBOR bytes used by the samples:

```text
BF        indefinite map
9F        indefinite array
FF        break/end marker
64 xx     4-byte text key
65 xx     5-byte text key
67 xx     7-byte text key
68 xx     8-byte text key
44 xx     4-byte byte string
4A xx     10-byte byte string
52 xx     18-byte byte string
54 xx     20-byte byte string
58 nn     byte string with one-byte length
59 nn nn  byte string with two-byte big-endian length
18 nn     uint8 value
19 nn nn  uint16 value, big-endian
```

CBOR uses the shortest available length/value encoding. For unsigned
integers, values `0..23` are encoded directly in the low bits of one
byte. For byte strings, lengths `0..23` are encoded as `0x40 + len`.
Larger values add an extra length/value field:

```text
unsigned int 16      10
unsigned int 20      14
unsigned int 24      18 18
unsigned int 256     19 01 00
unsigned int 33601   19 83 41

byte string len 16      50
byte string len 20      54
byte string len 24      58 18
byte string len 33601   59 83 41
```

So a small AppVar with `size = 16` stores the body size as the single
CBOR byte `10`, and its `data` byte string begins with `50`. A large
image with `size = 33601` stores the body size as `19 83 41`, and its
`data` byte string begins with `59 83 41`.

## Metadata `type` values seen

```text
0   .8xn2  real number
1   .8xl2  list
2   .8xp2  TI-BASIC program
4   .8ci2  Pic / graph picture
5   .8ca2  background image
6   .8xm2  matrix
7   .8xy2  equation
8   .8xv2  appvar
12  .8xw2  window settings
13  .8xz2  user zoom / RclWindw
14  .8xt2  table setup
15  .py    Python program, converted by Connect Evo before transfer
17          nested custom scalar entries inside settings files
```

## Common `version` and `flags` fields

There are two layers of `version` and `flags` fields:

```text
metaData.version   metadata/envelope schema version
metaData.flags     variable-level metadata flags

version            payload schema version
flags              payload-specific flags, when present
```

All parsed sample files use `metaData.version = 1` and body
`version = 1` where the body version field exists.

Settings containers (`.8xw2`, `.8xz2`, and `.8xt2`) do not have a
top-level body `version` in the current samples; their top-level `data`
field is an array of nested type `17` scalar entries, and those nested
entries have their own scalar body `version` and `flags` fields.

Observed `flags` values:

```text
metaData.flags = 0  ordinary numeric/list/program/matrix/appvar/settings files
metaData.flags = 1  image-like type 4 `.8ci2` and type 5 `.8ca2`
metaData.flags = 4  equation sample `.8xy2`
metaData.flags = 8  special graph/window coordinate variables (`X`, `Y`,
                    `XMIN`, `XMAX`, `YMIN`, `YMAX`)

body flags = 0      ordinary scalar payload
body flags = 6      top-level exact fraction `.8xn2`
```

Lists and matrices do not use a body `flags` key for exact-fraction
elements. They append one byte per displayed element/cell after the
expression region; `06` marks exact fractions there.

See `evo-version-flags.md` for the detailed sample table.

## Name encoding

The `metaData.name` byte string is a sequence of 16-bit little-endian
name tokens, usually terminated by `0000`.

Examples:

```text
SEUIL.8xp2 name bytes:
12 e8 04 e8 14 e8 08 e8 0b e8 00 00

Words:
E812 E804 E814 E808 E80B 0000
S    E    U    I    L    EOS
```

Same for AppVar names, which may use lowercase letter tokens. For example `PolyCnfg.8xv2` uses:

```text
E80F 006F 006C 0079 E802 006E 0066 0067 0000
P    o    l    y    C    n    f    g    EOS
```

## Checksum

16-bit big-endian added at the end.  
The checksum algorithm verified against all sample files is:

```js
function evoChecksum(body) {
  let wordCount = ((body.length - 3) >> 1) - ((body.length - 3) & 1);
  let checksum = 0;

  for (let i = 0; i < wordCount; i++) {
    checksum ^= body[i * 2] | (body[i * 2 + 1] << 8);
  }

  return checksum & 0xffff;
}
```

The stored trailer is:

```js
body.push(checksum >> 8, checksum & 0xff);
```

So `SEUIL.8xp2` ends in `48 c2`, meaning checksum value `0x48C2`.

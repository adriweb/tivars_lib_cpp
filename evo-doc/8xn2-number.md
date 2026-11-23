# `.8xn2` scalar numbers

Samples include real and complex numbers, plus exact-expression exports from
TI-83 Evo OS 7.0.0.3996 and 7.1.0.4421.

For evaluator behavior and the distinction between exact runtime objects and
confirmed transfer-file encodings, see
[`ti83-exact-math-engine.md`](ti83-exact-math-engine.md).

Metadata:

```text
metaData.type    = 0
metaData.version = 1
metaData.flags   = 0 or 8 in the samples
metaData.name    = variable token, little-endian, then 0000
```

The observed `metaData.flags = 8` top-level samples are `X.8xn2` and
`Y.8xn2`. The same metadata flag appears on nested `XMIN`, `XMAX`,
`YMIN`, and `YMAX` entries in `testData/evo/Window.8xw2`, while the
same nested entries use `metaData.flags = 0` in
`testData/evo/more/Window.8xw2` and
`testData/evo/more/Window_parametric.8xw2`. This appears tied to
metadata context for special graph/window coordinate variables, not to
the scalar data format itself, and should not be required to recognize
those variables.

Body fields:

```text
version   = scalar payload version
flags     = scalar display/type flags
arraylen  = number of 16-bit words in data
size      = byte length of data; equals arraylen * 2
data      = custom scalar payload, little-endian 16-bit words
```

## Observed body versions and flags

Fresh type-0 `.8xn2` exports from the tested 7.0 builds have body
`version = 1`. The tested 7.1 builds export body `version = 2`.
The metadata version is `1` in every sample.

| Model | Body version on OS 7.0.0.3996 | Tested 7.1 build | Body version on 7.1 |
| --- | --- | --- | --- |
| TI-83 Evo | `1` | 7.1.0.4421 | `2` |
| TI-84 Evo | `1` | 7.1.0.4421 | `2` |
| TI-84 Evo-T | `1` | 7.1.0.4421 | `2` |
| TI-84 Evo-S | — | 7.1.0.4404 | `2` |

The TI-84 checks covered `256`, `1.5`, and the result of `sqrt(8)`.
For each value, the TI-84 models produced byte-identical exports within the
tested release. The [paired integer fixtures](../testData/evo/exact/)
represent the TI-84 exports for both releases.

The paired TI-83 Evo exact-expression exports show these body flags.

| Exported value | 7.0 body flags | 7.1 body flags |
| --- | --- | --- |
| `1/3` entered with the fraction template | `0x06` | `0x06` |
| `pi`, `2*sqrt(2)` | `0x02` | `0x20` |

All three TI-83 Evo pairs have identical payload words. The TI-83 Evo on
7.1 also imported the 7.0 files. When it re-exported them, it wrote body
version `2` and a new checksum, preserving the imported flags and payload
words. Readers should preserve the stored version and flags when saving a file.

## Scalar expressions

The scalar payload is a postfix expression. A numeric leaf ends with its tag.
An operator follows its operands. The documented tags include the following.

```text
001F  positive exact integer
0020  negative exact integer
0021  positive exact fraction
0022  negative exact fraction
0023  decimal / approximate value
0024  pi leaf
0025  e leaf
0026  i leaf
0051  square root
0053  natural logarithm
007A  unary negation
008B  addition
008D  subtraction
008F  multiplication
0093  power
```

For addition, subtraction, and multiplication, the left operand comes first.
Power stores the exponent first, followed by the base. Read the payload from
the end to locate each tag. Integer limbs and decimal data can contain the
same word values as tags.

## Exact integers

Non-zero exact integers are stored as little-endian 16-bit limbs,
followed by a limb count, followed by a sign tag.

```text
positive integer:
  limb0 limb1 ... limbN limb_count 001F

negative integer:
  limb0 limb1 ... limbN limb_count 0020
```

The integer value is:

```text
limb0 + limb1 * 65536 + limb2 * 65536^2 + ...
```

Zero is special-cased as:

```text
0000 001F
```

Controlled samples:

```text
A = 0:
  0000 001F

B = 1:
  0001 0001 001F

C = -1:
  0001 0001 0020

D = 2:
  0002 0001 001F

E = -2:
  0002 0001 0020

F = 10:
  000A 0001 001F

G = 255:
  00FF 0001 001F

H = 256:
  0100 0001 001F

I = 65535:
  FFFF 0001 001F

P = 1000:
  03E8 0001 001F

R = -25000000:
  7840 017D 0002 0020
```

For `R`, `0x017D7840 = 25000000`.

## Decimals

Decimal values use six 16-bit words:

```text
mantissa_byte[8] sign_byte exponent_byte 0023
```

The first 8 bytes are packed BCD, least-significant byte first. To read
them, reverse the 8 bytes, print each byte as two decimal nibbles, then
strip leading zeroes. The result is the fixed-precision significand
digits; trailing zeroes may just be padding. The sign byte is `01` for
positive and `FF` for negative. The exponent byte is a signed base-10
exponent for the leading significand digit.

Examples:

```text
K = 0.1:
  bytes 00 00 00 00 00 00 00 10 01 ff 23 00
  words 0000 0000 0000 1000 FF01 0023
  significand 1.0, exponent -1

L = 1.5:
  bytes 00 00 00 00 00 00 00 15 01 00 23 00
  words 0000 0000 0000 1500 0001 0023
  significand 1.5, exponent 0

M = -1.5:
  bytes 00 00 00 00 00 00 00 15 ff 00 23 00
  words 0000 0000 0000 1500 00FF 0023
  significand -1.5, exponent 0

N = 9.8:
  bytes 00 00 00 00 00 00 00 98 01 00 23 00
  words 0000 0000 0000 9800 0001 0023
  significand 9.8, exponent 0

O = 675.89:
  bytes 00 00 00 00 00 90 58 67 01 02 23 00
  words 0000 0000 9000 6758 0201 0023
  significand 6.7589, exponent 2

Q = 0.001:
  bytes 00 00 00 00 00 00 00 10 01 fd 23 00
  words 0000 0000 0000 1000 FD01 0023
  significand 1.0, exponent -3

J = 1.6379784E66:
  bytes 00 00 00 00 84 97 37 16 01 42 23 00
  words 0000 0000 9784 1637 4201 0023
  significand 1.6379784, exponent 66
```

## Exact fractions

The sampled top-level exact fraction variables have body `flags = 6`. Their payload
stores denominator magnitude, denominator limb count, numerator
magnitude, numerator limb count, then a signed fraction tag:

```text
positive numerator/denominator:
  denominator den_limb_count numerator num_limb_count 0021

negative numerator/denominator:
  denominator den_limb_count numerator num_limb_count 0022
```

Controlled samples:

```text
S = 1/3:
  flags = 6
  data  = 0003 0001 0001 0001 0021

T = -2/5:
  flags = 6
  data  = 0005 0001 0002 0001 0022
```

## Exact expression exports

The following are complete payloads in 16-bit word order from TI-83 Evo
OS 7.1.0.4421. All use body version `2`. The constants `pi` and `e`
each occupy one word. A function such as `sqrt` or `ln` follows its argument.

| Stored value | Body flags | Data words |
| --- | --- | --- |
| `pi` | `0x20` | `0024` |
| `e` | `0x20` | `0025` |
| `i` | `0x00` | `0026` |
| `2*sqrt(2)` | `0x20` | `0002 0001 001F 0002 0001 001F 0051 008F` |
| `ln(2)` | `0x20` | `0002 0001 001F 0053` |
| `2^(1/3)` | `0x20` | `0003 0001 0001 0001 0021 0002 0001 001F 0093` |
| `(2/3)i` | `0x02` | `0003 0001 0002 0001 0021 0026 008F` |
| `1/3+sqrt(2)*i` | `0x20` | `0003 0001 0001 0001 0021 0002 0001 001F 0051 0026 008F 008B` |

The [test fixtures](../testData/evo/exact/) include paired `pi` exports and
the 7.1 exact power and complex examples.

`getReadableContent()` renders supported expressions in `readableContent`
and exposes the original bytes in `rawDataHex`. For an unrecognized
expression, the JSON still includes the metadata and raw bytes.

## Complex values

Complex payloads combine real and imaginary terms using the same postfix
operators as other scalar expressions.

Observed pure imaginary forms:

```text
U = 2i:
  0002 0001 001F 0026 008F

more/complex D = 0+2i, displayed as 2i:
  0002 0001 001F 0026 008F

V = -3i:
  0003 0001 001F 0026 008F 007A

X = -42i:
  002A 0001 001F 0026 008F 007A

more/complex E = 0-2i, displayed as -2i:
  0002 0001 001F 0026 008F 007A
```

Observed rectangular complex forms:

```text
W = 4+2i:
  0004 0001 001F 0002 0001 001F 0026 008F 008B

more/complex A = 4-2i:
  0004 0001 001F 0002 0001 001F 0026 008F 008D

more/complex B = -4-2i:
  0004 0001 0020 0002 0001 001F 0026 008F 008D

more/complex C = -4+(-2)i, displayed as -4-2i:
  0004 0001 0020 0002 0001 001F 0026 008F 008D

more/complex F = 1.5+0.1i:
  0000 0000 0000 1500 0001 0023
  0000 0000 0000 1000 FF01 0023
  0026 008F 008B

more/complex Z = -1.5479162+(2/3)i:
  0000 0000 9162 1547 00FF 0023
  6700 6666 6666 6666 FF01 0023
  0026 008F 008B
```

The list sample `L4.8xl2` also contains `-4+2i`:

```text
0004 0001 0020 0002 0001 001F 0026 008F 008B
```

The expressions above follow these patterns.

```text
scalar 0026 008F             scalar * i
scalar 0026 008F 007A        -(scalar * i)
real scalar 0026 008F 008B   real + scalar * i
real scalar 0026 008F 008D   real - scalar * i
```

`0026` is the `i` leaf, `008F` multiplies the preceding two nodes, `007A`
negates the preceding node, and `008B` / `008D` add or subtract the preceding
two nodes. For rectangular values, the imaginary scalar payload stores the
magnitude; the relationship between the real and imaginary parts is carried
by the final addition or subtraction operator. The real part keeps its own
scalar sign tag.

Entering the mixed expression `-1.5479162+(2/3)i` on OS 7.1 reproduces
the `Z.8xn2` payload word for word, with decimal leaves for both components.
Entering the exact fraction template `(2/3)i` by itself preserves the
fraction leaf, as shown above.

Implementation note: nested custom scalar entries inside `.8xw2`,
`.8xz2`, and `.8xt2` use the same `version`, `flags`, `arraylen`,
`size`, and `data` shape, but their `metaData.type` is `17` instead of
top-level number type `0`.

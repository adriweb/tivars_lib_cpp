# `.8xn2` real numbers

Sample files: the original top-level `.8xn2` files plus the controlled number samples.

Metadata:

```text
metaData.type    = 0
metaData.version = 1
metaData.flags   = 0 or 8 in the samples
metaData.name    = variable token, little-endian, then 0000
```

The observed `metaData.flags = 8` top-level samples are `X.8xn2` and
`Y.8xn2`. The same metadata flag appears on nested `XMIN`, `XMAX`,
`YMIN`, and `YMAX` entries in `Window.8xw2`. This appears tied to
special graph/window coordinate variables, not to the scalar data format
itself.

Body fields:

```text
version   = 1
flags     = scalar display/type flags; 6 marks exact fractions
arraylen  = number of 16-bit words in data
size      = byte length of data; equals arraylen * 2
data      = custom scalar payload, little-endian 16-bit words
```

The scalar payload format is postfix-tagged. From the number, list, and
matrix samples:

```text
Last word 001F  positive exact integer
Last word 0020  negative exact integer
Last word 0021  exact fraction
Last word 0022  negative exact fraction
Last word 0023  decimal / approximate value
```

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

Top-level exact fraction variables have body `flags = 6`. Their payload
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

## Complex values

Complex samples look like expression fragments built from the scalar
payloads above and short suffix markers.

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

Observed suffix meanings:

```text
0026 008F       positive pure imaginary suffix
0026 008F 007A  negative pure imaginary suffix
0026 008F 008B  rectangular real + imag*i suffix
0026 008F 008D  rectangular real - imag*i suffix
```

For rectangular values, the imaginary scalar payload stores the
magnitude; the plus/minus relationship between real and imaginary parts
is carried by the final `008B` or `008D` suffix. The real part keeps its
own scalar sign tag. In `Z.8xn2`, the typed exact fraction `(2/3)i` was
stored inside the complex number as the decimal approximation
`0.66666666666667i`, not as an exact-fraction payload.

Implementation note: nested custom scalar entries inside `.8xw2`,
`.8xz2`, and `.8xt2` use the same `version`, `flags`, `arraylen`,
`size`, and `data` shape, but their `metaData.type` is `17` instead of
top-level number type `0`.

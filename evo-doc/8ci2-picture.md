# `.8ci2` pictures

Sample files:

- `Pic1.8ci2` through `Pic6.8ci2`
- `testData/evo/img_compare/Zelda.8ci2`

Metadata:

```text
metaData.type    = 4
metaData.version = 1
metaData.flags   = 1
metaData.name    = Pic token, little-endian, then 0000
```

Observed Pic name tokens:

```text
Pic1  E880 TOK_VAR_PIC1
Pic2  E881 TOK_VAR_PIC2
Pic3  E882 TOK_VAR_PIC3
Pic4  E883 TOK_VAR_PIC4
Pic5  E884 TOK_VAR_PIC5
Pic6  E885 TOK_VAR_PIC6
```

The token table also defines `Pic7` through `Pic9` and `Pic0`:

```text
Pic7  E886 TOK_VAR_PIC7
Pic8  E887 TOK_VAR_PIC8
Pic9  E888 TOK_VAR_PIC9
Pic0  E889 TOK_VAR_PIC0
```

Body fields:

```text
version = 1
size    = 33441
data    = byte string, length 33441
```

Observed data layout:

```text
data[0]      picture-format marker; 0x0F in the provided samples
data[1..]    33440 bytes of packed pixel data
```

`33440 = 160 * 209`, which is 209 rows with 160 bytes per row. Each
byte stores two 4-bit pixels, so the pixel geometry is:

```text
width  = 320 pixels
height = 209 pixels
bpp    = 4
stride = 160 bytes per row
```

img2calc generates type `4` `.8ci2` files with this layout:

```text
size = 33441
data = 0F followed by 33440 packed 4-bpp bytes
```

For each output byte, img2calc writes the left pixel in the high nibble
and the right pixel in the low nibble:

```text
byte = (left_palette_index << 4) | right_palette_index
```

For `.8ci2`, img2calc writes rows top-to-bottom: output byte 0 is the
leftmost two pixels of display row 0. This differs from img2calc's
`.8ca2` path, where it vertically flips while reading the RGB pixels, so
the stored RGB565 payload is bottom-to-top.

The sample bytes are packed 4-bit color indices:

```text
byte 0x03  two pixels: color 0, color 3
byte 0x30  two pixels: color 3, color 0
byte 0x33  two pixels: color 3, color 3
```

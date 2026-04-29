# `.8ca2` background images

Sample files:

- `Image1.8ca2` through `Image7.8ca2`
- `testData/evo/img_compare/background.8ca2`
- `testData/evo/img_compare/background_ticonnectevo.8ca2`

Metadata:

```text
metaData.type    = 5
metaData.version = 1
metaData.flags   = 1
metaData.name    = image token, little-endian, then 0000
```

Observed image name tokens:

```text
Image1  E8B0 TOK_VAR_IMAGE1
Image2  E8B1 TOK_VAR_IMAGE2
Image3  E8B2 TOK_VAR_IMAGE3
Image4  E8B3 TOK_VAR_IMAGE4
Image5  E8B4 TOK_VAR_IMAGE5
Image6  E8B5 TOK_VAR_IMAGE6
Image7  E8B6 TOK_VAR_IMAGE7
```

Body fields:

```text
version = 1
size    = 33601
data    = byte string, length 33601
```

We have a hard-coded CBOR prefix of 70 bytes long before the RGB565 pixel data:

```text
BF                                      top-level map(*)
  "metaData": BF
    "type":    5
    "version": 1
    "flags":   1
    "name":    44 B0 E8 00 00          Image1 token, patched per slot
  FF
  "version": 1
  "size":    0x8341                    33601
  "data":    59 83 41 0B ...           byte string, first byte marker
```

The image data byte string is:

```text
data[0]      image-format marker
data[1..]    33600 bytes of RGB565 pixel data
```

Pixel geometry:

```text
width  = 160
height = 105
bytes  = 160 * 105 * 2 = 33600
format = RGB565, little-endian words
```

The converter draws the input image onto a 160 by 105 white canvas,
preserving aspect ratio and centering the result. It disables canvas
image smoothing, converts RGBA8888 to RGB565, then writes rows
bottom-to-top:

```text
stored row 0   = source/display row 104
stored row 1   = source/display row 103
...
stored row 104 = source/display row 0
```

Within each stored row, pixels are left-to-right.

RGB565 word decoding:

```js
let word = data[p] | (data[p + 1] << 8);
let r5 = (word >> 11) & 0x1f;
let g6 = (word >> 5) & 0x3f;
let b5 = word & 0x1f;
```

Observed data markers:

```text
0x0B  TI Connect Evo image import output
0x16  calculator/exported samples and img2calc-generated output
```


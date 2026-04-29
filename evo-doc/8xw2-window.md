# `.8xw2` window settings

Sample file: `Window.8xw2`.

Metadata:

```text
metaData.type    = 12
metaData.version = 1
metaData.flags   = 0
metaData.name    = E8BA 0000
```

`E8BA` is `TOK_VAR_WINDOW`.

The top-level body has no scalar `version` or `size` fields in the
sample. It contains:

```text
data = indefinite CBOR array of nested custom scalar entries
```

Each nested entry is itself a CBOR map with:

```text
metaData.type    = 17
metaData.version = 1
metaData.flags   = 0 or 8
metaData.name    = setting token, little-endian, then 0000

version   = 1
flags     = 0
arraylen  = number of 16-bit words in data
size      = byte length of data; equals arraylen * 2
data      = custom scalar payload
```

Observed `Window.8xw2` entries:

```text
00  E98F TOK_VAR_XMIN         metaData.flags 8, size 12
01  E990 TOK_VAR_XMAX         metaData.flags 8, size 12
02  E991 TOK_VAR_XSCL         size 6
03  E993 TOK_VAR_YMIN         metaData.flags 8, size 12
04  E994 TOK_VAR_YMAX         metaData.flags 8, size 12
05  E995 TOK_VAR_YSCL         size 6
06  E992 TOK_VAR_X_RES        size 6
07  E9B2 TOK_VAR_DELTA_X      size 12
08  E9B1 TOK_VAR_TRACE_STEP   size 12
09  E9B3 TOK_VAR_DELTA_Y      size 12
10  E999 TOK_VAR_THETA_MIN    size 4
11  E99A TOK_VAR_THETA_MAX    size 12
12  E99B TOK_VAR_THETA_STEP   size 12
13  E996 TOK_VAR_TMIN         size 4
14  E997 TOK_VAR_TMAX         size 12
15  E998 TOK_VAR_T_STEP       size 12
16  E9A9 TOK_VAR_N_MIN        size 6
17  E9AB TOK_VAR_N_MAX        size 6
18  E9AD TOK_VAR_PLOT_START   size 6
19  E9AF TOK_VAR_PLOT_STEP    size 6
```

Representative custom scalar payloads:

```text
XMIN:
  00 00 00 00 00 00 90 15 ff 01 23 00

XMAX:
  00 00 00 00 00 00 90 15 01 01 23 00

XSCL:
  01 00 01 00 1f 00

THETA_MIN:
  00 00 1f 00

THETA_MAX:
  00 00 00 07 53 18 83 62 01 00 23 00
```

The nested scalar `data` encoding matches the `.8xn2` custom scalar
shape. The observed nested `metaData.flags = 8` values are limited to
the X/Y min/max coordinate variables in this sample. Other window
variables use `metaData.flags = 0`.

# `.8xz2` RclWindw / user zoom settings

Sample file: `RclWindw.8xz2`.

Metadata:

```text
metaData.type    = 13
metaData.version = 1
metaData.flags   = 0
metaData.name    = E8BB 0000
```

`E8BB` is labeled `TOK_VAR_ZSTO` in the token table and displayed by
the web app as `RclWindw` / user zoom.

The top-level body has no scalar `version` or `size` fields in the
sample. It contains:

```text
data = indefinite CBOR array of nested custom scalar entries
```

Each nested entry uses `metaData.type = 17` and the same custom scalar
layout as `.8xn2` number data:

```text
metaData.type    = 17
metaData.version = 1
metaData.flags   = 0
metaData.name    = setting token, little-endian, then 0000

version   = 1
flags     = 0
arraylen  = number of 16-bit words in data
size      = byte length of data; equals arraylen * 2
data      = custom scalar payload
```

Observed entries:

```text
00  E99C TOK_VAR_UX_MIN        size 12
01  E99D TOK_VAR_UX_MAX        size 12
02  E99E TOK_VAR_UXSCL         size 6
03  E99F TOK_VAR_UXRES         size 6
04  E9A0 TOK_VAR_UY_MIN        size 12
05  E9A1 TOK_VAR_UY_MAX        size 12
06  E9A2 TOK_VAR_UYSCL         size 6
07  E9A3 TOK_VAR_UTHETA_MIN    size 4
08  E9A4 TOK_VAR_UTHETA_MAX    size 12
09  E9A5 TOK_VAR_UTHETA_STEP   size 12
10  E9A6 TOK_VAR_UT_MIN        size 4
11  E9A7 TOK_VAR_UT_MAX        size 12
12  E9A8 TOK_VAR_UT_STEP       size 12
13  E9AA TOK_VAR_UN_MIN        size 6
14  E9AC TOK_VAR_UN_MAX        size 6
15  E9AE TOK_VAR_UPLOT_START   size 6
16  E9B0 TOK_VAR_UPLOT_STEP    size 6
```

Representative payloads:

```text
UX_MIN:
  00 00 00 00 00 00 90 15 ff 01 23 00

UX_MAX:
  00 00 00 00 00 00 90 15 01 01 23 00

UXSCL:
  01 00 01 00 1f 00

UTHETA_MAX:
  00 00 00 07 53 18 83 62 01 00 23 00

UTHETA_STEP:
  00 57 99 38 69 99 08 13 01 ff 23 00
```

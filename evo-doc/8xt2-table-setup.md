# `.8xt2` table setup

Sample file: `TblSet.8xt2`.

Metadata:

```text
metaData.type    = 14
metaData.version = 1
metaData.flags   = 0
metaData.name    = E8BC 0000
```

`E8BC` is `TOK_VAR_TBLSETUP`.

The top-level body has no scalar `version` or `size` fields in the
sample. It contains:

```text
data = indefinite CBOR array of nested custom scalar entries
```

Observed entries:

```text
00  E9B6 TOK_VAR_TBL_MIN   size 4
01  E9B7 TOK_VAR_TBL_STEP  size 6
```

Nested entry layout:

```text
metaData.type    = 17
metaData.version = 1
metaData.flags   = 0
metaData.name    = setting token, little-endian, then 0000
version          = 1
flags            = 0
arraylen         = number of 16-bit words in data
size             = byte length of data
data             = custom scalar payload
```

Observed payloads:

```text
TBL_MIN:
  data bytes = 00 00 1f 00
  data words = 0000 001F

TBL_STEP:
  data bytes = 01 00 01 00 1f 00
  data words = 0001 0001 001F
```

The custom scalar encoding matches the exact integer-like samples in
`.8xn2`. The top-level settings container owns the array; only the
nested type `17` entries have scalar `version`, `flags`, `arraylen`,
`size`, and `data` fields.

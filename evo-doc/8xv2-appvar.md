# `.8xv2` appvars

Sample files: `EqnsCnfg.8xv2`, `PolyCnfg.8xv2`.

Metadata:

```text
metaData.type    = 8
metaData.version = 1
metaData.flags   = 0
metaData.name    = mixed token/ASCII 16-bit name units, little-endian
```

Body fields:

```text
version = 1
size    = byte length of data
data    = app-specific byte string
```

Unlike `.8xp2` and `.8xy2`, the appvar data is not a generic TI-BASIC
token stream and has no `arraylen` field. It is owned by the
corresponding app or system feature. Readers should treat the payload as
opaque unless they implement the specific appvar subtype, and writers
should preserve it byte-for-byte when round-tripping.

## `PolyCnfg.8xv2`

Name:

```text
E80F 006F 006C 0079 E802 006E 0066 0067 0000
P    o    l    y    C    n    f    g    EOS
```

Body:

```text
size = 20
data = 08 50 4c 59 02 00 00 00 82 e5 85 e5 87 e5 8e e5 96 e5 00 00
```

The ASCII bytes at `data[1..3]` spell `PLY`.

## `EqnsCnfg.8xv2`

Name:

```text
E804 0071 006E 0073 E802 006E 0066 0067 0000
E    q    n    s    C    n    f    g    EOS
```

Body:

```text
size = 16
data = 08 53 53 4f 01 01 01 00 96 e5 82 e5 85 e5 87 e5
```

The ASCII bytes at `data[1..3]` spell `SSO`.

# Python program transfer notes

Connect Evo treats `.py` files as metadata type `15`, displayed as
`Python Program`. Unlike ordinary Evo CBOR variable files, a selected
`.py` source file is directly converted at transfer time (both ways).

A real type 15 file, which we could invent extension "8xpy2" for, exists though.
`python3 evo_usb.py <script.py> [varname]` does not send the `.py` bytes
directly. It converts the UTF-8 source into a type `15` Evo CBOR variable
whose body `data` field is an AppVar-like payload containing the source.

The command-line variable name defaults to `pyscript` and must be 1 to 8
lowercase ASCII letters. The transfer URL is:

```text
hh01/xfr/var?name=<UTF-8 percent-encoded token name>&type=15&memtarget=0&policy=1
```

`--get-file <name> [type] [output]` is generic raw Evo-variable retrieval.
For type `15`, the default output extension is `8xpy2`; it writes the CBOR
payload returned by the calculator plus the two-byte Evo checksum. It does
not currently convert the script back to a plain `.py` file.

## Type 15 CBOR wrapper

For `name = "pyscript"` and `source = "print(42)\n"`, current
`evo_usb.py` builds this 109-byte payload before adding any file checksum:

```text
BF                                      # map(*)
   68                                   # text(8)
      6D65746144617461                  # "metaData"
   BF                                   # map(*)
      64                                # text(4)
         74797065                       # "type"
      0F                                # unsigned(15)
      67                                # text(7)
         76657273696F6E                 # "version"
      01                                # unsigned(1)
      64                                # text(4)
         6E616D65                       # "name"
      50                                # bytes(16)
         0FE818E812E802E811E808E80FE813E8
                                           # E80F E818 E812 E802 E811 E808 E80F E813
                                           # p    y    s    c    r    i    p    t
      FF                                # primitive(*)
   67                                   # text(7)
      76657273696F6E                    # "version"
   01                                   # unsigned(1)
   64                                   # text(4)
      73697A65                          # "size"
   18 24                                # unsigned(36)
   64                                   # text(4)
      64617461                          # "data"
   58 24                                # bytes(36)
      13010000                          # AppVar-like magic/header
      24000000                          # total length = 36, little-endian
      08                                # name length
      000000                            # reserved
      5059534352495054                  # "PYSCRIPT"
      00                                # name terminator
      0A00                              # script byte length = 10, little-endian
      0002                              # script header
      7072696E74283432290A              # "print(42)\n"
      00                                # trailing terminator
   FF                                   # primitive(*)
```

Notes:

- Current `build_payload()` emits `metaData.type`, `metaData.version`, and
  `metaData.name`; it does not emit `metaData.flags`.
- `metaData.name` is the lowercase variable name tokenized as little-endian
  `E8xx` words, with no `0000` terminator in the current type `15` sender.
- The body `size` is the byte length of the AppVar-like payload, not the
  length of the Python source.
- The body `data` byte string is:

```text
13 01 00 00
uint32_le total_payload_size
uint8     uppercase_name_length
00 00 00
uppercase_name_bytes
00
uint16_le utf8_source_size
00 02
utf8_source_bytes
00
```


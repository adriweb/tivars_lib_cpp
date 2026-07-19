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

The apparent fixed fields after the eight-byte object header are actually two
generic typed sections:

```text
uint24_le data_size
uint8     section_kind
byte      data[data_size]
00        trailing terminator
```

Kind `0` is the ASCII object/import name. Kind `2` is the executable body. The
object subtype decides whether kind `2` contains UTF-8 source or MicroPython
persistent bytecode.

## Compiled MicroPython modules

TI-Python reports this implementation tuple on OS 7.0.0.3996:

```text
implementation = tipython 1.13.0
mpy = 773 = 0x0305
```

The stored `ti_draw` and `ti_image` modules in `AAA.8xg2` contain genuine
MicroPython persistent-code streams beginning with:

```text
4D 05 03 1F
M  format 5, feature flags 3, 31 small-int bits
```

The two module objects use this layout:

```text
13 02 D8 20
uint32_le total_object_size

uint24_le name_size
00                          # section kind 0
ascii_name
00

# An optional kind-1 menu-definition section is present in TI's modules.

uint24_le mpy_size
02                          # section kind 2
mpy_bytes                   # begins 4D 05 03 1F
00
```

In `AAA.8xg2`, `ti_draw` carries 5,011 bytes of `.mpy` data and `ti_image`
carries 1,360 bytes. The surrounding `PURG` GroupObject records identify both
members as internal type `0x11`, the same internal object class used for
external type-15 Python objects. The `13 02 D8 20` header selects module
subtype 2; the ordinary `13 01 00 00` header selects runnable source-program
subtype 1.

This subtype distinction is important. Replacing the source bytes inside a
subtype-1 program with an `.mpy` stream does not make a bytecode program. The
Python app still treats its kind-2 section as source. A subtype-2 object is
instead importable as `<name>.mpy` and is intentionally absent from the normal
Python file manager.

### Firmware path

The validator requires magic `M`, format version `5`, feature flags `3`, no
native architecture, and no more than 31 small-int bits. These checks exactly
match `4D 05 03 1F`.

Official MicroPython 1.13 `mpy-cross` produces compatible output with:

```sh
mpy-cross -mcache-lookup-bc -s test.py -o test.mpy test.py
```

The `-mcache-lookup-bc` option is mandatory for TI-Python: without it the
header is `4D 05 02 1F`, whose feature flags are rejected.

### Direct transfer and validation

A subtype-2 object does not need to be installed through Group/Ungroup. An
ordinary Evo CBOR file with `metaData.type = 15` and the raw module object in
its `data` byte string was accepted by a direct Archive transfer. It was
omitted from the USB directory and normal Python file manager, but remained
visible in the OS memory menu. A normal 14-byte source program containing
`import evoldr` imported and executed it.

`tivars_lib_cpp` recognizes both subtype-1 source objects and subtype-2
compiled modules. Its Evo file probe also accepts the omitted
`metaData.flags` field emitted by the direct USB sender, defaulting it to zero.


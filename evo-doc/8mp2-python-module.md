# Evo MicroPython bytecode modules: `.8xpy2` and `.8mp2`

The module object and MPY bytecode are identical across the OS 7.0/7.1
boundary. Only the external Evo CBOR container changes; renaming the file
extension alone is insufficient.

| Field                                  | OS 7.0.x                     | OS 7.1 and later      |
|----------------------------------------|------------------------------|-----------------------|
| File extension                         | `.8xpy2`                     | `.8mp2`               |
| `metaData.type`                        | 15                           | 18 (`0x12`)           |
| `metaData.version`                     | 1                            | 1                     |
| `metaData.flags` in known module files | absent                       | 1                     |
| Tokenized `metaData.name`              | no terminator in known files | ends in `00 00`       |
| Body `version`                         | 1                            | 1                     |
| Body `size` and CBOR `data` length     | stored payload length        | stored payload length |
| Bytes after the declared object        | optional, opaque             | optional, opaque      |
| Transfer destination                   | Archive                      | Archive               |

The `flags=1` value matches TI's type-18 files and the hardware-tested Dash
modules. `A5` is **not** a type-18 marker: it is an extra stored byte in TI's
TI_DRAW object, already present in OS 7.0. New objects need no padding;
existing payloads may contain opaque bytes beyond their inner object length.
The final two-byte file checksum is recomputed over the updated CBOR body,
using the algorithm in [common-container.md](common-container.md).

Source Python programs still use type 15 with a subtype-1 `13 01 00 00`
object on both OS generations. Do not turn those, CE `PYMP` AppVars, or
ordinary Evo AppVars into type 18 by changing only their container.

## Shared object and sections

The CBOR `data` byte string begins with:

```text
13 02 D8 20                compiled Python object, subtype 2
uint32_le object_size     includes this 8-byte header and all sections

repeated sections:
    uint24_le length
    uint8     kind
    byte      data[length]
    00                    section terminator, excluded from length

optional opaque bytes     excluded from object_size; included in CBOR size/data
```

Section kind `0` stores the ASCII import name, kind `1` stores optional
editor menu definitions, and kind `2` stores the MPY stream. Typical order
is name, optional menu, bytecode. Menus and bytecode are unchanged when
switching wrappers. The stream begins `4D 05 03 1F`: MicroPython format 5,
cache-lookup feature enabled, 31 small-int bits. Compile source with
MicroPython 1.13 `mpy-cross -mcache-lookup-bc`; CE MPY v3 needs recompilation.

For more on object parsing and the original firmware observations, see
[python-transfer-notes.md](python-transfer-notes.md).

## Stored bytes after the object (the former `A5` assumption)

There are two independent lengths: the Python header's `object_size`, which
ends after the final section terminator, and the CBOR `size`/byte-string
length, which describe the complete stored payload. The latter can be larger.
The term "outer trailer" in this library is only a descriptive API label,
not an identified TI field.

TI-provided exports inspected on 2026-09-10:

| File            | Inner object length | CBOR `size` / `data` length | Extra bytes |
|-----------------|---------------------|-----------------------------|-------------|
| `TI_DRAW.8mp2`  | 6923                | 6924                        | `A5`        |
| `TI_IMAGE.8mp2` | 2656                | 2656                        | none        |

The extra byte(s) don't seem to actually matter to the OS.

## Library API

Load either file normally with `TIVarFile::loadFromFile()`. The entry's
`evoTypeID` is `PythonScript` (15) or `PythonModule` (18), while readable
JSON reports `python.compiledModule = true` in both cases. Type-15 source
programs instead report `compiledModule = false`.

`python.bodyHex` contains the MPY stream, `python.menuDefinitionHex` the
menu bytes, and `python.trailerHex` the executable section's `00`
terminator. `python.outerTrailerHex` separately reports any opaque bytes
after the declared object, regardless of wrapper. `rawDataHex` retains the
entire `data` byte string, including unknown sections and outer bytes.

Loaded type-18 entries use an Evo-only `PythonModule` descriptor, not an AppVar.
`PythonModuleAppVar` remains the CE `PYMP` type and has no Evo mapping.
The descriptor's `getId()` is -1 (no legacy ID); use the entry's `evoTypeID`
for its Evo type, 18. Neither `PythonModule` nor `8mp2` is registered in
`TIVarTypes`, so the legacy name-based constructors cannot create this type.
Existing modules can still be loaded, edited, and saved:

```cpp
auto module = tivars::TIVarFile::loadFromFile("DEMO.8mp2");
module.setContentFromString(R"({
  "python": {
    "name": "demo",
    "bodyHex": "...complete MPY v5 bytes as hex...",
    "menuDefinitionHex": "...optional menu bytes as hex..."
  }
})");
module.saveVarToFile("/output", "DEMO"); // DEMO.8mp2
```

Omit `menuDefinitionHex` if no menu is needed. These hex values are
placeholders, not a runnable module. The writer emits the subtype-2 object,
section lengths/terminators, metadata type 18, flags 1, terminated token
name, lengths, and checksum, without adding padding. It validates the MPY
header but does not validate or compile the instructions in the stream.

For legacy OS 7.0 output, create `PythonAppVar` on model `84Evo` instead
and include `"compiledModule": true` in the `python` object. This emits a
type-15 `.8xpy2` compiled object. Omitting that boolean retains
the existing source-program writer. The library's new legacy containers
use its existing terminated name and metadata flags convention; loading
an existing file preserves its actual name bytes and flags presence.

For raw object construction, `EvoFormat::build_evo_python_module_payload`
accepts the same JSON and optional default import name; it needs no wrapper
selector, since both wrappers use the same bytes. With `rawDataHex`, it
validates subtype 2 and preserves the entire payload, including unknown
internal sections and outer bytes.
`setContentFromData()` remains a raw API: supply the **complete object**,
not just the `.mpy` stream. No trailing `A5` is required.

Saving an existing file retains its type. Rebuilding from structured
fields without `rawDataHex` emits the known name/menu/code sections;
keep `rawDataHex` for lossless preservation of unknown or repeated sections.
Evo-to-CE and CE-to-Evo compiled-module conversion is rejected rather than
mislabeling incompatible bytecode.

### Switching wrappers

```cpp
auto module = tivars::TIVarFile::loadFromFile("DEMO.8xpy2");
module.convertToEvoPythonFormat("8mp2");
module.saveVarToFile("/output", "DEMO"); // DEMO.8mp2
module.convertToEvoPythonFormat("8xpy2");
module.saveVarToFile("/legacy-output", "DEMO"); // DEMO.8xpy2
```

The same method is exposed on the WASM `TIVarFile` class. With the CLI:

```sh
tivars_cli -i DEMO.8xpy2 -o DEMO.8mp2 --python-format 8mp2
tivars_cli -i DEMO.8mp2 -o DEMO.8xpy2 --python-format 8xpy2
```

Conversion preserves the complete CBOR `data` byte string byte-for-byte,
including MPY, menus, unknown/repeated internal sections, and opaque outer
bytes. It updates the container type, metadata, name terminator, and checksum
according to the table above; the payload length does not change.
Source scripts, CE modules, non-Python variables, and incompatible
MPY headers are also rejected without changing the variable.

A same-format call leaves the existing representation alone. Switching
formats normalizes metadata to the target convention, so switching back
preserves the object but need not reproduce every original container byte.
Use `saveVarToFile()` without conversion for an unchanged-format round trip.
The CLI does not infer conversion from a renamed output extension: request
`--python-format` explicitly.

## Validation

Physical tests established type-18 rejection on OS 7.0, type-15 bytecode
rejection on OS 7.1.0.4421, and successful type-18 transfers/readbacks on
7.1.0.4421. Fresh hardware tests of the corrected packer/Python sender on
7.1.0.4421 transferred and read back unpadded objects of 72, 73, 74, and 75
bytes, and legacy-wrapper objects with menus of 149, 150, 151, and 152 bytes
after automatic type-18 conversion. All readbacks were byte-exact, and all
variables were archived even when RAM was requested. Additional transfers
preserved `A6` and `A6 00 FF 42 01` tails byte-exactly through either input
wrapper. Temporary test variables were removed afterward.

The rebuilt native libticalcs sender also passed a direct type-18 transfer
and two real type-15 `DP`-triggered retries: an unpadded module with menus and
a module with the five-byte opaque tail above. Python-tool readbacks matched
the expected type-18 files byte-for-byte.

Offline tests additionally cover empty, `A5`, and arbitrary multibyte tails,
both conversion directions, source/invalid-input rejection, and the two
official exports. There was no new OS 7.0 hardware run or calculator-side
Python execution test for this correction.

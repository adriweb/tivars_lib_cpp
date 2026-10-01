# tivars_lib_cpp [![Build](https://github.com/adriweb/tivars_lib_cpp/actions/workflows/build.yml/badge.svg)](https://github.com/adriweb/tivars_lib_cpp/actions/workflows/build.yml)
A C++ "library" to interact with TI-Z80/eZ80 (82/83/84 series) calculators files (programs, lists, matrices...).  
JavaScript bindings (for use with emscripten) are provided for convenience.

### How to use

#### In C++

Right now, the best documentation is [the tests file](tests.cpp) itself, which uses the main API methods.  
Basically, though, there are loading/saving/conversion (data->string, string->data) methods you just have to call.

**Example 1**: Here's how to read the source of TI-Basic program from an .8xp file and print it:
```cpp
auto myPrgm = TIVarFile::loadFromFile("the/path/to/myProgram.8xp");
auto basicSource = myPrgm.getReadableContent(); // You can pass options like { {"reindent", true} }...
std::cout << basicSource << std::endl;
```
**Example 2**: Here's how to create a TI-Basic program (output: .8xp file) from a string:
```cpp
auto newPrgm = TIVarFile::createNew("Program");                       // Create an empty "container" first
newPrgm.setVarName("TEST");                                           // (also an optional parameter above)
newPrgm.setContentFromString("ClrHome:Disp \"Hello World!\"");        // Set the var's content from a string
newPrgm.saveVarToFile("path/to/output/directory/", "myNewPrgrm");     // The extension is added automatically
```

Several optional parameters for the functions are available. For instance, French input/output for tokenized content can be selected with an options map such as `{ {"lang", TH_Tokenized::LANG_FR} }`, and pretty-printing can enable reindentation with `{ {"reindent", true} }`.

Inside an ordinary TI-BASIC string, text is normally encoded character by character. Prefix a readable token name with `\` to force it to be encoded as one calculator token instead: `"sin(X` stores literal text, while `"\sin(X` stores the `sin(` token followed by `X`. This named form works for both legacy CE and Evo programs, including token names containing spaces such as `\ and \`; the trailing backslash remains a token boundary. Outside ordinary strings, `\` keeps its zero-width token-boundary meaning because normal code, equation strings, and evaluated `Send(`/`expr(` strings already use maximal munch. Raw `\xHH` and `\uHHHH` token escapes remain available when no unambiguous readable token name exists; write `\\` for a literal backslash token.

_Note: The code throws exceptions for you to catch in case of trouble._

#### Embedding in another C++ app

For apps that want to vendor the library without Git submodules or a CMake integration, generate the amalgamated distribution files:

```sh
make amalgamated
```

This writes:

- `dist/tivars_lib_cpp.hpp`
- `dist/tivars_lib_cpp.cpp`

Copy those two files into the host project and compile the `.cpp` once alongside the app:

```sh
c++ -std=c++20 -Ipath/to/tivars-dist \
    path/to/tivars-dist/tivars_lib_cpp.cpp \
    app.cpp \
    -o app
```

The generated `.cpp` contains the vendored JSON/pugixml code and token tables, so no other files from this repo are needed.

Graph DataBase (`.8xd`) readable JSON conversion is enabled automatically when the compiler/library support the required C++ features. If the host app does not need it, compile with `-DTH_GDB_SUPPORT=0` to force that implementation off:

```sh
c++ -std=c++20 -DTH_GDB_SUPPORT=0 -Ipath/to/tivars-dist \
    path/to/tivars-dist/tivars_lib_cpp.cpp \
    app.cpp \
    -o app
```

With CMake, the equivalent override is:

```sh
cmake -S . -B build -DTIVARS_GDB_SUPPORT=OFF
```

If `TIVARS_GDB_SUPPORT` is not set, CMake leaves `TH_GDB_SUPPORT` undefined and the header auto-detects support. `ON` and `OFF` map to `TH_GDB_SUPPORT=1` and `TH_GDB_SUPPORT=0`.

#### In JavaScript (via Emscripten)

Bindings are done for the necessary classes, so it should be pretty obvious.  
Integration example:
```html
<script type="module">
    import TIVarsLib from './TIVarsLib.js';
    const lib = await TIVarsLib();
    const prgm = lib.TIVarFile.createNew("Program", "TEST");
    prgm.setContentFromString("ClrHome:Disp \"Hello World!\"");
    const filePath = prgm.saveVarToFile("", "MyTestProgram");
    const file = lib.FS.readFile(filePath, {encoding: 'binary'});
    ...
</script>
```

You can find code that use this project as a JS lib here: https://github.com/TI-Planet/zText (look at `generator.js`)

#### On macOS: Quick Look app extensions

This repo ships a modern macOS Quick Look host app with embedded Preview and Thumbnail extensions, which is the supported replacement for the removed legacy `.qlgenerator` plugin model on macOS 15+.

Build it with:
```sh
cmake -S . -B build
cmake --build build --target tivars_quicklook
```

That produces `build/TIVarsQuickLook.app`, containing:
- `TIVarsQuickLookPreview.appex`
- `TIVarsQuickLookThumbnail.appex`

Install the app bundle for the current user with:
```sh
mkdir -p ~/Applications
cp -R build/TIVarsQuickLook.app ~/Applications/
qlmanage -r
```

The CMake build signs the app and both extensions automatically with the configured `TIVARS_QUICKLOOK_CODESIGN_IDENTITY` (ad hoc in CI). For a local ad hoc build, pass `-DTIVARS_QUICKLOOK_CODESIGN_IDENTITY=-` to CMake.

The Preview extension returns rich HTML previews for parsed legacy, Evo, and flash metadata and readable content when available. The Thumbnail extension renders custom badges/cards keyed off the detected TI file type. Evo's `8xn2` through `8xpy2` file extensions, plus `8mp2` Python modules, are registered alongside the pre-Evo formats.

If macOS does not pick the extensions up immediately, useful diagnostics are:
```sh
pluginkit -m -A -D -p com.apple.quicklook.preview
pluginkit -m -A -D -p com.apple.quicklook.thumbnail
```

### CI binary downloads and macOS signing

The Build workflow uploads three distributions under the run's **Artifacts**:

- Linux x86_64: `tivars_cli` in a `.tar.gz` (Ubuntu 24.04 / glibc 2.39 or newer).
- Windows x86_64: `tivars_cli.exe` in a `.zip`, with the C/C++ runtime linked statically.
- macOS universal (Intel and Apple Silicon, macOS 12+): `tivars_cli` and `TIVarsQuickLook.app` with both embedded extensions in a `.zip`.

Each archive includes the license, third-party notices and this README, with a `SHA256SUMS` file alongside it. The CLI embeds its token table and needs no separate token XML download. Extract the archive before using it; on macOS, copy the app to `~/Applications` and launch it once to register Quick Look. The outer Actions artifact ZIP contains the distribution archive and checksums; the inner archive preserves executable permissions and macOS bundle metadata.

Pushes and pull requests produce development downloads. A `v*` tag pushed to `adriweb/tivars_lib_cpp` automatically signs and notarizes the macOS CLI, app and both extensions. To test signing without a tag, run **Build** manually on `master` or `evo` and enable **sign_and_notarize**. This uploads artifacts without publishing a GitHub Release. Signed macOS archive names include `notarized`; a signing or notarization failure prevents their upload.

Configure the same repository Actions secrets as Safari WebUSB / CEmu:

| Secret | Value |
| --- | --- |
| `MACOS_CERTIFICATE` | Base64-encoded Developer ID Application `.p12`, including the private key |
| `MACOS_CERTIFICATE_PWD` | Password used to export the `.p12` |
| `MACOS_KEYCHAIN_PWD` | Password for the disposable CI keychain |
| `MACOS_CODESIGN_IDENT` | Developer ID Application identity name or certificate SHA-1 |
| `APPLE_NOTARIZATION_USERNAME` | Apple ID used for notarization |
| `APPLE_NOTARIZATION_PASSWORD` | Apple ID app-specific password |
| `APPLE_NOTARIZATION_TEAMID` | Ten-character Apple Developer team ID matching the signing certificate |

Signing runs only for upstream version-tag pushes and explicitly requested upstream manual builds. Pull requests and forks never receive signing credentials. The temporary keychain is added to the search list for private-key lookup, then the original list is restored and the keychain deleted on exit. The script signs extensions before the containing app, keeps their sandbox entitlements, enables hardened runtime and secure timestamps, and submits the CLI and app together to `notarytool`.

After Apple accepts the submission, the app's ticket is stapled and validated, and Gatekeeper must report **Notarized Developer ID**. Standalone CLI executables cannot be stapled; their ticket is verified online with `codesign --check-notarization`. Final packaging happens after these checks. Downloads are also checked for both macOS architectures and accidental dependencies on runner/Homebrew libraries.

For a local release build with those variables already in the environment:

```sh
bash scripts/sign-and-notarize.sh build/package/tivars_lib_cpp_cli build/package/TIVarsQuickLook.app
python3 scripts/package-binaries.py --platform macos --architecture universal --label local-notarized
```

### Automated fuzzing

A libFuzzer target is available through CMake and exercises both `TIVarFile` and `TIFlashFile` parsing, plus a small amount of post-parse processing and single-entry roundtripping.
Check ./run_fuzz.sh for details.

### Vartype handlers implementation: current status

| Vartype                   | data->string | string->data |
|---------------------------|:------------:|:------------:|
| Real                      |    **✓**     |    **✓**     |
| Real List                 |    **✓**     |    **✓**     |
| Matrix                    |    **✓**     |    **✓**     |
| Equation                  |    **✓**     |    **✓**     |
| String                    |    **✓**     |    **✓**     |
| Program                   |    **✓**     |    **✓**     |
| Protected Program         |    **✓**     |    **✓**     |
| Graph DataBase (GDB)      | **✓** (JSON) | **✓** (JSON) |
| Complex                   |    **✓**     |    **✓**     |
| Complex List              |    **✓**     |    **✓**     |
| Window Settings           | **✓** (JSON) | **✓** (JSON) |
| Recall Window             | **✓** (JSON) | **✓** (JSON) |
| Table Range               | **✓** (JSON) | **✓** (JSON) |
| Picture                   | **✓** (JSON metadata) | **✓** (`rawDataHex` JSON) |
| Image                     | **✓** (JSON metadata) | **✓** (`rawDataHex` JSON) |
| Application Variable      |    **✓**     |    **✓**     |
| Python AppVar             |    **✓**     |    **✓**     |
| Python Module AppVar      | **✓** (JSON) | **✓** (JSON) |
| Python Image AppVar       | **✓** (JSON) | **✓** (JSON) |
| StudyCards AppVar         | **✓** (JSON) | **✓** (`rawDataHex` JSON) |
| StudyCards Setgs AppVar   | **✓** (JSON) | **✓** (JSON) |
| CellSheet AppVar          | **✓** (JSON) | **✓** (JSON) |
| CellSheet State AppVar    | **✓** (JSON) | **✓** (JSON) |
| CabriJr AppVar            | **✓** (JSON) | **✓** (JSON) |
| Notefolio AppVar          | **✓** (JSON) | **✓** (JSON) |
| Group Object              | **✓** (JSON) | **✓** (JSON) |
| Backup                    | **✓** (JSON) | **✓** (JSON) |
| Exact Complex Fraction    |    **✓**     |    **✓**     |
| Exact Real Radical        |    **✓**     |    **✓**     |
| Exact Complex Radical     |    **✓**     |    **✓**     |
| Exact Complex Pi          |    **✓**     |    **✓**     |
| Exact Complex Pi Fraction |    **✓**     |    **✓**     |
| Exact Real Pi             |    **✓**     |    **✓**     |
| Exact Real Pi Fraction    |    **✓**     |    **✓**     |
| Operating System / Flash App / Certificate | **✓** (JSON metadata) | **✓** (JSON) |

Special vartype naming rules are implemented for constrained names such as strings, lists, matrices, equations, pictures, images, GDBs, and settings vars.

Picture/image support exposes metadata as JSON and supports `rawDataHex` import/export for exact roundtrips; raw pixel decoding/encoding is still not implemented.
Flash file support exposes header/object metadata as JSON and supports JSON -> file reconstruction, including multi-header files.
Structured AppVar support includes generic subtype detection from raw data / JSON for Python modules, Python images, StudyCards, StudyCards settings, CellSheet, CellSheet state, CabriJr, and Notefolio payloads.

Evo MicroPython modules support both OS 7.0 type-15 `.8xpy2` and OS 7.1+
type-18 `.8mp2` containers, including their menus. Loaded Evo `PythonModule`
variables retain their format on save; they are not registered as legacy types.
`convertToEvoPythonFormat()` converts between the two bytecode wrappers.
For varfile conversions, the CLI infers the Python format from the output
extension. Source Python programs remain type 15.
See [the bytecode format and API examples](evo-doc/8mp2-python-module.md).
JSON schemas for the JSON-compatible formats are available in `schemas/`.

Big thanks to @LogicalJoe for his research in https://github.com/TI-Toolkit/tivars_hexfiend_templates/

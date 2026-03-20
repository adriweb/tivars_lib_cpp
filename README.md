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

_Note: The code throws exceptions for you to catch in case of trouble._

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
cmake --build build --target tivars_quicklook_app
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

The CMake build ad hoc-signs the app and both extensions automatically when `codesign` is available.

The Preview extension returns rich HTML previews for parsed variable/flash metadata and readable content when available. The Thumbnail extension renders custom badges/cards keyed off the detected TI file type.

If macOS does not pick the extensions up immediately, useful diagnostics are:
```sh
pluginkit -m -A -D -p com.apple.quicklook.preview
pluginkit -m -A -D -p com.apple.quicklook.thumbnail
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
JSON schemas for the JSON-compatible formats are available in `schemas/`.

Big thanks to @LogicalJoe for his research in https://github.com/TI-Toolkit/tivars_hexfiend_templates/

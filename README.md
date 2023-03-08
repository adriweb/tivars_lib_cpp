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
auto newPrgm = TIVarFile::createNew(TIVarType::createFromName("Program"));  // Create an empty "container" first
newPrgm.setVarName("TEST");                                           // (also an optional parameter above)
newPrgm.setContentFromString("ClrHome:Disp \"Hello World!\"");        // Set the var's content from a string
newPrgm.saveVarToFile("path/to/output/directory/", "myNewPrgrm");     // The extension is added automatically
```

Several optional parameters for the functions are available. For instance, French is a supported input/output language for the program vartype, which is choosable with a boolean in an options array to pass.

_Note: The code throws exceptions for you to catch in case of trouble._

#### In JavaScript (via Emscripten)

Bindings are done for the necessary classes, so it should be pretty obvious.  
You can find code that use this project as a JS lib here: https://github.com/TI-Planet/zText (look at `generator.js`)

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
| Application Variable      |    **✓**     |    **✓**     |
| Python AppVar             |    **✓**     |    **✓**     |
| Exact Complex Fraction    |    **✓**     |              |
| Exact Real Radical        |    **✓**     |              |
| Exact Complex Radical     |    **✓**     |              |
| Exact Complex Pi          |    **✓**     |              |
| Exact Complex Pi Fraction |    **✓**     |              |
| Exact Real Pi             |    **✓**     |              |
| Exact Real Pi Fraction    |    **✓**     |              |

Note that some of the special varnames restrictions (for strings, matrices, list...) aren't implemented yet.

To this date, there are no plans to support other types (except maybe some fancy things with the image/picture vartypes...).  
Pull Requests are welcome, though :)

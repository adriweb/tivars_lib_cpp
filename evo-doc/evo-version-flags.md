# Evo `version` and `flags` fields

There are two layers of fields:

```text
metaData.version   metadata/envelope schema version
metaData.flags     variable-level metadata flags

version            payload schema version
flags              payload-specific flags, when present
```

`metaData.version` and `metaData.flags` live in the nested `metaData`
map with the variable type and tokenized name. Body `version` and body
`flags` are top-level fields beside payload fields such as `size`,
`arraylen`, `rows`, `cols`, and `len`.

## Observed `version` values

All currently parsed Evo variable files use version `1`:

```text
metaData.version = 1
body version     = 1, where a body version field exists
```

No sample currently proves another version value. Writers should emit
`1`. Readers should preserve the value, because the calculator has an
incompatible-variable-version transfer error.

## Observed `flags` values by type

```text
type  ext     meaning                  metaData.flags      body flags
0     .8xn2   real/complex number      0, except 8 seen    0 or 6
1     .8xl2   list                     0                  no body flags key
2     .8xp2   TI-BASIC program         0                  no body flags key
4     .8ci2   Pic                      1                  no body flags key
5     .8ca2   Image/background         1                  no body flags key
6     .8xm2   matrix                   0                  no body flags key
7     .8xy2   equation                 4                  no body flags key
8     .8xv2   AppVar                   0                  no body flags key
12    .8xw2   Window settings          0                  top data is array
13    .8xz2   RclWindw/user zoom       0                  top data is array
14    .8xt2   Table setup              0                  top data is array
17            nested scalar setting    0 or 8             0
```

## Body `flags = 6`

Body `flags = 6` is the clearest decoded flag so far. It marks a
top-level `.8xn2` exact fraction. The exact-fraction payload tag is also
stored in the scalar data itself; the body flag appears to preserve the
display/type state for the stored number.

For lists and matrices, the same exact-fraction state is not stored as a
body `flags` key. It is stored in trailing one-byte flags instead:

```text
.8xl2 list:    one trailing byte per displayed element
.8xm2 matrix:  one trailing byte per displayed cell

00  normal
06  exact fraction
```

Those per-element/per-cell flags are included in `size` but excluded
from `arraylen`.

## `metaData.flags = 1` for image-like variables

Both observed image-like formats use `metaData.flags = 1`:

```text
.8ci2 Pic/background picture    metaData.type = 4, metaData.flags = 1
.8ca2 Image/background image    metaData.type = 5, metaData.flags = 1
```

Connect Evo's image conversion helper also produces output with a fixed literal CBOR header:

```text
metaData.type    = 5
metaData.version = 1
metaData.flags   = 1
version          = 1
size             = 33601
data marker      = 0x0B in generated files
```

The exact semantic name for this bit is not proven, but it is clearly a
variable-level image/Pic metadata flag rather than a pixel-data flag.

## `metaData.flags = 4` for equations

The current `.8xy2` samples use:

```text
metaData.type    = 7
metaData.flags   = 4 or 6
```

The body has no `flags` key. This likely belongs to equation
graph/display state, not to the expression token stream itself.

In the observed Evo GDB sample, `testData/evo/more/GDB1.8xd2`, the
equation display metadata is stored in `NQEI` records next to the
equation expression records. The X1T/Y1T pair was disabled in the
equation editor and has `NQEI` flags `2`; enabled pairs in the same
file use `4` or `6`. This suggests bit `0x04` is tied to enabled /
plotted equation state, while the exact meaning of bit `0x02` is still
unknown.

The online calculator WASM contains a field-name table that includes
common fields and graph/equation metadata fields:

```text
name type size version flags data arraylen len rows cols product metaData
lineStyle color formulasize formula coordType coordOnOff gridType
gridColor axesColor axesLabelOnOff exprOnOff detectPOI background
seqStyle plotIdx plotOnOff plotType xList yList plotFreq plotAxis plotMark
width height bpp dispName tokName mem ram archive language ptt pmtBegOrEnd
```

That suggests equation, plot, graph, and image objects use one broader
metadata vocabulary, with `flags` only one compact status field.

## `metaData.flags = 8` for graph/window coordinate variables

`metaData.flags = 8` has appeared in top-level scalar variables:

```text
X.8xn2    metaData.flags = 8, body flags = 0
Y.8xn2    metaData.flags = 8, body flags = 0
```

and in nested scalar entries inside `testData/evo/Window.8xw2`:

```text
XMIN      metaData.flags = 8
XMAX      metaData.flags = 8
YMIN      metaData.flags = 8
YMAX      metaData.flags = 8
```

Other window values in the same file use `metaData.flags = 0`. A
second window sample, `testData/evo/more/Window.8xw2`, and a
Parametric-mode sample, `testData/evo/more/Window_parametric.8xw2`,
use `metaData.flags = 0` for all nested entries, including `XMIN`,
`XMAX`, `YMIN`, and `YMAX`.

Therefore bit `0x08` is not simply "decimal value" or "approximate
number"; many decimal scalar values use metadata flag `0`.

The current best description is:

```text
metaData.flags bit 0x08 appears tied to special graph/window coordinate
variable metadata context, especially X/Y and X/Y min/max, but observed
files do not always set it for those nested window variables.
```

The precise semantic name is still unknown.

## Settings containers and nested type 17

Top-level settings containers such as `.8xw2`, `.8xz2`, and `.8xt2`
do not store ordinary scalar body fields at the top level. Their `data`
field is an indefinite CBOR array of nested entries.

Each nested scalar setting currently has:

```text
metaData.type    = 17
metaData.version = 1
metaData.flags   = 0 or 8

version          = 1
flags            = 0
arraylen         = number of 16-bit scalar-expression words
size             = data byte length
data             = scalar payload
```

The nested entry body matches `.8xn2` scalar structure. The nested
`metaData.flags = 8` pattern is the coordinate-variable pattern
described above.

## Reader/writer guidance

For a reader:

- parse and preserve both metadata-level and body-level fields;
- do not collapse `metaData.flags` and body `flags`;
- treat unknown flag bits as meaningful and round-trip them;
- use body `flags = 6` and trailing `06` element/cell flags as exact
  fraction display/type indicators.

For a writer:

- emit `metaData.version = 1`;
- emit body `version = 1` where the format uses scalar body fields;
- emit `metaData.flags = 1` for type `4` and type `5` image-like
  variables;
- emit body `flags = 6` for top-level exact-fraction `.8xn2` values;
- emit trailing `06` flags for exact-fraction list and matrix elements;
- preserve `metaData.flags = 8` for graph/window coordinate variables
  when converting or round-tripping existing data.

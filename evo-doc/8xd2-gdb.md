# `.8xd2` graph databases

Sample file: `testData/evo/more/GDB1.8xd2`.

Metadata:

```text
metaData.type    = 3
metaData.version = 1
metaData.flags   = 0
metaData.name    = E890 0000
```

`E890` is `TOK_VAR_GDB1`.

Body fields:

```text
version = 1
size    = 836
data    = Evo graph database payload
```

The payload is not the same flat byte layout as pre-Evo `.8xd` graph
database data. The observed payload begins with a tokenized settings
header, followed by records marked with ASCII tags:

```text
RAVI  variable/value record
NQEI  equation display metadata record
```

Observed header words:

```text
000A
E58B TOK_PARAM
0001
0000 TOK_EOS
0000 TOK_EOS
0000 TOK_EOS
0010
E5B2 TOK_RECTG
E5B3 TOK_COORDON
E606 TOK_GRIDOFF
E5AC TOK_MEDGRAY
E5A2 TOK_BLACK
E608 TOK_LBLOFF
E4F7 TOK_EXPR_ON
E50B TOK_CRIT_PNTS_ON
E5B7 TOK_BACK_OFF
E5BB TOK_DETECT_ASYM_ON
E60A TOK_WEBOFF
```

The first `RAVI` records hold Parametric window settings:

```text
token  payload
E996   TMIN
E997   TMAX
E998   T_STEP
E98F   XMIN
E990   XMAX
E991   XSCL
E993   YMIN
E994   YMAX
E995   YSCL
```

The equation section alternates `NQEI` display metadata records and
`RAVI` equation expression records. In this sample:

```text
pair       NQEI style  NQEI flags  NQEI color token  expr
X1T/Y1T    8           2           E5A0 TOK_BLUE     X1T=2T, Y1T=-5.5T
X2T/Y2T    6           4           E5A1 TOK_RED      X2T=cos(T/2, Y2T=sin(T/3
X3T/Y3T    4           4           E5A4 TOK_GREEN    X3T=T^2, Y3T=-T
X4T/Y4T    1           6           E5A3 TOK_MAGENTA  X4T=0, Y4T=T
X5T/Y5T    1           0 or 1      E5A4 TOK_GREEN    empty
X6T/Y6T    1           0 or 1      E5A5 TOK_ORANGE   empty
```

The X1T/Y1T pair was disabled in the equation editor when this GDB was
exported. Its `NQEI` flags are `2`, while the enabled pairs in this
sample use `4` or `6`. This suggests bit `0x04` is tied to enabled /
plotted equation state, but the exact meaning of bit `0x02` is still
not fully proven.

The `NQEI` style values are not currently mapped to the legacy GDB
`GraphStyle` enum. Observed values in this sample are:

```text
8  blue thin line
6  red dotted line
4  green connected line
1  magenta thick line
```


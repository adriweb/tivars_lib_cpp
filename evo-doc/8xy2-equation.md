# `.8xy2` equations

Sample files:

- `testData/evo/Y1.8xy2`
- `testData/evo/more/X1T.8xy2` through `Y4T.8xy2`

Metadata:

```text
metaData.type    = 7
metaData.version = 1
metaData.flags   = 4 or 6 in the samples
metaData.name    = equation token, little-endian, then 0000
```

The body has no `flags` key. The observed metadata flags appear to
belong to equation graph/display state rather than to the expression
token stream. In the standalone parametric equation samples, `X1T` /
`Y1T` through `X3T` / `Y3T` use `metaData.flags = 4`, while `X4T` /
`Y4T` use `metaData.flags = 6`.

The standalone equation files do not expose separate `lineStyle` or
`color` keys. In the observed Evo GDB sample, pair style/color and
enabled-state metadata are carried by adjacent `NQEI` records, not by
the individual `.8xy2` expression records.

For `Y1.8xy2`:

```text
name bytes = 40 e8 00 00
name words = E840 0000
name token = TOK_VAR_EQU_Y1
```

Body fields:

```text
version   = 1
arraylen  = number of 16-bit token words in data
size      = byte length of data; equals arraylen * 2
data      = byte string of little-endian 16-bit expression tokens
```

This is structurally close to a `.8xp2` program, but the token stream is
an expression rather than a multi-line program.

Parametric samples:

```text
file      name bytes   metaData.flags  data hex                         code
X1T.8xy2  50 e8 00 00  4               03 e4 13 e8 00 00               2T
Y1T.8xy2  51 e8 00 00  4               2e e4 06 e4 0b e4 06 e4 13 e8 00 00
                                                                        -5.5T
X2T.8xy2  52 e8 00 00  4               41 e4 13 e8 2b e4 03 e4 00 00   cos(T/2
Y2T.8xy2  53 e8 00 00  4               3f e4 13 e8 2b e4 04 e4 00 00   sin(T/3
X3T.8xy2  54 e8 00 00  4               13 e8 2c e4 03 e4 00 00         T^2
Y3T.8xy2  55 e8 00 00  4               2e e4 13 e8 00 00               -T
X4T.8xy2  56 e8 00 00  6               01 e4 00 00                     0
Y4T.8xy2  57 e8 00 00  6               13 e8 00 00                     T
```

`Y1.8xy2` token stream:

```text
00 E401 TOK_0
01 E428 TOK_ADD
02 E404 TOK_3
03 E42D TOK_X_ROOT
04 E410 TOK_LPAREN
05 E440 TOK_ASIN
06 E442 TOK_ACOS
07 E441 TOK_COS
08 E43F TOK_SIN
09 E440 TOK_ASIN
10 E442 TOK_ACOS
11 E444 TOK_ATAN
12 E443 TOK_TAN
13 E441 TOK_COS
14 E43F TOK_SIN
15 E817 TOK_VAR_X
16 E411 TOK_RPAREN
17 E411 TOK_RPAREN
18 E411 TOK_RPAREN
19 E411 TOK_RPAREN
20 E411 TOK_RPAREN
21 E411 TOK_RPAREN
22 E411 TOK_RPAREN
23 E411 TOK_RPAREN
24 E411 TOK_RPAREN
25 E411 TOK_RPAREN
26 E411 TOK_RPAREN
27 0000 TOK_EOS
```

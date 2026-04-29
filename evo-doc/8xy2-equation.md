# `.8xy2` equations

Sample file: `Y1.8xy2`.

Metadata:

```text
metaData.type    = 7
metaData.version = 1
metaData.flags   = 4 in the sample
metaData.name    = equation token, little-endian, then 0000
```

The body has no `flags` key. The observed metadata flag `4` appears to
belong to equation graph/display state rather than to the expression
token stream. More equation samples are needed before naming this bit.

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

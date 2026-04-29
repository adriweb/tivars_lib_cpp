# `.8xm2` matrices

Sample files: original `A.8xm2` plus controlled matrix samples.

Metadata:

```text
metaData.type    = 6
metaData.version = 1
metaData.flags   = 0
metaData.name    = matrix token, little-endian, then 0000
```

Observed matrix name tokens:

```text
[A]  E820 TOK_VAR_MAT_A
[B]  E821 TOK_VAR_MAT_B
[C]  E822 TOK_VAR_MAT_C
[D]  E823 TOK_VAR_MAT_D
[E]  E824 TOK_VAR_MAT_E
[F]  E825 TOK_VAR_MAT_F
```

Body fields:

```text
version   = 1
rows      = displayed row count
cols      = displayed column count
arraylen  = number of 16-bit expression words, excluding trailing flags
size      = byte length of data, including trailing flags
data      = nested expression-list payload plus one-byte cell flags
```

## Data layout

Matrix data reuses the same expression-list framing seen in `.8xl2`
lists:

```text
00E5                    matrix list start
  00E5 ... 00D9         row list
  00E5 ... 00D9         row list
  ...
00D9                    matrix list end
flags[rows * cols]      one byte per displayed cell
```

The payload stores both dimensions in reverse display order. For a
payload position counted from 1:

```text
display_row = rows - payload_row + 1
display_col = cols - payload_col + 1
```

So payload row 1, column 1 is the displayed bottom-right cell.

`arraylen` counts only the 16-bit words through the final matrix `00D9`.
The trailing flags are excluded from `arraylen` and included in `size`.
Unlike the payload cells, the flag table is in display row-major order:

```text
flag_index = (display_row - 1) * cols + (display_col - 1)
```

Observed flag values:

```text
00  normal real or complex cell
06  exact fraction cell
```

## Small controlled matrices

The controlled samples confirm the reversed payload order:

```text
[A] = [[1]]
payload:
  row 1: 1

[B] = [[1,2]]
payload:
  row 1: 2, 1

[C] = [[1][2]]
payload:
  row 1: 2
  row 2: 1

[D] = [[1,2][3,4]]
payload:
  row 1: 4, 3
  row 2: 2, 1
```

`[E]` is a 3x3 matrix with display values:

```text
[ 1 0 2 ]
[ 0 5 0 ]
[ 7 0 9 ]
```

Its payload is:

```text
row 1: 9, 0, 7
row 2: 0, 5, 0
row 3: 2, 0, 1
```

`[F]` is a 2x2 mixed-type matrix with display values:

```text
[ 1/3  9.8 ]
[ -4   2i  ]
```

Its payload is:

```text
row 1: 2i, -4
row 2: 9.8, 1/3
```

The `[F]` flag table is:

```text
06 00 00 00
```

That matches display row-major order: `[F](1,1)` is the exact fraction
`1/3`; the other three cells have flag `00`.

## `A.8xm2`

The original larger sample `A.8xm2` has:

```text
rows     = 12
cols     = 7
arraylen = 202
size     = 488
```

The expression region is `202 * 2 = 404` bytes. The remaining
`488 - 404 = 84` bytes are the per-cell flag table (`12 * 7`).

All zero cells use:

```text
0000 001F
```

Non-zero cells in payload order:

```text
payload row 6, col 4  -> display [A](7,4)
  value 1/3
  words 0003 0001 0001 0001 0021

payload row 9, col 5  -> display [A](4,3)
  value 9.8
  words 0000 0000 0000 9800 0001 0023

payload row 12, col 7 -> display [A](1,1)
  value 1
  words 0001 0001 001F
```

The only non-zero flag byte in the original `A.8xm2` is at zero-based
index 45:

```text
index = (display_row - 1) * cols + (display_col - 1)
45    = (7 - 1) * 7 + (4 - 1)
```

That flag is `06`, matching the exact fraction at `[A](7,4)`. Other
cells have flag `00`.

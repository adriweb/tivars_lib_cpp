# `.8xl2` lists

Sample files: `L1.8xl2` through `L6.8xl2`, plus custom named list samples.

Metadata:

```text
metaData.type    = 1
metaData.version = 1
metaData.flags   = 0
metaData.name    = list token, little-endian, then 0000
```

Observed list name tokens:

```text
L1  E830 TOK_VAR_LIST_1
L2  E831 TOK_VAR_LIST_2
L3  E832 TOK_VAR_LIST_3
L4  E833 TOK_VAR_LIST_4
L5  E834 TOK_VAR_LIST_5
L6  E835 TOK_VAR_LIST_6
```

Custom named lists use `TOK_VAR_LIST_NAME` followed by variable-letter
tokens, then `0000`. The user-facing list marker `(L)` is not stored as
an ordinary `L` character; it is represented by `E836`. The calculator
UI allows custom list names up to five letters; the exported files
currently present in the sample tree include one-letter, three-letter,
and generated formula-list names.

Examples:

```text
(L)A:
  E836 E800 0000
  TOK_VAR_LIST_NAME TOK_VAR_A EOS

(L)B:
  E836 E801 0000
  TOK_VAR_LIST_NAME TOK_VAR_B EOS

(L)ABC:
  E836 E800 E801 E802 0000
  TOK_VAR_LIST_NAME TOK_VAR_A TOK_VAR_B TOK_VAR_C EOS

(L)XXX:
  E836 E817 E817 E817 0000
  TOK_VAR_LIST_NAME TOK_VAR_X TOK_VAR_X TOK_VAR_X EOS
```

Body fields:

```text
version   = 1
type      = 0 in the samples
len       = displayed element count
arraylen  = number of 16-bit expression words, excluding trailing flags
size      = byte length of data, including trailing flags
data      = expression list payload plus one-byte element flags
```

Empty lists (`L5.8xl2` and `L6.8xl2`) stop after `len = 0` and omit
`arraylen`, `size`, and `data`.

## Non-empty data layout

For a non-empty list, `data` has two regions:

```text
00E5          list start marker
...           scalar expression payloads
00D9          list end marker
flags[len]    one byte per displayed element
```

All words in the expression region are little-endian 16-bit values. The
list elements are stored in reverse display order: the first element
after `00E5` is the last element shown on the calculator.

The trailing flags are not counted by `arraylen`, but they are counted by
`size`. The flags are in display order, not reversed payload order.
Observed flag values:

```text
00  normal real or complex value
06  exact fraction
```

## Scalar payloads observed inside lists

The scalar format is postfix-tagged. These tags were observed:

```text
001F  positive exact integer
0020  negative exact integer
0021  exact fraction
0022  negative exact fraction
0023  decimal / approximate real
```

Observed exact integer encodings:

```text
0       0000 001F
1       0001 0001 001F
2       0002 0001 001F
3       0003 0001 001F
-4      0004 0001 0020
```

The middle `0001` appears to be a one-limb count for small non-zero
integers.

Observed exact fraction encoding:

```text
num/den is stored as:
  denominator 0001 numerator 0001 0021

2/3:
  0003 0001 0002 0001 0021

4/5:
  0005 0001 0004 0001 0021

-2/5:
  0005 0001 0002 0001 0022
```

Decimals are six 16-bit words ending in `0023`. As in `.8xn2`, they use
8 packed-BCD significand bytes, then sign byte, then signed base-10
exponent byte. Examples:

```text
4.5:
  bytes 00 00 00 00 00 00 00 45 01 00 23 00
  words 0000 0000 0000 4500 0001 0023

675.89:
  bytes 00 00 00 00 00 90 58 67 01 02 23 00
  words 0000 0000 9000 6758 0201 0023
```

Complex numbers reuse the `.8xn2` complex suffixes. `L4.8xl2` shows the
following payloads:

```text
-3i:
  0003 0001 001F 0026 008F 007A

-4+2i:
  0004 0001 0020 0002 0001 001F 0026 008F 008B
```

The suffixes here match the top-level number samples:

```text
0026 008F 007A  negative pure imaginary suffix
0026 008F 008B  rectangular real + imag*i suffix
```

## Sample breakdown

`L1.8xl2` is displayed as `{1,2,3}`:

```text
len      = 3
arraylen = 11
size     = 25
data     = 00E5 3 2 1 00D9, then flags 00 00 00
```

Expression words:

```text
00E5
0003 0001 001F
0002 0001 001F
0001 0001 001F
00D9
```

`L2.8xl2` is displayed as `{4.5,675.89}` and stores the two decimal
payloads in reverse order, followed by flags `00 00`.

`L3.8xl2` is displayed as `{2/3,4/5}` and stores `4/5` before `2/3`,
followed by flags `06 06`.

`L4.8xl2` is displayed as `{-4+2i,-3i}` and stores `-3i` before
`-4+2i`, followed by flags `00 00`.

Controlled one-element samples from `more`:

```text
L1 = {0}:
  arraylen = 4
  data     = 00E5 0000 001F 00D9, flags 00

L2 = {1}:
  arraylen = 5
  data     = 00E5 0001 0001 001F 00D9, flags 00

L3 = {-1}:
  arraylen = 5
  data     = 00E5 0001 0001 0020 00D9, flags 00

L4 = {4.5}:
  arraylen = 8
  data     = 00E5 <decimal 4.5> 00D9, flags 00

L5 = {2i}:
  arraylen = 7
  data     = 00E5 0002 0001 001F 0026 008F 00D9, flags 00

L6 = {1/3}:
  arraylen = 7
  data     = 00E5 0003 0001 0001 0001 0021 00D9, flags 06
```

Custom named samples:

```text
(L)A = {1,2}:
  stored payload order is 2, then 1
  flags 00 00

(L)B = {1/3,4.5}:
  stored payload order is 4.5, then 1/3
  flags 06 00

(L)ABC = {4+2i,-3i}:
  stored payload order is -3i, then 4+2i
  flags 00 00
```

`(L)XXX` was created as a formula list from `2L1+3L6`, but the exported
file contains an ordinary cached list value only:

```text
(L)XXX:
  name     = E836 E817 E817 E817 0000
  len      = 1
  arraylen = 5
  data     = 00E5 0001 0001 001F 00D9, flags 00
```

No separate formula key or token stream was present in the exported sample.

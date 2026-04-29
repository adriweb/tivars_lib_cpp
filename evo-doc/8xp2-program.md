# `.8xp2` TI-BASIC programs

Sample files: `SEUIL.8xp2`, `MIRETEXT.8xp2`, `PREC.8xp2`, plus
controlled samples `EMPTY.8xp2` and `ONELINE.8xp2`.

Metadata:

```text
metaData.type    = 2
metaData.version = 1
metaData.flags   = 0 in the samples
metaData.name    = program name token words, little-endian, terminated by 0000
```

Body fields:

```text
version   = 1
arraylen  = number of 16-bit token words in data
size      = byte length of data; equals arraylen * 2
data      = byte string of little-endian 16-bit TI-BASIC tokens
```

The final token word is `0000` (`TOK_EOS`). Program line starts are not
stored as colon tokens. New lines are stored explicitly as:

```text
E41C  TOK_NEW_LINE
```

A real same-line colon is stored as:

```text
E418  TOK_COLON
```

## Minimal programs

`EMPTY.8xp2` has program name `EMPTY`:

```text
name words = E804 E80C E80F E813 E818 0000
checksum   = FB 9B
arraylen   = 1
size       = 2
data       = 0000
```

So an empty program still has a one-token data stream containing only
`TOK_EOS`.

`ONELINE.8xp2` has program name `ONELINE` and source:

```text
Disp 1
```

Its token stream is:

```text
E4D3 TOK_DISP
E402 TOK_1
0000 TOK_EOS
```

There is no `TOK_NEW_LINE` after the only line.

## `SEUIL.8xp2` layout

```text
file length     = 213 bytes
CBOR body       = 211 bytes
checksum        = 48 c2
data offset     = 0x56
data size       = 0x7c bytes
arraylen        = 0x3e tokens
```

Name bytes:

```text
12 e8 04 e8 14 e8 08 e8 0b e8 00 00
```

Name tokens:

```text
E812 E804 E814 E808 E80B 0000
S    E    U    I    L    EOS
```

Program token stream:

```text
Prompt D
  E4D2 TOK_PROMPT
  E803 TOK_VAR_D
  E41C TOK_NEW_LINE

0->N
  E401 TOK_0
  E41D TOK_STORE
  E80D TOK_VAR_N
  E41C TOK_NEW_LINE

2.->U
  E403 TOK_2
  E40B TOK_DECPT
  E41D TOK_STORE
  E814 TOK_VAR_U
  E41C TOK_NEW_LINE

D^2->D
  E803 TOK_VAR_D
  E42C TOK_POWER
  E403 TOK_2
  E41D TOK_STORE
  E803 TOK_VAR_D
  E41C TOK_NEW_LINE

While (U-1)^2>=D
  E4C3 TOK_WHILE
  E410 TOK_LPAREN
  E814 TOK_VAR_U
  E429 TOK_SUB
  E402 TOK_1
  E411 TOK_RPAREN
  E42C TOK_POWER
  E403 TOK_2
  E483 TOK_GE
  E803 TOK_VAR_D
  E41C TOK_NEW_LINE

1+1/((1-U)*(N+1))->U
  E402 TOK_1
  E428 TOK_ADD
  E402 TOK_1
  E42B TOK_DIV
  E410 TOK_LPAREN
  E410 TOK_LPAREN
  E402 TOK_1
  E429 TOK_SUB
  E814 TOK_VAR_U
  E411 TOK_RPAREN
  E42A TOK_MUL
  E410 TOK_LPAREN
  E80D TOK_VAR_N
  E428 TOK_ADD
  E402 TOK_1
  E411 TOK_RPAREN
  E411 TOK_RPAREN
  E41D TOK_STORE
  E814 TOK_VAR_U
  E41C TOK_NEW_LINE

N+1->N
  E80D TOK_VAR_N
  E428 TOK_ADD
  E402 TOK_1
  E41D TOK_STORE
  E80D TOK_VAR_N
  E41C TOK_NEW_LINE

End
  E4C6 TOK_END
  E41C TOK_NEW_LINE

{N,U
  E414 TOK_LBRACE
  E80D TOK_VAR_N
  E417 TOK_COMMA
  E814 TOK_VAR_U
  0000 TOK_EOS
```

## Other observed programs

```text
MIRETEXT.8xp2:
  arraylen = 49
  size     = 98
  checksum = BE ED

PREC.8xp2:
  arraylen = 80
  size     = 160
  checksum = EE FC
```

`PREC.8xp2` contains `TOK_COLON` (`E418`) for multiple statements on one source line.

## Additional token samples

The `more/seq` samples confirm that program data remains a plain
little-endian 16-bit token stream even for sequence and graph-related
commands.

`prgm_only_seq_command.8xp2` has program name `A` and contains only:

```text
E44D TOK_SERIES
0000 TOK_EOS
```

`missing2.8xp2` has program name `B`:

```text
E6C6 TOK_RECV_MBL
E41C TOK_NEW_LINE
E6C7 TOK_SEND_MBL
E41C TOK_NEW_LINE
E593 TOK_MGT
E41C TOK_NEW_LINE
E4F9 TOK_DEL_LAST
0000 TOK_EOS
```

`missing.8xp2` has program name `A` and includes one token per line for
several sequence-window variables and commands:

```text
E9A3 TOK_VAR_UTHETA_MIN
E9A4 TOK_VAR_UTHETA_MAX
E9A6 TOK_VAR_UT_MIN
E9A7 TOK_VAR_UT_MAX
E9AC TOK_VAR_UN_MAX
E9AA TOK_VAR_UN_MIN
E9A8 TOK_VAR_UT_STEP
E9A5 TOK_VAR_UTHETA_STEP
E9B0 TOK_VAR_UPLOT_STEP
E99F TOK_VAR_UXRES
E59C TOK_AUTO_FILL_ON
E59D TOK_AUTO_FILL_OFF
E59E TOK_AUTO_CALC_ON
E59F TOK_AUTO_CALC_OFF
E600 TOK_DRAWLINE
E601 TOK_DRAWDOT
E60A TOK_WEBOFF
E41A TOK_APOST
E499 TOK_DT
0000 TOK_EOS
```

Every entry except the last is followed by `E41C TOK_NEW_LINE` in the
file.

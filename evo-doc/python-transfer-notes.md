# Python program transfer notes

Connect Evo treats `.py` files as metadata type `15`, displayed as
`Python Program`. Unlike ordinary Evo CBOR variable files, a selected
`.py` source file is directly converted at transfer time (both ways).

A real type 15 file, which we could invent extension "8xpy2" for, exists though.
For instance:

```text
BF                                      # map(*)
   68                                   # text(8)
      6D65746144617461                  # "metaData"
   BF                                   # map(*)
      64                                # text(4)
         74797065                       # "type"
      0F                                # unsigned(15)
      67                                # text(7)
         76657273696F6E                 # "version"
      01                                # unsigned(1)
      65                                # text(5)
         666C616773                     # "flags"
      00                                # unsigned(0)
      64                                # text(4)
         6E616D65                       # "name"
      4A                                # bytes(10)
         0FE818E813E807E80000           # "\u000F\xE8\u0018\xE8\u0013\xE8\a\xE8\u0000\u0000"
      FF                                # primitive(*)
   67                                   # text(7)
      76657273696F6E                    # "version"
   01                                   # unsigned(1)
   64                                   # text(4)
      73697A65                          # "size"
   18 60                                # unsigned(96)
   64                                   # text(4)
      64617461                          # "data"
   58 60                                # bytes(96)
   ...python script bytes here...
   FF                                   # primitive(*)
```

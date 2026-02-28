## Add `style/type-alias-format` and `style/array-literal-format` lint rules

pony-lint now checks formatting of multiline type aliases and array literals per the Pony style guide.

The `style/type-alias-format` rule checks multiline union and intersection type aliases for correct parenthesis placement and spacing:

```pony
// Flagged: hanging indent, missing spaces
type Signed is (I8
  |I16
  |I32)

// Clean: paren on its own line, spaces after ( and | and before )
type Signed is
  ( I8
  | I16
  | I32 )
```

The `style/array-literal-format` rule checks multiline array literals for correct bracket placement and spacing:

```pony
// Flagged: hanging indent
let ns = [as USize:
  1
  2
]

// Clean: bracket on its own line with space after [
let ns =
  [ as USize:
    1
    2
  ]
```

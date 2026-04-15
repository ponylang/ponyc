class Renameable
  """
  Demonstrates same-file and cross-file rename.

  Place the cursor on `_value` (field declaration on the `var` line, in
  `create`, or in `get_value`) and invoke Rename Symbol. Expect 3 edits
  in this file: the field declaration and both uses.

  Place the cursor on `get_value` (the `fun` line) and invoke Rename Symbol.
  Expect 2 edits across both files: the declaration here and the call in
  rename_user.pony.

  Place the cursor on `Renameable` (the class name) and invoke Rename Symbol.
  Expect 2 edits across both files: the declaration here and the type
  annotation in rename_user.pony.
  """
  var _value: U32 = 0

  new create(v: U32) =>
    _value = v

  fun get_value(): U32 =>
    _value

  fun compute(x: U32): U32 =>
    let result: U32 = x + _value
    result

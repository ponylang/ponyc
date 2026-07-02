class ReferencedClass
  """
  Demonstrates same-file and cross-file references.

  Place the cursor on `_count` (on the `var` line, in `create`, or anywhere
  in `increment`) and invoke Find All References. Expect 5 locations: the
  field declaration and the four uses in `create` and `increment`.

  Place the cursor on `increment` (on the `fun ref` line) and invoke Find All
  References. Expect 2 locations: the declaration here and the call in
  references_user.pony.

  Place the cursor on `ReferencedClass` (the class name on line 1) and invoke
  Find All References. Expect locations in both files: the declaration here
  and the parameter type annotation in references_user.pony.
  """
  var _count: U32 = 0

  new create() =>
    _count = 0

  fun ref increment(): U32 =>
    _count = _count + 1
    _count

  fun maybe(): (U32 | None) =>
    None

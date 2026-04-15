class Generics[A: Any val]
  """
  Demonstrates rename of type parameters at the class and function level.

  Place the cursor on `A` (the class-level type parameter) anywhere it
  appears and invoke Rename Symbol. Expect 4 edits: the declaration in
  `[A: Any val]`, the field type, the constructor parameter type, and the
  return type of `get`.

  Place the cursor on `B` (the function-level type parameter in `transform`)
  anywhere it appears and invoke Rename Symbol. Expect 3 edits: the
  declaration in `[B: Any val]`, the parameter type, and the return type.
  """
  var _item: A

  new create(item: A) =>
    _item = item

  fun get(): A =>
    _item

  fun transform[B: Any val](item: B): B =>
    item

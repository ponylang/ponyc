class RenameUser
  """
  Uses Renameable to demonstrate cross-file rename.

  Place the cursor on `Renameable` (the type annotation) and invoke Rename
  Symbol. Expect 2 edits across both files: the declaration in
  renameable.pony and the type annotation here.

  Place the cursor on `get_value` (the call) and invoke Rename Symbol.
  Expect 2 edits across both files: the declaration in renameable.pony
  and the call here.
  """
  fun use_it(r: Renameable): U32 =>
    r.get_value()

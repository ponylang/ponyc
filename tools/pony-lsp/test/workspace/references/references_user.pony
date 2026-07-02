class ReferencesUser
  """
  Uses ReferencedClass to demonstrate cross-file reference discovery.

  Place the cursor on `increment` (on the call line) and invoke Find All
  References. Expect 2 locations: the call here and the declaration in
  referenced_class.pony.

  Place the cursor on `ReferencedClass` (the parameter type annotation) and
  invoke Find All References. Expect locations in both files.
  """
  fun use_it(obj: ReferencedClass): U32 =>
    obj.increment()

trait _Trait
  """
  A trait used to test goto definition via a trait-typed receiver.

  When calling `trait_method` on a `_DefTrait`-typed variable, goto definition
  resolves to this trait declaration — not to any concrete implementation.
  """
  fun trait_method(): U32

class _DefImplA is _Trait
  """
  First concrete implementation of _DefTrait.
  """
  fun trait_method(): U32 => 1

class _DefImplB is _Trait
  """
  Second concrete implementation of _DefTrait.
  """
  fun trait_method(): U32 => 2

class _DefLeft
  """
  Left member of a union type used to test multiple-definition resolution.

  When calling `shared` on a `(_DefLeft | _DefRight)`-typed variable, goto
  definition resolves to both _DefLeft.shared AND _DefRight.shared, producing
  two results. Editors typically show a picker in this case.
  """
  fun shared(): U32 => 1

class _DefRight
  """
  Right member of the union — see _DefLeft for scenario description.
  """
  fun shared(): U32 => 2

primitive _DefDemo
  """
  Demonstrates goto definition via trait and union types.

  To manually test in your editor:
  1. Open the lsp/test/workspace directory as a project.
  2. Open this file while the Pony language server is active.
  3. Place your cursor on `trait_method` in `use_via_trait` — one result,
     the trait declaration above.
  4. Place your cursor on `shared` in `use_via_union` — two results,
     one for each member of the union.
  """
  fun use_via_trait(t: _Trait): U32 =>
    t.trait_method()

  fun use_via_union(e: (_DefLeft | _DefRight)): U32 =>
    e.shared()

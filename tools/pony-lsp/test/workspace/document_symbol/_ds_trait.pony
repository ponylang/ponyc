// Fixture for `document_symbol/integration/cross_file_trait` — trait half.
//
// The trait provides a default method body. ponyc merges that default
// into classes that declare `is _DsTrait` without overriding it, so the
// implementing class's AST contains descendant tokens whose
// `source_file()` points *here*, not at the impl file. `ASTSourceSpan`
// filters those out so the impl class's documentSymbol `range` stays
// within its own file.
//
// The padding lines below push the trait method body past line 20. If
// the source-file filter in `ASTSourceSpan` regresses, the impl class's
// `range.end.line` in `_ds_impl.pony` would jump to this file's line
// numbers — detectable as an end.line past the impl file's actual end.
//
// Pairs with `_ds_impl.pony`.

trait _DsTrait
  """
  Trait with a default method so ponyc merges its body into
  non-overriding implementers.
  """

  fun ds_default_method(): U32 =>
    let a: U32 = 0
    let b: U32 = 0
    let c: U32 = 0
    a + b + c + 1

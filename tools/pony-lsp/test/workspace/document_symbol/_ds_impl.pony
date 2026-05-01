// Fixture for `document_symbol/integration/cross_file_trait` — impl half.
//
// `_DsImpl` inherits `ds_default_method` from `_DsTrait` (defined in
// `_ds_trait.pony`) without overriding it. After ponyc sugar, the default
// method's body is merged into `_DsImpl`'s AST, but its tokens carry the
// trait file's `source_file()`. `ASTSourceSpan` filters those out when
// building the class's documentSymbol `range`.
//
// The test asserts that `_DsImpl.range.end.line` stays within this
// file — a regression in the source-file filter would inflate the end
// line to match the trait file, well past this file's length.

class _DsImpl is _DsTrait

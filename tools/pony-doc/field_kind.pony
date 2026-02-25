use ast = "pony_compiler"

// The kind of a Pony field declaration: var, let, or embed.
type FieldKind is
  ( FieldVar
  | FieldLet
  | FieldEmbed )

primitive FieldVar
  """Represents a Pony `var` field."""
  fun string(): String => "var"

primitive FieldLet
  """Represents a Pony `let` field."""
  fun string(): String => "let"

primitive FieldEmbed
  """Represents a Pony `embed` field."""
  fun string(): String => "embed"

primitive FieldKindBuilder
  """Maps a pony_compiler TokenId to the corresponding FieldKind."""
  fun apply(id: ast.TokenId): FieldKind ? =>
    match id
    | ast.TokenIds.tk_fvar() => FieldVar
    | ast.TokenIds.tk_flet() => FieldLet
    | ast.TokenIds.tk_embed() => FieldEmbed
    else error
    end

use ast = "pony_compiler"

// The kind of a Pony method declaration: constructor, function, or behaviour.
type MethodKind is
  ( MethodConstructor
  | MethodFunction
  | MethodBehaviour )

primitive MethodConstructor
  """Represents a Pony `new` constructor."""
  fun string(): String => "new"

primitive MethodFunction
  """Represents a Pony `fun` method."""
  fun string(): String => "fun"

primitive MethodBehaviour
  """Represents a Pony `be` behaviour."""
  fun string(): String => "be"

primitive MethodKindBuilder
  """Maps a pony_compiler TokenId to the corresponding MethodKind."""
  fun apply(id: ast.TokenId): MethodKind ? =>
    match id
    | ast.TokenIds.tk_new() => MethodConstructor
    | ast.TokenIds.tk_fun() => MethodFunction
    | ast.TokenIds.tk_be() => MethodBehaviour
    else error
    end

use ast = "pony_compiler"

// The kind of a Pony type declaration. Each variant provides `string()`
// (for display) and `keyword()` (the Pony keyword that introduces it).
type EntityKind is
  ( EntityClass
  | EntityActor
  | EntityTrait
  | EntityInterface
  | EntityPrimitive
  | EntityStruct
  | EntityTypeAlias )

primitive EntityClass
  """Represents a Pony `class` declaration."""
  fun string(): String => "class"
  fun keyword(): String => "class"

primitive EntityActor
  """Represents a Pony `actor` declaration."""
  fun string(): String => "actor"
  fun keyword(): String => "actor"

primitive EntityTrait
  """Represents a Pony `trait` declaration."""
  fun string(): String => "trait"
  fun keyword(): String => "trait"

primitive EntityInterface
  """Represents a Pony `interface` declaration."""
  fun string(): String => "interface"
  fun keyword(): String => "interface"

primitive EntityPrimitive
  """Represents a Pony `primitive` declaration."""
  fun string(): String => "primitive"
  fun keyword(): String => "primitive"

primitive EntityStruct
  """Represents a Pony `struct` declaration."""
  fun string(): String => "struct"
  fun keyword(): String => "struct"

primitive EntityTypeAlias
  """Represents a Pony `type` alias declaration."""
  fun string(): String => "type"
  fun keyword(): String => "type"

primitive EntityKindBuilder
  """Maps a pony_compiler TokenId to the corresponding EntityKind."""
  fun apply(id: ast.TokenId): EntityKind ? =>
    match id
    | ast.TokenIds.tk_class() => EntityClass
    | ast.TokenIds.tk_actor() => EntityActor
    | ast.TokenIds.tk_trait() => EntityTrait
    | ast.TokenIds.tk_interface() => EntityInterface
    | ast.TokenIds.tk_primitive() => EntityPrimitive
    | ast.TokenIds.tk_struct() => EntityStruct
    | ast.TokenIds.tk_type() => EntityTypeAlias
    else error
    end

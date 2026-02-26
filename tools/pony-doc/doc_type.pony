// A Pony type as represented in the documentation IR. Mirrors the type AST
// nodes that docgen.c handles in `doc_type()`.
type DocType is
  ( DocNominal
  | DocTypeParamRef
  | DocUnion
  | DocIntersection
  | DocTuple
  | DocArrow
  | DocThis
  | DocCapability )

class val DocNominal
  """
  A named type reference (e.g., `Array[String] val`).

  The `tqfn` field holds the Type Qualified Full Name used for
  cross-reference links in documentation output.
  """
  let name: String
  let tqfn: String
  let type_args: Array[DocType] val
  let cap: (String | None)
  let ephemeral: (String | None)
  let is_private: Bool
  let is_anonymous: Bool

  new val create(
    name': String,
    tqfn': String,
    type_args': Array[DocType] val,
    cap': (String | None),
    ephemeral': (String | None),
    is_private': Bool,
    is_anonymous': Bool)
  =>
    name = name'
    tqfn = tqfn'
    type_args = type_args'
    cap = cap'
    ephemeral = ephemeral'
    is_private = is_private'
    is_anonymous = is_anonymous'

class val DocTypeParamRef
  """
  A reference to a type parameter (e.g., `A` in `fun foo(x: A)`).
  May have an ephemeral modifier (`^` or `!`).
  """
  let name: String
  let ephemeral: (String | None)

  new val create(name': String, ephemeral': (String | None)) =>
    name = name'
    ephemeral = ephemeral'

class val DocUnion
  """A union type (e.g., `(String | None)`)."""
  let types: Array[DocType] val

  new val create(types': Array[DocType] val) =>
    types = types'

class val DocIntersection
  """An intersection type (e.g., `(Stringable & Hashable)`)."""
  let types: Array[DocType] val

  new val create(types': Array[DocType] val) =>
    types = types'

class val DocTuple
  """A tuple type (e.g., `(String, U32)`)."""
  let types: Array[DocType] val

  new val create(types': Array[DocType] val) =>
    types = types'

class val DocArrow
  """An arrow (viewpoint adaptation) type (e.g., `this->T`)."""
  let left: DocType
  let right: DocType

  new val create(left': DocType, right': DocType) =>
    left = left'
    right = right'

class val DocThis
  """The `this` type literal."""
  new val create() => None

class val DocCapability
  """
  A bare capability token (iso, trn, ref, val, box, tag).

  Also holds generic capabilities (#read, #send, #share) when used
  for entity or method cap fields via `doc_get_cap()`. Does not
  include #alias or #any.
  """
  let name: String

  new val create(name': String) =>
    name = name'

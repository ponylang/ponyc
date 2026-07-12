use "collections"

type _Alias is U8

class _TypeAliasFields
  """
  Test fixture for goto definition on a type alias usage.

  In `Map[String, U32]`, goto definition on `Map` resolves to the `Map` type
  alias declaration, and on the type arguments `String` and `U32` to their own
  declarations. `_Alias` is a simple type alias defined in this file; goto
  definition on it resolves to that declaration.
  """
  let _data: Map[String, U32] val
  let _name: String
  let _alias_field: _Alias

  new create() =>
    _data = recover val Map[String, U32] end
    _name = ""
    _alias_field = 0

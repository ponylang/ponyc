use "collections"

type _Alias is U8

class _TypeAliasFields
  """
  Test fixture for goto definition on type arguments inside type aliases.

  `Map` is a type alias for `HashMap[K, V, HashEq[K]]`. After the compiler's
  name resolution pass, the alias is expanded and the original `Map` reference
  is replaced. The type arguments (`String`, `U32`) are substituted into the
  expanded tree but must remain reachable by the position index for goto
  definition to work.

  `_Alias` is a simple type alias defined in this file, testing whether goto
  definition works on a local type alias usage.
  """
  let _data: Map[String, U32] val
  let _name: String
  let _alias_field: _Alias

  new create() =>
    _data = recover val Map[String, U32] end
    _name = ""
    _alias_field = 0

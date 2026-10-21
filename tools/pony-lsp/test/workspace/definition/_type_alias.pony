type _TaAlias is U8

primitive _TaKey

primitive _TaValue

class _TaBox[K, V]
  new create() => None

type _TaMap[K, V] is _TaBox[K, V]

class _TypeAliasFields
  """
  Test fixture for goto definition on type alias and type argument usages.

  `_TaMap` is a generic type alias; goto definition on it resolves to the
  alias declaration. Goto definition on `_TaKey` and `_TaValue` in the type
  arguments resolves to their own declarations. `_TaAlias` is a simple alias
  defined in this file.
  """
  let _data: _TaMap[_TaKey, _TaValue]
  let _alias_field: _TaAlias

  new create() =>
    _data = _TaBox[_TaKey, _TaValue]
    _alias_field = 0

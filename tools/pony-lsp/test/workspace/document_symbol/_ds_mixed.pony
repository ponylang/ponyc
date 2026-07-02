// Fixture for `document_symbol/integration/mixed_children`.
//
// `_DsMixed` has no explicit `new`, so ponyc's sugar pass synthesizes a
// `create` constructor at the class keyword's source position. It has one
// explicitly written `fun`. The position filter in `find_members` must
// suppress the synthesized constructor (at the class keyword's position)
// without suppressing the explicit method (at a later position).

class _DsMixed
  fun ds_mixed_method(): U32 => 0

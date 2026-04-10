actor Main
  new create(env: Env) =>
    None

class _FieldInitCases
  var with_init: U32 = 0
  var without_init: U32
  var eq_in_comment: U32 // default=0
  let flet_with_init: Bool = false
  let flet_without_init: Bool

  new create() =>
    without_init = 0
    eq_in_comment = 0
    flet_without_init = false

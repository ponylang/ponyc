class _TConstructed
  new create() => None

class _TCallConstructorCaller
  fun f_new_explicit(): None =>
    _TConstructed.create()
    None

class _TConstructorWithBody
  new create(a: _TCallA) =>
    a.f_a()

class _TPolyA
  fun poly_method(): None => None

class _TPolyB
  fun poly_method(): None => None

class _TPolyUser
  fun call_a(a: _TPolyA): None =>
    a.poly_method()

  fun call_b(b: _TPolyB): None =>
    b.poly_method()

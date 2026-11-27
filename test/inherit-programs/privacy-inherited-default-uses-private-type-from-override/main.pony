// lib's T.bar calls foo().y(). C in main overrides foo to return main's
// private _MainPriv. C's bar must call y on it and print main._MainPriv.
use l = "lib"
primitive _MainPriv
  fun y(): String => "main._MainPriv"
class C is l.T
  new create() => None
  fun foo(): _MainPriv => _MainPriv
actor Main
  new create(env: Env) => env.out.print(C.bar())

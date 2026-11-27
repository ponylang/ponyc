// T writes a guard that shadows the one before it. That is T's own error,
// so the test expects it once, with no inherited-by note, though two
// classes inherit f.
trait T[A: Any val]
  fun f(): U8 => 0
  fun f(): U8 iftype A <: U8 val => 1
  fun f(): U8 iftype A <: U8 val => 2
class C1 is T[U8]
class C2 is T[U16]
actor Main
  new create(env: Env) => None

// lib's T[X].a() and b() each return a T[X] literal. Bar in main is
// l.T[_P] with main's private _P, and Bar.a(_P).b(_P).s() must print
// "main._P": a private type passed as a type argument may be used in the
// literals of another package's trait (#6711).
use l = "lib"
primitive _P
  fun string(): String iso^ => "main._P".clone()
class Bar is l.T[_P]
  new create() => None
  fun x(): _P => _P
actor Main
  new create(env: Env) =>
    env.out.print(Bar.a(_P).b(_P).s())

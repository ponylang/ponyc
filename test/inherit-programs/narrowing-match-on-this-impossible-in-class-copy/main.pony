// T.f matches this against class D. In C's inherited copy the pattern can
// never match, but it can in D's copy, so the program must compile and print
// "other" instead of reporting a pattern that can never match.
actor Main
  new create(env: Env) =>
    env.out.print(C.f())

trait T
  fun f(): String =>
    match this
    | let d: D box => "D"
    else "other" end

class D is T
class C is T

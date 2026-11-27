// Calls on (A | B) and (B | A) receivers use the default of the union's
// first member, while the object's own f runs. Expects 11 2.
class A
  fun f(x: U64 = 1): U64 => x
class B
  fun f(x: U64 = 2): U64 => x + 10
actor Main
  new create(env: Env) =>
    let u: (A | B) = B
    let v: (B | A) = A
    env.out.print(u.f().string() + " " + v.f().string())

// With (U8 | None) for A, the union (A | N | I8) repeats None, once from A and
// once through the alias N. The compiler reaches the type both with N and with
// N unfolded; both must be the same type. The program must compile and print 3.
type N is None

class G[A: Any val]
  fun pick(x: (A | N | I8)): (A | N | I8) =>
    match x
    | let r: (A | N | I8) => return r
    end
    x

actor Main
  new create(env: Env) =>
    match G[(U8 | None)].pick(U8(3))
    | let u: U8 => env.out.print(u.string())
    | None => env.out.print("none")
    end

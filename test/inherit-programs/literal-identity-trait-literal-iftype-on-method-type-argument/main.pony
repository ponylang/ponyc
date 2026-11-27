// h's object literal reads h's type argument B in an iftype. A literal made
// with B = String, then h[U8] called on that literal, must give a literal
// that sees B = U8: the program prints o, u8, u8 (#6736).
trait U
  fun get(): String => "base"
  fun h[B: Any val](b: B): U val =>
    object val is U
      fun get(): String => iftype B <: U8 then "u8" else "o" end
    end

primitive P is U

actor Main
  new create(env: Env) =>
    let t2 = P.h[String]("s")
    env.out.print(t2.get())
    env.out.print(t2.h[U8](1).get())
    env.out.print(P.h[U8](1).get())

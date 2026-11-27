// m's literal captures m's parameter p. The literal from C and the literal
// made from that literal must each read their own p, printing 1 then 3.
trait T[A: Any val]
  fun label(): String => "leaf"
  fun m(p: U64): T[Array[A] val] val =>
    object val is T[Array[A] val]
      fun label(): String => p.string()
    end

class val C is T[U8]
  new val create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.m(1).label() + C.m(2).m(3).label())

// T.m() returns a T2 literal that captures v = get(); T2 overrides m() to
// return a named class. C overrides get() to return U64, so C.m().label()
// prints "u64", and C.m().m() uses T2's m() and prints "leaf".
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end

trait T[A: Any val]
  fun get(): (U64 | None) => None
  fun label(): String => "leaf"
  fun m(): T[Array[A] val] val =>
    let v = get()
    object val is T2[Array[A] val]
      fun label(): String => Name(v)
    end

trait T2[A: Any val] is T[A]
  fun m(): T[Array[A] val] val => Leaf[Array[A] val]

class val Leaf[A: Any val] is T[A]
  new val create() => None

class val C is T[U8]
  new val create() => None
  fun get(): U64 => 1

actor Main
  new create(env: Env) =>
    env.out.print(C.m().label() + "|" + C.m().m().label())

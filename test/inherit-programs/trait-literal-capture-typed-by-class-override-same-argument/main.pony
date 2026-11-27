// m captures v = get(), and C overrides get to return U64. In C's copy v is
// U64, so Name(v) infers U64 while Name[(U64 | None)](v) does not: the
// program prints other/u64. The literal provides T[A].
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end

trait T[A: Any val]
  fun get(): (U64 | None) => None
  fun label(): String => "leaf"
  fun m(): T[A] val =>
    let v = get()
    object val is T[A]
      fun label(): String => Name[(U64 | None)](v) + "/" + Name(v)
    end

class val C is T[U8]
  new val create() => None
  fun get(): U64 => 1

actor Main
  new create(env: Env) =>
    env.out.print(C.m().label())

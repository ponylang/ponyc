// T.m() captures this as me in a literal whose label() names me.get()'s
// type. In C's copy me is a C, whose get() returns U64, so C.m() prints
// "u64". In the literal's copy me is a T val, whose get() returns
// (U64 | None), so C.m().m() prints "other".
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end

trait T
  fun get(): (U64 | None) => None
  fun label(): String => "leaf"
  fun val m(): T val =>
    let me = this
    object val is T
      fun get(): U64 => 2
      fun label(): String => Name(me.get())
    end

class val C is T
  new val create() => None
  fun get(): U64 => 1

actor Main
  new create(env: Env) =>
    env.out.print(C.m().label() + "|" + C.m().m().label())

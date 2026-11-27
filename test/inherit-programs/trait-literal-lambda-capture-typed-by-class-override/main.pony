// lam's lambda captures q = get(), and C overrides get to return U64. In C's
// copy q is U64, so Name(q) prints u64 after the literal's L.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end

trait T[A: Any val]
  fun get(): (U64 | None) => None
  fun label(): String => "leaf"
  fun m(): T[Array[A] val] val =>
    object val is T[Array[A] val]
      fun label(): String => "L"
    end
  fun lam(p: (U64 | None)): {(): String} val =>
    {()(q = get()): String => Name(q) }

class val C is T[U8]
  new val create() => None
  fun get(): U64 => 1

actor Main
  new create(env: Env) =>
    env.out.print(C.m().label() + C.lam(None)())

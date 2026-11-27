// Mk.mk's object literal of T overrides get to return U64 and inherits label.
// In the inherited label, get() keeps T's (U64 | None) type, so the program
// prints other.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait T
  fun get(): (U64 | None) => None
  fun label(): String => Name(get())
trait Mk
  fun mk(): T val => object val is T fun get(): U64 => 1 end
primitive C is Mk
actor Main
  new create(env: Env) =>
    env.out.print(C.mk().label())

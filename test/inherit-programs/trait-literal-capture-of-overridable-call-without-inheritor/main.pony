// m's literal captures v = get(), typed (U64 | None) in T. Nothing provides
// T; the program must still compile and print x.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end

trait T[A: Any val]
  fun get(): (U64 | None) => None
  fun label(): String => "leaf"
  fun m(): T[Array[A] val] val =>
    let v = get()
    object val is T[Array[A] val]
      fun label(): String => Name[(U64 | None)](v) + "/" + Name(v)
    end

class val C
  new val create() => None
  fun get(): U64 => 1

actor Main
  new create(env: Env) =>
    env.out.print("x")

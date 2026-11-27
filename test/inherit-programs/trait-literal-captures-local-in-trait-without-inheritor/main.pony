// An object literal in a trait method captures a local, and nothing inherits
// the trait. The program must compile; expects x.
trait T
  fun get(): U64 => 1
  fun m(): Any val =>
    let v = get()
    object val
      fun label(): String => v.string()
    end
actor Main
  new create(env: Env) => env.out.print("x")

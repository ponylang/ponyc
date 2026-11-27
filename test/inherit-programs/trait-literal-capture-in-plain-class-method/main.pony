// An object literal in a plain class method captures a local. No trait is
// involved; this is the baseline for the trait version. Expects x.
class C
  fun get(): U64 => 1
  fun m(): Any val =>
    let v = get()
    object val
      fun label(): String => v.string()
    end
actor Main
  new create(env: Env) => C.m(); env.out.print("x")

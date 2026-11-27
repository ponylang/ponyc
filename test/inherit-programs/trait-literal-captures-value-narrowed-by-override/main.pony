// T.m's object literal captures v = get(). In T, v is a (U8 | String); in C1,
// whose get returns String, v is a String. v.string() is valid in both, so
// the program must compile and run, printing nothing.
trait T
  fun get(): (U8 | String) => U8(1)
  fun m(): Any val =>
    let v = get()
    object val fun k(): U8 => v.string().size().u8() end
class C1 is T
  fun get(): String => "x"
actor Main
  new create(env: Env) => C1.m()

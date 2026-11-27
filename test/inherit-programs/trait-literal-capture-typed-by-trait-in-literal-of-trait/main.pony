// T.m() captures a = [get()] in a T literal that overrides get() to return
// U64. Calling m() on C and then on the literals must print "1 1 1": inside
// each inherited m() the captured array keeps the type the trait gives it,
// so it matches the literal's field.
trait T
  fun get(): (U64 | None) => None
  fun m(): T ref =>
    let a = [get()]
    object ref is T
      fun get(): U64 => 1
      fun size(): USize => a.size()
    end
  fun size(): USize => 0
class C is T
actor Main
  new create(env: Env) =>
    env.out.print(C.m().size().string() + " " + C.m().m().size().string() + " " + C.m().m().m().size().string())

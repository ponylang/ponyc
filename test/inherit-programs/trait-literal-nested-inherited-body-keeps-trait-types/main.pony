// T0's literal overrides get() to return U64 and inherits T1.fa, which
// assigns None to v = get(). The copy keeps T1's (U64 | None) for v, so it
// compiles; expects t2.
trait T0
  fun fa(): T1 ref =>
    object ref is T1
      fun get(): U64 => 1
    end
trait T1
  fun get(): (U64 | None) => None
  fun fa(): T2 ref =>
    var v = get()
    v = None
    object ref is T2 end
trait T2
  fun name(): String => "t2"
class C is T0
actor Main
  new create(env: Env) => env.out.print(C.fa().fa().name())

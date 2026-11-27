// C.fa() returns T0's literal, which overrides get() to return U64, and that
// literal inherits T1.fa. T1.fa assigns array b to a and captures a. Both keep
// T1's (U64 | None) element type, so this compiles; expects 1.
trait T0
  fun fa(): T1 ref =>
    object ref is T1
      fun get(): U64 => 1
    end
trait T1
  fun get(): (U64 | None) => None
  fun fa(): T2 ref =>
    var a = [get()]
    let b = [get()]
    a = b
    object ref is T2
      fun name(): String => a.size().string()
    end
trait T2
  fun name(): String => "t2"
class C is T0
actor Main
  new create(env: Env) => env.out.print(C.fa().fa().name())

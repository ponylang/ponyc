// T2 provides T0 and T1, whose m0() bodies each declare a local, and C0
// provides T2 with its own m0(). The test expects "c": C0's body replaces
// both inherited ones (#6722).
actor Main
  new create(env: Env) =>
    env.out.print(C0.m0())

trait T0
  fun m0(): String =>
    let s = "a"
    s

trait T1
  fun m0(): String =>
    let s = "b"
    s

trait T2 is (T0 & T1)

class C0 is T2
  new create() => None
  fun m0(): String => "c"

// T0 and T1 both give m0 a body, and C0 provides both directly. The test
// expects the one error asking C0 to choose, at C0 (#6722).
trait T0
  fun m0(): String =>
    let s = "a"
    s
trait T1
  fun m0(): String =>
    let s = "b"
    s
class C0 is (T0 & T1)
  new create() => None
actor Main
  new create(env: Env) => None

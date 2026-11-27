// T1 declares f[P: X val, Q: Y val] and T2 gives f[Q: X val, P: Y val] a body
// whose lambda creates its Q, the first parameter. Parameters match by
// position, so in C the lambda must create the first parameter's type, X,
// whatever it is named. The test expects "x".
primitive X
  new create() => None
  fun s(): String => "x"
primitive Y
  new create() => None
  fun s(): String => "y"
trait T1
  fun f[P: X val, Q: Y val](): String
trait T2
  fun f[Q: X val, P: Y val](): String =>
    let h = {(): String => Q.create().s()}
    h()
class C is (T1 & T2)
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.f[X, Y]())

// T1 declares f[X, Y] and T2 gives f[Y, X] a body; the parameters match by
// position, so they are one signature and C takes T2's body. The test expects
// 3 (#6733).
trait T1
  fun f[X: Any val, Y: Any val](x: X, y: Y): U8
trait T2
  fun f[Y: Any val, X: Any val](x: Y, y: X): U8 => 3
class C is (T1 & T2)
actor Main
  new create(env: Env) => env.out.print(C.f[U8, String](1, "a").string())

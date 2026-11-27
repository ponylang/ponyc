// lib1's default has a lambda with a local named u. lib2's T2 provides T1 and
// declares `use u`. The test expects "3 0": the lambda's local u doesn't clash
// with lib2's alias when C inherits the body through T2.
use l2 = "lib2"
class C is l2.T2
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.f() + " " + C.other().string())

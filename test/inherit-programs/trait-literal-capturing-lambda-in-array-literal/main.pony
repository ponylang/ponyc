// T's f puts a lambda that captures x in an array literal; C inherits f. The
// array's handling rebuilds the nodes around the lambda after the lambda's
// class is made for C's copy. The test expects the class to be reached and
// the lambda to return x + 1, printing 8.
trait T
  fun f(x: USize): USize =>
    let a = [{(): USize => x + 1}]
    try a(0)?() else 0 end
class C is T
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(7).string())

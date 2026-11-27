// T's f puts an object literal with a field in an array literal; C inherits
// f. The array's handling rebuilds the nodes around the literal after the
// literal's class is made for C's copy. The test expects the class to be
// reached and apply to return 7.
trait T
  fun f(): USize =>
    let a = [object let n: USize = 7 fun apply(): USize => n end]
    try a(0)?.apply() else 0 end
class C is T
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f().string())

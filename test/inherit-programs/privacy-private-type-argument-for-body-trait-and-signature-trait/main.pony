// l2's X provides lib's T2 and S, both with l2's private _Y. S declares
// bar(a: B) with no body and T2 gives bar(a: A) a body that calls y() on a.
// l2 chose _Y for both traits that declare bar, so C's inherited bar may call
// y() on it. Here X provides T2 before S. The test expects "l2._Y".
use l2 = "l2"
class C is l2.X
  new create() => None
actor Main
  new create(env: Env) => env.out.print(l2.Run(C))

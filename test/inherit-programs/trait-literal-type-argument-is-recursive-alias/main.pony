// C gives T the recursive alias L for A, and T's f makes an object literal
// whose method names A. Checking whether L names another package's private
// type must stop at L's reference to itself, so the test expects the program
// to compile and print 1.
type L is (None | Array[L] val)
trait T[A]
  fun f(): U64 =>
    let o = object val
      fun g(): (A | None) => None
    end
    1
class C is T[L]
actor Main
  new create(env: Env) =>
    env.out.print(C.f().string())

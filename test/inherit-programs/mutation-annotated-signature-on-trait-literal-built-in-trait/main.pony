// T.mk returns an object literal of (Sig & T). Sig declares get with the
// nomutate annotation, and T supplies get's body. The literal must get T's
// body, and the program must compile and print 3.
trait Sig
  fun \nomutate\ get(): U64

trait T
  fun get(): U64 => U64(1) + 2
  fun mk(): T val => object is (Sig & T) end

class C is T
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.mk().get().string())

// Two object literals of _Donor[U64] get the same lambda from get; the first
// also provides _Signature, whose get is \nomutate\. Expects the two lambdas
// to be the same object, the _Donor[U8] lambda to differ, and 5, 5, 6.
actor \nodoc\ Main
  new create(env: Env) =>
    let marked = object is (_Signature & _Donor[U64]) end
    let unmarked = object is _Donor[U64] end
    let other = object is _Donor[U8] end
    let a = marked.get()
    let b = unmarked.get()
    let c = other.get()
    env.out.print((a is b).string())
    env.out.print((a is c).string())
    env.out.print(a(4).string())
    env.out.print(b(4).string())
    env.out.print(c(4).string())

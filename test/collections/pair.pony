class Pair[A, B]
  var a:A
  var b:B

  new{var} create(a':A, b':B) =
    a = a';
    b = b'

  fun{box} first():A->this = a
  fun{box} second():B->this = b

  // FIX: if a:Foo{iso}, we can return Foo{iso} here
  // this notation turns it into Foo{tag}
  // but we can't return A, because if a:Foo{var} and this:{iso} then we have
  // to return Foo{tag}.
  //
  // A{iso}, this{iso} = A{iso}
  //
  // typechecked with {var} receiver, so the result is A{iso}
  // at use-site, it becomes {iso}->{iso}
  // need another type viewpoint operator? ie destructive read?
  //
  // A{var}, this{iso} = A{tag} // this one works
  // A{val}, this{iso} = A{val} // this one works
  // everything works with {var} receiver
  fun{var} set_first(a':A):A->this = a = a'
  fun{var} set_second(b':B):B->this = b = b'

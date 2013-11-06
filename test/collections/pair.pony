class Pair[A, B]
  var a:A
  var b:B

  new create(a':A, b':B) =
    a = a';
    b = b'

  fun first{iso|var|val}():A->this = a
  fun second{iso|var|val}():B->this = b

  // FIX: if a:Foo{iso}, we can return Foo{iso} here
  // this notation turns it into Foo{tag}
  // but we can't return A, because if a:Foo{var} and this:{iso} then we have
  // to return Foo{tag}.
  //
  // A{iso}, this{iso} = A{iso}
  // A{var}, this{iso} = A{tag}
  // A{val}, this{iso} = A{val}
  // A{iso}, this{var} = A{iso}
  fun set_first{iso|var}(a':A):A->this = a = a'
  fun set_second{iso|var}(b':B):B->this = b = b'

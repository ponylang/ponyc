trait T[A: Stringable val]
  fun show(a: A, f: {(A): String} val = {(x: A): String => "lib:" + x.string()}): String =>
    f(a)

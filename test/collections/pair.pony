class Pair[A, B]
  var a:A
  var b:B

  fun default(a':A, b':B) =
    Partial[This](a', b').absorb()

  fun first{iso|var|val}():A->this = a
  fun second{iso|var|val}():B->this = b

  fun set_first{iso|var}(a':A):A->this = a = a'
  fun set_second{iso|var}(b':B):B->this = b = b'

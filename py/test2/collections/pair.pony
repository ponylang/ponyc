class Pair[A, B]
  var a:A
  var b:B

  fun default(a':A, b':B) =
    Partial[This](a', b').absorb()

  fun{iso|var|val} first():A->this = a
  fun{iso|var|val} second():B->this = b

  fun{iso|var} set_first(a':A):A->this = a = a'
  fun{iso|var} set_second(b':B):B->this = b = b'

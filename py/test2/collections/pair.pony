class Pair[A, B]
  var a:A
  var b:B

  fun default(a:A, b:B) = this

  fun{iso|var|val} first():A->this = a
  fun{iso|var|val} second():B->this = b

  fun{iso|var} set_first(a_:A):A->this = a = a_
  fun{iso|var} set_second(b_:B):B->this = b = b_

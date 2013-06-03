class Pair[A, B]
{
  var a:A
  var b:B

  new default(a:A, b:B) = this

  read first()->(A this) = a
  read second()->(B this) = b

  write set_first(a_:A)->(A this) = swap(a, a_)
  write set_second(b_:B)->(B this) = swap(b, b_)
}

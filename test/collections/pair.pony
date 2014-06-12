class Pair[A, B]
  var a: A
  var b: B

  new create(a': A, b': B) =>
    a = a'
    b = b'

  fun box first(): A this => a
  fun box second(): B this => b

  fun ref set_first(a': A): A this^ => a = a'
  fun ref set_second(b': B): B this^ => b = b'

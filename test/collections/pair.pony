class Pair[A, B]
  var a: A
  var b: B

  new create(a': A, b': B) ->
    a := a'
    b := b'

  fun:box first(): this.A -> a
  fun:box second(): this.B -> b

  fun:var set_first(a': A): this.A^ -> a := a'
  fun:var set_second(b': B): this.B^ -> b := b'

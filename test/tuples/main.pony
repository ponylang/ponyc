actor Main
  var _tuple: (U32, U32) = (3, 5)

  new create(env: Env) =>
    var test: (U32, U32) = (3, 5)
    var (a, b): (U32, U32) = (1, 2)

    if _tuple == test then
      @printf[I32]("eq\n".cstring())
    else
      @printf[I32]("ne\n".cstring())
    end

    @printf[I32]("%d, %d\n".cstring(), _tuple.0, _tuple.1)

    _tuple = var tuple = (a, b) = (b, a)

    @printf[I32]("%d, %d\n".cstring(), a, b)
    @printf[I32]("%d, %d\n".cstring(), tuple.0, tuple.1)
    @printf[I32]("%d, %d\n".cstring(), _tuple.0, _tuple.1)

    var array = Array[String].append("Hello").append("World").append("!")

    for (index, value) in array.pairs() do
      @printf[I32]("%d: %s\n".cstring(), index.i32(), value.cstring())
    end

actor Main
  var _tuple: (U32, U32) = (3, 5)

  new create(env: Env) =>
    try
      var a = Array[(Array[F32], Array[F32])]
      a.push((Array[F32], Array[F32]))
      a(0)._1.push(99)

      var x = (a(0)._1)(0)
      env.out.print(x.string())
    else
      env.out.print("error")
    end

    var test: (U32, U32) = (3, 5)
    (var a: U32, var b: U32) = (1, 2)

    if _tuple is test then
      env.out.print("is")
    else
      env.out.print("isnt")
    end

    env.out.print(_tuple._1.string() + ", " + _tuple._2.string())

    var tuple = (a, b) = (b, a)
    _tuple = tuple

    env.out.print(a.string() + ", " + b.string())
    env.out.print(tuple._1.string() + ", " + tuple._2.string())
    env.out.print(_tuple._1.string() + ", " + _tuple._2.string())

    var array = Array[String].push("Hello").push("World").push("!")

    try
      for (index, value) in array.pairs() do
        env.out.print(index.string() + ": " + value)
      end
    end

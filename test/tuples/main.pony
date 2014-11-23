actor Main
  var _tuple: (U32, U32) = (3, 5)

  new create(env: Env) =>
    try
      var a = Array[(Array[F32], Array[F32])]
      a.append((Array[F32], Array[F32]))
      a(0)._1.append(99)

      var x = (a(0)._1)(0)
      env.out.println(x.string())
    else
      env.out.println("error")
    end

    var test: (U32, U32) = (3, 5)
    var (a, b): (U32, U32) = (1, 2)

    if _tuple is test then
      env.out.println("is")
    else
      env.out.println("isnt")
    end

    env.out.println(_tuple._1.string() + ", " + _tuple._2.string())

    var tuple = (a, b) = (b, a)
    _tuple = tuple

    env.out.println(a.string() + ", " + b.string())
    env.out.println(tuple._1.string() + ", " + tuple._2.string())
    env.out.println(_tuple._1.string() + ", " + _tuple._2.string())

    var array = Array[String].append("Hello").append("World").append("!")

    try
      for (index, value) in array.pairs() do
        env.out.println(index.string() + ": " + value)
      end
    end

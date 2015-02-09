actor Main
  let _env: Env

  new create(env: Env) =>
    _env = env

    var a: String = "test".cut(1,2)
    var b: String = "test".cut(0,1)
    var c: String = "aardvark".cut(1,5)
    var d: String = "wombat".cut(3,6)

    var e =
      recover val
        String.append("test").cut_in_place(1,2)
      end

    var f =
      recover val
        String.append("test").cut_in_place(0,1)
      end

    var g =
      recover val
        String.append("aardvark").cut_in_place(1,5)
      end

    var h =
      recover val
        String.append("wombat").cut_in_place(3,6)
      end

    env.out.print(a + " expected: tt")
    env.out.print(b + " expected: st")
    env.out.print(c + " expected: ark")
    env.out.print(d + " expected: empty string")
    env.out.print("===")
    env.out.print(e + " expected: tt")
    env.out.print(f + " expected: st")
    env.out.print(g + " expected: ark")
    env.out.print(h + " expected: wombat")

    try
      var idx1 = "123123123".find("3")
      env.out.print(idx1.string())

      var idx2 = "123123123".find("3", idx1 + 1)
      env.out.print(idx2.string())
    end

    var j = recover val String.append("  \thi!\r\n").trim() end
    env.out.print("[" + j.size().string() + ": " + j + "]")

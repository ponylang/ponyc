actor Main
  new create(env: Env) =>
    var a: String val = "test".cut(1,2)
    var b: String val = "test".cut(0,1)
    var c: String val = "aardvark".cut(1,5)
    var d: String val = "wombat".cut(3,6)

    var e: String val =
      recover
        var s = String.append("test").cut_in_place(1,2)
        consume s
      end

    var f: String val =
      recover
        var s = String.append("test").cut_in_place(0,1)
        consume s
      end

    var g: String val =
      recover
        var s = String.append("aardvark").cut_in_place(1,5)
        consume s
      end

    var h: String val =
      recover
        var s = String.append("wombat").cut_in_place(3,6)
        consume s
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

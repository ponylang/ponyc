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

    env.out.println(a + " expected: tt")
    env.out.println(b + " expected: st")
    env.out.println(c + " expected: ark")
    env.out.println(d + " expected: empty string")
    env.out.println("===")
    env.out.println(e + " expected: tt")
    env.out.println(f + " expected: st")
    env.out.println(g + " expected: ark")
    env.out.println(h + " expected: wombat")

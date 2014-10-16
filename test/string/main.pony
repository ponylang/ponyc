actor Main
  new create(env: Env) =>
    var a: String val = "test".cut(1,2)
    var b: String val = "test".cut(0,1)
    var c: String val = "ardwark".cut(1,5)
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
        var s = String.append("ardwark").cut_in_place(1,5)
        consume s
      end

    var h: String val =
      recover
        var s = String.append("wombat").cut_in_place(3,6)
        consume s
      end

    env.stdout.print(a + " expected: tt")
    env.stdout.print(b + " expected: st")
    env.stdout.print(c + " expected: ak")
    env.stdout.print(d + " expected: empty string")
    env.stdout.print("===")
    env.stdout.print(e + " expected: tt")
    env.stdout.print(f + " expected: st")
    env.stdout.print(g + " expected: ak")
    env.stdout.print(h + " expected: wombat")

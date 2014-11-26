trait Animal

trait Vegetable

class Wombat is Animal

class Aardvark is Animal

class Dormouse is Animal

class Tomato is Vegetable

class Thumbtack

actor Main
  var _env: Env

  new create(env: Env) =>
    _env = env

    test(Wombat)
    test(Aardvark)
    test(Dormouse)
    test(Tomato)
    test(Thumbtack)
    test(None)

    test2(3)
    test2(4)

    test3((Wombat, Tomato))
    test3((Aardvark, Tomato))

    test4(3)
    test4(None)
    test4(4)

    test5((Wombat, Tomato))
    test5((I32(1), "Hi"))
    test5((Aardvark, Tomato))

  fun box test(x: Any) =>
    match x
    | var y: Wombat tag =>
      _env.out.print("Wombat")
    | var y: Aardvark tag =>
      _env.out.print("Aardvark")
    | var y: Animal tag =>
      _env.out.print("Animal")
    | var y: (Animal tag | Vegetable tag) =>
      _env.out.print("Animal or Vegetable")
    | var y: None tag =>
      _env.out.print("None")
    else
      _env.out.print("Unknown")
    end

  fun box test2(x: U32) =>
    match x
    | U32(5) - U32(2) => _env.out.print("Three")
    else
      _env.out.print("Not Three")
    end

  fun box test3(x: (Animal, Vegetable)) =>
    match x
    | (var a: Wombat, var b: Tomato) =>
      _env.out.print("Wombat, Tomato")
    else
      _env.out.print("Unknown")
    end

  fun box test4(x: (U32 | None)) =>
    match x
    | U32(3) => _env.out.print("Three")
    | None => _env.out.print("None")
    else
      _env.out.print("Unknown")
    end

  fun box test5(x: ((Animal, Vegetable) | (I32, String))) =>
    match x
    | (var a: Wombat, var b: Tomato) =>
      _env.out.print("Wombat, Tomato")
    | (I32(1), "Hi") =>
      _env.out.print("1, Hi")
    else
      _env.out.print("Unknown")
    end

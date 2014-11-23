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
      _env.out.println("Wombat")
    | var y: Aardvark tag =>
      _env.out.println("Aardvark")
    | var y: Animal tag =>
      _env.out.println("Animal")
    | var y: (Animal tag | Vegetable tag) =>
      _env.out.println("Animal or Vegetable")
    | var y: None tag =>
      _env.out.println("None")
    else
      _env.out.println("Unknown")
    end

  fun box test2(x: U32) =>
    match x
    | U32(5) - U32(2) => _env.out.println("Three")
    else
      _env.out.println("Not Three")
    end

  fun box test3(x: (Animal, Vegetable)) =>
    match x
    | (var a: Wombat, var b: Tomato) =>
      _env.out.println("Wombat, Tomato")
    else
      _env.out.println("Unknown")
    end

  fun box test4(x: (U32 | None)) =>
    match x
    | U32(3) => _env.out.println("Three")
    | None => _env.out.println("None")
    else
      _env.out.println("Unknown")
    end

  fun box test5(x: ((Animal, Vegetable) | (I32, String))) =>
    match x
    | (var a: Wombat, var b: Tomato) =>
      _env.out.println("Wombat, Tomato")
    | (I32(1), "Hi") =>
      _env.out.println("1, Hi")
    else
      _env.out.println("Unknown")
    end

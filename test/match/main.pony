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

    test2(4)

    test3((Wombat, Tomato))
    test3((Aardvark, Tomato))

  fun box test(x: Any) =>
    match x
    | var y: Wombat tag =>
      _env.stdout.print("Wombat")
    | var y: Aardvark tag =>
      _env.stdout.print("Aardvark")
    | var y: Animal tag =>
      _env.stdout.print("Animal")
    | var y: (Animal tag | Vegetable tag) =>
      _env.stdout.print("Animal or Vegetable")
    | var y: None tag =>
      _env.stdout.print("None")
    else
      _env.stdout.print("Unknown")
    end

  fun box test2(x: U32) =>
    match x
    | U32(5) - U32(2) => _env.stdout.print("Three")
    else
      _env.stdout.print("Unknown")
    end

  fun box test3(x: (Animal, Vegetable)) =>
    match x
    | (var a: Wombat, var b: Tomato) =>
      _env.stdout.print("Wombat, Tomato")
    else
      _env.stdout.print("Unknown")
    end

  /*fun box test4(x: ((Animal, Vegetable) | (I32, String))) =>
    match x
    | (var a: Wombat, var b: Tomato) =>
      _env.stdout.print("Wombat, Tomato")
    | (1, "Hi") =>
      _env.stdout.print("1, 2, Hi")
    else
      _env.stdout.print("Unknown")
    end*/

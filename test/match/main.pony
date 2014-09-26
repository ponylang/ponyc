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

  fun box test(x: Any) =>
    match x
    | var y: Wombat =>
      _env.stdout.print("Wombat")
    | var y: Aardvark =>
      _env.stdout.print("Aardvark")
    | var y: Animal =>
      _env.stdout.print("Animal")
    | var y: (Animal | Vegetable) =>
      _env.stdout.print("Animal or Vegetable")
    else
      _env.stdout.print("Unknown")
    end

// Shape has six methods, each returning a different object literal that is a
// Shape. Chaining all six through Box must reach the last one; expects f.
trait Shape
  fun name(): String => "base"
  fun a(): Shape val => object val is Shape fun name(): String => "a" end
  fun b(): Shape val => object val is Shape fun name(): String => "b" end
  fun c(): Shape val => object val is Shape fun name(): String => "c" end
  fun d(): Shape val => object val is Shape fun name(): String => "d" end
  fun e(): Shape val => object val is Shape fun name(): String => "e" end
  fun f(): Shape val => object val is Shape fun name(): String => "f" end

class Box is Shape
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(Box.a().b().c().d().e().f().name())

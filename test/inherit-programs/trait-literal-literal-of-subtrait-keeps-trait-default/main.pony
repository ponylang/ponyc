// T.mk returns an object literal of U, where U is T. C overrides name, but the
// literal is not a C, so hello() on it must use T's name and print
// "hello T", also after mk() is called again on the literal.
trait T
  fun name(): String => "T"
  fun mk(): U val => object val is U end
trait U is T
  fun hello(): String => "hello " + name()
class C is T
  new create() => None
  fun name(): String => "C"
actor Main
  new create(env: Env) =>
    env.out.print(C.mk().hello())
    env.out.print(C.mk().mk().mk().hello())

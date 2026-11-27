// T.here calls Where, whose argument defaults to __loc. Called on class C the
// location names C; called on an object literal written in Main or in M.mk it
// names T, the trait whose text holds the call. The line is 8 in every case.
primitive Where
  fun apply(l: SourceLoc = __loc): String =>
    l.type_name() + "." + l.method_name() + ":" + l.line().string()
trait T
  fun here(): String => Where()
trait M
  fun mk(): T val => object val is T end
class val C is (T & M)
  new val create() => None
actor Main
  new create(env: Env) =>
    let o = object val is T end
    env.out.print("named " + C.here())
    env.out.print("objlit " + o.here())
    env.out.print("traitlit " + C.mk().here())
    env.out.print("depth2 " + C.mk().here())

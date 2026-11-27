// T.here uses __loc directly. Called on class C, on an object literal in
// Main, or on an object literal in M.mk, the location names T.here.
trait T
  fun here(): String => __loc.type_name() + "." + __loc.method_name()
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

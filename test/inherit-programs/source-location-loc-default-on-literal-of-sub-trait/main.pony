// T.here calls Where, whose argument defaults to __loc. S inherits here from
// T. Class C reports C.here, an object literal of S reports S.here, and an
// object literal of T reports T.here.
primitive Where
  fun apply(l: SourceLoc = __loc): String =>
    l.type_name() + "." + l.method_name()
trait T
  fun here(): String => Where()
trait S is T
class val C is S
  new val create() => None
actor Main
  new create(env: Env) =>
    let o = object val is S end
    let p = object val is T end
    env.out.print("named " + C.here() + " objS " + o.here() + " objT " + p.here())

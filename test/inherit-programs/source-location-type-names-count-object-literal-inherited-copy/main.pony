// Two object literals provide T, whose here() calls Where with a __loc default.
// Expects o.own's location to name o's class at line 11, and o2's type name to
// be $1$4: o's copy of here() builds a location class too.
primitive Where
  fun apply(l: SourceLoc = __loc): String =>
    l.type_name() + "." + l.method_name() + ":" + l.line().string()
trait T
  fun here(): String => Where()
actor Main
  new create(env: Env) =>
    let o = object val is T fun own(): String => Where() end
    env.out.print("own " + o.own())
    let o2 = object val is T fun own(): String => __loc.type_name() end
    env.out.print("own2 " + o2.own())

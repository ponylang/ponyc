// U.k() returns a T[Array[U8] val] literal and T.m() returns a U literal,
// both as Any val. C is T[U16]; matching C.m() as a U and its k() as a T
// must print "u" then "t": each trait's literal class must be complete even
// though it provides the other trait.
trait U
  fun k(): Any val => object val is T[Array[U8] val] end
  fun un(): String => "u"
trait T[A: Any val]
  fun m(): Any val => object val is U end
  fun tn(): String => "t"
class val C is T[U16]
  new val create() => None
actor Main
  new create(env: Env) =>
    match C.m()
    | let u: U val =>
      env.out.print(u.un())
      match u.k()
      | let t: T[Array[U8] val] val => env.out.print(t.tn())
      end
    end

use u = "../ua"

trait X
  fun m[A: Stringable val](a: A): String =>
    let s = a.string()
    u.Thing.name() + " " + s

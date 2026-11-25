// A trait in another package has generic methods whose type parameter is
// named Foo. One uses `this` as the trait type and one uses Foo inside a
// lambda. The main package has a class named Foo that provides the trait, and
// an object literal that provides it.
use @pony_exitcode[None](code: I32)
use p = "./pkg"

class Foo is p.T
  fun name(): String => "foo"

actor Main
  new create(env: Env) =>
    if Foo.f[U8](3) != "foo3" then @pony_exitcode(1) end
    if Foo.g[U8](4) != "4" then @pony_exitcode(2) end
    let o = object is p.T end
    if o.g[U8](5) != "5" then @pony_exitcode(3) end

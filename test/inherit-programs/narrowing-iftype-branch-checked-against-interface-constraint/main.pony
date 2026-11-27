// An interface default adds 1 to x inside `iftype In <: Stringable val`, and
// nothing provides the interface. The test expects "couldn't find 'add'",
// because Stringable has no add.
interface Foo[In: Any val]
  fun f(x: In): String =>
    iftype In <: Stringable val then (x + 1).string() else "o" end
actor Main
  new create(env: Env) => None

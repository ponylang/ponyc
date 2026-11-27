// A trait with no inheritors has two defaults with errors: an add inside
// `iftype In <: Stringable val`, and a String returned as U8. The test expects
// both errors.
trait Foo[In: Any val]
  fun f(x: In): String =>
    iftype In <: Stringable val then (x + 1).string() else "o" end
  fun g(): U8 => "notU8"
actor Main
  new create(env: Env) => None

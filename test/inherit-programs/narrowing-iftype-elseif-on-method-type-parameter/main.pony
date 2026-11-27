// f() first checks the trait's A, which Baz fills with (P | None), then the
// method's own B. The A branch can't run in Baz's copy. Expects "b" for
// f[U8] and "other" for f[P]: the elseif on B is chosen per call.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f[B: Any val](b: B): String =>
    iftype A <: Stringable val then mk().string()
    elseif B <: Stringable val then "b"
    else "other" end
class Baz is Foo[(P | None)]
  new create() => None
  fun mk(): P => P
actor Main
  new create(env: Env) => env.out.print(Baz.f[U8](1)); env.out.print(Baz.f[P](P))

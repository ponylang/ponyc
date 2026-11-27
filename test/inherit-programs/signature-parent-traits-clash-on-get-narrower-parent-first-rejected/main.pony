// S3 provides S1, which redeclares get(): U64, then S2, which gets
// get(): (U64 | None) from T0. An object literal in another trait provides S3.
// The test expects the same clashing-definitions error as with the parents in
// the other order, so the order of the provides list doesn't change it.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait T0
  fun get(): (U64 | None)
  fun label(): String => Name(get())
trait S1 is T0
  fun get(): U64
trait S2 is T0
trait M
  fun mk(): T0 val => object val is S3 fun get(): U64 => 1 end
class val C is M
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.mk().label())
trait S3 is (S1 & S2)

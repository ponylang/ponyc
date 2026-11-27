// T0.mk() captures v = get() in a literal. The subtrait S overrides get() to
// return U64, so S's copy of mk() captures a U64. C.mm() returns an S
// literal; calling mk() on it directly and through S.mk2() must both print
// "u64".
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait T0
  fun get(): (U64 | None) => None
  fun lab(): String => "t0"
  fun mk(): T0 val =>
    let v = get()
    object val is T0
      fun lab(): String => Name(v)
    end
trait S is T0
  fun get(): U64 => 1
  fun mk2(): T0 val => mk()
trait M
  fun mm(): S val => object val is S end
class val C is M
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.mm().mk().lab() + "|" + C.mm().mk2().lab())

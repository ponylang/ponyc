// Group and C both guard check with the recursive alias L. Comparing the two
// guards must stop at L's reference to itself. For U8 neither guard holds, so
// the test expects Group's unguarded check, printing 10.
type L is (None | Array[L] val)
trait Group[A: Any val]
  fun check(): U64 => 10
  fun check(): U64 iftype A <: L => 20
class C[A: Any val] is Group[A]
  fun check(): U64 iftype A <: L => 40
actor Main
  new create(env: Env) =>
    env.out.print(C[U8].check().string())

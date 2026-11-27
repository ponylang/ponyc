// T.run sums get() n times, matching the U64 case of its (U64 | None) result;
// C's get returns U64 from a generator. With n = 1000 from the program
// arguments, the sum must be 1060360335704.
trait T
  fun ref get(): (U64 | None)
  fun ref run(n: U64): U64 =>
    var s: U64 = 0
    var i: U64 = 0
    while i < n do
      match get()
      | let x: U64 => s = s + x
      end
      i = i + 1
    end
    s

class C is T
  var _v: U64 = 0
  fun ref get(): U64 => _v = (_v * 6364136223846793005) + 1442695040888963407; _v >> 33

actor Main
  new create(env: Env) =>
    let n: U64 = try env.args(1)?.u64()? else 100_000_000 end
    env.out.print(C.run(n).string())

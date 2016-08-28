use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestAutoOps)

class iso _TestAutoOps is UnitTest
  fun name(): String => "ponybench/_AutoOps"
  fun apply(h: TestHelper) =>
    let ao = _AutoOps(1_000_000_000, 100_000_000)
    let tests: Array[(U64, U64, U64)] = [
      (10, 1000, 1000),
      (20, 1000, 2000),
      (30, 1000, 3000),
      (40, 1000, 5000),
      (50, 1000, 5000),
      (60, 1000, 10000)
    ]
    for (ops, time, expected) in tests.values() do
      let res = match ao(ops, time, time/ops)
      | let n: U64 => n
      else 0
      end
      h.assert_eq[U64](expected, res)
    end

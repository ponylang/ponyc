use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestNanos)

class iso _TestNanos is UnitTest
  fun name(): String => "time/Nanos"

  fun apply(h: TestHelper) =>
    h.assert_eq[U64](5_000_000_000, Nanos.from_seconds(5))
    h.assert_eq[U64](5_000_000, Nanos.from_millis(5))
    h.assert_eq[U64](5_000, Nanos.from_micros(5))

    h.assert_eq[U64](1_230_000_000, Nanos.from_seconds_f(1.23))
    h.assert_eq[U64](1_230_000, Nanos.from_millis_f(1.23))
    h.assert_eq[U64](1_230, Nanos.from_micros_f(1.23))

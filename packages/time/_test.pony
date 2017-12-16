use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestNanos)
    test(_TestPosixDate)

class iso _TestNanos is UnitTest
  fun name(): String => "time/Nanos"

  fun apply(h: TestHelper) =>
    h.assert_eq[U64](5_000_000_000, Nanos.from_seconds(5))
    h.assert_eq[U64](5_000_000, Nanos.from_millis(5))
    h.assert_eq[U64](5_000, Nanos.from_micros(5))

    h.assert_eq[U64](1_230_000_000, Nanos.from_seconds_f(1.23))
    h.assert_eq[U64](1_230_000, Nanos.from_millis_f(1.23))
    h.assert_eq[U64](1_230, Nanos.from_micros_f(1.23))

class iso _TestPosixDate is UnitTest
  fun name(): String => "time/PosixDate"

  fun apply(h: TestHelper) =>
    h.assert_eq[I64](0, PosixDate(0, 0).time())
    h.assert_eq[I64](0, PosixDate(0, 999).time())
    h.assert_eq[I64](100, PosixDate(100, 0).time())
    // negaive seconds should be changed to zero
    h.assert_eq[I64](0, PosixDate(-100, 0).time())
    // negative nanoseconds should not affect seconds
    h.assert_eq[I64](100, PosixDate(100, -999).time())

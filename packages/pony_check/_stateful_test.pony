use "pony_test"

class \nodoc\ ref _TestCounter
  var count: USize = 0

  fun ref increment() => count = count + 1

primitive \nodoc\ _Increment is Stringable
  fun string(): String iso^ => "increment".string()

type _CounterCmd is _Increment

class \nodoc\ iso _SuccessfulStatefulProperty
  is StatefulProperty[_TestCounter, USize, _CounterCmd]
  fun name(): String => "stateful/counter/successful"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 50)

  fun max_steps(): USize => 10

  fun initial_sut(): _TestCounter => _TestCounter

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[_TestCounter, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : _CounterCmd
  =>
    ctx.sut.increment()
    ctx.model = ctx.model + 1
    _Increment

  fun invariant(
    ctx: StatefulContext[_TestCounter, USize] box,
    h: PropertyHelper)
    : Bool
  =>
    h.assert_eq[USize](ctx.model, ctx.sut.count)

class \nodoc\ iso _SuccessfulStatefulPropertyTest is UnitTest
  fun name(): String => "stateful/counter/successful"

  fun apply(h: TestHelper) =>
    let property = recover iso _SuccessfulStatefulProperty end
    let notify = _UnitTestPropertyNotify(h, true)
    let logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    h.long_test(params.timeout)
    let runner =
      StatefulPropertyRunner[_TestCounter, USize, _CounterCmd](
        consume property, params, notify, logger, h.env)
    h.dispose_when_done(runner)
    runner.run()

class \nodoc\ iso _FailingStatefulProperty
  is StatefulProperty[_TestCounter, USize, _CounterCmd]
  fun name(): String => "stateful/counter/failing"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 50)

  fun max_steps(): USize => 10

  fun initial_sut(): _TestCounter => _TestCounter

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[_TestCounter, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : _CounterCmd
  =>
    ctx.sut.increment()
    ctx.model = ctx.model + 1
    _Increment

  fun invariant(
    ctx: StatefulContext[_TestCounter, USize] box,
    h: PropertyHelper)
    : Bool
  =>
    h.assert_true(ctx.sut.count <= 3)

class \nodoc\ iso _FailingStatefulPropertyTest is UnitTest
  fun name(): String => "stateful/counter/failing"

  fun apply(h: TestHelper) =>
    let property = recover iso _FailingStatefulProperty end
    let notify = _UnitTestPropertyNotify(h, false)
    let logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    h.long_test(params.timeout)
    let runner =
      StatefulPropertyRunner[_TestCounter, USize, _CounterCmd](
        consume property, params, notify, logger, h.env)
    h.dispose_when_done(runner)
    runner.run()

class \nodoc\ iso _StatefulUnitTestAdapterTest is UnitTest
  fun name(): String => "stateful/unit_test_adapter"

  fun apply(h: TestHelper) ? =>
    let wrapped =
      StatefulPropertyUnitTest[_TestCounter, USize, _CounterCmd](
        _SuccessfulStatefulProperty)
    h.assert_eq[String]("stateful/counter/successful", wrapped.name())
    wrapped.apply(h)?

class \nodoc\ iso _StatefulMaxStepsZeroProperty
  is StatefulProperty[_TestCounter, USize, _CounterCmd]
  fun name(): String => "stateful/max_steps_zero"

  fun max_steps(): USize => 0

  fun initial_sut(): _TestCounter => _TestCounter

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[_TestCounter, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : _CounterCmd
  =>
    _Increment

class \nodoc\ iso _StatefulMaxStepsZeroTest is UnitTest
  fun name(): String => "stateful/max_steps_zero"

  fun apply(h: TestHelper) =>
    let property = recover iso _StatefulMaxStepsZeroProperty end
    let notify = _UnitTestPropertyNotify(h, false)
    let logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    h.long_test(params.timeout)
    let runner =
      StatefulPropertyRunner[_TestCounter, USize, _CounterCmd](
        consume property, params, notify, logger, h.env)
    h.dispose_when_done(runner)
    runner.run()

class \nodoc\ iso _StatefulAsyncRejectedProperty
  is StatefulProperty[_TestCounter, USize, _CounterCmd]
  fun name(): String => "stateful/async_rejected"

  fun params(): PropertyParams =>
    PropertyParams(where async' = true)

  fun max_steps(): USize => 5

  fun initial_sut(): _TestCounter => _TestCounter

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[_TestCounter, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : _CounterCmd
  =>
    _Increment

class \nodoc\ iso _StatefulAsyncRejectedTest is UnitTest
  fun name(): String => "stateful/async_rejected"

  fun apply(h: TestHelper) =>
    let property = recover iso _StatefulAsyncRejectedProperty end
    let notify = _UnitTestPropertyNotify(h, false)
    let logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    h.long_test(params.timeout)
    let runner =
      StatefulPropertyRunner[_TestCounter, USize, _CounterCmd](
        consume property, params, notify, logger, h.env)
    h.dispose_when_done(runner)
    runner.run()

class \nodoc\ iso _StatefulStepAlwaysErrorsProperty
  is StatefulProperty[_TestCounter, USize, _CounterCmd]
  fun name(): String => "stateful/step_always_errors"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, max_generator_retries' = 3)

  fun max_steps(): USize => 5

  fun initial_sut(): _TestCounter => _TestCounter

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[_TestCounter, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : _CounterCmd ?
  =>
    error

class \nodoc\ iso _StatefulStepAlwaysErrorsTest is UnitTest
  fun name(): String => "stateful/step_always_errors"

  fun apply(h: TestHelper) =>
    let property = recover iso _StatefulStepAlwaysErrorsProperty end
    let notify = _UnitTestPropertyNotify(h, false)
    let logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    h.long_test(params.timeout)
    let runner =
      StatefulPropertyRunner[_TestCounter, USize, _CounterCmd](
        consume property, params, notify, logger, h.env)
    h.dispose_when_done(runner)
    runner.run()

class \nodoc\ iso _StatefulFinalCheckFailureProperty
  is StatefulProperty[_TestCounter, USize, _CounterCmd]
  fun name(): String => "stateful/final_check_failure"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 50)

  fun max_steps(): USize => 5

  fun initial_sut(): _TestCounter => _TestCounter

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[_TestCounter, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : _CounterCmd
  =>
    ctx.sut.increment()
    ctx.model = ctx.model + 1
    _Increment

  fun final_check(
    ctx: StatefulContext[_TestCounter, USize] box,
    h: PropertyHelper)
    : Bool
  =>
    h.assert_true(ctx.sut.count <= 2)

class \nodoc\ iso _StatefulFinalCheckFailureTest is UnitTest
  fun name(): String => "stateful/final_check_failure"

  fun apply(h: TestHelper) =>
    let property = recover iso _StatefulFinalCheckFailureProperty end
    let notify = _UnitTestPropertyNotify(h, false)
    let logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    h.long_test(params.timeout)
    let runner =
      StatefulPropertyRunner[_TestCounter, USize, _CounterCmd](
        consume property, params, notify, logger, h.env)
    h.dispose_when_done(runner)
    runner.run()

actor \nodoc\ _ShrinkQualityChecker
  let _th: TestHelper
  var _shrunk_ok: Bool = false

  new create(th: TestHelper) =>
    _th = th

  be check_msg(msg: String) =>
    _shrunk_ok =
      msg.contains("4. increment") and
        (not msg.contains("5. increment"))

  be finish(success: Bool) =>
    if success then
      _th.complete(false)
    else
      _th.complete(_shrunk_ok)
    end

class \nodoc\ val _ShrinkQualityNotify is PropertyResultNotify
  let _checker: _ShrinkQualityChecker

  new val create(checker: _ShrinkQualityChecker) =>
    _checker = checker

  fun fail(msg: String) =>
    _checker.check_msg(msg)

  fun complete(success: Bool) =>
    _checker.finish(success)

class \nodoc\ iso _StatefulShrinkQualityTest is UnitTest
  fun name(): String => "stateful/shrink_quality"

  fun apply(h: TestHelper) =>
    let property = recover iso _FailingStatefulProperty end
    let checker = _ShrinkQualityChecker(h)
    let notify = _ShrinkQualityNotify(checker)
    let logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    h.long_test(params.timeout)
    let runner =
      StatefulPropertyRunner[_TestCounter, USize, _CounterCmd](
        consume property, params, notify, logger, h.env)
    h.dispose_when_done(runner)
    runner.run()

primitive \nodoc\ _Reset is Stringable
  fun string(): String iso^ => "reset".string()

type _MultiCmd is (_Increment | _Reset)

class \nodoc\ iso _MultiCommandStatefulProperty
  is StatefulProperty[_TestCounter, USize, _MultiCmd]
  fun name(): String => "stateful/multi_command"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 50)

  fun max_steps(): USize => 10

  fun initial_sut(): _TestCounter => _TestCounter

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[_TestCounter, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : _MultiCmd ?
  =>
    if rnd.bool()? then
      ctx.sut.increment()
      ctx.model = ctx.model + 1
      _Increment
    else
      ctx.sut.count = 0
      ctx.model = 0
      _Reset
    end

  fun invariant(
    ctx: StatefulContext[_TestCounter, USize] box,
    h: PropertyHelper)
    : Bool
  =>
    h.assert_eq[USize](ctx.model, ctx.sut.count)

class \nodoc\ iso _MultiCommandStatefulPropertyTest is UnitTest
  fun name(): String => "stateful/multi_command"

  fun apply(h: TestHelper) =>
    let property = recover iso _MultiCommandStatefulProperty end
    let notify = _UnitTestPropertyNotify(h, true)
    let logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    h.long_test(params.timeout)
    let runner =
      StatefulPropertyRunner[_TestCounter, USize, _MultiCmd](
        consume property, params, notify, logger, h.env)
    h.dispose_when_done(runner)
    runner.run()

use "pony_test"

use "collections"
use "itertools"
use "random"
use "time"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)

  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_ASCIIRangeTest)
    test(_ASCIIStringShrinkTest)
    test(_ErroringPropertyTest)
    test(_FailingPropertyTest)
    test(_FilterMapShrinkTest)
    test(_ForAllTest)
    test(_ForAll2Test)
    test(_ForAll3Test)
    test(_ForAll4Test)
    test(_GenFilterTest)
    test(_GenFrequencySafeTest)
    test(_GenFrequencyTest)
    test(_GenOneOfSafeTest)
    test(_GenOneOfTest)
    test(_GenRndTest)
    test(_GenUnionTest)
    test(_IsoSeqOfTest)
    test(_MapIsOfEmptyTest)
    test(_MapIsOfIdentityTest)
    test(_MapIsOfMaxTest)
    test(_MapOfEmptyTest)
    test(_MapOfIdentityTest)
    test(_MapOfMaxTest)
    test(_MinASCIIStringShrinkTest)
    test(_MinUnicodeStringShrinkTest)
    test(_MultipleForAllTest)
    test(Property1UnitTest[(I8, I8)](_RandomnessProperty[I8, _RandomCaseI8]("I8")))
    test(Property1UnitTest[(I16, I16)](_RandomnessProperty[I16, _RandomCaseI16]("I16")))
    test(Property1UnitTest[(I32, I32)](_RandomnessProperty[I32, _RandomCaseI32]("I32")))
    test(Property1UnitTest[(I64, I64)](_RandomnessProperty[I64, _RandomCaseI64]("I64")))
    test(Property1UnitTest[(I128, I128)](_RandomnessProperty[I128, _RandomCaseI128]("I128")))
    test(Property1UnitTest[(ILong, ILong)](_RandomnessProperty[ILong, _RandomCaseILong]("ILong")))
    test(Property1UnitTest[(ISize, ISize)](_RandomnessProperty[ISize, _RandomCaseISize]("ISize")))
    test(Property1UnitTest[(U8, U8)](_RandomnessProperty[U8, _RandomCaseU8]("U8")))
    test(Property1UnitTest[(U16, U16)](_RandomnessProperty[U16, _RandomCaseU16]("U16")))
    test(Property1UnitTest[(U32, U32)](_RandomnessProperty[U32, _RandomCaseU32]("U32")))
    test(Property1UnitTest[(U64, U64)](_RandomnessProperty[U64, _RandomCaseU64]("U64")))
    test(Property1UnitTest[(U128, U128)](_RandomnessProperty[U128, _RandomCaseU128]("U128")))
    test(_RunnerAsyncCompleteActionTest)
    test(_RunnerAsyncCompleteMultiActionTest)
    test(_RunnerAsyncCompleteMultiSucceedActionTest)
    test(_RunnerAsyncFailTest)
    test(_RunnerAsyncMultiCompleteFailTest)
    test(_RunnerAsyncMultiCompleteSucceedTest)
    test(_RunnerAsyncPropertyCompleteFalseTest)
    test(_RunnerAsyncPropertyCompleteTest)
    test(_RunnerErroringGeneratorTest)
    test(_RunnerInfiniteShrinkTest)
    test(_RunnerReportFailedSampleTest)
    test(_RunnerSometimesErroringGeneratorTest)
    test(_SeqOfTest)
    test(_SetIsOfIdentityTest)
    test(_SetOfEmptyTest)
    test(_SetOfMaxTest)
    test(_SetOfTest)
    test(_SignedShrinkTest)
    test(_StringifyTest)
    test(Property1UnitTest[U8](_SuccessfulProperty))
    test(Property2UnitTest[U8, U8](_SuccessfulProperty2))
    test(Property3UnitTest[U8, U8, U8](_SuccessfulProperty3))
    test(Property4UnitTest[U8, U8, U8, U8](_SuccessfulProperty4))
    test(IntPairUnitTest(_SuccessfulIntPairProperty))
    test(_SuccessfulIntPairPropertyTest)
    test(IntUnitTest(_SuccessfulIntProperty))
    test(_SuccessfulIntPropertyTest)
    test(_SuccessfulPropertyTest)
    test(_SuccessfulProperty2Test)
    test(_SuccessfulProperty3Test)
    test(_SuccessfulProperty4Test)
    test(_UnicodeStringShrinkTest)
    test(_UnsignedShrinkTest)
    test(_UTF32CodePointStringTest)


class \nodoc\ iso _StringifyTest is UnitTest
  fun name(): String => "stringify"

  fun apply(h: TestHelper) =>
    (let _, var s) = _Stringify.apply[(U8, U8)]((0, 1))
    h.assert_eq[String](s, "(0, 1)")
    (let _, s) = _Stringify.apply[(U8, U32, U128)]((0, 1, 2))
    h.assert_eq[String](s, "(0, 1, 2)")
    (let _, s) = _Stringify.apply[(U8, (U32, U128))]((0, (1, 2)))
    h.assert_eq[String](s, "(0, (1, 2))")
    (let _, s) = _Stringify.apply[((U8, U32), U128)](((0, 1), 2))
    h.assert_eq[String](s, "((0, 1), 2)")
    let a: Array[U8] = [ U8(0); U8(42) ]
    (let _, s) = _Stringify.apply[Array[U8]](a)
    h.assert_eq[String](s, "[0 42]")

class \nodoc\ iso _SuccessfulProperty is Property1[U8]
  """
  this just tests that a property is compatible with PonyTest
  """
  fun name(): String => "as_unit_test/successful/property"

  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun ref property(arg1: U8, h: PropertyHelper) =>
    h.assert_true(arg1 <= U8(10))

class \nodoc\ iso _SuccessfulPropertyTest is UnitTest
  fun name(): String => "as_unit_test/successful"

  fun apply(h: TestHelper) =>
    let property = recover iso _SuccessfulProperty end
    let property_notify = _UnitTestPropertyNotify(h, true)
    let property_logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    h.long_test(params.timeout)
    let runner = PropertyRunner[U8](
      consume property,
      params,
      property_notify,
      property_logger,
      h.env)
    runner.run()

class \nodoc\ iso _FailingProperty is Property1[U8]
  fun name(): String => "as_unit_test/failing/property"

  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun ref property(arg1: U8, h: PropertyHelper) =>
    h.assert_true(arg1 <= U8(5))

class \nodoc\ iso _FailingPropertyTest is UnitTest
  fun name(): String => "as_unit_test/failing"

  fun apply(h: TestHelper) =>
    let property = recover iso _FailingProperty end
    let property_notify = _UnitTestPropertyNotify(h, false)
    let property_logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    h.long_test(params.timeout)
    let runner = PropertyRunner[U8](
      consume property,
      params,
      property_notify,
      property_logger,
      h.env)
    runner.run()

class \nodoc\ iso _ErroringProperty is Property1[U8]
  fun name(): String => "as_unit_test/erroring/property"

  fun gen(): Generator[U8] => Generators.u8(0, 1)

  fun ref property(arg1: U8, h: PropertyHelper) ? =>
    if arg1 < 2 then
      error
    end

class \nodoc\ iso _ErroringPropertyTest is UnitTest
  fun name(): String => "as_unit_test/erroring"

  fun apply(h: TestHelper) =>
    h.long_test(20_000_000_000)
    let property = recover iso _ErroringProperty end
    let property_notify = _UnitTestPropertyNotify(h, false)
    let property_logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    let runner = PropertyRunner[U8](
      consume property,
      params,
      property_notify,
      property_logger,
      h.env)
    runner.run()


class \nodoc\ iso _SuccessfulProperty2 is Property2[U8, U8]
  fun name(): String => "as_unit_test/successful2/property"
  fun gen1(): Generator[U8] => Generators.u8(0, 1)
  fun gen2(): Generator[U8] => Generators.u8(2, 3)

  fun ref property2(arg1: U8, arg2: U8, h: PropertyHelper) =>
    h.assert_ne[U8](arg1, arg2)

class \nodoc\ iso _SuccessfulProperty2Test is UnitTest
  fun name(): String => "as_unit_test/successful2"

  fun apply(h: TestHelper) =>
    let property2 = recover iso _SuccessfulProperty2 end
    let property2_notify = _UnitTestPropertyNotify(h, true)
    let property2_logger = _UnitTestPropertyLogger(h)
    let params = property2.params()
    h.long_test(params.timeout)
    let runner = PropertyRunner[(U8, U8)](
      consume property2,
      params,
      property2_notify,
      property2_logger,
      h.env)
    runner.run()

class \nodoc\ iso _SuccessfulProperty3 is Property3[U8, U8, U8]
  fun name(): String => "as_unit_test/successful3/property"
  fun gen1(): Generator[U8] => Generators.u8(0, 1)
  fun gen2(): Generator[U8] => Generators.u8(2, 3)
  fun gen3(): Generator[U8] => Generators.u8(4, 5)

  fun ref property3(arg1: U8, arg2: U8, arg3: U8, h: PropertyHelper) =>
    h.assert_ne[U8](arg1, arg2)
    h.assert_ne[U8](arg2, arg3)
    h.assert_ne[U8](arg1, arg3)

class \nodoc\ iso _SuccessfulProperty3Test is UnitTest

  fun name(): String => "as_unit_test/successful3"

  fun apply(h: TestHelper) =>
    let property3 = recover iso _SuccessfulProperty3 end
    let property3_notify = _UnitTestPropertyNotify(h, true)
    let property3_logger = _UnitTestPropertyLogger(h)
    let params = property3.params()
    h.long_test(params.timeout)
    let runner = PropertyRunner[(U8, U8, U8)](
      consume property3,
      params,
      property3_notify,
      property3_logger,
      h.env)
    runner.run()

class \nodoc\ iso _SuccessfulProperty4 is Property4[U8, U8, U8, U8]
  fun name(): String => "as_unit_test/successful4/property"
  fun gen1(): Generator[U8] => Generators.u8(0, 1)
  fun gen2(): Generator[U8] => Generators.u8(2, 3)
  fun gen3(): Generator[U8] => Generators.u8(4, 5)
  fun gen4(): Generator[U8] => Generators.u8(6, 7)

  fun ref property4(arg1: U8, arg2: U8, arg3: U8, arg4: U8, h: PropertyHelper) =>
    h.assert_ne[U8](arg1, arg2)
    h.assert_ne[U8](arg1, arg3)
    h.assert_ne[U8](arg1, arg4)
    h.assert_ne[U8](arg2, arg3)
    h.assert_ne[U8](arg2, arg4)
    h.assert_ne[U8](arg3, arg4)

class \nodoc\ iso _SuccessfulProperty4Test is UnitTest

  fun name(): String => "as_unit_test/successful4"

  fun apply(h: TestHelper) =>
    let property4 = recover iso _SuccessfulProperty4 end
    let property4_notify = _UnitTestPropertyNotify(h, true)
    let property4_logger = _UnitTestPropertyLogger(h)
    let params = property4.params()
    h.long_test(params.timeout)
    let runner = PropertyRunner[(U8, U8, U8, U8)](
      consume property4,
      params,
      property4_notify,
      property4_logger,
      h.env)
    runner.run()

class \nodoc\ iso _RunnerAsyncPropertyCompleteTest is UnitTest
  fun name(): String => "property_runner/async/complete"

  fun apply(h: TestHelper) =>
    _Async.run_async_test(h, {(ph) => ph.complete(true) }, true)

class \nodoc\ iso _RunnerAsyncPropertyCompleteFalseTest is UnitTest
  fun name(): String => "property_runner/async/complete-false"

  fun apply(h: TestHelper) =>
    _Async.run_async_test(h,{(ph) => ph.complete(false) }, false)

class \nodoc\ iso _RunnerAsyncFailTest is UnitTest
  fun name(): String => "property_runner/async/fail"

  fun apply(h: TestHelper) =>
    _Async.run_async_test(h, {(ph) => ph.fail("Oh noes!") }, false)

class \nodoc\ iso _RunnerAsyncMultiCompleteSucceedTest is UnitTest
  fun name(): String => "property_runner/async/multi_succeed"

  fun apply(h: TestHelper) =>
    _Async.run_async_test(
      h,
      {(ph) =>
        ph.complete(true)
        ph.complete(false)
      }, true)

class \nodoc\ iso _RunnerAsyncMultiCompleteFailTest is UnitTest
  fun name(): String => "property_runner/async/multi_fail"

  fun apply(h: TestHelper) =>
    _Async.run_async_test(
      h,
      {(ph) =>
        ph.complete(false)
        ph.complete(true)
      }, false)

class \nodoc\ iso _RunnerAsyncCompleteActionTest is UnitTest
  fun name(): String => "property_runner/async/complete_action"

  fun apply(h: TestHelper) =>
    _Async.run_async_test(
      h,
      {(ph) =>
        let action = "blaaaa"
        ph.expect_action(action)
        ph.complete_action(action)
      },
      true)

class \nodoc\ iso _RunnerAsyncCompleteFalseActionTest is UnitTest
  fun name(): String => "property_runner/async/complete_action"

  fun apply(h: TestHelper) =>
    _Async.run_async_test(
      h,
      {(ph) =>
        let action = "blaaaa"
        ph.expect_action(action)
        ph.fail_action(action)
      }, false)

class \nodoc\ iso _RunnerAsyncCompleteMultiActionTest is UnitTest
  fun name(): String => "property_runner/async/complete_multi_action"

  fun apply(h: TestHelper) =>
    _Async.run_async_test(
      h,
      {(ph) =>
        let action = "only-once"
        ph.expect_action(action)
        ph.fail_action(action)
        ph.complete_action(action) // should be ignored
      },
      false)

class \nodoc\ iso _RunnerAsyncCompleteMultiSucceedActionTest is UnitTest
  fun name(): String => "property_runner/async/complete_multi_fail_action"

  fun apply(h: TestHelper) =>
    _Async.run_async_test(
      h,
      {(ph) =>
        let action = "succeed-once"
        ph.expect_action(action)
        ph.complete_action(action)
        ph.fail_action(action)
      },
      true)

class \nodoc\ iso _ForAllTest is UnitTest
  fun name(): String => "pony_check/for_all"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[U8](recover Generators.unit[U8](0) end, h)(
      {(u, h) => h.assert_eq[U8](u, 0, u.string() + " == 0") })?

class \nodoc\ iso _MultipleForAllTest is UnitTest
  fun name(): String => "pony_check/multiple_for_all"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[U8](recover Generators.unit[U8](0) end, h)(
      {(u, h) => h.assert_eq[U8](u, 0, u.string() + " == 0") })?

    PonyCheck.for_all[U8](recover Generators.unit[U8](1) end, h)(
      {(u, h) => h.assert_eq[U8](u, 1, u.string() + " == 1") })?

class \nodoc\ iso _ForAll2Test is UnitTest
  fun name(): String => "pony_check/for_all2"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all2[U8, String](
      recover Generators.unit[U8](0) end,
      recover Generators.ascii() end,
      h)(
        {(arg1, arg2, h) =>
          h.assert_false(arg2.contains(String.from_array([as U8: arg1])))
        })?

class \nodoc\ iso _ForAll3Test is UnitTest
  fun name(): String => "pony_check/for_all3"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all3[U8, U8, String](
      recover Generators.unit[U8](0) end,
      recover Generators.unit[U8](255) end,
      recover Generators.ascii() end,
      h)(
        {(b1, b2, str, h) =>
          h.assert_false(str.contains(String.from_array([b1])))
          h.assert_false(str.contains(String.from_array([b2])))
        })?

class \nodoc\ iso _ForAll4Test is UnitTest
  fun name(): String => "pony_check/for_all4"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all4[U8, U8, U8, String](
      recover Generators.unit[U8](0) end,
      recover Generators.u8() end,
      recover Generators.u8() end,
      recover Generators.ascii() end,
      h)(
        {(b1, b2, b3, str, h) =>
          let cmp = String.from_array([b1; b2; b3])
          h.assert_false(str.contains(cmp))
        })?

class \nodoc\ iso _GenRndTest is UnitTest
  fun name(): String => "Gen/random_behaviour"

  fun apply(h: TestHelper) ? =>
    let gen = Generators.u32()
    let rnd1 = Randomness(0)
    let rnd2 = Randomness(0)
    let rnd3 = Randomness(1)
    var same: U32 = 0
    for x in Range(0, 100) do
      let g1 = gen.generate_value(rnd1)?
      let g2 = gen.generate_value(rnd2)?
      let g3 = gen.generate_value(rnd3)?
      h.assert_eq[U32](g1, g2)
      if g1 == g3 then
        same = same + 1
      end
    end
    h.assert_ne[U32](same, 100)


class \nodoc\ iso _GenFilterTest is UnitTest
  fun name(): String => "Gen/filter"

  fun apply(h: TestHelper) ? =>
    """
    ensure that filter condition is met for all generated results
    """
    let gen = Generators.u32().filter({
      (u: U32^): (U32^, Bool) =>
        (u, (u%2) == 0)
    })
    let rnd = Randomness(Time.millis())
    for x in Range(0, 100) do
      let v = gen.generate_value(rnd)?
      h.assert_true((v%2) == 0)
    end

class \nodoc\ iso _GenUnionTest is UnitTest
  fun name(): String => "Gen/union"

  fun apply(h: TestHelper) ? =>
    """
    assert that a unioned Generator
    produces shrinks of the same type than the generated value.
    """
    let gen = Generators.ascii().union[U8](Generators.u8())
    let rnd = Randomness(Time.millis())
    for x in Range(0, 100) do
      let gs = gen.generate(rnd)?
      match gs
      | (let vs: String, let shrink_iter: Iterator[String^]) =>
        h.assert_true(true)
      | (let vs: U8, let shrink_iter: Iterator[U8^]) =>
        h.assert_true(true)
      | (let vs: U8, let shrink_iter: Iterator[String^]) =>
        h.fail("u8 value, string shrink iter")
      | (let vs: String, let shrink_iter: Iterator[U8^]) =>
        h.fail("string value, u8 shrink iter")
      else
        h.fail("invalid type generated")
      end
    end

class \nodoc\ iso _GenFrequencyTest is UnitTest
  fun name(): String => "Gen/frequency"

  fun apply(h: TestHelper) ? =>
    """
    ensure that Generators.frequency(...) generators actually return different
    values with given frequency
    """
    let gen = Generators.frequency[U8]([
      as WeightedGenerator[U8]:
      (1, Generators.unit[U8](0))
      (0, Generators.unit[U8](42))
      (2, Generators.unit[U8](1))
    ])
    let rnd: Randomness ref = Randomness(Time.millis())

    let generated = Array[U8](100)
    for i in Range(0, 100) do
      generated.push(gen.generate_value(rnd)?)
    end
    h.assert_false(generated.contains(U8(42)), "frequency generated value with 0 weight")
    h.assert_true(generated.contains(U8(0)), "frequency did not generate value with weight of 1")
    h.assert_true(generated.contains(U8(1)), "frequency did not generate value with weight of 2")

    let empty_gen = Generators.frequency[U8](Array[WeightedGenerator[U8]](0))

    h.assert_error({() ? =>
      empty_gen.generate_value(Randomness(Time.millis()))?
    })

class \nodoc\ iso _GenFrequencySafeTest is UnitTest
  fun name(): String => "Gen/frequency_safe"

  fun apply(h: TestHelper) =>
    h.assert_error({() ? =>
      Generators.frequency_safe[U8](Array[WeightedGenerator[U8]](0))?
    })

class \nodoc\ iso _GenOneOfTest is UnitTest
  fun name(): String => "Gen/one_of"

  fun apply(h: TestHelper) =>
    let gen = Generators.one_of[U8]([as U8: 0; 1])
    let rnd = Randomness(Time.millis())
    h.assert_true(
      Iter[U8^](gen.value_iter(rnd))
        .take(100)
        .all({(u: U8): Bool => (u == 0) or (u == 1) }),
      "one_of generator generated illegal values")
    let empty_gen = Generators.one_of[U8](Array[U8](0))

    h.assert_error({() ? =>
      empty_gen.generate_value(Randomness(Time.millis()))?
    })

class \nodoc\ iso _GenOneOfSafeTest is UnitTest
  fun name(): String => "Gen/one_of_safe"

  fun apply(h: TestHelper) =>
    h.assert_error({() ? =>
      Generators.one_of_safe[U8](Array[U8](0))?
    })

class \nodoc\ iso _SeqOfTest is UnitTest
  fun name(): String => "Gen/seq_of"

  fun apply(h: TestHelper) ? =>
    let seq_gen =
      Generators.seq_of[U8, Array[U8]](
        Generators.u8(),
        0,
        10)
    let rnd = Randomness(Time.millis())
    h.assert_true(
      Iter[Array[U8]^](seq_gen.value_iter(rnd))
        .take(100)
        .all({
          (a: Array[U8]): Bool =>
            (a.size() >= 0) and (a.size() <= 10) }),
      "Seqs generated with Generators.seq_of are out of bounds")

    match seq_gen.generate(rnd)?
    | (let gen_sample: Array[U8], let shrinks: Iter[Array[U8]^]) =>
      let max_size = gen_sample.size()
      h.assert_true(
        Iter[Array[U8]^](shrinks)
          .all({(a: Array[U8]): Bool =>
            if not (a.size() < max_size) then
              h.log(a.size().string() + " >= " + max_size.string())
              false
            else
              true
            end
          }),
        "shrinking of Generators.seq_of produces too big Seqs")
    else
      h.fail("Generators.seq_of did not produce any shrinks")
    end

class \nodoc\ iso _IsoSeqOfTest is UnitTest
  let min: USize = 0
  let max: USize = 200
  fun name(): String => "Gen/iso_seq_of"

  fun apply(h: TestHelper) ? =>
    let seq_gen = Generators.iso_seq_of[String, Array[String] iso](
      Generators.ascii(),
      min,
      max
    )
    let rnd = Randomness(Time.millis())
    h.assert_true(
      Iter[Array[String] iso^](seq_gen.value_iter(rnd))
        .take(100)
        .all({
          (a: Array[String] iso): Bool =>
            (a.size() >= min) and (a.size() <= max) }),
      "Seqs generated with Generators.iso_seq_of are out of bounds")

    match seq_gen.generate(rnd)?
    | (let gen_sample: Array[String] iso, let shrinks: Iter[Array[String] iso^]) =>
      let max_size = gen_sample.size()
      h.assert_true(
        Iter[Array[String] iso^](shrinks)
          .all({(a: Array[String] iso): Bool =>
            if not (a.size() < max_size) then
              h.log(a.size().string() + " >= " + max_size.string())
              false
            else
              true
            end
          }),
        "shrinking of Generators.iso_seq_of produces too big Seqs")
    else
      h.fail("Generators.iso_seq_of did not produce any shrinks")
    end

class \nodoc\ iso _SetOfTest is UnitTest
  fun name(): String => "Gen/set_of"

  fun apply(h: TestHelper) ? =>
    """
    this mainly tests that a source generator with a smaller range
    than max is terminating and generating sane sets
    """
    let set_gen =
      Generators.set_of[U8](
        Generators.u8(),
        1024)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample: Set[U8] = set_gen.generate_value(rnd)?
      h.assert_true(sample.size() <= 256, "something about U8 is not right")
    end

class \nodoc\ iso _SetOfMaxTest is UnitTest
  fun name(): String => "Gen/set_of_max"

  fun apply(h: TestHelper) ? =>
    """
    """
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, U8.max_value().usize()) do
      let set_gen =
        Generators.set_of[U8](
          Generators.u8(),
          size)
      let sample: Set[U8] = set_gen.generate_value(rnd)?
      h.assert_true(sample.size() <= size, "generated set is too big.")
    end


class \nodoc\ iso _SetOfEmptyTest is UnitTest
  fun name(): String => "Gen/set_of_empty"

  fun apply(h: TestHelper) ? =>
    """
    """
    let set_gen =
      Generators.set_of[U8](
        Generators.u8(),
        0)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample: Set[U8] = set_gen.generate_value(rnd)?
      h.assert_true(sample.size() == 0, "non-empty set created.")
    end

class \nodoc\ iso _SetIsOfIdentityTest is UnitTest
  fun name(): String => "Gen/set_is_of_identity"
  fun apply(h: TestHelper) ? =>
    """
    """
    let set_is_gen_same =
      Generators.set_is_of[String](
        Generators.unit[String]("the highlander"),
        100)
    let rnd = Randomness(Time.millis())
    let sample: SetIs[String] = set_is_gen_same.generate_value(rnd)?
    h.assert_true(sample.size() <= 1,
        "invalid SetIs instances generated: size " + sample.size().string())

class \nodoc\ iso _MapOfEmptyTest is UnitTest
  fun name(): String => "Gen/map_of_empty"

  fun apply(h: TestHelper) ? =>
    """
    """
    let map_gen =
      Generators.map_of[String, I64](
        Generators.zip2[String, I64](
          Generators.u8().map[String]({(u: U8): String^ =>
            let s = u.string()
            consume s }),
          Generators.i64(-10, 10)
          ),
        0)
    let rnd = Randomness(Time.millis())
    let sample = map_gen.generate_value(rnd)?
    h.assert_eq[USize](sample.size(), 0, "non-empty map created")

class \nodoc\ iso _MapOfMaxTest is UnitTest
  fun name(): String => "Gen/map_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())

    for size in Range(1, U8.max_value().usize()) do
      let map_gen =
        Generators.map_of[String, I64](
          Generators.zip2[String, I64](
            Generators.u16().map[String^]({(u: U16): String^ =>
              u.string()
            }),
            Generators.i64(-10, 10)
            ),
        size)
      let sample = map_gen.generate_value(rnd)?
      h.assert_true(sample.size() <= size, "generated map is too big.")
    end

class \nodoc\ iso _MapOfIdentityTest is UnitTest
  fun name(): String => "Gen/map_of_identity"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let map_gen =
      Generators.map_of[String, I64](
        Generators.zip2[String, I64](
          Generators.repeatedly[String]({(): String^ =>
            let s = recover String.create(14) end
            s.add("the highlander")
            consume s }),
          Generators.i64(-10, 10)
          ),
      100)
    let sample = map_gen.generate_value(rnd)?
    h.assert_true(sample.size() <= 1)

class \nodoc\ iso _MapIsOfEmptyTest is UnitTest
  fun name(): String => "Gen/map_is_of_empty"

  fun apply(h: TestHelper) ? =>
    """
    """
    let map_is_gen =
      Generators.map_is_of[String, I64](
        Generators.zip2[String, I64](
          Generators.u8().map[String]({(u: U8): String^ =>
            let s = u.string()
            consume s }),
          Generators.i64(-10, 10)
          ),
        0)
    let rnd = Randomness(Time.millis())
    let sample = map_is_gen.generate_value(rnd)?
    h.assert_eq[USize](sample.size(), 0, "non-empty map created")

class \nodoc\ iso _MapIsOfMaxTest is UnitTest
  fun name(): String => "Gen/map_is_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())

    for size in Range(1, U8.max_value().usize()) do
      let map_is_gen =
        Generators.map_is_of[String, I64](
          Generators.zip2[String, I64](
            Generators.u16().map[String]({(u: U16): String^ =>
              let s = u.string()
              consume s }),
            Generators.i64(-10, 10)
            ),
        size)
      let sample = map_is_gen.generate_value(rnd)?
      h.assert_true(sample.size() <= size, "generated map is too big.")
    end

class \nodoc\ iso _MapIsOfIdentityTest is UnitTest
  fun name(): String => "Gen/map_is_of_identity"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let map_gen =
      Generators.map_is_of[String, I64](
        Generators.zip2[String, I64](
          Generators.unit[String]("the highlander"),
          Generators.i64(-10, 10)
          ),
      100)
    let sample = map_gen.generate_value(rnd)?
    h.assert_true(sample.size() <= 1)

class \nodoc\ iso _ASCIIRangeTest is UnitTest
  fun name(): String => "Gen/ascii_range"
  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let ascii_gen = Generators.ascii( where min=1, max=1, range=ASCIIAll)

    for i in Range[USize](0, 100) do
      let sample = ascii_gen.generate_value(rnd)?
      h.assert_true(ASCIIAll().contains(sample), "\"" + sample + "\" not valid ascii")
    end

class \nodoc\ iso _UTF32CodePointStringTest is UnitTest
  fun name(): String => "Gen/utf32_codepoint_string"
  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let string_gen = Generators.utf32_codepoint_string(
      Generators.u32(),
      50,
      100)

    for i in Range[USize](0, 100) do
      let sample = string_gen.generate_value(rnd)?
      for cp in sample.runes() do
        h.assert_true((cp <= 0xD7FF ) or (cp >= 0xE000), "\"" + sample + "\" invalid utf32")
      end
    end

class \nodoc\ iso _SuccessfulIntProperty is IntProperty
  fun name(): String  => "property/int/property"

  fun ref int_property[T: (Int & Integer[T] val)](x: T, h: PropertyHelper) =>
    h.assert_eq[T](x.min(T.max_value()), x)
    h.assert_eq[T](x.max(T.min_value()), x)

class \nodoc\ iso _SuccessfulIntPropertyTest is UnitTest
  fun name(): String => "property/int"

  fun apply(h: TestHelper) =>
    let property = recover iso _SuccessfulIntProperty end
    let property_notify = _UnitTestPropertyNotify(h, true)
    let property_logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    h.long_test(params.timeout)
    let runner = PropertyRunner[IntPropertySample](
      consume property,
      params,
      property_notify,
      property_logger,
      h.env)
    runner.run()

class \nodoc\ iso _SuccessfulIntPairProperty is IntPairProperty
  fun name(): String => "property/intpair/property"

  fun int_property[T: (Int & Integer[T] val)](x: T, y: T, h: PropertyHelper) =>
    h.assert_eq[T](x * y, y * x)

class \nodoc\ iso _SuccessfulIntPairPropertyTest is UnitTest
  fun name(): String => "property/intpair"

  fun apply(h: TestHelper) =>
    let property = recover iso _SuccessfulIntPairProperty end
    let property_notify = _UnitTestPropertyNotify(h, true)
    let property_logger = _UnitTestPropertyLogger(h)
    let params = property.params()
    h.long_test(params.timeout)
    let runner = PropertyRunner[IntPairPropertySample](
      consume property,
      params,
      property_notify,
      property_logger,
      h.env)
    runner.run()

class \nodoc\ iso _InfiniteShrinkProperty is Property1[String]
  fun name(): String => "property_runner/inifinite_shrink/property"

  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(r: Randomness): String^ =>
          "decided by fair dice roll, totally random"

        fun shrink(t: String): ValueAndShrink[String] =>
          (t, Iter[String^].repeat_value(t))
      end)

  fun ref property(arg1: String, ph: PropertyHelper) =>
    ph.assert_true(arg1.size() >  100) // assume this failing


class \nodoc\ iso _RunnerInfiniteShrinkTest is UnitTest
  """
  ensure that having a failing property with an infinite generator
  is not shrinking infinitely
  """
  fun name(): String => "property_runner/infinite_shrink"

  fun apply(h: TestHelper) =>

    let property = recover iso _InfiniteShrinkProperty end
    let params = property.params()

    h.long_test(params.timeout)

    let runner = PropertyRunner[String](
      consume property,
      params,
      _UnitTestPropertyNotify(h, false),
      _UnitTestPropertyLogger(h),
      h.env)
    runner.run()

class \nodoc\ iso _ErroringGeneratorProperty is Property1[String]
  fun name(): String => "property_runner/erroring_generator/property"

  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(r: Randomness): String^ ? =>
          error
      end)

  fun ref property(sample: String, h: PropertyHelper) =>
    None

class \nodoc\ iso _RunnerErroringGeneratorTest is UnitTest
  fun name(): String => "property_runner/erroring_generator"

  fun apply(h: TestHelper) =>
    let property = recover iso _ErroringGeneratorProperty end
    let params = property.params()

    h.long_test(params.timeout)

    let runner = PropertyRunner[String](
      consume property,
      params,
      _UnitTestPropertyNotify(h, false),
      _UnitTestPropertyLogger(h),
      h.env)
    runner.run()

class \nodoc\ iso _SometimesErroringGeneratorProperty is Property1[String]
  fun name(): String => "property_runner/sometimes_erroring_generator"
  fun params(): PropertyParams =>
    PropertyParams(where
      num_samples' = 3,
      seed' = 6, // known seed to produce a value, an error and a value
      max_generator_retries' = 1
    )
  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(r: Randomness): String^ ? =>
          match (r.u64() % 2)
          | 0 => "foo"
          else
            error
          end
      end
    )

  fun ref property(sample: String, h: PropertyHelper) =>
    None


class \nodoc\ iso _RunnerSometimesErroringGeneratorTest is UnitTest
  fun name(): String => "property_runner/sometimes_erroring_generator"

  fun apply(h: TestHelper) =>
    let property = recover iso _SometimesErroringGeneratorProperty end
    let params = property.params()

    h.long_test(params.timeout)

    let runner = PropertyRunner[String](
      consume property,
      params,
      _UnitTestPropertyNotify(h, true),
      _UnitTestPropertyLogger(h),
      h.env)
    runner.run()

class \nodoc\ iso _ReportFailedSampleProperty is Property1[U8]
  fun name(): String => "property_runner/sample_reporting/property"
  fun gen(): Generator[U8] => Generators.u8(0, 1)
  fun ref property(sample: U8, h: PropertyHelper) =>
    h.assert_eq[U8](sample, U8(0))

class \nodoc\ iso _RunnerReportFailedSampleTest is UnitTest
  fun name(): String => "property_runner/sample_reporting"
  fun apply(h: TestHelper) =>
    let property = recover iso _ReportFailedSampleProperty end
    let params = property.params()

    h.long_test(params.timeout)

    let logger =
      object val is PropertyLogger
        fun log(msg: String, verbose: Bool) =>
          if msg.contains("Property failed for sample 1 ") then
            h.complete(true)
          elseif msg.contains("Propety failed for sample 0 ") then
            h.fail("wrong sample reported.")
            h.complete(false)
          end
      end
    let notify =
      object val is PropertyResultNotify
        fun fail(msg: String) =>
          h.log("FAIL: " + msg)
        fun complete(success: Bool) =>
          h.assert_false(success, "property did not fail")
      end

    let runner = PropertyRunner[U8](
      consume property,
      params,
      _UnitTestPropertyNotify(h, false),
      logger,
      h.env)
    runner.run()

trait \nodoc\ _ShrinkTest is UnitTest
  fun shrink[T](gen: Generator[T], shrink_elem: T): Iterator[T^] =>
    (_, let shrinks': Iterator[T^]) = gen.shrink(consume shrink_elem)
    shrinks'

  fun _collect_shrinks[T](gen: Generator[T], shrink_elem: T): Array[T] =>
    Iter[T^](shrink[T](gen, consume shrink_elem)).collect[Array[T]](Array[T])

  fun _size(shrinks: Iterator[Any^]): USize =>
    Iter[Any^](shrinks).count()

  fun _test_int_constraints[T: (Int & Integer[T] val)](
    h: TestHelper,
    gen: Generator[T],
    x: T,
    min: T = T.min_value()
  ) ?
    =>
    let shrinks = shrink[T](gen, min)
    h.assert_false(shrinks.has_next(), "non-empty shrinks for minimal value " + min.string())

    let shrinks1 = _collect_shrinks[T](gen, min + 1)
    h.assert_array_eq[T]([min], shrinks1, "didn't include min in shrunken list of samples")

    let shrinks2 = shrink[T](gen, x)
    h.assert_true(
      Iter[T^](shrinks2)
        .all(
          {(u: T): Bool =>
            match \exhaustive\ x.compare(min)
            | Less =>
              (u <= min) and (u > x)
            | Equal => true
            | Greater =>
              (u >= min) and (u < x)
            end
          }),
      "generated shrinks from " + x.string() + " that violate minimum or maximum")

    let count_shrinks = shrink[T](gen, x)
    let max_count =
      if (x - min) < 0 then
        -(x - min)
      else
        x - min
      end
    let actual_count = T.from[USize](Iter[T^](count_shrinks).count())
    h.assert_true(
      actual_count <= max_count,
      "generated too much values from " + x.string() + " : " + actual_count.string() + " > " + max_count.string())

class \nodoc\ iso _UnsignedShrinkTest is _ShrinkTest
  fun name(): String => "shrink/unsigned_generators"

  fun apply(h: TestHelper)? =>
    let gen = Generators.u8()
    _test_int_constraints[U8](h, gen, U8(42))?
    _test_int_constraints[U8](h, gen, U8.max_value())?

    let min = U64(10)
    let gen_min = Generators.u64(where min=min)
    _test_int_constraints[U64](h, gen_min, 42, min)?

class \nodoc\ iso _SignedShrinkTest is _ShrinkTest
  fun name(): String => "shrink/signed_generators"

  fun apply(h: TestHelper) ? =>
    let gen = Generators.i64()
    _test_int_constraints[I64](h, gen, (I64.min_value() + 100))?

    let gen2 = Generators.i64(-10, 20)
    _test_int_constraints[I64](h, gen2, 20, -10)?
    _test_int_constraints[I64](h, gen2, 30, -10)?
    _test_int_constraints[I64](h, gen2, -12, -10)? // weird case but should still work


class \nodoc\ iso _ASCIIStringShrinkTest is _ShrinkTest
  fun name(): String => "shrink/ascii_string_generators"

  fun apply(h: TestHelper) =>
    let gen = Generators.ascii(where min=0)

    let shrinks_min = shrink[String](gen, "")
    h.assert_false(shrinks_min.has_next(), "non-empty shrinks for minimal value")

    let sample = "ABCDEF"
    let shrinks = _collect_shrinks[String](gen, sample)
    h.assert_array_eq[String](
      ["ABCDE"; "ABCD"; "ABC"; "AB"; "A"; ""],
      shrinks)

    let short_sample = "A"
    let short_shrinks = _collect_shrinks[String](gen, short_sample)
    h.assert_array_eq[String]([""], short_shrinks, "shrinking 'A' returns wrong results")

class \nodoc\ iso _MinASCIIStringShrinkTest is _ShrinkTest
  fun name(): String => "shrink/min_ascii_string_generators"

  fun apply(h: TestHelper) =>
    let min: USize = 10
    let gen = Generators.ascii(where min=min)

    let shrinks_min = shrink[String](gen, "abcdefghi")
    h.assert_false(shrinks_min.has_next(), "generated non-empty shrinks for string smaller than minimum")

    let shrinks = shrink[String](gen, "abcdefghijlkmnop")
    h.assert_true(
      Iter[String](shrinks)
        .all({(s: String): Bool => s.size() >= min}), "generated shrinks that violate minimum string length")

class \nodoc\ iso _UnicodeStringShrinkTest is _ShrinkTest
  fun name(): String => "shrink/unicode_string_generators"

  fun apply(h: TestHelper) =>
    let gen = Generators.unicode()

    let shrinks_min = shrink[String](gen, "")
    h.assert_false(shrinks_min.has_next(), "non-empty shrinks for minimal value")

    let sample2 = "ΣΦΩ"
    let shrinks2 = _collect_shrinks[String](gen, sample2)
    h.assert_false(shrinks2.contains(sample2))
    h.assert_true(shrinks2.size() > 0, "empty shrinks for non-minimal unicode string")

    let sample3 = "Σ"
    let shrinks3 = _collect_shrinks[String](gen, sample3)
    h.assert_array_eq[String]([""], shrinks3, "minimal non-empty string not properly shrunk")

class \nodoc\ iso _MinUnicodeStringShrinkTest is _ShrinkTest
  fun name(): String => "shrink/min_unicode_string_generators"

  fun apply(h: TestHelper) =>
    let min = USize(5)
    let gen = Generators.unicode(where min=min)

    let min_sample = "ΣΦΩ"
    let shrinks_min = shrink[String](gen, min_sample)
    h.assert_false(shrinks_min.has_next(), "non-empty shrinks for minimal value")

    let sample = "ΣΦΩΣΦΩ"
    let shrinks = _collect_shrinks[String](gen, sample)
    h.assert_true(
      Iter[String](shrinks.values())
        .all({(s: String): Bool => s.codepoints() >= min}),
      "generated shrinks that violate minimum string length")
    h.assert_false(
      shrinks.contains(sample),
      "shrinks contain sample value")

class \nodoc\ iso _FilterMapShrinkTest is _ShrinkTest
  fun name(): String => "shrink/filter_map"

  fun apply(h: TestHelper) =>
    let gen: Generator[U64] =
      Generators.u8()
        .filter({(byte) => (byte, byte > 10) })
        .map[U64]({(byte) => (byte * 2).u64() })
    // shrink from 100 and only expect even values > 20
    let shrink_iter = shrink[U64](gen, U64(100))
    h.assert_true(
      Iter[U64](shrink_iter)
        .all({(u) =>
          (u > 20) and ((u % 2) == 0) }),
      "shrinking does not maintain filter invariants")

primitive \nodoc\ _Async
  """
  utility to run tests for async properties
  """
  fun run_async_test(
    h: TestHelper,
    action: {(PropertyHelper): None} val,
    should_succeed: Bool = true)
  =>
    """
    Run the given action in an asynchronous property
    providing if you expect success or failure with `should_succeed`.
    """
    let property = _AsyncProperty(action)
    let params = property.params()
    h.long_test(params.timeout)

    let runner = PropertyRunner[String](
      consume property,
      params,
      _UnitTestPropertyNotify(h, should_succeed),
      _UnitTestPropertyLogger(h),
      h.env)
    runner.run()

class \nodoc\ val _UnitTestPropertyLogger is PropertyLogger
  """
  just forwarding logs to the TestHelper log
  with a custom prefix
  """
  let _th: TestHelper

  new val create(th: TestHelper) =>
    _th = th

  fun log(msg: String, verbose: Bool) =>
    _th.log("[PROPERTY] " + msg, verbose)

class \nodoc\ val _UnitTestPropertyNotify is PropertyResultNotify
  let _th: TestHelper
  let _should_succeed: Bool

  new val create(th: TestHelper, should_succeed: Bool = true) =>
    _should_succeed = should_succeed
    _th = th

  fun fail(msg: String) =>
    _th.log("FAIL: " + msg)

  fun complete(success: Bool) =>
    _th.log("COMPLETE: " + success.string())
    let result = (success and _should_succeed) or ((not success) and (not _should_succeed))
    _th.complete(result)


actor \nodoc\ _AsyncDelayingActor
  """
  running the given action in a behavior
  """

  let _ph: PropertyHelper
  let _action: {(PropertyHelper): None} val

  new create(ph: PropertyHelper, action: {(PropertyHelper): None} val) =>
    _ph = ph
    _action = action

  be do_it() =>
    _action.apply(_ph)

class \nodoc\ iso _AsyncProperty is Property1[String]
  """
  A simple property running the given action
  asynchronously in an `AsyncDelayingActor`.
  """

  let _action: {(PropertyHelper): None} val
  new iso create(action: {(PropertyHelper): None } val) =>
    _action = action

  fun name(): String => "property_runner/async/property"

  fun params(): PropertyParams =>
    PropertyParams(where async' = true)

  fun gen(): Generator[String] =>
    Generators.ascii_printable()

  fun ref property(arg1: String, ph: PropertyHelper) =>
    _AsyncDelayingActor(ph, _action).do_it()

interface \nodoc\ val _RandomCase[A: Comparable[A] #read]
  new val create()

  fun test(min: A, max: A): A

  fun generator(): Generator[A]

primitive \nodoc\ _RandomCaseU8 is _RandomCase[U8]
  fun test(min: U8, max: U8): U8 =>
    let rnd = Randomness(Time.millis())
    rnd.u8(min, max)

  fun generator(): Generator[U8] =>
    Generators.u8()

primitive \nodoc\ _RandomCaseU16 is _RandomCase[U16]
  fun test(min: U16, max: U16): U16 =>
    let rnd = Randomness(Time.millis())
    rnd.u16(min, max)

  fun generator(): Generator[U16] =>
    Generators.u16()

primitive \nodoc\ _RandomCaseU32 is _RandomCase[U32]
  fun test(min: U32, max: U32): U32 =>
    let rnd = Randomness(Time.millis())
    rnd.u32(min, max)

  fun generator(): Generator[U32] =>
    Generators.u32()

primitive \nodoc\ _RandomCaseU64 is _RandomCase[U64]
  fun test(min: U64, max: U64): U64 =>
    let rnd = Randomness(Time.millis())
    rnd.u64(min, max)

  fun generator(): Generator[U64] =>
    Generators.u64()

primitive \nodoc\ _RandomCaseU128 is _RandomCase[U128]
  fun test(min: U128, max: U128): U128 =>
    let rnd = Randomness(Time.millis())
    rnd.u128(min, max)

  fun generator(): Generator[U128] =>
    Generators.u128()

primitive \nodoc\ _RandomCaseI8 is _RandomCase[I8]
  fun test(min: I8, max: I8): I8 =>
    let rnd = Randomness(Time.millis())
    rnd.i8(min, max)

  fun generator(): Generator[I8] =>
    Generators.i8()

primitive \nodoc\ _RandomCaseI16 is _RandomCase[I16]
  fun test(min: I16, max: I16): I16 =>
    let rnd = Randomness(Time.millis())
    rnd.i16(min, max)

  fun generator(): Generator[I16] =>
    Generators.i16()

primitive \nodoc\ _RandomCaseI32 is _RandomCase[I32]
  fun test(min: I32, max: I32): I32 =>
    let rnd = Randomness(Time.millis())
    rnd.i32(min, max)

  fun generator(): Generator[I32] =>
    Generators.i32()

primitive \nodoc\ _RandomCaseI64 is _RandomCase[I64]
  fun test(min: I64, max: I64): I64 =>
    let rnd = Randomness(Time.millis())
    rnd.i64(min, max)

  fun generator(): Generator[I64] =>
    Generators.i64()

primitive \nodoc\ _RandomCaseI128 is _RandomCase[I128]
  fun test(min: I128, max: I128): I128 =>
    let rnd = Randomness(Time.millis())
    rnd.i128(min, max)

  fun generator(): Generator[I128] =>
    Generators.i128()

primitive \nodoc\ _RandomCaseISize is _RandomCase[ISize]
  fun test(min: ISize, max: ISize): ISize =>
    let rnd = Randomness(Time.millis())
    rnd.isize(min, max)

  fun generator(): Generator[ISize] =>
    Generators.isize()

primitive \nodoc\ _RandomCaseILong is _RandomCase[ILong]
  fun test(min: ILong, max: ILong): ILong =>
    let rnd = Randomness(Time.millis())
    rnd.ilong(min, max)

  fun generator(): Generator[ILong] =>
    Generators.ilong()

class \nodoc\ iso _RandomnessProperty[A: Comparable[A] #read, R: _RandomCase[A] val] is Property1[(A, A)]
  """
  Ensure Randomness generates a random number within the given range.
  """
  let _type_name: String

  new iso create(type_name: String) =>
    _type_name = type_name

  fun name(): String => "randomness/" + _type_name

  fun gen(): Generator[(A, A)] =>
    let min = R.generator()
    let max = R.generator()
    Generators.zip2[A, A](min, max)
      .filter(
        { (pair) => (pair, (pair._1 <= pair._2)) }
      )

  fun property(arg1: (A, A), ph: PropertyHelper) =>
    (let min, let max) = arg1

    let value = R.test(min, max)
    ph.assert_true(value >= min)
    ph.assert_true(value <= max)

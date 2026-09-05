use "pony_test"

use "collections"
use persistent = "collections/persistent"
use "random"
use "time"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)

  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_ASCIIRangeTest)
    test(_ErroringPropertyTest)
    test(_FailingPropertyTest)
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
    test(_MapIsOfEmptyTest)
    test(_MapIsOfIdentityTest)
    test(_MapIsOfMaxTest)
    test(_MapIsOfMinTest)
    test(_MapOfEmptyTest)
    test(_MapOfIdentityTest)
    test(_MapOfMaxTest)
    test(_MapOfMinTest)
    test(_MultipleForAllTest)
    test(_PersistentListOfEmptyTest)
    test(_PersistentListOfMaxTest)
    test(_PersistentListOfMinTest)
    test(_PersistentMapIsOfEmptyTest)
    test(_PersistentMapIsOfIdentityTest)
    test(_PersistentMapIsOfMaxTest)
    test(_PersistentMapIsOfMinTest)
    test(_PersistentMapOfEmptyTest)
    test(_PersistentMapOfIdentityTest)
    test(_PersistentMapOfMaxTest)
    test(_PersistentMapOfMinTest)
    test(_PersistentSetIsOfEmptyTest)
    test(_PersistentSetIsOfIdentityTest)
    test(_PersistentSetIsOfMaxTest)
    test(_PersistentSetIsOfMinTest)
    test(_PersistentSetOfEmptyTest)
    test(_PersistentSetOfMaxTest)
    test(_PersistentSetOfMinTest)
    test(Property1UnitTest[(I8, I8)](
      _RandomnessProperty[I8, _RandomCaseI8]("I8")))
    test(Property1UnitTest[(I16, I16)](
      _RandomnessProperty[I16, _RandomCaseI16]("I16")))
    test(Property1UnitTest[(I32, I32)](
      _RandomnessProperty[I32, _RandomCaseI32]("I32")))
    test(Property1UnitTest[(I64, I64)](
      _RandomnessProperty[I64, _RandomCaseI64]("I64")))
    test(Property1UnitTest[(I128, I128)](
      _RandomnessProperty[I128, _RandomCaseI128]("I128")))
    test(Property1UnitTest[(ILong, ILong)](
      _RandomnessProperty[ILong, _RandomCaseILong]("ILong")))
    test(Property1UnitTest[(ISize, ISize)](
      _RandomnessProperty[ISize, _RandomCaseISize]("ISize")))
    test(Property1UnitTest[(U8, U8)](
      _RandomnessProperty[U8, _RandomCaseU8]("U8")))
    test(Property1UnitTest[(U16, U16)](
      _RandomnessProperty[U16, _RandomCaseU16]("U16")))
    test(Property1UnitTest[(U32, U32)](
      _RandomnessProperty[U32, _RandomCaseU32]("U32")))
    test(Property1UnitTest[(U64, U64)](
      _RandomnessProperty[U64, _RandomCaseU64]("U64")))
    test(Property1UnitTest[(U128, U128)](
      _RandomnessProperty[U128, _RandomCaseU128]("U128")))
    test(_RunnerAsyncCompleteActionTest)
    test(_RunnerAsyncCompleteMultiActionTest)
    test(_RunnerAsyncCompleteMultiSucceedActionTest)
    test(_RunnerAsyncFailTest)
    test(_RunnerAsyncMultiCompleteFailTest)
    test(_RunnerAsyncMultiCompleteSucceedTest)
    test(_RunnerAsyncPropertyCompleteFalseTest)
    test(_RunnerAsyncPropertyCompleteTest)
    test(_RunnerErroringGeneratorTest)
    test(_RunnerReportFailedSampleTest)
    test(_ShrinkIntToMinTest)
    test(_ShrinkIntAboveThresholdTest)
    test(_ShrinkArrayToMinTest)
    test(_RunnerSometimesErroringGeneratorTest)
    test(_SeqOfTest)
    test(_SetIsOfIdentityTest)
    test(_SetIsOfMinTest)
    test(_SetOfEmptyTest)
    test(_SetOfMaxTest)
    test(_SetOfMinTest)
    test(_SetOfTest)
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
    test(_VecOfEmptyTest)
    test(_VecOfFromToReversedTest)
    test(_VecOfMaxTest)
    test(_VecOfMinTest)
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
    let runner =
      PropertyRunner[U8](
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
    let runner =
      PropertyRunner[U8](
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
    let runner =
      PropertyRunner[U8](
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
    let runner =
      PropertyRunner[(U8, U8)](
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
    let runner =
      PropertyRunner[(U8, U8, U8)](
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

  fun ref property4(
    arg1: U8, arg2: U8, arg3: U8, arg4: U8, h: PropertyHelper)
  =>
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
    let runner =
      PropertyRunner[(U8, U8, U8, U8)](
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
      let g1 = gen.generate(rnd1)?
      let g2 = gen.generate(rnd2)?
      let g3 = gen.generate(rnd3)?
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
    let gen =
      Generators.u32().filter({
        (u: U32^): (U32^, Bool) =>
          (u, (u % 2) == 0)
      })
    let rnd = Randomness(Time.millis())
    for x in Range(0, 100) do
      let v = gen.generate(rnd)?
      h.assert_true((v % 2) == 0)
    end

class \nodoc\ iso _GenUnionTest is UnitTest
  fun name(): String => "Gen/union"

  fun apply(h: TestHelper) ? =>
    """
    assert that a unioned Generator produces values of both types.
    """
    let gen = Generators.ascii().union[U8](Generators.u8())
    let rnd = Randomness(Time.millis())
    var got_string: Bool = false
    var got_u8: Bool = false
    for x in Range(0, 100) do
      match \exhaustive\ gen.generate(rnd)?
      | let vs: String => got_string = true
      | let vs: U8 => got_u8 = true
      end
    end
    h.assert_true(got_string, "union never generated String")
    h.assert_true(got_u8, "union never generated U8")

class \nodoc\ iso _GenFrequencyTest is UnitTest
  fun name(): String => "Gen/frequency"

  fun apply(h: TestHelper) ? =>
    """
    ensure that frequency generates values with given weights
    and does not generate values with weight of 0
    """
    let gen =
      Generators.frequency[U8](
        [ as WeightedGenerator[U8]:
          (1, Generators.unit[U8](0))
          (0, Generators.unit[U8](42))
          (2, Generators.unit[U8](1))
        ])
    let rnd: Randomness ref = Randomness(Time.millis())

    let generated = Array[U8](100)
    for i in Range(0, 100) do
      generated.push(gen.generate(rnd)?)
    end
    h.assert_false(
      generated.contains(U8(42)),
      "frequency generated value with 0 weight")
    h.assert_true(
      generated.contains(U8(0)),
      "frequency did not generate value with weight of 1")
    h.assert_true(
      generated.contains(U8(1)),
      "frequency did not generate value with weight of 2")

    let empty_gen = Generators.frequency[U8](Array[WeightedGenerator[U8]](0))

    h.assert_error({() ? =>
      empty_gen.generate(Randomness(Time.millis()))?
    })

class \nodoc\ iso _GenFrequencySafeTest is UnitTest
  fun name(): String => "Gen/frequency_safe"

  fun apply(h: TestHelper) =>
    h.assert_error({() ? =>
      Generators.frequency_safe[U8](Array[WeightedGenerator[U8]](0))?
    })

class \nodoc\ iso _GenOneOfTest is UnitTest
  fun name(): String => "Gen/one_of"

  fun apply(h: TestHelper) ? =>
    let gen = Generators.one_of[U8]([as U8: 0; 1])
    let rnd = Randomness(Time.millis())
    for x in Range(0, 100) do
      let v = gen.generate(rnd)?
      h.assert_true(
        (v == 0) or (v == 1),
        "one_of generator generated illegal value")
    end
    let empty_gen = Generators.one_of[U8](Array[U8](0))

    h.assert_error({() ? =>
      empty_gen.generate(Randomness(Time.millis()))?
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
    for i in Range(0, 100) do
      let sample = seq_gen.generate(rnd)?
      h.assert_true(
        (sample.size() >= 0) and (sample.size() <= 10),
        "Seqs generated with Generators.seq_of are out of bounds")
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
        Generators.u8()
        where to = 1024)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample: Set[U8] = set_gen.generate(rnd)?
      h.assert_true(sample.size() <= 256, "something about U8 is not right")
    end

class \nodoc\ iso _SetOfMaxTest is UnitTest
  fun name(): String => "Gen/set_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, U8.max_value().usize()) do
      let set_gen =
        Generators.set_of[U8](
          Generators.u8()
          where to = size)
      let sample: Set[U8] = set_gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated set is too big.")
    end

class \nodoc\ iso _SetOfEmptyTest is UnitTest
  fun name(): String => "Gen/set_of_empty"

  fun apply(h: TestHelper) ? =>
    let set_gen =
      Generators.set_of[U8](
        Generators.u8()
        where to = 0)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample: Set[U8] = set_gen.generate(rnd)?
      h.assert_true(sample.size() == 0, "non-empty set created.")
    end

class \nodoc\ iso _SetIsOfIdentityTest is UnitTest
  fun name(): String => "Gen/set_is_of_identity"

  fun apply(h: TestHelper) ? =>
    let set_is_gen_same =
      Generators.set_is_of[String](
        Generators.unit[String]("the highlander")
        where to = 100)
    let rnd = Randomness(Time.millis())
    let sample: SetIs[String] = set_is_gen_same.generate(rnd)?
    h.assert_true(
      sample.size() <= 1,
      "invalid SetIs instances generated: size " + sample.size().string())

class \nodoc\ iso _MapOfEmptyTest is UnitTest
  fun name(): String => "Gen/map_of_empty"

  fun apply(h: TestHelper) ? =>
    let map_gen =
      Generators.map_of[String, I64](
        Generators.zip2[String, I64](
          Generators.u8().map[String](
            {(u: U8): String^ =>
              let s = u.string()
              consume s
            }),
          Generators.i64(-10, 10))
        where to = 0)
    let rnd = Randomness(Time.millis())
    let sample = map_gen.generate(rnd)?
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
            Generators.i64(-10, 10))
          where to = size)
      let sample = map_gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated map is too big.")
    end

class \nodoc\ iso _MapOfIdentityTest is UnitTest
  fun name(): String => "Gen/map_of_identity"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let map_gen =
      Generators.map_of[String, I64](
        Generators.zip2[String, I64](
          Generators.repeatedly[String](
            {(): String^ =>
              let s = recover String.create(14) end
              s.add("the highlander")
              consume s
            }),
          Generators.i64(-10, 10))
        where to = 100)
    let sample = map_gen.generate(rnd)?
    h.assert_true(sample.size() <= 1)

class \nodoc\ iso _MapIsOfEmptyTest is UnitTest
  fun name(): String => "Gen/map_is_of_empty"

  fun apply(h: TestHelper) ? =>
    let map_is_gen =
      Generators.map_is_of[String, I64](
        Generators.zip2[String, I64](
          Generators.u8().map[String](
            {(u: U8): String^ =>
              let s = u.string()
              consume s
            }),
          Generators.i64(-10, 10))
        where to = 0)
    let rnd = Randomness(Time.millis())
    let sample = map_is_gen.generate(rnd)?
    h.assert_eq[USize](sample.size(), 0, "non-empty map created")

class \nodoc\ iso _MapIsOfMaxTest is UnitTest
  fun name(): String => "Gen/map_is_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())

    for size in Range(1, U8.max_value().usize()) do
      let map_is_gen =
        Generators.map_is_of[String, I64](
          Generators.zip2[String, I64](
            Generators.u16().map[String](
              {(u: U16): String^ =>
                let s = u.string()
                consume s
              }),
            Generators.i64(-10, 10))
          where to = size)
      let sample = map_is_gen.generate(rnd)?
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
          Generators.i64(-10, 10))
        where to = 100)
    let sample = map_gen.generate(rnd)?
    h.assert_true(sample.size() <= 1)

class \nodoc\ iso _SetOfMinTest is UnitTest
  fun name(): String => "Gen/set_of_min"

  fun apply(h: TestHelper) ? =>
    let set_gen =
      Generators.set_of[U8](
        Generators.u8()
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample: Set[U8] = set_gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty set created with min = 1")
    end

class \nodoc\ iso _SetIsOfMinTest is UnitTest
  fun name(): String => "Gen/set_is_of_min"

  fun apply(h: TestHelper) ? =>
    let set_is_gen =
      Generators.set_is_of[String](
        Generators.ascii(where from = 1, to = 10)
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample: SetIs[String] = set_is_gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty SetIs created with min = 1")
    end

class \nodoc\ iso _MapOfMinTest is UnitTest
  fun name(): String => "Gen/map_of_min"

  fun apply(h: TestHelper) ? =>
    let map_gen =
      Generators.map_of[String, I64](
        Generators.zip2[String, I64](
          Generators.u16().map[String^]({(u: U16): String^ =>
            u.string()
          }),
          Generators.i64(-10, 10))
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = map_gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty map created with min = 1")
    end

class \nodoc\ iso _MapIsOfMinTest is UnitTest
  fun name(): String => "Gen/map_is_of_min"

  fun apply(h: TestHelper) ? =>
    let map_is_gen =
      Generators.map_is_of[String, I64](
        Generators.zip2[String, I64](
          Generators.u16().map[String^]({(u: U16): String^ =>
            u.string()
          }),
          Generators.i64(-10, 10))
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = map_is_gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty MapIs created with min = 1")
    end

class \nodoc\ iso _ASCIIRangeTest is UnitTest
  fun name(): String => "Gen/ascii_range"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let ascii_gen =
      Generators.ascii(where from = 1, to = 1, range = ASCIIAll)

    for i in Range[USize](0, 100) do
      let sample = ascii_gen.generate(rnd)?
      h.assert_true(
        ASCIIAll().contains(sample),
        "\"" + sample + "\" not valid ascii")
    end

class \nodoc\ iso _UTF32CodePointStringTest is UnitTest
  fun name(): String => "Gen/utf32_codepoint_string"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let string_gen =
      Generators.utf32_codepoint_string(
        Generators.u32(),
        50,
        100)

    for i in Range[USize](0, 100) do
      let sample = string_gen.generate(rnd)?
      for cp in sample.runes() do
        h.assert_true(
          (cp <= 0xD7FF) or (cp >= 0xE000),
          "\"" + sample + "\" invalid utf32")
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
    let runner =
      PropertyRunner[IntPropertySample](
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
    let runner =
      PropertyRunner[IntPairPropertySample](
        consume property,
        params,
        property_notify,
        property_logger,
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

    let runner =
      PropertyRunner[String](
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
      seed' = 6,
      max_generator_retries' = 1
    )

  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(r: Randomness): String^ ? =>
          match (r.u64()? % 2)
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

    let runner =
      PropertyRunner[String](
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

    let runner =
      PropertyRunner[U8](
        consume property,
        params,
        _UnitTestPropertyNotify(h, false),
        logger,
        h.env)
    runner.run()

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

    let runner =
      PropertyRunner[String](
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
    let result =
      (success and _should_succeed) or
      ((not success) and (not _should_succeed))
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

  new iso create(action: {(PropertyHelper): None} val) =>
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

  fun test(min: A, max: A): A ?

  fun generator(): Generator[A]

primitive \nodoc\ _RandomCaseU8 is _RandomCase[U8]
  fun test(min: U8, max: U8): U8 ? =>
    let rnd = Randomness(Time.millis())
    rnd.u8(min, max)?

  fun generator(): Generator[U8] =>
    Generators.u8()

primitive \nodoc\ _RandomCaseU16 is _RandomCase[U16]
  fun test(min: U16, max: U16): U16 ? =>
    let rnd = Randomness(Time.millis())
    rnd.u16(min, max)?

  fun generator(): Generator[U16] =>
    Generators.u16()

primitive \nodoc\ _RandomCaseU32 is _RandomCase[U32]
  fun test(min: U32, max: U32): U32 ? =>
    let rnd = Randomness(Time.millis())
    rnd.u32(min, max)?

  fun generator(): Generator[U32] =>
    Generators.u32()

primitive \nodoc\ _RandomCaseU64 is _RandomCase[U64]
  fun test(min: U64, max: U64): U64 ? =>
    let rnd = Randomness(Time.millis())
    rnd.u64(min, max)?

  fun generator(): Generator[U64] =>
    Generators.u64()

primitive \nodoc\ _RandomCaseU128 is _RandomCase[U128]
  fun test(min: U128, max: U128): U128 ? =>
    let rnd = Randomness(Time.millis())
    rnd.u128(min, max)?

  fun generator(): Generator[U128] =>
    Generators.u128()

primitive \nodoc\ _RandomCaseI8 is _RandomCase[I8]
  fun test(min: I8, max: I8): I8 ? =>
    let rnd = Randomness(Time.millis())
    rnd.i8(min, max)?

  fun generator(): Generator[I8] =>
    Generators.i8()

primitive \nodoc\ _RandomCaseI16 is _RandomCase[I16]
  fun test(min: I16, max: I16): I16 ? =>
    let rnd = Randomness(Time.millis())
    rnd.i16(min, max)?

  fun generator(): Generator[I16] =>
    Generators.i16()

primitive \nodoc\ _RandomCaseI32 is _RandomCase[I32]
  fun test(min: I32, max: I32): I32 ? =>
    let rnd = Randomness(Time.millis())
    rnd.i32(min, max)?

  fun generator(): Generator[I32] =>
    Generators.i32()

primitive \nodoc\ _RandomCaseI64 is _RandomCase[I64]
  fun test(min: I64, max: I64): I64 ? =>
    let rnd = Randomness(Time.millis())
    rnd.i64(min, max)?

  fun generator(): Generator[I64] =>
    Generators.i64()

primitive \nodoc\ _RandomCaseI128 is _RandomCase[I128]
  fun test(min: I128, max: I128): I128 ? =>
    let rnd = Randomness(Time.millis())
    rnd.i128(min, max)?

  fun generator(): Generator[I128] =>
    Generators.i128()

primitive \nodoc\ _RandomCaseISize is _RandomCase[ISize]
  fun test(min: ISize, max: ISize): ISize ? =>
    let rnd = Randomness(Time.millis())
    rnd.isize(min, max)?

  fun generator(): Generator[ISize] =>
    Generators.isize()

primitive \nodoc\ _RandomCaseILong is _RandomCase[ILong]
  fun test(min: ILong, max: ILong): ILong ? =>
    let rnd = Randomness(Time.millis())
    rnd.ilong(min, max)?

  fun generator(): Generator[ILong] =>
    Generators.ilong()

class \nodoc\ iso _RandomnessProperty[
  A: Comparable[A] #read, R: _RandomCase[A] val]
  is Property1[(A, A)]
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
        {(pair) => (pair, (pair._1 <= pair._2)) }
      )

  fun property(arg1: (A, A), ph: PropertyHelper) ? =>
    (let min, let max) = arg1

    let value = R.test(min, max)?
    ph.assert_true(value >= min)
    ph.assert_true(value <= max)

class \nodoc\ iso _VecOfEmptyTest is UnitTest
  fun name(): String => "Gen/vec_of_empty"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.vec_of[U8](
        Generators.u8()
        where to = 0)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_eq[USize](sample.size(), 0, "non-empty vec created")
    end

class \nodoc\ iso _VecOfFromToReversedTest is UnitTest
  fun name(): String => "Gen/vec_of_from_to_reversed"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.vec_of[U8](
        Generators.u8()
        where from = 10, to = 5)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 5,
        "vec smaller than lo when from > to")
      h.assert_true(
        sample.size() <= 10,
        "vec larger than hi when from > to")
    end

class \nodoc\ iso _VecOfMaxTest is UnitTest
  fun name(): String => "Gen/vec_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, 50) do
      let gen =
        Generators.vec_of[U8](
          Generators.u8()
          where to = size)
      let sample = gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated vec is too big")
    end

class \nodoc\ iso _VecOfMinTest is UnitTest
  fun name(): String => "Gen/vec_of_min"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.vec_of[U8](
        Generators.u8()
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty vec created with from = 1")
    end

class \nodoc\ iso _PersistentListOfEmptyTest is UnitTest
  fun name(): String => "Gen/persistent_list_of_empty"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_list_of[U8](
        Generators.u8()
        where to = 0)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_eq[USize](sample.size(), 0, "non-empty list created")
    end

class \nodoc\ iso _PersistentListOfMaxTest is UnitTest
  fun name(): String => "Gen/persistent_list_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, 50) do
      let gen =
        Generators.persistent_list_of[U8](
          Generators.u8()
          where to = size)
      let sample = gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated list is too big")
    end

class \nodoc\ iso _PersistentListOfMinTest is UnitTest
  fun name(): String => "Gen/persistent_list_of_min"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_list_of[U8](
        Generators.u8()
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty list created with from = 1")
    end

class \nodoc\ iso _PersistentSetIsOfIdentityTest is UnitTest
  fun name(): String => "Gen/persistent_set_is_of_identity"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_set_is_of[String](
        Generators.unit[String]("the highlander")
        where to = 100)
    let rnd = Randomness(Time.millis())
    let sample = gen.generate(rnd)?
    h.assert_true(
      sample.size() <= 1,
      "invalid persistent SetIs: size " + sample.size().string())

class \nodoc\ iso _PersistentSetOfEmptyTest is UnitTest
  fun name(): String => "Gen/persistent_set_of_empty"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_set_of[U8](
        Generators.u8()
        where to = 0)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_eq[USize](sample.size(), 0, "non-empty set created")
    end

class \nodoc\ iso _PersistentSetOfMaxTest is UnitTest
  fun name(): String => "Gen/persistent_set_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, U8.max_value().usize()) do
      let gen =
        Generators.persistent_set_of[U8](
          Generators.u8()
          where to = size)
      let sample = gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated set is too big")
    end

class \nodoc\ iso _PersistentSetOfMinTest is UnitTest
  fun name(): String => "Gen/persistent_set_of_min"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_set_of[U8](
        Generators.u8()
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty set created with from = 1")
    end

class \nodoc\ iso _PersistentSetIsOfEmptyTest is UnitTest
  fun name(): String => "Gen/persistent_set_is_of_empty"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_set_is_of[U8](
        Generators.u8()
        where to = 0)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_eq[USize](sample.size(), 0, "non-empty SetIs created")
    end

class \nodoc\ iso _PersistentSetIsOfMaxTest is UnitTest
  fun name(): String => "Gen/persistent_set_is_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, U8.max_value().usize()) do
      let gen =
        Generators.persistent_set_is_of[U8](
          Generators.u8()
          where to = size)
      let sample = gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated SetIs is too big")
    end

class \nodoc\ iso _PersistentSetIsOfMinTest is UnitTest
  fun name(): String => "Gen/persistent_set_is_of_min"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_set_is_of[U8](
        Generators.u8()
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty SetIs created with from = 1")
    end

class \nodoc\ iso _PersistentMapOfIdentityTest is UnitTest
  fun name(): String => "Gen/persistent_map_of_identity"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let gen =
      Generators.persistent_map_of[String, U8](
        Generators.zip2[String, U8](
          Generators.repeatedly[String](
            {(): String^ =>
              let s = recover String.create(14) end
              s.add("the highlander")
              consume s
            }),
          Generators.u8())
        where to = 100)
    let sample = gen.generate(rnd)?
    h.assert_true(sample.size() <= 1)

class \nodoc\ iso _PersistentMapIsOfIdentityTest is UnitTest
  fun name(): String => "Gen/persistent_map_is_of_identity"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let gen =
      Generators.persistent_map_is_of[String, U8](
        Generators.zip2[String, U8](
          Generators.unit[String]("the highlander"),
          Generators.u8())
        where to = 100)
    let sample = gen.generate(rnd)?
    h.assert_true(sample.size() <= 1)

class \nodoc\ iso _PersistentMapOfEmptyTest is UnitTest
  fun name(): String => "Gen/persistent_map_of_empty"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_map_of[U8, U8](
        Generators.zip2[U8, U8](
          Generators.u8(),
          Generators.u8())
        where to = 0)
    let rnd = Randomness(Time.millis())
    let sample = gen.generate(rnd)?
    h.assert_eq[USize](sample.size(), 0, "non-empty map created")

class \nodoc\ iso _PersistentMapOfMaxTest is UnitTest
  fun name(): String => "Gen/persistent_map_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, U8.max_value().usize()) do
      let gen =
        Generators.persistent_map_of[U8, U8](
          Generators.zip2[U8, U8](
            Generators.u8(),
            Generators.u8())
          where to = size)
      let sample = gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated map is too big")
    end

class \nodoc\ iso _PersistentMapOfMinTest is UnitTest
  fun name(): String => "Gen/persistent_map_of_min"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_map_of[U8, U8](
        Generators.zip2[U8, U8](
          Generators.u8(),
          Generators.u8())
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty map created with from = 1")
    end

class \nodoc\ iso _PersistentMapIsOfEmptyTest is UnitTest
  fun name(): String => "Gen/persistent_map_is_of_empty"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_map_is_of[U8, U8](
        Generators.zip2[U8, U8](
          Generators.u8(),
          Generators.u8())
        where to = 0)
    let rnd = Randomness(Time.millis())
    let sample = gen.generate(rnd)?
    h.assert_eq[USize](sample.size(), 0, "non-empty MapIs created")

class \nodoc\ iso _PersistentMapIsOfMaxTest is UnitTest
  fun name(): String => "Gen/persistent_map_is_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, U8.max_value().usize()) do
      let gen =
        Generators.persistent_map_is_of[U8, U8](
          Generators.zip2[U8, U8](
            Generators.u8(),
            Generators.u8())
          where to = size)
      let sample = gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated MapIs is too big")
    end

class \nodoc\ iso _PersistentMapIsOfMinTest is UnitTest
  fun name(): String => "Gen/persistent_map_is_of_min"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_map_is_of[U8, U8](
        Generators.zip2[U8, U8](
          Generators.u8(),
          Generators.u8())
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty MapIs created with from = 1")
    end

// --- Shrinking tests ---
class \nodoc\ iso _ShrinkIntToMinProperty is Property1[U32]
  fun name(): String => "shrink/int_to_min/property"

  fun gen(): Generator[U32] => Generators.u32(0, 1000)

  fun ref property(sample: U32, h: PropertyHelper) =>
    h.assert_eq[U32](sample, U32(0))

class \nodoc\ iso _ShrinkIntToMinTest is UnitTest
  """
  Any non-zero U32 fails the property. The shrinker should reduce it to 1.
  """
  fun name(): String => "shrink/int_to_min"

  fun apply(h: TestHelper) =>
    let property = recover iso _ShrinkIntToMinProperty end
    let params =
      PropertyParams(where num_samples' = 100,
        max_shrink_reductions' = 100)

    h.long_test(params.timeout)

    let notify =
      object val is PropertyResultNotify
        fun fail(msg: String) =>
          if msg.contains("Property failed for sample 1 ") then
            h.complete(true)
          else
            h.fail("expected shrunk sample to be 1, got: " + msg)
            h.complete(false)
          end

        fun complete(success: Bool) =>
          if success then
            h.fail("property should have failed")
            h.complete(false)
          end
      end
    let logger = _UnitTestPropertyLogger(h)

    let runner =
      PropertyRunner[U32](
        consume property,
        params,
        notify,
        logger,
        h.env)
    runner.run()

class \nodoc\ iso _ShrinkIntAboveThresholdProperty is Property1[U32]
  fun name(): String => "shrink/int_above_threshold/property"

  fun gen(): Generator[U32] => Generators.u32(0, 1000)

  fun ref property(sample: U32, h: PropertyHelper) =>
    h.assert_true(sample <= 5)

class \nodoc\ iso _ShrinkIntAboveThresholdTest is UnitTest
  """
  U32 values above 5 fail. The shrinker should reduce to 6.
  """
  fun name(): String => "shrink/int_above_threshold"

  fun apply(h: TestHelper) =>
    let property = recover iso _ShrinkIntAboveThresholdProperty end
    let params =
      PropertyParams(where num_samples' = 100,
        max_shrink_reductions' = 100)

    h.long_test(params.timeout)

    let notify =
      object val is PropertyResultNotify
        fun fail(msg: String) =>
          if msg.contains("Property failed for sample 6 ") then
            h.complete(true)
          else
            h.fail("expected shrunk sample to be 6, got: " + msg)
            h.complete(false)
          end

        fun complete(success: Bool) =>
          if success then
            h.fail("property should have failed")
            h.complete(false)
          end
      end
    let logger = _UnitTestPropertyLogger(h)

    let runner =
      PropertyRunner[U32](
        consume property,
        params,
        notify,
        logger,
        h.env)
    runner.run()

class \nodoc\ iso _ShrinkArrayToMinProperty is Property1[Array[U8]]
  fun name(): String => "shrink/array_to_min/property"

  fun gen(): Generator[Array[U8]] =>
    Generators.array_of[U8](Generators.u8() where from = 1, to = 20)

  fun ref property(sample: Array[U8], h: PropertyHelper) =>
    h.assert_true(sample.size() == 0)

class \nodoc\ iso _ShrinkArrayToMinTest is UnitTest
  """
  Any non-empty array fails. The shrinker should reduce it to a 1-element
  array with the element shrunk toward 0.
  """
  fun name(): String => "shrink/array_to_min"

  fun apply(h: TestHelper) =>
    let property = recover iso _ShrinkArrayToMinProperty end
    let params =
      PropertyParams(where num_samples' = 100,
        max_shrink_reductions' = 100)

    h.long_test(params.timeout)

    let notify =
      object val is PropertyResultNotify
        fun fail(msg: String) =>
          if msg.contains("Property failed for sample [0] ") then
            h.complete(true)
          else
            h.fail("expected shrunk sample to be [0], got: " + msg)
            h.complete(false)
          end

        fun complete(success: Bool) =>
          if success then
            h.fail("property should have failed")
            h.complete(false)
          end
      end
    let logger = _UnitTestPropertyLogger(h)

    let runner =
      PropertyRunner[Array[U8]](
        consume property,
        params,
        notify,
        logger,
        h.env)
    runner.run()

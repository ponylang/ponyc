actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestListPreservesOrder)
    test(_TestShuffleVariesAcrossSeeds)
    test(_TestListShuffleSeedZero)

    // Generator tests
    test(_GenRndTest)
    test(_GenFilterTest)
    test(_GenUnionTest)
    test(_GenFrequencyTest)
    test(_GenFrequencySafeTest)
    test(_GenOneOfTest)
    test(_GenOneOfSafeTest)
    test(_SeqOfTest)
    test(_SetOfTest)
    test(_SetOfMaxTest)
    test(_SetOfEmptyTest)
    test(_SetIsOfIdentityTest)
    test(_MapOfEmptyTest)
    test(_MapOfMaxTest)
    test(_MapOfIdentityTest)
    test(_MapIsOfEmptyTest)
    test(_MapIsOfMaxTest)
    test(_MapIsOfIdentityTest)
    test(_SetOfMinTest)
    test(_SetIsOfMinTest)
    test(_MapOfMinTest)
    test(_MapIsOfMinTest)
    test(_ASCIIRangeTest)
    test(_GenF32Test)
    test(_GenF32ReversedTest)
    test(_GenF32FullRangeTest)
    test(_GenF32NaNTest)
    test(_GenF64Test)
    test(_GenF64ReversedTest)
    test(_GenF64FullRangeTest)
    test(_GenF64NaNTest)
    test(_UTF32CodePointStringTest)
    test(_VecOfEmptyTest)
    test(_VecOfFromToReversedTest)
    test(_VecOfMaxTest)
    test(_VecOfMinTest)

    // Persistent collection generator tests
    test(_PersistentListOfEmptyTest)
    test(_PersistentListOfMaxTest)
    test(_PersistentListOfMinTest)
    test(_PersistentSetIsOfIdentityTest)
    test(_PersistentSetOfEmptyTest)
    test(_PersistentSetOfMaxTest)
    test(_PersistentSetOfMinTest)
    test(_PersistentSetIsOfEmptyTest)
    test(_PersistentSetIsOfMaxTest)
    test(_PersistentSetIsOfMinTest)
    test(_PersistentMapOfIdentityTest)
    test(_PersistentMapIsOfIdentityTest)
    test(_PersistentMapOfEmptyTest)
    test(_PersistentMapOfMaxTest)
    test(_PersistentMapOfMinTest)
    test(_PersistentMapIsOfEmptyTest)
    test(_PersistentMapIsOfMaxTest)
    test(_PersistentMapIsOfMinTest)

    // Stringify test
    test(_StringifyTest)

    // Replay and structure tests
    test(_ReplayDeterminismTest)
    test(_BooleanPerElementStructureTest)

    // Shrinker unit tests
    test(_ShrinkerDeleteSpanTest)
    test(_ShrinkerLowerChoicesTest)
    test(_ShrinkerConvergenceLoopTest)
    test(_ShrinkerRedistributeTest)
    test(_ShrinkerRedistributeU128Test)
    test(_ShrinkerShortenEmptyTest)
    test(_ShrinkerShortenTest)
    test(_ShrinkerSortSpansTest)
    test(_ShrinkerBoolLoweringTest)
    test(_ShrinkerForcedBoolSkipTest)
    test(_ShrinkerLowerU128Test)
    test(_ShrinkerLowerFloatTest)
    test(_ShrinkerDeleteDiscardedSpanTest)
    test(_ShrinkerSortSpansLengthTest)

    // Serializer tests
    test(_SerializerRoundTripAllTypesTest)
    test(_SerializerEmptyArrayTest)
    test(_SerializerBadHeaderTest)
    test(_SerializerBadLineTest)
    test(_SerializerFloatExactBitsTest)
    test(_SerializerNaNRoundTripTest)

    // Regression DB tests
    test(_EncodeNameSafeCharsTest)
    test(_EncodeNameSpecialCharsTest)
    test(_RegressionDbSaveLoadClearTest)
    test(_RegressionDbLoadMissingTest)
    test(_RegressionDbCorruptDeletesTest)
    test(_RegressionDbCreatesDirTest)

    // Property tests (passing)
    test.property(_SuccessfulProperty)
    test.property[(U8, U8)](_SuccessfulProperty2)
    test.property[(U8, U8, U8)](_SuccessfulProperty3)
    test.property[(U8, U8, U8, U8)](_SuccessfulProperty4)
    test.property[IntPropertySample](_SuccessfulIntProperty)
    test.property[IntPairPropertySample](_SuccessfulIntPairProperty)

    // Randomness property tests
    test.property(
      _RandomnessProperty[F32, _RandomCaseF32]("f32"))
    test.property(
      _RandomnessProperty[F64, _RandomCaseF64]("f64"))
    test.property(
      _RandomnessProperty[U8, _RandomCaseU8]("u8"))
    test.property(
      _RandomnessProperty[U16, _RandomCaseU16]("u16"))
    test.property(
      _RandomnessProperty[U32, _RandomCaseU32]("u32"))
    test.property(
      _RandomnessProperty[U64, _RandomCaseU64]("u64"))
    test.property(
      _RandomnessProperty[U128, _RandomCaseU128]("u128"))
    test.property(
      _RandomnessProperty[I8, _RandomCaseI8]("i8"))
    test.property(
      _RandomnessProperty[I16, _RandomCaseI16]("i16"))
    test.property(
      _RandomnessProperty[I32, _RandomCaseI32]("i32"))
    test.property(
      _RandomnessProperty[I64, _RandomCaseI64]("i64"))
    test.property(
      _RandomnessProperty[I128, _RandomCaseI128]("i128"))
    test.property(
      _RandomnessProperty[ISize, _RandomCaseISize]("isize"))
    test.property(
      _RandomnessProperty[ILong, _RandomCaseILong]("ilong"))

    // For-all inline tests
    test(_ForAllTest)
    test(_ForAll2Test)
    test(_ForAll3Test)
    test(_ForAll4Test)
    test(_ClassifyForAllTest)

    // Classification/coverage property tests (passing)
    test.property(_ClassifyAllSameProperty)
    test.property(_ClassifyMultiLabelProperty)
    test.property(_ClassifyAlphabeticalProperty)
    test.property(_CoverSatisfiedProperty)
    test.property(_CoverAndClassifyProperty)
    test.property(_CoverLastWinsProperty)
    test.property(_CollectStringableProperty)
    test.property(_TabulateSingleHeadingProperty)
    test.property(_TabulateMultipleHeadingsProperty)
    test.property(_TabulateAndClassifyProperty)
    test.property(_TabulateSameLabelDiffHeadingsProperty)

    // Health check property tests (passing)
    test.property(_CleanProperty)
    test.property(_DisabledCheckProperty)
    test.property(_AllDisabledCheckProperty)

    // Stateful property tests (passing)
    test(_SuccessfulStatefulPropertyTest)
    test(_StatefulMaxStepsZeroTest)
    test(_MultiCommandStatefulPropertyTest)

    // Multiple for_all test
    test(_MultipleForAllTest)

    // Direct property execution tests
    test(_DirectErroringPropertyTest)
    test(_DirectErroringGeneratorTest)
    test(_DirectSometimesErroringGeneratorTest)
    test(_DirectFailingStatefulTest)
    test(_DirectStepAlwaysRejectsTest)
    test(_DirectStepRetryTest)
    test(_DirectFinalCheckFailureTest)
    test(_DirectStatefulShrinkQualityTest)
    test(_DirectStepSutErrorTest)
    test(_DirectCoverUnsatisfiedTest)
    test(_DirectCoverNoShrinkTest)
    test(_DirectShrinkIntToMinTest)
    test(_DirectShrinkIntAboveThresholdTest)
    test(_DirectShrinkArrayToMinTest)
    test(_DirectShrinkFilterPreservationTest)
    test(_DirectShrinkFlatMapTest)

    // Meta-tests (run a sub-PonyTest and verify its output)
    test(_AssertionOnlyFailTest)
    test(_AsyncPropertyTest)
    test(_MultipleForAllAsyncTest)
    test(_AsyncFailingPropertyTest)
    test(_AsyncStatefulPropertyTest)

    // Health check warning unit tests
    test(_HealthCheckFilterWarningTest)
    test(_HealthCheckFilterBelowThresholdTest)
    test(_HealthCheckChoiceWarningTest)
    test(_HealthCheckChoiceBelowThresholdTest)
    test(_HealthCheckSlowWarningTest)
    test(_HealthCheckSlowBelowThresholdTest)
    test(_HealthCheckNoWarningTest)
    test(_HealthCheckDisabledWarningTest)
    test(_HealthCheckAllDisabledWarningTest)
    test(_HealthCheckMultipleWarningsTest)

    // Async action protocol meta-tests
    test(_AsyncCompleteTest)
    test(_AsyncCompleteFalseTest)
    test(_AsyncFailTest)
    test(_AsyncExpectCompleteTest)
    test(_AsyncExpectFailTest)
    test(_AsyncFailThenCompleteTest)
    test(_AsyncCompleteThenFailTest)

    // Classification contamination tests
    test(_ClassifyShrinkContaminationTest)
    test(_ClassifyRegressionReplayTest)

    // Regression save-on-fail meta-tests
    test(_RegressionSaveOnFailTest)
    test(_StatefulRegressionSaveOnFailTest)

class \nodoc\ iso _TestListPreservesOrder is UnitTest
  """
  Without --shuffle, tests complete in registration order.
  """
  fun name(): String => "pony_test/dispatch/preserves_order"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test(_NamedTest("A"))
          test(_NamedTest("B"))
          test(_NamedTest("C"))
          test(_NamedTest("D"))
          test(_NamedTest("E"))
      end
    let expected = recover val ["A"; "B"; "C"; "D"; "E"] end
    let reporter = _CompletionOrderReporter(h, expected)
    let env = _TestEnv(h, recover val ["test"; "--sequential"] end)
    PonyTest(env, list, reporter)

class \nodoc\ iso _TestShuffleVariesAcrossSeeds is UnitTest
  """
  Across 10 different seeds, the shuffled test order varies. At least two
  orderings must differ.
  """
  fun name(): String => "pony_test/shuffle/varies_across_seeds"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let num_seeds: USize = 10
    let collector = _MultiSeedCollector(h, num_seeds)
    var seed: U64 = 1
    while seed <= num_seeds.u64() do
      let list =
        object tag is TestList
          fun tag tests(test: PonyTest) =>
            test(_NamedTest("A"))
            test(_NamedTest("B"))
            test(_NamedTest("C"))
            test(_NamedTest("D"))
            test(_NamedTest("E"))
            test(_NamedTest("F"))
            test(_NamedTest("G"))
            test(_NamedTest("H"))
            test(_NamedTest("I"))
            test(_NamedTest("J"))
        end
      let args =
        recover val
          ["test"; "--sequential"; "--shuffle=" + seed.string()]
        end
      let reporter = _PerSeedReporter(collector)
      let env = _TestEnv(h, args)
      PonyTest(env, list, reporter)
      seed = seed + 1
    end

class \nodoc\ iso _TestListShuffleSeedZero is UnitTest
  """
  Seed 0 is valid and not confused with "no seed provided".
  """
  fun name(): String => "pony_test/shuffle/seed_zero"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test(_NamedTest("A"))
          test(_NamedTest("B"))
          test(_NamedTest("C"))
          test(_NamedTest("D"))
          test(_NamedTest("E"))
      end
    let expected =
      recover val
        ["E"; "A"; "C"; "D"; "B"]
      end
    let reporter = _CompletionOrderReporter(h, expected)
    let env =
      _TestEnv(h, recover val ["test"; "--sequential"; "--shuffle=0"] end)
    PonyTest(env, list, reporter)

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------
primitive \nodoc\ _TestEnv
  """
  Create a sandboxed Env for a sub-PonyTest with controlled arguments.
  """
  fun apply(h: TestHelper, args: Array[String] val): Env =>
    Env.create(
      h.env.root,
      h.env.input,
      h.env.out,
      h.env.err,
      args,
      h.env.vars,
      {(code: I32) => None })

class \nodoc\ iso _NamedTest is UnitTest
  """
  A trivially-passing test with a configurable name.
  """
  let _name: String

  new iso create(name': String) => _name = name'
  fun name(): String => _name
  fun apply(h: TestHelper) => None

actor \nodoc\ _CompletionOrderReporter is TestReporter
  """
  Asserts that tests complete in the expected order.
  """
  let _h: TestHelper
  let _expected: Array[String] val
  embed _completion_order: Array[String] = Array[String]

  new create(h: TestHelper, expected: Array[String] val) =>
    _h = h
    _expected = expected

  be test_started(name: String) => None

  be test_complete(result: TestResult val) =>
    _completion_order.push(result.name)

  be testing_complete(results: Array[TestResult val] val) =>
    _h.assert_array_eq[String](_expected, _completion_order)
    _h.complete(true)

actor \nodoc\ _MultiSeedCollector
  """
  Receives shuffled test orders from multiple seeds and verifies that at
  least two orderings differ.
  """
  let _h: TestHelper
  let _total: USize
  embed _orders: Array[Array[String] val] = Array[Array[String] val]

  new create(h: TestHelper, total: USize) =>
    _h = h
    _total = total

  be receive(order: Array[String] val) =>
    _orders.push(order)
    if _orders.size() == _total then
      _verify()
    end

  fun ref _verify() =>
    var found_different = false
    try
      let first = _orders(0)?
      var i: USize = 1
      while i < _orders.size() do
        let other = _orders(i)?
        if not _arrays_equal(first, other) then
          found_different = true
          break
        end
        i = i + 1
      end
    else
      _Unreachable()
    end
    _h.assert_true(
      found_different,
      "All 10 seeds produced the same test order")
    _h.complete(true)

  fun _arrays_equal(a: Array[String] val, b: Array[String] val): Bool =>
    if a.size() != b.size() then return false end
    try
      var i: USize = 0
      while i < a.size() do
        if a(i)? != b(i)? then return false end
        i = i + 1
      end
    else
      return false
    end
    true

actor \nodoc\ _PerSeedReporter is TestReporter
  """
  Delivers one seed's completion order to the parent _MultiSeedCollector.
  """
  let _parent: _MultiSeedCollector
  embed _completion_order: Array[String] = Array[String]

  new create(parent: _MultiSeedCollector) =>
    _parent = parent

  be test_started(name: String) => None

  be test_complete(result: TestResult val) =>
    _completion_order.push(result.name)

  be testing_complete(results: Array[TestResult val] val) =>
    let sz = _completion_order.size()
    let order = recover iso Array[String](sz) end
    for n in _completion_order.values() do
      order.push(n)
    end
    _parent.receive(consume order)


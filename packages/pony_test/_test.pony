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
  --list without --shuffle prints test names in registration order.
  """
  fun name(): String => "pony_test/list/preserves_order"

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
    _RunList(h, ["test"; "--list"], list, expected)

class \nodoc\ iso _TestShuffleVariesAcrossSeeds is UnitTest
  """
  Across 10 different seeds, the shuffled test order varies. Each seed is run
  through the full PonyTest code path (argument parsing, buffered dispatch,
  shuffle, output) and the resulting orderings are collected. The test passes
  when at least two orderings differ.
  """
  fun name(): String => "pony_test/shuffle/varies_across_seeds"

  fun apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let num_tests: USize = 10
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
          ["test"; "--list"; "--shuffle=" + seed.string()]
        end
      let out = _PerSeedCollector(collector, num_tests + 1)
      _RunListWith(h, args, list, out)
      seed = seed + 1
    end

class \nodoc\ iso _TestListShuffleSeedZero is UnitTest
  """
  Seed 0 is valid and not confused with "no seed provided".
  """
  fun name(): String => "pony_test/list/shuffle_seed_zero"

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
        ["Test seed: 0"; "E"; "A"; "C"; "D"; "B"]
      end
    _RunList(h, ["test"; "--list"; "--shuffle=0"], list, expected)

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------
primitive \nodoc\ _RunList
  """
  Create a PonyTest in --list mode with controlled args and verify its output.
  """
  fun apply(
    h: TestHelper,
    args: Array[String] val,
    list: TestList tag,
    expected: Array[String] val)
  =>
    let collector = _OutputCollector(h, expected, 1)
    _RunListWith(h, args, list, collector)

primitive \nodoc\ _RunListWith
  """
  Create a PonyTest in --list mode, sending output to the given collector.
  """
  fun apply(
    h: TestHelper,
    args: Array[String] val,
    list: TestList tag,
    collector: OutStream)
  =>
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        collector,
        h.env.err,
        args,
        h.env.vars,
        {(code: I32) => None })
    PonyTest(env, list)

class \nodoc\ iso _NamedTest is UnitTest
  """
  A trivially-passing test with a configurable name.
  """
  let _name: String

  new iso create(name': String) => _name = name'
  fun name(): String => _name
  fun apply(h: TestHelper) => None

actor \nodoc\ _OutputCollector is OutStream
  """
  Captures print output from a PonyTest instance and verifies it against
  expected lines. Supports multiple runs: _runs_remaining counts how many
  complete sets of expected output must be received before signaling
  completion. Each run must produce output identical to _expected.
  """
  let _h: TestHelper
  let _expected: Array[String] val
  var _runs_remaining: USize
  embed _received: Array[String] = Array[String]

  new create(
    h: TestHelper,
    expected: Array[String] val,
    runs: USize = 1)
  =>
    _h = h
    _expected = expected
    _runs_remaining = runs

  be print(data: ByteSeq) =>
    if _runs_remaining == 0 then return end
    match \exhaustive\ data
    | let s: String => _received.push(s)
    | let a: Array[U8] val => _received.push(String.from_array(a))
    end
    if _received.size() == _expected.size() then
      _h.assert_array_eq[String](_expected, _received)
      _received.clear()
      _runs_remaining = _runs_remaining - 1
      if _runs_remaining == 0 then
        _h.complete(true)
      end
    end

  be write(data: ByteSeq) => None
  be printv(data: ByteSeqIter) => None
  be writev(data: ByteSeqIter) => None
  be flush() => None

actor \nodoc\ _MultiSeedCollector
  """
  Collects shuffled test orders from multiple PonyTest runs (one per seed)
  and verifies that at least two different orderings were produced.
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

actor \nodoc\ _PerSeedCollector is OutStream
  """
  Captures output from a single --list --shuffle=SEED run. After receiving
  all expected lines, strips the seed line and sends just the test name
  ordering to the parent _MultiSeedCollector.
  """
  let _parent: _MultiSeedCollector
  let _expected_lines: USize
  var _done: Bool = false
  embed _received: Array[String] = Array[String]

  new create(parent: _MultiSeedCollector, expected_lines: USize) =>
    _parent = parent
    _expected_lines = expected_lines

  be print(data: ByteSeq) =>
    if _done then return end
    match \exhaustive\ data
    | let s: String => _received.push(s)
    | let a: Array[U8] val => _received.push(String.from_array(a))
    end
    if _received.size() == _expected_lines then
      _done = true
      let order: Array[String] iso = recover iso Array[String] end
      var i: USize = 1  // skip "Test seed: N" line
      while i < _received.size() do
        try order.push(_received(i)?) else _Unreachable() end
        i = i + 1
      end
      _parent.receive(consume order)
    end

  be write(data: ByteSeq) => None
  be printv(data: ByteSeqIter) => None
  be writev(data: ByteSeqIter) => None
  be flush() => None


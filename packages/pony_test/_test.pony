actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestListPreservesOrder)
    test(_TestShuffleVariesAcrossSeeds)
    test(_TestListShuffleSeedZero)

class \nodoc\ iso _TestListPreservesOrder is UnitTest
  """
  --list without --shuffle prints test names in registration order.
  """
  fun name(): String => "pony_test/list/preserves_order"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let list = object tag is TestList
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
    let collector = _MultiSeedCollector(h, num_seeds, num_tests)
    var seed: U64 = 1
    while seed <= num_seeds.u64() do
      let list = object tag is TestList
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
      let args = recover val
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
    let list = object tag is TestList
      fun tag tests(test: PonyTest) =>
        test(_NamedTest("A"))
        test(_NamedTest("B"))
        test(_NamedTest("C"))
        test(_NamedTest("D"))
        test(_NamedTest("E"))
    end
    let expected = recover val
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
  fun apply(h: TestHelper, args: Array[String] val, list: TestList tag,
    collector: OutStream)
  =>
    let env = Env.create(
      h.env.root,
      h.env.input,
      collector,
      h.env.err,
      args,
      h.env.vars,
      {(code: I32) => None})
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

  new create(h: TestHelper, expected: Array[String] val,
    runs: USize = 1)
  =>
    _h = h
    _expected = expected
    _runs_remaining = runs

  be print(data: ByteSeq) =>
    match data
    | let s: String => _received.push(s)
    | let a: Array[U8] val => _received.push(String.from_array(a))
    end
    if _received.size() == _expected.size() then
      _check_and_maybe_complete()
    end

  fun ref _check_and_maybe_complete() =>
    _h.assert_array_eq[String](_expected, _received)
    _received.clear()
    _runs_remaining = _runs_remaining - 1
    if _runs_remaining == 0 then
      _h.complete(true)
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
  let _num_tests: USize
  embed _orders: Array[Array[String] val] = Array[Array[String] val]

  new create(h: TestHelper, total: USize, num_tests: USize) =>
    _h = h
    _total = total
    _num_tests = num_tests

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
    end
    _h.assert_true(found_different,
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
  embed _received: Array[String] = Array[String]

  new create(parent: _MultiSeedCollector, expected_lines: USize) =>
    _parent = parent
    _expected_lines = expected_lines

  be print(data: ByteSeq) =>
    match data
    | let s: String => _received.push(s)
    | let a: Array[U8] val => _received.push(String.from_array(a))
    end
    if _received.size() == _expected_lines then
      let order: Array[String] iso = recover iso Array[String] end
      var i: USize = 1  // skip "Test seed: N" line
      while i < _received.size() do
        try order.push(_received(i)?) end
        i = i + 1
      end
      _parent.receive(consume order)
    end

  be write(data: ByteSeq) => None
  be printv(data: ByteSeqIter) => None
  be writev(data: ByteSeqIter) => None
  be flush() => None

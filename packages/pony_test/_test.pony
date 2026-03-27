actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestListPreservesOrder)
    test(_TestListShuffleOrder)
    test(_TestListShuffleDeterministic)
    test(_TestListShuffleDifferentSeeds)
    test(_TestListShuffleSeedZero)

class \nodoc\ iso _TestListPreservesOrder is UnitTest
  """
  --list without --shuffle prints test names in registration order.
  """
  fun name(): String => "pony_test/list/preserves_order"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let expected = recover val ["A"; "B"; "C"; "D"; "E"] end
    _RunList(h, ["test"; "--list"], _FiveTests, expected)

class \nodoc\ iso _TestListShuffleOrder is UnitTest
  """
  --list --shuffle=42 prints the seed then test names in shuffled order.
  Exercises the full code path: _process_opts() parses --shuffle=42 into
  _Shuffled(42), apply() buffers names, _all_tests_applied() shuffles and
  prints.
  """
  fun name(): String => "pony_test/list/shuffle_order"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let expected = recover val
      ["Test seed: 42"; "C"; "D"; "B"; "A"; "E"]
    end
    _RunList(h, ["test"; "--list"; "--shuffle=42"], _FiveTests, expected)

class \nodoc\ iso _TestListShuffleDeterministic is UnitTest
  """
  Running --list --shuffle=42 twice produces identical output.
  """
  fun name(): String => "pony_test/list/shuffle_deterministic"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let expected = recover val
      ["Test seed: 42"; "C"; "D"; "B"; "A"; "E"]
    end
    let collector = _OutputCollector(h, expected, 2)
    _RunListWith(h, ["test"; "--list"; "--shuffle=42"], _FiveTests, collector)
    _RunListWith(h, ["test"; "--list"; "--shuffle=42"], _FiveTests, collector)

class \nodoc\ iso _TestListShuffleDifferentSeeds is UnitTest
  """
  Different seeds produce different orderings through the full PonyTest flow.
  """
  fun name(): String => "pony_test/list/shuffle_different_seeds"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let from_42 = recover val
      ["Test seed: 42"; "C"; "D"; "B"; "A"; "E"]
    end
    let from_123 = recover val
      ["Test seed: 123"; "B"; "E"; "A"; "C"; "D"]
    end
    let collector = _OutputCollectorPair(h, from_42, from_123)
    _RunListWith(h, ["test"; "--list"; "--shuffle=42"], _FiveTests,
      collector.first())
    _RunListWith(h, ["test"; "--list"; "--shuffle=123"], _FiveTests,
      collector.second())

class \nodoc\ iso _TestListShuffleSeedZero is UnitTest
  """
  Seed 0 is valid and not confused with "no seed provided".
  """
  fun name(): String => "pony_test/list/shuffle_seed_zero"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    let expected = recover val
      ["Test seed: 0"; "E"; "A"; "C"; "D"; "B"]
    end
    _RunList(h, ["test"; "--list"; "--shuffle=0"], _FiveTests, expected)

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

primitive \nodoc\ _FiveTests is TestList
  fun tag tests(test: PonyTest) =>
    test(_NamedTest("A"))
    test(_NamedTest("B"))
    test(_NamedTest("C"))
    test(_NamedTest("D"))
    test(_NamedTest("E"))

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

actor \nodoc\ _OutputCollectorPair
  """
  Manages two independent collectors for tests that compare output from
  two different PonyTest runs.
  """
  let _h: TestHelper
  let _first_expected: Array[String] val
  let _second_expected: Array[String] val
  var _first_done: Bool = false
  var _second_done: Bool = false

  new create(h: TestHelper, first_expected: Array[String] val,
    second_expected: Array[String] val)
  =>
    _h = h
    _first_expected = first_expected
    _second_expected = second_expected

  fun tag first(): _OutputCollectorHalf =>
    _OutputCollectorHalf(this, true)

  fun tag second(): _OutputCollectorHalf =>
    _OutputCollectorHalf(this, false)

  be _half_done(is_first: Bool, received: Array[String] val) =>
    if is_first then
      _h.assert_array_eq[String](_first_expected, received)
      _first_done = true
    else
      _h.assert_array_eq[String](_second_expected, received)
      _second_done = true
    end
    if _first_done and _second_done then
      _h.complete(true)
    end

actor \nodoc\ _OutputCollectorHalf is OutStream
  """
  Captures output for one half of a collector pair.
  """
  let _parent: _OutputCollectorPair
  let _is_first: Bool
  let _expected_count: USize
  embed _received: Array[String] = Array[String]

  new create(parent: _OutputCollectorPair, is_first: Bool) =>
    _parent = parent
    _is_first = is_first
    _expected_count = 6  // seed line + 5 test names

  be print(data: ByteSeq) =>
    match data
    | let s: String => _received.push(s)
    | let a: Array[U8] val => _received.push(String.from_array(a))
    end
    if _received.size() == _expected_count then
      let snap: Array[String] iso = recover iso Array[String] end
      for s in _received.values() do
        snap.push(s)
      end
      _parent._half_done(_is_first, consume snap)
    end

  be write(data: ByteSeq) => None
  be printv(data: ByteSeqIter) => None
  be writev(data: ByteSeqIter) => None
  be flush() => None

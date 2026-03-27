actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestShuffledDeterministic)
    test(_TestShuffledChangesOrder)
    test(_TestShuffledPreservesElements)
    test(_TestShuffledDifferentSeeds)
    test(_TestShuffledSeedZero)

class \nodoc\ iso _TestShuffledDeterministic is UnitTest
  """
  _Shuffled.apply with the same seed produces identical results.
  """
  fun name(): String => "pony_test/shuffled/deterministic"

  fun apply(h: TestHelper) =>
    let a: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]
    let b: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]

    _Shuffled(42).apply[USize](a)
    _Shuffled(42).apply[USize](b)

    h.assert_array_eq[USize](a, b)

class \nodoc\ iso _TestShuffledChangesOrder is UnitTest
  """
  _Shuffled.apply reorders an array of 10 elements.
  """
  fun name(): String => "pony_test/shuffled/changes_order"

  fun apply(h: TestHelper) =>
    let original: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]
    let shuffled: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]

    _Shuffled(42).apply[USize](shuffled)

    var any_different = false
    try
      for (i, v) in original.pairs() do
        if v != shuffled(i)? then
          any_different = true
          break
        end
      end
    end
    h.assert_true(any_different, "Shuffle with seed 42 did not change order")

class \nodoc\ iso _TestShuffledPreservesElements is UnitTest
  """
  _Shuffled.apply is a permutation: all original elements are preserved.
  """
  fun name(): String => "pony_test/shuffled/preserves_elements"

  fun apply(h: TestHelper) =>
    let original: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]
    let shuffled: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]

    _Shuffled(42).apply[USize](shuffled)

    h.assert_array_eq_unordered[USize](original, shuffled)

class \nodoc\ iso _TestShuffledDifferentSeeds is UnitTest
  """
  _Shuffled instances with different seeds produce different orderings.
  """
  fun name(): String => "pony_test/shuffled/different_seeds"

  fun apply(h: TestHelper) =>
    let a: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]
    let b: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]

    _Shuffled(42).apply[USize](a)
    _Shuffled(123).apply[USize](b)

    var any_different = false
    try
      for (i, v) in a.pairs() do
        if v != b(i)? then
          any_different = true
          break
        end
      end
    end
    h.assert_true(any_different,
      "Seeds 42 and 123 produced the same order")

class \nodoc\ iso _TestShuffledSeedZero is UnitTest
  """
  Seed value 0 is valid: _Shuffled(0).apply produces a deterministic shuffle.
  """
  fun name(): String => "pony_test/shuffled/seed_zero"

  fun apply(h: TestHelper) =>
    let a: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]
    let b: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]

    _Shuffled(0).apply[USize](a)
    _Shuffled(0).apply[USize](b)

    h.assert_array_eq[USize](a, b)

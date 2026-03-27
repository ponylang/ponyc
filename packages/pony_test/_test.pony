use "random"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestShuffleDeterministic)
    test(_TestShuffleChangesOrder)
    test(_TestShufflePreservesElements)
    test(_TestShuffleDifferentSeeds)
    test(_TestShuffleSeedZero)

class \nodoc\ iso _TestShuffleDeterministic is UnitTest
  """
  Shuffling the same array with the same seed produces identical results.
  """
  fun name(): String => "pony_test/shuffle/deterministic"

  fun apply(h: TestHelper) =>
    let a: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]
    let b: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]

    Rand.from_u64(42).shuffle[USize](a)
    Rand.from_u64(42).shuffle[USize](b)

    h.assert_array_eq[USize](a, b)

class \nodoc\ iso _TestShuffleChangesOrder is UnitTest
  """
  Shuffling an array of 10 elements changes its order.
  """
  fun name(): String => "pony_test/shuffle/changes_order"

  fun apply(h: TestHelper) =>
    let original: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]
    let shuffled: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]

    Rand.from_u64(42).shuffle[USize](shuffled)

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

class \nodoc\ iso _TestShufflePreservesElements is UnitTest
  """
  Shuffling preserves all elements (it is a permutation, not a
  transformation).
  """
  fun name(): String => "pony_test/shuffle/preserves_elements"

  fun apply(h: TestHelper) =>
    let original: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]
    let shuffled: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]

    Rand.from_u64(42).shuffle[USize](shuffled)

    h.assert_array_eq_unordered[USize](original, shuffled)

class \nodoc\ iso _TestShuffleDifferentSeeds is UnitTest
  """
  Different seeds produce different orderings.
  """
  fun name(): String => "pony_test/shuffle/different_seeds"

  fun apply(h: TestHelper) =>
    let a: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]
    let b: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]

    Rand.from_u64(42).shuffle[USize](a)
    Rand.from_u64(123).shuffle[USize](b)

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

class \nodoc\ iso _TestShuffleSeedZero is UnitTest
  """
  Seed value 0 is valid and produces a deterministic shuffle.
  """
  fun name(): String => "pony_test/shuffle/seed_zero"

  fun apply(h: TestHelper) =>
    let a: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]
    let b: Array[USize] = [0; 1; 2; 3; 4; 5; 6; 7; 8; 9]

    Rand.from_u64(0).shuffle[USize](a)
    Rand.from_u64(0).shuffle[USize](b)

    h.assert_array_eq[USize](a, b)

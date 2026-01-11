use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(SortedSmallArray)

class \nodoc\iso SortedSmallArray is UnitTest
  fun name(): String =>
    "SortedSmallArray"
  fun apply(h: TestHelper) =>
    let array: Array[U32] = [1; 2; 3; 4; 5; 6; 7; 8; 9]
    assert_search(h, 3, array where expected = (2, true))
    assert_search(h, 0, array where expected = (0, false))
    assert_search(h, 10, array where expected = (9, false))

  fun assert_search(h: TestHelper, needle: U32, haystack: ReadSeq[U32], expected: (USize, Bool)) =>
    (let idx, let found) = BinarySearch[U32](needle, haystack)
    h.assert_eq[USize](expected._1, idx)
    h.assert_eq[Bool](expected._2, found)

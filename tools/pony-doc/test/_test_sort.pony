use "pony_test"
use doc = ".."

class \nodoc\ _TestNameSortBasic is UnitTest
  fun name(): String => "NameSort/basic"

  fun apply(h: TestHelper) =>
    h.assert_eq[Compare](doc.NameSort.compare("Foo", "Bar"), Greater)
    h.assert_eq[Compare](doc.NameSort.compare("Bar", "Foo"), Less)

class \nodoc\ _TestNameSortUnderscore is UnitTest
  fun name(): String => "NameSort/underscore-stripped"

  fun apply(h: TestHelper) =>
    // Leading underscore is stripped before comparison
    h.assert_eq[Compare](doc.NameSort.compare("_Bar", "Foo"), Less)
    h.assert_eq[Compare](doc.NameSort.compare("_Foo", "Bar"), Greater)

class \nodoc\ _TestNameSortEqual is UnitTest
  fun name(): String => "NameSort/equal"

  fun apply(h: TestHelper) =>
    h.assert_eq[Compare](doc.NameSort.compare("_Foo", "Foo"), Equal)
    h.assert_eq[Compare](doc.NameSort.compare("Foo", "Foo"), Equal)

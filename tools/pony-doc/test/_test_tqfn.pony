use "pony_test"
use doc = ".."

class \nodoc\ _TestTQFNSimple is UnitTest
  fun name(): String => "TQFN/simple"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](doc.TQFN("builtin", "Array"), "builtin-Array")

class \nodoc\ _TestTQFNPathSeparator is UnitTest
  fun name(): String => "TQFN/path-separator"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      doc.TQFN("collections/persistent", "Map"),
      "collections-persistent-Map")

class \nodoc\ _TestTQFNIndexOverride is UnitTest
  fun name(): String => "TQFN/index-override"

  fun apply(h: TestHelper) =>
    // The "-index" override produces a double dash: separator + "-index"
    h.assert_eq[String](doc.TQFN("builtin", "-index"), "builtin--index")

class \nodoc\ _TestTQFNEmptyPackage is UnitTest
  fun name(): String => "TQFN/empty-package"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](doc.TQFN("", "Foo"), "-Foo")

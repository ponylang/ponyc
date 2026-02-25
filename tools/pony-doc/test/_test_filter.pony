use "pony_test"
use doc = ".."

class \nodoc\ _TestFilterTestPackage is UnitTest
  fun name(): String => "Filter/test-package"

  fun apply(h: TestHelper) =>
    h.assert_true(doc.Filter.is_test_package("test"))
    h.assert_true(doc.Filter.is_test_package("builtin_test"))
    h.assert_false(doc.Filter.is_test_package("builtin"))
    h.assert_false(doc.Filter.is_test_package("collections"))

class \nodoc\ _TestFilterPrivate is UnitTest
  fun name(): String => "Filter/private"

  fun apply(h: TestHelper) =>
    h.assert_true(doc.Filter.is_private("_Foo"))
    h.assert_true(doc.Filter.is_private("_"))
    h.assert_false(doc.Filter.is_private("Foo"))
    h.assert_false(doc.Filter.is_private(""))

class \nodoc\ _TestFilterInternal is UnitTest
  fun name(): String => "Filter/internal"

  fun apply(h: TestHelper) =>
    h.assert_true(doc.Filter.is_internal("$1"))
    h.assert_true(doc.Filter.is_internal("$foo"))
    h.assert_false(doc.Filter.is_internal("Foo"))
    h.assert_false(doc.Filter.is_internal("_Foo"))
    h.assert_false(doc.Filter.is_internal(""))

use "pony_test"
use doc = ".."

class \nodoc\ _TestPathSanitizeDot is UnitTest
  fun name(): String => "PathSanitize/dot-to-underscore"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      doc.PathSanitize.replace_path_separator("collections.persistent"),
      "collections_persistent")

class \nodoc\ _TestPathSanitizeSlash is UnitTest
  fun name(): String => "PathSanitize/slash-to-dash"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      doc.PathSanitize.replace_path_separator("foo/bar"),
      "foo-bar")

class \nodoc\ _TestPathSanitizeMixed is UnitTest
  fun name(): String => "PathSanitize/mixed"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      doc.PathSanitize.replace_path_separator("org.example/my.pkg"),
      "org_example-my_pkg")

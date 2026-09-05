use "pony_test"
use dep = ".."

class \nodoc\ _TestPathValidatorSimplePath is UnitTest
  fun name(): String => "PathValidator/simple path"

  fun apply(h: TestHelper) =>
    h.assert_true(dep.PathValidator("foo.pony"))

class \nodoc\ _TestPathValidatorNestedPath is UnitTest
  fun name(): String => "PathValidator/nested path"

  fun apply(h: TestHelper) =>
    h.assert_true(dep.PathValidator("src/foo/bar.pony"))

class \nodoc\ _TestPathValidatorEmptyPath is UnitTest
  fun name(): String => "PathValidator/empty path"

  fun apply(h: TestHelper) =>
    h.assert_false(dep.PathValidator(""))

class \nodoc\ _TestPathValidatorAbsolutePath is UnitTest
  fun name(): String => "PathValidator/absolute path"

  fun apply(h: TestHelper) =>
    h.assert_false(dep.PathValidator("/etc/passwd"))

class \nodoc\ _TestPathValidatorDotDot is UnitTest
  fun name(): String => "PathValidator/dot-dot component"

  fun apply(h: TestHelper) =>
    h.assert_false(dep.PathValidator("../escape"))
    h.assert_false(dep.PathValidator("foo/../bar"))
    h.assert_false(dep.PathValidator("foo/.."))

class \nodoc\ _TestPathValidatorDot is UnitTest
  fun name(): String => "PathValidator/dot component"

  fun apply(h: TestHelper) =>
    h.assert_false(dep.PathValidator("./foo"))
    h.assert_false(dep.PathValidator("foo/./bar"))
    h.assert_false(dep.PathValidator("."))

class \nodoc\ _TestPathValidatorBackslash is UnitTest
  fun name(): String => "PathValidator/backslash"

  fun apply(h: TestHelper) =>
    h.assert_false(dep.PathValidator("foo\\bar"))
    h.assert_false(dep.PathValidator("src\\foo.pony"))

class \nodoc\ _TestPathValidatorNulByte is UnitTest
  fun name(): String => "PathValidator/NUL byte"

  fun apply(h: TestHelper) =>
    h.assert_false(dep.PathValidator("foo\x00bar"))

class \nodoc\ _TestPathValidatorConsecutiveSlashes is UnitTest
  fun name(): String => "PathValidator/consecutive slashes"

  fun apply(h: TestHelper) =>
    h.assert_false(dep.PathValidator("foo//bar"))

class \nodoc\ _TestPathValidatorTrailingSlash is UnitTest
  fun name(): String => "PathValidator/trailing slash"

  fun apply(h: TestHelper) =>
    h.assert_false(dep.PathValidator("foo/bar/"))

class \nodoc\ _TestPathValidatorDotDotSubstring is UnitTest
  fun name(): String => "PathValidator/dot-dot as substring"

  fun apply(h: TestHelper) =>
    h.assert_true(dep.PathValidator("foo..bar"))
    h.assert_true(dep.PathValidator("..foo"))

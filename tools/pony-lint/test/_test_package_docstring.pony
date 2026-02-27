use "pony_test"
use lint = ".."

class \nodoc\ _TestPackageDocstringClean is UnitTest
  """Package with expected file and docstring produces 0."""
  fun name(): String => "PackageDocstring: has file and docstring"

  fun apply(h: TestHelper) =>
    let diags = lint.PackageDocstring.check_package(
      "my_package", true, true, "my_package/foo.pony")
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestPackageDocstringNoFile is UnitTest
  """Package without expected file produces 1 diagnostic."""
  fun name(): String => "PackageDocstring: no package file flagged"

  fun apply(h: TestHelper) =>
    let diags = lint.PackageDocstring.check_package(
      "my_package", false, false, "my_package/foo.pony")
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[String](
        "style/package-docstring", diags(0)?.rule_id)
      h.assert_true(
        diags(0)?.message.contains("no 'my_package.pony'"))
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestPackageDocstringNoDocstring is UnitTest
  """Package with file but no docstring produces 1 diagnostic."""
  fun name(): String => "PackageDocstring: no docstring flagged"

  fun apply(h: TestHelper) =>
    let diags = lint.PackageDocstring.check_package(
      "my_package", true, false, "my_package/foo.pony")
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[String](
        "style/package-docstring", diags(0)?.rule_id)
      h.assert_true(
        diags(0)?.message.contains("should have a package docstring"))
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestPackageDocstringHyphenated is UnitTest
  """Hyphenated package name with underscored file produces 0."""
  fun name(): String => "PackageDocstring: hyphenated name normalized"

  fun apply(h: TestHelper) =>
    // The linter normalizes hyphens to underscores before calling
    // check_package, so we test with the already-normalized name
    let diags = lint.PackageDocstring.check_package(
      "pony_lint", true, true, "pony-lint/foo.pony")
    h.assert_eq[USize](0, diags.size())
